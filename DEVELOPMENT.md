# Development

## Setting up the machine

### Windows

Что нужно иметь:

- Visual Studio 2022
- Python 3.11.3 (лучше всего через pyenv)
- Установленный LLVM (clang-tidy, clang-format - для линтинга и форматтинга)
- cli утилита "sed" (для линтинга)

Завод проекта:

```
call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set CC="cl"
set CXX="cl"

REM Создать / перегенерить .sln
python cmd\cli.py cmake_vs_files

REM Билд проекта
python cmd\cli.py build

REM Прогнать тесты
python cmd\cli.py test

REM Запуск
python cmd\cli.py run

REM Форматирование
python cmd\cli.py format

REM Запуск линта
python cmd\cli.py lint
```
