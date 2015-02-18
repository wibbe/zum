
#pragma once

#include <vector>
#include "Str.h"

namespace tcl {

  enum ReturnCode
  {
    RET_ERROR,
    RET_OK,
    RET_RETURN,
    RET_BREAK,
    RET_CONTINUE
  };

  using ArgumentVector = std::vector<Str>;

  struct Procedure
  {
    Procedure(Str const& name);

    virtual ~Procedure() { }
    virtual bool native() const = 0;
    virtual ReturnCode call(ArgumentVector const& args) = 0;
  };

  class Variable
  {
    public:
      Variable(const char * name, const char * defaultValue);

      Str const& get() const;
      void set(Str const& value) const;

    private:
      const Str name_;
  };

  void initialize();
  void shutdown();
  void reset();

  ReturnCode evaluate(Str const& code);

  ReturnCode exec(const char * command);
  ReturnCode exec(const char * command, Str const& arg1);
  ReturnCode exec(const char * command, Str const& arg1, Str const& arg2);

  ReturnCode execFile(std::string const& filename);

  std::vector<Str> findMatches(Str const& name);

  // -- Internal functions --
  ReturnCode _arityError(Str const& command);
  ReturnCode _reportError(Str const& _error);
  void _return(Str const& value);
}

#define TCL_OK() \
  return tcl::RET_OK

#define TCL_RETURN(arg) \
  do { \
    tcl::_return(arg); \
    return tcl::RET_OK; \
  } while (false)

#define TCL_ARITY(num) \
  do { if (args.size() < num) return tcl::_arityError(args[0]); } while (false)

#define TCL_PROC(name) \
  struct TclProc_##name : public tcl::Procedure { TclProc_##name() : Procedure(Str(#name)) { } bool native() const { return true; } tcl::ReturnCode call(tcl::ArgumentVector const& args); }; \
  namespace { TclProc_##name __dummy##name; } \
  tcl::ReturnCode TclProc_##name::call(tcl::ArgumentVector const& args)

#define TCL_PROC2(funcName, tclName) \
  struct TclProc_##funcName : public tcl::Procedure { TclProc_##funcName() : Procedure(Str(tclName)) { } bool native() const { return true; } tcl::ReturnCode call(tcl::ArgumentVector const& args); }; \
  namespace { TclProc_##funcName __dummy##funcName; } \
  tcl::ReturnCode TclProc_##funcName::call(tcl::ArgumentVector const& args)

#define FUNC_0(funcName, tclName) \
  bool funcName(); \
  struct CppFunc_##funcName : public tcl::Procedure { \
    CppFunc_##funcName() : Procedure(Str(tclName)) { } \
    bool native() const { return true; } \
    tcl::ReturnCode call(tcl::ArgumentVector const& args) { \
      if (args.size() != 1) return tcl::_arityError(args[0]); \
      return funcName() ? tcl::RET_OK : tcl::RET_ERROR; \
    } \
  }; \
  namespace { CppFunc_##funcName __dummy##funcName; } \
  bool funcName()

#define FUNC_X(funcName, tclName) \
  bool funcName(tcl::ArgumentVector const& args); \
  struct CppFunc_##funcName : public tcl::Procedure { \
    CppFunc_##funcName() : Procedure(Str(tclName)) { } \
    bool native() const { return true; } \
    tcl::ReturnCode call(tcl::ArgumentVector const& args) { \
      return funcName(args) ? tcl::RET_OK : tcl::RET_ERROR; \
    } \
  }; \
  namespace { CppFunc_##funcName __dummy##funcName; } \
  bool funcName(tcl::ArgumentVector const& args)

#define FUNC_1(funcName, tclName) \
  bool funcName(Str const& arg1); \
  struct CppFunc_##funcName : public tcl::Procedure \
  { \
    CppFunc_##funcName() : Procedure(Str(tclName)) { } \
    bool native() const { return true; } \
    tcl::ReturnCode call(tcl::ArgumentVector const& args) \
    { \
      if (args.size() != 2) return tcl::_arityError(args[0]); \
      return funcName(args[1]) ? tcl::RET_OK : tcl::RET_ERROR; \
    } \
  }; \
  namespace { CppFunc_##funcName __dummy##funcName; } \
  bool funcName(Str const& arg1)

#define FUNC_2(funcName, tclName) \
  bool funcName(Str const& arg1, Str const& arg2); \
  struct CppFunc_##funcName : public tcl::Procedure \
  { \
    CppFunc_##funcName() : Procedure(Str(tclName)) { } \
    bool native() const { return true; } \
    tcl::ReturnCode call(tcl::ArgumentVector const& args) \
    { \
      if (args.size() != 3) return tcl::_arityError(args[0]); \
      return funcName(args[1], args[2]]) ? tcl::RET_OK : tcl::RET_ERROR; \
    } \
  }; \
  namespace { CppFunc_##funcName __dummy##funcName; } \
  bool funcName(Str const& arg1, Str const& arg2)