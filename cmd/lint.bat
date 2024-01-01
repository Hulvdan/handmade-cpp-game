@echo off

REM set CC="cl"
REM set CXX="cl"

REM echo baboasba:20:1: warning: function 'WinMain' has a definition with different parameter names [readability-inconsistent-declaration-parameter-name]

REM cmake ^
REM     -D CMAKE_CXX_COMPILER=cl ^
REM     -D CMAKE_C_COMPILER=cl ^
REM     -G Ninja ^
REM     -B .cmake\ninja ^
REM     --log-level=ERROR ^
REM     &&
"C:/Program Files/LLVM/bin/clang-tidy.exe" sources\main.cpp -checks=llvm-include-order
