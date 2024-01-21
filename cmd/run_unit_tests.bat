@echo off

cls

pushd %0\..\..\.cmake\vs17\Debug\

echo [32mINFO: Running run_unit_tests.bat...[0m

echo.
echo [33mINFO: Running .cmake\vs17\Debug\tests.exe...[0m
tests.exe
if %errorlevel% neq 0 (
    echo.
    echo Running .cmake\vs17\Debug\tests.exe [31mFailed[0m

    popd
    exit /b %errorlevel%
)

echo.
echo Running run_unit_tests.bat [32mSucceeded[0m
popd
