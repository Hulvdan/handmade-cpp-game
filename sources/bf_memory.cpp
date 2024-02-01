#define Allocate_For(arena, type) (type*)Allocate_(arena, sizeof(type))
#define Allocate_Array(arena, type, count) (type*)Allocate_(arena, sizeof(type) * (count))
#define Deallocate_Array(arena, type, count) Deallocate_(arena, sizeof(type) * (count))

u8* Allocate_(Arena& arena, size_t size)
{
    assert(size > 0);
    assert(arena.size >= size);
    assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;
    return result;
}

void Deallocate_(Arena& arena, size_t size)
{
    assert(size > 0);
    assert(arena.used >= size);
    arena.used -= size;
}
