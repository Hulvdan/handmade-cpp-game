@echo off

pushd %0\..\..

cmake ^
    -D CMAKE_CXX_COMPILER=cl ^
    -D CMAKE_C_COMPILER=cl ^
    -G Ninja ^
    -B .cmake\ninja ^
    --log-level=ERROR ^
    && "C:/Program Files/LLVM/bin/clang-tidy.exe" sources\win32_platform.cpp sources\bf_game.cpp -checks=llvm-include-order

popd
