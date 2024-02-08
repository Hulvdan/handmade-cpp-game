# Handmade C++ Game

Paving my path to the C++ competency following the steps of [Casey Muratori](https://caseymuratori.com/about) in his ["Handmade Hero"](https://handmadehero.org/) programming series.

## Interesting Things I'd Point Out About This Repo

1. 0 calls to `new` / `malloc` / `free` in the game's logic code.
2. There is hot-reload of game logic - I compile executable that loads game's logic dll.
3. The only libraries used here are `ImGui`, `glew` and `glm`. Everything else was written from scratch.
4. We don't really do C++'s templates and OOP here. See ["Clean" Code, Horrible Performance](https://www.youtube.com/watch?v=tD5NrevFtbU).
