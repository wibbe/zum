
#include "Completion.h"
#include "Editor.h"
#include "Tcl.h"

void completeEditLine(Str & editLine)
{
  const std::vector<Str> parts = editLine.split(' ', true);

  const std::vector<Str> hints = tcl::findMatches(parts.empty() ? Str::EMPTY : parts.back());

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

      editLine.append(hints.front());
    }
    else
      setCompletionHints(hints);
  }
}