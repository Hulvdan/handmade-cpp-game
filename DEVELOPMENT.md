# Development

## Setting up the machine

### Windows

Что нужно иметь:

- Visual Studio 2022
- Python 3.11.3 (лучше всего через `pyenv`) + `poetry`
- Установленный LLVM (`clang-tidy`, `clang-format` - для линтинга и форматтинга)
- cli утилита `sed` (для линтинга)
- cli утилита `sedfree-tex-packer-cli` (для создания атласа) (`npm install -g free-tex-packer-cli`)

Завод проекта:

```
call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set CC="cl"
set CXX="cl"

REM Создать / перегенерить .sln
poetry run python cmd\cli.py cmake_vs_files

REM Кодогенерация
poetry run python cmd\cli.py generate

REM Билд проекта
poetry run python cmd\cli.py build

REM Прогнать тесты
poetry run python cmd\cli.py test

REM Запуск
poetry run python cmd\cli.py run

REM Форматирование
poetry run python cmd\cli.py format

REM Запуск линта
poetry run python cmd\cli.py lint
```
