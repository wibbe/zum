
#include "Index.h"
#include "Tcl.h"
#include "Document.h"

#include <cmath>


Str Index::rowToStr(int row)
{
  return Str::fromInt(row + 1);
}

Str Index::columnToStr(int col)
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

  Str result;
  while (power >= 0)
  {
    const int value = std::pow(26, power);
    if (value > col)
    {
      power--;

      if (power < 0)
        result.append('A');
    }
    else
    {
      const int division = col / value;
      const int remainder = col % value;

      result.append('A' + division);

      col = remainder;
      power--;
    }
  }

  return result;
}

Str Index::toStr() const
{
  return columnToStr(x).append(rowToStr(y));
}

Index Index::fromStr(Str const& str)
{
  Index idx(0, 0);

  if (str.empty())
    return idx;

  int i = 0;
  while ((i < str.size()) && isUpperAlpha(str[i]))
  {
    idx.x *= 26;
    idx.x += str[i] - 'A';
    i++;
  }

  while ((i < str.size()) && isDigit(str[i]))
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
  TCL_PROC(index)
  {
    TCL_CHECK_ARG_OLDS(2, 1000, "index subcommand ?argument ...?");

    const uint32_t command = args[1].hash();
    switch (command)
    {
      case kNew:
        {
          TCL_CHECK_ARG_OLD(4, "index new column row");
          return resultStr(Index(args[2].toInt(), args[3].toInt()).toStr());
        }
        break;

      case kRow:
        {
          TCL_CHECK_ARG_OLD(3, "index row index");
          return resultInt(Index::fromStr(args[2]).y);
        }
        break;

      case kColumn:
        {
          TCL_CHECK_ARG_OLD(3, "index column index");
          return resultInt(Index::fromStr(args[2]).x);
        }
        break;

      default:
        break;
    }

    TCL_OK();
  }
}

