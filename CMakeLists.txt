# SHIT: Oh, god, I hate this shit
cmake_minimum_required(VERSION 3.27)

project(game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR "${PROJECT_SOURCE_DIR}/vendor/libraries/imgui-1.90.1")
set(FLATBUFFERS_DIR "${PROJECT_SOURCE_DIR}/vendor/libraries/flatbuffers-24.3.25")

add_compile_definitions(
    BF_CLIENT=1
    BF_SERVER=0

    PROFILING
    TRACY_ENABLE
    # TRACY_ON_DEMAND
)

add_compile_definitions("$<$<CONFIG:Debug>:BF_DEBUG=1>")
add_compile_definitions("$<$<NOT:$<CONFIG:Debug>>:BF_DEBUG=0>")

#-----------------------------------------------------------------------------------
# glm library.
#-----------------------------------------------------------------------------------
add_subdirectory(vendor/libraries/glm-0.9.9.8)

#-----------------------------------------------------------------------------------
# GLEW library.
#-----------------------------------------------------------------------------------
add_library(glew STATIC IMPORTED)
set_target_properties(glew PROPERTIES
    IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/lib/Release/x64/glew32s.lib"
)
target_include_directories(glew INTERFACE "${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/include/GL")
target_compile_definitions(glew INTERFACE GLEW_STATIC)

#-----------------------------------------------------------------------------------
# Tracy Profiler as Dll.
#-----------------------------------------------------------------------------------
add_library(tracy SHARED)
target_include_directories(tracy PUBLIC "${PROJECT_SOURCE_DIR}/vendor/tracy/public")
target_compile_definitions(tracy PRIVATE TRACY_EXPORTS)
target_compile_definitions(tracy INTERFACE TRACY_IMPORTS)
target_sources(tracy PRIVATE
    "${PROJECT_SOURCE_DIR}/vendor/tracy/public/TracyClient.cpp"
)

#-----------------------------------------------------------------------------------
# bf_game.
#-----------------------------------------------------------------------------------
add_library(bf_game SHARED)
target_sources(bf_game PUBLIC sources/bf_game.h)
target_sources(bf_game PRIVATE sources/bf_game.cpp)
target_compile_definitions(bf_game PRIVATE
    GAME_LIBRARY_BUILD=1

    IMGUI_IMPL_OPENGL_LOADER_CUSTOM
    IMGUI_DEFINE_MATH_OPERATORS
)

IF(WIN32)
    target_compile_definitions(bf_game PRIVATE
        _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
    )
ENDIf()

target_include_directories(bf_game PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${FLATBUFFERS_DIR}/include"
    "${PROJECT_SOURCE_DIR}/codegen/flatbuffers"
)
target_sources(bf_game PRIVATE
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)
# TODO: How to decouple from windows OpenGL?
target_link_libraries(bf_game PRIVATE glm glew glu32.lib opengl32.lib tracy)

#-----------------------------------------------------------------------------------
# Building win32.
#-----------------------------------------------------------------------------------
add_executable(win32 WIN32 sources/win32_platform.cpp)
target_compile_definitions(win32 PRIVATE
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
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
)
target_link_libraries(win32 PRIVATE user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
target_link_libraries(win32 PRIVATE glew glm tracy)
add_dependencies(win32 bf_game)

#-----------------------------------------------------------------------------------
# Building win32_noise_sandbox.
#-----------------------------------------------------------------------------------
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
# target_link_libraries(win32_noise_sandbox PRIVATE user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
# target_link_libraries(win32_noise_sandbox PRIVATE glew glm)

#-----------------------------------------------------------------------------------
# Building meta_tools.
#-----------------------------------------------------------------------------------
# add_executable(meta_tools sources/meta_tools.cpp)
# target_link_libraries(win32 PRIVATE user32.lib gdi32.lib winmm.lib glu32.lib opengl32.lib)
# target_link_libraries(win32 PRIVATE glew glm)

add_custom_command(
    TARGET win32 POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E
        copy_directory_if_different ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:win32>/resources
    DEPENDS win32)

#-----------------------------------------------------------------------------------
# Tests.
#-----------------------------------------------------------------------------------
add_executable(tests sources/tests.cpp)
target_include_directories(tests PRIVATE
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
    "${FLATBUFFERS_DIR}/include"
    "${PROJECT_SOURCE_DIR}/codegen/flatbuffers"
)
target_compile_definitions(tests PRIVATE
    GAME_LIBRARY_BUILD=1
    TESTS=1

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
target_link_libraries(tests PRIVATE glew glm tracy)
target_include_directories(tests PRIVATE "${PROJECT_SOURCE_DIR}/sources")
target_include_directories(tests PRIVATE "${PROJECT_SOURCE_DIR}/vendor/libraries/doctest")

IF(WIN32 AND NOT CMAKE_GENERATOR STREQUAL Ninja)
    # set(COMPILATION_FLAGS ${COMPILATION_FLAGS} /fsanitize=address)
    target_compile_options(bf_game PRIVATE ${COMPILATION_FLAGS})
    target_compile_options(win32 PRIVATE ${COMPILATION_FLAGS})
    target_compile_options(tests PRIVATE ${COMPILATION_FLAGS})
ENDIF()

#-----------------------------------------------------------------------------------
# Cleaning Up Stale Files.
#-----------------------------------------------------------------------------------
# - Casey Muratori: MSVC PDBs Are Filled With Stale Debug Info:
#   https://www.computerenhance.com/p/msvc-pdbs-are-filled-with-stale-debug
IF ((WIN32) AND (NOT CMAKE_CONFIGURATION_TYPES STREQUAL "RelWithDebInfo"))
    # NOTE: Visual Studio won't let us delete old bf_game.dll PDB files.
    # This is why we are creating PDBs with timestamped names.
    string(TIMESTAMP V_TIMESTAMP "%Y%m%d-%H%M%S" UTC)
    set_target_properties(bf_game PROPERTIES
        PDB_NAME bf_game_${V_TIMESTAMP}
    )
    file(GLOB files_to_delete
        ${CMAKE_BINARY_DIR}/Debug/bf_game_*.pdb
        ${CMAKE_BINARY_DIR}/Debug/game_*.dll
    )
    list(LENGTH files_to_delete files_to_delete_length)
    IF (files_to_delete_length GREATER 0)
        file(REMOVE ${files_to_delete})
    ENDIF()
ENDIF()

#-----------------------------------------------------------------------------------
# Enabling Linting On Win32.
#-----------------------------------------------------------------------------------
IF(WIN32 AND CMAKE_GENERATOR STREQUAL Ninja)
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/glm-0.9.9.8")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/glew-2.2.0/include/GL")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/doctest")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/imgui-1.90.1")
    include_directories("${PROJECT_SOURCE_DIR}/vendor/libraries/imgui-1.90.1/backends")
ENDIF()

#-----------------------------------------------------------------------------------
# Other Win32 Bullshiet.
#-----------------------------------------------------------------------------------
# Disabling incremental linking warning on win32.
IF(MSVC)
    set_target_properties(bf_game PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
    set_target_properties(win32 PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
    set_target_properties(tests PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
ENDIF()

# Setting win32 as a startup project.
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT win32)

# Now VS will be able to load generated dll file if opened cmake-generated sln file.
set_target_properties(win32 PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:win32>"
    VS_DEBUGGER_COMMAND           "$<TARGET_FILE:win32>"
    VS_DEBUGGER_ENVIRONMENT       "PATH=%PATH%;${CMAKE_PREFIX_PATH}/bin"
)
