
#include "editor.h"
#include "document.h"

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>

static const int ROW_HEADER_WIDTH = 6;

int editor_mode = NAVIGATE_MODE;
int current_row = 0;
int current_column = 0;
int current_row_scroll = 0;
int current_column_scroll = 0;

extern int application_running;

typedef struct {
  int x;
  int width;
} ColumnInfo;

#define CELL_BUFFER_LEN 1024
static struct tb_cell cell_buffer[CELL_BUFFER_LEN];

int edit_line_pos = 0;
String edit_line;
String yank_buffer;

extern void parse_and_execute_command(String command);
extern void complete_command(String command);

void update_cursor()
{
  switch (editor_mode)
  {
    case NAVIGATE_MODE:
      tb_set_cursor(-1, -1);
      break;

    case EDIT_MODE:
      tb_set_cursor(edit_line_pos, tb_height() - 1);
      break;

    case COMMAND_MODE:
      tb_set_cursor(edit_line_pos + 1, tb_height() - 1);
      break;
  }
}

void navigate_left()
{
  if (current_column > 0)
   current_column--;

  if (current_column < current_column_scroll)
    current_column_scroll = current_column;
}

void navigate_right()
{
  if (current_column < (doc_get_column_count() - 1))
   current_column++;
}

void navigate_up()
{
  if (current_row > 0)
    current_row--;

  if (current_row < current_row_scroll)
    current_row_scroll = current_row;
}

void navigate_down()
{
  if (current_row < (doc_get_row_count() - 1))
    current_row++;
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
              editor_mode = COMMAND_MODE;
              string_clear(edit_line);
              edit_line_pos = 0;
            }
            break;

          case 'i':
            {
              editor_mode = EDIT_MODE;
              string_copy(edit_line, *doc_get_cell_text(current_column, current_row));
              edit_line_pos = string_len(edit_line);
            }
            break;

          case 'y':
            string_copy(yank_buffer, *doc_get_cell_text(current_column, current_row));
            break;

          case 'p':
            doc_set_cell_text(current_column, current_row, yank_buffer);
            navigate_down();
            break;

          case 'P':
            doc_set_cell_text(current_column, current_row, yank_buffer);
            navigate_right();
            break;

          case 'd':
            doc_set_cell_text(current_column, current_row, EMPTY_STR);
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
      if (edit_line_pos > 0)
        edit_line_pos--;
      break;

    case TB_KEY_ARROW_RIGHT:
      if (edit_line_pos < string_len(edit_line))
        edit_line_pos++;
      break;

    case TB_KEY_SPACE:
      string_insert_char(edit_line, edit_line_pos, ' ');
      edit_line_pos++;
      break;

    case TB_KEY_BACKSPACE:
    case TB_KEY_BACKSPACE2:
      if (edit_line_pos > 0)
      {
        string_erase_char(edit_line, edit_line_pos - 1);
        edit_line_pos--;
      }
      break;

    case TB_KEY_DELETE:
      {
        string_erase_char(edit_line, edit_line_pos);
        int len = string_len(edit_line);
        if (edit_line_pos > len)
          edit_line_pos = len;
      }
      break;

    default:
      {
        if (event->ch)
        {
          string_insert_char(edit_line, edit_line_pos, event->ch);
          edit_line_pos++;
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
      editor_mode = NAVIGATE_MODE;
      break;

    case TB_KEY_ENTER:
      doc_set_cell_text(current_column, current_row, edit_line);
      string_clear(edit_line);
      navigate_right();
      editor_mode = NAVIGATE_MODE;
      break;
  }
}

void handle_command_event(struct tb_event * event)
{
  handle_text_input(event);

  switch (event->key)
  {
    case TB_KEY_ENTER:
      parse_and_execute_command(edit_line);
      editor_mode = NAVIGATE_MODE;
      break;

    case TB_KEY_TAB:
      complete_command(edit_line);
      break;

    case TB_KEY_ESC:
      editor_mode = NAVIGATE_MODE;
      break;
  }
}

void handle_key_event(struct tb_event * event)
{
  switch (editor_mode)
  {
    case NAVIGATE_MODE:
      handle_navigate_event(event);
      break;

    case EDIT_MODE:
      handle_edit_event(event);
      break;

    case COMMAND_MODE:
      handle_command_event(event);
      break;
  }
}

void draw_text(int x, int y, int len, uint16_t fg, uint16_t bg, String str)
{
  if (len == -1)
  {
    int i = 0;
    while (str[i])
      tb_change_cell(x + i, y, str[i], fg, bg);
  }
  else
  {
    for (int i = 0; i < len; ++i)
      tb_change_cell(x + i, y, str[i], fg, bg);
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
  for (int i = current_column_scroll; i < doc_get_column_count(); ++i, ++draw_column_count)
  {
    const int width = doc_get_column_width(i);

    draw_column_info[draw_column_count].x = x;
    draw_column_info[draw_column_count].width = width;

    x += width;
    if (x >= tb_width())
      break;
  }

  // Draw column header
  for (int x = 0; x < draw_column_count; ++x)
  {
    const uint16_t color = (x + current_column_scroll) == current_column ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

    String header;
    string_clear(header);
    string_append_char(header, ' ');
    string_append_char(header, (char)('A' + x + current_column_scroll));

    draw_text(draw_column_info[x].x, 0, draw_column_info[x].width, color, color, header);
  }

  // Draw row header
  const int y_end = doc_get_row_count() < (tb_height() - 2) ? doc_get_row_count() : tb_height() - 2;
  for (int y = 0; y < tb_height() - 2; ++y)
  {
    const uint16_t color = (y + current_row_scroll) == current_row ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

    String header;
    string_clear(header);

    if (y >= current_row_scroll && y < y_end)
      doc_row_to_label(header, y);

    draw_text(0, y - current_column_scroll + 1, ROW_HEADER_WIDTH, color, color, header);
  }
}

void draw_workspace()
{
  const int y_end = doc_get_row_count() < (tb_height() - 2) ? doc_get_row_count() : tb_height() - 1;

  for (int y = current_row_scroll; y < y_end; ++y)
  {
    for (int x = 0; x < draw_column_count; ++x)
    {
      const int selected = x == current_column && y == current_row;
      const uint16_t color = selected ? TB_REVERSE | TB_DEFAULT : TB_DEFAULT;

      const int width = doc_get_column_width(x + current_column_scroll);

      if (selected && editor_mode == EDIT_MODE)
        draw_text(draw_column_info[x + current_column_scroll].x, y + 1, width, color, color, edit_line);
      else
        draw_text(draw_column_info[x + current_column_scroll].x, y + 1, width, color, color, *doc_get_cell_text(x + current_column_scroll, y));
    }
  }
}

void draw_commandline()
{
  String info_line;
  String command_line;

  switch (editor_mode)
  {
    case NAVIGATE_MODE:
      string_copy(command_line, *doc_get_cell_text(current_column, current_row));
      break;

    case EDIT_MODE:
      string_copy(command_line, edit_line);
      break;

    case COMMAND_MODE:
      string_clear(command_line);
      string_append_char(command_line, ':');
      string_append(command_line, edit_line);
      break;
  }

  string_append(info_line, *doc_get_filename());

  draw_text(0, tb_height() - 2, tb_width(), TB_REVERSE | TB_DEFAULT, TB_REVERSE | TB_DEFAULT, info_line);
  draw_text(0, tb_height() - 1, tb_width(), TB_DEFAULT, TB_DEFAULT, command_line);
}