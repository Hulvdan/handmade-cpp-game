#define Allocate_For(arena, type) (type*)Allocate_For_(arena, sizeof(type))
#define Allocate_Array(arena, type, count) (type*)Allocate_For_(arena, sizeof(type) * (count))

u8* Allocate_For_(Arena& arena, size_t size)
{
    assert(size > 0);
    assert(size <= arena.size);
    assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;
    return result;
}
