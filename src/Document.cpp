
#include "Document.h"
#include "Str.h"
#include "Types.h"
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

  static std::list<Buffer> & documentBuffer()
  {
    static std::list<Buffer> buffer;
    return buffer;
  }

  static Buffer & currentBuffer()
  {
    assert(!documentBuffer().empty());
    return documentBuffer().front();
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

  void createDefaultEmpty()
  {
    documentBuffer().push_front({});
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
    documentBuffer().pop_front();

    if (documentBuffer().empty())
      createDefaultEmpty();
  }

  static Cell & getCell(Index const& idx)
  {
    return currentDoc().cells_[idx];
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
        Str const& cell = getCellText(Index(x, y));
        file << cell.utf8() << (x < (currentDoc().width_ - 1) ? (char)currentDoc().delimiter_ : '\n');
      }
    }

    currentDoc().filename_ = filename;
    flashMessage("Document saved!");
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
      for (auto ch : data)
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
          onlyNumbers &= std::isdigit(ch);
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
          Cell & cell = getCell(Index(i, 0));
          cell.text = cells[i];

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
      {
        Cell & cell = getCell(Index(column, currentDoc().height_));
        cell.text = cellText;
      }

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

    return true;
  }

  bool load(std::string const& filename)
  {
    std::ifstream file(filename.c_str());
    if (!file.is_open())
    {
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

    // Did we succeed in reading the file
    if (!file)
      return false;

    if (length == 0)
      return false;

    if (!loadDocument(std::string(buffer.begin(), buffer.end()), 0))
      return false;

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
    assert(column < currentDoc().width_);
    return currentDoc().columnWidth_[column];
  }

  void setColumnWidth(int column, int width)
  {
    if (column >= currentDoc().width_)
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

  std::string getCellText(Index const& idx)
  {
    if (idx.x < 0 || idx.x >= currentDoc().width_)
      return "";

    if (idx.y < 0 || idx.y >= currentDoc().height_)
      return "";

    return currentDoc().cells_[idx].text;
  }

  void setCellText(Index const& idx, std::string const& text)
  {
    assert(idx.x < currentDoc().width_);
    assert(idx.y < currentDoc().height_);

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::CellText, false);

    Cell & cell = getCell(idx);
    cell.text = text;
  }

  void increaseColumnWidth(int column)
  {
    assert(column < currentDoc().width_);

    if (currentDoc().readOnly_)
      return;

    takeUndoSnapshot(EditAction::ColumnWidth, true);

    currentDoc().columnWidth_[column]++;
  }

  void decreaseColumnWidth(int column)
  {
    assert(column < currentDoc().width_);

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
    assert(column < doc::getColumnCount());

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
    assert(row < getRowCount());

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

  TCL_FUNC(doc_createEmpty, "columnCount rowCount", "Create a new empty document with the specified dimensions")
  {
    TCL_CHECK_ARG(3, "columnCount rowCount");
    TCL_INT_ARG(1, columnCount);
    TCL_INT_ARG(2, rowCount);

    createEmpty(columnCount, rowCount);
    return JIM_OK;
  }

  TCL_FUNC(doc_createDefaultEmpty, "", "Create a new default created document")
  {
    createDefaultEmpty();
    return JIM_OK;
  }

  TCL_FUNC(doc_closeBuffer, "", "Close the current buffer")
  {
    close();
    return JIM_OK;
  }

  TCL_FUNC(doc_nextBuffer, "", "Switch to the next open buffer")
  {
    documentBuffer().push_back(currentBuffer());
    documentBuffer().pop_front();
    return JIM_OK;
  }

  TCL_FUNC(doc_prevBuffer, "", "Switch to the previous buffer")
  {
    documentBuffer().push_front(documentBuffer().back());
    documentBuffer().pop_back();
    return JIM_OK;
  }

  TCL_FUNC(doc_undo, "Undo the last command")
  {
    if (undo())
      return JIM_OK;
    return JIM_ERR;
  }

  TCL_FUNC(doc_redo, "", "Redo the last undone command")
  {
    if (redo())
      return JIM_OK;
    return JIM_ERR;
  }

  TCL_FUNC(doc_columnCount, "", "Returns the column count of the current document")
  {
    TCL_INT_RESULT(currentDoc().width_);
  }

  TCL_FUNC(doc_rowCount, "", "Returns the row count of the current document")
  {
    TCL_INT_RESULT(currentDoc().height_);
  }

  TCL_FUNC(doc_addColumn, "column", "Add a new column to the current document after the indicated column")
  {
    TCL_CHECK_ARG(2, "column");
    TCL_INT_ARG(1, column);

    addColumn(column);
    return JIM_OK;
  }

  TCL_FUNC(doc_addRow, "row", "Add a new row to the current document after the indicated row")
  {
    TCL_CHECK_ARG(2, "row");
    TCL_INT_ARG(1, row);

    addRow(row);
    return JIM_OK;
  }

  TCL_FUNC(doc_delimiter, "?delimiter?", "Set or return the delimiter to use in the current document")
  {
    TCL_CHECK_ARGS(1, 2, "?delimiter?");
    TCL_STRING_ARG(1, delims);

    if (argc == 1)
      TCL_STRING_RESULT(std::string(1, currentDoc().delimiter_));
    else if (argc == 2)
      currentDoc().delimiter_ = delims.empty() ? ',' : delims.front();

    return JIM_OK;
  }

  TCL_FUNC(doc_filename, "", "Returns the filename of the current document")
  {
    TCL_STRING_RESULT(currentDoc().filename_);
  }

  TCL_FUNC(doc_columnWidth, "column ?width?", "This function returns and optionally sets the width of the specified column.")
  {
    TCL_CHECK_ARGS(2, 3, "column ?width?");
    TCL_INT_ARG(1, column);
    TCL_INT_ARG(2, width);

    if (argc == 3)
      setColumnWidth(column, width);

    TCL_INT_RESULT(getColumnWidth(column));
  }

  TCL_FUNC(doc_cell, "index ?value?", "Set or return the value of a particular cell in the current document")
  {
    TCL_CHECK_ARGS(2, 3, "index ?value?");
    TCL_STRING_ARG(1, index);
    TCL_STRING_ARG(2, value);

    const Index idx = Index::fromStr(index);

    if (argc == 3)
      setCellText(idx, value);

    TCL_STRING_RESULT(getCellText(idx));
  }

}
