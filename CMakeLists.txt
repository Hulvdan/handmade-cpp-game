cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

# === GLEW library ===
set(V_GLEW_INCLUDE_PATH "${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/include/GL")
add_library(glew STATIC IMPORTED)
set_target_properties(glew PROPERTIES
    IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/lib/Release/x64/glew32s.lib"
)
add_compile_definitions(glew INTERFACE GLEW_STATIC)

# === Building game.dll ===
add_library(game SHARED)
target_sources(game PUBLIC sources/game.h)
target_sources(game PRIVATE sources/game.cpp)
target_compile_definitions(game
    PRIVATE GAME_LIBRARY_BUILD
)
set_target_properties(game PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

# === Building win32_platform.exe ===
add_executable(win32 WIN32 sources/win32_platform.cpp)
target_compile_definitions(win32 PRIVATE
    BFG_INTERNAL
)
target_include_directories(win32 PUBLIC ${V_GLEW_INCLUDE_PATH})
target_link_libraries(win32 PUBLIC user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
target_link_libraries(win32 PRIVATE glew)

# === Cleaning Up Stale Files ===
# Kinda related information:
# - Casey Muratori: MSVC PDBs Are Filled With Stale Debug Info:
#   https://www.computerenhance.com/p/msvc-pdbs-are-filled-with-stale-debug
IF (WIN32)
    # NOTE(hulvdan): Visual Studio won't let us delete old game.dll PDB files.
    # This is why we are creating PDBs with timestamped names.
    string(TIMESTAMP V_TIMESTAMP "%Y%m%d-%H%M%S" UTC)
    set_target_properties(game PROPERTIES
        PDB_NAME game_${V_TIMESTAMP}
    )

    file(GLOB files_to_delete
        ${CMAKE_BINARY_DIR}/Debug/game_*.pdb
        ${CMAKE_BINARY_DIR}/Debug/game_*.dll
    )
    list(LENGTH files_to_delete files_to_delete_length)
    if (files_to_delete_length GREATER 0)
        file(REMOVE ${files_to_delete})
    endif()
ENDIF()
