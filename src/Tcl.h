
#pragma once

#include <vector>
#include "Str.h"
#include "jim.h"
#include "utf8.h"

namespace tcl {

  class BuiltInProc
  {
    public:
      BuiltInProc(const char * name, Jim_CmdProc * proc);
      BuiltInProc(const char * name, const char * args, Jim_CmdProc * proc);
      BuiltInProc(const char * name, const char * args, const char * desc, Jim_CmdProc * proc);

      const char * name() const { return name_; }
      const char * args() const { return args_; }

      int call(struct Jim_Interp * interp, int argc, Jim_Obj * const * argv);

    private:
      const char * name_ = nullptr;
      const char * args_ = nullptr;
      const char * desc_ = nullptr;
      Jim_CmdProc * proc_ = nullptr;
  };


  typedef int SubCmdProc(struct Jim_Interp * interp, int subCommand, int argc, Jim_Obj * const * argv);


  class BuiltInSubProc
  {
    public:
      BuiltInSubProc(const char * name, std::vector<const char *> subCommands, SubCmdProc * proc);

      const char * name() const { return name_; }

      int call(struct Jim_Interp * interp, int argc, Jim_Obj * const * argv);

    private:
      const char * name_ = nullptr;
      std::vector<const char *> subCommands_;
      SubCmdProc * proc_ = nullptr;
  };


  class Variable
  {
    private:
      enum class ValueType
      {
        STRING,
        INT,
        BOOL
      };

    public:
      Variable(const char * name, const char * defaultValue);
      Variable(const char * name, int defaultValue);
      Variable(const char * name, bool defaultValue);

      bool toBool() const;
      int toInt() const;
      std::string toStr() const;

      const char * name() const { return name_; }

      Jim_Obj * value() const;
      Jim_Obj * defaultValue() const;

    private:
      const char * name_ = nullptr;

      ValueType type_ = ValueType::STRING;
      union {
        const char * defaultStrValue_;
        int defaultIntValue_;
        bool defaultBoolValue_;
      };
  };

  void initialize();
  void shutdown();
  void reset();

  bool evaluate(std::string const& code);
  std::string result();

  std::vector<std::string> findMatches(std::string const& name);

}

// -- Useful macros --

#define TCL_FUNC(name, ...) \
  int tclBuiltIn__##name(Jim_Interp *, int argc, Jim_Obj * const *); \
  namespace { const tcl::BuiltInProc _buildInProc__##name(#name, ##__VA_ARGS__, &tclBuiltIn__##name); } \
  int tclBuiltIn__##name(Jim_Interp * interp, int argc, Jim_Obj * const * argv)

#define TCL_SUBFUNC(name, ...) \
  int tclBuiltInSub__##name(Jim_Interp *, int subCommand, int argc, Jim_Obj * const *); \
  namespace { const tcl::BuiltInSubProc _buildInSubProc__##name(#name, std::vector<const char *>{ __VA_ARGS__ }, &tclBuiltInSub__##name); } \
  int tclBuiltInSub__##name(Jim_Interp * interp, int subCommand, int argc, Jim_Obj * const * argv)

#define TCL_EXPOSE_FUNC(name, desc) \
  int tclBuiltInExposed__##name(Jim_Interp *, int argc, Jim_Obj * const *)  { name(); } \
  namespace { const tcl::BuiltInProc _buildInExposedProc__##name(#name, "", desc, &tclBuiltInExposed__##name); } \

#define TCL_CHECK_ARG(count) \
  do { if (argc != count) { Jim_WrongNumArgs(interp, 1, argv, static_cast<tcl::BuiltInProc *>(Jim_CmdPrivData(interp))->args()); return JIM_ERR; } } while (false)

#define TCL_CHECK_ARG_DESC(count, desc) \
  do { if (argc != count) { Jim_WrongNumArgs(interp, 1, argv, desc); return JIM_ERR; } } while (false)

#define TCL_CHECK_ARGS(min, max) \
  do { if (argc < min || argc > max) { Jim_WrongNumArgs(interp, 1, argv, static_cast<tcl::BuiltInProc *>(Jim_CmdPrivData(interp))->args()); return JIM_ERR; } } while (false)

#define TCL_CHECK_ARGS_DESC(min, max, desc) \
  do { if (argc < min || argc > max) { Jim_WrongNumArgs(interp, 1, argv, desc); return JIM_ERR; } } while (false)

#define TCL_INT_ARG(i, name) \
  long name = 0; \
  do { \
    if (i < argc) \
      if (Jim_GetLong(interp, argv[i], &name) != JIM_OK) \
        return JIM_ERR; \
  } while (false)

#define TCL_STRING_ARG(i, name) \
  std::string name; \
  do { \
    if (i < argc) \
      name = std::string(Jim_String(argv[i])); \
  } while (false)

#define TCL_INT_RESULT(value) \
  Jim_SetResultInt(interp, value); \
  return JIM_OK

#define TCL_DOUBLE_RESULT(value) \
  Jim_SetResult(interp, Jim_NewDoubleObj(interp, value)); \
  return JIM_OK

#define TCL_STRING_UTF8_RESULT(value) \
  do { \
    const std::string result = (value); \
    Jim_SetResult(interp, Jim_NewStringObjUtf8(interp, result.c_str(), utf8_strlen(result.c_str(), result.size()))); \
    return JIM_OK; \
  } while (false)

#define TCL_STRING_RESULT(value) \
  do { \
    const std::string result = (value); \
    Jim_SetResult(interp, Jim_NewStringObj(interp, result.c_str(), result.size())); \
    return JIM_OK; \
  } while (false)
