@echo off

pushd %0\..\..

if [%1]==[] (
    "C:/Program Files/LLVM/bin/clang-format.exe" -i sources\*.cpp sources\*.h
) else (
    "C:/Program Files/LLVM/bin/clang-format.exe" -i "%1"
)

popd
