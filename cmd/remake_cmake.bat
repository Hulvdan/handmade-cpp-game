@echo off

pushd %0\..\..

echo [32mINFO: Running remake_cmake.bat...[0m

echo.
echo [33mINFO: Removing .cmake directory...[0m
rmdir /S /Q .cmake
if %errorlevel% neq 0 (
    echo.
    echo [31mFailed[0m
    echo Remaking CMake files [31mFailed[0m

    popd
    exit /b %errorlevel%
)

set CC=cl
set CXX=cl

echo.
echo [33mINFO: Remaking CMake files for Visual Studio...[0m
cmake -G "Visual Studio 17 2022" -B .cmake\vs17 -DCMAKE_UNITY_BUILD=ON
if %errorlevel% neq 0 (
    echo.
    echo Remaking CMake files [31mFailed[0m
    echo Could not create Visual Studio files

    popd
    exit /b %errorlevel%
)

echo.
echo [33mINFO: Remaking CMake files for Ninja...[0m
cmake -G Ninja -B .cmake\ninja -DCMAKE_UNITY_BUILD=ON
if %errorlevel% neq 0 (
    echo.
    echo Remaking CMake files [31mFailed[0m
    echo Could not create Ninja files

    popd
    exit /b %errorlevel%
)

echo.
echo [33mINFO: Remaking compile_commands.json...[0m
ninja -C .cmake\ninja -f build.ninja -t compdb > compile_commands.json
if %errorlevel% neq 0 (
    echo.
    echo Remaking CMake files [31mFailed[0m
    echo Could not create compile_command.json

    popd
    exit /b %errorlevel%
)

echo.
echo INFO: Remaking CMake files [32mSucceeded[0m

popd
