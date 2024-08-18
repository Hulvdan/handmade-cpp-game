# Development

## Setting up the machine

### Windows

Что нужно иметь:

- Visual Studio 2022
- Python 3.11.3 через `pyenv` + `poetry`
- Установленный LLVM (`clang-tidy`, `clang-format` - для линтинга и форматтинга)
- cli утилита `sed` (для линтинга)
- cli утилита `free-tex-packer-cli` (для создания атласа) (`npm install -g free-tex-packer-cli`)

Завод проекта:

```
call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set CC="cl"
set CXX="cl"

pyenv install 3.11.3
pyenv local 3.11.3
pip install poetry
poetry install

REM Кодогенерация
.venv\Scripts\python.exe cmd\cli.py generate

REM Создать / перегенерить .sln для дебага
.venv\Scripts\python.exe cmd\cli.py cmake_vs_debug_files

REM Создать / перегенерить .sln для релиза
.venv\Scripts\python.exe cmd\cli.py cmake_vs_release_files

REM Билд проекта - дебаг
.venv\Scripts\python.exe cmd\cli.py build_game_debug

REM Билд проекта - релиз
.venv\Scripts\python.exe cmd\cli.py build_game_release

REM Прогнать тесты
.venv\Scripts\python.exe cmd\cli.py test

REM Форматирование всех файлов в проекте
.venv\Scripts\python.exe cmd\cli.py format

REM Форматирование указанных файлов
.venv\Scripts\python.exe cmd\cli.py format file1 file2 ...

REM Линт
.venv\Scripts\python.exe cmd\cli.py lint


REM Ссылка на latest.log
ni C:\Users\user\dev\home\handmade-cpp-game\latest.log -i SymbolicLink -ta "c:\Users\user\dev\home\handmade-cpp-game\.cmake\vs17\Debug\latest.log"
```
