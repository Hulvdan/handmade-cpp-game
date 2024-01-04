@echo off

pushd %0\..\..

devenv .cmake\vs17\Debug\game.exe

popd
