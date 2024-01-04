@echo off

pushd %0\..\..

"C:/Program Files/LLVM/bin/clang-format.exe" -i sources\*.cpp

popd
