"""cli для работы с проектом.

Как использовать:

(из корня проекта)

- poetry run python cmd\cli.py generate
- poetry run python cmd\cli.py cmake_vs_files
- poetry run python cmd\cli.py build
- poetry run python cmd\cli.py run
- poetry run python cmd\cli.py stoopid_windows_visual_studio_run
- poetry run python cmd\cli.py test
- poetry run python cmd\cli.py format
- poetry run python cmd\cli.py format file1 file2 ...
- poetry run python cmd\cli.py lint
"""

import glob
import hashlib
import logging
import os
import shutil
import subprocess
import sys
import tempfile
from contextlib import contextmanager
from functools import wraps
from pathlib import Path
from time import time
from typing import Any, Callable, NoReturn, TypeVar

import pyjson5 as json
import typer
from PIL import Image

T = TypeVar("T")

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
def better_json_dump(data, path):
    with open(path, "wb") as out_file:
        json.dump(data, out_file, indent="\t", ensure_ascii=False)


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


def hash32(value: str) -> int:
    sha = hashlib.sha256(value.encode(encoding="ascii"))
    return int(sha.hexdigest()[:8], 16)


# ======================================== #
#          Индивидуальные задачи           #
# ======================================== #
def do_build() -> None:
    run_command(r"MSBuild .cmake\vs17\game.sln -v:minimal -property:WarningLevel=3")


def listfiles_with_hashes_in_dir(path: Path) -> dict[str, int]:
    files = glob.glob(str(Path(path) / "**" / "*"), recursive=True, include_hidden=True)

    res = {}

    for file in files:
        with open(file, "rb") as in_file:
            res[str(Path(file).relative_to(path))] = hash(in_file.read())

    return res


def make_gamelib(
    callback: Callable[[str], None], *, texture_name_hashes: set[int]
) -> str | None:
    with open(PROJECT_DIR / "gamelib.jsonc") as in_file:
        data = json.load(in_file)

    hashify_texture = lambda data, key: hashify_texture_with_check(
        data, key, hashes_set=texture_name_hashes
    )

    for i in data["buildings"]:
        hashify_texture(i, "texture")
    for i in data["resources"]:
        hashify_texture(i, "texture")
        hashify_texture(i, "small_texture")

    with tempfile.TemporaryDirectory(prefix="handmade_cpp_game_cli_") as dir:
        p = Path(dir) / "temp_gamelib.jsonc"
        better_json_dump(data, p)
        callback(p)


def make_atlas(path: Path) -> set[int]:
    """Создание атласа из .ftpp (free texture packer) файла.

    Герярится .bmp (текстура) и .json (спецификация) для использования внутри игры.

    Возвращает множество хешей названий использованных в этом атласе текстур,
    которое используются для валидации того, что текстуры,
    указанные в различных сущностях в gamelib-е существуют.
    """
    assert str(path).endswith(".ftpp")

    directory = path.parent
    filename_wo_extension = path.name.rsplit(".", 1)[0]

    texture_name_hashes: set[int] = set()

    # NOTE: Генерируем атлас из .ftpp файла.
    # Во время этого создаются .json спецификация и .png текстура.
    log.debug("Generating {} atlas".format(path))
    run_command("free-tex-packer-cli --project {} --output {}".format(path, path.parent))

    # NOTE: Подгоняем спецификацию под наш формат.
    json_path = directory / (filename_wo_extension + ".json")
    with open(json_path) as json_file:
        json_data = json.load(json_file)

    textures = []
    for name, data in json_data["frames"].items():
        texture_name_hashes.add(hash32(name))

        texture_data = {
            "id": hash32(name),
            "debug_name": name,
            "size_x": data["frame"]["w"],
            "size_y": data["frame"]["h"],
            "pos_x": data["frame"]["x"],
            "pos_y": data["frame"]["y"],
        }
        textures.append(texture_data)

    better_json_dump({"textures": textures}, json_path)

    # NOTE: Конвертируем .png to .bmp.
    png_path = directory / (filename_wo_extension + ".png")
    bmp_path = directory / (filename_wo_extension + ".bmp")
    log.debug("Converting {} to {}".format(png_path, bmp_path))

    img = Image.open(png_path)
    img.save(bmp_path)

    return texture_name_hashes


def do_generate() -> None:
    # NOTE: Алтас.
    texture_name_hashes: set[int] = set()
    texture_name_hashes |= make_atlas(Path("assets") / "art" / "atlas.ftpp")

    hashes_for_msbuild = listfiles_with_hashes_in_dir(SOURCES_DIR / "generated")

    glob_pattern = SOURCES_DIR / "**" / "*.fbs"

    # NOTE: Генерируем cpp файлы из FlatBuffer (.fbs) файлов.
    flatbuffer_files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)

    # NOTE: Мудацкий костыль, чтобы MSBuild не ребилдился каждый раз.
    with tempfile.TemporaryDirectory() as td:
        run_command([FLATC_PATH, "-o", td, "--cpp", *flatbuffer_files])

        generated_file_paths = [
            SOURCES_DIR / "generated" / (file.stem + "_generated.h")
            for file in map(Path, flatbuffer_files)
        ]

        for file, file_hash in listfiles_with_hashes_in_dir(td).items():
            if file_hash != hashes_for_msbuild.get(file):
                shutil.copyfile(Path(td) / file, SOURCES_DIR / "generated" / file)

    # NOTE: Конвертим gamelib.jsonc в бинарю.
    make_gamelib(
        lambda path: run_command(
            [FLATC_PATH, "-b", SOURCES_DIR / "bf_gamelib.fbs", path]
        ),
        texture_name_hashes=texture_name_hashes,
    )


def hashify_texture_with_check(
    data: dict[str, Any], key: str, hashes_set: set[int]
) -> None:
    hashed_value = hash32(data[key])
    assert hashed_value in hashes_set
    data[key] = hashed_value


def assert_contains(value: T, container) -> T:
    if value not in container:
        log.critical("")
        exit(1)

    return value


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
    # do_cmake_vs_files()
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
