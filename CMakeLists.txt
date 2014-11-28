
cmake_minimum_required(VERSION 2.8)
project(zum C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src
                    ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty/termbox)

set(SOURCE
    src/zum.c
    src/str.c
    src/editor.c
    src/document.c
    src/commands.c
    src/3rdparty/termbox/termbox.c
    src/3rdparty/termbox/utf8.c
)

set(CMAKE_C_FLAGS "-std=gnu99")

add_executable(zum ${SOURCE})
target_link_libraries(zum)
