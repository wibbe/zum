
#include "Tcl.h"
#include "ScriptingLib.tcl.h"
#include "Editor.h"

#include <iostream>
#include <map>

namespace tcl {

  // -- Externs

  //double calculateExpr(std::string const& str);

  // -- Parser --

  enum Token
  {
    EndOfLine,
    EndOfFile,
    Separator,
    String,
    Variable,
    Escaped,
    Command,
    Append,
    Error
  };

  static const char * tokenAsReadable[] = {
    "EndOfLine",
    "EndOfFile",
    "Separator",
    "String",
    "Variable",
    "Escaped",
    "Command",
    "Append",
    "Error"
  };

  struct Parser
  {
    Parser(Str const& code)
      : code_(code),
        value_(),
        token_(EndOfLine),
        current_(code.empty() ? 0 : code[0]),
        pos_(0),
        insideString_(false)
    { }

    bool next();
    void inc() { if (pos_ < code_.size()) current_ = code_[++pos_]; }
    bool eof() const { return len() <= 0; }
    uint32_t len() const { return code_.size() - pos_; }

    Str code_;
    Str value_;
    Token token_;
    Str::char_type current_;
    uint32_t pos_;
    bool insideString_;
  };

  static bool eatWhitespace(Parser * p)
  {
    p->value_.clear();

    while (isWhitespace(p->current_) && !p->eof())
      p->inc();

    p->token_ = Separator;
    return true;
  }

  static bool parseBracet(Parser * p)
  {
    int level = 1;
    p->inc();
    p->value_.clear();

    while (!p->eof())
    {
      if (p->len() >= 2 && p->current_ == '\\')
      {
        p->inc();
        p->value_.append(p->current_);
      }
      else if (p->len() == 0 || p->current_ == '}')
      {
        level--;

        if (level == 0 || p->len() == 0)
        {
          if (!p->eof())
            p->inc();

          p->token_ = String;
          return true;
        }
      }
      else if (p->current_ == '{')
      {
        level++;
      }

      p->value_.append(p->current_);
      p->inc();
    }

    return true;
  }

  static bool parseComment(Parser * p)
  {
    while (!p->eof() && p->current_ != '\n')
      p->inc();
    return true;
  }

  static bool parseCommand(Parser * p)
  {
    int outerLevel = 1;
    int innerLevel = 0;

    p->inc();
    p->value_.clear();

    while (!p->eof())
    {
      if (p->current_ == '[' && innerLevel == 0)
      {
        outerLevel++;
      }
      else if (p->current_ == ']' && innerLevel == 0)
      {
        if (--outerLevel == 0)
          break;
      }
      else if (p->current_ == '\\')
      {
        p->inc();
      }
      else if (p->current_ == '{')
      {
        innerLevel++;
      }
      else if (p->current_ == '}')
      {
        if (innerLevel > 0)
          innerLevel--;
      }

      p->value_.append(p->current_);
      p->inc();
    }

    if (p->current_ == ']')
      p->inc();

    p->token_ = Command;
    return true;
  }

  static bool parseEndOfLine(Parser * p)
  {
    while (isWhitespace(p->current_) || p->current_ == ';')
      p->inc();

    p->token_ = EndOfLine;
    return true;
  }

  static bool parseVariable(Parser * p)
  {
    p->value_.clear();
    p->token_ = Variable;
    p->inc(); // eat $

    while (isAlpha(p->current_) || isDigit(p->current_))
    {
      p->value_.append(p->current_);
      p->inc();
    }

    if (p->value_.empty()) // This was just a single character string "$"
    {
      p->value_.set("$");
      p->token_ = String;
    }

    return true;
  }

  static bool parseString(Parser * p)
  {
    const bool newWord = (p->token_ == Separator || p->token_ == EndOfLine || p->token_ == String);

    if (newWord && p->current_ == '{')
      return parseBracet(p);
    else if (newWord && p->current_ == '"')
    {
      p->insideString_ = true;
      p->inc();
    }

    p->value_.clear();

    while (true)
    {
      if (p->eof())
      {
        p->token_ = Escaped;
        return true;
      }

      switch (p->current_)
      {
        case '\\':
          if (p->len() >= 2)
          {
            p->value_.append(p->current_);
            p->inc();
          }
          break;

        case '$': case '[':
          p->token_ = Escaped;
          return true;

        case ' ': case '\t': case '\n': case ';':
          if (!p->insideString_)
          {
            p->token_ = Escaped;
            return true;
          }
          break;

        case '"':
          if (p->insideString_)
          {
            p->inc();
            p->token_ = Escaped;
            p->insideString_ = false;
            return true;
          }
          break;
      }

      p->value_.append(p->current_);
      p->inc();
    }

    return String;
  }

  bool Parser::next()
  {
    value_.clear();

    if (pos_ >= code_.size())
    {
      token_ = EndOfFile;
      return true;
    }

    while (true)
    {
      switch (current_)
      {
        case ' ': case '\t': case '\r':
          if (insideString_)
            return parseString(this);
          else
            return eatWhitespace(this);

        case '\n': case ';':
          if (insideString_)
            return parseString(this);
          else
            return parseEndOfLine(this);

        case '$':
          return parseVariable(this);

        case '[':
          return parseCommand(this);

        case '#':
          parseComment(this);
          continue;

        default:
          return parseString(this);
      }
    }
  }

  // -- CallFrame --

  struct CallFrame
  {
    void set(Str const& name, Str const& value)
    {
      for (auto & pair : variables_)
        if (pair.first.equals(name))
        {
          pair.second.set(value);
          return;
        }

      variables_.push_back({name, value});
    }

    Str const& get(Str const& name)
    {
      for (auto const& pair : variables_)
        if (pair.first.equals(name))
          return pair.second;

      return Str::EMPTY;
    }

    bool get(Str const& name, Str & value) const
    {
      for (auto const& pair : variables_)
        if (pair.first.equals(name))
        {
          value.set(pair.second);
          return true;
        }

      return false;
    }

    std::vector<std::pair<Str, Str>> variables_;
    Str result_;
  };

  // -- Globals --


  static bool debug_ = true;
  static Str error_;

  using CallFrames = std::vector<CallFrame>;
  using Procedures = std::vector<std::pair<Str, Procedure *>>;

  static CallFrames & callFrames()
  {
    static CallFrames frames;
    return frames;
  }

  static CallFrame & currentFrame()
  {
    return callFrames().back();
  }

  static Procedures & procedures()
  {
    static Procedures procs;
    return procs;
  }

  static Procedure * findProcedure(Str const& name)
  {
    for (auto const& proc : procedures())
    {
      if (proc.first.equals(name))
        return proc.second;
    }

    return nullptr;
  }

  Procedure::Procedure(Str const& name)
  {
    procedures().push_back({name, this});
  }

  // -- Interface --

  static const std::string CONFIG_FILE = "/.zumrc";

  void initialize()
  {
    reset();

    evaluate(Str(std::string((char *)&ScriptingLib_tcl[0], ScriptingLib_tcl_len).c_str()));

    // Try to load the config file
    char * home = getenv("HOME");
    if (home)
      exec(std::string(home) + CONFIG_FILE);
    else
      exec("~" + CONFIG_FILE);
  }

  void shutdown()
  {
    for (auto & proc : procedures())
      if (!proc.second->native())
        delete proc.second;
  }

  void reset()
  {
    callFrames().clear();
    callFrames().push_back(CallFrame());
  }

  Str completeName(Str const& name)
  {
    for (auto const& proc : procedures())
    {
      if (proc.first.starts_with(name))
        return proc.first;
    }

    return Str::EMPTY;
  }

  void _return(Str const& value)
  {
    currentFrame().result_.set(value);
  }

  ReturnCode evaluate(Str const& code)
  {
    if (debug_)
      logInfo(Str("Debugging code: ").append(code).append('\n'));

    Parser parser(code);

    currentFrame().result_.clear();
    std::vector<Str> args;

    while (true)
    {
      Token previousToken = parser.token_;
      if (!parser.next())
        return RET_ERROR;

      if (debug_)
        logInfo(Str::format("Token: %s = '%s'", tokenAsReadable[parser.token_], parser.value_.utf8().c_str()));

      Str value = parser.value_;

      if (parser.token_ == Variable)
      {
        if (debug_)
          logInfo(Str::format("Accessing variable '%s'", parser.value_.utf8().c_str()));

        Str var;
        if (currentFrame().get(value, var))
        {
          value.set(var);
          currentFrame().result_.set(var);
        }
        else
          return _reportError(Str("Could not locate variable: ").append(value));
      }
      else if (parser.token_ == Command)
      {
        if (!evaluate(parser.value_))
          return RET_ERROR;

        value = currentFrame().result_;
      }
      else if (parser.token_ == Separator)
      {
        previousToken = parser.token_;
        continue;
      }

      if (parser.token_ == EndOfLine || parser.token_ == EndOfFile)
      {
        if (debug_)
        {
          Str exec("Evaluating: ");

          for (size_t i = 0; i < args.size(); ++i)
            exec.append(args[i]).append(' ');

          logInfo(exec);
        }

        if (args.size() >= 1)
        {
          if (Procedure * proc = findProcedure(args[0]))
          {
            ReturnCode retCode = proc->call(args);
            if (retCode != RET_OK)
              return retCode;
          }
          else
            return _reportError(Str("Could not find procedure: ").append(args[0]));
        }

        args.clear();
      }

      if (!value.empty())
      {
        if (previousToken == Separator || previousToken == EndOfLine)
        {
          args.push_back(value);
        }
        else
        {
          if (args.empty())
            args.push_back(value);
          else
            args.back().append(value);
        }

        if (debug_)
        {
          Str exec("Building arg vector: ");

          for (size_t i = 0; i < args.size(); ++i)
            exec.append(args[i]).append(' ');

          logInfo(exec);
        }
      }
      else if (debug_)
      {
        logInfo(Str("Nothing to do"));
      }

      if (parser.token_ == EndOfFile)
        break;
    }

    return RET_OK;
  }

  ReturnCode exec(std::string const& filename)
  {
    //std::string code = util::loadFileStr(filename);
    //return evaluate(code);
    return RET_OK;
  }

  ReturnCode _arityError(Str const& command)
  {
    return _reportError(Str("Wrong number of arguments to procedure '").append(command).append('\''));
  }

  ReturnCode _reportError(Str const& error)
  {
    logError(Str("TCL: ").append(error));
    flashMessage(error);

    return RET_ERROR;
  }

  // -- Build in functions --

  struct TclProc : public Procedure
  {
    TclProc(Str const& name)
      : Procedure(name)
    { }

    bool native() const { return false; }

    ReturnCode call(ArgumentVector const& args)
    {
      if ((args.size() - 1) != arguments_.size())
        return _reportError(Str::format("Procedure '%s' called with wrong number of arguments, expected %d but got %d", args[0].utf8().c_str(), arguments_.size(), args.size() - 1));

      callFrames().push_back(CallFrame());

      // Setup arguments
      for (size_t i = 0, len = arguments_.size(); i < len; ++i)
        currentFrame().set(arguments_[i], args[i + 1]);

      const ReturnCode retCode = evaluate(body_);
      const Str result(currentFrame().result_);
      callFrames().pop_back();
      currentFrame().result_ = result;

      return retCode;
    };

    Str body_;
    ArgumentVector arguments_;
  };

  TCL_PROC(proc)
  {
    TCL_ARITY(4);

    if (findProcedure(args[1]))
      return _reportError(Str("Procedure of name '").append(args[1]).append(Str("' already exists")));

    TclProc * procData = new TclProc(args[1]);
    procData->body_ = args[3];
    procData->arguments_ = args[2].split(' ');

    return RET_OK;
  }

  TCL_PROC(puts)
  {
    if (args.size() > 1)
    {
      Str log;

      for (uint32_t i = 1, len = args.size(); i < len; ++i)
        log.append(args[i]).append((i - 1) == len ? Str::EMPTY : Str(" "));

      logInfo(log);
      flashMessage(log);
    }

    return RET_OK;
  }

  TCL_PROC(set)
  {
    const uint32_t len = args.size();

    if (len == 2)
    {
      return currentFrame().get(args[1], currentFrame().result_) ? RET_OK : RET_ERROR;
    }
    else if (len == 3)
    {
      currentFrame().result_.set(args[2]);
      currentFrame().set(args[1], args[2]);
      return RET_OK;
    }
    else
      return _arityError(args[0]);
  }

  TCL_PROC(expr)
  {
    /*
    if (args.size() == 1)
      return _arityError(args[0]);

    std::string str;
    for (size_t i = 1; i < args.size(); ++i)
      str += args[i];

    error_ = "";
    double result = calculateExpr(str);
    char buf[128];
    snprintf(buf, 128, "%f", result);
    currentFrame().result = std::string(buf);
    return error_.empty() ? RET_OK : RET_ERROR;
    */

    return RET_OK;
  }
/*
  TCL_PROC(if)
  {
    if (args.size() != 3 && args.size() != 5)
      return _arityError(args[0]);

    double result = calculateExpr(args[1]);

    if (result > 0.0)
      return evaluate(args[2]);
    else if (args.size() == 5)
      return evaluate(args[4]);

    return RET_OK;
  }
*/
  TCL_PROC(return)
  {
    if (args.size() != 2)
      return _arityError(args[0]);

    currentFrame().result_ = args[1];
    return RET_RETURN;
  }

  TCL_PROC(error)
  {
    if (args.size() != 2)
      return _arityError(args[0]);
    return _reportError(args[1]);
  }

  TCL_PROC(eval)
  {
    if (args.size() != 2)
      return _arityError(args[0]);

    Str str;
    for (size_t i = 1; i < args.size(); ++i)
      str.append(args[i]).append(' ');

    return evaluate(str);
  }
/*
  TCL_PROC(while)
  {
    if (args.size() != 3)
      return _arityError(args[0]);

    const std::string check = "expr " + args[1];

    while (true)
    {
      ReturnCode retCode = evaluate(check);
      if (retCode != RET_OK)
        return retCode;

      if (std::atof(currentFrame().result.c_str()) > 0.0)
      {
        retCode = evaluate(args[2]);
        if (retCode == RET_OK || retCode == RET_CONTINUE)
          continue;
        else if (retCode == RET_BREAK)
          return RET_OK;
        else
          return retCode;
      }
      else
      {
        return RET_OK;
      }
    }
  }
*/
  TCL_PROC(incr)
  {
    if (args.size() != 2 && args.size() != 3)
      return _arityError(args[0]);

    Str var;
    if (!currentFrame().get(args[1], var))
      return _reportError(Str::format("Could not find variable '%s'", args[1].utf8().c_str()));

    int inc = 1;
    int value = var.toInt();

    if (args.size() == 3)
      inc = args[2].toInt();

    currentFrame().result_ = Str::fromInt(value + inc);
    return RET_OK;
  }

  TCL_PROC(break)
  {
    return RET_BREAK;
  }

  TCL_PROC(continue)
  {
    return RET_CONTINUE;
  }

}
