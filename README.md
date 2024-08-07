# Roads of Horses (C++)

> 📝 Это C++ порт моей репы [RoadsOfHorses на Unity, C#](https://github.com/Hulvdan/RoadsOfHorses)

🎨 Превью - [hulvdan.github.io](https://hulvdan.github.io/).

## Некоторые интересные факты об этой репе:

1. Прикрутил hot-reload кода игровой логики - компилирую exe, который подгружает dll.
2. Стараюсь держать код простым по заветам [John Carmack](http://number-none.com/blow/blog/programming/2014/09/26/carmack-on-inlined-code.html), Jonathan Blow, [Casey Muratori](https://caseymuratori.com/blog_0015), [Timothy Cain](https://www.youtube.com/watch?v=wTjm-e0eZ8E).
3. Из библиотек:
    - 0 фреймворков
    - [ImGui](https://github.com/ocornut/imgui)
    - [glew](https://github.com/nigels-com/glew)
    - [glm](https://github.com/g-truc/glm)
    - doctest
    - tracy
    - spdlog
4. Использую [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) и [clang-format](https://clang.llvm.org/docs/ClangFormat.html).

## Разработка

Документация для разработки находится в [DEVELOPMENT.md](./DEVELOPMENT.md)

## Соглашения / Conventions

### ZII - Zero Is Initialization

ref: 25:00 - 30:20  [Handmade Hero Day 341 - Dynamically Growing Arenas](https://www.youtube.com/watch?v=lzdKgeovBN0&t=1500s)

Casey Muratori высказывает своё мнение о том, что считает, что первичная инициализация памяти "нулями" (в сравнении с RAII и конструкторами) сокращает количество багов.

### Контейнеры не принимают значения

Я заметил, что можно обойтись без использования "move" конструкций при разработке контейнеров, если, к примеру, "Vector" бы для операции "Push_Back" выдавал указатель на память, в которую нужно записать значение, но сам бы его не записывал.
