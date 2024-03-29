# Roads of Horses (C++)

> 📝 Это C++ порт моей репы [RoadsOfHorses на Unity, C#](https://github.com/Hulvdan/RoadsOfHorses)

🎨 Превью - [hulvdan.github.io](https://hulvdan.github.io/).

## Некоторые интересные факты об этой репе:

1. Прикрутил hot-reload кода игровой логики - компилирую exe, который подгружает dll.
2. Стараюсь держать код простым по заветам [John Carmack](http://number-none.com/blow/blog/programming/2014/09/26/carmack-on-inlined-code.html), Jonathan Blow, Casey Muratori, [Timothy Cain](https://www.youtube.com/watch?v=wTjm-e0eZ8E).
3. Из библиотек:
    - 0 фреймворков
    - [ImGui](https://github.com/ocornut/imgui)
    - [glew](https://github.com/nigels-com/glew)
    - [glm](https://github.com/g-truc/glm)
    - doctest
    - tracy
4. Использую [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) и [clang-format](https://clang.llvm.org/docs/ClangFormat.html).
