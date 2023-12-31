@echo off

REM TODO(hulvdan): Display the duration of the build time in the end

pushd %0\..\..

echo [32mINFO: Running build.bat...[0m

echo.
echo [33mINFO: Remaking CMake files...[0m
cmake ^
    -G "Visual Studio 17 2022" ^
    -B .cmake\vs17 ^
    -Wdev ^
    -Wdeprecated ^
    -Werror=dev ^
    -Werror-deprecated ^
    --log-level=WARNING

if %errorlevel% neq 0 (
    echo.
    echo Remaking CMake files [31mFailed[0m

    popd
    exit /b %errorlevel%
)

echo.
echo [33mINFO: Compiling...[0m
MSBuild .cmake\vs17\game.sln -v:minimal

if %errorlevel% neq 0 (
    echo.
    echo Compilation [31mFailed[0m

    popd
    exit /b %errorlevel%
)

echo.
echo Compilation [32mSucceeded[0m

popd
