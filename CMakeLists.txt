cmake_minimum_required(VERSION 3.27)

include(GenerateExportHeader)

project(game)

add_executable(win32 WIN32 sources/win32_platform.cpp)
add_library(game SHARED sources/game.cpp)
generate_export_header(game)

target_link_libraries(win32 user32.lib gdi32.lib)
