
#include "Expression.h"
#include "Tokenizer.h"
#include "Document.h"
#include "Editor.h"
#include "Log.h"
#include "Tcl.h"

#include <unordered_map>

#include "ExpressionFunctions.h"

typedef bool EvalFunction(std::vector<Expr> & valueStack);

struct FuncDef
{
  FuncDef(int precedence, const char * name, EvalFunction * func)
    : precedence_(precedence),
      name_(name),
      func_(func)
  { }

  int precedence_ = -1;
  const char * name_;
  EvalFunction * func_= nullptr;
};

static const std::unordered_map<std::string, FuncDef> functionDefinitions_ = {
  { "*", FuncDef(4, "*", opMultiply) },
  { "/", FuncDef(3, "/", opDivide) },
  { "+", FuncDef(1, "+", opAdd) },
  { "-", FuncDef(1, "-", opSubtract) },

  { "SUM", FuncDef(-1, "SUM", funcSum) },
  { "MIN", FuncDef(-1, "MIN", funcMin) },
  { "MAX", FuncDef(-1, "MAX", funcMax) },
  { "ABS", FuncDef(-1, "ABS", funcAbs) },
  { "COS", FuncDef(-1, "COS", funcCos) },
  { "SIN", FuncDef(-1, "SIN", funcSin) },
};

std::string Expr::toStr() const
{
  switch (type_)
  {
    case Expr::Constant:
      return str::fromDouble(constant_);

    case Expr::Function:
      return "Function()";

    case Expr::Cell:
      return startIndex_.toStr();

    case Expr::Range:
      return startIndex_.toStr() + ":" + endIndex_.toStr();
  }
}

double Expr::toDouble() const
{
  switch (type_)
  {
    case Expr::Constant:
      return constant_;

    case Expr::Cell:
      return doc::getCellValue(startIndex_);

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

std::vector<Expr> parseExpression(std::string const& source)
{
  std::vector<Expr> output;

  std::vector<std::tuple<Token, std::string>> operatorStack;
  operatorStack.reserve(10);

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

      case Token::Range:
        {
          const std::size_t pos = tokenizer.value().find_first_of(":");
          if (pos == std::string::npos)
          {
            logError("error in expression '", source, "' - malformated range '", tokenizer.value(), "'");
            return {};
          }

          const std::string start = tokenizer.value().substr(0, pos);
          const std::string end = tokenizer.value().substr(pos + 1);

          output.push_back(Expr(Index::fromStr(start), Index::fromStr(end)));
        }
        break;

      case Token::Operator:
        {
          FuncDef const& tokenDef = functionDefinitions_.at(tokenizer.value());

          while (!operatorStack.empty())
          {
            Token opToken;
            std::string opValue;
            std::tie(opToken, opValue) = operatorStack.back();

            if (opToken != Token::Operator)
              break;

            FuncDef const& stackDef = functionDefinitions_.at(opValue);

            if (tokenDef.precedence_ <= stackDef.precedence_)
            {
              output.push_back(Expr(&stackDef));
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
          if (functionDefinitions_.count(tokenizer.value()) == 1)
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

            FuncDef const& stackDef = functionDefinitions_.at(opValue);
            output.push_back(Expr(&stackDef));
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
              FuncDef const& stackDef = functionDefinitions_.at(opValue);
              output.push_back(Expr(&stackDef));

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
              case Token::Identifier:
                output.push_back(Expr(&functionDefinitions_.at(opValue)));
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
      case Token::Identifier:
        output.push_back(Expr(&functionDefinitions_.at(opValue)));
        break;

      case Token::LeftParenthesis:
        logError("parse error in expression '", source, "' - missing closing parenthesis");
        return {};

      default:
        logError("error in expression '", source, "' - did not expect ", opValue);
        return {};
    }
  }

  return output;
}

double evaluate(std::vector<Expr> const& expression)
{
  std::vector<Expr> valueStack;
  valueStack.reserve(expression.size() / 2);

  for (auto const& expr : expression)
  {
    switch (expr.type_)
    {
      case Expr::Constant:
      case Expr::Cell:
      case Expr::Range:
        valueStack.push_back(expr);
        break;

      case Expr::Function:
        {
          if (!expr.func_->func_(valueStack))
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


TCL_FUNC(calculate, "string ?string ...?", "Evaluates the given expression in the same way a cell that starts with = is evaluated")
{
  TCL_CHECK_ARGS(2, 1000);

  std::string expressionString;
  for (uint32_t i = 1; i < argc; ++i)
    expressionString += Jim_String(argv[i]) + std::string(" ");

  const std::vector<Expr> expr = parseExpression(expressionString);

  if (expr.empty())
    return JIM_ERR;

  doc::evaluateDocument();
  const double result = evaluate(expr);

  TCL_DOUBLE_RESULT(result);
}
