
#pragma once

#include "Types.h"
#include "Index.h"

namespace doc {

  bool createDefaultEmpty();
  bool createEmpty(Str const& width, Str const& height);
  bool close();

  Index & cursorPos();
  Index & scroll();

  int getColumnWidth(int column);
  void setColumnWidth(int column, int width);

  int getRowCount();
  int getColumnCount();
  bool isReadOnly();

  bool undo();
  bool redo();

  bool save(Str const& filename);
  bool load(Str const& filename);

  // This will load a document as read-only from the supplied string.
  bool loadRaw(std::string const& data, Str const& filename, Str::char_type delimiter = 0);

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
  Str columnToLabel(int col);

  Str toCellRef(Index const& idx);
  Index parseCellRef(Str const& str);

}
