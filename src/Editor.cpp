
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
static const int MAX_ROW_COUNT = 100000;
static const int MAX_COLUMN_COUNT = 10000;

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

enum class SelectionMode
{
  NONE,
  BLOCK,
  ROW,
};

EditorMode editMode_ = EditorMode::NAVIGATE;

static std::vector<ColumnInfo> drawColumnInfo_;

static SelectionMode selectionMode_ = SelectionMode::NONE;
static Index selectionStart_;

int editLinePos_ = 0;
static Str editLine_;   //< Edit line if a string of utf32
static std::string yankBuffer_;
static std::string flashMessage_;
static std::string searchTerm_;
static std::vector<std::string> completionHints_;
static std::vector<std::string> messageLines_;

static const tcl::Variable ALWAYS_SHOW_HEADER("app_alwaysShowHeader", false);

extern void clearTimeout();

static int getCommandLineHeight()
{
  return 2 + messageLines_.size();
}

void updateCursor()
{
  switch (editMode_)
  {
    case EditorMode::NAVIGATE:
      view::hideCursor();
      break;

    case EditorMode::EDIT:
      view::setCursor(editLinePos_, view::height() - 1);
      break;

    case EditorMode::COMMAND:
    case EditorMode::SEARCH:
      view::setCursor(editLinePos_ + 1, view::height() - 1);
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

    while (width >= view::width())
    {
      scroll = true;
      width -= doc::getColumnWidth(doc::scroll().x);
      doc::scroll().x++;
    }
  }

  while ((doc::cursorPos().y - doc::scroll().y) >= (view::height() - 3))
    doc::scroll().y++;
}

Index getCursorPos()
{
  return doc::cursorPos();
}

void setCursorPos(Index const& idx)
{
  doc::cursorPos().x = std::min(std::max(idx.x, 0), MAX_COLUMN_COUNT);
  doc::cursorPos().y = std::min(std::max(idx.y, 0), MAX_ROW_COUNT);
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

static void updateSelection()
{
  switch (selectionMode_)
  {
    case SelectionMode::BLOCK:
      {
        Index cursor = doc::cursorPos();

        doc::selectionStart().x = std::min(selectionStart_.x, cursor.x);
        doc::selectionStart().y = std::min(selectionStart_.y, cursor.y);

        doc::selectionEnd().x = std::max(selectionStart_.x, cursor.x);
        doc::selectionEnd().y = std::max(selectionStart_.y, cursor.y);
      }
      break;

    case SelectionMode::ROW:
      {
        Index cursor = doc::cursorPos();

        doc::selectionStart().x = 0;
        doc::selectionStart().y = std::min(selectionStart_.y, cursor.y);

        doc::selectionEnd().x = doc::getColumnCount();
        doc::selectionEnd().y = std::max(selectionStart_.y, cursor.y);
      }
      break;

    default:
      break;
  }
}

void navigateLeft()
{
  if (doc::cursorPos().x > 0)
    doc::cursorPos().x--;

  ensureCursorVisibility();
  updateSelection();
}

void navigateRight()
{
  if (doc::cursorPos().x < MAX_COLUMN_COUNT)
   doc::cursorPos().x++;

  ensureCursorVisibility();
  updateSelection();
}

void navigateUp()
{
  if (doc::cursorPos().y > 0)
    doc::cursorPos().y--;

  ensureCursorVisibility();
  updateSelection();
}

void navigateDown()
{
  if (doc::cursorPos().y < MAX_ROW_COUNT)
    doc::cursorPos().y++;

  ensureCursorVisibility();
  updateSelection();
}

void navigatePageUp()
{
  doc::cursorPos().y -= view::height() - getCommandLineHeight() - 1;
  doc::scroll().y -= view::height() - getCommandLineHeight() - 1;

  if (doc::cursorPos().y < 0)
  {
    doc::cursorPos().y = 0;
    doc::scroll().y = 0;
  }

  ensureCursorVisibility();
  updateSelection();
}

void navigatePageDown()
{
  doc::cursorPos().y += view::height() - getCommandLineHeight() - 1;
  doc::scroll().y += view::height() - getCommandLineHeight() - 1;
  if (doc::cursorPos().y > MAX_ROW_COUNT)
    doc::cursorPos().y = MAX_ROW_COUNT - 1;

  ensureCursorVisibility();
  updateSelection();
}

void navigateHome()
{
  doc::cursorPos().x = 0;
  ensureCursorVisibility();
  updateSelection();
}

void navigateEnd()
{
  doc::cursorPos().x = doc::getColumnCount() - 1;
  ensureCursorVisibility();
  updateSelection();
}

void moveCursorInSelection(int x, int y)
{
  if (doc::hasSelection())
  {
    doc::cursorPos().x += x;
    doc::cursorPos().y += y;

    if (doc::cursorPos().x > doc::selectionEnd().x)
      doc::cursorPos().y++;

    if (doc::cursorPos().y > doc::selectionEnd().y)
      doc::cursorPos().x++;

    if (doc::cursorPos().x > doc::selectionEnd().x)
      doc::cursorPos().x = doc::selectionStart().x;
    if (doc::cursorPos().y > doc::selectionEnd().y)
      doc::cursorPos().y = doc::selectionStart().y;

    ensureCursorVisibility();
  }
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

void handleTextInput(view::Event * event)
{
  switch (event->key)
  {
    case view::KEY_ARROW_LEFT:
      if (editLinePos_ > 0)
        editLinePos_--;
      break;

    case view::KEY_ARROW_RIGHT:
      if (editLinePos_ < editLine_.size())
        editLinePos_++;
      break;

    case view::KEY_HOME:
      editLinePos_ = 0;
      break;

    case view::KEY_END:
      editLinePos_ = editLine_.size();
      break;

    case view::KEY_SPACE:
      editLine_.insert(editLinePos_, ' ');
      editLinePos_++;
      break;

    case view::KEY_BACKSPACE:
      if (editLinePos_ > 0)
      {
        editLine_.erase(editLinePos_ - 1);
        editLinePos_--;
      }
      break;

    case view::KEY_DELETE:
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

void handleNavigateEvent(view::Event * event)
{
  switch (event->key)
  {
    case view::KEY_ARROW_LEFT:
      pushEditCommandKey('h');
      break;

    case view::KEY_ARROW_RIGHT:
      pushEditCommandKey('l');
      break;

    case view::KEY_ARROW_UP:
      pushEditCommandKey('k');
      break;

    case view::KEY_ARROW_DOWN:
      pushEditCommandKey('j');
      break;

    case view::KEY_CTRL_R:
      pushEditCommandKey('U');
      break;

    case view::KEY_CTRL_V:
      selectionMode_ = SelectionMode::BLOCK;
      selectionStart_ = doc::cursorPos();
      updateSelection();
      break;

    case view::KEY_ESC:
      if (selectionMode_ != SelectionMode::NONE)
        doc::cursorPos() = selectionStart_;

      selectionMode_ = SelectionMode::NONE;
      doc::selectionStart() = Index(-1, -1);
      doc::selectionEnd() = Index(-1, -1);
      clearEditCommandSequence();
      break;

    case view::KEY_PGUP:
    case view::KEY_CTRL_B:
      navigatePageUp();
      break;

    case view::KEY_PGDN:
    case view::KEY_CTRL_F:
      navigatePageDown();
      break;

    case view::KEY_HOME:
      navigateHome();
      break;

    case view::KEY_END:
      navigateEnd();
      break;

    case view::KEY_SPACE:
      pushEditCommandKey(' ');
      break;

    case view::KEY_ENTER:
      if (selectionMode_ == SelectionMode::NONE)
      {
        editCurrentCell();
      }
      else
      {
        updateSelection();
        doc::cursorPos() = doc::selectionStart();
        selectionMode_ = SelectionMode::NONE;
      }
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
            }
            break;

          case '/':
            {
              editMode_ = EditorMode::SEARCH;
              editLine_.clear();
              editLinePos_ = 0;
            }
            break;

          case 'i':
            editCurrentCell();
            break;

          case 'V':
            selectionMode_ = SelectionMode::ROW;
            selectionStart_ = doc::cursorPos();
            updateSelection();
            break;

          default:
            pushEditCommandKey(event->ch);
        }
      }
      break;
  }

  if (editMode_ != EditorMode::COMMAND)
    clearMessageLines();
}

void handleEditEvent(view::Event * event)
{
  handleTextInput(event);
  clearMessageLines();

  switch (event->key)
  {
    case view::KEY_ESC:
      editMode_ = EditorMode::NAVIGATE;
      break;

    case view::KEY_TAB:
      doc::setCellText(doc::cursorPos(), editLine_.utf8());
      editLine_.clear();

      if (doc::hasSelection())
      {
        moveCursorInSelection(1, 0);
        editCurrentCell();
      }
      else
      {
        navigateRight();
        editMode_ = EditorMode::NAVIGATE;
      }
      break;

    case view::KEY_ENTER:
      doc::setCellText(doc::cursorPos(), editLine_.utf8());
      editLine_.clear();

      if (doc::hasSelection())
      {
        moveCursorInSelection(0, 1);
        editCurrentCell();
      }
      else
      {
        navigateDown();
        editMode_ = EditorMode::NAVIGATE;
      }
      break;
  }
}

void handleCommandEvent(view::Event * event)
{
  handleTextInput(event);

  switch (event->key)
  {
    case view::KEY_ENTER:
      executeAppCommands(editLine_.utf8());
      editMode_ = EditorMode::NAVIGATE;
      break;

    case view::KEY_TAB:
      {
        completeEditLine(editLine_);
        editLinePos_ = editLine_.size();
      }
      break;

    case view::KEY_ESC:
      if (messageLines_.empty())
        editMode_ = EditorMode::NAVIGATE;
      else
        clearMessageLines();
      break;
  }
}

void handleSearchEvent(view::Event * event)
{
  handleTextInput(event);
  clearMessageLines();

  switch (event->key)
  {
    case view::KEY_ENTER:
      searchTerm_ = editLine_.utf8();
      editMode_ = EditorMode::NAVIGATE;
      findNextMatch();
      break;

    case view::KEY_ESC:
      searchTerm_.clear();
      editMode_ = EditorMode::NAVIGATE;
      break;
  }
}

void handleKeyEvent(view::Event * event)
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

void drawText(int x, int y, int length, uint16_t fg, uint16_t bg, std::string const& str, uint32_t format = 0)
{
  static const uint32_t BUFFER_LEN = 1024;
  static uint32_t BUFFER[BUFFER_LEN];
  const uint32_t strLen = str::toUTF32(str, BUFFER, BUFFER_LEN);


  if (length == -1)
  {
    for (int i = 0; i < strLen; ++i)
      view::changeCell(x + i, y, BUFFER[i], fg, bg);
  }
  else
  {
    int start = 0;

    switch (format & ALIGN_MASK)
    {
      case ALIGN_LEFT:
        start = 0;
        break;

      case ALIGN_CENTER:
        start = (length / 2) - (strLen / 2);
        break;

      case ALIGN_RIGHT:
        start = length - strLen;
        break;
    }


    for (int i = 0; i < length; ++i)
    {
      const int charIdx = i - start;
      const uint32_t ch = charIdx < 0 || charIdx >= strLen ? ' ' : BUFFER[charIdx];

      uint32_t style = fg;
      if (charIdx >= 0 && charIdx < strLen)
      {
        if (format & FONT_BOLD)
          style |= view::COLOR_BOLD;
        if (format & FONT_UNDERLINE)
          style |= view::COLOR_UNDERLINE;
      }

      view::changeCell(x + i, y, ch, style, bg);
    }
  }
}

void calculateColumDrawWidths()
{
  drawColumnInfo_.clear();

  int x = ROW_HEADER_WIDTH;
  for (int i = doc::scroll().x; i < MAX_COLUMN_COUNT; ++i)
  {
    const int width = doc::getColumnWidth(i);
    drawColumnInfo_.emplace_back(i, x, width);

    x += width;
    if (x >= view::width())
      break;
  }
}

void clearFlashMessage()
{
  messageLines_.clear();
}

void flashMessage(std::string const& message)
{
  messageLines_.clear();
  std::string line = "";

  for (size_t i = 0; i < message.size(); ++i)
  {
    if (message[i] == '\n')
    {
      if (!line.empty())
        messageLines_.push_back(line);
      line = "";
    }
    else
    {
      line += message[i];
    }
  }

  if (!line.empty())
    messageLines_.push_back(line);
}

static void buildCompletionMessageLines()
{
  messageLines_.clear();

  const int width = view::width();

  int columnWidth = 10;
  for (auto const& hint : completionHints_)
    if (hint.size() >= columnWidth)
      columnWidth = std::ceil((hint.size() + 1) / 10.0) * 10;

  std::string currentLine;
  for (auto const& hint : completionHints_)
  {
    if (currentLine.size() + hint.size() > width)
    {
      messageLines_.push_back(currentLine);
      currentLine = hint;
    }
    else
      currentLine.append(hint);

    while (currentLine.size() % columnWidth != 0)
      currentLine.append(1, ' ');
  }

  if (!currentLine.empty())
    messageLines_.push_back(currentLine);
}

void clearMessageLines()
{
  messageLines_.clear();
}

void setCompletionHints(std::vector<std::string> const& hints)
{
  completionHints_ = hints;
  buildCompletionMessageLines();
}

void drawInterface()
{
  calculateColumDrawWidths();

  view::setClearAttributes(view::COLOR_DEFAULT, view::COLOR_DEFAULT);
  view::clear();

  drawHeaders();
  drawWorkspace();

  drawCommandLine();
  view::present();
}

void drawHeaders()
{
  // Draw column header
  for (int x = 0; x < drawColumnInfo_.size(); ++x)
  {
    const uint16_t bg = (drawColumnInfo_[x].column_) == doc::cursorPos().x ? view::COLOR_HIGHLIGHT : view::COLOR_BACKGROUND;
    const uint16_t fg = (drawColumnInfo_[x].column_) == doc::cursorPos().x ? view::COLOR_WHITE : view::COLOR_TEXT;
    drawText(drawColumnInfo_[x].x_, 0, drawColumnInfo_[x].width_, fg, bg, Index::columnToStr(drawColumnInfo_[x].column_), ALIGN_CENTER);
  }

  // Draw row header
  for (int y = 1; y < view::height() - getCommandLineHeight(); ++y)
  {
    int row = y + doc::scroll().y - 1;
    const uint16_t bg = row == doc::cursorPos().y ? view::COLOR_HIGHLIGHT : view::COLOR_BACKGROUND;
    const uint16_t fg = row == doc::cursorPos().y ? view::COLOR_WHITE : view::COLOR_TEXT;

    if (y == 1 && ALWAYS_SHOW_HEADER.toBool())
      row = 0;

    const std::string columnNumber = Index::rowToStr(row);

    std::string header;
    while (header.size() < (ROW_HEADER_WIDTH - columnNumber.size() - 1))
      header.append(1, ' ');

    header.append(columnNumber)
          .append(1, ' ');

    drawText(0, y, ROW_HEADER_WIDTH, fg, bg, header);
  }
}

void drawWorkspace()
{
  for (int y = 1; y < view::height() - getCommandLineHeight(); ++y)
  {
    for (int x = 0; x < drawColumnInfo_.size(); ++x)
    {
      int row = y + doc::scroll().y - 1;

      if (y == 1 && ALWAYS_SHOW_HEADER.toBool())
        row = 0;

      const bool cursorHere = drawColumnInfo_[x].column_ == doc::cursorPos().x && row == doc::cursorPos().y;
      const bool sameAsCursor = drawColumnInfo_[x].column_ == doc::cursorPos().x || row == doc::cursorPos().y;
      const bool selected = drawColumnInfo_[x].column_ >= doc::selectionStart().x && drawColumnInfo_[x].column_ <= doc::selectionEnd().x &&
                            row >= doc::selectionStart().y && row <= doc::selectionEnd().y;

      uint16_t bg = sameAsCursor ? view::COLOR_SELECTION : view::COLOR_PANEL;

      if (selected || cursorHere)
      {
        bg = view::COLOR_HIGHLIGHT;

        if (selected && cursorHere)
          bg = view::COLOR_SELECTION;
      }

      const uint16_t fg = bg == view::COLOR_HIGHLIGHT ? view::COLOR_WHITE : view::COLOR_TEXT;
      const int width = doc::getColumnWidth(drawColumnInfo_[x].column_);

      //if (row < doc::getRowCount())
      {
        if (cursorHere && editMode_ == EditorMode::EDIT)
          drawText(drawColumnInfo_[x].x_, y, width, fg, bg, editLine_.utf8());
        else
        {
          std::string cellText = doc::getCellDisplayText(Index(drawColumnInfo_[x].column_, row));
          const uint32_t format = doc::getCellFormat(Index(drawColumnInfo_[x].column_, row));

          if (cellText.size() >= width)
          {
            cellText = cellText.substr(0, width - 3);
            cellText.append(2, '.').append(1, ' ');
          }

          drawText(drawColumnInfo_[x].x_, y, width, fg, bg, cellText, format);
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

  const std::string pos(doc::cursorPos().toStr());

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


    std::string progress = str::fromInt(std::min(100, std::max(0, (int)((double)(doc::cursorPos().y + 1) / (double)(doc::getRowCount() == 0 ? 1 : doc::getRowCount()) * 100.0)))).append(1, '%');

    const int maxFileAreaSize = view::width() - pos.size() - progress.size() - 5;
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

  for (int i = 0; i < messageLines_.size(); ++i)
    drawText(0, view::height() - 1 - messageLines_.size() + i, view::width(), view::COLOR_TEXT, view::COLOR_SELECTION, messageLines_[i]);

  drawText(0, view::height() - messageLines_.size() - 2, view::width(), view::COLOR_WHITE, view::COLOR_HIGHLIGHT | view::COLOR_DEFAULT, infoLine);
  drawText(0, view::height() - 1, view::width(), view::COLOR_TEXT, view::COLOR_BACKGROUND, commandLine);
}

namespace tcl {
  TCL_EXPOSE_FUNC(navigateLeft, "Move the cursor to previous column");
  TCL_EXPOSE_FUNC(navigateRight, "Move the cursor to next column");
  TCL_EXPOSE_FUNC(navigateUp, "Move the cursor to the previous row");
  TCL_EXPOSE_FUNC(navigateDown, "Move the cursor to the next row");
  TCL_EXPOSE_FUNC(navigatePageUp, "Move the cursor one page up");
  TCL_EXPOSE_FUNC(navigatePageDown, "Move the cursor one page down");
  TCL_EXPOSE_FUNC(navigateHome, "Move the cursor to the first column in the document");
  TCL_EXPOSE_FUNC(navigateEnd, "Move the cursor to the last column in the document");
  TCL_EXPOSE_FUNC(yankCurrentCell, "Copy the content from the current cell to the yank buffer");
  TCL_EXPOSE_FUNC(findNextMatch, "Jump to the next search result");
  TCL_EXPOSE_FUNC(findPreviousMatch, "Jump to the previous search result");

  TCL_FUNC(getYankBuffer, "", "Returns the content in the yank buffer")
  {
    TCL_STRING_UTF8_RESULT(getYankBuffer());
  }
}
