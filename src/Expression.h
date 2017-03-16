
#pragma once

#include "Index.h"

#include <string>

struct FuncDef;

struct Expr
{
  enum Type
  {
    Constant,
    Cell,
    Range,
    Function,
  };

  Expr() { }
  Expr(double value) : type_(Type::Constant), constant_(value) { }
  Expr(const FuncDef * func) : type_(Function), func_(func) { }
  Expr(Index const& idx) : type_(Type::Cell), startIndex_(idx) { }
  Expr(Index const& start, Index const& end) : type_(Range), startIndex_(start), endIndex_(end) { }

  std::string toStr() const;
  double toDouble() const;

  Type type_ = Type::Constant;

  union {
    double constant_;
    const FuncDef * func_;
  };

  Index startIndex_;
  Index endIndex_;
};


std::vector<Expr> parseExpression(std::string const& source);
double evaluate(std::vector<Expr> const& expr);
std::string exprToString(std::vector<Expr> const& expr);


namespace exp {

  class Expr
  {
    public:
      virtual double evaluate() const = 0;
  };

  class Constant : Expr
  {
    public:
      double evaluate() const { return value_; }

    private:
      double value_ = 0.0;
  };

  class Cell : Expr
  {
    public:
      virtual std::vector<Index> indices() const = 0;
  };

  class CellRef : Cell
  {
    public:
  };

  class CellRange : Cell
  {
    public:
  };

  class Function : Expr
  {
    public:
      Function(std::vector<std::unique_ptr<Expr>> && args)
        : arguments_(std::move(args))
      { }

    private:
      std::vector<std::unique_ptr<Expr>> arguments_;
  };

  class BinaryOp : Expr
  {
    public:

    private:
      std::unique_ptr<Expr> leftHandSide_;
      std::unique_ptr<Expr> rightHandSide_;
  };
}