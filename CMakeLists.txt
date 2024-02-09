cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/vendor/libraries/imgui-1.90.1")

# === glm library ===
add_subdirectory(vendor/libraries/glm-0.9.9.8)

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
target_compile_definitions(bf_game PRIVATE
    GAME_LIBRARY_BUILD
    BF_CLIENT
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM
    IMGUI_DEFINE_MATH_OPERATORS
)
target_include_directories(bf_game PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)
target_sources(bf_game PRIVATE
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)
# TODO(hulvdan): How to decouple from windows OpenGL?
target_link_libraries(bf_game PRIVATE glm glew glu32.lib opengl32.lib)

# === Building win32_platform.exe ===
add_executable(win32 WIN32 sources/win32_platform.cpp)
target_compile_definitions(win32 PRIVATE
    BFG_INTERNAL
    BF_CLIENT
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM
    IMGUI_DEFINE_MATH_OPERATORS
)
target_include_directories(win32 PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)
target_sources(win32 PRIVATE
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)
target_link_libraries(win32 PRIVATE user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
target_link_libraries(win32 PRIVATE glew glm)

# === Building win32_noise_sandbox.exe ===
# add_executable(win32_noise_sandbox sources/win32_noise_sandbox.cpp)
# target_compile_definitions(win32_noise_sandbox PRIVATE
#     IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#     IMGUI_DEFINE_MATH_OPERATORS
# )
# target_include_directories(win32_noise_sandbox PRIVATE
#     "${IMGUI_DIR}"
#     "${IMGUI_DIR}/backends"
# )
# target_sources(win32_noise_sandbox PRIVATE
#     "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
#     "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
#     "${IMGUI_DIR}/imgui.cpp"
#     "${IMGUI_DIR}/imgui_demo.cpp"
#     "${IMGUI_DIR}/imgui_draw.cpp"
#     "${IMGUI_DIR}/imgui_tables.cpp"
#     "${IMGUI_DIR}/imgui_widgets.cpp"
# )
# target_compile_definitions(win32_noise_sandbox PRIVATE
#     BF_CLIENT
# )
# target_link_libraries(win32_noise_sandbox PRIVATE user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
# target_link_libraries(win32_noise_sandbox PRIVATE glew glm)

# === Building meta_tools.exe ===
# add_executable(meta_tools sources/meta_tools.cpp)
# target_compile_definitions(win32 PRIVATE
#     BFG_INTERNAL
# )
# target_link_libraries(win32 PRIVATE user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
# target_link_libraries(win32 PRIVATE glew glm)

# === Tests ===
add_executable(tests tests/tests.cpp)
target_include_directories(tests PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)
target_compile_definitions(tests PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_CUSTOM
    IMGUI_DEFINE_MATH_OPERATORS
)
target_sources(tests PRIVATE
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)
target_link_libraries(tests PRIVATE opengl32.lib)
target_link_libraries(tests PRIVATE glew glm)
target_include_directories(tests PRIVATE "${PROJECT_SOURCE_DIR}/sources")
target_include_directories(tests PRIVATE "${PROJECT_SOURCE_DIR}/vendor/libraries/doctest")

set(COMPILATION_FLAGS ${COMPILATION_FLAGS} /fsanitize=undefined /fsanitize=address)
target_compile_options(bf_game PRIVATE ${COMPILATION_FLAGS})
target_compile_options(win32 PRIVATE ${COMPILATION_FLAGS})
target_compile_options(tests PRIVATE ${COMPILATION_FLAGS})

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
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/glm-0.9.9.8")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/include/GL")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/doctest")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/imgui-1.90.1")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/imgui-1.90.1/backends")
endif()

# === Other Win32 Bullshiet ===
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT win32)
# Now VS will be able to load generated dll file if opened cmake-generated sln file.
set_target_properties(win32 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:win32>"
    VS_DEBUGGER_COMMAND           "$<TARGET_FILE:win32>"
    VS_DEBUGGER_ENVIRONMENT       "PATH=%PATH%;${CMAKE_PREFIX_PATH}/bin"
)
