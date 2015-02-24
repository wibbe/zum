
#pragma once

#include <vector>
#include "Str.h"
#include "jim.h"

namespace tcl {

  // Hashed strings that are used in the evaluation
  enum HashCodes
  {
    kNum0 = 1114619285u,      // 0
    kNum1 = 3410215576u,      // 1
    kYes = 3390905302u,       // yes
    kNo = 2138104260u,        // no
    kTrue = 3899511338u,      // true
    kFalse = 3213626187u,     // false
    kOn = 31923308u,          // on
    kOff = 3527196773u,       // off
    kLength = 3673030522u,    // length
    kIndex = 3470952552u,     // index
    kHash = 1615745465u,      // hash
    kToLower = 1911912945u,   // tolower
    kToUpper = 98427793u,     // toupper
    kEqual = 1526800470u,     // equal
    kFirst = 3996466946u,     // first
    kIs = 983832745u,         // is
    kAlpha = 553757373u,      // alpha
    kAlnum = 3831713840u,     // alnum
    kBoolean = 1395558365u,   // boolean
    kInteger = 318025161u,    // integer
    kRepeat = 1415255128u,    // repeat
    kArgs = 2284312459u,      // args
    kBody = 3886621310u,      // body
    kLength_ = 1325425629u,   // -length
    kNoCase_ = 188802517u,    // -nocase
  };

  enum ReturnCode
  {
    RET_ERROR,
    RET_OK,
    RET_RETURN,
    RET_BREAK,
    RET_CONTINUE
  };

  using ArgumentVector = std::vector<Str>;

  class BuiltInProc
  {
    public:
      BuiltInProc(const char * name, Jim_CmdProc * proc);
  };


  struct Procedure
  {
    Procedure(Str const& name);

    virtual ~Procedure() { }
    virtual bool native() const = 0;
    virtual ReturnCode call(ArgumentVector const& args) = 0;

    Str desc_;
  };

  class Variable
  {
    public:
      Variable(const char * name, const char * defaultValue);

      bool toBool() const;
      int toInt() const;

      Str const& get() const;
      void set(Str const& value) const;

    private:
      const Str name_;
  };

  void initialize();
  void shutdown();
  void reset();

  ReturnCode evaluate(Str const& code, int level = 0);

  // Returns the result from the last code evaluation.
  Str const& result();

  ReturnCode exec(const char * command);
  ReturnCode exec(const char * command, Str const& arg1);
  ReturnCode exec(const char * command, Str const& arg1, Str const& arg2);

  ReturnCode execFile(std::string const& filename);

  std::vector<Str> findMatches(Str const& name);

  // -- Type checking --

  bool isTrue(Str const& str);
  bool isFalse(Str const& str);
  bool isBoolean(Str const str);

  // -- Internal functions --
  ReturnCode _arityError(Str const& command);
  ReturnCode _reportError(Str const& _error);

  ReturnCode argError(const char * desc);

  ReturnCode resultStr(Str const& value);
  ReturnCode resultBool(bool value);
  ReturnCode resultInt(long long int value);
}

// -- Useful macros --

#define TCL_FUNC(name) \
  int tclBuiltIn__##name(Jim_Interp *, int argc, Jim_Obj * const *); \
  namespace { const tcl::BuiltInProc _buildInProc__##name(#name, &tclBuiltIn__##name); } \
  int tclBuiltIn__##name(Jim_Interp * interp, int argc, Jim_Obj * const * argv)

#define TCL_CHECK_ARG(count, desc) \
  do { if (argc != count) Jim_WrongNumArgs(interp, 1, argv, desc); return JIM_ERR; } while (false)

#define TCL_INT_ARG(i, name) \
  long name = 0; \
  if (Jim_GetLong(interp, argv[i], &name) != JIM_OK) \
    return JIM_ERR

#define TCL_INT_RESULT(value) \
  Jim_SetResultInt(interp, value); \
  return JIM_OK



// -- Old Macros --

#define TCL_OK() \
  return tcl::RET_OK

#define TCL_CHECK_ARG_OLD(count, desc) \
  do { if (args.size() != count) return tcl::argError(desc); } while (false)

#define TCL_CHECK_ARG_OLDS(min, max, desc) \
  do { if (args.size() < min || args.size() > max) return tcl::argError(desc); } while (false)

#define TCL_DESC(descStr) \
  this->desc_ = Str(descStr)

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
      if (args.size() != 1) return tcl::argError(tclName); \
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

#define FUNC_1(funcName, tclName, desc) \
  bool funcName(Str const& arg1); \
  struct CppFunc_##funcName : public tcl::Procedure \
  { \
    CppFunc_##funcName() : Procedure(Str(tclName)) { } \
    bool native() const { return true; } \
    tcl::ReturnCode call(tcl::ArgumentVector const& args) \
    { \
      if (args.size() != 2) return tcl::argError(desc); \
      return funcName(args[1]) ? tcl::RET_OK : tcl::RET_ERROR; \
    } \
  }; \
  namespace { CppFunc_##funcName __dummy##funcName; } \
  bool funcName(Str const& arg1)

#define FUNC_2(funcName, tclName, desc) \
  bool funcName(Str const& arg1, Str const& arg2); \
  struct CppFunc_##funcName : public tcl::Procedure \
  { \
    CppFunc_##funcName() : Procedure(Str(tclName)) { } \
    bool native() const { return true; } \
    tcl::ReturnCode call(tcl::ArgumentVector const& args) \
    { \
      if (args.size() != 3) return tcl::argError(desc); \
      return funcName(args[1], args[2]) ? tcl::RET_OK : tcl::RET_ERROR; \
    } \
  }; \
  namespace { CppFunc_##funcName __dummy##funcName; } \
  bool funcName(Str const& arg1, Str const& arg2)

