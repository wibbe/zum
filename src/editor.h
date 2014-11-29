
#ifndef EDITOR_H
#define EDITOR_H

#include "termbox.h"
#include "Types.h"

enum class EditorMode 
{
  NAVIGATE,
  EDIT,
  COMMAND
};

extern int editor_mode;
extern int current_row;
extern int current_column;

Index getCursorPos();
void setCursorPos(Index const& idx);

void handleKeyEvent(struct tb_event * event);
void updateCursor();

void drawInterface();
void drawHeaders();
void drawWorkspace();
void drawCommandLine();

#endif
