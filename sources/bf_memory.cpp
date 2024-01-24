#define AllocateFor(arena, type) (type*)AllocateFor_(arena, sizeof(type))
#define AllocateArray(arena, type, count) (type*)AllocateFor_(arena, sizeof(type) * (count))

u8* AllocateFor_(Arena& arena, size_t size)
{
    assert(size > 0);
    assert(size <= arena.size);
    assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;
    return result;
}
