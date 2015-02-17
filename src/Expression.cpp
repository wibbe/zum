
#include "Expression.h"
#include "Tokenizer.h"

#include <unordered_map>
#include <stack>

Constant::Constant(Str const& value)
  : value_(value)
{ }

Str Constant::toLua() const
{
  return value_;
}

Str Constant::toText() const
{
  return value_;
}

Operator::Operator(ExprPtr leftSide, ExprPtr rightSide, Str::char_type op)
  : leftSide_(std::move(leftSide)),
    rightSide_(std::move(rightSide_)),
    op_(op)
{ }

Str Operator::toLua() const
{
  Str result;

  if (op_ == ':')
  {

  }
  else
  {
    result.append(leftSide_->toLua())
          .append(op_)
          .append(rightSide_->toLua());
  }

  return result;
}

Str Operator::toText() const
{
  Str result;
  result.append(leftSide_->toText())
        .append(op_)
        .append(rightSide_->toText());

  return result;
}



bool parseExpression(Str const& source, std::vector<Expr> & expr)
{
  Tokenizer tok(source);
  std::stack<Expr> valueStack;

  return false;
}

double evaluate(std::vector<Expr> const& expr)
{
  return 0.0;
}
