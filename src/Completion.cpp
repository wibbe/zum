
#include "Completion.h"
#include "Editor.h"
#include "Tcl.h"
#include "Log.h"

void completeEditLine(Str & editLine)
{
  const std::vector<Str> parts = editLine.split(' ', true);

  std::string word = parts.empty() ? std::string("") : parts.back().utf8();
  std::string removedPart = "";

  switch (word.front())
  {
    case '$':
    case '[':
      removedPart.append(1, word.front());
      word.erase(0, 1);
      break;
  }


  const std::vector<std::string> hints = tcl::findMatches(word);

  if (hints.empty())
  {
    clearCompletionHints();
  }
  else
  {
    if (hints.size() == 1)
    {
      clearCompletionHints();

      editLine.clear();
      for (int i = 0; i < (parts.size() - 1); ++i)
        editLine.append(parts[i]);

      editLine.append(Str(removedPart + hints.front())).append(' ');
    }
    else
    {
      // See if we append to the search word
      int len = word.size();
      while (true)
      {
        char ch = 0;
        if (len < hints.front().size())
          ch = hints.front()[len];
        else
          break;

        bool matches = true;
        for (auto const& hint : hints)
          matches &= (len < hint.size()) && (hint[len] == ch);

        if (!matches)
          break;

        ++len;
      }

      if (len > 0)
      {
        editLine.clear();
        for (int i = 0; i < (parts.size() - 1); ++i)
          editLine.append(parts[i]);

        editLine.append(Str(removedPart + hints.front().substr(0, len)));
      }

      setCompletionHints(hints);
    }
  }
}