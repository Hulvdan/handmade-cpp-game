cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

# === GLEW library ===
set(V_NAME glew)
set(V_GLEW_INCLUDE_PATH "vendor/libraries/glew-2.2.0/include/GL")
add_library(${V_NAME} STATIC IMPORTED)
set_target_properties(${V_NAME} PROPERTIES
    IMPORTED_LOCATION "../../vendor/libraries/glew-2.2.0/lib/Release/x64/glew32s.lib"
)
add_compile_definitions(${V_NAME} INTERFACE
    GLEW_STATIC
)

# === Building game.dll ===
set(V_NAME game)
add_library(${V_NAME} SHARED)
target_sources(${V_NAME} PUBLIC sources/game.h)
target_sources(${V_NAME} PRIVATE sources/game.cpp)
target_compile_definitions(${V_NAME}
    PRIVATE GAME_LIBRARY_BUILD
)
set_target_properties(${V_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

# === Building win32_platform.exe ===
set(V_NAME win32)
add_executable(${V_NAME} WIN32 sources/win32_platform.cpp)
target_compile_definitions(${V_NAME} PRIVATE
    BFG_INTERNAL
)
target_include_directories(${V_NAME} PUBLIC ${V_GLEW_INCLUDE_PATH})
target_link_libraries(${V_NAME} PUBLIC user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
target_link_libraries(${V_NAME} PRIVATE glew)

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
