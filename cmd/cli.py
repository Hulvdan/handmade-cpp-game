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
from typing import Any, NoReturn, TypeVar

import fnvhash
import pyjson5 as json
import typer
from PIL import Image

T = TypeVar("T")

global_timing_manager_instance = None


# При вызове exit отображаем затраченное время.
old_exit = exit  # noqa


def timed_exit(code: int) -> NoReturn:
    global global_timing_manager_instance
    if global_timing_manager_instance is not None:
        global_timing_manager_instance.__exit__(None, None, None)  # noqa
        console_handler.flush()

    old_exit(code)


def hook_exit():
    global exit  # noqa
    exit = timed_exit  # noqa


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
MSBUILD_PATH = r"c:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"


# ======================================== #
#                 Logging                  #
# ======================================== #
class CustomLoggingFormatter(logging.Formatter):
    _grey = "\x1b[30;20m"
    _green = "\x1b[32;20m"
    _yellow = "\x1b[33;20m"
    _red = "\x1b[31;20m"
    _bold_red = "\x1b[31;1m"

    @staticmethod
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
        json.dump(data, out_file, indent="\t", ensure_ascii=False)  # noqa


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
    return fnvhash.fnv1a_32(value.encode(encoding="ascii"))


def assert_contains(value: T, container) -> T:
    if value not in container:
        log.critical("value '{}' not found inside '{}'".format(value, container))
        exit(1)

    return value


# ======================================== #
#  Всякая хрень для индивидуальных задач   #
# ======================================== #
def listfiles_with_hashes_in_dir(path: str | Path) -> dict[str, int]:
    files = glob.glob(str(Path(path) / "**" / "*"), recursive=True, include_hidden=True)

    res = {}

    for file in files:
        with open(file, "rb") as in_file:
            res[str(Path(file).relative_to(path))] = hash(in_file.read())

    return res


def convert_gamelib_json_to_binary(texture_name_hashes: set[int]) -> None:
    # Загрузка json
    with open(PROJECT_DIR / "gamelib.jsonc") as in_file:
        data = json.load(in_file)

    # Хеширование названий текстур с проверкой на то, что они есть в атласе
    hashify_texture = lambda data, key: hashify_texture_with_check(
        data, key, hashes_set=texture_name_hashes
    )
    hashify_textures_list = lambda data, key: hashify_textures_list_with_check(
        data, key, hashes_set=texture_name_hashes
    )

    for instance in data["buildings"]:
        hashify_texture(instance, "texture")
    for instance in data["resources"]:
        hashify_texture(instance, "texture")
        hashify_texture(instance, "small_texture")
    hashify_texture(data["art"], "human")
    hashify_texture(data["art"], "building_in_progress")
    hashify_textures_list(data["art"], "forest")
    hashify_textures_list(data["art"], "grass")
    hashify_textures_list(data["art"], "road")
    hashify_textures_list(data["art"], "flag")

    # Создание файла
    intermediate_path = PROJECT_DIR / "gamelib.intermediate.jsonc"
    better_json_dump(data, intermediate_path)
    run_command([FLATC_PATH, "-b", SOURCES_DIR / "bf_gamelib.fbs", intermediate_path])

    intermediate_binary_path = str(intermediate_path).rsplit(".", 1)[0] + ".bin"
    final_binary_path = PROJECT_DIR / "gamelib.bin"
    if os.path.exists(final_binary_path):
        os.remove(final_binary_path)
    os.rename(intermediate_binary_path, final_binary_path)


def make_atlas(path: Path) -> set[int]:
    """Создание атласа из .ftpp (free texture packer) файла.

    Генерятся .bmp (текстура) и .bin (переведённая в бинарный формат спецификация
    - хеши названий текстур, их размеры и положение в атласе).

    Возвращает множество хешей названий использованных в этом атласе текстур,
    которое используются для валидации того, что текстуры,
    указанные в различных сущностях в gamelib-е существуют.
    """
    assert str(path).endswith(".ftpp")

    directory = path.parent
    filename_wo_extension = path.name.rsplit(".", 1)[0]

    texture_name_hashes: set[int] = set()

    # Генерируем атлас из .ftpp файла.
    # Во время этого создаются .json спецификация и .png текстура.
    log.debug("Generating {} atlas".format(path))
    run_command("free-tex-packer-cli --project {} --output {}".format(path, path.parent))

    # Подгоняем спецификацию под наш формат.
    json_path = directory / (filename_wo_extension + ".json")
    with open(json_path) as json_file:
        json_data = json.load(json_file)

    textures = []
    for name, data in json_data["frames"].items():
        name = name.removeprefix("sources/")

        hashed_name = hash32(name)
        assert hashed_name not in texture_name_hashes
        texture_name_hashes.add(hashed_name)

        texture_data = {
            "id": hash32(name),
            "debug_name": name,
            "size_x": data["frame"]["w"],
            "size_y": data["frame"]["h"],
            "atlas_x": data["frame"]["x"],
            "atlas_y": data["frame"]["y"],
        }
        textures.append(texture_data)

    # NOTE: Сортировка по хешу в теории бы позволила в бинарный поиск.
    textures.sort(key=lambda x: x["id"])

    json_intermediate_path = directory / (filename_wo_extension + ".intermediate.json")
    better_json_dump(
        {
            "textures": textures,
            "size_x": json_data["meta"]["size"]["w"],
            "size_y": json_data["meta"]["size"]["h"],
        },
        json_intermediate_path,
    )

    # Конвертируем спецификацию атласа в бинарный формат.
    run_command(
        [
            FLATC_PATH,
            "-o",
            "assets/art",
            "-b",
            SOURCES_DIR / "bf_atlas.fbs",
            json_intermediate_path,
        ]
    )

    # Переименовываем сгенерированную бинарную спецификацию.
    itermediate_binary_path = directory / (filename_wo_extension + ".intermediate.bin")
    final_binary_path = directory / (filename_wo_extension + ".bin")
    if os.path.exists(final_binary_path):
        os.remove(final_binary_path)
    os.rename(itermediate_binary_path, final_binary_path)

    # Конвертируем .png to .bmp.
    png_path = directory / (filename_wo_extension + ".png")
    bmp_path = directory / (filename_wo_extension + ".bmp")
    log.debug("Converting {} to {}".format(png_path, bmp_path))

    img = Image.open(png_path)
    img.save(bmp_path)

    return texture_name_hashes


def remove_intermediate_generation_files() -> None:
    glob_pattern = PROJECT_DIR / "**" / "*.intermediate*"
    files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)

    for file in files:
        os.remove(file)


def validate_tilerules(hashes_set: set[int]) -> None:
    glob_pattern = PROJECT_DIR / "assets" / "art" / "tiles" / "tilerule_*.txt"
    files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)

    for filepath in files:
        with open(filepath) as in_file:
            lines = in_file.readlines()

        for line in lines:
            line = line.strip()
            if line.startswith("|"):
                continue

            hashed_value = hash32(line)
            assert hashed_value in hashes_set, f"Texture '{line}' not found in atlas!"


def hashify_textures_list_with_check(
    data: dict[str, Any],
    key: str,
    hashes_set: set[int],
) -> None:
    textures = data[key]
    assert isinstance(textures, list)

    for i, texture_name in enumerate(textures):
        hashed_value = hash32(texture_name)
        assert hashed_value in hashes_set, f"Texture '{texture_name}' not found in atlas!"
        textures[i] = hashed_value


def hashify_texture_with_check(
    data: dict[str, Any], key: str, hashes_set: set[int]
) -> None:
    texture_name = data[key]
    assert isinstance(texture_name, str)

    hashed_value = hash32(texture_name)
    assert hashed_value in hashes_set, f"Texture '{texture_name}' not found in atlas!"
    data[key] = hashed_value


# ======================================== #
#          Индивидуальные задачи           #
# ======================================== #
def do_build() -> None:
    run_command(
        rf'"{MSBUILD_PATH}" .cmake\vs17\game.sln -v:minimal -property:WarningLevel=3'
    )


def do_generate() -> None:
    remove_intermediate_generation_files()

    hashes_for_msbuild = listfiles_with_hashes_in_dir(SOURCES_DIR / "generated")
    glob_pattern = SOURCES_DIR / "**" / "*.fbs"

    # Генерируем cpp файлы из FlatBuffer (.fbs) файлов.
    flatbuffer_files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)

    # NOTE: Мудацкий костыль, чтобы MSBuild не ребилдился каждый раз.
    with tempfile.TemporaryDirectory() as td:
        run_command([FLATC_PATH, "-o", td, "--cpp", *flatbuffer_files])

        for file, file_hash in listfiles_with_hashes_in_dir(td).items():
            if file_hash != hashes_for_msbuild.get(file):
                shutil.copyfile(Path(td) / file, SOURCES_DIR / "generated" / file)

    # Собираем атлас.
    texture_name_hashes: set[int] = set()
    texture_name_hashes |= make_atlas(Path("assets") / "art" / "atlas.ftpp")

    validate_tilerules(texture_name_hashes)

    # Конвертим gamelib.jsonc в бинарю.
    convert_gamelib_json_to_binary(texture_name_hashes)


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
        # Убираем абсолютный путь к проекту из выдачи линтинга.
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
    # Всегда исполняем файл относительно корня проекта.
    os.chdir(PROJECT_DIR)

    caught_exc = None

    # При выпадении exception-а отображаем затраченное время.
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
    # assert hash32("") == 2166136261, hash32("")
    value = hash32("test")
    assert value == 0xAFD071E5, value
    # Hash32((u8*)"test", 5) -> 2854439807
    # assert value == 2854439807, value
    main()
