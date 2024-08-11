# Development

## Setting up the machine

### Windows

Что нужно иметь:

- Visual Studio 2022
- Python 3.11.3 (лучше всего через `pyenv`) + `poetry`
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

REM Создать / перегенерить .sln
.venv\Scripts\python.exe cmd\cli.py cmake_vs_files

REM Кодогенерация
.venv\Scripts\python.exe cmd\cli.py generate

REM Билд проекта
.venv\Scripts\python.exe cmd\cli.py build

REM Прогнать тесты
.venv\Scripts\python.exe cmd\cli.py test

REM Запуск
.venv\Scripts\python.exe cmd\cli.py run

REM Форматирование
.venv\Scripts\python.exe cmd\cli.py format

REM Запуск линта
.venv\Scripts\python.exe cmd\cli.py lint
```
