
#pragma once

#include <functional>
#include <vector>
#include <string>

class Str
{
  public:
    typedef uint32_t char_type;
    typedef std::vector<char_type>::iterator iterator;
    typedef std::vector<char_type>::const_iterator const_iterator;

    static Str EMPTY;

  public:
    Str();
    explicit Str(const char * str);
    Str(Str const& str);
    Str(Str && str);

    Str & operator = (Str const& copy);
    char_type operator [] (uint32_t idx) const { return data_[idx]; }

    void set(const char * str);
    void clear();

    Str & append(Str const& str);
    Str & append(char_type ch);

    void insert(uint32_t pos, char_type ch);
    void erase(uint32_t pos);

    int size() const { return data_.size(); }
    bool empty() const { return data_.size() == 0; }

    iterator begin() { return data_.begin(); }
    iterator end() { return data_.end(); }
    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.end(); }

    std::string utf8() const;

    static Str format(const char * fmt, ...);

  private:
    std::vector<char_type> data_;
};

struct Cell
{
  Str text;
};

struct Index
{
  Index() { }
  Index(int x_, int y_) : x(x_), y(y_) { }

  bool operator == (Index const& other) const
  {
    return x == other.x && y == other.y;
  }

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