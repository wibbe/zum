
#include "Editor.h"
#include "Document.h"
#include "Str.h"
#include "Commands.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>

static const int ROW_HEADER_WIDTH = 6;

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

extern int application_running;

#define CELL_BUFFER_LEN 1024
static struct tb_cell cell_buffer[CELL_BUFFER_LEN];

static std::vector<ColumnInfo> drawColumnInfo_;

static Index currentScroll_(0, 0);
static Index currentIndex_(0, 0);
  
int editLinePos_ = 0;
static Str editLine_;
static Str yankBuffer_;

extern void parseAndExecute(Str const& command);
extern Str completeCommand(Str const& command);

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
      tb_set_cursor(editLinePos_ + 1, tb_height() - 1);
      break;
  }
}

Index getCursorPos()
{
  return currentIndex_;
}

void setCursorPos(Index const& idx)
{
  currentIndex_ = idx;
}

EditorMode getEditorMode()
{
  return editMode_;
}

Str getYankBuffer()
{
  return yankBuffer_;
}

void yankCurrentCell()
{
  yankBuffer_ = doc::getCellText(currentIndex_);
}

void navigateLeft()
{
  if (currentIndex_.x > 0)
   currentIndex_.x--;

  if (currentIndex_.x < currentScroll_.x)
    currentScroll_.x = currentIndex_.x;
}

void navigateRight()
{
  if (currentIndex_.x < (doc::getColumnCount() - 1))
   currentIndex_.x++;

 if (!drawColumnInfo_.empty() && currentIndex_.x > drawColumnInfo_.back().column_)
    currentScroll_.x++;
}

void navigateUp()
{
  if (currentIndex_.y > 0)
    currentIndex_.y--;

  if (currentIndex_.y < currentScroll_.y)
    currentScroll_.y = currentIndex_.y;
}

void navigateDown()
{
  if (currentIndex_.y < (doc::getRowCount() - 1))
    currentIndex_.y++;
}

void handleNavigateEvent(struct tb_event * event)
{
  switch (event->key)
  {
    case TB_KEY_ARROW_LEFT:
      pushCommandKey('h');
      break;

    case TB_KEY_ARROW_RIGHT:
      pushCommandKey('l');
      break;

    case TB_KEY_ARROW_UP:
      pushCommandKey('k');
      break;

    case TB_KEY_ARROW_DOWN:
      pushCommandKey('j');
      break;

    case TB_KEY_CTRL_R:
      pushCommandKey('U');
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

          case 'i':
            {
              editMode_ = EditorMode::EDIT;
              editLine_ = doc::getCellText(currentIndex_);
              editLinePos_ = editLine_.size();
            }
            break;

          default:
            pushCommandKey(event->ch);
        }
      }
      break;
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

void handleEditEvent(struct tb_event * event)
{
  handleTextInput(event);

  switch (event->key)
  {
    case TB_KEY_ESC:
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_TAB:
      doc::setCellText(currentIndex_, editLine_);
      editLine_.clear();
      navigateRight();
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_ENTER:
      doc::setCellText(currentIndex_, editLine_);
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
      parseAndExecute(editLine_);
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_TAB:
      //complete_command(editLine_);
      break;

    case TB_KEY_ESC:
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
  }
}

void drawText(int x, int y, int len, uint16_t fg, uint16_t bg, Str const& str)
{
  if (len == -1)
  {
    int i = 0;
    for (auto ch : str)
      tb_change_cell(x + i++, y, ch, fg, bg);
  }
  else
  {
    const int size = str.size();
    for (int i = 0; i < len; ++i)
      tb_change_cell(x + i, y, i < size ? str[i] : ' ', fg, bg);
  }
}

void calculateColumDrawWidths()
{
  drawColumnInfo_.clear();

  int x = ROW_HEADER_WIDTH;
  for (int i = currentScroll_.x; i < doc::getColumnCount(); ++i)
  {
    const int width = doc::getColumnWidth(i);
    drawColumnInfo_.emplace_back(i, x, width);

    x += width;
    if (x >= tb_width())
      break;
  }
}

void drawInterface()
{
  calculateColumDrawWidths();

  tb_set_clear_attributes(TB_DEFAULT, TB_DEFAULT);
  tb_clear();

  drawHeaders();
  drawWorkspace();
  drawCommandLine();

  tb_present();
}

void drawHeaders()
{
  // Draw column header
  for (int x = 0; x < drawColumnInfo_.size(); ++x)
  {
    const uint16_t color = (x + currentScroll_.x) == currentIndex_.x ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

    Str header;
    header.append(' ')
          .append((char)('A' + x + currentScroll_.x));

    drawText(drawColumnInfo_[x].x_, 0, drawColumnInfo_[x].width_, color, color, header);
  }

  // Draw row header
  const int y_end = doc::getRowCount() < (tb_height() - 2) ? doc::getRowCount() : tb_height() - 2;
  for (int y = 0; y < tb_height() - 2; ++y)
  {
    const uint16_t color = (y + currentScroll_.y) == currentIndex_.y ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

    Str header;

    if (y >= currentScroll_.y && y < y_end)
      header = doc::rowToLabel(y);

    drawText(0, y - currentScroll_.x + 1, ROW_HEADER_WIDTH, color, color, header);
  }
}

void drawWorkspace()
{
  const int y_end = doc::getRowCount() < (tb_height() - 2) ? doc::getRowCount() : tb_height() - 1;

  for (int y = currentScroll_.y; y < y_end; ++y)
  {
    for (int x = 0; x < drawColumnInfo_.size(); ++x)
    {
      const int selected = drawColumnInfo_[x].column_ == currentIndex_.x && y == currentIndex_.y;
      const uint16_t color = selected ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

      const int width = doc::getColumnWidth(x + currentScroll_.x);

      if (selected && editMode_ == EditorMode::EDIT)
        drawText(drawColumnInfo_[x + currentScroll_.x].x_, y + 1, width, 
                 color, color, editLine_);
      else
        drawText(drawColumnInfo_[x + currentScroll_.x].x_, y + 1, width, 
                 color, color, doc::getCellText(Index(drawColumnInfo_[x].column_, y)));
    }
  }
}

void drawCommandLine()
{
  Str infoLine;
  Str commandLine;

  switch (editMode_)
  {
    case EditorMode::NAVIGATE:
      commandLine = doc::getCellText(currentIndex_);
      break;

    case EditorMode::EDIT:
      commandLine = editLine_;
      break;

    case EditorMode::COMMAND:
      commandLine.append(':')
                 .append(editLine_);
      break;
  }

  { // Costruct the info line
    Str filename = doc::getFilename();
    if (filename.empty())
      filename.set("[NoName]");

    infoLine = filename;
  }

  drawText(0, tb_height() - 2, tb_width(), TB_REVERSE | TB_DEFAULT, TB_REVERSE | TB_DEFAULT, infoLine);
  drawText(0, tb_height() - 1, tb_width(), TB_DEFAULT, TB_DEFAULT, commandLine);
}