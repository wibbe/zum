
#include "str.h"
#include "editor.h"
#include "document.h"

extern int application_running;

typedef void (*Command)();

void parse_and_execute_command(String command)
{
  if (string_len(command) == 0)
    return;

  switch (command[0])
  {
    case 'q':
      application_running = 0;
      break;

    case 'n':
      doc_create_empty();
      break;

    case 'w':
      {
        String filename;
        string_copy(filename, command);
        string_erase_char(filename, 0);
        string_erase_char(filename, 0);
        if (string_len(filename) > 0)
          doc_save(filename);
      }
  }
}

void complete_command(String command)
{
}