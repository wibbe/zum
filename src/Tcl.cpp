
#include "Tcl.h"
#include "ScriptingLib.tcl.h"
#include "Editor.h"
#include "Log.h"

#include "bx/bx.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cassert>
#include <string.h>
#include <map>

#include "bx/os.h"

extern "C" {
  int Jim_clockInit(Jim_Interp * interp);
  int Jim_regexpInit(Jim_Interp * interp);
}

namespace tcl {

  // -- Globals --

  static Jim_Interp * interpreter_ = nullptr;

  // -- Variable --

  static std::vector<Variable *> & builtInVariables()
  {
    static std::vector<Variable *> vars;
    return vars;
  }

  Variable::Variable(const char * name, const char * defaultValue)
    : name_(name),
      type_(ValueType::STRING),
      defaultStrValue_(defaultValue)
  {
    builtInVariables().push_back(this);
  }

  Variable::Variable(const char * name, int defaultValue)
    : name_(name),
      type_(ValueType::INT),
      defaultIntValue_(defaultValue)
  {
    builtInVariables().push_back(this);
  }

  Variable::Variable(const char * name, bool defaultValue)
    : name_(name),
      type_(ValueType::BOOL),
      defaultBoolValue_(defaultValue)
  {
    builtInVariables().push_back(this);
  }

  Jim_Obj * Variable::value() const
  {
    assert(interpreter_);
    return Jim_GetGlobalVariableStr(interpreter_, name_, 0);
  }

  Jim_Obj * Variable::defaultValue() const
  {
    assert(interpreter_);

    switch (type_)
    {
      case ValueType::STRING:
        return Jim_NewStringObj(interpreter_, defaultStrValue_, std::strlen(defaultStrValue_));

      case ValueType::INT:
        return Jim_NewIntObj(interpreter_, defaultIntValue_);

      case ValueType::BOOL:
        return Jim_NewIntObj(interpreter_, defaultBoolValue_ ? 1 : 0);
    }

    return nullptr;
  }

  std::string Variable::toStr() const
  {
    return std::string(Jim_String(value()));
  }

  bool Variable::toBool() const
  {
    return toInt() > 0;
  }

  int Variable::toInt() const
  {
    long val;
    if (Jim_GetLong(interpreter_, value(), &val) != JIM_OK)
      return 0;

    return val;
  }

  // -- BuiltInProc --

  static std::vector<BuiltInProc *> & builtInProcs()
  {
    static std::vector<BuiltInProc *> procs;
    return procs;
  }

  BuiltInProc::BuiltInProc(const char * name, Jim_CmdProc * proc)
    : name_(name),
      proc_(proc)
  {
    builtInProcs().push_back(this);
  }

  BuiltInProc::BuiltInProc(const char * name, const char * args, Jim_CmdProc * proc)
    : name_(name),
      args_(args),
      proc_(proc)
  {
    builtInProcs().push_back(this);
  }

  BuiltInProc::BuiltInProc(const char * name, const char * args, const char * desc, Jim_CmdProc * proc)
    : name_(name),
      args_(args),
      desc_(desc),
      proc_(proc)
  {
    builtInProcs().push_back(this);
  }

  int BuiltInProc::call(struct Jim_Interp * interp, int argc, Jim_Obj * const * argv)
  {
    // Check for the help command
    if (argc == 2 && Jim_CompareStringImmediate(interp, argv[1], "-help"))
    {
      Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
      Jim_AppendStrings(interp, Jim_GetResult(interp), "Usage: \"", Jim_String(argv[0]), NULL);

      if (strlen(args_) > 0)
        Jim_AppendStrings(interp, Jim_GetResult(interp), " ", args_, NULL);
      
      Jim_AppendStrings(interp, Jim_GetResult(interp), "\"");

      if (strlen(desc_) > 0)
        Jim_AppendStrings(interp, Jim_GetResult(interp), " ", desc_, NULL);

      return JIM_OK;
    }

    return proc_(interp, argc, argv);
  }


  static int cmdProc(Jim_Interp * interp, int argc, Jim_Obj * const * argv)
  {
    BuiltInProc * cmd = static_cast<BuiltInProc *>(Jim_CmdPrivData(interp));
    return cmd->call(interp, argc, argv);
  }

  // -- BuiltInSubProc

  static void addSubCommands(struct Jim_Interp * interp, std::vector<const char *> const& subCommands, const char * sep)
  {
    const char * s = "";

    for (int i = 0; i < subCommands.size() / 3; ++i)
    {
      logInfo(subCommands[i * 3]);
      Jim_AppendStrings(interp, Jim_GetResult(interp), s, subCommands[i * 3], NULL);
      s = sep;
    }
  }

  static void showCommandUsage(struct Jim_Interp * interp, int argc, Jim_Obj * const * argv, std::vector<const char *> const& subCommands)
  {
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), "Usage: \"", Jim_String(argv[0]), " command ... \", where command is one of: ", NULL);
    addSubCommands(interp, subCommands, ", ");
  }

  static std::vector<BuiltInSubProc *> & builtInSubCmdProcs()
  {
    static std::vector<BuiltInSubProc *> procs;
    return procs;
  }

  BuiltInSubProc::BuiltInSubProc(const char * name, std::vector<const char *> subCommands, SubCmdProc * proc)
    : name_(name),
      subCommands_(subCommands),
      proc_(proc)
  {
    assert(subCommands.size() % 3 == 0);
    builtInSubCmdProcs().push_back(this);
  }

  int BuiltInSubProc::call(struct Jim_Interp * interp, int argc, Jim_Obj * const * argv)
  {
    bool help = false;
    const char * cmdName = Jim_String(argv[0]);

    if (argc < 2)
    {
      Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
      Jim_AppendStrings(interp, Jim_GetResult(interp), "wrong # args: should be \"", cmdName, " command ...\"\n", NULL);
      Jim_AppendStrings(interp, Jim_GetResult(interp), "Use \"", cmdName, " -help ?command?\" for help about the command", NULL);    
      return JIM_ERR;
    }

    Jim_Obj * cmd = argv[1];

    // Check for the help command
    if (Jim_CompareStringImmediate(interp, cmd, "-help"))
    {
      if (argc == 2)
      {
        showCommandUsage(interp, argc, argv, subCommands_);
        return JIM_OK;
      }

      help = true;
      cmd = argv[2];
    }

    // Check for special builtin '-commands' command first
    if (Jim_CompareStringImmediate(interp, cmd, "-commands"))
    {
        /* Build the result here */
        Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
        addSubCommands(interp, subCommands_, " ");
        return JIM_OK;
    }

    for (int i = 0; i < subCommands_.size() / 3; ++i)
    {
      if (Jim_CompareStringImmediate(interp, cmd, subCommands_[i * 3]))
      {
        if (help)
        {
          Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
          Jim_AppendStrings(interp, Jim_GetResult(interp), "Usage: \"", cmdName, " ", subCommands_[i * 3], NULL);

          if (strlen(subCommands_[i * 3 + 1]) > 0)
            Jim_AppendStrings(interp, Jim_GetResult(interp), " ", subCommands_[i * 3 + 1], NULL);
          
          Jim_AppendStrings(interp, Jim_GetResult(interp), "\"");

          if (strlen(subCommands_[i * 3 + 2]) > 0)
            Jim_AppendStrings(interp, Jim_GetResult(interp), " ", subCommands_[i * 3 + 2], NULL);

          return JIM_OK;
        }
        else
        {
          return proc_(interp, i, argc - 2, argv + 2);
        }
      }
    }

    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
    Jim_AppendStrings(interp, Jim_GetResult(interp), cmdName, ", unknown command \"", Jim_String(cmd), "\" should be one of: ", NULL);
    addSubCommands(interp, subCommands_, ", ");

    return JIM_ERR;
  }

  static int subCmdProc(Jim_Interp * interp, int argc, Jim_Obj * const * argv)
  {
    BuiltInSubProc * subCmd = static_cast<BuiltInSubProc *>(Jim_CmdPrivData(interp));
    return subCmd->call(interp, argc, argv);
  }

  // -- Exposed procs --

  class ExposedProc
  {
    public:
      ExposedProc(std::string const& name)
        : name_(name)
      { }

      std::string name() const { return name_; }

    private:
      std::string name_;
  };

  static std::vector<ExposedProc> exposedProcs_;

  // -- Interface --

  static const std::string CONFIG_FILE = "zum.conf";

  void initialize()
  {
    interpreter_ = Jim_CreateInterp();
    Jim_RegisterCoreCommands(interpreter_);

    // Register extensions
    ::Jim_clockInit(interpreter_);
    ::Jim_regexpInit(interpreter_);

    // Register built in commands
    for (auto * cmd : builtInProcs())
      Jim_CreateCommand(interpreter_, cmd->name(), cmdProc, cmd, nullptr);

    for (auto * cmd : builtInSubCmdProcs())
      Jim_CreateCommand(interpreter_, cmd->name(), subCmdProc, cmd, nullptr);

    // Register built in variables
    for (auto * var : builtInVariables())
      Jim_SetGlobalVariableStr(interpreter_, var->name(), var->defaultValue());

#if DEBUG
    // Use the file from the source directory
    Jim_EvalFileGlobal(interpreter_, "../src/ScriptingLib.tcl");
#else
    Jim_EvalSource(interpreter_, __FILE__, __LINE__, std::string((char *)&ScriptingLib[0], BX_COUNTOF(ScriptingLib)).c_str());
#endif


    std::string configFile = "";
#if BX_PLATFORM_LINUX || BX_PLATFORM_OSX
    // Try to load the config file
    char * home = getenv("HOME");
    if (home)
      configFile = std::string(home) + "/." + CONFIG_FILE;
    else
      configFile = "~/." + CONFIG_FILE;
#else
    char pwd[1024];
    configFile = std::string(bx::pwd(pwd, 1024)) + "/" + CONFIG_FILE;
#endif

    logInfo("Loading config file: ", configFile);
    Jim_EvalFileGlobal(interpreter_, configFile.c_str());
  }

  void shutdown()
  {
    Jim_FreeInterp(interpreter_);
    interpreter_ = nullptr;
  }

  bool evaluate(std::string const& code)
  {
    const bool ok = Jim_EvalGlobal(interpreter_, code.c_str()) == JIM_OK;
    if (!ok)
      logError(result());

    return ok;
  }

  std::string result()
  {
    return std::string(Jim_String(Jim_GetResult(interpreter_)));
  }

  std::vector<std::string> findMatches(std::string const& name)
  {
    std::vector<std::string> result;

    // Search for matching procs
    for (auto * cmd : builtInProcs())
    {
      const std::string cmdName(cmd->name());
      if (cmdName.find(name) == 0)
        result.push_back(cmdName);
    }

    // Search for matching global variables
    for (auto * cmd : builtInSubCmdProcs())
    {
      const std::string cmdName(cmd->name());
      if (cmdName.find(name) == 0)
        result.push_back(cmdName);
    }

    for (auto const& it : exposedProcs_)
    {
      if (it.name().find(name) == 0)
        result.push_back(it.name());
    }

    // Search for matching global variables
    for (auto * var : builtInVariables())
    {
      const std::string varName(var->name());
      if (varName.find(name) == 0)
        result.push_back(varName);
    }

    return result;
  }

  TCL_FUNC(puts, "string ?string ...?")
  {
    TCL_CHECK_ARGS(2, 1000);

    std::string log;
    for (uint32_t i = 1; i < argc; ++i)
      log += Jim_String(argv[i]) + std::string((i + 1) == argc ? "" : " ");

    logInfo(log);
    flashMessage(log);

    return JIM_OK;
  }

  TCL_FUNC(expose, "string", "Expose a tcl function to command auto-completion")
  {
    TCL_CHECK_ARG(2);
    TCL_STRING_ARG(1, procName);

    exposedProcs_.push_back(ExposedProc(procName));
  }
}
