
#include "Str.h"
#include "Editor.h"
#include "Document.h"
#include "Commands.h"
#include "Help.h"
#include "Tokenizer.h"

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
    "Create a new document",
    [] (Str const& arg) {
      doc::createEmpty(arg);
    }
  },
  {
    Str("w"),
    "[filename]",
    "Save the current document",
    [] (Str const& arg) {
      bool saved = false;
      if (arg.empty() && !doc::getFilename().empty())
        doc::save(doc::getFilename());
      else if (!arg.empty())
        doc::save(arg);
    }
  },
  {
    Str("e"),
    "filename",
    "Open a document",
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
  },
  {
    Str("tokenize"),
    "string",
    "Test the tokenizer",
    [] (Str const& arg) {
      Tokenizer tokenizer(arg);

      Str final;

      Token token = tokenizer.next();
      while (token != Token::EndOfFile && token != Token::Error)
      {
        switch (token)
        {
          case Token::Number:
            final.append(Str(" Number("))
                 .append(tokenizer.value())
                 .append(Str(")"));
            break;

          case Token::Cell:
            final.append(Str(" Cell("))
                 .append(tokenizer.value())
                 .append(Str(")"));
            break;

          case Token::Operator:
            final.append(Str(" Operator("))
                 .append(tokenizer.value())
                 .append(Str(")"));
            break;

          case Token::Identifier:
            final.append(Str(" Identifier("))
                 .append(tokenizer.value())
                 .append(Str(")"));
            break;

          case Token::LeftParenthesis:
            final.append(Str(" ("));
            break;

          case Token::RightParenthesis:
            final.append(Str(" )"));
            break;

          case Token::Comma:
            final.append(Str(" ,"));
            break;

        };

        token = tokenizer.next();
      }

      if (token == Token::Error)
      {
        final.set("Error: ");
        final.append(tokenizer.value());
      }

      flashMessage(final);
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
    else if ((commandLine.front() >= '0' && commandLine.front() <= '9') ||
             (commandLine.front() >= 'A' && commandLine.front() <= 'Z'))
    {
      const Index idx = doc::parseCellRef(commandLine);
      setCursorPos(idx);
      break;
    }
    else
      break;
  }
}

Str completeCommand(Str const& command)
{
  return Str::EMPTY;
}