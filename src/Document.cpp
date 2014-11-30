
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

namespace doc {

  static const int kDefaultRowCount = 10;
  static const int kDefaultColumnCount = 10;
  static const int kDefaultColumnWidth = 12;

  struct Document
  {
    int width_;
    int height_;
    std::vector<int> columnWidth_;
    std::unordered_map<Index, Cell> cells_;
    Str filename_;
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

  void createEmpty()
  {
    close();
    currentDoc_.width_ = kDefaultRowCount;
    currentDoc_.height_ = kDefaultColumnCount;
    currentDoc_.columnWidth_.resize(kDefaultColumnCount, kDefaultColumnWidth);
    currentDoc_.filename_ = Str::EMPTY;
  }

  void close()
  {
    currentDoc_.width_ = 0;
    currentDoc_.height_ = 0;
    currentDoc_.filename_ = Str::EMPTY;
    currentDoc_.cells_.clear();
    currentDoc_.columnWidth_.clear();
    undoStack_.clear();
    redoStack_.clear();
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

  bool undo()
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

  bool redo()
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

  bool save(Str const& filename)
  {
    std::ofstream file(filename.utf8().c_str());
    if (!file.is_open())
      return false;

    // Write header info
    for (int i = 0; i < currentDoc_.width_; ++i)
      file << currentDoc_.columnWidth_[i] << (i < (currentDoc_.width_ - 1) ? "," : "");

    file << std::endl;

    // Write all the cells
    for (int y = 0; y < currentDoc_.height_; ++y)
    {
      for (int x = 0; x < currentDoc_.width_; ++x)
      {
        Str const& cell = getCellText(Index(x, y));
        file << cell.utf8() << (x < (currentDoc_.width_ - 1) ? "," : "");
      }

      file << std::endl;
    }

    currentDoc_.filename_ = filename;
    return true;
  }

  bool load(Str const& filename)
  {
    std::ifstream file(filename.utf8().c_str());
    if (!file.is_open())
      return false;

    // Start by closing thr current document
    close();

    { // Read column width
      std::string line, cell;
      std::getline(file, line);
      std::stringstream lineStream(line);

      while (std::getline(lineStream, cell, ','))
      {
        const int width = std::atoi(cell.c_str());
        currentDoc_.columnWidth_.push_back(width > 0 ? width : kDefaultColumnWidth);
      }

      currentDoc_.width_ = currentDoc_.columnWidth_.size();
      currentDoc_.height_ = 0;
    }

    { // Read cell data
      // For each line
      std::string line;
      while (std::getline(file, line))
      {
        std::stringstream lineStream(line);
        std::string cellText;
        int column = 0;

        while (std::getline(lineStream, cellText, ','))
        {
          if (!cellText.empty())
          {
            Cell & cell = getCell(Index(column, currentDoc_.height_));
            cell.text.set(cellText.c_str());
          }

          column++;

          if (column > currentDoc_.width_)
          {
            currentDoc_.width_ = column;
            currentDoc_.columnWidth_.push_back(kDefaultColumnWidth);
          }
        }

        currentDoc_.height_++;
      }
    }

    currentDoc_.filename_ = filename;

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

    takeUndoSnapshot(EditAction::CellText, false);

    Cell & cell = getCell(idx);
    cell.text = text;
  }

  void increaseColumnWidth(int column)
  {
    assert(column < currentDoc_.width_);
    takeUndoSnapshot(EditAction::ColumnWidth, true);

    currentDoc_.columnWidth_[column]++;
  }

  void decreaseColumnWidth(int column)
  {
    assert(column < currentDoc_.width_);

    if (currentDoc_.columnWidth_[column] > 3)
    {
      takeUndoSnapshot(EditAction::ColumnWidth, true);
      currentDoc_.columnWidth_[column]--;
    }
  }

  void addColumn(int column)
  {
    assert(column < currentDoc_.width_);
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
    return Str::format("%5d", row + 1);
  }

  Str columnTolabel(int col)
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
        //result.append('A');
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

    // Special case to handle col == 0
    //if (result.empty())
    //  result.append('A');

    return result;
/*

    do
    {
      const int reminder = col % 26;
      if (reminder > 0)
      {
        result.append('A' + (reminder - 1));
        col -= reminder * 26;
      }
      else
      {
        result.append('A' + (col - 1));
        col -= col;
      }
    }
    while (col > 0);


    */
  }

}
