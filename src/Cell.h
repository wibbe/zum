
#pragma once

#include "Str.h"
#include "Expression.h"

struct Cell
{
  std::string text = "";
  std::string display = "";
  double value = 0.0;
  bool hasExpression = false;
  std::vector<Expr> expression;
};
