# from typing import Callable, TypeAlias
#
# JsonType: TypeAlias = dict | list | str | int | float | bool
#
#
# def deep_iter(
#     item: JsonType, function: Callable[[JsonType], None], run_on_types: list[Any]
# ) -> None:
#     if isinstance(item, list):
#         pass
#
#     if isinstance(item, dict):
#         pass
#
#     if isinstance(item, (str, int, float, bool)):
#         pass
#
#     if isinstance(item, (str, int, float, bool)):
#         pass
#
#     else:
#         assert False
#
#
# def test_deep_iter() -> None:
#     container = [1, [2, 3], 4]
#     deep_iter
