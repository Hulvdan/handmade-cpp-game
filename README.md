# Handmade C++ Game

Paving my path to the C++ competency following the steps of [Casey Muratori](https://caseymuratori.com/about) in his ["Handmade Hero"](https://handmadehero.org/) programming series.

## Interesting Things I'd Point Out About This Repo

1. 0 calls to `new` / `malloc` / `free` in the game's logic code (e.g. loading files, processing textures, initializing the game's state, rendering, creating buildings, etc.). Only the hardcore use of `VirtualAlloc`.
2. There is **hot-reload** of game logic - I compile executable that loads game logic dll.
3. I don't really do C++'s templates and OOP here. After getting acquainted with ["Clean" Code, Horrible Performance](https://www.youtube.com/watch?v=tD5NrevFtbU) and other reasonings, I started exploring other ways of writing code better.
4. The only libraries used here are [ImGui](https://github.com/ocornut/imgui), [glew](https://github.com/nigels-com/glew) and [glm](https://github.com/g-truc/glm). Everything else was written from scratch.
5. I'm using [clang-tidy](https://clang.llvm.org/extra/clang-tidy/).
6. I'm using [clang-format](https://clang.llvm.org/docs/ClangFormat.html).
