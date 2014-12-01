
#pragma once

#include "Types.h"
#include "Str.h"

enum class ExprType
{
  Value,
  CellRef,
  Operator,
  Function
};

enum class Operator
{
  Multiply,
  Divide,
  Reminder,
  Add,
  Subtract,
};

enum class Function
{
  Sum,
  Min,
  Max,
  Sin,
  Cos,
  Tan
};

struct Expr
{
  ExprType type_;

  union
  {
    double value_;
    Index cell_;
    Operator operator_;
    Function function_;
  } value_;
};

bool parseExpression(Str const& source, std::vector<Expr> & expr);

double evaluate(std::vector<Expr> const& expr);
