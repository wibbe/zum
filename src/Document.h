
#pragma once

#include "Cell.h"
#include "Index.h"

namespace doc {

  void createDefaultEmpty();
  void close();

  void nextBuffer();
  void previousBuffer();
  void jumpToBuffer(int buffer);
  int currentBufferIndex();
  int getOpenBufferCount();

  Index & cursorPos();
  Index & scroll();
  Index & selectionStart();
  Index & selectionEnd();

  bool hasSelection();

  std::vector<Index> selectedCells();
  std::vector<Index> selectedColumns();
  std::vector<Index> selectedRows();
  Index selectionIndex(Index const& idx);

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

  void evaluateDocument();

  std::string getCellText(Index const& idx);
  std::string getCellDisplayText(Index const& idx);
  uint32_t getCellFormat(Index const& idx);
  double getCellValue(Index const& idx);

  void setCellText(Index const& idx, std::string const& text);
  void setCellFormat(Index const& idx, uint32_t format);

  void increaseColumnWidth(int column);
  void decreaseColumnWidth(int column);

  void addColumn(int column);
  void addRow(int row);
  void removeColumn(int column);
  void removeRow(int row);
}
