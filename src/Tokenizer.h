
#pragma once

#include <string>

enum class Token
{
  Number,
  Cell,
  Operator,
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
    Tokenizer(std::string const& str);

    Token next();
    std::string const& value() const { return value_; }

    bool eof() const { return pos_ >= stream_.size(); }

  private:
    void eatWhitespace();
    Token parseNumber();
    Token parseIdentifier();
    bool parseCell();

    char current() const { return stream_[pos_]; }
    char peak() const { return pos_ < (stream_.size() - 1) ? stream_[pos_ + 1] : 0; }

    bool step()
    {
      ++pos_;
      return eof();
    }

  private:
    std::string stream_;
    std::string value_;
    uint32_t pos_;
};