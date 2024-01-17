cmake_minimum_required(VERSION 3.27)

include(ExternalProject)

project(game LANGUAGES CXX)

# === GLEW library ===
# set(V_NAME glew)
# set(V_GLEW_INCLUDE_PATH "vendor/libraries/glew-2.2.0/include/GL")
# add_library(${V_NAME} STATIC IMPORTED)
# add_compile_definitions(GLEW_STATIC)
# set_target_properties(${V_NAME} PROPERTIES
#     IMPORTED_LOCATION "../../vendor/libraries/glew-2.2.0/lib/Release/x64/glew32.lib"
# )

set(V_NAME glew)
set(V_GLEW_INCLUDE_PATH "vendor/libraries/glew-2.2.0/include/GL")
add_library(${V_NAME} STATIC IMPORTED)
add_compile_definitions(GLEW_STATIC)
set_target_properties(${V_NAME} PROPERTIES
    IMPORTED_LOCATION "../../vendor/libraries/glew-2.2.0/lib/Release/x64/glew32s.lib"
)

# set(V_NAME glew)
# set(V_GLEW_INCLUDE_PATH "vendor/libraries/glew-2.2.0/include/GL")
# set(V_GLEW_SRC_PATH "vendor/libraries/glew-2.2.0/src/")
# add_library(${V_NAME} STATIC)
# target_compile_definitions(${V_NAME}
#     PUBLIC GLEW_STATIC _WIN64
# )
# add_compile_definitions(GLEW_STATIC _WIN32 _WIN64)
# target_sources(${V_NAME} PUBLIC ${V_GLEW_INCLUDE_PATH}/glew.h)
# target_sources(${V_NAME} PRIVATE ${V_GLEW_SRC_PATH}/glew.c)
# target_sources(${V_NAME} PRIVATE ${V_GLEW_SRC_PATH}/glewinfo.c)
# target_sources(${V_NAME} PRIVATE ${V_GLEW_SRC_PATH}/visualinfo.c)
# set_target_properties(${V_NAME} PROPERTIES
#     LANGUAGE C
#     LINKER_LANGUAGE C
#     # C_VISIBILITY_PRESET hidden
#     # CXX_VISIBILITY_PRESET hidden
# )

# set(V_NAME glew)
# ExternalProject_Add(${V_NAME}
#     GIT_REPOSITORY    git@github.com:nigels-com/glew.git
#     GIT_TAG           glew-2.2.0
#     PREFIX            ${CMAKE_CURRENT_BINARY_DIR}/vendor/libraries/glew
#     BUILD_COMMAND     cmake make
# )

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
    # GLEW_STATIC
)
# target_compile_definitions(${V_NAME} PUBLIC
#     GLEW_STATIC
# )
# add_compile_definitions(GLEW_STATIC)
target_include_directories(${V_NAME} PUBLIC ${V_GLEW_INCLUDE_PATH})
target_link_libraries(${V_NAME} PUBLIC user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
# target_link_libraries(${V_NAME} PUBLIC ../../vendor/libraries/glew-2.2.0/lib/Release/x64/glew32s)
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
