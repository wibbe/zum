
#pragma once

#include <functional>
#include "Str.h"

class Index
{
  public:
    Index() { }
    Index(int x_, int y_) : x(x_), y(y_) { }

    bool operator == (Index const& other) const
    {
      return x == other.x && y == other.y;
    }

    std::string toStr() const;

    static Index fromStr(std::string const& str);

    static std::string rowToStr(int row);
    static std::string columnToStr(int col);

    static int strToColumn(std::string const& str);

  public:
    int x = -1;
    int y = -1;
};

namespace std
{
  template<>
  struct hash<Index>
  {
    typedef Index argument_type;
    typedef std::size_t result_type;

    result_type operator()(argument_type const& s) const
    {
      result_type const h1(std::hash<int>()(s.x));
      result_type const h2(std::hash<int>()(s.y));
      return h1 ^ (h2 << 1);
    }
  };
}
