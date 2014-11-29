
#include <stdio.h>

#include "termbox.h"
#include "Document.h"
#include "Editor.h"

int application_running = 1;

void draw_interface()
{
  tb_set_clear_attributes(TB_DEFAULT, TB_DEFAULT);
  tb_clear();

  draw_headers();
  draw_workspace();
  draw_commandline();

  tb_present();
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
      doc::createEmpty();
  }
  else
    doc::createEmpty();

  update_cursor();
  draw_interface();

  struct tb_event event;
  while (application_running && tb_poll_event(&event))
  {
    switch (event.type)
    {
      case TB_EVENT_KEY:
        handle_key_event(&event);
        break;

      case TB_EVENT_RESIZE:
        break;

      default:
        break;
    }

    update_cursor();
    draw_interface();
  }

  tb_shutdown();

  return 0;
}