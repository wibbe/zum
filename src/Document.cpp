
#include "Document.h"
#include "Str.h"
#include "Types.h"
#include "Editor.h"

#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cmath>

#include "Tcl.h"

namespace doc {

  static const int kDefaultRowCount = 10;
  static const int kDefaultColumnCount = 10;
  static const int kDefaultColumnWidth = 12;

  static const tcl::Variable DELIMITERS("app:delimiters", ",;");
  static const tcl::Variable FIRST_ROW_IS_COLUMN_WIDTH("app:first_row_is_column_width", "1");

  struct Document
  {
    int width_;
    int height_;
    std::vector<int> columnWidth_;
    std::unordered_map<Index, Cell> cells_;
    Str filename_;
    bool readOnly_;
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

  static Document currentDoc_;
  static std::vector<UndoState> undoStack_;
  static std::vector<UndoState> redoStack_;

  FUNC_1(createEmpty, "doc:createEmpty")
  {
    close();
    currentDoc_.width_ = kDefaultRowCount;
    currentDoc_.height_ = kDefaultColumnCount;
    currentDoc_.columnWidth_.resize(kDefaultColumnCount, kDefaultColumnWidth);
    currentDoc_.filename_ = arg1;
    currentDoc_.delimiter_ = ',';
    currentDoc_.readOnly_ = false;
    return true;
  }

  FUNC_0(close, "doc:close")
  {
    currentDoc_.width_ = 0;
    currentDoc_.height_ = 0;
    currentDoc_.filename_ = Str::EMPTY;
    currentDoc_.delimiter_ = ',';
    currentDoc_.cells_.clear();
    currentDoc_.columnWidth_.clear();
    currentDoc_.readOnly_ = true;
    undoStack_.clear();
    redoStack_.clear();
    return true;
  }

  static Cell & getCell(Index const& idx)
  {
    auto result = currentDoc_.cells_.find(idx);
    if (result == currentDoc_.cells_.end())
      result = currentDoc_.cells_.emplace(idx, Cell()).first;

    return result->second;
  }

  static void takeUndoSnapshot(EditAction action, bool canMerge)
  {
    bool createNew = true;

    if (!undoStack_.empty())
    {
      UndoState & state = undoStack_.back();

      if (state.action_ == action && canMerge)
        createNew = false;
    }

    if (createNew)
    {
      undoStack_.emplace_back(currentDoc_, getCursorPos(), action);
      redoStack_.clear();
    }
  }

  FUNC_0(undo, "doc:undo")
  {
    if (!undoStack_.empty())
    {
      redoStack_.emplace_back(currentDoc_, getCursorPos(), EditAction::UndoRedo);

      UndoState const& state = undoStack_.back();
      currentDoc_ = state.doc_;
      setCursorPos(state.cursor_);

      undoStack_.pop_back();

      return true;
    }

    return false;
  }

  FUNC_0(redo, "doc:redo")
  {
    if (!redoStack_.empty())
    {
      undoStack_.emplace_back(currentDoc_, getCursorPos(), EditAction::UndoRedo);

      UndoState const& state = redoStack_.back();
      currentDoc_ = state.doc_;
      setCursorPos(state.cursor_);

      redoStack_.pop_back();

      return true;
    }

    return false;
  }

  Str getFilename()
  {
    return currentDoc_.filename_;
  }

  bool isReadOnly()
  {
    return currentDoc_.readOnly_;
  }

  FUNC_1(save, "doc:save")
  {
    logInfo(Str("Saving document: ").append(arg1));

    std::ofstream file(arg1.utf8().c_str());
    if (!file.is_open())
    {
      flashMessage(Str("Could not save document!"));
      return false;
    }

    // Write header info
    if (FIRST_ROW_IS_COLUMN_WIDTH.get().toInt() > 0)
      for (int i = 0; i < currentDoc_.width_; ++i)
        file << currentDoc_.columnWidth_[i] << (i < (currentDoc_.width_ - 1) ? (char)currentDoc_.delimiter_ : '\n');

    // Write all the cells
    for (int y = 0; y < currentDoc_.height_; ++y)
    {
      for (int x = 0; x < currentDoc_.width_; ++x)
      {
        Str const& cell = getCellText(Index(x, y));
        file << cell.utf8() << (x < (currentDoc_.width_ - 1) ? (char)currentDoc_.delimiter_ : '\n');
      }
    }

    currentDoc_.filename_ = arg1;
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

  static bool loadDocument(Str const& data)
  {
    // Examin document to determin delimiter type
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
      currentDoc_.delimiter_ = delimiters[0];
      for (int i = 0; i < delimCount.size(); ++i)
      {
        if (delimCount[i] > maxCount)
        {
          maxCount = delimCount[i];
          currentDoc_.delimiter_ = delimiters[i];
        }
      }
    }

    Parser p(data, currentDoc_.delimiter_);

    // Read column width
    if (FIRST_ROW_IS_COLUMN_WIDTH.get().toInt() > 0)
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
          currentDoc_.columnWidth_.push_back(width > 0 ? width : kDefaultColumnWidth);
        }

        currentDoc_.width_ = currentDoc_.columnWidth_.size();
        currentDoc_.height_ = 0;
      }
      else
      {
        for (uint32_t i = 0; i < cells.size(); ++i)
        {
          Cell & cell = getCell(Index(i, 0));
          cell.text = cells[i];

          currentDoc_.columnWidth_.push_back(kDefaultColumnWidth);
        }

        currentDoc_.width_ = cells.size();
        currentDoc_.height_ = 1;
      }
    }

    int column = 0;
    while (!p.eof())
    {
      Str cellText;
      const bool newLine = !p.next(cellText);

      if (!cellText.empty())
      {
        Cell & cell = getCell(Index(column, currentDoc_.height_));
        cell.text = cellText;
      }

      column++;

      if (column > currentDoc_.width_)
      {
        currentDoc_.width_ = column;
        currentDoc_.columnWidth_.push_back(kDefaultColumnWidth);
      }

      if (newLine)
      {
        column = 0;
        currentDoc_.height_++;
      }
    }

    return true;
  }

  FUNC_1(load, "doc:load")
  {
    logInfo(Str("Loading document: ").append(arg1));

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

    // Start by closing thr current document
    close();

    const Str data(&buffer[0]);
    if (!loadDocument(data))
      return false;

    currentDoc_.filename_ = arg1;
    currentDoc_.readOnly_ = false;

    return true;
  }

  bool loadRaw(std::string const& data, Str const& filename)
  {
    close();

    if (!loadDocument(Str(data.c_str())))
      return false;

    currentDoc_.filename_ = filename;
    currentDoc_.readOnly_ = true;

    return true;
  }

  int getColumnWidth(int column)
  {
    assert(column < currentDoc_.width_);
    return currentDoc_.columnWidth_[column];
  }

  int getRowCount()
  {
    return currentDoc_.height_;
  }

  int getColumnCount()
  {
    return currentDoc_.width_;
  }

  static int get_cell_index(int x, int y)
  {
    return (y * currentDoc_.width_) + x;
  }

  Str const& getCellText(Index const& idx)
  {
    assert(idx.x < currentDoc_.width_);
    assert(idx.y < currentDoc_.height_);

    auto result = currentDoc_.cells_.find(idx);
    if (result == currentDoc_.cells_.end())
      return Str::EMPTY;

    return result->second.text;
  }

  void setCellText(Index const& idx, Str const& text)
  {
    assert(idx.x < currentDoc_.width_);
    assert(idx.y < currentDoc_.height_);

    if (currentDoc_.readOnly_)
      return;

    takeUndoSnapshot(EditAction::CellText, false);

    Cell & cell = getCell(idx);
    cell.text = text;
  }

  void increaseColumnWidth(int column)
  {
    assert(column < currentDoc_.width_);

    if (currentDoc_.readOnly_)
      return;

    takeUndoSnapshot(EditAction::ColumnWidth, true);

    currentDoc_.columnWidth_[column]++;
  }

  void decreaseColumnWidth(int column)
  {
    assert(column < currentDoc_.width_);

    if (currentDoc_.readOnly_)
      return;

    if (currentDoc_.columnWidth_[column] > 3)
    {
      takeUndoSnapshot(EditAction::ColumnWidth, true);
      currentDoc_.columnWidth_[column]--;
    }
  }

  void addColumn(int column)
  {
    assert(column < currentDoc_.width_);

    if (currentDoc_.readOnly_)
      return;

    takeUndoSnapshot(EditAction::AddColumn, true);

    currentDoc_.width_++;
    currentDoc_.columnWidth_.insert(currentDoc_.columnWidth_.begin() + column + 1, kDefaultColumnWidth);

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc_.cells_)
    {
      if (cell.first.x > column)
        cell.first.x++;

      newCells.insert(cell);
    }

    currentDoc_.cells_ = std::move(newCells);
  }

  void addRow(int row)
  {
    assert(row < currentDoc_.height_);

    if (currentDoc_.readOnly_)
      return;

    takeUndoSnapshot(EditAction::AddRow, true);

    currentDoc_.height_++;

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc_.cells_)
    {
      if (cell.first.y > row)
        cell.first.y++;

      newCells.insert(cell);
    }

    currentDoc_.cells_ = std::move(newCells);
  }

  void removeColumn(int column)
  {
    assert(column < doc::getColumnCount());

    if (currentDoc_.readOnly_)
      return;

    takeUndoSnapshot(EditAction::RemoveColumn, false);

    currentDoc_.width_--;

    if (column == currentDoc_.columnWidth_.size() - 1)
      currentDoc_.columnWidth_.pop_back();
    else
      currentDoc_.columnWidth_.erase(currentDoc_.columnWidth_.begin() + column);

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc_.cells_)
    {
      if (cell.first.x != column)
      {
        if (cell.first.x > column)
          cell.first.x--;

        newCells.insert(cell);
      }
    }

    currentDoc_.cells_ = std::move(newCells);
  }

  void removeRow(int row)
  {
    assert(row < getRowCount());

    if (currentDoc_.readOnly_)
      return;

    takeUndoSnapshot(EditAction::RemoveRow, false);

    currentDoc_.height_--;

    std::unordered_map<Index, Cell> newCells;

    for (std::pair<Index, Cell> cell : currentDoc_.cells_)
    {
      if (cell.first.y != row)
      {
        if (cell.first.y > row)
          cell.first.y--;

        newCells.insert(cell);
      }
    }

    currentDoc_.cells_ = std::move(newCells);
  }

  Str rowToLabel(int row)
  {
    return Str::format("%d", row + 1);
  }

  Str columnToLabel(int col)
  {
    int power = 0;
    for (int i = 0; i < 10; ++i)
    {
      const int value = std::pow(26, i);
      if (value > col)
      {
        power = i;
        break;
      }
    }

    Str result;
    while (power >= 0)
    {
      const int value = std::pow(26, power);
      if (value > col)
      {
        power--;

        if (power < 0)
          result.append('A');
      }
      else
      {
        const int division = col / value;
        const int remainder = col % value;

        result.append('A' + division);

        col = remainder;
        power--;
      }
    }

    return result;
  }

  Index parseCellRef(Str const& str)
  {
    Index idx(0, 0);

    if (str.empty())
      return idx;

    int i = 0;
    while ((i < str.size()) && isUpperAlpha(str[i]))
    {
      idx.x *= 26;
      idx.x += str[i] - 'A';
      i++;
    }

    while ((i < str.size()) && isDigit(str[i]))
    {
      idx.y *= 10;
      idx.y += str[i] - '0';
      i++;
    }

    if (idx.y > 0)
      idx.y--;

    if (idx.x >= getColumnCount())
      idx.x = getColumnCount() - 1;
    if (idx.y >= getRowCount())
      idx.y = getRowCount() - 1;

    return idx;
  }

  // -- Tcl procs --

  TCL_PROC2(doc_delimiter, "doc:delimiter")
  {
    if (args.size() == 1)
    {
      Str result;
      result.append(currentDoc_.delimiter_);
      TCL_RETURN(result);
    }
    else if (args.size() >= 2)
    {
      currentDoc_.delimiter_ = args[1].front();
    }

    TCL_OK();
  }
}
