
#include "Editor.h"
#include "Document.h"
#include "Str.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>

static const int ROW_HEADER_WIDTH = 6;

EditorMode editMode_ = EditorMode::NAVIGATE;
int current_row_scroll = 0;
int current_column_scroll = 0;

extern int application_running;

typedef struct {
  int x;
  int width;
} ColumnInfo;

#define CELL_BUFFER_LEN 1024
static struct tb_cell cell_buffer[CELL_BUFFER_LEN];

static Index currentIndex_(0, 0);
  
int editLinePos_ = 0;
static Str editLine_;
static Str yankBuffer_;

extern void parseAndExecute(Str const& command);
extern void complete_command(String command);

void update_cursor()
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

void navigate_left()
{
  if (currentIndex_.x > 0)
   currentIndex_.x--;

  if (currentIndex_.x < current_column_scroll)
    current_column_scroll = currentIndex_.x;
}

void navigate_right()
{
  if (currentIndex_.x < (doc::getColumnCount() - 1))
   currentIndex_.x++;
}

void navigate_up()
{
  if (currentIndex_.y > 0)
    currentIndex_.y--;

  if (currentIndex_.y < current_row_scroll)
    current_row_scroll = currentIndex_.y;
}

void navigate_down()
{
  if (currentIndex_.y < (doc::getRowCount() - 1))
    currentIndex_.y++;
}

void handle_navigate_event(struct tb_event * event)
{
  switch (event->key)
  {
    case TB_KEY_ARROW_LEFT:
      navigate_left();
      break;

    case TB_KEY_ARROW_RIGHT:
      navigate_right();
      break;

    case TB_KEY_ARROW_UP:
      navigate_up();
      break;

    case TB_KEY_ARROW_DOWN:
      navigate_down();
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

          case 'y':
            yankBuffer_ = doc::getCellText(currentIndex_);
            break;

          case 'p':
            doc::setCellText(currentIndex_, yankBuffer_);
            navigate_down();
            break;

          case 'P':
            doc::setCellText(currentIndex_, yankBuffer_);
            navigate_right();
            break;

          case 'd':
            doc::setCellText(currentIndex_, Str::EMPTY);
            break;

        }
      }
      break;
  }
}

void handle_text_input(struct tb_event * event)
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

void handle_edit_event(struct tb_event * event)
{
  handle_text_input(event);

  switch (event->key)
  {
    case TB_KEY_ESC:
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_TAB:
      doc::setCellText(currentIndex_, editLine_);
      editLine_.clear();
      navigate_right();
      editMode_ = EditorMode::NAVIGATE;
      break;

    case TB_KEY_ENTER:
      doc::setCellText(currentIndex_, editLine_);
      editLine_.clear();
      navigate_down();
      editMode_ = EditorMode::NAVIGATE;
      break;
  }
}

void handle_command_event(struct tb_event * event)
{
  handle_text_input(event);

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

void handle_key_event(struct tb_event * event)
{
  switch (editMode_)
  {
    case EditorMode::NAVIGATE:
      handle_navigate_event(event);
      break;

    case EditorMode::EDIT:
      handle_edit_event(event);
      break;

    case EditorMode::COMMAND:
      handle_command_event(event);
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

static int draw_column_count = 0;
static ColumnInfo draw_column_info[128];

void draw_headers()
{
  // Calculate the with of the columns we should display
  memset(draw_column_info, 0, sizeof(draw_column_info));
  draw_column_count = 0;

  int x = ROW_HEADER_WIDTH;
  for (int i = current_column_scroll; i < doc::getColumnCount(); ++i, ++draw_column_count)
  {
    const int width = doc::getColumnWidth(i);

    draw_column_info[draw_column_count].x = x;
    draw_column_info[draw_column_count].width = width;

    x += width;
    if (x >= tb_width())
      break;
  }

  // Draw column header
  for (int x = 0; x < draw_column_count; ++x)
  {
    const uint16_t color = (x + current_column_scroll) == currentIndex_.x ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

    Str header;
    header.append(' ')
          .append((char)('A' + x + current_column_scroll));

    drawText(draw_column_info[x].x, 0, draw_column_info[x].width, color, color, header);
  }

  // Draw row header
  const int y_end = doc::getRowCount() < (tb_height() - 2) ? doc::getRowCount() : tb_height() - 2;
  for (int y = 0; y < tb_height() - 2; ++y)
  {
    const uint16_t color = (y + current_row_scroll) == currentIndex_.y ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

    Str header;

    if (y >= current_row_scroll && y < y_end)
      header = doc::rowToLabel(y);

    drawText(0, y - current_column_scroll + 1, ROW_HEADER_WIDTH, color, color, header);
  }
}

void draw_workspace()
{
  const int y_end = doc::getRowCount() < (tb_height() - 2) ? doc::getRowCount() : tb_height() - 1;

  for (int y = current_row_scroll; y < y_end; ++y)
  {
    for (int x = 0; x < draw_column_count; ++x)
    {
      const int selected = x == currentIndex_.x && y == currentIndex_.y;
      const uint16_t color = selected ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

      const int width = doc::getColumnWidth(x + current_column_scroll);

      if (selected && editMode_ == EditorMode::EDIT)
        drawText(draw_column_info[x + current_column_scroll].x, y + 1, width, color, color, editLine_);
      else
        drawText(draw_column_info[x + current_column_scroll].x, y + 1, width, color, color, doc::getCellText(Index(x + current_column_scroll, y)));
    }
  }
}

void draw_commandline()
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

  infoLine = doc::getFilename();

  drawText(0, tb_height() - 2, tb_width(), TB_REVERSE | TB_DEFAULT, TB_REVERSE | TB_DEFAULT, infoLine);
  drawText(0, tb_height() - 1, tb_width(), TB_DEFAULT, TB_DEFAULT, commandLine);
}