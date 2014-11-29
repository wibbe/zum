
#include "Str.h"
#include "Editor.h"
#include "Document.h"
#include "Commands.h"

#include <functional>

extern int application_running;

static Str commandSequence_;

struct Command
{
  uint32_t key;
  bool manualRepeat;
  std::function<void (int)> command;
};

// List of all commands we support
static std::vector<Command> commands_ = {
  { // Paste into current cell and move down
    'p', false,
    [] (int) { 
      doc::setCellText(getCursorPos(), getYankBuffer());
      navigateDown();
    }
  },
  { // Paste into current cell and move right
    'P', false,
    [] (int) { 
      doc::setCellText(getCursorPos(), getYankBuffer());
      navigateRight();
    }
  },
  { // Clear current cell and move down
    'd', false,
    [] (int) { 
      doc::setCellText(getCursorPos(), Str::EMPTY);
      navigateDown();
    }
  },
  { // Clear current cell and move right
    'D', false,
    [] (int) { 
      doc::setCellText(getCursorPos(), Str::EMPTY);
      navigateRight();
    }
  },
  { // Yank the current cell
    'y', false,
    [] (int) { yankCurrentCell(); }
  },
  { // Increase column width
    '+', false,
    [] (int) { doc::increaseColumnWidth(getCursorPos().x); }
  },
  { // Decrease column with
    '-', false,
    [] (int) { doc::decreaseColumnWidth(getCursorPos().x); }
  },
  { // Undo
    'u', false,
    [] (int) { doc::undo(); }
  },
  { // Redo
    'U', false,
    [] (int) { doc::undo(); }
  },
  { // Left
    'h', false,
    [] (int) { navigateLeft(); }
  },
  { // Right
    'l', false,
    [] (int) { navigateRight(); }
  },
  { // Up
    'k', false,
    [] (int) { navigateUp(); }
  },
  { // Down
    'j', false,
    [] (int) { navigateDown(); }
  },
  { // Jump to line
    '#', true,
    [] (int line) {
      const Index pos = getCursorPos();
      setCursorPos(Index(pos.x, line - 1));
    }
  }
};

static bool getCommand(uint32_t key, Command ** command)
{
  for (auto & cmd : commands_)
    if (cmd.key == key)
    {
      if (command)
        *command = &cmd;
      return true;
    }

  return false;
}


void pushCommandKey(uint32_t ch)
{
  commandSequence_.append(ch);
}

void executeCommandLine()
{
  if (!commandSequence_.empty())
  {
    Command * command = nullptr;
    int count = 0;

    for (auto ch : commandSequence_)
    {
      if (ch >= '0' && ch <= '9')
        count = count * 10 + (ch - '0');
      else if (!command)
        getCommand(ch, &command);
    }

    if (command) 
    {
      if (count == 0)
        count = 1;

      if (command->manualRepeat)
        command->command(count);
      else
        for (; count > 0; --count)
          command->command(0);

      commandSequence_.clear();
    }
  }
}


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

Str completeCommand(Str const& command)
{
  return Str::EMPTY;
}