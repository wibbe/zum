
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

  static const tcl::Variable DELIMITERS("app:delimiters", ",;");
  static const tcl::Variable IGNORE_FIRST_ROW("app:ignore_first_row", "true");
  static const tcl::Variable DEFAULT_ROW_COUNT("doc:default_row_count", "10");
  static const tcl::Variable DEFAULT_COLUMN_COUNT("doc:default_column_count", "5");
  static const tcl::Variable DEFAULT_COLUMN_WIDTH("doc:default_column_width", "20");

  struct Document
  {
    int width_ = 0;
    int height_ = 0;
    std::vector<int> columnWidth_ = {0};
    std::unordered_map<Index, Cell> cells_;
    Str filename_;
    bool readOnly_ = false;
    Str::char_type delimiter_;
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

  FUNC_0(nextBuffer, "doc:nextBuffer")
  {
    documentBuffer().push_back(currentBuffer());
    documentBuffer().pop_front();
  }

  FUNC_0(prevBuffer, "doc:prevBuffer")
  {
    documentBuffer().push_front(documentBuffer().back());
    documentBuffer().pop_back();
  }

  FUNC_0(createDefaultEmpty, "doc:createDefaultEmpty")
  {
    documentBuffer().push_front({});
    currentDoc().width_ = std::max(DEFAULT_ROW_COUNT.toInt(), 1);
    currentDoc().height_ = std::max(DEFAULT_COLUMN_COUNT.toInt(), 1);
    currentDoc().columnWidth_.resize(DEFAULT_COLUMN_COUNT.toInt(), DEFAULT_COLUMN_WIDTH.toInt());
    currentDoc().filename_ = Str("[No Name]");
    currentDoc().delimiter_ = ',';
    currentDoc().readOnly_ = false;
    return true;
  }

  FUNC_2(createEmpty, "doc:createEmpty", "doc:createEmpty columnCount rowCount")
  {
    createDefaultEmpty();
    currentDoc().width_ = std::max(arg1.toInt(), 1);
    currentDoc().height_ = std::max(arg2.toInt(), 1);
    currentDoc().columnWidth_.resize(currentDoc().width_, DEFAULT_COLUMN_WIDTH.toInt());
    return true;
  }

  FUNC_0(close, "doc:closeBuffer")
  {
    documentBuffer().pop_front();

    if (documentBuffer().empty())
      createDefaultEmpty();

    return true;
  }

  static Cell & getCell(Index const& idx)
  {
    auto result = currentDoc().cells_.find(idx);
    if (result == currentDoc().cells_.end())
      result = currentDoc().cells_.emplace(idx, Cell()).first;

    return result->second;
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

  FUNC_0(undo, "doc:undo")
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

  FUNC_0(redo, "doc:redo")
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

  Str getFilename()
  {
    return currentDoc().filename_;
  }

  bool isReadOnly()
  {
    return currentDoc().readOnly_;
  }

  FUNC_1(save, "doc:save", "doc:save filename")
  {
    logInfo(Str("Saving document: ").append(arg1));

    std::ofstream file(arg1.utf8().c_str());
    if (!file.is_open())
    {
      flashMessage(Str("Could not save document!"));
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

    currentDoc().filename_ = arg1;
    flashMessage(Str("Document saved!"));
    return true;
  }

  struct Parser
  {
    Parser(Str const& data, Str::char_type delim)
      : data_(data),
        delim_(delim)
    { }

    bool next(Str & value)
    {
      value.clear();

      if (eof())
        return false;

      while (!eof() && data_[pos_] != delim_ && data_[pos_] != '\n')
        value.append(data_[pos_++]);

      if (eof())
        return false;

      const bool newLine = data_[pos_++] == '\n';
      return !newLine;
    }

    bool eof() const { return pos_ >= data_.size(); }

    Str const& data_;
    Str::char_type delim_;
    int pos_ = 0;
  };

  static bool loadDocument(Str const& data, Str::char_type defaultDelimiter)
  {
    createDefaultEmpty();
    currentDoc().width_ = 0;
    currentDoc().height_ = 0;
    currentDoc().columnWidth_.resize(0);

    // Examin document to determin delimiter type
    if (defaultDelimiter == 0)
    {
      const Str delimiters = DELIMITERS.get();

      std::vector<int> delimCount(delimiters.size(), 0);
      for (auto ch : data)
      {
        const int idx = delimiters.findChar(ch);
        if (idx >= 0)
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

      std::vector<Str> cells;

      while (firstLine)
      {
        Str cellText;
        firstLine = p.next(cellText);

        cells.push_back(cellText);

        for (auto ch : cellText)
          onlyNumbers &= isDigit(ch);
      }

      if (onlyNumbers)
      {
        for (auto const& cellText : cells)
        {
          const int width = cellText.toInt();
          currentDoc().columnWidth_.push_back(width > 0 ? width : DEFAULT_COLUMN_WIDTH.toInt());
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
      Str cellText;
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

  FUNC_1(load, "doc:load", "doc:load filename")
  {
    std::ifstream file(arg1.utf8().c_str());
    if (!file.is_open())
    {
      flashMessage(Str("Could not open document!"));
      return false;
    }

    // get length of file:
    file.seekg (0, file.end);
    const int length = file.tellg();
    file.seekg (0, file.beg);

    std::vector<char> buffer;
    buffer.reserve(length + 1);
    file.read(&buffer[0], length);
    buffer[length] = '\0';

    // Did we succeed in reading the file
    if (!file)
      return false;

    if (length == 0)
      return false;

    const Str data(&buffer[0]);
    if (!loadDocument(data, 0))
      return false;

    currentDoc().filename_ = arg1;
    currentDoc().readOnly_ = false;

    return true;
  }

  bool loadRaw(std::string const& data, Str const& filename, Str::char_type delimiter)
  {
    if (!loadDocument(Str(data.c_str()), delimiter))
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

  static int get_cell_index(int x, int y)
  {
    return (y * currentDoc().width_) + x;
  }

  Str const& getCellText(Index const& idx)
  {
    assert(idx.x < currentDoc().width_);
    assert(idx.y < currentDoc().height_);

    auto result = currentDoc().cells_.find(idx);
    if (result == currentDoc().cells_.end())
      return Str::EMPTY;

    return result->second.text;
  }

  void setCellText(Index const& idx, Str const& text)
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

  // -- Tcl procs --

  TCL_PROC2(docDelimiter, "doc:delimiter")
  {
    TCL_CHECK_ARGS(1, 2, "doc:delimiter ?delimiter?");

    if (args.size() == 1)
      return tcl::resultStr(Str(currentDoc().delimiter_));
    else if (args.size() >= 2)
      currentDoc().delimiter_ = args[1].front();

    TCL_OK();
  }

  TCL_PROC2(docFilename, "doc:filename")
  {
    TCL_CHECK_ARG(1, "doc:filename");
    return tcl::resultStr(currentDoc().filename_);
  }

  TCL_PROC2(docColumWidth, "doc:columnWidth")
  {
    TCL_CHECK_ARGS(2, 3, "doc:columnWidth column ?width?");

    const int column = args[1].toInt();

    if (args.size() == 3)
      setColumnWidth(column, args[2].toInt());

    return tcl::resultInt(getColumnWidth(column));
  }

  TCL_PROC2(docColumnCount, "doc:columnCount")
  {
    TCL_CHECK_ARG(1, "doc:columnCount");
    return tcl::resultInt(currentDoc().width_);
  }

  TCL_PROC2(docRowCount, "doc:rowCount")
  {
    TCL_CHECK_ARG(1, "doc:rowCount");
    return tcl::resultInt(currentDoc().height_);
  }

  TCL_PROC2(docAddRow, "doc:addRow")
  {
    TCL_CHECK_ARG(2, "doc:addRow row");
    addRow(args[1].toInt());
    TCL_OK();
  }

  TCL_PROC2(docAddColumn, "doc:addColumn")
  {
    TCL_CHECK_ARG(2, "doc:addColumn column");
    addColumn(args[1].toInt());
    TCL_OK();
  }

  TCL_PROC2(docCell, "doc:cell")
  {
    TCL_CHECK_ARGS(2, 3, "doc:cell index ?value?");
    const Index idx = Index::fromStr(args[1]);

    if (args.size() == 3)
      setCellText(idx, args[2]);

    return tcl::resultStr(getCellText(idx));
  }
}
