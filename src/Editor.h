
#ifndef EDITOR_H
#define EDITOR_H

#include "termbox.h"
#include "Cell.h"
#include "Index.h"

enum class EditorMode
{
  NAVIGATE,
  EDIT,
  COMMAND,
  SEARCH,
};

EditorMode getEditorMode();

std::string getYankBuffer();
void yankCurrentCell();

void navigateLeft();
void navigateRight();
void navigateUp();
void navigateDown();
void navigatePageUp();
void navigatePageDown();

bool findNextMatch();
bool findPreviousMatch();

void editCurrentCell();

void handleKeyEvent(struct tb_event * event);
void updateCursor();

void clearFlashMessage();
void flashMessage(std::string const& message);

void clearCompletionHints();
void setCompletionHints(std::vector<std::string> const& hints);

void drawInterface();
void drawHeaders();
void drawWorkspace();
void drawCommandLine();

#endif
