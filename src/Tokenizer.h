
#pragma once

#include "Str.h"

enum class Token
{
  Number,
  Identifier,
  LeftParenthesis,
  RightParenthesis,
  Comma,
  EndOfFile,
  Error
};

class Tokenizer
{
  public:
    Tokenizer(Str const& str);

    Token next();
    Str const& value() const { return value_; }

  private:
    void eatWhitespace();
    Token parseNumber();
    Token parseIdentifier();

    Str::char_type current() const { return stream_[pos_]; }
    Str::char_type peak() const { return pos_ < (stream_.size() - 1) ? stream_[pos_ + 1] : 0; }
    bool eof() const { return pos_ >= stream_.size(); }

    bool step()
    {
      ++pos_;
      return eof();
    }

  private:
    Str stream_;
    uint32_t pos_;
    Str value_;
};