cmake_minimum_required(VERSION 3.27)

project(game)

add_executable(game WIN32 sources/main.cpp)
target_link_libraries(game user32.lib gdi32.lib)

