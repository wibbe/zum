
#include "Expression.h"
#include "Tokenizer.h"
#include "Document.h"
#include "Editor.h"
#include "Log.h"
#include "Tcl.h"

#include <unordered_map>
#include <stack>

enum class Function
{
  Sum,
  Min,
  Max
};

enum class Operator
{
  Range,
  Multiply,
  Divide,
  Add,
  Subtract
};

static const std::unordered_map<std::string, std::tuple<uint32_t, Operator>> operatorTable_ = {
  { ":",  std::make_tuple(5, Operator::Range) },
  { "*",  std::make_tuple(4, Operator::Multiply) },
  { "/",  std::make_tuple(3, Operator::Divide) },
  { "+",  std::make_tuple(1, Operator::Add) },
  { "-",  std::make_tuple(1, Operator::Subtract) }
};

static const std::unordered_map<std::string, Function> functionTable_ = {
  { "SUM", Function::Sum },
  { "MIN", Function::Min },
  { "MAX", Function::Max }
};


std::string Expr::toStr() const
{
  switch (type_)
  {
    case Expr::Constant:
      return str::fromDouble(constant_);

    case Expr::Operator:
      return "Operator(" + str::fromInt(id_) + ")";

    case Expr::Function:
      return "Function(" + str::fromInt(id_) + ")";

    case Expr::Cell:
      return index_.toStr();
  }
}

double Expr::toDouble() const
{
  switch (type_)
  {
    case Expr::Constant:
      return constant_;

    case Expr::Cell:
      return doc::getCellValue(index_);

    default:
      return 0.0;
  }
}

static std::string stringExpr(std::vector<Expr> const& output)
{
  std::string result;
  for (auto const& expr : output)
    result += expr.toStr() + " ";
  return result;
}

static void printExpr(std::vector<Expr> const& output)
{
  const std::string result = "Output: " + stringExpr(output);

  logInfo(result);
  flashMessage(result);
}

static std::string tokenName(Token token)
{
  switch (token)
  {
    case Token::Number:
      return "Number";

    case Token::Cell:
      return "Cell";

    case Token::Operator:
      return "Operator";

    case Token::Identifier:
      return "Identifier";

    case Token::LeftParenthesis:
      return "LeftParenthesis";

    case Token::RightParenthesis:
      return "RightParenthesis";

    case Token::Comma:
      return "Comma";
  }

  return "Unknown";
}

static void printStack(std::vector<std::tuple<Token, std::string>> const& stack)
{
  std::string result = "Stack: ";
  for (auto const& value : stack)
    result += tokenName(std::get<0>(value)) + "(" + std::get<1>(value) + ") ";

  logInfo(result);
  flashMessage(result);
}


std::vector<Expr> parseExpression(std::string const& source)
{
  std::vector<Expr> output;

  std::vector<std::tuple<Token, std::string>> operatorStack;

  Tokenizer tokenizer(source);

  bool cont = true;
  while (cont && !tokenizer.eof())
  {
    Token token = tokenizer.next();

    switch (token)
    {
      case Token::Number:
        output.push_back(Expr(std::stof(tokenizer.value().c_str())));
        break;

      case Token::Cell:
        output.push_back(Expr(Index::fromStr(tokenizer.value())));
        break;

      case Token::Operator:
        {
          const int tokenPrecedence = std::get<0>(operatorTable_.at(tokenizer.value()));

          while (!operatorStack.empty())
          {
            Token opToken;
            std::string opValue;
            std::tie(opToken, opValue) = operatorStack.back();

            if (opToken != Token::Operator)
              break;

            int stackPrecedence;
            Operator stackOperator;
            std::tie(stackPrecedence, stackOperator) = operatorTable_.at(opValue);

            if (tokenPrecedence <= stackPrecedence)
            {
              output.push_back(Expr(Expr::Operator, (uint32_t)stackOperator));
              operatorStack.pop_back();
            }
            else
              break;
          }

          operatorStack.push_back(std::make_tuple(Token::Operator, tokenizer.value()));
        }
        break;

      case Token::Identifier:
        {
          if (functionTable_.count(tokenizer.value()) == 1)
          {
            operatorStack.push_back(std::make_tuple(Token::Identifier, tokenizer.value()));
          }
          else
          {
            logError("unknown function in expression '", source, "' - ", tokenizer.value());
            return {};
          }
        }
        break;

      case Token::LeftParenthesis:
        operatorStack.push_back(std::make_tuple(Token::LeftParenthesis, ""));
        break;

      case Token::RightParenthesis:
        {
          bool hasLeftParenthesis = false;

          while (!operatorStack.empty())
          {
            Token opToken;
            std::string opValue;
            std::tie(opToken, opValue) = operatorStack.back();
            operatorStack.pop_back();

            if (opToken == Token::LeftParenthesis)
            {
              hasLeftParenthesis = true;
              break;
            }

            if (opToken != Token::Operator)
            {
              logError("parse error in expression '", source, "' - expected operator");
              return {};
            }

            output.push_back(Expr(Expr::Operator, (uint32_t)std::get<1>(operatorTable_.at(opValue))));
          }

          if (!hasLeftParenthesis)
          {
            logError("parse error in expression '", source, "' - missing start parenthesis");
            return {};
          }

          if (!operatorStack.empty())
          {
            Token opToken;
            std::string opValue;
            std::tie(opToken, opValue) = operatorStack.back();

            if (opToken == Token::Identifier)
            {
              output.push_back(Expr(Expr::Function, (uint32_t)functionTable_.at(opValue)));
              operatorStack.pop_back();
            }
          }
        }
        break;

      case Token::Comma:
        {
          bool foundLeftParenthesis = false;

          while (!operatorStack.empty())
          {
            Token opToken;
            std::string opValue;
            std::tie(opToken, opValue) = operatorStack.back();

            if (opToken == Token::LeftParenthesis)
            {
              foundLeftParenthesis = true;
              break;
            }

            operatorStack.pop_back();

            switch (opToken)
            {
              case Token::Operator:
                output.push_back(Expr(Expr::Operator, (uint32_t)std::get<1>(operatorTable_.at(opValue))));
                break;

              case Token::Identifier:
                output.push_back(Expr(Expr::Function, (uint32_t)functionTable_.at(opValue)));
                break;

              default:
                logError("error in expression '", source, "' - did not expect ", opValue);
                return {};
            }
          }

          if (!foundLeftParenthesis)
          {
            logError("error in expression '", source, "' - missplaced parenthesis or comma");
            return {};
          }
        }
        break;

      case Token::EndOfFile:
        cont = false;
        break;

      case Token::Error:
        logError("parse error in expression '", source, "' - ", tokenizer.value());
        return {};
    };
  }

  while (!operatorStack.empty())
  {
    Token opToken;
    std::string opValue;
    std::tie(opToken, opValue) = operatorStack.back();
    operatorStack.pop_back();

    switch (opToken)
    {
      case Token::Operator:
        output.push_back(Expr(Expr::Operator, (uint32_t)std::get<1>(operatorTable_.at(opValue))));
        break;

      case Token::Identifier:
        output.push_back(Expr(Expr::Function, (uint32_t)functionTable_.at(opValue)));
        break;

      case Token::LeftParenthesis:
        logError("parse error in expression '", source, "' - missing closing parenthesis");
        return {};

      default:
        logError("error in expression '", source, "' - did not expect ", opValue);
        return {};
    }
  }

  printExpr(output);

  return output;
}

Expr popExpr(std::vector<Expr> & valueStack)
{
  Expr expr = valueStack.back();
  valueStack.pop_back();
  return expr;
}

bool execOperator(std::vector<Expr> & valueStack, Operator op)
{
  const double b = popExpr(valueStack).toDouble();
  const double a = popExpr(valueStack).toDouble();

  switch (op)
  {
    case Operator::Multiply:
      valueStack.push_back(a * b);
      break;

    case Operator::Divide:
      valueStack.push_back(a / b);
      break;

    case Operator::Add:
      valueStack.push_back(a + b);
      break;

    case Operator::Subtract:
      valueStack.push_back(a - b);
      break;

    default:
      return false;
  }

  return true;
}

bool execFunction(std::vector<Expr> & valueStack, Function func)
{
  switch (func)
  {
    case Function::Sum:
      {
        if (valueStack.size() < 3)
        {
          logError("wrong number of arguments to SUM(range)");
          return false;
        }

        const Expr rangeOp = popExpr(valueStack);
        if (rangeOp.type_ != Expr::Operator || rangeOp.id_ != (uint32_t)Operator::Range)
        {
          logError("sum function expected range argument");
          return false;
        }

        const Expr endExpr = popExpr(valueStack);
        const Expr startExpr = popExpr(valueStack);

        if (startExpr.type_ != Expr::Cell || endExpr.type_ != Expr::Cell)
        {
          logError("invalid range, two cell references expected");
          return false;
        }

        Index const& startIdx = startExpr.index_;
        Index const& endIdx = endExpr.index_;

        if (startIdx.x > endIdx.x || startIdx.y > endIdx.y)
        {
          logError("invalid range, row and column in start index ", startIdx.toStr(), " must be less than end index ", endIdx.toStr());
          return false;
        }

        double sum = 0.0;
        for (int y = startIdx.y; y <= endIdx.y; ++y)
          for (int x = startIdx.x; x <= endIdx.x; ++x)
            sum += doc::getCellValue(Index(x, y));

        valueStack.push_back(sum);
      }
      break;

    case Function::Min:
      {
        if (valueStack.size() < 2)
        {
          logError("wrong number of arguments to MIN(a, b)");
          return false;
        }

        const double b = popExpr(valueStack).toDouble();
        const double a = popExpr(valueStack).toDouble();

        valueStack.push_back(std::min(a, b));
      }
      break;

    case Function::Max:
      {
        if (valueStack.size() < 2)
        {
          logError("wrong number of arguments to MAX(a, b)");
          return false;
        }

        const double b = popExpr(valueStack).toDouble();
        const double a = popExpr(valueStack).toDouble();

        valueStack.push_back(std::max(a, b));
      }
      break;
  }

  return true;
}

double evaluate(std::vector<Expr> const& expression)
{
  std::vector<Expr> valueStack;
  valueStack.reserve(expression.size());

  for (auto const& expr : expression)
  {
    switch (expr.type_)
    {
      case Expr::Constant:
      case Expr::Cell:
        valueStack.push_back(expr);
        break;

      case Expr::Operator:
        {
          if (expr.id_ == (uint32_t)Operator::Range)
          {
            valueStack.push_back(expr);
          }
          else
          {
            if (!execOperator(valueStack, (Operator)expr.id_))
              return 0.0f;
          }
        }
        break;

      case Expr::Function:
        {
          if (!execFunction(valueStack, (Function)expr.id_))
            return 0.0;
        }
        break;
    }
  }

  if (valueStack.size() != 1)
  {
    logError("error while calculating expression '", stringExpr(expression));
    return 0.0;
  }

  return valueStack.front().toDouble();
}


TCL_FUNC(calculate, "string ?string ...?")
{
  TCL_CHECK_ARGS(2, 1000);

  std::string expressionString;
  for (uint32_t i = 1; i < argc; ++i)
    expressionString += Jim_String(argv[i]) + std::string(" ");

  const std::vector<Expr> expr = parseExpression(expressionString);

  if (expr.empty())
    return JIM_ERR;

  const double result = evaluate(expr);
  TCL_DOUBLE_RESULT(result);
}
