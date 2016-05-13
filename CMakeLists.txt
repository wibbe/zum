
cmake_minimum_required(VERSION 2.8)
project(zum)

option(ConsoleApp "Build a console application using Termbox" OFF)

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
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/stb
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/tigr
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/sqlite
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/nativefiledialog
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/bx/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/jimtcl)

set(PLATFORM_SOURCE)

# Compiler compability
if(UNIX)
  if(APPLE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/bx/include/compat/osx)
    set(PLATFORM_LIBRARIES OpenGL)

    set(PLATFORM_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/nativefiledialog/nfd_cocoa.m)
  else()
    set(PLATFORM_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/nativefiledialog/nfd_gtk.c
    )
  endif()
elseif(WIN32)
  set(PLATFORM_LIBRARIES d3d9)

  set(PLATFORM_SOURCE
      ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/nativefiledialog/nfd_win32.c
  )
  if(MINGW)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/bx/include/compat/mingw)
  elseif (MSVC)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/bx/include/compat/msvc)
  endif()
endif()

set(BIN2C_SOURCE
    src/3rdparty/bx/bin2c/bin2c.cpp
)

set(SQLITE3_SOURCE
    src/3rdparty/sqlite/shell.c
    src/3rdparty/sqlite/sqlite3.c
)

set (TIGR_SOURCE
     src/3rdparty/tigr/tigr.c
)


set(ZUM_SOURCE
    src/Zum.cpp
    src/Str.cpp
    src/Cell.cpp
    src/Editor.cpp
    src/Document.cpp
    src/Commands.cpp
    src/Help.cpp
    src/Tokenizer.cpp
    src/Expression.cpp
    src/Tcl.cpp
    src/MurmurHash.cpp
    src/Completion.cpp
    src/Log.cpp
    src/Index.cpp
    src/3rdparty/jimtcl/jim.c
    src/3rdparty/jimtcl/jim-subcmd.c
    src/3rdparty/jimtcl/jim-win32compat.c
    src/3rdparty/jimtcl/jim-clock.c
    src/3rdparty/jimtcl/jim-regexp.c
    src/3rdparty/jimtcl/jim-format.c
    src/3rdparty/jimtcl/jimregexp.c
    src/3rdparty/jimtcl/utf8.c
    src/3rdparty/termbox/utf8.c
    src/3rdparty/nativefiledialog/nfd_common.c
    src/3rdparty/stb/stb_truetype.c
    src/3rdparty/sqlite/sqlite3.c
    ${CMAKE_CURRENT_BINARY_DIR}/ScriptingLib.tcl.h
    ${CMAKE_CURRENT_BINARY_DIR}/UbuntuMono.ttf.h
    ${PLATFORM_SOURCE}
)

if(${ConsoleApp})
  set(ZUM_SOURCE
      ${ZUM_SOURCE}
      src/ViewTermbox.cpp
      src/3rdparty/termbox/termbox.c
  )

  set(ZUM_LIBRARIES)
  set(ZUM_TYPE)
else()
  set(ZUM_SOURCE
      ${ZUM_SOURCE}
      ${TIGR_SOURCE}
      src/ViewTigr.cpp
  )

  set(ZUM_LIBRARIES ${PLATFORM_LIBRARIES})
  set(ZUM_TYPE WIN32)
endif()

# Add some sqlite3 definitions
add_definitions(-DSQLITE_ENABLE_FTS4 -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION)

add_custom_command(OUTPUT ScriptingLib.tcl.h COMMAND "${CMAKE_CURRENT_BINARY_DIR}/bin2c" -f ScriptingLib.tcl -o "${CMAKE_CURRENT_BINARY_DIR}/ScriptingLib.tcl.h" -n ScriptingLib
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ScriptingLib.tcl" bin2c)

add_custom_command(OUTPUT UbuntuMono.ttf.h COMMAND "${CMAKE_CURRENT_BINARY_DIR}/bin2c" -f UbuntuMono.ttf -o "${CMAKE_CURRENT_BINARY_DIR}/UbuntuMono.ttf.h" -n UbuntuMono
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/UbuntuMono.ttf" bin2c)

add_executable(bin2c ${BIN2C_SOURCE})
target_link_libraries(bin2c)

add_executable(sqlite3 ${SQLITE3_SOURCE})
target_link_libraries(sqlite3)

add_executable(zum ${ZUM_TYPE} ${ZUM_SOURCE})
target_link_libraries(zum ${ZUM_LIBRARIES})
