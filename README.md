# Handmade C++ Game

Paving my path to the C++ competency following the steps of [Casey Muratori](https://caseymuratori.com/about) in his ["Handmade Hero"](https://handmadehero.org/) programming series.

## Interesting Things I'd Point Out About This Repo

1. 0 calls to `new` / `malloc` / `free` in the game's logic code (e.g. loading files, processing textures, initializing the game's state, rendering, etc.). Only 2 calls to `VirtualAlloc` in the `win32_platform.cpp`.
2. There is **hot-reload** of game logic - I compile executable that loads game logic dll.
3. We don't really do C++'s templates and OOP here. See ["Clean" Code, Horrible Performance](https://www.youtube.com/watch?v=tD5NrevFtbU).
4. The only libraries used here are [ImGui](https://github.com/ocornut/imgui), [glew](https://github.com/nigels-com/glew) and [glm](https://github.com/g-truc/glm). Everything else was written from scratch.
5. I'm using a linter - [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) - it helps me with becoming aware of faulty code constructions.
6. I'm using a formatter - [clang-format](https://clang.llvm.org/docs/ClangFormat.html) - it helps my code remain consistent while letting me not spend too much time styling it manually. However, some minor readability tradeoffs have been made. And I'm okay with them.
