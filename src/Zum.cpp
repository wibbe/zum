
#include <stdio.h>

#include "termbox.h"
#include "Document.h"
#include "Editor.h"

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
  while (application_running && tb_poll_event(&event))
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

    updateCursor();
    drawInterface();
  }

  tb_shutdown();

  return 0;
}