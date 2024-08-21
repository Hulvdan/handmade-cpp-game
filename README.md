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
    - flatbuffers
    - Прикручивал, а потом выкинул spdlog и fmt, т.к. они существенно замедляли время компиляции.
4. Использую линтер - [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) и [clang-format](https://clang.llvm.org/docs/ClangFormat.html).

## Разработка

Документация для разработки находится в [DEVELOPMENT.md](./DEVELOPMENT.md)

## Соглашения, conventions, интересные (для меня) мысли

### Структруры

Во имя C приоритет отдаю полностью "пустым" структурам без явно прописанных конструкторов.

Бан move, copy конструкторов и операторов.

Jonathan Blow, [Constructors, Destructors](https://www.youtube.com/watch?v=8C6zuDDGU2w):
- Бан динамических аллокаций во время отработки конструкторов.
- В конструкторах ничего не зависит от порядка назначения.

### Полезные ссылки

#### Философия

- [Записи стримов. Handmade Hero](https://guide.handmadehero.org/code/)
- [Статья. Orthodox C++](https://gist.github.com/bkaradzic/2e39896bc7d8c34e042b)
- [Статья. Casey Muratori. Semantic Compression](https://caseymuratori.com/blog_0015)
- [Статья. Jonathan Blow's blog, John Carmack on Inlined Code](http://number-none.com/blow/blog/programming/2014/09/26/carmack-on-inlined-code.html)
- [YouTube. Jonathan Blow. Constructors, Destructors](https://youtu.be/8C6zuDDGU2w?si=OGRfXkEhbdGNHj3J)
- [YouTube. Jonathan Blow. Q&A: Constructors, Destructors](https://youtu.be/wfG2mCPzIA4?si=lSGcsKcshtv-bOg5)

#### Работа с памятью

Арены

- В "Handmade Hero" Casey Muratori изначально начинает использовать арены.
- [YouTube. Ryan Fleury. Enter The Arena: Simplifying Memory Management (2023)](https://www.youtube.com/watch?v=TZ5a3gCCZYo)

Generic аллокации

- [YouTube. CppCon. Andrei Alexandrescu. std::allocator Is to Allocation what std::vector Is to Vexation](https://www.youtube.com/watch?v=LIb3L4vKZ7U)

Размышления на тему

- [YouTube. Jonathan Blow. Demo: Implicit Context](https://www.youtube.com/watch?v=ciGQCP6HgqI&list=PLmV5I2fxaiCKfxMBrNsU1kgKJXD3PkyxO&index=16)

#### Data Oriented Design

- [YouTube. CppCon 2014: Mike Acton "Data-Oriented Design and C++"](https://youtu.be/rX0ItVEVjHc?si=iPMMxjQkQEcCm-q4)
- [Книга. Richard Fabian - Data-Oriented Design](https://www.dataorienteddesign.com/dodbook/)

#### Критика

- [Форум-тред. Why many AAA gamedev studios opt out of the STL](https://web.archive.org/web/20220227163717/https://threadreaderapp.com/thread/1497768472184430600.html)
- [YouTube. Timothy Cain. Programming Languages](https://www.youtube.com/watch?v=wTjm-e0eZ8E)
- [YouTube. Handmade Hero | new vs delete | struct vs class | How to get fired](https://www.youtube.com/watch?v=zjkuXtiG1og)
- [YouTube. Casey Muratori | Smart-Pointers, RAII, ZII? Becoming an N+2 programmer](https://www.youtube.com/watch?v=xt1KNDmOYqA)
- [YouTube. Casey Muratori. Handmade Hero | Getting rid of the OOP mindset](https://youtu.be/GKYCA3UsmrU?si=5aWiWEaT06OcTPxg)
- [YouTube. Casey Muratori. Handmade Hero | Private Data & Getters/Setters (Epic rant!)](https://youtu.be/_xLgr6Ng4qQ?si=aCAFzwsmz5F_SNMp)
- [Email response. Linus Torvalds on C++](https://harmful.cat-v.org/software/c++/linus)
- [YouTube. Casey Muratori. "Clean" Code, Horrible Performance](https://www.youtube.com/watch?v=tD5NrevFtbU)
- [Статья. Discussion between Casey Muratori and Robert Martin (Uncle Bob) about Clean Code. Part 1](https://github.com/unclebob/cmuratori-discussion/blob/main/cleancodeqa.md)
- [Статья. Discussion between Casey Muratori and Robert Martin (Uncle Bob) about Clean Code. Part 2](https://github.com/unclebob/cmuratori-discussion/blob/main/cleancodeqa-2.md)

#### Кодогенерация

- [Статья. Ryan Fleury. Table-Driven Code Generation](https://www.rfleury.com/p/table-driven-code-generation)

#### Инструменты

- [Free Texture Packer](http://free-tex-packer.com/cli/)
- [Godbolt. Compiler Explorer](https://godbolt.org/)
- [FlatBuffers](https://flatbuffers.dev/)

