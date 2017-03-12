
#ifndef EDITOR_H
#define EDITOR_H

#include "Cell.h"
#include "Index.h"
#include "View.h"

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
void navigateHome();
void navigateEnd();

bool findNextMatch();
bool findPreviousMatch();

void editCurrentCell();

void handleKeyEvent(view::Event * event);
void updateCursor();

void clearFlashMessage();
void flashMessage(std::string const& message);

void clearMessageLines();
void setCompletionHints(std::vector<std::string> const& hints);

void drawInterface();
void drawHeaders();
void drawWorkspace();
void drawCommandLine();

#endif
