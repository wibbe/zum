
#include "Str.h"
#include "Editor.h"
#include "Document.h"
#include "Commands.h"
#include "Help.h"
#include "Tokenizer.h"
#include "Tcl.h"


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
  {
    {'o', 0}, true,
    "Add new row below the current and start editing that row",
    [] (int) {
      if (!doc::isReadOnly())
      {
        Index idx = getCursorPos();
        doc::addRow(idx.y);
        idx.y += 1;
        setCursorPos(idx);
        editCurrentCell();
      }
    }
  },
  {
    {'O', 0}, true,
    "Add new row above the current and start editing that row",
    [] (int) {
      if (!doc::isReadOnly() && getCursorPos().y > 0)
      {
        Index idx = getCursorPos();
        doc::addRow(idx.y - 1);
        editCurrentCell();
      }
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
        if (doc::getColumnCount() > 1)
        {
          Index pos = getCursorPos();
          doc::removeColumn(pos.x);

          if (pos.x >= doc::getColumnCount())
            pos.x--;

          setCursorPos(pos);
        }
        else
          flashMessage(Str("Not allowed to delete the last column"));
      }
    }
  },
  {
    {'d', 'r'}, false,
    "Delete the current row",
    [] (int) {
      if (!doc::isReadOnly())
      {
        if (doc::getRowCount() > 1)
        {
          Index pos = getCursorPos();
          doc::removeRow(pos.y);

          if (pos.y >= doc::getRowCount())
            pos.y--;

          setCursorPos(pos);
        }
        else
          flashMessage(Str("Not allowed to delete the last row"));
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
  {
    {'a', 'c'}, false,
    "Insert a new column after the current column",
    [] (int) {
      Index idx = getCursorPos();
      doc::addColumn(idx.x);
      idx.x += 1;
      setCursorPos(idx);
    }
  },
  {
    {'a', 'r'}, false,
    "Insert a new row after the current row",
    [] (int) {
      Index idx = getCursorPos();
      doc::addRow(idx.y);
      idx.y += 1;
      setCursorPos(idx);
    }
  },
  {
    {'a', 'p'}, false,
    "Append the yank-buffer to the current cell",
    [] (int) {
      if (!doc::isReadOnly())
      {
        Str text = doc::getCellText(getCursorPos());
        text.append(getYankBuffer());

        doc::setCellText(getCursorPos(), text);
      }
    }
  },
  {
    {'t', 'r'}, false,
    "Right-justify the text in the current cell",
    [] (int) {
      const Index idx = getCursorPos();
      const int columnWidth = doc::getColumnWidth(idx.x);
      const Str text = doc::getCellText(idx).stripWhitespace();

      if (text.size() < columnWidth)
      {
        const int spacing = columnWidth - text.size();
        Str newText;
        for (int i = 0; i < spacing; ++i)
          newText.append(' ');
        newText.append(text);

        doc::setCellText(idx, newText);
      }
    }
  },
  {
    {'t', 'c'}, false,
    "Center-justify the text in the current cell",
    [] (int) {
      const Index idx = getCursorPos();
      const int columnWidth = doc::getColumnWidth(idx.x);
      const Str text = doc::getCellText(idx).stripWhitespace();

      if (text.size() < columnWidth)
      {
        const int spacing = (columnWidth - text.size()) / 2;
        Str newText;
        for (int i = 0; i < spacing; ++i)
          newText.append(' ');
        newText.append(text);

        doc::setCellText(idx, newText);
      }
    }
  },
  {
    {'t', 'l'}, false,
    "Left-justify the text in the current cell",
    [] (int) {
      const Index idx = getCursorPos();
      const Str text = doc::getCellText(idx).stripWhitespace();

      doc::setCellText(idx, text);
    }
  },
};

extern void quitApplication();

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

std::vector<EditCommand> const& getEditCommands()
{
  return editCommands_;
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

void executeAppCommands(Str const& commandLine)
{
  tcl::evaluate(commandLine);
}

Str completeCommand(Str const& command)
{
  return tcl::completeName(command);
}

// -- Application wide commands --

TCL_PROC(q)
{
  quitApplication();

  TCL_OK();
}

TCL_PROC(n)
{
  if (args.size() == 2)
    doc::createEmpty(args[1]);
  else
    doc::createEmpty(Str::EMPTY);

  TCL_OK();
}

TCL_PROC(e)
{
  TCL_ARITY(1);

  setCursorPos(Index(0, 0));
  doc::load(args[1]);

  TCL_OK();
}

TCL_PROC(w)
{
  if (args.size() == 2 && !args[1].empty())
    doc::save(args[1]);
  else if (!doc::getFilename().empty())
    doc::save(doc::getFilename());

  TCL_OK();
}

TCL_PROC(edit)
{
  TCL_ARITY(1);

  for (auto i = 1; i < args.size(); ++i)
  {
    const Str arg = args[i];
    if (!arg.empty())
    {
      clearEditCommandSequence();
      for (auto ch : args[1])
        pushEditCommandKey(ch);

      executeEditCommands();
    }
  }

  TCL_OK();
}

TCL_PROC(help)
{
  doc::loadRaw(getHelpDocument(), Str("[Help]"));
  TCL_OK();
}
