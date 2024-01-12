@echo off

pushd %0\..\..\.cmake\vs17\Debug\

echo [32mINFO: Running run.bat...[0m

echo.
echo [33mINFO: Running .cmake\vs17\Debug\win32.exe...[0m
win32.exe
if %errorlevel% neq 0 (
    echo.
    echo Running .cmake\vs17\Debug\win32.exe [31mFailed[0m

    popd
    exit /b %errorlevel%
)

echo.
echo Running run.bat [32mSucceeded[0m
popd
