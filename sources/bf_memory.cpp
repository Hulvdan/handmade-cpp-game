// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
#define Allocate_For(arena, type) rcast<type*>(Allocate_(arena, sizeof(type)))
#define Allocate_Array(arena, type, count) rcast<type*>(Allocate_(arena, sizeof(type) * (count)))

#define Allocate_Zeros_For(arena, type) rcast<type*>(Allocate_Zeros_(arena, sizeof(type)))
#define Allocate_Zeros_Array(arena, type, count) \
    rcast<type*>(Allocate_Zeros_(arena, sizeof(type) * (count)))
// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

#define Deallocate_Array(arena, type, count) Deallocate_(arena, sizeof(type) * (count))

// TODO(hulvdan): Introduce the notion of `alignment` here!
// NOTE(hulvdan): Refer to Casey's memory allocation functions
// https://youtu.be/MvDUe2evkHg?list=PLEMXAbCVnmY6Azbmzj3BiC3QRYHE9QoG7&t=2121
u8* Allocate_(Arena& arena, size_t size) {
    assert(size > 0);
    assert(arena.size >= size);
    assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;
    return result;
}

u8* Allocate_Zeros_(Arena& arena, size_t size) {
    auto result = Allocate_(arena, size);
    memset(result, 0, size);
    return result;
}

void Deallocate_(Arena& arena, size_t size) {
    assert(size > 0);
    assert(arena.used >= size);
    arena.used -= size;
}

u8* Book_Single_Page(Game_State& state) {
    auto& pages = state.pages;
    auto& os_data = *state.os_data;

    // NOTE(hulvdan): If there exists allocated page that is not in use -> return it
    FOR_RANGE(u32, i, pages.allocated_count) {
        bool& in_use = *(pages.in_use + i);
        if (!in_use) {
            in_use = true;
            return (pages.base + i)->base;
        }
    }

    // NOTE(hulvdan): Allocating more pages and mapping them
    assert(pages.allocated_count < pages.total_count_cap);

    auto pages_to_allocate = os_data.min_pages_per_allocation;
    auto allocation_address = os_data.Allocate_Pages(pages_to_allocate);

    FOR_RANGE(u32, i, pages_to_allocate) {
        auto& page = *(pages.base + pages.allocated_count + i);
        page.base = allocation_address + (ptrd)i * os_data.page_size;
    }

    // NOTE(hulvdan): Booking the first page that we allocated and returning it
    Page* result = pages.base + (ptrd)pages.allocated_count;

    *(pages.in_use + pages.allocated_count) = true;
    pages.allocated_count += pages_to_allocate;

    return result->base;
}
