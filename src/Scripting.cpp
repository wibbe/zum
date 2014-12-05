
#include "Scripting.h"
#include "ScriptingLib.lua.h"

#include "Document.h"
#include "Editor.h"

#include "lua.hpp"
#include <stdlib.h>

static void assertArgCount(lua_State * L, int min, int max)
{
  const int count = lua_gettop(L);
  if (count < min || count > max)
    luaL_error(L, "Wrong number of arguments, got %d, expected %d\n", count, min);
}

static void registerTable(lua_State * L, const char * name, const luaL_Reg * functions)
{
  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setglobal(L, name);
  luaL_setfuncs(L, functions, 0);
  lua_pop(L, 1);
}

static bool runFile(lua_State * L, std::string const& filename)
{
  const int error = luaL_loadfile(L, filename.c_str());
  if (error == LUA_ERRFILE)
    return false;

  int retCode = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (retCode != LUA_OK)
  {
    fprintf(stderr, "Lua (%s): %s\n", filename.c_str(), lua_tostring(L, -1));
    return false;
  }

  return true;
}

static bool runString(lua_State * L, std::string const& code)
{
  luaL_loadstring(L, code.c_str());
  int retCode = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (retCode != LUA_OK)
  {
    fprintf(stderr, "Lua: %s\n", lua_tostring(L, -1));
    return false;
  }

  return true;
}

#define ARG_COUNT(min) assertArgCount(L, min, min)
#define FUNC(name) static int name(lua_State * L)


// -- Document Functions --

FUNC(docCreateEmpty)
{
  ARG_COUNT(1);
  doc::createEmpty(Str(luaL_checkstring(L, 1)));

  return 0;
}

FUNC(docLoadFile)
{
  ARG_COUNT(1);

  const bool success = doc::load(Str(luaL_checkstring(L, 1)));
  lua_pushboolean(L, success);
  return 1;
}

FUNC(docLoadStr)
{
  ARG_COUNT(2);

  const bool success = doc::loadRaw(luaL_checkstring(L, 1), Str(luaL_checkstring(L, 2)));
  lua_pushboolean(L, success);
  return 1;
}

FUNC(docSave)
{
  ARG_COUNT(1);
  const bool success = doc::save(Str(luaL_checkstring(L, 1)));
  lua_pushboolean(L, success);
  return 1;
}

FUNC(docGetFilename)
{
  ARG_COUNT(0);
  lua_pushstring(L, doc::getFilename().utf8().c_str());
  return 1;
}

FUNC(docIncColumnWith)
{
  ARG_COUNT(1);
  doc::increaseColumnWidth(luaL_checkinteger(L, 1));
  return 0;
}

FUNC(docDecColumnWith)
{
  ARG_COUNT(1);
  doc::decreaseColumnWidth(luaL_checkinteger(L, 1));
  return 0;
}

FUNC(docAddColumn)
{
  ARG_COUNT(1);
  doc::addColumn(luaL_checkinteger(L, 1));
  return 0;
}

FUNC(docRemoveColumn)
{
  ARG_COUNT(1);
  doc::removeColumn(luaL_checkinteger(L, 1));
  return 0;
}

FUNC(docAddRow)
{
  ARG_COUNT(1);
  doc::addRow(luaL_checkinteger(L, 1));
  return 0;
}

FUNC(docRemoveRow)
{
  ARG_COUNT(1);
  doc::removeRow(luaL_checkinteger(L, 1));
  return 0;
}

FUNC(docRowToLabel)
{
  ARG_COUNT(1);
  lua_pushstring(L, doc::rowToLabel(luaL_checkinteger(L, 1)).utf8().c_str());
  return 1;
}

FUNC(docColumnToLabel)
{
  ARG_COUNT(1);
  lua_pushstring(L, doc::columnToLabel(luaL_checkinteger(L, 1)).utf8().c_str());
  return 1;
}

FUNC(docParseCellRef)
{
  ARG_COUNT(1);
  const Index idx = doc::parseCellRef(Str(luaL_checkstring(L, 1)));
  lua_pushinteger(L, idx.x);
  lua_pushinteger(L, idx.y);
  return 2;
}

FUNC(docGetColumnWidth)
{
  ARG_COUNT(1);
  lua_pushinteger(L, doc::getColumnWidth(luaL_checkinteger(L, 1)));
  return 1;
}

FUNC(docGetColumnCount)
{
  ARG_COUNT(0);
  lua_pushinteger(L, doc::getColumnCount());
  return 1;
}


FUNC(docGetRowCount)
{
  ARG_COUNT(0);
  lua_pushinteger(L, doc::getRowCount());
  return 1;
}

FUNC(docIsReadOnly)
{
  ARG_COUNT(0);
  lua_pushboolean(L, doc::isReadOnly());
  return 1;
}

// -- Application Functions --

FUNC(appGetCurorPos)
{
  ARG_COUNT(0);
  const Index idx = getCursorPos();
  lua_pushinteger(L, idx.x);
  lua_pushinteger(L, idx.y);
  return 2;
}

FUNC(appSetCursorPos)
{
  ARG_COUNT(2);
  setCursorPos(Index(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)));
  return 0;
}

FUNC(appGetYankBuffer)
{
  ARG_COUNT(0);
  lua_pushstring(L, getYankBuffer().utf8().c_str());
  return 1;
}

FUNC(appYankCurrentCell)
{
  ARG_COUNT(0);
  yankCurrentCell();
  return 0;
}

FUNC(appMoveLeft)
{
  ARG_COUNT(0);
  navigateLeft();
  return 0;
}

FUNC(appMoveRight)
{
  ARG_COUNT(0);
  navigateRight();
  return 0;
}

FUNC(appMoveUp)
{
  ARG_COUNT(0);
  navigateUp();
  return 0;
}

FUNC(appMoveDown)
{
  ARG_COUNT(0);
  navigateDown();
  return 0;
}

FUNC(appClearFlashMessage)
{
  ARG_COUNT(0);
  clearFlashMessage();
  return 0;
}

FUNC(appFlashMessage)
{
  ARG_COUNT(1);
  flashMessage(Str(luaL_checkstring(L, 1)));
  return 0;
}

FUNC(appEditCurrentCell)
{
  ARG_COUNT(0);
  editCurrentCell();
  return 0;
}

FUNC(appRegisterAppCommand)
{
  return 0;
}

FUNC(appRegisterEditCommand)
{
  return 0;
}

// -- Function Registrations --

static const luaL_Reg docFunctions[] = {
  {"create", docCreateEmpty},
  {"loadFile", docLoadFile},
  {"loadStr", docLoadStr},
  {"save", docSave},
  {"getFilename", docGetFilename},
  {"increaseColumnWidth", docIncColumnWith},
  {"decreaseColumnWidth", docDecColumnWith},
  {"addColumn", docAddColumn},
  {"removeColumn", docRemoveColumn},
  {"addRow", docAddRow},
  {"removeRow", docRemoveRow},
  {"rowToLabel", docRowToLabel},
  {"columnTolabel", docColumnToLabel},
  {"parseCellRef", docParseCellRef},
  {"getColumnWidth", docGetColumnWidth},
  {"getColumnCount", docGetColumnCount},
  {"getRowCount", docGetRowCount},
  {"isReadOnly", docIsReadOnly},
  {nullptr, nullptr}
};

static const luaL_Reg appFunctions[] = {
  {"getCursorPos", appGetCurorPos},
  {"setCursorPos", appSetCursorPos},
  {"getYankBuffer", appGetYankBuffer},
  {"yankCurrentCell", appYankCurrentCell},
  {"moveLeft", appMoveLeft},
  {"moveRight", appMoveRight},
  {"moveUp", appMoveUp},
  {"moveDown", appMoveDown},
  {"clearFlashMessage", appClearFlashMessage},
  {"flashMessage", appFlashMessage},
  {"editCurrentCell", appEditCurrentCell},
  {"registerAppCommand", appRegisterAppCommand},
  {"registerEditCommand", appRegisterEditCommand},
  {nullptr, nullptr}
};

static std::string CONFIG_FILE = "/.zumrc";
static lua_State * state_ = nullptr;

namespace script {

  bool initialize()
  {
    state_ = luaL_newstate();
    luaL_openlibs(state_);

    registerTable(state_, "doc", docFunctions);
    registerTable(state_, "app", appFunctions);

    // Run the default implementation
    if (!runString(state_, std::string(ScriptingLib_lua, ScriptingLib_lua + ScriptingLib_lua_len)))
      return false;

    // Try to load the config file
    char * home = getenv("HOME");
    if (home)
      runFile(state_, std::string(home) + CONFIG_FILE);
    else
      runFile(state_, "~" + CONFIG_FILE);

    return true;
  }

  void shutdown()
  {
    lua_close(state_);
    state_ = nullptr;
  }

}