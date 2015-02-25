
#include "Tokenizer.h"

#include <cctype>

inline bool isOperator(char ch)
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

Tokenizer::Tokenizer(std::string const& stream)
  : stream_(stream),
    value_(),
    pos_(0)
{ }

Token Tokenizer::next()
{
  value_.clear();
  eatWhitespace();

  if (eof())
    return Token::EndOfFile;

  if (std::isdigit(current()))
  {
    return parseNumber();
  }
  else if (current() == '-' && std::isdigit(peak()))
  {
    // Eat the '-'' and then parse the number
    value_.append(1, '-');
    return parseNumber();
  }
  else if (current() == '(')
  {
    step();
    return Token::LeftParenthesis;
  }
  else if (current() == ')')
  {
    step();
    return Token::RightParenthesis;
  }
  else if (isOperator(current()) && (std::isspace(peak()) || std::isalpha(peak()) || std::isdigit(peak())))
  {
    value_.append(1, current());
    step();

    return Token::Operator;
  }
  else if (std::isalpha(current()))
  {
    return parseIdentifier();
  }

  // If we get here, we have encountered an error
  value_.append("Parse error - unknown character: ")
        .append(1, current());

  return Token::Error;
}

void Tokenizer::eatWhitespace()
{
  while (!eof() && std::isspace(current()))
    step();
}

Token Tokenizer::parseNumber()
{
  value_.append(1, current());
  step();

  while (!eof() && std::isdigit(current()))
  {
    value_.append(1, current());
    step();
  }

  if (current() == '.')
  {
    if (!std::isdigit(peak()))
    {
      const std::string number = value_;
      value_ = "Parse error - expected digit but got ";
      value_.append(1, peak())
            .append(" in number ")
            .append(number);
      Token::Error;
    }
    else
    {
      value_.append(1, current());
      step();

      while (!eof() && std::isdigit(current()))
      {
        value_.append(1, current());
        step();
      }
    }
  }

  return Token::Number;
}

Token Tokenizer::parseIdentifier()
{
  if (std::isupper(current()))
    if (parseCell())
      return Token::Cell;

  while (!eof() && (std::isalpha(current()) || std::isdigit(current()) || current() == '_'))
  {
    value_.append(1, current());
    step();
  }

  return Token::Identifier;
}

bool Tokenizer::parseCell()
{
  while (!eof() && std::isupper(current()))
  {
    value_.append(1, current());
    step();
  }

  if (!eof() && std::isdigit(current()))
  {
    while (!eof() && std::isdigit(current()))
    {
      value_.append(1, current());
      step();
    }

    return true;
  }

  return false;
}
