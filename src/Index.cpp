
#include "Index.h"
#include "Tcl.h"
#include "Document.h"

#include <cmath>
#include <cctype>

std::string Index::rowToStr(int row)
{
  return str::fromInt(row + 1);
}

std::string Index::columnToStr(int col)
{
  int power = 0;
  for (int i = 0; i < 10; ++i)
  {
    const int value = std::pow(26, i);
    if (value > col)
    {
      power = i;
      break;
    }
  }

  std::string result;
  while (power >= 0)
  {
    const int value = std::pow(26, power);
    if (value > col)
    {
      power--;

      if (power < 0)
        result.append(1, 'A');
    }
    else
    {
      const int division = col / value;
      const int remainder = col % value;

      result.append(1, 'A' + division);

      col = remainder;
      power--;
    }
  }

  return result;
}

int Index::strToColumn(std::string const& str)
{
  if (str.empty())
    return 0;

  if (std::isdigit(str.front()))
  {
    return std::stoi(str);
  }
  else
  {
    int column = 0;
    int i = 0;

    while ((i < str.size()) && std::isupper(str[i]))
    {
      column *= 26;
      column += str[i] - 'A';
      i++;
    }

    return column;
  }
}

std::string Index::toStr() const
{
  return columnToStr(x) + rowToStr(y);
}

Index Index::fromStr(std::string const& str)
{
  Index idx(0, 0);

  if (str.empty())
    return idx;

  int i = 0;
  while ((i < str.size()) && std::isupper(str[i]))
  {
    idx.x *= 26;
    idx.x += str[i] - 'A';
    i++;
  }

  while ((i < str.size()) && std::isdigit(str[i]))
  {
    idx.y *= 10;
    idx.y += str[i] - '0';
    i++;
  }

  if (idx.y > 0)
    idx.y--;

  return idx;
}


namespace tcl {
  TCL_SUBFUNC(index, "new",     "column row", "Construct a new index",
                     "row",     "index",      "Returns the row in the supplied index",
                     "column",  "index",      "Returns the column in the supplied index")
  {
    enum { CMD_NEW, CMD_ROW, CMD_COLUMN };

    switch (subCommand)
    {
      case CMD_NEW:
        {
          TCL_CHECK_ARG_DESC(2, "column row");
          TCL_INT_ARG(0, column);
          TCL_INT_ARG(1, row);
          TCL_STRING_RESULT(Index(column, row).toStr());
        }
        break;

      case CMD_ROW:
        {
          TCL_CHECK_ARG_DESC(1, "index");
          TCL_STRING_ARG(0, index);
          TCL_INT_RESULT(Index::fromStr(index).y);
        }
        break;

      case CMD_COLUMN:
        {
          TCL_CHECK_ARG_DESC(1, "index");
          TCL_STRING_ARG(0, index);
          TCL_INT_RESULT(Index::fromStr(index).x);
        }
        break;
    }

    return JIM_OK;
  }
}

