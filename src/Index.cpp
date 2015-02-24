
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
  TCL_FUNC(index)
  {
    TCL_CHECK_ARGS(2, 1000, "index subcommand &argument ...?");

    int subcommand;
    static const char * const subcommands[] = {
      "new", "row", "column", nullptr
    };

    enum
    {
      CMD_NEW, CMD_ROW, CMD_COLUMN
    };

    if (Jim_GetEnum(interp, argv[1], subcommands, &subcommand, nullptr, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
        return JIM_ERR;

    switch (subcommand)
    {
      case CMD_NEW:
        {
          TCL_CHECK_ARG(4, "index new column row");
          TCL_INT_ARG(2, column);
          TCL_INT_ARG(3, row);
          TCL_STRING_RESULT(Index(column, row).toStr());
        }
        break;

      case CMD_ROW:
        {
          TCL_CHECK_ARG(3, "index row index");
          TCL_STRING_ARG(2, index);
          TCL_INT_RESULT(Index::fromStr(index).y);
        }
        break;

      case CMD_COLUMN:
        {
          TCL_CHECK_ARG(3, "index column index");
          TCL_STRING_ARG(2, index);
          TCL_INT_RESULT(Index::fromStr(index).x);
        }
        break;
    }

    return JIM_OK;
  }
}

