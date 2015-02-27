
#include "Editor.h"
#include "Document.h"
#include "Str.h"
#include "Commands.h"
#include "Completion.h"
#include "Tcl.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <regex>

static const int ROW_HEADER_WIDTH = 8;

struct ColumnInfo
{
  ColumnInfo(int column, int x, int width)
    : column_(column),
      x_(x),
      width_(width)
  { }

  int column_;
  int x_;
  int width_;
};

EditorMode editMode_ = EditorMode::NAVIGATE;

static std::vector<ColumnInfo> drawColumnInfo_;

int editLinePos_ = 0;
static Str editLine_;   //< Edit line if a string of utf32
static std::string yankBuffer_;
static std::string flashMessage_;
static std::string searchTerm_;
static std::vector<std::string> completionHints_;
static std::vector<std::string> completionHintLines_;

static const tcl::Variable ALWAYS_SHOW_HEADER("app_alwaysShowHeader", false);

extern void clearTimeout();

static int getCommandLineHeight()
{
  return 2 + completionHintLines_.size();
}

void updateCursor()
{
  switch (editMode_)
  {
    case EditorMode::NAVIGATE:
      tb_set_cursor(-1, -1);
      break;

    case EditorMode::EDIT:
      tb_set_cursor(editLinePos_, tb_height() - 1);
      break;

    case EditorMode::COMMAND:
    case EditorMode::SEARCH:
      tb_set_cursor(editLinePos_ + 1, tb_height() - 1);
      break;
  }
}

static void ensureCursorVisibility()
{
  if (doc::cursorPos().x < doc::scroll().x)
    doc::scroll().x = doc::cursorPos().x;

  if ((doc::cursorPos().y - (ALWAYS_SHOW_HEADER.toBool() ? 1 : 0)) < doc::scroll().y)
    doc::scroll().y = doc::cursorPos().y - (ALWAYS_SHOW_HEADER.toBool() && doc::cursorPos().y != 0 ? 1 : 0);

  int width = ROW_HEADER_WIDTH;
  bool scroll = false;
  for (int i = doc::scroll().x; i <= doc::cursorPos().x && !scroll; ++i)
  {
    width += doc::getColumnWidth(i);

    while (width >= tb_width())
    {
      scroll = true;
      width -= doc::getColumnWidth(doc::scroll().x);
      doc::scroll().x++;
    }
  }

  while ((doc::cursorPos().y - doc::scroll().y) >= (tb_height() - 3))
    doc::scroll().y++;
}

Index getCursorPos()
{
  return doc::cursorPos();
}

void setCursorPos(Index const& idx)
{
  doc::cursorPos().x = std::min(std::max(idx.x, 0), doc::getColumnCount());
  doc::cursorPos().y = std::min(std::max(idx.y, 0), doc::getRowCount());
  ensureCursorVisibility();
}

EditorMode getEditorMode()
{
  return editMode_;
}

std::string getYankBuffer()
{
  return yankBuffer_;
}

void yankCurrentCell()
{
  yankBuffer_ = doc::getCellText(doc::cursorPos());
}

void navigateLeft()
{
  if (doc::cursorPos().x > 0)
   doc::cursorPos().x--;

  ensureCursorVisibility();
}

void navigateRight()
{
  if (doc::cursorPos().x < (doc::getColumnCount() - 1))
   doc::cursorPos().x++;

  ensureCursorVisibility();
}

void navigateUp()
{
  if (doc::cursorPos().y > 0)
    doc::cursorPos().y--;

  ensureCursorVisibility();
}

void navigateDown()
{
  if (doc::cursorPos().y < (doc::getRowCount() - 1))
    doc::cursorPos().y++;

  ensureCursorVisibility();
}

void navigatePageUp()
{
  doc::cursorPos().y -= tb_height() - getCommandLineHeight() - 1;
  doc::scroll().y -= tb_height() - getCommandLineHeight() - 1;

  if (doc::cursorPos().y < 0)
  {
    doc::cursorPos().y = 0;
    doc::scroll().y = 0;
  }

  ensureCursorVisibility();
}

void navigatePageDown()
{
  doc::cursorPos().y += tb_height() - getCommandLineHeight() - 1;
  doc::scroll().y += tb_height() - getCommandLineHeight() - 1;
  if (doc::cursorPos().y > (doc::getRowCount() - 1))
    doc::cursorPos().y = doc::getRowCount() - 1;

  ensureCursorVisibility();
}

bool findNextMatch()
{
  if (searchTerm_.empty())
    return false;

  Index pos = getCursorPos();
  pos.x++;

  while (pos.y < doc::getRowCount())
  {
    while (pos.x < doc::getColumnCount())
    {
      const std::string text = doc::getCellText(pos);
      if (text.find(searchTerm_) != std::string::npos)
      {
        setCursorPos(pos);
        ensureCursorVisibility();
        return true;
      }

      pos.x++;
    }

    pos.y++;
    pos.x = 0;
  }

  return false;
}

bool findPreviousMatch()
{
  if (searchTerm_.empty())
    return false;

  Index pos = getCursorPos();
  pos.x--;

  while (pos.y >= 0)
  {
    while (pos.x >= 0)
    {
      const std::string text = doc::getCellText(pos);
      if (text.find(searchTerm_) != std::string::npos)
      {
        setCursorPos(pos);
        ensureCursorVisibility();
        return false;
      }

      pos.x--;
    }

    pos.y--;
    pos.x = doc::getColumnCount() - 1;
  }

  return false;
}

void editCurrentCell()
{
  if (!doc::isReadOnly())
  {
    editMode_ = EditorMode::EDIT;
    editLine_ = doc::getCellText(doc::cursorPos());
    editLinePos_ = editLine_.size();
    clearFlashMessage();
  }
}

void handleTextInput(struct tb_event * event)
{
  switch (event->key)
  {
    case TB_KEY_ARROW_LEFT:
      if (editLinePos_ > 0)
        editLinePos_--;
      break;

    case TB_KEY_ARROW_RIGHT:
      if (editLinePos_ < editLine_.size())
        editLinePos_++;
      break;

    case TB_KEY_HOME:
      editLinePos_ = 0;
      break;

    case TB_KEY_END:
      editLinePos_ = editLine_.size();
      break;

    case TB_KEY_SPACE:
      editLine_.insert(editLinePos_, ' ');
      editLinePos_++;
      break;

    case TB_KEY_BACKSPACE:
    case TB_KEY_BACKSPACE2:
      if (editLinePos_ > 0)
      {
        editLine_.erase(editLinePos_ - 1);
        editLinePos_--;
      }
      break;

    case TB_KEY_DELETE:
      {
        editLine_.erase(editLinePos_);
        const int len = editLine_.size();
        if (editLinePos_ > len)
          editLinePos_ = len;
      }
      break;

    default:
      {
        if (event->ch)
        {
          editLine_.insert(editLinePos_, event->ch);
          editLinePos_++;
        }
      }
      break;
  }
}

void handleNavigateEvent(struct tb_event * event)
{
  switch (event->key)
  {
    case TB_KEY_ARROW_LEFT:
      pushEditCommandKey('h');
      break;

    case TB_KEY_ARROW_RIGHT:
      pushEditCommandKey('l');
      break;

    case TB_KEY_ARROW_UP:
      pushEditCommandKey('k');
      break;

    case TB_KEY_ARROW_DOWN:
      pushEditCommandKey('j');
      break;

    case TB_KEY_CTRL_R:
      pushEditCommandKey('U');
      break;

    case TB_KEY_ESC:
      clearEditCommandSequence();
      break;

    case TB_KEY_PGUP:
    case TB_KEY_CTRL_B:
      navigatePageUp();
      break;

    case TB_KEY_PGDN:
    case TB_KEY_CTRL_F:
      navigatePageDown();
      break;

    default:
      {
        switch (event->ch)
        {
          case ':':
            {
              editMode_ = EditorMode::COMMAND;
              editLine_.clear();
              editLinePos_ = 0;
              clearFlashMessage();
            }
            break;

          case '/':
            {
              editMode_ = EditorMode::SEARCH;
              editLine_.clear();
              editLinePos_ = 0;
              clearFlashMessage();
            }
            break;

          case 'i':
            editCurrentCell();
            break;

          default:
            pushEditCommandKey(event->ch);
        }
      }
      break;
  }
}

void handleEditEvent(struct tb_event * event)
{
  handleTextInput(event);

  switch (event->key)
  {
    case TB_KEY_ESC:
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_TAB:
      doc::setCellText(doc::cursorPos(), editLine_.utf8());
      editLine_.clear();
      navigateRight();
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_ENTER:
      doc::setCellText(doc::cursorPos(), editLine_.utf8());
      editLine_.clear();
      navigateDown();
      editMode_ = EditorMode::NAVIGATE;
      break;
  }
}

void handleCommandEvent(struct tb_event * event)
{
  handleTextInput(event);

  switch (event->key)
  {
    case TB_KEY_ENTER:
      executeAppCommands(editLine_.utf8());
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_TAB:
      {
        completeEditLine(editLine_);
        editLinePos_ = editLine_.size();
      }
      break;

    case TB_KEY_ESC:
      if (completionHints_.empty())
        editMode_ = EditorMode::NAVIGATE;
      else
        clearCompletionHints();
      break;
  }
}

void handleSearchEvent(struct tb_event * event)
{
  handleTextInput(event);

  switch (event->key)
  {
    case TB_KEY_ENTER:
      searchTerm_ = editLine_.utf8();
      editMode_ = EditorMode::NAVIGATE;
      findNextMatch();
      break;

    case TB_KEY_ESC:
      searchTerm_.clear();
      editMode_ = EditorMode::NAVIGATE;
      break;
  }
}

void handleKeyEvent(struct tb_event * event)
{
  switch (editMode_)
  {
    case EditorMode::NAVIGATE:
      handleNavigateEvent(event);
      break;

    case EditorMode::EDIT:
      handleEditEvent(event);
      break;

    case EditorMode::COMMAND:
      handleCommandEvent(event);
      break;

    case EditorMode::SEARCH:
      handleSearchEvent(event);
      break;
  }
}

void drawText(int x, int y, int length, uint16_t fg, uint16_t bg, std::string const& str)
{
  static const uint32_t BUFFER_LEN = 1024;
  static uint32_t BUFFER[BUFFER_LEN];

  // Convert to uft32
  const char * it = str.c_str();
  uint32_t strLen = 0;

  while (*it && strLen < (BUFFER_LEN - 1))
  {
    it += tb_utf8_char_to_unicode(&BUFFER[strLen], it);
    ++strLen;
  }
  BUFFER[strLen] = '\0';


  if (length == -1)
  {
    for (int i = 0; i < strLen; ++i)
      tb_change_cell(x + i, y, BUFFER[i], fg, bg);
  }
  else
  {
    for (int i = 0; i < length; ++i)
      tb_change_cell(x + i, y, i < strLen ? BUFFER[i] : ' ', fg, bg);
  }
}

void calculateColumDrawWidths()
{
  drawColumnInfo_.clear();

  int x = ROW_HEADER_WIDTH;
  for (int i = doc::scroll().x; i < doc::getColumnCount(); ++i)
  {
    const int width = doc::getColumnWidth(i);
    drawColumnInfo_.emplace_back(i, x, width);

    x += width;
    if (x >= tb_width())
      break;
  }
}

void clearFlashMessage()
{
  flashMessage_.clear();
  clearTimeout();
}

void flashMessage(std::string const& message)
{
  flashMessage_ = message;
  clearTimeout();
}

static void buildCompletionHintLines()
{
  completionHintLines_.clear();

  const int width = tb_width();

  int columnWidth = 10;
  for (auto const& hint : completionHints_)
    if (hint.size() >= columnWidth)
      columnWidth = std::ceil((hint.size() + 1) / 10.0) * 10;

  std::string currentLine;
  for (auto const& hint : completionHints_)
  {
    if (currentLine.size() + hint.size() > width)
    {
      completionHintLines_.push_back(currentLine);
      currentLine = hint;
    }
    else
      currentLine.append(hint);

    while (currentLine.size() % columnWidth != 0)
      currentLine.append(1, ' ');
  }

  if (!currentLine.empty())
    completionHintLines_.push_back(currentLine);
}

void clearCompletionHints()
{
  completionHints_.clear();
}

void setCompletionHints(std::vector<std::string> const& hints)
{
  completionHints_ = hints;
}

void drawInterface()
{
  calculateColumDrawWidths();
  buildCompletionHintLines();

  tb_set_clear_attributes(TB_DEFAULT, TB_DEFAULT);
  tb_clear();

  if (doc::getColumnCount() > 0 && doc::getRowCount() > 0)
  {
    drawHeaders();
    drawWorkspace();
  }

  drawCommandLine();

  tb_present();
}

void drawHeaders()
{
  // Draw column header
  for (int x = 0; x < drawColumnInfo_.size(); ++x)
  {
    const uint16_t color = (drawColumnInfo_[x].column_) == doc::cursorPos().x ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;
    const int before = (drawColumnInfo_[x].width_ - 1) / 2;

    std::string header(before, ' ');
    header.append(Index::columnToStr(drawColumnInfo_[x].column_));

    drawText(drawColumnInfo_[x].x_, 0, drawColumnInfo_[x].width_, color, color, header);
  }

  // Draw row header
  const int y_end = doc::getRowCount() < (tb_height() - 2) ? doc::getRowCount() : tb_height() - 2;
  for (int y = 1; y < tb_height() - getCommandLineHeight(); ++y)
  {
    int row = y + doc::scroll().y - 1;
    const uint16_t color = row == doc::cursorPos().y ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

    if (y == 1 && ALWAYS_SHOW_HEADER.toBool())
      row = 0;

    if (row < doc::getRowCount())
    {
      const std::string columnNumber = Index::rowToStr(row);

      std::string header;
      while (header.size() < (ROW_HEADER_WIDTH - columnNumber.size() - 1))
        header.append(1, ' ');

      header.append(columnNumber)
            .append(1, ' ');

      drawText(0, y, ROW_HEADER_WIDTH, color, color, header);
    }
  }
}

void drawWorkspace()
{
  for (int y = 1; y < tb_height() - getCommandLineHeight(); ++y)
  {
    for (int x = 0; x < drawColumnInfo_.size(); ++x)
    {
      int row = y + doc::scroll().y - 1;
      const int selected = drawColumnInfo_[x].column_ == doc::cursorPos().x && row == doc::cursorPos().y;
      const uint16_t color = selected ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

      if (y == 1 && ALWAYS_SHOW_HEADER.toBool())
        row = 0;

      const int width = doc::getColumnWidth(drawColumnInfo_[x].column_);

      if (row < doc::getRowCount())
      {
        if (selected && editMode_ == EditorMode::EDIT)
          drawText(drawColumnInfo_[x].x_, y, width, color, color, editLine_.utf8());
        else
        {
          std::string cellText = doc::getCellDisplayText(Index(drawColumnInfo_[x].column_, row));
          if (cellText.size() >= width)
          {
            cellText = cellText.substr(0, width - 3);
            cellText.append(2, '.').append(1, ' ');
          }

          drawText(drawColumnInfo_[x].x_, y, width, color, color, cellText);
        }
      }
    }
  }
}

void drawCommandLine()
{
  std::string infoLine;
  std::string commandLine;
  char mode;

  switch (editMode_)
  {
    case EditorMode::NAVIGATE:
      mode = 'N';
      commandLine = doc::getCellText(doc::cursorPos());
      break;

    case EditorMode::EDIT:
      mode = 'I';
      commandLine = editLine_.utf8();
      break;

    case EditorMode::COMMAND:
      mode = ':';
      commandLine.append(1, ':')
                 .append(editLine_.utf8());
      break;

    case EditorMode::SEARCH:
      mode = '/';
      commandLine.append(1, '/')
                 .append(editLine_.utf8());
      break;
  }

  { // Costruct the info line
    std::string filename = doc::getFilename();
    if (filename.empty())
      filename = "[No Name]";

    const std::string pos(doc::cursorPos().toStr());

    std::string progress = str::fromInt((int)(((double)(doc::cursorPos().y + 1) / (double)doc::getRowCount()) * 100.0)).append(1, '%');

    const int maxFileAreaSize = tb_width() - pos.size() - progress.size() - 5;
    if (filename.size() > maxFileAreaSize)
      filename = filename.substr(filename.size() - maxFileAreaSize);

    while (filename.size() < maxFileAreaSize)
      filename.append(1, ' ');

    infoLine.clear();
    infoLine.append(filename)
            .append(1, ' ')
            .append(pos)
            .append(1, ' ')
            .append(progress)
            .append(1, ' ')
            .append(1, mode)
            .append(1, ' ');
  }

  if (!flashMessage_.empty())
    commandLine = flashMessage_;

  for (int i = 0; i < completionHintLines_.size(); ++i)
    drawText(0, tb_height() - 1 - completionHintLines_.size() + i, tb_width(), TB_DEFAULT, TB_DEFAULT, completionHintLines_[i]);

  drawText(0, tb_height() - completionHintLines_.size() - 2, tb_width(), TB_REVERSE | TB_DEFAULT, TB_REVERSE | TB_DEFAULT, infoLine);
  drawText(0, tb_height() - 1, tb_width(), TB_DEFAULT, TB_DEFAULT, commandLine);
}