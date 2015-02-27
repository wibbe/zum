
#pragma once

#include "Index.h"

#include <string>

struct Expr;
typedef bool ExprCallback(std::vector<Expr> & valueStack);

struct Expr
{
  enum Type
  {
    Constant,
    Operator,
    Function,
    Cell
  };

  Expr() { }
  Expr(double value) : type_(Type::Constant), constant_(value) { }
  Expr(Type type, uint32_t id) : type_(type), id_(id) { }
  Expr(Index const& idx) : type_(Type::Cell), index_(idx) { }

  std::string toStr() const;
  double toDouble() const;

  Type type_ = Type::Constant;

  union {
    double constant_;
    uint32_t id_;
    ExprCallback * func_;
  };

  Index index_;
};

std::vector<Expr> parseExpression(std::string const& source);

double evaluate(std::vector<Expr> const& expr);
