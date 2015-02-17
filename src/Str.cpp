
#include "Str.h"

#include "termbox.h"
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>

Str Str::EMPTY;

Str::Str()
{ }

Str::Str(const char * str)
{
  set(str);
}

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

void Str::set(const char * str)
{
  data_.clear();
  const char * it = str;

  while (*it)
  {
    char_type ch;
    it += tb_utf8_char_to_unicode(&ch, it);
    data_.push_back(ch);
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

Str & Str::append(char_type ch)
{
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

bool Str::starts_with(Str const& str) const
{
  if (size() < str.size())
    return false;

  for (int i = 0; i < str.size(); ++i)
    if (data_[i] != str[i])
      return false;

  return true;
}

bool Str::equals(Str const& str) const
{
  if (size() != str.size())
    return false;

  for (int i = 0; i < size(); ++i)
    if (data_[i] != str[i])
      return false;

  return true;
}

void Str::pop_back()
{
  data_.pop_back();
}

void Str::pop_front(uint32_t count)
{
  for (int i = 0; i < (data_.size() - count); ++i)
    data_[i] = data_[i + count];

  for (; count > 0; --count)
    data_.pop_back();
}

std::vector<Str> Str::split(char_type delimiter) const
{
  std::vector<Str> result;
  Str part;

  for (auto ch : data_)
  {
    if (ch == delimiter)
    {
      if (!part.empty())
        result.push_back(part);
      part.clear();
    }
    else
      part.append(ch);
  }

  if (!part.empty())
    result.push_back(part);

  return result;
}

Str Str::stripWhitespace() const
{
  Str str(*this);

  while (!str.empty() && isWhitespace(str.front()))
    str.pop_front();

  while (!str.empty() && isWhitespace(str.back()))
    str.pop_back();

  return str;
}

void Str::eatWhitespaceFront()
{
  while (!empty() && isWhitespace(front()))
    pop_front();
}

int Str::toInt() const
{
  int result = 0;
  int sign = 1;

  for (uint32_t i = 0, len = size(); i < len; ++i)
  {
    Str::char_type ch = data_[i];

    if (ch == '-' && i == 0)
      sign = -1;

    if (isDigit(ch))
      result = (result * 10) + (ch - '0');
    else
      break;
  }

  return result * sign;
}

std::string Str::utf8() const
{
  std::string result;

  for (auto const& ch : data_)
  {
    char str[7];
    char * it = str + tb_utf8_unicode_to_char(str, ch);
    *it = '\0';
    result += str;
  }

  return result;
}

Str Str::format(const char * fmt, ...)
{
  static const int LEN = 1024;
  char buffer[LEN];

  va_list argp;
  va_start(argp, fmt);
  vsnprintf(buffer, LEN, fmt, argp);
  va_end(argp);

  return Str(buffer);
}

Str Str::fromInt(int value)
{
  Str str;

  if (value < 0)
  {
    str.data_.push_back('-');
    value *= -1;
  }

  while (value > 0)
  {
    str.data_.push_back('0' + (value % 10));
    value /= 10;
  }

  str.data_.push_back('0' + (value % 10));
  return str;
}