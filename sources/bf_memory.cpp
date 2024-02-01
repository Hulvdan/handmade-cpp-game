// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
#define Allocate_For(arena, type) rcast<type*>(Allocate_(arena, sizeof(type)))
#define Allocate_Array(arena, type, count) rcast<type*>(Allocate_(arena, sizeof(type) * (count)))
// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

#define Deallocate_Array(arena, type, count) Deallocate_(arena, sizeof(type) * (count))

u8* Allocate_(Arena& arena, size_t size) {
    assert(size > 0);
    assert(arena.size >= size);
    assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;
    return result;
}

void Deallocate_(Arena& arena, size_t size) {
    assert(size > 0);
    assert(arena.used >= size);
    arena.used -= size;
}
