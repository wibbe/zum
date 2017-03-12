
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

  const Str buffer = Str(keySequence);

  if (buffer.size() == 0 || buffer.size() > 2)
  {
    logError("error in keysequence '", keySequence, "', must be 1 or 2 characters long");
    return JIM_ERR;
  }

  EditCommand * command = nullptr;
  if (getEditCommand(buffer[0], buffer.size() == 2 ? buffer[1] : 0, &command))
  {
    logInfo("Rebinding key-sequence '", keySequence, "' to ", commandStr);

    command->manualRepeat = false;
    command->description = description;
    command->command = [commandStr] (int) { tcl::evaluate(commandStr); };
  }
  else
  {
    editCommands_.push_back({
      buffer[0], buffer.size() == 2 ? buffer[1] : 0, false,
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
