
#include "Str.h"
#include "MurmurHash.h"

#include "termbox.h"
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <ios>

namespace str {

  std::string fromInt(long long int value)
  {
    std::stringstream ss;
    ss << value;
    return ss.str();
  }

  std::string fromDouble(double value)
  {
    std::stringstream ss;
    ss << value;
    return ss.str();
  }

  std::string stripWhitespace(std::string const& str)
  {
    static const std::string WHITESPACES(" \t\f\v\n\r");

    std::size_t front = str.find_first_not_of(WHITESPACES);
    std::size_t back = str.find_last_not_of(WHITESPACES);

    front = (front == std::string::npos ? 0 : front);
    back = (back == std::string::npos ? str.size() : back);

    return str.substr(front, back - front);
  }

  uint32_t hash(std::string const& str)
  {
    return murmurHash(str.c_str(), str.size(), 0);
  }

  uint32_t toUTF32(std::string const& in, uint32_t * out, uint32_t outLen)
  {
    // Convert to uft32
    const char * it = in.c_str();
    uint32_t strLen = 0;

    while (*it && strLen < (outLen - 1))
    {
      //it += tb_utf8_char_to_unicode(&out[strLen], it);
      out[strLen] = *it;
      ++it;
      ++strLen;
    }
    out[strLen] = '\0';

    return strLen;
  }
}


Str Str::EMPTY;

Str::Str()
{ }

Str::Str(const char * str)
{
  set(str);
}

Str::Str(std::string const& str)
{
  set(str.c_str());
}

Str::Str(char_type ch, int count)
  : data_(count, ch)
{ }

Str::Str(Str const& str)
  : data_(str.begin(), str.end())
{ }

Str::Str(Str && str)
  : data_(std::move(str.data_))
{ }

Str & Str::operator = (Str const& copy)
{
  data_.clear();
  data_.reserve(copy.size());
  for (auto ch : copy)
    data_.push_back(ch);

  return *this;
}

Str & Str::operator = (std::string const& copy)
{
  set(copy.c_str());
  return *this;
}

void Str::set(const char * str)
{
  data_.clear();
  const char * it = str;

  while (*it)
  {
    data_.push_back(*it);
    //char_type ch;
    //it += tb_utf8_char_to_unicode(&ch, it);
    //data_.push_back(ch);
  }
}

void Str::set(Str const& str)
{
  data_.clear();
  data_.reserve(str.size());
  for (auto ch : str)
    data_.push_back(ch);
}

Str & Str::clear()
{
  data_.clear();
  return *this;
}

Str & Str::append(Str const& str)
{
  data_.reserve(size() + str.size());
  for (auto ch : str)
    data_.push_back(ch);
  return *this;
}

Str & Str::append(char_type ch, int count)
{
  for (; count > 0; --count)
    data_.push_back(ch);
  return *this;
}

void Str::insert(uint32_t pos, char_type ch)
{
  data_.insert(data_.begin() + pos, ch);
}

void Str::erase(uint32_t pos)
{
  data_.erase(data_.begin() + pos);
}

std::vector<Str> Str::split(char_type delimiter, bool keepDelimiter) const
{
  std::vector<Str> result;
  Str part;

  for (auto ch : data_)
  {
    if (ch == delimiter)
    {
      if (!part.empty())
        result.push_back(part);

      if (keepDelimiter)
      {
        if (!result.empty() && result.back().back() == delimiter)
          result.back().append(delimiter);
        else
          result.push_back(Str(delimiter));
      }

      part.clear();
    }
    else
      part.append(ch);
  }

  if (!part.empty())
    result.push_back(part);

  return result;
}

std::string Str::utf8() const
{
  std::string result;

  for (auto const& ch : data_)
  {
    //char str[7];
    //char * it = str + tb_utf8_unicode_to_char(str, ch);
    //*it = '\0';
    //result += str;
    result += (char)ch;
  }

  return result;
}
