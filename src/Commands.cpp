
#include "Str.h"
#include "Editor.h"
#include "Document.h"
#include "Commands.h"
#include "Tokenizer.h"
#include "Tcl.h"
#include "Log.h"


static Str commandSequence_;

// List of all commands we support
static std::vector<EditCommand> editCommands_ = {
  { // Paste into current cell and move down
    {'p', 0}, false,
    "Paste to the current cell and move down",
    [] (int) {
      if (!doc::isReadOnly())
      {
        doc::setCellText(doc::cursorPos(), getYankBuffer());
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
        doc::setCellText(doc::cursorPos(), getYankBuffer());
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
        Index idx = doc::cursorPos();
        doc::addRow(idx.y);
        idx.y += 1;
        doc::cursorPos() = idx;
        editCurrentCell();
      }
    }
  },
  {
    {'O', 0}, true,
    "Add new row above the current and start editing that row",
    [] (int) {
      if (!doc::isReadOnly() && doc::cursorPos().y > 0)
      {
        Index idx = doc::cursorPos();
        doc::addRow(idx.y - 1);
        editCurrentCell();
      }
    }
  },
  { // Clear current cell
    {'d', 'w'}, false,
    "Clear the current cell",
    [] (int) {
      doc::setCellText(doc::cursorPos(), "");
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
          Index pos = doc::cursorPos();
          doc::removeColumn(pos.x);

          if (pos.x >= doc::getColumnCount())
            pos.x--;

          doc::cursorPos() = pos;
        }
        else
          flashMessage("Not allowed to delete the last column");
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
          Index pos = doc::cursorPos();
          doc::removeRow(pos.y);

          if (pos.y >= doc::getRowCount())
            pos.y--;

          doc::cursorPos() = pos;
        }
        else
          flashMessage("Not allowed to delete the last row");
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
    [] (int) { doc::increaseColumnWidth(doc::cursorPos().x); }
  },
  { // Decrease column with
    {'-', 0}, false,
    "Decrease current column width",
    [] (int) { doc::decreaseColumnWidth(doc::cursorPos().x); }
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
  { // Find next match
    {'n', 0}, false,
    "Find next match",
    [] (int) { findNextMatch(); }
  },
  { // Find previous match
    {'N', 0}, false,
    "Find previous match",
    [] (int) { findPreviousMatch(); }
  },
  {
    {'a', 'c'}, false,
    "Insert a new column after the current column",
    [] (int) {
      Index idx = doc::cursorPos();
      doc::addColumn(idx.x);
      idx.x += 1;
      doc::cursorPos() = idx;
    }
  },
  {
    {'a', 'r'}, false,
    "Insert a new row after the current row",
    [] (int) {
      Index idx = doc::cursorPos();
      doc::addRow(idx.y);
      idx.y += 1;
      doc::cursorPos() = idx;
    }
  },
  {
    {'a', 'p'}, false,
    "Append the yank-buffer to the current cell",
    [] (int) {
      if (!doc::isReadOnly())
      {
        std::string text = doc::getCellText(doc::cursorPos());
        text.append(getYankBuffer());

        doc::setCellText(doc::cursorPos(), text);
      }
    }
  },
  {
    {'t', 'r'}, false,
    "Right-justify the text in the current cell",
    [] (int) {
      const Index idx = doc::cursorPos();
      const int columnWidth = doc::getColumnWidth(idx.x);
      const std::string text = str::stripWhitespace(doc::getCellText(idx));

      if (text.size() < columnWidth)
      {
        std::string newText(columnWidth - text.size(), ' ');
        newText.append(text);

        doc::setCellText(idx, newText);
      }
    }
  },
  {
    {'t', 'c'}, false,
    "Center-justify the text in the current cell",
    [] (int) {
      const Index idx = doc::cursorPos();
      const int columnWidth = doc::getColumnWidth(idx.x);
      const std::string text = str::stripWhitespace(doc::getCellText(idx));

      if (text.size() < columnWidth)
      {
        std::string newText((columnWidth - text.size()) / 2, ' ');
        newText.append(text);

        doc::setCellText(idx, newText);
      }
    }
  },
  {
    {'t', 'l'}, false,
    "Left-justify the text in the current cell",
    [] (int) {
      const Index idx = doc::cursorPos();
      doc::setCellText(idx, str::stripWhitespace(doc::getCellText(idx)));
    }
  },
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

static const int COMMAND_HISTORY_LENGHT = 1000;
static std::vector<std::string> commandHistory_;

void executeAppCommands(std::string const& commandLine)
{
  tcl::evaluate(commandLine);

  const std::string result = tcl::result();
  if (!result.empty())
    flashMessage(result);
}

// -- Application wide commands --

TCL_FUNC(e)
{
  TCL_CHECK_ARG(2, "filename");
  TCL_STRING_ARG(1, filename);

  doc::load(filename);
  return JIM_OK;
}

TCL_FUNC(w)
{
  TCL_CHECK_ARGS(1, 2, "?filename?");
  TCL_STRING_ARG(1, filename);

  if (argc == 2 && !filename.empty())
    doc::save(filename);
  else if (!doc::getFilename().empty())
    doc::save(doc::getFilename());

  return JIM_OK;
}

TCL_FUNC(app_edit)
{
  TCL_CHECK_ARG(2, "index");
  TCL_STRING_ARG(1, index);

  doc::cursorPos() = Index::fromStr(index);
  editCurrentCell();

  return JIM_OK;
}

TCL_FUNC(app_cursor)
{
  TCL_CHECK_ARGS(1, 2, "?index?");
  TCL_STRING_ARG(1, index);

  if (argc == 2)
    doc::cursorPos() = Index::fromStr(index);

  TCL_STRING_RESULT(doc::cursorPos().toStr());
}
