
#include "Expression.h"
#include "Tokenizer.h"

#include <unordered_map>
#include <stack>

std::vector<Expr> parseExpression(std::string const& source)
{
  std::vector<Expr> output;

  std::vector<std::pair<Token, std::string>> operatorStack;

  Tokenizer tokenizer(source);

  while (true)
  {
    Token token = tokenizer.next();

    switch (token)
    {
      case Token::Number:
        output.push_back(Expr(std::stof(tokenizer.value().c_str())));
        break;

      case Token::Cell:
        output.push_back(Expr(Index::fromStr(tokenizer.value())));
        break;
    };
  }

  return output;
}

double evaluate(std::vector<Expr> const& expr)
{
  return 0.0;
}
