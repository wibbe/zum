
#pragma once

#include "Types.h"

namespace doc {

  void createEmpty();
  void close();

  int getColumnWidth(int column);
  int getRowCount();
  int getColumnCount();

  bool undo();
  bool redo();

  bool save(Str const& filename);
  bool load(Str const& filename);

  Str getFilename();

  Str const& getCellText(Index const& idx);
  void setCellText(Index const& idx, Str const& text);

  void increaseColumnWidth(int column);
  void decreaseColumnWidth(int column);

  void addColumn(int column);
  void addRow(int row);
  void removeColumn(int column);
  void removeRow(int row);

  Str rowToLabel(int row);
  Str columnTolabel(int col);

}
