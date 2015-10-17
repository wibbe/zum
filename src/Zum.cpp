
#include <stdio.h>

#include "termbox.h"
#include "Document.h"
#include "Editor.h"
#include "Commands.h"
#include "Tcl.h"
#include "Log.h"
#include "View.h"

static bool applicationRunning_ = true;
static int timeout_ = 0;

static const tcl::Variable DEFAULT_WIDTH("app_defaultWidth", 120);
static const tcl::Variable DEFAULT_HEIGHT("app_defaultHeight", 40);

TCL_FUNC(quit, "", "Quit the application")
{
  applicationRunning_ = false;
  return true;
}

void clearTimeout()
{
  timeout_ = 0;
}

int main(int argc, char * argv[])
{
  clearLog();

  logInfo("Initializing Tcl...");
  tcl::initialize();

  logInfo("Initializing view...");
  if (!view::init(DEFAULT_WIDTH.toInt(), DEFAULT_HEIGHT.toInt(), "Zum"))
  {
    logError("Faild to initialize the view");
    return 1;
  }

  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
      doc::load(argv[i]);
  }

  if (doc::getOpenBufferCount() == 0)
    doc::createDefaultEmpty();

  updateCursor();
  drawInterface();

  view::Event event;

  logInfo("Application running...");

  while (applicationRunning_)
  {
    if (view::peekEvent(&event, 100))
    {
      switch (event.type)
      {
        case view::EVENT_KEY:
          handleKeyEvent(&event);
          break;

        case view::EVENT_RESIZE:
          break;

        case view::EVENT_QUIT:
          applicationRunning_ = false;
          break;

        default:
          break;
      }

      // Only update cursor and redraw interface when we have recievied an event
      executeEditCommands();
      updateCursor();
      drawInterface();
    }
    else
    {
      timeout_++;
      if (timeout_ > 20)
      {
        clearFlashMessage();
        drawInterface();
        clearTimeout();
      }
    }
  }

  tcl::shutdown();
  view::shutdown();

  return 0;
}