cmake_minimum_required(VERSION 3.27)

include(GenerateExportHeader)

project(game LANGUAGES CXX)

# === Building game.dll ===
add_library(game SHARED)
target_sources(game PUBLIC sources/game.h)
target_sources(game PRIVATE sources/game.cpp)
target_compile_definitions(game PRIVATE GAME_LIBRARY_BUILD)
set_target_properties(game
    PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
)

# === Building win32_platform.exe ===
add_executable(win32 WIN32 sources/win32_platform.cpp)
target_compile_definitions(win32 PRIVATE BFG_INTERNAL)
target_link_libraries(win32 PUBLIC user32.lib gdi32.lib)
