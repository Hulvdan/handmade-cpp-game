@echo off
setlocal EnableDelayedExpansion

pushd %0\..\..

echo [32mINFO: Running build.bat...[0m

set "startTime=%time: =0%"

echo.
echo [33mINFO: Remaking CMake files...[0m
cmake ^
    -G "Visual Studio 17 2022" ^
    -B .cmake\vs17 ^
    -Wdev ^
    -Wdeprecated ^
    -Werror=dev ^
    -Werror-deprecated ^
    --log-level=WARNING ^
    -DCMAKE_UNITY_BUILD=ON ^
    -DCMAKE_UNITY_BUILD_BATCH_SIZE=0

if %errorlevel% neq 0 (
    echo.
    echo Remaking CMake files [31mFailed[0m

    popd
    exit /b %errorlevel%
)

cls

echo.
echo [33mINFO: Compiling...[0m
MSBuild .cmake\vs17\game.sln -v:minimal

if %errorlevel% neq 0 (
    echo.
    echo Compilation [31mFailed[0m

    popd
    exit /b %errorlevel%
)

set "endTime=%time: =0%"
rem Get elapsed time:
set "end=!endTime:%time:~8,1%=%%100)*100+1!"  &  set "start=!startTime:%time:~8,1%=%%100)*100+1!"
set /A "elap=((((10!end:%time:~2,1%=%%100)*60+1!%%100)-((((10!start:%time:~2,1%=%%100)*60+1!%%100), elap-=(elap>>31)*24*60*60*100"
rem Convert elapsed time to HH:MM:SS:CC format:
set /A "cc=elap%%100+100,elap/=100,ss=elap%%60+100,elap/=60,mm=elap%%60+100,hh=elap/60+100"

echo.
echo Compilation [32mSucceeded[0m in %hh:~1%%time:~2,1%%mm:~1%%time:~2,1%%ss:~1%%time:~8,1%%cc:~1%
REM echo --- Start:    %startTime%
REM echo --- End:      %endTime%

popd
