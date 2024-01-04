@echo off

pushd %0\..\..

echo [32mINFO: Running run.bat...[0m

echo.
echo [33mINFO: Running .cmake\vs17\Debug\game.exe...[0m
.cmake\vs17\Debug\game.exe
if %errorlevel% neq 0 (
    echo.
    echo Running .cmake\vs17\Debug\game.exe [31mFailed[0m

    popd
    exit /b %errorlevel%
)

echo Running run.bat [31mSucceeded[0m
popd
