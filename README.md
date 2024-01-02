# Handmade C++ Game

Paving my path to the C++ competency following the steps of Casey Muratori in his "Handmade Hero" programming series.

## Development

### Setting up the machine


#### Windows

Project setup and CMake bullshiet.

```
call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set CC="cl"
set CXX="cl"
cmake -G "Visual Studio 17 2022" -B .cmake\vs17

.\cmd\build.bat
.\cmd\run.bat
```

Linting. Having `compile_commands.json` will allow the use of `clang-tidy`.

```
cmake -G Ninja -B .cmake\ninja
ninja -C .cmake\ninja -f build.ninja -t compdb > compile_commands.json
```
