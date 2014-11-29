
#include <stdio.h>

#include "termbox.h"
#include "Document.h"
#include "Editor.h"
#include "Commands.h"

int application_running = 1;

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
      doc::createEmpty();
  }
  else
    doc::createEmpty();

  updateCursor();
  drawInterface();

  struct tb_event event;

  while (application_running)
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
      executeCommandLine();
      updateCursor();
      drawInterface();
    }
    else
    {
    }
  }

  tb_shutdown();
  return 0;
}