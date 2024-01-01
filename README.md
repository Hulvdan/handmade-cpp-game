# test-zig-game

## Development

### Setting up the machine

Windows project setup and other BS

```
call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set CC="cl"
set CXX="cl"
cmake -G Ninja -B .cmake\ninja
cmake -G "Visual Studio 17 2022" -B .cmake\vs17
ninja -C .cmake\ninja -f build.ninja -t compdb > compile_commands.json

.\cmd\build.bat
.\cmd\run.bat
```

Linting

Windows: TODO


