
#pragma once

#include "Index.h"

#include <string>

enum class ExprType
{
  Constant,
  Operator,
  Function,
  Cell
};

struct Expr
{
  Expr() { }
  Expr(double value) : type_(ExprType::Constant), constant_(value) { }
  Expr(uint32_t id, ExprType type) : type_(type), id_(id) { }
  Expr(Index const& idx) : type_(ExprType::Cell), index_(idx) { }

  ExprType type_ = ExprType::Constant;

  union {
    double constant_;
    uint32_t id_;
  };

  Index index_;
};

std::vector<Expr> parseExpression(std::string const& source);

double evaluate(std::vector<Expr> const& expr);
