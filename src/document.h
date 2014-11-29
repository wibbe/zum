
#pragma once

#include "Types.h"

namespace doc {

  void createEmpty();
  void close();

  int getColumnWidth(int column);
  int getRowCount();
  int getColumnCount();

  void save(Str const& filename);
  int load(Str const& filename);

  Str const& getFilename();

  Str const& getCellText(Index const& idx);
  void setCellText(Index const& idx, Str const& text);

  Str rowToLabel(int row);
  Str columnTolabel(int col);

}
