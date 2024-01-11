@echo off

pushd %0\..\..

REM devenv .cmake\vs17\game.sln

call cmd\build.bat
devenv .cmake\vs17\Debug\win32.exe

popd
