
#pragma once

#include "Str.h"
#include "Expression.h"

struct Cell
{
  std::string text;
  std::string display;
  double value;
  std::vector<Expr> expression;
};
