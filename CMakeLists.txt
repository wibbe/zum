
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
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/termbox)

set(SOURCE
    src/Zum.cpp
    src/Str.cpp
    src/Types.cpp
    src/Editor.cpp
    src/Document.cpp
    src/Commands.cpp
    src/Help.cpp
    src/Expression.cpp
    src/3rdparty/termbox/termbox.c
    src/3rdparty/termbox/utf8.c
)

set(CMAKE_C_FLAGS "-std=gnu99")

add_executable(zum ${SOURCE})
target_link_libraries(zum)
