
#include "Expression.h"

#include <unordered_map>
#include <stack>

struct OperatorDef
{
  Str::char_type character_;
  Operator id_;
  int precedence_;
  std::function<double (std::vector<Expr> const& expr, uint32_t pos)> function_;
};

struct FunctionDef
{
  Str name_;
  Function id_;
  std::function<double (std::vector<Expr> const& expr, uint32_t pos)> function_;
};


bool parseExpression(Str const& source, std::vector<Expr> & expr)
{
  std::stack<Expr> valueStack;

  return false;
}

double evaluate(std::vector<Expr> const& expr)
{
  return 0.0;
}
