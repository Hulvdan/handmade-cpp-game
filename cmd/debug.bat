@echo off

pushd %0\..\..

call cmd\build.bat
devenv .cmake\vs17\game.sln

REM devenv .cmake\vs17\Debug\win32.exe

popd
