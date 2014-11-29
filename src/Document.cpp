
#include "Document.h"
#include "Str.h"
#include "Types.h"

#include <assert.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <utility>
#include <unordered_map>

namespace doc {

  static const int kDefaultRowCount = 10;
  static const int kDefaultColumnCount = 10;
  static const int kDefaultColumnWidth = 12;

  #define CELL_DATA_LEN 256

  static int width_;
  static int height_;
  static std::vector<int> columnWidth_;
  static std::unordered_map<Index, Cell> cells_;
  static Str filename_;

  static Cell ** doc_cells;
  static String doc_filename;

  static int cellCount()
  {
    return width_ * height_;
  }

  void createEmpty()
  {
    width_ = kDefaultRowCount;
    height_ = kDefaultColumnCount;
    columnWidth_.resize(kDefaultColumnCount, kDefaultColumnWidth);
    cells_.clear();
    filename_.set("[NoName]");

    for (int i = 0; i < width_; ++i)
      columnWidth_[i] = kDefaultColumnWidth;
  }

  void close()
  {
    cells_.clear();
    columnWidth_.clear();
  }

  Str const& getFilename()
  {
    return filename_;
  }

  static void write_row(FILE * file, int row)
  {
    char cell[STRING_LEN];

    for (int column = 0; column < width_; ++column)
    {
      const std::string str = getCellText(Index(column, row)).utf8();

      fprintf(file, "%s", str.c_str());
      if (column < (width_ - 1))
        fprintf(file, ",");
    }
    fprintf(file, "\n");
  }

  void save(Str const& filename)
  {
    FILE * file = fopen(filename.utf8().c_str(), "w");
    if (!file)
      return;

    // Write header info
    for (int i = 0; i < width_; ++i)
    {
      fprintf(file, "%d", columnWidth_[i]);
      if (i < (width_ - 1))
        fprintf(file, ",");
    }
    fprintf(file, "\n");

    for (int row = 0; row < height_; ++row)
      write_row(file, row);

    fclose(file);

    filename_ = filename;
  }

  static char * read_cell(char * it, String cell)
  {
    char * start = it;

    while (*it != ',' && *it != '\0' && *it != '\n')
      ++it;

    char curr = *it;
    *it = '\0';

    string_set(cell, start);
    *it = curr;
    return it;
  }

  static void read_column_sizes(char * data)
  {
    String cell;
    char * it = data;

    while (1)
    {
      it = read_cell(it, cell);
      if (*it == '\n' || *it == '\0')
        return;
    }
  }

  int load(Str const& filename)
  {
    /*
    char filename_utf8[STRING_LEN];
    string_utf8(filename_utf8, filename);

    FILE * file = fopen(filename_utf8, "w");
    if (!file)
      return 0;

    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char * data = malloc(size);
    fread(data, size, 1, file);
    fclose(file);


    free(data);
    */
    return 0;
  }

  int getColumnWidth(int column)
  {
    assert(column < width_);
    return columnWidth_[column];
  }

  int getRowCount()
  {
    return width_;
  }

  int getColumnCount()
  {
    return height_;
  }

  static int get_cell_index(int x, int y)
  {
    return (y * width_) + x;
  }

  static Cell & getCell(Index const& idx)
  {
    auto result = cells_.find(idx);
    if (result == cells_.end())
      result = cells_.emplace(idx, Cell()).first;

    return result->second;
  }

  Str const& getCellText(Index const& idx)
  {
    assert(idx.x < width_);
    assert(idx.y < height_);

    auto result = cells_.find(idx);
    if (result == cells_.end())
      return Str::EMPTY;

    return result->second.text;
  }

  void setCellText(Index const& idx, Str const& text)
  {
    assert(idx.x < width_);
    assert(idx.y < height_);

    Cell & cell = getCell(idx);
    cell.text = text;
  }

  Str rowToLabel(int row)
  {
    return Str::format("%5d", row + 1);
  }

  Str columnTolabel(int col)
  {

  }

}
