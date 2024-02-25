@echo off

call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
pushd %0\..\..


@REM start /MAX nvui --ext_multigrid .
start c:\Users\user\Downloads\neovide.exe\neovide.exe .

sleep 2
