@echo off
if not exist build (
    mkdir build;
)
pushd build

echo Compiling...
zig c++ -g ..\sources\main.cpp -o out.exe User32.lib
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Compilation [31mFailed[0m

    popd
    exit %ERRORLEVEL%
)

echo.
echo Compilation [32mSucceeded[0m

popd

