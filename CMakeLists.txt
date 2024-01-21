cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# === glm library ===
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        bf71a834948186f4097caa076cd2663c69a10e1e # 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# === GLEW library ===
add_library(glew STATIC IMPORTED)
set_target_properties(glew PROPERTIES
    IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/lib/Release/x64/glew32s.lib"
)
target_include_directories(glew INTERFACE "${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/include/GL")
target_compile_definitions(glew INTERFACE GLEW_STATIC)

# === Building bf_game.dll ===
add_library(bf_game SHARED)
target_sources(bf_game PUBLIC sources/bf_game.h)
target_sources(bf_game PRIVATE sources/bf_game.cpp)
target_compile_definitions(bf_game
    PRIVATE GAME_LIBRARY_BUILD
)
set_target_properties(bf_game PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)
# TODO(hulvdan): How to decouple from windows OpenGL?
target_link_libraries(bf_game PRIVATE glm glew glu32.lib opengl32.lib)

# === Building win32_platform.exe ===
add_executable(win32 WIN32 sources/win32_platform.cpp)
target_compile_definitions(win32 PRIVATE
    BFG_INTERNAL
)
target_link_libraries(win32 PRIVATE user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
target_link_libraries(win32 PRIVATE glew glm)

# === googletest library ===
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# === Tests ===
add_executable(tests tests/tests.cpp)
target_include_directories(tests PRIVATE "${PROJECT_SOURCE_DIR}/sources")
target_link_libraries(tests GTest::gtest_main glm)
include(GoogleTest)
gtest_discover_tests(tests)

# === Cleaning Up Stale Files ===
# Kinda related information:
# - Casey Muratori: MSVC PDBs Are Filled With Stale Debug Info:
#   https://www.computerenhance.com/p/msvc-pdbs-are-filled-with-stale-debug
IF (WIN32)
    # NOTE(hulvdan): Visual Studio won't let us delete old bf_game.dll PDB files.
    # This is why we are creating PDBs with timestamped names.
    string(TIMESTAMP V_TIMESTAMP "%Y%m%d-%H%M%S" UTC)
    set_target_properties(bf_game PROPERTIES
        PDB_NAME bf_game_${V_TIMESTAMP}
    )
    file(GLOB files_to_delete
        ${CMAKE_BINARY_DIR}/Debug/bf_game_*.pdb
        ${CMAKE_BINARY_DIR}/Debug/bf_game_*.dll
    )
    list(LENGTH files_to_delete files_to_delete_length)
    if (files_to_delete_length GREATER 0)
        file(REMOVE ${files_to_delete})
    endif()
ENDIF()

# === Enabling Linting On Win32 ===
if(WIN32 AND CMAKE_GENERATOR STREQUAL Ninja)
    include_directories("${PROJECT_SOURCE_DIR}/.cmake/ninja/_deps/glm-src")
    include_directories("${PROJECT_SOURCE_DIR}/.cmake/ninja/_deps/googletest-src/googletest/include")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/include/GL")
endif()

# === Other Win32 Bullshiet ===
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT win32)
# Now VS will be able to load generated dll file if opened cmake-generated sln file.
set_target_properties(win32 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:win32>"
    VS_DEBUGGER_COMMAND           "$<TARGET_FILE:win32>"
    VS_DEBUGGER_ENVIRONMENT       "PATH=%PATH%;${CMAKE_PREFIX_PATH}/bin")
