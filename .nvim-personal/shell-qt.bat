@echo off

call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
pushd %0\..\..

start /MAX nvim-qt .
sleep 2
