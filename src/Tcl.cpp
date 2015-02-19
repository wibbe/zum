
#include "Tcl.h"
#include "ScriptingLib.tcl.h"
#include "Editor.h"
#include "Log.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <map>

namespace tcl {

  // -- Externs

  double calculateExpr(std::string const& str);

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
    bool eof() const { return pos_ >= code_.size(); }
    uint32_t len() const { return code_.size() - pos_; }

    const Str code_;
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

    while (!p->eof() && (isAlpha(p->current_) || isDigit(p->current_) || p->current_ == ':' || p->current_ == '_'))
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

    while (pos_ < code_.size())
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

    token_ = EndOfFile;
    return true;
  }


  // -- CallFrame --


  class CallFrame
  {
    public:
      void set(Str const& name, Str const& value)
      {
        for (auto & pair : variables_)
        {
          if (pair.first.equals(name))
          {
            pair.second = value;
            return;
          }
        }

        variables_.push_back({name, value});
      }

      Str const& get(Str const& name)
      {
        for (auto const& pair : variables_)
        {
          if (pair.first.equals(name))
            return pair.second;
        }

        return Str::EMPTY;
      }

      bool get(Str const& name, Str & value) const
      {
        for (auto const& pair : variables_)
        {
          if (pair.first.equals(name))
          {
            value = pair.second;
            return true;
          }
        }

        return false;
      }

      std::vector<std::pair<Str, Str>> const& variables() const { return variables_; }

    public:
      std::vector<std::pair<Str, Str>> variables_;
      Str result_;
  };

  class FrameStack
  {
    public:
      FrameStack()
      {
        // The global frame
        frames_.push_back(CallFrame());
      }

      CallFrame & global() { return frames_.front(); }
      CallFrame & current() { return frames_.back(); }

      void push() { frames_.push_back(CallFrame()); }
      void pop() { frames_.pop_back(); }

    private:
      std::vector<CallFrame> frames_;
  };

  static FrameStack & frames()
  {
    static FrameStack stack;
    return stack;
  }


  // -- Variable --


  Variable::Variable(const char * name, const char * defaultValue)
    : name_(name)
  {
    frames().global().set(name_, Str(defaultValue));
  }

  Str const& Variable::get() const
  {
    return frames().global().get(name_);
  }

  void Variable::set(Str const& value) const
  {
    frames().global().set(name_, value);
  }

  bool Variable::toBool() const
  {
    return !isFalse(get());
  }

  int Variable::toInt() const
  {
    return get().toInt();
  }


  // -- Globals --


  static bool debug_ = false;
  static Str error_;

  // -- Procedures --

  struct ProcedureInfo
  {
    ProcedureInfo(uint32_t hash, Str const& name, Procedure * proc)
      : hash_(hash),
        name_(name),
        proc_(proc)
    { }

    uint32_t hash_ = 0;
    Str name_;
    Procedure * proc_ = nullptr;
  };

  struct ProcedureInfoOrder
  {
    inline bool operator () (ProcedureInfo const& left, ProcedureInfo const right) const
    {
        return left.hash_ < right.hash_;
    }

    inline bool operator () (ProcedureInfo const& left, uint32_t right) const
    {
        return left.hash_ < right;
    }
  };

  static std::vector<ProcedureInfo> & procedures()
  {
    static std::vector<ProcedureInfo> procs;
    return procs;
  }

  static Procedure * findProcedure(Str const& name)
  {
    const uint32_t hash = name.hash();

    const auto result = std::lower_bound(procedures().begin(), procedures().end(), hash, ProcedureInfoOrder());
    if (result != procedures().end() && result->hash_ == hash)
      return result->proc_;

    return nullptr;
  }

  Procedure::Procedure(Str const& name)
  {
    procedures().emplace_back(ProcedureInfo(name.hash(), name, this));

    // Make sure the procedure vector is sorted
    std::sort(procedures().begin(), procedures().end(), ProcedureInfoOrder());
  }


  // -- Interface --


  static const std::string CONFIG_FILE = "/.zumrc";

  void initialize()
  {
    logInfo("Evaluating ScriptingLib...");
    evaluate(Str(std::string((char *)&ScriptingLib_tcl[0], ScriptingLib_tcl_len).c_str()));

    logInfo("Evaluating ~/.zumrc");
    // Try to load the config file
    char * home = getenv("HOME");
    if (home)
      execFile(std::string(home) + CONFIG_FILE);
    else
      execFile("~" + CONFIG_FILE);
  }

  void shutdown()
  {
    for (auto & info : procedures())
      if (!info.proc_->native())
        delete info.proc_;
  }

  std::vector<Str> findMatches(Str const& name)
  {
    std::vector<Str> result;

    for (auto const& proc : procedures())
    {
      if (proc.name_.starts_with(name))
        result.push_back(proc.name_);
    }

    for (auto const& var : frames().global().variables())
    {
      if (var.first.starts_with(name))
        result.push_back(var.first);
    }

    return result;
  }

  ReturnCode resultStr(Str const& value)
  {
    frames().current().result_ = value;
    return RET_OK;
  }

  ReturnCode resultBool(bool value)
  {
    static const Str trueStr('1');
    static const Str falseStr('0');

    frames().current().result_ = value ? trueStr : falseStr;
    return RET_OK;
  }

  ReturnCode resultInt(long long int value)
  {
    frames().current().result_ = Str::fromInt(value);
    return RET_OK;
  }

  Str const& result()
  {
    return frames().current().result_;
  }

  ReturnCode evaluate(Str const& code, int level)
  {
    if (debug_)
    {
      logInfo(Str(' ', level * 2), "evaluate {\n", code, "\n}");
      level++;
    }

    Parser parser(code);

    frames().current().result_.clear();
    std::vector<Str> args;

    while (true)
    {
      Token previousToken = parser.token_;

      if (debug_)
        logInfo(Str(' ', level * 2), "Parse next token");

      if (!parser.next())
        return RET_ERROR;

      if (debug_)
        logInfo(Str(' ', level * 2), "Token: ", tokenAsReadable[parser.token_]," = '", parser.value_,"'");

      Str value(parser.value_);

      if (parser.token_ == Variable)
      {
        if (debug_)
          logInfo(Str(' ', level * 2), "Accessing variable '", parser.value_,"'");

        Str var;
        if (frames().current().get(value, var))
        {
          value = var;
          frames().current().result_.set(var);
        }
        else
          return _reportError(Str("Could not locate variable: ").append(value));
      }
      else if (parser.token_ == Command)
      {
        if (debug_)
          logInfo(Str(' ', level * 2), "Evaluating sub-command: '", parser.value_, "'");

        if (evaluate(parser.value_, level + 1) != RET_OK)
          return RET_ERROR;

        value = frames().current().result_;

        if (debug_)
          logInfo(Str(' ', level * 2), "Result: '", value, "'");
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
          Str exec;
          for (size_t i = 0; i < args.size(); ++i)
            exec.append(args[i]).append(i < (args.size() - 1) ? ' ' : '\'');

          logInfo(Str(' ', level * 2), "Calling procedure: '", exec);
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

      if (!value.empty() || parser.token_ == String)
      {
        if (previousToken == Separator || previousToken == EndOfLine || parser.token_ == Command)
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
      }

      if (parser.token_ == EndOfFile)
        break;
    }

    return RET_OK;
  }

  ReturnCode exec(const char * command)
  {
    return evaluate(Str(command));
  }

  ReturnCode exec(const char * command, Str const& arg1)
  {
    return evaluate(Str(command).append(' ').append(arg1));
  }

  ReturnCode exec(const char * command, Str const& arg1, Str const& arg2)
  {
    return evaluate(Str(command).append(' ').append(arg1).append(' ').append(arg2));
  }

  ReturnCode execFile(std::string const& filename)
  {
    std::ifstream file(filename.c_str());
    if (!file.is_open())
      return _reportError(Str::format("Could not open document %s", filename.c_str()));

    // get length of file:
    file.seekg (0, file.end);
    const int length = file.tellg();
    file.seekg (0, file.beg);

    std::vector<char> buffer;
    buffer.reserve(length + 1);
    file.read(&buffer[0], length);
    buffer[length] = '\0';

    return evaluate(Str(&buffer[0]));
  }

  ReturnCode _arityError(Str const& command)
  {
    return argError(command.utf8().c_str());
  }

  ReturnCode _reportError(Str const& error)
  {
    error_.set(error);

    logError(Str("TCL: ").append(error));
    flashMessage(error);

    return RET_ERROR;
  }

  ReturnCode argError(const char * desc)
  {
    return _reportError(Str("wrong # args: should be \"").append(Str(desc)).append('\"'));
  }

  // -- Type checking --

  bool isTrue(Str const& str)
  {
    switch (str.hash())
    {
      case kNum1:
      case kTrue:
      case kOn:
      case kYes:
        return true;

      default:
        return false;
    }
  }

  bool isFalse(Str const& str)
  {
    switch (str.hash())
    {
      case kNum0:
      case kFalse:
      case kOff:
      case kNo:
        return true;

      default:
        return false;
    }
  }

  bool isBoolean(Str const str)
  {
    switch (str.hash())
    {
      case kNum0:
      case kNum1:
      case kTrue:
      case kFalse:
      case kOn:
      case kOff:
      case kYes:
      case kNo:
        return true;

      default:
        return false;
    }
  }

  // -- Build in functions --

  struct TclProc : public Procedure
  {
    TclProc(Str const& name, std::vector<Str> const& arguments, Str const& body)
      : Procedure(name),
        arguments_(arguments),
        body_(body),
        varargs_(arguments.size() > 0 && arguments.back().equals(Str("args")))
    { }

    bool native() const { return false; }

    ReturnCode call(ArgumentVector const& args)
    {
      if (!varargs_ && (args.size() - 1) != arguments_.size())
        return _reportError(Str::format("Procedure '%s' called with wrong number of arguments, expected %d but got %d", args[0].utf8().c_str(), arguments_.size(), args.size() - 1));

      frames().push();

      // Setup arguments
      if (varargs_)
      {

      }
      else
      {
        for (size_t i = 0, len = arguments_.size(); i < len; ++i)
          frames().current().set(arguments_[i], args[i + 1]);
      }

      const ReturnCode retCode = evaluate(body_);
      const Str result(frames().current().result_);
      frames().pop();
      frames().current().result_ = result;

      return retCode;
    };

    Str body_;
    ArgumentVector arguments_;
    bool varargs_ = false;
  };

  TCL_PROC(proc)
  {
    TCL_CHECK_ARG(4, "proc name args body");

    if (findProcedure(args[1]))
      return _reportError(Str("Procedure of name '").append(args[1]).append(Str("' already exists")));

    new TclProc(args[1], args[2].split(' '), args[3]);
    return RET_OK;
  }

  TCL_PROC(puts)
  {
    TCL_CHECK_ARGS(2, 1000, "puts string ?string ...?");

    Str log;
    for (uint32_t i = 1, len = args.size(); i < len; ++i)
      log.append(args[i]).append((i - 1) == len ? Str::EMPTY : Str(" "));

    logInfo(log);
    flashMessage(log);

    return RET_OK;
  }

  TCL_PROC(set)
  {
    TCL_CHECK_ARGS(2, 3, "set varName ?newValue?");

    const uint32_t len = args.size();
    if (len == 2)
    {
      return frames().current().get(args[1], frames().current().result_) ? RET_OK : RET_ERROR;
    }
    else if (len == 3)
    {
      frames().current().result_.set(args[2]);
      frames().current().set(args[1], args[2]);
      return RET_OK;
    }
  }

  TCL_PROC(expr)
  {
    TCL_CHECK_ARGS(2, 1000, "expr arg ?arg ...?");

    Str str;
    for (uint32_t i = 1; i < args.size(); ++i)
      str.append(args[i]).append(' ');

    error_.clear();
    double result = calculateExpr(str.utf8());
    frames().current().result_.set(Str::fromDouble(result));

    return error_.empty() ? RET_OK : RET_ERROR;
  }

  TCL_PROC(if)
  {
    if (args.size() != 3 && args.size() != 5)
      return _arityError(args[0]);

    if (exec("expr", args[1]) == RET_OK)
    {
      if (!isFalse(frames().current().result_))
        return evaluate(args[2]);
      else
        return evaluate(args[4]);
    }

    return RET_ERROR;
  }

  TCL_PROC(return)
  {
    TCL_CHECK_ARG(2, "return value");
    return resultStr(args[1]);
  }

  TCL_PROC(error)
  {
    TCL_CHECK_ARG(2, "error string");
    return _reportError(args[1]);
  }

  TCL_PROC(eval)
  {
    TCL_CHECK_ARGS(2, 1000, "eval arg ?arg ...?");

    Str str;
    for (size_t i = 1; i < args.size(); ++i)
      str.append(args[i]).append(' ');

    return evaluate(str);
  }

  TCL_PROC(while)
  {
    TCL_CHECK_ARG(3, "while test command");

    Str check("expr ");
    check.append(args[1]);

    while (true)
    {
      ReturnCode retCode = evaluate(check);
      if (retCode != RET_OK)
        return retCode;

      if (frames().current().result_.toDouble() > 0.0)
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

  TCL_PROC(incr)
  {
    TCL_CHECK_ARGS(2, 3, "incr varName ?increment?");

    Str var;
    if (!frames().current().get(args[1], var))
      return _reportError(Str::format("Could not find variable '%s'", args[1].utf8().c_str()));

    int inc = 1;
    int value = var.toInt();

    if (args.size() == 3)
      inc = args[2].toInt();

    frames().current().result_ = Str::fromInt(value + inc);
    frames().current().set(args[1], frames().current().result_);

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

  TCL_PROC(debug)
  {
    TCL_CHECK_ARGS(1, 2, "debug ?enabled?");

    const double result = calculateExpr(args[1].utf8());
    debug_ = result > 0.0;

    TCL_OK();
  }

}
