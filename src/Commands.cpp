
#include "Str.h"
#include "Editor.h"
#include "Document.h"

extern int application_running;

typedef void (*Command)();

void parseAndExecute(Str const& command)
{
  if (command.empty())
    return;

  switch (command[0])
  {
    case 'q':
      application_running = 0;
      break;

    case 'n':
      doc::createEmpty();
      break;

    case 'w':
      {
        Str filename = command;
        filename.erase(0);
        filename.erase(0);

        if (!filename.empty())
          doc::save(filename);
      }
  }
}

void complete_command(String command)
{
}