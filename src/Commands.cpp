
#include "Str.h"
#include "Editor.h"
#include "Document.h"
#include "Commands.h"
#include "Help.h"

static Str commandSequence_;

// List of all commands we support
static std::vector<EditCommand> editCommands_ = {
  { // Paste into current cell and move down
    {'p', 0}, false,
    "Paste to the current cell and move down",
    [] (int) {
      if (!doc::isReadOnly())
      {
        doc::setCellText(getCursorPos(), getYankBuffer());
        navigateDown();
      }
    }
  },
  { // Paste into current cell and move right
    {'P', 0}, false,
    "Paste to the current cell and move right",
    [] (int) {
      if (!doc::isReadOnly())
      {
        doc::setCellText(getCursorPos(), getYankBuffer());
        navigateRight();
      }
    }
  },
  { // Clear current cell
    {'d', 'd'}, false,
    "Clear the current cell",
    [] (int) { 
      doc::setCellText(getCursorPos(), Str::EMPTY);
    }
  },
  { // Clear current cell
    {'d', 'w'}, false,
    "Clear the current cell",
    [] (int) { 
      doc::setCellText(getCursorPos(), Str::EMPTY);
    }
  },
  {
    {'d', 'c'}, false,
    "Delete the current column",
    [] (int) {
      if (!doc::isReadOnly())
      {
        Index pos = getCursorPos();
        doc::removeColumn(pos.x);

        if (pos.x >= doc::getColumnCount())
          pos.x--;

        setCursorPos(pos);
      }
    }
  },
  {
    {'d', 'r'}, false,
    "Delete the current row",
    [] (int) { 
      if (!doc::isReadOnly())
      {
        Index pos = getCursorPos();
        doc::removeRow(pos.y);

        if (pos.y >= doc::getRowCount())
          pos.y--;
        
        setCursorPos(pos);
      }
    }
  },
  { // Yank the current cell
    {'y', 0}, false,
    "Yank from the current cell",
    [] (int) { yankCurrentCell(); }
  },
  { // Increase column width
    {'+', 0}, false,
    "Increase current column with",
    [] (int) { doc::increaseColumnWidth(getCursorPos().x); }
  },
  { // Decrease column with
    {'-', 0}, false,
    "Decrease current column width",
    [] (int) { doc::decreaseColumnWidth(getCursorPos().x); }
  },
  { // Undo
    {'u', 0}, false,
    "Undo",
    [] (int) { doc::undo(); }
  },
  { // Redo
    {'U', 0}, false,
    "Redo",
    [] (int) { doc::redo(); }
  },
  { // Left
    {'h', 0}, false,
    "Move left",
    [] (int) { navigateLeft(); }
  },
  { // Right
    {'l', 0}, false,
    "Move right",
    [] (int) { navigateRight(); }
  },
  { // Up
    {'k', 0}, false,
    "Move up",
    [] (int) { navigateUp(); }
  },
  { // Down
    {'j', 0}, false,
    "Move down",
    [] (int) { navigateDown(); }
  },
  { // Jump to row
    {'#', 0}, true,
    "Jump to row",
    [] (int line) {
      const Index pos = getCursorPos();
      setCursorPos(Index(pos.x, line - 1));
    }
  },
  {
    {'a', 'c'}, false,
    "Insert a new column after the current column",
    [] (int) { doc::addColumn(getCursorPos().x); }
  },
  {
    {'a', 'r'}, false,
    "Insert a new row after the current row",
    [] (int) { doc::addRow(getCursorPos().y); }
  },
};

extern void quitApplication();

static std::vector<AppCommand> appCommands_ = {
  { 
    Str("q"),
    "",
    "Quit application",
    [] (Str const& arg) { quitApplication(); }
  },
  {
    Str("n"),
    "",
    "New document",
    [] (Str const& arg) { doc::createEmpty(); }
  },
  {
    Str("w"),
    "[filename]",
    "Save document",
    [] (Str const& arg) {
      if (arg.empty() && !doc::getFilename().empty())
        doc::save(doc::getFilename());
      else if (!arg.empty())
        doc::save(arg);
    }
  },
  {
    Str("e"),
    "filename",
    "Open document",
    [] (Str const& arg) {
      if (!arg.empty())
      {
        setCursorPos(Index(0, 0));
        doc::load(arg);
      }
    }
  },
  {
    Str("help"),
    "",
    "Show help documentation",
    [] (Str const& arg) {
      setCursorPos(Index(0, 0));
      doc::loadRaw(getHelpDocument(), Str("[Help]"));
    }
  }
};

static bool getEditCommand(uint32_t key1, uint32_t key2, EditCommand ** command)
{
  for (auto & cmd : editCommands_)
    if (cmd.key[0] == key1 && (cmd.key[1] == 0 || cmd.key[1] == key2))
    {
      if (command)
        *command = &cmd;
      return true;
    }

  return false;
}

static bool getAppCommand(Str const& line, AppCommand ** command)
{
  for (auto & cmd : appCommands_)
    if (line.starts_with(cmd.id))
    {
      if (command)
        *command = &cmd;
      return true;
    }

  return false;
}

std::vector<EditCommand> const& getEditCommands()
{
  return editCommands_;
}

std::vector<AppCommand> const& getAppCommands()
{
  return appCommands_;
}

void pushEditCommandKey(uint32_t ch)
{
  commandSequence_.append(ch);
}

void clearEditCommandSequence()
{
  commandSequence_.clear();
}

void executeEditCommands()
{
  if (!commandSequence_.empty())
  {
    EditCommand * command = nullptr;
    int count = 0;

    for (int i = 0; i < commandSequence_.size(); ++i)
    {
      const auto ch1 = commandSequence_[i];
      const auto ch2 = (i + 1) < commandSequence_.size() ? commandSequence_[i + 1] : 0;

      if (ch1 >= '0' && ch1 <= '9')
        count = count * 10 + (ch1 - '0');
      else if (!command)
      {
        // If we found a command that uses two keys, increase i by one
        if (getEditCommand(ch1, ch2, &command))
          if (command->key[1] != 0)
            i++;
      }
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


void executeAppCommands(Str commandLine)
{
  while (!commandLine.empty())
  {
    AppCommand * command = nullptr;
    if (getAppCommand(commandLine, &command))
    {
      commandLine.pop_front(command->id.size());
      
      Str arg;
      if (commandLine.front() == ' ')
      {
        commandLine.pop_front();
        while (!commandLine.empty() && commandLine.front() != ' ')
        {
          arg.append(commandLine.front());
          commandLine.pop_front();
        }
      }

      command->command(arg);
    }
    else
      break;
  }
}

Str completeCommand(Str const& command)
{
  return Str::EMPTY;
}