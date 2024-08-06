#pragma once

struct Arena : public Non_Copyable {
    size_t      used;
    size_t      size;
    u8*         base;
    const char* name;
};

#define Allocate_For(arena, type) rcast<type*>(Allocate_(arena, sizeof(type)))
#define Allocate_Array(arena, type, count) \
    rcast<type*>(Allocate_(arena, sizeof(type) * (count)))

#define Allocate_Zeros_For(arena, type) rcast<type*>(Allocate_Zeros_(arena, sizeof(type)))
#define Allocate_Zeros_Array(arena, type, count) \
    rcast<type*>(Allocate_Zeros_(arena, sizeof(type) * (count)))

#define Allocate_Array_And_Initialize(arena, type, count)                  \
    [&]() {                                                                \
        auto ptr = rcast<type*>(Allocate_(arena, sizeof(type) * (count))); \
        FOR_RANGE (int, i, (count)) {                                      \
            std::construct_at(ptr + i);                                    \
        }                                                                  \
        return ptr;                                                        \
    }()

#define Deallocate_Array(arena, type, count) Deallocate_(arena, sizeof(type) * (count))

//
// TODO: Introduce the notion of `alignment` here!
// NOTE: Refer to Casey's memory allocation functions
// https://youtu.be/MvDUe2evkHg?list=PLEMXAbCVnmY6Azbmzj3BiC3QRYHE9QoG7&t=2121
//
u8* Allocate_(Arena& arena, size_t size) {
    Assert(size > 0);
    Assert(arena.size >= size);
    Assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;

#ifdef PROFILING
    // TODO: Изучить способы того, как можно прикрутить профилирование памяти с
    // поддержкой arena аллокаций таким образом, чтобы не приходилось запускать Free в
    // профилировщике для старых аллокаций, когда делаем Reset арен
    //
    // Assert(arena.name != nullptr);
    // TracyAllocN(result, size, arena.name);
#endif  // PROFILING

    return result;
}

u8* Allocate_Zeros_(Arena& arena, size_t size) {
    auto result = Allocate_(arena, size);
    memset(result, 0, size);
    return result;
}

void Deallocate_(Arena& arena, size_t size) {
    Assert(size > 0);
    Assert(arena.used >= size);
    arena.used -= size;

#ifdef PROFILING
    // TODO: См. выше
    //
    // Assert(arena.name != nullptr);
    // TracyFreeN(arena.base + arena.used, arena.name);
#endif
}

// ============================== //
//             Other              //
// ============================== //
[[nodiscard]] BF_FORCE_INLINE u8* Align_Forward(u8* ptr, size_t alignment) noexcept {
    const auto addr         = rcast<size_t>(ptr);
    const auto aligned_addr = (addr + (alignment - 1)) & -alignment;
    return rcast<u8*>(aligned_addr);
}

// TEMP_USAGE используется для временного использования арены.
// При вызове TEMP_USAGE запоминается текущее количество занятого
// пространства арены, которое обратно устанавливается при выходе из scope.
//
// Пример использования:
//
//     size_t X = trash_arena.used;
//     {
//         TEMP_USAGE(trash_arena);
//         int* aboba = ALLOCATE_FOR(trash_arena, u32);
//         Assert(trash_arena.used == X + 4);
//     }
//     Assert(trash_arena.used == X);
//
#define TEMP_USAGE(arena)                     \
    auto _arena_used_ = (arena).used;         \
    defer {                                   \
        Assert((arena).used >= _arena_used_); \
        (arena).used = _arena_used_;          \
    };
