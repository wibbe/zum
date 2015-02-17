
cmake_minimum_required(VERSION 2.8)
project(zum)

# Check for and enable C++11 support
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
  message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src
                    ${CMAKE_CURRENT_BINARY_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/termbox
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/lua)

set(SOURCE
    src/Zum.cpp
    src/Str.cpp
    src/Types.cpp
    src/Editor.cpp
    src/Document.cpp
    src/Commands.cpp
    src/Help.cpp
    src/Tokenizer.cpp
    src/Expression.cpp
    src/Scripting.cpp
    src/Variable.cpp
    src/3rdparty/termbox/termbox.c
    src/3rdparty/termbox/utf8.c
    src/3rdparty/lua/lapi.c
    src/3rdparty/lua/lcode.c
    src/3rdparty/lua/lctype.c
    src/3rdparty/lua/ldebug.c
    src/3rdparty/lua/ldo.c
    src/3rdparty/lua/ldump.c
    src/3rdparty/lua/lfunc.c
    src/3rdparty/lua/lgc.c
    src/3rdparty/lua/llex.c
    src/3rdparty/lua/lmem.c
    src/3rdparty/lua/lobject.c
    src/3rdparty/lua/lopcodes.c
    src/3rdparty/lua/lparser.c
    src/3rdparty/lua/lstate.c
    src/3rdparty/lua/lstring.c
    src/3rdparty/lua/ltable.c
    src/3rdparty/lua/ltm.c
    src/3rdparty/lua/lundump.c
    src/3rdparty/lua/lvm.c
    src/3rdparty/lua/lzio.c
    src/3rdparty/lua/lauxlib.c
    src/3rdparty/lua/lbaselib.c
    src/3rdparty/lua/lbitlib.c
    src/3rdparty/lua/lcorolib.c
    src/3rdparty/lua/ldblib.c
    src/3rdparty/lua/liolib.c
    src/3rdparty/lua/lmathlib.c
    src/3rdparty/lua/loslib.c
    src/3rdparty/lua/lstrlib.c
    src/3rdparty/lua/ltablib.c
    src/3rdparty/lua/loadlib.c
    src/3rdparty/lua/linit.c
    ${CMAKE_CURRENT_BINARY_DIR}/ScriptingLib.lua.h
)

add_custom_command(OUTPUT ScriptingLib.lua.h COMMAND xxd -i ScriptingLib.lua > "${CMAKE_CURRENT_BINARY_DIR}/ScriptingLib.lua.h"
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ScriptingLib.lua")

add_executable(zum ${SOURCE})
target_link_libraries(zum)

