@echo off

mkdir build
pushd build
zig c++ -g ..\sources\main.cpp -o out.exe User32.lib
popd

