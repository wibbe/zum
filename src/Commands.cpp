
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
    "Clear the selected cells",
    [] (int) {
      for (auto const& idx : doc::selectedCells())
        doc::setCellText(idx, "");
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
    [] (int) { 
      for (auto const& idx : doc::selectedColumns())
        doc::increaseColumnWidth(idx.x);
    }
  },
  { // Decrease column with
    {'-', 0}, false,
    "Decrease current column width",
    [] (int) { 
      for (auto const& idx : doc::selectedColumns())
        doc::decreaseColumnWidth(idx.x);
    }
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
    {'f', 'r'}, false,
    "Right-justify the text in the current cell",
    [] (int) {
      for (auto const& idx : doc::selectedCells())
      {
        const uint32_t oldFormat = doc::getCellFormat(idx);
        const uint32_t newFormat = (oldFormat & ~ALIGN_MASK) | ALIGN_RIGHT;
        doc::setCellFormat(idx, newFormat);
      }
    }
  },
  {
    {'f', 'c'}, false,
    "Center-justify the text in the current cell",
    [] (int) {
      for (auto const& idx : doc::selectedCells())
      {
        const uint32_t oldFormat = doc::getCellFormat(idx);
        const uint32_t newFormat = (oldFormat & ~ALIGN_MASK) | ALIGN_CENTER;
        doc::setCellFormat(idx, newFormat);
      }
    }
  },
  {
    {'f', 'l'}, false,
    "Left-justify the text in the current cell",
    [] (int) {
      for (auto const& idx : doc::selectedCells())
      {
        const uint32_t oldFormat = doc::getCellFormat(idx);
        const uint32_t newFormat = (oldFormat & ~ALIGN_MASK) | ALIGN_LEFT;
        doc::setCellFormat(idx, newFormat);
      }
    }
  },
  {
    {'f', 'n'}, false,
    "Normal font in the current cell",
    [] (int) {
      for (auto const& idx : doc::selectedCells())
      {
        const uint32_t oldFormat = doc::getCellFormat(idx);
        const uint32_t newFormat = oldFormat & ~FONT_MASK;
        doc::setCellFormat(idx, newFormat);
      }
    }
  },
  {
    {'f', 'b'}, false,
    "Bold font in the current cell",
    [] (int) {
      for (auto const& idx : doc::selectedCells())
      {
        const uint32_t oldFormat = doc::getCellFormat(idx);
        const uint32_t newFormat = oldFormat ^ FONT_BOLD;
        doc::setCellFormat(idx, newFormat);
      }
    }
  },
  {
    {'f', 'u'}, false,
    "Underline the font in the current cell",
    [] (int) {
      for (auto const& idx : doc::selectedCells())
      {
        const uint32_t oldFormat = doc::getCellFormat(idx);
        const uint32_t newFormat = oldFormat ^ FONT_UNDERLINE;
        doc::setCellFormat(idx, newFormat);
      }
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

TCL_FUNC(edit, "index")
{
  TCL_CHECK_ARG(2);
  TCL_STRING_ARG(1, index);

  doc::cursorPos() = Index::fromStr(index);
  editCurrentCell();

  return JIM_OK;
}

TCL_FUNC(cursor, "?index?", "Return and optionally set the position of the cursor in the current document")
{
  TCL_CHECK_ARGS(1, 2);
  TCL_STRING_ARG(1, index);

  if (argc == 2)
    doc::cursorPos() = Index::fromStr(index);

  TCL_STRING_RESULT(doc::cursorPos().toStr());
}

TCL_FUNC(bind, "keysequence command ?description?", "Bind a command to a specific keyboard sequence")
{
  TCL_CHECK_ARGS(3, 4);
  TCL_STRING_ARG(1, keySequence);
  TCL_STRING_ARG(2, commandStr);
  TCL_STRING_ARG(3, description);

  static const uint32_t BUFFER_LEN = 2;
  static uint32_t buffer[BUFFER_LEN];
  const uint32_t len = str::toUTF32(keySequence, buffer, BUFFER_LEN);

  if (len > 2)
  {
    logError("error in keysequence '", keySequence, "', maximum length is 2");
    return JIM_ERR;
  }

  EditCommand * command = nullptr;
  if (getEditCommand(buffer[0], buffer[1], &command))
  {
    logInfo("Rebinding key-sequence '", keySequence, "' to ", commandStr);

    command->manualRepeat = false;
    command->description = description;
    command->command = [commandStr] (int) { tcl::evaluate(commandStr); };
  }
  else
  {
    editCommands_.push_back({
      buffer[0], buffer[1], false,
      description,
      [commandStr] (int) { tcl::evaluate(commandStr); }
    });
  }

  return JIM_OK;
}

TCL_FUNC(flashMessage, "message")
{
  TCL_CHECK_ARG(2);
  TCL_STRING_ARG(1, message);
  flashMessage(message);
}
