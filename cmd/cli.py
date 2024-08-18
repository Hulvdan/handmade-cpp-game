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
import logging
import os
import re
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
import sdl2
import typer
from OpenGL.GL import shaders
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


# -----------------------------------------------------------------------------------
# Constants.
# -----------------------------------------------------------------------------------


LOG_FILE_POSITION = False

PROJECT_DIR = Path(__file__).parent.parent
CMD_DIR = PROJECT_DIR / "cmd"
RESOURCES_DIR = PROJECT_DIR / "resources"
SOURCES_DIR = PROJECT_DIR / "sources"
FLATBUFFERS_GENERATED_DIR = PROJECT_DIR / "codegen" / "flatbuffers"
CMAKE_DEBUG_BUILD_DIR = PROJECT_DIR / ".cmake" / "vs17" / "Debug"
CMAKE_RELEASE_BUILD_DIR = PROJECT_DIR / ".cmake" / "vs17" / "Release" / "RoadsOfHorses"

CLANG_FORMAT_PATH = "C:/Program Files/LLVM/bin/clang-format.exe"
CLANG_TIDY_PATH = "C:/Program Files/LLVM/bin/clang-tidy.exe"
FLATC_PATH = CMD_DIR / "flatc.exe"
MSBUILD_PATH = r"c:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"

REPLACING_SPACES_PATTERN = re.compile("\s+")
SHADERS_ERROR_PATTERN = re.compile(r"\d+\((\d+)\) : error (.*)")


# -----------------------------------------------------------------------------------
# Logging.
# -----------------------------------------------------------------------------------


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


# -----------------------------------------------------------------------------------
# Library Functions.
# -----------------------------------------------------------------------------------


def better_json_dump(data, path):
    with open(path, "wb") as out_file:
        json.dump(data, out_file, indent="\t", ensure_ascii=False)  # noqa


def replace_double_spaces(string: str) -> str:
    return re.sub(REPLACING_SPACES_PATTERN, " ", string)


def run_command(cmd: list[str] | str) -> None:
    if isinstance(cmd, str):
        cmd = replace_double_spaces(cmd.replace("\n", " ").strip())

    log.debug(f"Executing command: {cmd}")

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


# -----------------------------------------------------------------------------------
# Всякая хрень для индивидуальных задач.
# -----------------------------------------------------------------------------------


def listfiles_with_hashes_in_dir(path: str | Path) -> dict[str, int]:
    files = glob.glob(str(Path(path) / "**" / "*"), recursive=True, include_hidden=True)

    res = {}

    for file in files:
        with open(file, "rb") as in_file:
            res[str(Path(file).relative_to(path))] = hash(in_file.read())

    return res


def convert_gamelib_json_to_binary(texture_name_2_id: dict[str, int]) -> None:
    # Загрузка json
    with open(PROJECT_DIR / "gamelib.jsonc") as in_file:
        data = json.load(in_file)

    # Хеширование названий текстур с проверкой на то, что они есть в атласе
    transform_texture = lambda data, key: transform_to_texture_index(
        data, key, texture_name_2_id=texture_name_2_id
    )
    transform_textures_list = lambda data, key: transform_to_texture_indexes_list(
        data, key, texture_name_2_id=texture_name_2_id
    )

    for instance in data["buildings"]:
        transform_texture(instance, "texture")
    for instance in data["resources"]:
        transform_texture(instance, "texture")
        transform_texture(instance, "small_texture")
    transform_texture(data["art"]["ui"], "buildables_panel")
    transform_texture(data["art"]["ui"], "buildables_placeholder")
    transform_texture(data["art"], "human")
    transform_texture(data["art"], "building_in_progress")
    transform_textures_list(data["art"], "forest")
    transform_textures_list(data["art"], "grass")
    transform_textures_list(data["art"], "road")
    transform_textures_list(data["art"], "flag")

    data["art"]["tile_rule_forest"] = load_tile_rule(
        "tile_rule_forest", texture_name_2_id
    )
    data["art"]["tile_rule_grass"] = load_tile_rule("tile_rule_grass", texture_name_2_id)

    # Создание файла
    intermediate_path = PROJECT_DIR / "gamelib.intermediate.jsonc"
    better_json_dump(data, intermediate_path)
    run_command([FLATC_PATH, "-b", SOURCES_DIR / "bf_gamelib.fbs", intermediate_path])

    intermediate_binary_path = str(intermediate_path).rsplit(".", 1)[0] + ".bin"
    final_binary_path = RESOURCES_DIR / "gamelib.bin"
    if os.path.exists(final_binary_path):
        os.remove(final_binary_path)
    os.rename(intermediate_binary_path, final_binary_path)


def load_tile_rule(name: str, texture_name_2_id: dict[str, int]) -> Any:
    with open(PROJECT_DIR / "assets" / "art" / "tiles" / f"{name}.txt") as in_file:
        data = in_file.readlines()

    data = [  # Убираем пустые строки
        line.strip(" \r\n\t") for line in data if line.strip(" \r\n\t")
    ]

    default_texture_name = data[0]
    default_texture_id = texture_name_2_id[default_texture_name]
    assert ((len(data) - 1) % 4) == 0, "Incorrect file format!"

    rules_count = (len(data) - 1) // 4
    assert rules_count < 100, "Maximum of 100 rules per smart tile is supported!"

    states = []

    for i in range(rules_count):
        texture_name = data[1 + i * 4]
        texture_id = texture_name_2_id[texture_name]

        rule_lines = (
            data[2 + i * 4],
            data[3 + i * 4],
            data[4 + i * 4],
        )

        condition = []

        symbols = {
            " ": 0,  # SKIP,
            "*": 1,  # EXCLUDED,
            "@": 2,  # INCLUDED,
        }

        for i, rule_line in enumerate(rule_lines):
            assert rule_line.startswith("|")
            assert rule_line.endswith("|")
            assert len(rule_line) == 5

            for k in range(3):
                should_skip = (i == 1) and (k == 1)
                if should_skip:
                    continue

                condition.append(symbols[rule_line[1 + k]])

        states.append(
            {
                "texture": texture_id,
                "condition": condition,
            }
        )

    result = {
        "default_texture": default_texture_id,
        "states": states,
    }
    return result


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

    # Генерируем атлас из .ftpp файла.
    # Во время этого создаются .json спецификация и .png текстура.
    log.debug("Generating {} atlas".format(path))
    run_command("free-tex-packer-cli --project {} --output {}".format(path, path.parent))

    # Подгоняем спецификацию под наш формат.
    json_path = directory / (filename_wo_extension + ".json")
    with open(json_path) as json_file:
        json_data = json.load(json_file)

    found_textures: set[str] = set()

    textures: list[Any] = []
    for name, data in json_data["frames"].items():
        name = name.removeprefix("sources/")

        assert name not in found_textures
        found_textures.add(name)

        texture_data = {
            "id": -1,
            "debug_name": name,
            "size_x": data["frame"]["w"],
            "size_y": data["frame"]["h"],
            "atlas_x": data["frame"]["x"],
            "atlas_y": data["frame"]["y"],
        }
        textures.append(texture_data)

    texture_name_2_id: dict[str, int] = dict()

    textures.sort(key=lambda x: x["debug_name"])

    for i in range(len(textures)):
        new_id = i + 1  # NOTE: Резерв 0 для обозначения отсутствия текстуры
        textures[i]["id"] = new_id

        name = textures[i]["debug_name"]
        texture_name_2_id[name] = new_id

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
    final_binary_path = RESOURCES_DIR / (filename_wo_extension + ".bin")
    if os.path.exists(final_binary_path):
        os.remove(final_binary_path)
    os.rename(itermediate_binary_path, final_binary_path)

    # Конвертируем .png to .bmp.
    png_path = directory / (filename_wo_extension + ".png")
    bmp_path = RESOURCES_DIR / (filename_wo_extension + ".bmp")
    log.debug("Converting {} to {}".format(png_path, bmp_path))

    img = Image.open(png_path)
    img.save(bmp_path)

    return texture_name_2_id


def remove_intermediate_generation_files() -> None:
    glob_pattern = PROJECT_DIR / "**" / "*.intermediate*"
    files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)

    for file in files:
        os.remove(file)


def validate_tilerules(texture_name_2_id: dict[str, int]) -> None:
    glob_pattern = PROJECT_DIR / "assets" / "art" / "tiles" / "tilerule_*.txt"
    files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)

    for filepath in files:
        with open(filepath) as in_file:
            lines = in_file.readlines()

        for line in lines:
            line = line.strip()
            if line.startswith("|"):
                continue

            assert line in texture_name_2_id, f"Texture '{line}' not found in atlas!"


def transform_to_texture_indexes_list(
    data: dict[str, Any],
    key: str,
    texture_name_2_id: dict[str, int],
) -> None:
    textures = data[key]
    assert isinstance(textures, list)

    for i, texture_name in enumerate(textures):
        assert (
            texture_name in texture_name_2_id
        ), f"Texture '{texture_name}' not found in atlas!"

        texture_index = texture_name_2_id[texture_name]
        textures[i] = texture_index


def transform_to_texture_index(
    data: dict[str, Any],
    key: str,
    texture_name_2_id: dict[str, int],
) -> None:
    texture_name = data[key]
    assert isinstance(texture_name, str)

    assert (
        texture_name in texture_name_2_id
    ), f"Texture '{texture_name}' not found in atlas!"

    texture_index = texture_name_2_id[texture_name]
    data[key] = texture_index


class GraphicsContext:
    def __init__(self):
        self.window = None
        self.context = None

    def __enter__(self):
        sdl2.SDL_Init(sdl2.SDL_INIT_VIDEO)
        self.window = sdl2.SDL_CreateWindow(
            b"Shadertest",
            sdl2.SDL_WINDOWPOS_UNDEFINED,
            sdl2.SDL_WINDOWPOS_UNDEFINED,
            1,
            1,
            sdl2.SDL_WINDOW_OPENGL,
        )
        sdl2.video.SDL_GL_SetAttribute(sdl2.video.SDL_GL_CONTEXT_MAJOR_VERSION, 4)
        sdl2.video.SDL_GL_SetAttribute(sdl2.video.SDL_GL_CONTEXT_MINOR_VERSION, 2)
        sdl2.video.SDL_GL_SetAttribute(
            sdl2.video.SDL_GL_CONTEXT_PROFILE_MASK,
            sdl2.video.SDL_GL_CONTEXT_PROFILE_CORE,
        )
        self.context = sdl2.SDL_GL_CreateContext(self.window)

    def __exit__(self, *args):
        sdl2.SDL_GL_DeleteContext(self.context)
        sdl2.SDL_DestroyWindow(self.window)
        sdl2.SDL_Quit()


def match_all_errors(shader_compilation_output: list[str]) -> list[tuple[int, str]]:
    res: list[tuple[int, str]] = []

    for error_string in shader_compilation_output:
        found_value = re.search(SHADERS_ERROR_PATTERN, error_string)
        if found_value:
            ln = int(found_value.group(1))
            res.append((ln, found_value.group(2)))

    return res


def find_and_test_shaders(
    text: str, prefix_text: str, suffix_text: str, shader_type
) -> bool:
    failed = False

    found_index = text.find(prefix_text)
    line_number = text[:found_index].count("\n")

    while found_index != -1:
        end_index = text.find(suffix_text)
        assert end_index != -1

        shader_text = text[found_index + len(prefix_text) : end_index]

        try:
            shaders.compileShader(shader_text, shader_type)
        except Exception as err:
            failed = True
            for ln, error_string in match_all_errors(err.args[0].split("\\n")):
                error_string = (
                    str(SOURCES_DIR / ("bfc_renderer.cpp:{}: ".format(line_number + ln)))
                    + error_string
                )
                print(error_string)

        text = text[end_index + len(suffix_text) :]

        found_index = text.find(prefix_text)
        line_number += shader_text.count("\n")
        line_number += text[:found_index].count("\n")

    return failed


# -----------------------------------------------------------------------------------
# Индивидуальные задачи.
# -----------------------------------------------------------------------------------


def do_build_debug() -> None:
    run_command(
        rf"""
        "{MSBUILD_PATH}" .cmake\vs17\game.sln
        -v:minimal
        -property:WarningLevel=3
        -t:win32
        """
    )


def do_build_release() -> None:
    run_command(
        rf"""
        "{MSBUILD_PATH}" .cmake\vs17\game.sln
        -v:minimal
        -property:WarningLevel=3
        -property:Configuration=Release
        -property:DEBUG=true
        -property:OutDir=Release\RoadsOfHorses\
        -t:win32
        """
    )

    previously_moved_resources_dir = CMAKE_RELEASE_BUILD_DIR / "resources"
    if previously_moved_resources_dir.exists():
        shutil.rmtree(previously_moved_resources_dir)

    shutil.move(CMAKE_RELEASE_BUILD_DIR / ".." / "resources", CMAKE_RELEASE_BUILD_DIR)


def do_build_tests() -> None:
    run_command(
        rf'"{MSBUILD_PATH}" .cmake\vs17\game.sln -v:minimal -property:WarningLevel=3 -t:tests'
    )


def do_generate() -> None:
    remove_intermediate_generation_files()

    hashes_for_msbuild = listfiles_with_hashes_in_dir(FLATBUFFERS_GENERATED_DIR)
    glob_pattern = SOURCES_DIR / "**" / "*.fbs"

    # Генерируем cpp файлы из FlatBuffer (.fbs) файлов.
    flatbuffer_files = glob.glob(str(glob_pattern), recursive=True, include_hidden=True)

    # NOTE: Мудацкий костыль, чтобы MSBuild не ребилдился каждый раз.
    with tempfile.TemporaryDirectory() as td:
        run_command([FLATC_PATH, "-o", td, "--cpp", *flatbuffer_files])

        for file, file_hash in listfiles_with_hashes_in_dir(td).items():
            if file_hash != hashes_for_msbuild.get(file):
                shutil.copyfile(Path(td) / file, FLATBUFFERS_GENERATED_DIR / file)

    # Собираем атлас.
    texture_name_2_id = make_atlas(Path("assets") / "art" / "atlas.ftpp")

    validate_tilerules(texture_name_2_id)

    # Конвертим gamelib.jsonc в бинарю.
    convert_gamelib_json_to_binary(texture_name_2_id)


def do_run_debug() -> None:
    run_command(str(CMAKE_DEBUG_BUILD_DIR / "win32.exe"))


def do_run_release() -> None:
    run_command(str(CMAKE_RELEASE_BUILD_DIR / "win32.exe"))


def do_test() -> None:
    run_command(str(CMAKE_DEBUG_BUILD_DIR / "tests.exe"))


def do_test_shaders() -> None:
    with open(SOURCES_DIR / "bfc_renderer.cpp", encoding="utf-8") as in_file:
        text = in_file.read()

    failed = False

    with GraphicsContext():
        for prefix_text, suffix_text, shader_type in (
            ('R"VertexShader(', ')VertexShader"', shaders.GL_VERTEX_SHADER),  # noqa
            ('R"FragmentShader(', ')FragmentShader"', shaders.GL_FRAGMENT_SHADER),  # noqa
        ):
            failed |= find_and_test_shaders(text, prefix_text, suffix_text, shader_type)

    if failed:
        exit(1)


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


def do_cmake_vs_debug_files() -> None:
    run_command(
        r"""
            cmake
            -G "Visual Studio 17 2022"
            -B .cmake\vs17
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_UNITY_BUILD=ON
            -DCMAKE_UNITY_BUILD_BATCH_SIZE=0
            --log-level=ERROR
        """
    )


def do_cmake_vs_release_files() -> None:
    run_command(
        r"""
            cmake
            -G "Visual Studio 17 2022"
            -B .cmake\vs17
            -DCMAKE_BUILD_TYPE=Release
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


# -----------------------------------------------------------------------------------
# CLI Commands.
# -----------------------------------------------------------------------------------


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
    do_cmake_vs_debug_files()


@app.command("build_game")
def action_build_game():
    do_test_shaders()
    do_generate()
    do_cmake_vs_debug_files()
    do_build_debug()


@app.command("release_game")
def action_release_game():
    do_test_shaders()
    do_generate()
    do_cmake_vs_release_files()
    do_build_release()


@app.command("run")
def action_run_debug():
    do_test_shaders()
    do_generate()
    do_cmake_vs_debug_files()
    do_build_debug()

    do_run_debug()


@app.command("stoopid_windows_visual_studio_run")
def action_stoopid_windows_visual_studio_run():
    do_stop_vs_ahk()

    do_test_shaders()
    do_generate()
    do_build_debug()

    do_run_vs_ahk()


@app.command("stoopid_windows_visual_studio_run_release")
def action_stoopid_windows_visual_studio_run_release():
    do_stop_vs_ahk()

    do_test_shaders()
    do_generate()
    do_build_release()

    do_run_vs_ahk()


@app.command("test")
def action_test():
    do_test_shaders()
    do_generate()
    do_build_tests()

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


@app.command("test_shaders")
def action_test_shaders() -> None:
    do_test_shaders()


# -----------------------------------------------------------------------------------
# Main.
# -----------------------------------------------------------------------------------


@contextmanager
def timing_manager():
    started_at = time()
    yield
    log.info("Running took: {:.2f} sec".format(time() - started_at))
    console_handler.flush()


def main() -> None:
    test_value = hash32("test")
    assert test_value == 0xAFD071E5, test_value

    # Всегда исполняем файл относительно корня проекта.
    os.chdir(PROJECT_DIR)

    caught_exc = None

    # При выпадении exception-а отображаем затраченное время.
    global global_timing_manager_instance
    global_timing_manager_instance = timing_manager()

    if not os.path.exists(RESOURCES_DIR):
        os.mkdir(RESOURCES_DIR)

    with global_timing_manager_instance:
        try:
            app()
        except Exception as e:
            caught_exc = e

    if caught_exc is not None:
        raise caught_exc


if __name__ == "__main__":

    main()
