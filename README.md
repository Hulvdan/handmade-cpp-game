# Roads of Horses (C++)

> 📝 Это C++ порт моей репы [RoadsOfHorses на Unity, C#](https://github.com/Hulvdan/RoadsOfHorses)

🎨 Превью - [hulvdan.github.io](https://hulvdan.github.io/).

## Некоторые интересные факты об этой репе:

1. Прикрутил hot-reload кода игровой логики - компилирую exe, который подгружает dll.
2. Стараюсь держать код простым по заветам [John Carmack](http://number-none.com/blow/blog/programming/2014/09/26/carmack-on-inlined-code.html), Jonathan Blow, [Casey Muratori](https://caseymuratori.com/blog_0015), [Timothy Cain](https://www.youtube.com/watch?v=wTjm-e0eZ8E).
3. 0 фреймворков.
4. Из библиотек:
    - ImGui
    - glew
    - glm
    - doctest
    - tracy
    - Прикручивал, а потом выкинул spdlog и fmt, т.к. они замедляли время компиляции.
5. Использую линтер - [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) и [clang-format](https://clang.llvm.org/docs/ClangFormat.html).

## Разработка

Документация для разработки находится в [DEVELOPMENT.md](./DEVELOPMENT.md)

## Соглашения, conventions, интересные (для меня) мысли

### Структруры

Во имя C приоритет отдаю полностью "пустым" структурам без явно прописанных конструкторов.

Бан move, copy конструкторов и операторов.

Casey Muratori в [25:00 - 30:20 Handmade Hero Day 341 - Dynamically Growing Arenas](https://www.youtube.com/watch?v=lzdKgeovBN0&t=1500s) высказывает мнение о том, что первичная инициализация памяти "нулями" (Zero Is Initialization, в противопоставление RAII - Resource Acquisition Is Initialization) сокращает количество багов.

По совету Jonathan Blow в [Constructors, Destructors](https://www.youtube.com/watch?v=8C6zuDDGU2w):
- Ничто не аллоцирует память во время отработки конструкторов.
- В конструкторах ничего не зависит от порядка назначения.

### Контейнеры не принимают значения

Я заметил, что можно обойтись без использования move конструкций при разработке контейнеров, если, к примеру, "Vector" бы для операции "Push_Back" выдавал указатель на память, в которую нужно записать значение, но сам бы его не записывал.
