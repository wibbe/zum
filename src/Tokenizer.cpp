
#include "Tokenizer.h"

Tokenizer::Tokenizer(Str const& stream)
  : stream_(stream),
    pos_(0),
    value_()
{ }

Token Tokenizer::next()
{
  value_.clear();
  eatWhitespace();

  if (eof())
    return Token::EndOfFile;

  if (current() >= '0' && current() <= '9')
  {
    return parseNumber();
  }
  else if (current() == '-')
  {
    if (peak() >= '0' && peak() <= '9')
      return parseNumber();
    else
      return parseIdentifier();
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
  else if ((current() >= 'a' && current() <= 'z') || (current() >= 'A' && current() <= 'Z'))
  {
    return parseIdentifier();
  }

  // If we get here, we have encountered an error
  value_.append(Str("Parse error - unknown character: "))
        .append(current());
  return Token::Error;
}

void Tokenizer::eatWhitespace()
{
  while (!eof() && (current() == ' ' || current() == '\t'))
    step();
}

Token Tokenizer::parseNumber()
{
  value_.append(current());


  return Token::Number;
}

Token Tokenizer::parseIdentifier()
{

}
