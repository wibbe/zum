
#pragma once

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
    explicit Str(char_type ch);
    Str(Str const& str);
    Str(Str && str);

    Str & operator = (Str const& copy);
    char_type operator [] (uint32_t idx) const { return data_[idx]; }

    void set(const char * str);
    void set(Str const& str);
    Str & clear();

    Str & append(Str const& str);
    Str & append(char_type ch);

    char_type front() const { return data_.front(); }
    char_type back() const { return data_.back(); }

    void insert(uint32_t pos, char_type ch);
    void erase(uint32_t pos);

    bool starts_with(Str const& str) const;
    bool equals(Str const& str) const;

    int size() const { return data_.size(); }
    bool empty() const { return data_.size() == 0; }

    std::vector<Str> split(char_type delimiter, bool keepDelimiter = false) const;

    int findChar(char_type ch) const;

    iterator begin() { return data_.begin(); }
    iterator end() { return data_.end(); }
    const_iterator begin() const { return data_.begin(); }
    const_iterator end() const { return data_.end(); }

    void pop_back();
    void pop_front(uint32_t count = 1);

    Str toUpper() const;
    Str toLower() const;

    Str stripWhitespace() const;
    void eatWhitespaceFront();

    int toInt() const;
    double toDouble() const;

    uint32_t hash() const;

    std::string utf8() const;

    static Str format(const char * fmt, ...);

    static Str fromInt(long long int value);
    static Str fromDouble(double value);

    static uint32_t hash(const char * str);

  private:
    std::vector<char_type> data_;
};

inline bool isDigit(Str::char_type ch)
{
  return (ch >= '0' && ch <= '9');
}

inline bool isWhitespace(Str::char_type ch)
{
  return (ch == ' ' || ch == '\t' || ch == '\n');
}

inline bool isAlpha(Str::char_type ch)
{
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

inline bool isUpperAlpha(Str::char_type ch)
{
  return (ch >= 'A' && ch <= 'Z');
}

inline bool isLowerAlpha(Str::char_type ch)
{
  return (ch >= 'a' && ch <= 'z');
}


inline bool isOperator(Str::char_type ch)
{
  switch (ch)
  {
    case '-':
    case '+':
    case '*':
    case '/':
    case '%':
    case ':':
      return true;

    default:
      return false;
  }
}

