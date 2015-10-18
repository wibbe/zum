
#include "Document.h"
#include "Str.h"
#include "Cell.h"
#include "Editor.h"
#include "Log.h"

#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <list>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

#include "Tcl.h"

namespace doc {

  static const tcl::Variable DELIMITERS("app_delimiters", ",;|");
  static const tcl::Variable IGNORE_FIRST_ROW("app_ignoreFirstRow", true);
  static const tcl::Variable DEFAULT_ROW_COUNT("doc_defaultRowCount", 10);
  static const tcl::Variable DEFAULT_COLUMN_COUNT("doc_defaultColumnCount", 5);
  static const tcl::Variable DEFAULT_COLUMN_WIDTH("doc_defaultColumnWidth", 20);

  struct Document
  {
    int width_ = 0;
    int height_ = 0;
    std::vector<int> columnWidth_ = {0};
    std::unordered_map<Index, Cell> cells_;
    std::string filename_;
    bool readOnly_ = false;
    char delimiter_;
  };

  enum class EditAction
  {
    CellText,
    ColumnWidth,
    UndoRedo,
    AddColumn,
    RemoveColumn,
    AddRow,
    RemoveRow
  };

  struct UndoState
  {
    UndoState(Document const& doc, Index const& idx, EditAction action)
      : doc_(doc),
        cursor_(idx),
        action_(action)
    { }

    Document doc_;
    Index cursor_;
    EditAction action_;
  };

  struct Buffer
  {
    Document doc_;
    Index cursorPos_ = Index(0, 0);
    Index scroll_ = Index(0, 0);
    std::vector<UndoState> undoStack_;
    std::vector<UndoState> redoStack_;
  };

  static std::vector<Buffer> & documentBuffers()
  {
    static std::vector<Buffer> buffer;
    return buffer;
  }

  static int currentBufferIndex_ = 0;

  static Buffer & currentBuffer()
  {
    assert(!documentBuffers().empty());
    return documentBuffers().at(currentBufferIndex_);
  }

  static Document & currentDoc()
  {
    return currentBuffer().doc_;
  }

  Index & cursorPos()
  {
    return currentBuffer().cursorPos_;
  }

  Index & scroll()
  {
    return currentBuffer().scroll_;
  }

  void nextBuffer()
  {
    currentBufferIndex_++;
    if (currentBufferIndex_ >= documentBuffers().size())
      currentBufferIndex_ = 0;
  }

  void previousBuffer()
  {
    if (currentBufferIndex_ == 0)
      currentBufferIndex_ = documentBuffers().size() - 1;
    else
      currentBufferIndex_--;
  }

  void jumpToBuffer(int buffer)
  {
    if (buffer < 0 || buffer >= documentBuffers().size())
      return;
    currentBufferIndex_ = buffer;
  }

  int currentBufferIndex()
  {
    return currentBufferIndex_;
  }

  int getOpenBufferCount()
  {
    return documentBuffers().size();
  }

  void createDefaultEmpty()
  {
    documentBuffers().push_back({});
    jumpToBuffer(documentBuffers().size() - 1);

    currentDoc().width_ = std::max(DEFAULT_COLUMN_COUNT.toInt(), 1);
    currentDoc().height_ = std::max(DEFAULT_ROW_COUNT.toInt(), 1);
    currentDoc().columnWidth_ = std::vector<int>(currentDoc().width_, std::max(DEFAULT_COLUMN_WIDTH.toInt(), 3));
    currentDoc().filename_ = "[No Name]";
    currentDoc().delimiter_ = ',';
    currentDoc().readOnly_ = false;
  }

  void createEmpty(int width, int height)
  {
    createDefaultEmpty();
    currentDoc().width_ = std::max(width, 1);
    currentDoc().height_ = std::max(height, 1);
    currentDoc().columnWidth_ = std::vector<int>(currentDoc().width_, std::max(DEFAULT_COLUMN_WIDTH.toInt(), 3));
  }

  void close()
  {
    documentBuffers().erase(documentBuffers().begin() + currentBufferIndex_);

    if (documentBuffers().empty())
      createDefaultEmpty();
    else
      currentBufferIndex_ = std::max(0, std::min((int)documentBuffers().size(), currentBufferIndex()));
  }

  static Cell & getCell(Index const& idx)
  {
    return currentDoc().cells_[idx];
  }

  static std::string getText(Cell const& cell)
  {
    if (cell.hasExpression && !cell.expression.empty())
      return "=" + exprToString(cell.expression);

    return cell.text;
  }

  static void takeUndoSnapshot(EditAction action, bool canMerge)
  {
    bool createNew = true;

    if (!currentBuffer().undoStack_.empty())
    {
      UndoState & state = currentBuffer().undoStack_.back();

      if (state.action_ == action && canMerge)
        createNew = false;
    }

    if (createNew)
    {
      currentBuffer().undoStack_.emplace_back(currentDoc(), cursorPos(), action);
      currentBuffer().redoStack_.clear();
    }
  }

  bool undo()
  {
    if (!currentBuffer().undoStack_.empty())
    {
      currentBuffer().redoStack_.emplace_back(currentDoc(), cursorPos(), EditAction::UndoRedo);

      UndoState const& state = currentBuffer().undoStack_.back();
      currentDoc() = state.doc_;
      cursorPos() = state.cursor_;

      currentBuffer().undoStack_.pop_back();
      return true;
    }

    return false;
  }

  bool redo()
  {
    if (!currentBuffer().redoStack_.empty())
    {
      currentBuffer().undoStack_.emplace_back(currentDoc(), cursorPos(), EditAction::UndoRedo);

      UndoState const& state = currentBuffer().redoStack_.back();
      currentDoc() = state.doc_;
      cursorPos() = state.cursor_;

      currentBuffer().redoStack_.pop_back();

      return true;
    }

    return false;
  }

  std::string getFilename()
  {
    return currentDoc().filename_;
  }

  bool isReadOnly()
  {
    return currentDoc().readOnly_;
  }

  bool save(std::string const& filename)
  {
    logInfo("Saving document: ", filename);

    std::ofstream file(filename.c_str());
    if (!file.is_open())
    {
      flashMessage("Could not save document!");
      return false;
    }

    // Write header info
    if (IGNORE_FIRST_ROW.toBool())
      for (int i = 0; i < currentDoc().width_; ++i)
        file << currentDoc().columnWidth_[i] << (i < (currentDoc().width_ - 1) ? (char)currentDoc().delimiter_ : '\n');

    // Write all the cells
    for (int y = 0; y < currentDoc().height_; ++y)
    {
      for (int x = 0; x < currentDoc().width_; ++x)
      {
        Cell const& cell = getCell(Index(x, y));

        file << getText(cell) << formatToStr(cell.format);

        if (x < (currentDoc().width_ - 1))
        {
          file << currentDoc().delimiter_;
        }
        else
        {
          if (y < (currentDoc().height_ - 1))
            file << std::endl;
        }
      }
    }

    currentDoc().filename_ = filename;
    return true;
  }

  struct Parser
  {
    Parser(std::string const& data, char delim)
      : data_(data),
        delim_(delim)
    { }

    bool next(std::string & value)
    {
      value.clear();

      if (eof())
        return false;

      while (!eof() && data_[pos_] != delim_ && data_[pos_] != '\n')
        value.append(1, data_[pos_++]);

      if (eof())
        return false;

      const bool newLine = data_[pos_++] == '\n';
      return !newLine;
    }

    bool eof() const { return pos_ >= data_.size(); }

    std::string const& data_;
    char delim_;
    int pos_ = 0;
  };

  static void setText(Index const& idx, std::string const& text, bool forceFormat = false)
  {
    Cell & cell = getCell(idx);

    if (forceFormat)
    {
      std::tie(cell.format, cell.text) = parseFormatAndValue(text);
    }
    else
    {
      uint32_t format;
      std::tie(format, cell.text) = parseFormatAndValue(text);
    }


    if (cell.text.front() == '=')
    {
      cell.hasExpression = true;
      cell.expression = parseExpression(cell.text.substr(1));
    }
    else
    {
      cell.hasExpression = false;
    }
  }

  static bool loadDocument(std::string const& data, char defaultDelimiter)
  {
    createDefaultEmpty();
    currentDoc().width_ = 0;
    currentDoc().height_ = 0;
    currentDoc().columnWidth_.resize(0);

    // Examin document to determin delimiter type
    if (defaultDelimiter == 0)
    {
      const std::string delimiters = DELIMITERS.toStr();

      std::vector<int> delimCount(delimiters.size(), 0);
      std::stringstream ss(data);

      std::string firstLine;
      std::getline(ss, firstLine);

      for (auto ch : firstLine)
      {
        const std::size_t idx = delimiters.find_first_of(ch);
        if (idx != std::string::npos)
          delimCount[idx]++;
      }

      int maxCount = -1;
      currentDoc().delimiter_ = delimiters[0];
      for (int i = 0; i < delimCount.size(); ++i)
      {
        if (delimCount[i] > maxCount)
        {
          maxCount = delimCount[i];
          currentDoc().delimiter_ = delimiters[i];
        }
      }
    }
    else
     currentDoc().delimiter_ = defaultDelimiter;

    Parser p(data, currentDoc().delimiter_);

    // Read column width
    if (IGNORE_FIRST_ROW.toBool())
    {
      bool onlyNumbers = true;
      bool firstLine = true;

      std::vector<std::string> cells;

      while (firstLine)
      {
        std::string cellText;
        firstLine = p.next(cellText);

        cells.push_back(cellText);

        for (auto ch : cellText)
          onlyNumbers &= std::isdigit(ch) != 0;
      }

      if (onlyNumbers)
      {
        for (auto const& cellText : cells)
        {
          const int width = std::max(cellText.empty() ? DEFAULT_COLUMN_WIDTH.toInt() : std::stoi(cellText), 3);
          currentDoc().columnWidth_.push_back(width);
        }

        currentDoc().width_ = currentDoc().columnWidth_.size();
        currentDoc().height_ = 0;
      }
      else
      {
        for (uint32_t i = 0; i < cells.size(); ++i)
        {
          setText(Index(i, 0), cells[i], true);

          currentDoc().columnWidth_.push_back(DEFAULT_COLUMN_WIDTH.toInt());
        }

        currentDoc().width_ = cells.size();
        currentDoc().height_ = 1;
      }
    }

    int column = 0;
    while (!p.eof())
    {
      std::string cellText;
      const bool newLine = !p.next(cellText);

      if (!cellText.empty())
        setText(Index(column, currentDoc().height_), cellText, true);

      column++;

      if (column > currentDoc().width_)
      {
        currentDoc().width_ = column;
        currentDoc().columnWidth_.push_back(DEFAULT_COLUMN_WIDTH.toInt());
      }

      if (newLine)
      {
        column = 0;
        currentDoc().height_++;
      }
    }

    evaluateDocument();
    return true;
  }

  bool load(std::string const& filename)
  {
    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
      logError("Could not open document '", filename, "'");
      flashMessage("Could not open document!");
      return false;
    }

    // get length of file:
    file.seekg (0, file.end);
    const int length = file.tellg();
    file.seekg (0, file.beg);

    std::vector<char> buffer;
    buffer.resize(length + 1);
    file.read(&buffer[0], length);
    buffer[length] = '\0';

    if (length == 0)
    {
      logError("No data in file '", filename, "'");
      return false;
    }

    if (!loadDocument(std::string(buffer.begin(), buffer.end()), 0))
    {
      logError("Could not parse document '", filename, "'");
      return false;
    }

    currentDoc().filename_ = filename;
    currentDoc().readOnly_ = false;

    return true;
  }

  bool loadRaw(std::string const& data, std::string const& filename, char delimiter)
  {
    if (!loadDocument(data, delimiter))
      return false;

    currentDoc().filename_ = filename;
    currentDoc().readOnly_ = true;

    return true;
  }

  int getColumnWidth(int column)
  {
    if (column < 0 || column >= currentDoc().width_)
      return -1;

    return currentDoc().columnWidth_[column];
  }

  void setColumnWidth(int column, int width)
  {
    if (column < 0 || column >= currentDoc().width_)
      return;

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::ColumnWidth, true);
    currentDoc().columnWidth_[column] = std::max(3, width);
  }

  int getRowCount()
  {
    return currentDoc().height_;
  }

  int getColumnCount()
  {
    return currentDoc().width_;
  }

  static void collectDependencies(Cell const& cell, Index const& idx, std::unordered_set<Index> & deps)
  {
    if (!cell.hasExpression)
      return;

    for (auto const& expr : cell.expression)
    {
      if (expr.type_ == Expr::Cell)
      {
        const auto result = deps.insert(expr.startIndex_);
        if (result.second)
          collectDependencies(getCell(expr.startIndex_), expr.startIndex_, deps);
      }
      else if (expr.type_ == Expr::Range)
      {
        const Index startIdx = expr.startIndex_;
        const Index endIdx = expr.endIndex_;

        for (int y = startIdx.y; y <= endIdx.y; ++y)
          for (int x = startIdx.x; x <= endIdx.x; ++x)
          {
            const auto result = deps.insert(Index(x, y));
            if (result.second)
              collectDependencies(getCell(Index(x, y)), Index(x, y), deps);
          }
      }
    }
  }

  void evaluateDocument()
  {
    std::unordered_map<Index, std::unordered_set<Index>> cellDeps;
    std::vector<Index> toEvaluate;

    Document & doc = currentDoc();

    for (auto & it : doc.cells_)
    {
      const Index idx = it.first;
      Cell & cell = it.second;

      if (cell.hasExpression)
      {
        if (cell.expression.empty())
        {
          cell.display = "#ERROR";
          cell.value = 0.0;
        }
        else
        {
          toEvaluate.push_back(idx);
          std::unordered_set<Index> & deps = cellDeps[idx];

          collectDependencies(cell, idx, deps);
        }
      }
      else
      {
        cell.display = cell.text;
        cell.value = 0.0;

        try {
          cell.value = std::stod(cell.text);
        } catch (std::exception) {
        }
      }
    }

    // Sort the cells that needs to be evaluated
    std::sort(begin(toEvaluate), end(toEvaluate),
              [&cellDeps] (Index const& a, Index const& b) -> bool
              {
                std::unordered_set<Index> & deps = cellDeps[b];
                return deps.count(a) > 0;
              });

    // Evaluate the cells
    for (auto const& idx : toEvaluate)
    {
      Cell & cell = doc.cells_[idx];

      cell.value = evaluate(cell.expression);
      cell.display = str::fromDouble(cell.value);

      std::unordered_set<Index> & deps = cellDeps[idx];

      FILE * f = _logBegin();

      _logValue(f, "Evaluating cell(", idx.toStr(), ") ", cell.value, " - ", cell.text, " deps: ");
      for (auto cell : deps)
        _logValue(f, cell.toStr(), ", ");

      _logEnd(f);
    }
  }

  std::string getCellText(Index const& idx)
  {
    if (idx.x < 0 || idx.x >= currentDoc().width_ || idx.y < 0 || idx.y >= currentDoc().height_)
      return "";

    Cell const& cell = currentDoc().cells_[idx];
    return getText(cell);
  }

  std::string getCellDisplayText(Index const& idx)
  {
    if (idx.x < 0 || idx.x >= currentDoc().width_ || idx.y < 0 || idx.y >= currentDoc().height_)
      return "";

    Cell const& cell = currentDoc().cells_[idx];

    if (cell.display.empty())
      return getText(cell);
    return cell.display;
  }

  double getCellValue(Index const& idx)
  {
    if (idx.x < 0 || idx.x >= currentDoc().width_ || idx.y < 0 || idx.y >= currentDoc().height_)
      return 0.0;

    return currentDoc().cells_[idx].value;
  }

  uint32_t getCellFormat(Index const& idx)
  {
    if (idx.x < 0 || idx.x >= currentDoc().width_ || idx.y < 0 || idx.y >= currentDoc().height_)
      return 0;

    Cell const& cell = currentDoc().cells_[idx];
    return cell.format;
  }

  void setCellText(Index const& idx, std::string const& text)
  {
    if (idx.x < 0 || idx.x >= currentDoc().width_ || idx.y < 0 || idx.y >= currentDoc().height_)
      return;

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::CellText, false);
    setText(idx, text);
    evaluateDocument();
  }

  void setCellFormat(Index const& idx, uint32_t format)
  {
    if (idx.x < 0 || idx.x >= currentDoc().width_ || idx.y < 0 || idx.y >= currentDoc().height_)
      return;

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::CellText, false);

    Cell & cell = currentDoc().cells_[idx];
    cell.format = format;
  }

  void increaseColumnWidth(int column)
  {
    if (column < 0 || column >= currentDoc().width_)
      return;

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::ColumnWidth, true);

    currentDoc().columnWidth_[column]++;
  }

  void decreaseColumnWidth(int column)
  {
    if (column < 0 || column >= currentDoc().width_)
      return;

    if (currentDoc().readOnly_)
      return;

    if (currentDoc().columnWidth_[column] > 3)
    {
      takeUndoSnapshot(EditAction::ColumnWidth, true);
      currentDoc().columnWidth_[column]--;
    }
  }

  void addColumn(int column)
  {
    column = std::min(column, currentDoc().width_ - 1);

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::AddColumn, true);

    currentDoc().width_++;
    currentDoc().columnWidth_.insert(currentDoc().columnWidth_.begin() + column + 1, DEFAULT_COLUMN_WIDTH.toInt());

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc().cells_)
    {
      if (cell.first.x > column)
        cell.first.x++;

      newCells.insert(cell);
    }

    currentDoc().cells_ = std::move(newCells);
  }

  void addRow(int row)
  {
    row = std::min(row, currentDoc().height_ - 1);

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::AddRow, true);

    currentDoc().height_++;

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc().cells_)
    {
      if (cell.first.y > row)
        cell.first.y++;

      newCells.insert(cell);
    }

    currentDoc().cells_ = std::move(newCells);
  }

  void removeColumn(int column)
  {
    if (column < 0 || column >= getColumnCount())
      return;

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::RemoveColumn, false);

    currentDoc().width_--;

    if (column == currentDoc().columnWidth_.size() - 1)
      currentDoc().columnWidth_.pop_back();
    else
      currentDoc().columnWidth_.erase(currentDoc().columnWidth_.begin() + column);

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc().cells_)
    {
      if (cell.first.x != column)
      {
        if (cell.first.x > column)
          cell.first.x--;

        newCells.insert(cell);
      }
    }

    currentDoc().cells_ = std::move(newCells);
  }

  void removeRow(int row)
  {
    if (row < 0 || row >= getRowCount())
      return;

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::RemoveRow, false);

    currentDoc().height_--;

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc().cells_)
    {
      if (cell.first.y != row)
      {
        if (cell.first.y > row)
          cell.first.y--;

        newCells.insert(cell);
      }
    }

    currentDoc().cells_ = std::move(newCells);
  }

  // -- Tcl bindings --

  TCL_FUNC(createEmpty, "columnCount rowCount", "Create a new empty document with the specified dimensions")
  {
    TCL_CHECK_ARG(3);
    TCL_INT_ARG(1, columnCount);
    TCL_INT_ARG(2, rowCount);

    createEmpty(columnCount, rowCount);
    return JIM_OK;
  }

  TCL_FUNC(createDefaultEmpty, "", "Create a new default created document")
  {
    createDefaultEmpty();
    return JIM_OK;
  }

  TCL_FUNC(closeBuffer, "", "Close the current buffer")
  {
    close();
    return JIM_OK;
  }

  TCL_FUNC(load, "filename", "Open a new document")
  {
    TCL_CHECK_ARG(2);
    TCL_STRING_ARG(1, filename);

    logInfo("Trying to load document ", filename);

    const bool loaded = load(filename);
    TCL_INT_RESULT(loaded ? 1 : 0);
  }

  TCL_FUNC(save, "filename", "Save the current document")
  {
    TCL_CHECK_ARG(2);
    TCL_STRING_ARG(1, filename);

    const bool saved = save(filename);
    TCL_INT_RESULT(saved ? 1 : 0);
  }

  TCL_FUNC(nextBuffer, "", "Switch to the next open buffer")
  {
    nextBuffer();
    return JIM_OK;
  }

  TCL_FUNC(prevBuffer, "", "Switch to the previous buffer")
  {
    previousBuffer();
    return JIM_OK;
  }

  TCL_FUNC(currentBuffer, "?buffer?", "Returns the current buffer and optionally jumps to a specified buffer")
  {
    TCL_CHECK_ARGS(1, 2);
    TCL_INT_ARG(1, buffer);

    if (argc == 2)
      jumpToBuffer(buffer);

    TCL_INT_RESULT(currentBufferIndex());
  }

  TCL_FUNC(undo, "", "Undo the last command")
  {
    if (undo())
      return JIM_OK;
    return JIM_ERR;
  }

  TCL_FUNC(redo, "", "Redo the last undone command")
  {
    if (redo())
      return JIM_OK;
    return JIM_ERR;
  }

  TCL_FUNC(columnCount, "", "Returns the column count of the current document")
  {
    TCL_INT_RESULT(currentDoc().width_);
  }

  TCL_FUNC(rowCount, "", "Returns the row count of the current document")
  {
    TCL_INT_RESULT(currentDoc().height_);
  }

  TCL_FUNC(addColumn, "column", "Add a new column to the current document after the indicated column")
  {
    TCL_CHECK_ARG(2);
    TCL_INT_ARG(1, column);

    addColumn(column);
    return JIM_OK;
  }

  TCL_FUNC(addRow, "row", "Add a new row to the current document after the indicated row")
  {
    TCL_CHECK_ARG(2);
    TCL_INT_ARG(1, row);

    addRow(row);
    return JIM_OK;
  }

  TCL_FUNC(delimiter, "?delimiter?", "Set or return the delimiter to use in the current document")
  {
    TCL_CHECK_ARGS(1, 2);
    TCL_STRING_ARG(1, delims);

    if (argc == 1)
      TCL_STRING_RESULT(std::string(1, currentDoc().delimiter_));
    else if (argc == 2)
      currentDoc().delimiter_ = delims.empty() ? ',' : delims.front();

    return JIM_OK;
  }

  TCL_FUNC(filename, "", "Returns the filename of the current document")
  {
    TCL_STRING_RESULT(currentDoc().filename_);
  }

  TCL_FUNC(columnWidth, "column ?width?", "This function returns and optionally sets the width of the specified column.")
  {
    TCL_CHECK_ARGS(2, 3);
    TCL_STRING_ARG(1, columnStr);
    TCL_INT_ARG(2, width);

    const int column = Index::strToColumn(columnStr);

    if (argc == 3)
      setColumnWidth(column, width);

    TCL_INT_RESULT(getColumnWidth(column));
  }

  TCL_FUNC(cell, "index ?value?", "Set or return the value of a particular cell in the current document")
  {
    TCL_CHECK_ARGS(2, 3);
    TCL_STRING_ARG(1, index);
    TCL_STRING_ARG(2, value);

    const Index idx = Index::fromStr(index);

    if (argc == 3)
      setCellText(idx, value);

    TCL_STRING_UTF8_RESULT(getCellText(idx));
  }

  enum class FilterOp
  {
    Equal,
    NotEqual,
    Match,
    NoMatch,
    Greater,
    LessThan
  };

  TCL_FUNC(filter, "?-noHeader? column operation value ?column operation value ...?")
  {
    TCL_CHECK_ARGS(4, 1000);

    bool copyHeader = true;
    if (std::string(Jim_String(argv[1])) == "-noHeader")
      copyHeader = false;

    if (!copyHeader && argc < 5)
      return JIM_ERR;

    int column = 0;
    FilterOp op = FilterOp::Match;
    std::vector<std::tuple<int, FilterOp, std::string>> matches;

    for (int field = 0, i = copyHeader ? 1 : 2; i < argc; ++field, ++i)
    {
      const std::string value(Jim_String(argv[i]));

      switch (field)
      {
        case 0:
          {
            column = Index::strToColumn(value);
            if (column < 0 || column >= getColumnCount())
            {
              logError("filter column ", column, " out of range");
              return JIM_ERR;
            }
          }
          break;

        case 1:
          {
            if (value == "-equal")
              op = FilterOp::Equal;
            else if (value == "-nequal")
              op = FilterOp::NotEqual;
            else if (value == "-match")
              op = FilterOp::Match;
            else if (value == "-nomatch")
              op = FilterOp::NoMatch;
            else if (value == "-gt")
              op = FilterOp::Greater;
            else if (value == "-lt")
              op = FilterOp::LessThan;
            else
            {
              logError("unknown filter operation '", value, "'");
              return JIM_ERR;
            }
          }
          break;

        case 2:
          {
            matches.push_back(std::make_tuple(column, op, value));
            field = 0;
          }
          break;
      }
    }

    Document & doc = currentDoc();
    Buffer buffer;

    buffer.doc_.width_ = doc.width_;
    buffer.doc_.columnWidth_ = doc.columnWidth_;
    buffer.doc_.delimiter_ = doc.delimiter_;
    buffer.doc_.filename_ = "[No Name]";
    buffer.doc_.readOnly_ = false;

    if (copyHeader)
    {
      for (int i = 0; i < doc.width_; ++i)
        buffer.doc_.cells_[Index(i, 0)] = doc.cells_[Index(i, 0)];
    }

    int row = copyHeader ? 1 : 0;
    for (int y = copyHeader ? 1 : 0; y < doc.height_; ++y)
    {
      bool include = true;

      for (auto const& it : matches)
      {
        int column;
        FilterOp op;
        std::string value;
        std::tie(column, op, value) = it;

        const std::string text = getCellDisplayText(Index(column, y));
        if (text.empty() || value.empty())
        {
          include = false;
          break;
        }

        switch (op)
        {
          case FilterOp::Equal:
            include &= text == value;
            break;

          case FilterOp::NotEqual:
            include &= text != value;
            break;

          case FilterOp::Match:
            include &= text.find(value) != std::string::npos;
            break;

          case FilterOp::NoMatch:
            include &= text.find(value) == std::string::npos;
            break;

          case FilterOp::Greater:
            {
              try {
                include &= std::stod(text) > std::stod(value);
              } catch (std::exception e) {
                logError("could not make comparison ", text, " > ", value);
                return JIM_ERR;
              }
            }
            break;

          case FilterOp::LessThan:
              try {
                include &= std::stod(text) < std::stod(value);
              } catch (std::exception e) {
                logError("could not make comparison ", text, " < ", value);
                return JIM_ERR;
              }
            break;
        }
      }

      if (include)
      {
        for (int i = 0; i < doc.width_; ++i)
          buffer.doc_.cells_[Index(i, row)] = doc.cells_[Index(i, y)];
        ++row;
      }
    }

    buffer.doc_.height_ = row;

    documentBuffers().push_back(buffer);
    jumpToBuffer(documentBuffers().size() - 1);

    return JIM_OK;
  }

}
