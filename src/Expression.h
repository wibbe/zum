
#pragma once

#include "Types.h"
#include "Str.h"

#include <memory>


class Expr
{
  public:

    virtual ~Expr()
    { }

    virtual Str toLua() const = 0;
    virtual Str toText() const = 0;

};

typedef std::unique_ptr<Expr> ExprPtr;

class Constant : public Expr
{
  public:
    Constant(Str const& value);

    Str toLua() const override;
    Str toText() const override;

  private:
    Str value_;
};

class Operator : public Expr
{
  public:
    Operator(ExprPtr leftSide, ExprPtr rightSide, Str::char_type op);

    Str toLua() const override;
    Str toText() const override;

  private:
    ExprPtr leftSide_;
    ExprPtr rightSide_;
    Str::char_type op_;
};

bool parseExpression(Str const& source, std::vector<Expr> & expr);

double evaluate(std::vector<Expr> const& expr);
