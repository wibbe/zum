
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
