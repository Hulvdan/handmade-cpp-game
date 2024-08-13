# Roads of Horses (C++)

> 📝 Это C++ порт моей репы [RoadsOfHorses на Unity, C#](https://github.com/Hulvdan/RoadsOfHorses)

🎨 Превью - [hulvdan.github.io](https://hulvdan.github.io/).

## Некоторые интересные факты об этой репе:

1. Прикрутил hot-reload кода игровой логики - компилирую exe, который подгружает dll.
2. 0 фреймворков.
3. Из библиотек:
    - ImGui
    - glew
    - glm
    - doctest
    - tracy
    - Прикручивал, а потом выкинул spdlog и fmt, т.к. они существенно замедляли время компиляции.
4. Использую линтер - [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) и [clang-format](https://clang.llvm.org/docs/ClangFormat.html).

## Разработка

Документация для разработки находится в [DEVELOPMENT.md](./DEVELOPMENT.md)

## Соглашения, conventions, интересные (для меня) мысли

### Структруры

Во имя C приоритет отдаю полностью "пустым" структурам без явно прописанных конструкторов.

Бан move, copy конструкторов и операторов.

По совету Jonathan Blow в [Constructors, Destructors](https://www.youtube.com/watch?v=8C6zuDDGU2w):
- Бан динамических аллокаций во время отработки конструкторов.
- В конструкторах ничего не зависит от порядка назначения.

Casey Muratori в [25:00 - 30:20 Handmade Hero Day 341 - Dynamically Growing Arenas](https://www.youtube.com/watch?v=lzdKgeovBN0&t=1500s) высказывает мнение о том, что первичная инициализация памяти "нулями" (Zero Is Initialization, в противопоставление RAII - Resource Acquisition Is Initialization) сокращает количество багов.

### Полезные ссылки

- [Videos. Handmade Hero](https://handmadehero.org/)
- [Article. Orthodox C++](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b)
- [Article. Jonathan Blow's blog, John Carmack on Inlined Code](http://number-none.com/blow/blog/programming/2014/09/26/carmack-on-inlined-code.html)
- [Article. Casey Muratori. Semantic Compression](https://caseymuratori.com/blog_0015)
- [YouTube. Timothy Cain. Programming Languages](https://www.youtube.com/watch?v=wTjm-e0eZ8E)
- [YouTube. Jonathan Blow. Demo: Implicit Context](https://www.youtube.com/watch?v=ciGQCP6HgqI&list=PLmV5I2fxaiCKfxMBrNsU1kgKJXD3PkyxO&index=16)
- [YouTube. Jonathan Blow. Constructors, Destructors](https://youtu.be/8C6zuDDGU2w?si=OGRfXkEhbdGNHj3J)
- [YouTube. Jonathan Blow. Q&A: Constructors, Destructors](https://youtu.be/wfG2mCPzIA4?si=lSGcsKcshtv-bOg5)
- [YouTube. Casey Muratori. Handmade Hero | Getting rid of the OOP mindset](https://youtu.be/GKYCA3UsmrU?si=5aWiWEaT06OcTPxg)
- [YouTube. Casey Muratori. Handmade Hero | Private Data & Getters/Setters (Epic rant!)](https://youtu.be/_xLgr6Ng4qQ?si=aCAFzwsmz5F_SNMp)
- [Forum Thread. Why many AAA gamedev studios opt out of the STL](https://web.archive.org/web/20220227163717/https://threadreaderapp.com/thread/1497768472184430600.html)
- [Article. Ryan Fleury. Table-Driven Code Generation](https://www.rfleury.com/p/table-driven-code-generation)
- [Email response. Linus Torvalds on C++](https://harmful.cat-v.org/software/c++/linus)
- [YouTube. CppCon. Andrei Alexandrescu. std::allocator Is to Allocation what std::vector Is to Vexation](https://www.youtube.com/watch?v=LIb3L4vKZ7U)
- [YouTube. Ryan Fleury. Enter The Arena: Simplifying Memory Management (2023)](https://www.youtube.com/watch?v=TZ5a3gCCZYo)
- [YouTube. CppCon 2014: Mike Acton "Data-Oriented Design and C++"](https://youtu.be/rX0ItVEVjHc?si=iPMMxjQkQEcCm-q4)
