@echo off
if not exist build (
    mkdir build;
)
pushd build

echo Compiling...
zig c++ -g ..\sources\main.cpp -o main.exe user32.lib gdi32.lib

if %errorlevel% neq 0 (
    echo.
    echo Compilation [31mFailed[0m

    popd
    exit /b %errorlevel%
)

echo.
echo Compilation [32mSucceeded[0m

popd

