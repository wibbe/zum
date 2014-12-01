
#include <stdio.h>

#include "termbox.h"
#include "Document.h"
#include "Editor.h"
#include "Commands.h"

static bool applicationRunning_ = true;
static int timeout_ = 0;

void quitApplication()
{
  applicationRunning_ = false;
}

void clearTimeout()
{
  timeout_ = 0;
}

int main(int argc, char * argv[])
{
  int result = tb_init();
  if (result)
  {
    fprintf(stderr, "Faild to initialize Termbox with error code %d\n", result);
    return 1;
  }

  if (argc > 1)
  {
    if (!doc::load(Str(argv[1])))
      doc::createEmpty(Str::EMPTY);
  }
  else
    doc::createEmpty(Str::EMPTY);

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

  tb_shutdown();
  return 0;
}