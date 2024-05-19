"""cli для работы с проектом.

Как использовать:

- python cmd\cli.py generate
- python cmd\cli.py cmake_vs_files
- python cmd\cli.py build
- python cmd\cli.py run
- python cmd\cli.py stoopid_windows_visual_studio_run
- python cmd\cli.py test
- python cmd\cli.py format
- python cmd\cli.py format file1 file2 ...
- python cmd\cli.py lint
"""

import glob
import logging
import os
import subprocess
import sys
from contextlib import contextmanager
from functools import wraps
from pathlib import Path
from time import time
from typing import NoReturn

import typer

global_timing_manager_instance = None


# NOTE: При вызове exit отображаем затраченное время.
old_exit = exit


def timed_exit(code: int) -> NoReturn:
    global global_timing_manager_instance
    if global_timing_manager_instance is not None:
        global_timing_manager_instance.__exit__(None, None, None)
        console_handler.flush()

    old_exit(code)


def hook_exit():
    global exit
    exit = timed_exit


app = typer.Typer(callback=hook_exit, result_callback=timed_exit)


# ======================================== #
#                Constants                 #
# ======================================== #
LOG_FILE_POSITION = False

PROJECT_DIR = Path(__file__).parent.parent
CMD_DIR = Path("cmd")
SOURCES_DIR = Path("sources")
CMAKE_DEBUG_BUILD_DIR = Path(".cmake") / "vs17" / "Debug"

CLANG_FORMAT_PATH = "C:/Program Files/LLVM/bin/clang-format.exe"
CLANG_TIDY_PATH = "C:/Program Files/LLVM/bin/clang-tidy.exe"
FLATC_PATH = CMD_DIR / "flatc.exe"


# ======================================== #
#                 Logging                  #
# ======================================== #
class CustomLoggingFormatter(logging.Formatter):
    _grey = "\x1b[30;20m"
    _green = "\x1b[32;20m"
    _yellow = "\x1b[33;20m"
    _red = "\x1b[31;20m"
    _bold_red = "\x1b[31;1m"

    def _get_format(color: str | None) -> str:
        reset = "\x1b[0m"
        if color is None:
            color = reset

        suffix = ""
        if LOG_FILE_POSITION:
            suffix = " (%(filename)s:%(lineno)d)"

        return f"{color}[%(levelname)s] %(message)s{suffix}{reset}"

    _FORMATS = {
        logging.NOTSET: _get_format(None),
        logging.DEBUG: _get_format(None),
        logging.INFO: _get_format(_green),
        logging.WARNING: _get_format(_yellow),
        logging.ERROR: _get_format(_red),
        logging.CRITICAL: _get_format(_bold_red),
    }

    def format(self, record):
        log_fmt = self._FORMATS.get(record.levelno)
        formatter = logging.Formatter(log_fmt)
        return formatter.format(record)


log = logging.getLogger(__file__)
log.setLevel(logging.DEBUG)
console_handler = logging.StreamHandler()
console_handler.setLevel(logging.DEBUG)
console_handler.setFormatter(CustomLoggingFormatter())
log.addHandler(console_handler)


# ======================================== #
#            Library Functions             #
# ======================================== #


def replace_double_spaces(string: str) -> str:
    while "  " in string:
        string = string.replace("  ", " ")

    return string


def run_command(cmd: list[str] | str) -> None:
    if isinstance(cmd, str):
        cmd = replace_double_spaces(cmd.replace("\n", " ").strip())

    log.debug(f"Executing command: {cmd}")

    # TODO: Стримить напрямую, не дожидаясь завершения работы программы
    p = subprocess.run(
        cmd,
        shell=True,
        stdout=sys.stdout,
        stderr=sys.stderr,
        text=True,
        encoding="utf-8",
    )

    if p.returncode:
        log.critical(f"Failed to execute: {cmd}")
        exit(p.returncode)


# ======================================== #
#          Индивидуальные задачи           #
# ======================================== #
def do_build() -> None:
    run_command(r"MSBuild .cmake\vs17\game.sln -v:minimal -property:WarningLevel=3")


def do_generate() -> None:
    glob_pattern = SOURCES_DIR / "**" / "*.fbs"

    # NOTE: Генерируем cpp файлы из FlatBuffer (.fbs) файлов
    flatbuffer_files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)
    run_command([FLATC_PATH, "-o", SOURCES_DIR / "generated", "--cpp", *flatbuffer_files])

    generated_file_paths = [
        SOURCES_DIR / "generated" / (file.stem + "_generated.h")
        for file in map(Path, flatbuffer_files)
    ]

    # NOTE: Форматируем сгенерированные файлы
    do_format(generated_file_paths)
    log.debug(
        "Formatted {}".format(
            ", ".join("'{}'".format(file) for file in generated_file_paths)
        ),
    )


def do_run() -> None:
    run_command(str(CMAKE_DEBUG_BUILD_DIR / "win32.exe"))


def do_test() -> None:
    run_command(str(CMAKE_DEBUG_BUILD_DIR / "tests.exe"))


def do_format(specific_files: list[str]) -> None:
    if specific_files:
        run_command([CLANG_FORMAT_PATH, "-i", *specific_files])
        return

    glob_patterns = [
        Path(SOURCES_DIR) / "**" / "*.cpp",
        Path(SOURCES_DIR) / "**" / "*.h",
    ]

    files_to_format = []
    for pattern in glob_patterns:
        files_to_format.extend(
            glob.glob(str(pattern), recursive=True, include_hidden=True)
        )

    run_command([CLANG_FORMAT_PATH, "-i", *files_to_format])


def do_lint() -> None:
    run_command(
        r"""
            "C:/Program Files/LLVM/bin/clang-tidy.exe"
            --use-color
            sources\win32_platform.cpp
            sources\bf_game.cpp
        """
        # NOTE: Убираем абсолютный путь к проекту из выдачи линтинга.
        # Тут куча экранирования происходит, поэтому нужно дублировать обратные слеши.
        + r'| sed "s/{}//"'.format(
            str(PROJECT_DIR).replace(os.path.sep, os.path.sep * 3) + os.path.sep * 3
        )
    )


def do_cmake_vs_files() -> None:
    run_command(
        r"""
            cmake
            -G "Visual Studio 17 2022"
            -B .cmake\vs17
            -DCMAKE_UNITY_BUILD=ON
            -DCMAKE_UNITY_BUILD_BATCH_SIZE=0
            --log-level=ERROR
        """
    )


def do_cmake_ninja_files() -> None:
    run_command(
        r"""
            cmake
            -G Ninja
            -B .cmake\ninja
            -D CMAKE_CXX_COMPILER=cl
            -D CMAKE_C_COMPILER=cl
            --log-level=ERROR
        """
    )


def do_compile_commands_json() -> None:
    run_command(
        r"""
            ninja
            -C .cmake\ninja
            -f build.ninja
            -t compdb
            > compile_commands.json
        """
    )


def do_stop_vs_ahk() -> None:
    run_command(r".nvim-personal\stop_vs.ahk")


def do_run_vs_ahk() -> None:
    run_command(r".nvim-personal\launch_vs.ahk")


# ======================================== #
#               CLI Commands               #
# ======================================== #
def timing(f):
    @wraps(f)
    def wrap(*args, **kw):
        started_at = time()
        result = f(*args, **kw)
        elapsed = time() - started_at
        log.info("Running '{}' took: {:.2f} sec".format(f.__name__, elapsed))
        return result

    return wrap


@app.command("generate")
@timing
def action_generate():
    do_generate()


@app.command("cmake_vs_files")
@timing
def action_cmake_vs_files():
    do_cmake_vs_files()


@app.command("build")
@timing
def action_build():
    do_cmake_vs_files()
    do_generate()
    do_build()


@app.command("run")
def action_run():
    action_build()
    do_run()


@app.command("stoopid_windows_visual_studio_run")
def action_stoopid_windows_visual_studio_run():
    do_stop_vs_ahk()
    action_build()
    do_run_vs_ahk()


@app.command("test")
def action_test():
    action_build()
    do_test()


@app.command("format")
def action_format(filepaths: list[str] = typer.Argument(default=None)):
    do_cmake_ninja_files()
    do_compile_commands_json()
    do_format(filepaths)


@app.command("lint")
def action_lint():
    do_cmake_ninja_files()
    do_compile_commands_json()
    do_lint()


# ======================================== #
#                   Main                   #
# ======================================== #
@contextmanager
def timing_manager():
    started_at = time()
    yield
    log.info("Running took: {:.2f} sec".format(time() - started_at))
    console_handler.flush()


def main() -> None:
    # NOTE: Всегда исполняем файл относительно корня проекта.
    os.chdir(PROJECT_DIR)

    caught_exc = None

    # NOTE: При выпадении exception-а отображаем затраченное время.
    global global_timing_manager_instance
    global_timing_manager_instance = timing_manager()

    with global_timing_manager_instance:
        try:
            app()
        except Exception as e:
            caught_exc = e

    if caught_exc is not None:
        raise caught_exc


if __name__ == "__main__":
    main()
