
#include "Expression.h"
#include "Tokenizer.h"
#include "Document.h"
#include "Editor.h"
#include "Log.h"
#include "Tcl.h"

#include <unordered_map>
#include <cmath>

#include "ExpressionFunctions.h"

struct FuncDef;

typedef bool EvalFunction(std::vector<Expr> & valueStack);
typedef bool StrFunction(FuncDef const* func, std::vector<std::tuple<int, std::string>> & args);

bool opToString(FuncDef const* func, std::vector<std::tuple<int, std::string>> & args);
bool funcToString(FuncDef const* func, std::vector<std::tuple<int, std::string>> & args);

struct FuncDef
{
  FuncDef(int precedence, int argCount, const char * name, EvalFunction * evalFunc)
    : precedence_(precedence),
      argCount_(argCount),
      name_(name),
      evalFunc_(evalFunc),
      strFunc_(precedence == -1 ? funcToString : opToString)
  { }

  int precedence_ = -1;
  int argCount_ = 0;
  const char * name_;
  EvalFunction * evalFunc_= nullptr;
  StrFunction * strFunc_ = nullptr;
};


static const std::unordered_map<std::string, FuncDef> functionDefinitions_ = {
  { "*", FuncDef(4, 2, "*", opMultiply) },
  { "/", FuncDef(3, 2, "/", opDivide) },
  { "+", FuncDef(1, 2, "+", opAdd) },
  { "-", FuncDef(1, 2, "-", opSubtract) },

  { "SUM", FuncDef(-1, 1, "SUM", funcSum) },
  { "MIN", FuncDef(-1, 1, "MIN", funcMin) },
  { "MAX", FuncDef(-1, 1, "MAX", funcMax) },
  { "ABS", FuncDef(-1, 1, "ABS", funcAbs) },
  { "COS", FuncDef(-1, 1, "COS", funcCos) },
  { "SIN", FuncDef(-1, 1, "SIN", funcSin) },
};

static const int MAX_PRECEDENCE = 99999;

bool opToString(FuncDef const* func, std::vector<std::tuple<int, std::string>> & args)
{
  if (func->argCount_ != 2 || args.size() < 2)
    return false;

  int aPrecedence, bPrecedence;
  std::string aValue, bValue;
  std::tie(bPrecedence, bValue) = args.back();
  args.pop_back();

  std::tie(aPrecedence, aValue) = args.back();
  args.pop_back();

  if (aPrecedence < func->precedence_)
    aValue = "(" + aValue + ")";

  if (bPrecedence < func->precedence_)
    bValue = "(" + bValue + ")";

  args.push_back(std::make_tuple(func->precedence_, aValue + " " + func->name_ + " " + bValue));
  return true;
}

bool funcToString(FuncDef const* func, std::vector<std::tuple<int, std::string>> & args)
{
  if (args.size() < func->argCount_)
    return false;

  std::string result = func->name_ + std::string("(");

  for (int i = 0; i < func->argCount_; ++i)
  {
    result += std::get<1>(args.back());
    args.pop_back();

    if (i < (func->argCount_ - 1))
      result += ", ";
  }

  result += ")";
  args.push_back(std::make_tuple(MAX_PRECEDENCE, result));

  return true;
}

std::string exprToString(std::vector<Expr> const& expression)
{
  std::vector<std::tuple<int, std::string>> result;

  for (auto const& expr : expression)
  {
    if (expr.type_ == Expr::Function)
    {
      expr.func_->strFunc_(expr.func_, result);
    }
    else
    {
      result.push_back(std::make_tuple(MAX_PRECEDENCE, expr.toStr()));
    }
  }

  return std::get<1>(result.front());
}

std::string Expr::toStr() const
{
  switch (type_)
  {
    case Expr::Constant:
      return str::fromDouble(constant_);

    case Expr::Function:
      return func_->name_;

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

static void printExpr(std::vector<Expr> const& output)
{
  const std::string result = "Output: " + exprToString(output);

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
          if (!expr.func_->evalFunc_(valueStack))
            return 0.0;
        }
        break;
    }
  }

  if (valueStack.size() != 1)
  {
    logError("error while calculating expression '", exprToString(expression));
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
