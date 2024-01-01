@echo off

cmake ^
    -D CMAKE_CXX_COMPILER=cl ^
    -D CMAKE_C_COMPILER=cl ^
    -G Ninja ^
    -B .cmake\ninja ^
    --log-level=ERROR ^
    && "C:/Program Files/LLVM/bin/clang-tidy.exe" sources\main.cpp -checks=llvm-include-order
