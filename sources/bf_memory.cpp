#define Allocate_For(arena, type) rcast<type*>(Allocate_(arena, sizeof(type)))
#define Allocate_Array(arena, type, count) rcast<type*>(Allocate_(arena, sizeof(type) * (count)))

#define Allocate_Zeros_For(arena, type) rcast<type*>(Allocate_Zeros_(arena, sizeof(type)))
#define Allocate_Zeros_Array(arena, type, count) \
    rcast<type*>(Allocate_Zeros_(arena, sizeof(type) * (count)))

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

[[nodiscard]] inline u8* Align_Forward(u8* ptr, size_t alignment) noexcept {
    const auto addr = bcast<size_t>(ptr);
    const auto aligned_addr = (addr + (alignment - 1)) & -alignment;
    return bcast<u8*>(aligned_addr);
}

#define A_Active(node_) *rcast<bool*>((node_) + active_offset)
#define A_Next(node_) *rcast<size_t*>((node_) + next_offset)
#define A_Base(node_) *rcast<u8**>((node_) + base_offset)
#define A_Size(node_) *rcast<size_t*>((node_) + size_offset)

// NOTE(hulvdan): `toc_page` должно быть занулено!
struct Allocator : Non_Copyable {
    // int pages_count;
    // int used_pages_count;
    Page* allocation_pages;
    Page* toc_page;

    size_t current_allocations_count;
    size_t first_allocation_index;

    // Allocator(Page& a_toc_page, Page* a_allocation_pages, size_t a_allocation_pages_count)
    //     : first_allocation_index(0), allocations(a_allocation_pages) {}

    std::tuple<size_t, u8*> Allocate(
        size_t size,
        size_t alignment,
        size_t active_offset,
        size_t base_offset,
        size_t size_offset,
        size_t next_offset,
        size_t node_size  //
    ) {
        assert(size > 0);
        assert(alignment > 0);
        assert(toc_page != nullptr);
        assert(allocation_pages != nullptr);

        u8* nodes = toc_page->base;

        auto previous_node_index = size_t_max;
        auto next_node_index = size_t_max;
        if (current_allocations_count > 0)
            next_node_index = first_allocation_index;

        u8* previous_node = nullptr;
        u8* next_node = nullptr;
        u8* base_ptr = Align_Forward(allocation_pages->base, alignment);

        FOR_RANGE(size_t, i, current_allocations_count) {
            next_node = nodes + next_node_index * node_size;
            assert(A_Active(next_node));

            if (base_ptr + size > A_Base(next_node)) {
                base_ptr = Align_Forward(A_Base(next_node) + A_Size(next_node), alignment);

                previous_node = next_node;
                previous_node_index = next_node_index;
                next_node_index = A_Next(next_node);
                next_node = nullptr;
            } else
                break;
        }

        // Получение незаюзанной ноды
        size_t new_free_node_index = size_t_max;
        u8* new_free_node = nullptr;
        {
            FOR_RANGE(size_t, i, current_allocations_count + 1) {
                u8* n = nodes + i * node_size;
                if (A_Active(n))
                    continue;

                new_free_node = n;
                new_free_node_index = i;
                break;
            }
            assert(new_free_node_index != size_t_max);
            assert(new_free_node != nullptr);
        }

        A_Active(new_free_node) = true;
        A_Size(new_free_node) = size;
        A_Base(new_free_node) = base_ptr;
        A_Next(new_free_node) = next_node_index;

        if (previous_node == nullptr)
            first_allocation_index = new_free_node_index;
        else
            A_Next(previous_node) = new_free_node_index;

        current_allocations_count++;
        return {new_free_node_index, base_ptr};
    }

    void Free(
        size_t key,
        size_t active_offset,
        size_t base_offset,
        size_t size_offset,
        size_t next_offset,
        size_t node_size  //
    ) {
        assert(current_allocations_count > 0);

        u8* nodes = toc_page->base;
        u8* previous_node = nullptr;
        auto current_index = first_allocation_index;

        FOR_RANGE(size_t, i, current_allocations_count) {
            auto node = nodes + current_index * node_size;
            if (current_index == key) {
                if (previous_node != nullptr)
                    A_Next(previous_node) = A_Next(node);

                if (first_allocation_index == current_index) {
                    if (A_Next(node) != size_t_max)
                        first_allocation_index = A_Next(node);
                    else
                        first_allocation_index = 0;
                }

                A_Active(node) = false;
                current_allocations_count--;
                return;
            }

            previous_node = node;
            current_index = A_Next(node);
        }

        INVALID_PATH;
    }
};
