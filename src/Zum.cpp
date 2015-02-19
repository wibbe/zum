
#include <stdio.h>

#include "termbox.h"
#include "Document.h"
#include "Editor.h"
#include "Commands.h"
#include "Tcl.h"
#include "Log.h"

static bool applicationRunning_ = true;
static int timeout_ = 0;

FUNC_0(quitApplication, "app:quit")
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

  int result = tb_init();
  if (result)
  {
    logError("Faild to initialize Termbox with error code", result);
    return 1;
  }

  tcl::initialize();

  if (argc > 1)
  {
    if (!doc::load(Str(argv[1])))
      doc::createDefaultEmpty();
  }
  else
    doc::createDefaultEmpty();

  updateCursor();
  drawInterface();

  struct tb_event event;

  while (applicationRunning_)
  {
    if (tb_peek_event(&event, 100) > 0)
    {
      switch (event.type)
      {
        case TB_EVENT_KEY:
          handleKeyEvent(&event);
          break;

        case TB_EVENT_RESIZE:
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
  tb_shutdown();
  return 0;
}