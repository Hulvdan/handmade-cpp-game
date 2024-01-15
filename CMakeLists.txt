cmake_minimum_required(VERSION 3.27)

include(GenerateExportHeader)

project(game LANGUAGES CXX)

# === Building game.dll ===
add_library(game SHARED)

target_sources(game PUBLIC sources/game.h)
target_sources(game PRIVATE sources/game.cpp)
target_compile_definitions(game PRIVATE GAME_LIBRARY_BUILD)

set_target_properties(game PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

IF (WIN32)
    string(TIMESTAMP timestamp "%Y%m%d-%H%M%S" UTC)
    set_target_properties(game PROPERTIES
        PDB_NAME game_${timestamp}
    )

    # Cleaning up old pdbs and dlls
    file(GLOB files_to_delete
        ${CMAKE_BINARY_DIR}/Debug/game_*.pdb
        ${CMAKE_BINARY_DIR}/Debug/game_*.dll
    )
    list(LENGTH files_to_delete files_to_delete_length)
    if (files_to_delete_length GREATER 0)
        file(REMOVE ${files_to_delete})
    endif()
ENDIF()

# === Building win32_platform.exe ===
add_executable(win32 WIN32 sources/win32_platform.cpp)
target_compile_definitions(win32 PRIVATE BFG_INTERNAL)
target_link_libraries(win32 PUBLIC user32.lib gdi32.lib winmm.lib Opengl32.lib)
