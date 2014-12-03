
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

EditorMode getEditorMode();

Index getCursorPos();
void setCursorPos(Index const& idx);

Str getYankBuffer();
void yankCurrentCell();

void navigateLeft();
void navigateRight();
void navigateUp();
void navigateDown();

void editCurrentCell();

void handleKeyEvent(struct tb_event * event);
void updateCursor();

void clearFlashMessage();
void flashMessage(Str const& message);

void drawInterface();
void drawHeaders();
void drawWorkspace();
void drawCommandLine();

#endif
