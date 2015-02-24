
#pragma once

#include "Types.h"
#include "Index.h"

namespace doc {

  void createDefaultEmpty();
  void createEmpty(int width, int height);
  void close();

  Index & cursorPos();
  Index & scroll();

  int getColumnWidth(int column);
  void setColumnWidth(int column, int width);

  int getRowCount();
  int getColumnCount();
  bool isReadOnly();

  bool undo();
  bool redo();

  bool save(std::string const& filename);
  bool load(std::string const& filename);

  // This will load a document as read-only from the supplied string.
  bool loadRaw(std::string const& data, std::string const& filename, char delimiter = 0);

  std::string getFilename();

  std::string getCellText(Index const& idx);
  void setCellText(Index const& idx, std::string const& text);

  void increaseColumnWidth(int column);
  void decreaseColumnWidth(int column);

  void addColumn(int column);
  void addRow(int row);
  void removeColumn(int column);
  void removeRow(int row);
}
