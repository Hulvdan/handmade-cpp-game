@echo off

pushd %0\..\..

set CLICOLOR_FORCE=1
cmake ^
    -D CMAKE_CXX_COMPILER=cl ^
    -D CMAKE_C_COMPILER=cl ^
    -G Ninja ^
    -B .cmake\ninja ^
    --log-level=ERROR ^
    && cls && "C:/Program Files/LLVM/bin/clang-tidy.exe" --use-color sources\win32_platform.cpp sources\bf_game.cpp | sed "s/C:\\\Users\\\user\\\dev\\\home\\\handmade-cpp-game\\\//"
    REM && cls && "C:/Program Files/LLVM/bin/clang-tidy.exe" sources\win32_platform.cpp sources\bf_game.cpp

popd
