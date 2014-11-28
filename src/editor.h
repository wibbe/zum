
#ifndef EDITOR_H
#define EDITOR_H

#include "termbox.h"

enum EditorMode
{
  NAVIGATE_MODE,
  EDIT_MODE,
  COMMAND_MODE
};

extern int editor_mode;
extern int current_row;
extern int current_column;

void handle_key_event(struct tb_event * event);
void update_cursor();

void draw_headers();
void draw_workspace();
void draw_commandline();

#endif
