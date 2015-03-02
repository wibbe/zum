
#define CHECK_ARG(count, func) \
  if (valueStack.size() < count) \
  { \
    logError("wrong number of arguments in ", func); \
    return false; \
  }

#define RESULT(value) \
  valueStack.push_back(value); \
  return true

Expr popExpr(std::vector<Expr> & valueStack)
{
  Expr expr = valueStack.back();
  valueStack.pop_back();
  return expr;
}

static bool opMultiply(std::vector<Expr> & valueStack)
{
  const double b = popExpr(valueStack).toDouble();
  const double a = popExpr(valueStack).toDouble();
  RESULT(a * b);
}

static bool opDivide(std::vector<Expr> & valueStack)
{
  const double b = popExpr(valueStack).toDouble();
  const double a = popExpr(valueStack).toDouble();
  RESULT(a / b);
}

static bool opAdd(std::vector<Expr> & valueStack)
{
  const double b = popExpr(valueStack).toDouble();
  const double a = popExpr(valueStack).toDouble();
  RESULT(a + b);
}

static bool opSubtract(std::vector<Expr> & valueStack)
{
  const double b = popExpr(valueStack).toDouble();
  const double a = popExpr(valueStack).toDouble();
  RESULT(a - b);
}

static bool funcSum(std::vector<Expr> & valueStack)
{
  CHECK_ARG(1, "SUM(range)");

  const Expr range = popExpr(valueStack);
  if (range.type_ != Expr::Range)
  {
    logError("sum function expected range argument");
    return false;
  }

  Index const& startIdx = range.startIndex_;
  Index const& endIdx = range.endIndex_;

  if (startIdx.x > endIdx.x || startIdx.y > endIdx.y)
  {
    logError("invalid range, row and column in start index ", startIdx.toStr(), " must be less than end index ", endIdx.toStr());
    return false;
  }

  double sum = 0.0;
  for (int y = startIdx.y; y <= endIdx.y; ++y)
    for (int x = startIdx.x; x <= endIdx.x; ++x)
      sum += doc::getCellValue(Index(x, y));

  RESULT(sum);
}

static bool funcMin(std::vector<Expr> & valueStack)
{
  CHECK_ARG(2, "MIN(a, b)");

  const double b = popExpr(valueStack).toDouble();
  const double a = popExpr(valueStack).toDouble();

  RESULT(std::max(a, b));
}

static bool funcMax(std::vector<Expr> & valueStack)
{
  CHECK_ARG(2, "MAX(a, b)");

  const double b = popExpr(valueStack).toDouble();
  const double a = popExpr(valueStack).toDouble();

  RESULT(std::min(a, b));
}

static bool funcAbs(std::vector<Expr> & valueStack)
{
  CHECK_ARG(1, "ABS(value)");
  const double value = popExpr(valueStack).toDouble();

  RESULT(std::abs(value));
}

static bool funcCos(std::vector<Expr> & valueStack)
{
  CHECK_ARG(1, "COS(value)");
  const double value = popExpr(valueStack).toDouble();

  RESULT(std::cos(value));
}

static bool funcSin(std::vector<Expr> & valueStack)
{
  CHECK_ARG(1, "SIN(value)");
  const double value = popExpr(valueStack).toDouble();

  RESULT(std::sin(value));
}
