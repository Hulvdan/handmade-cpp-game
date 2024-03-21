struct Arena : public Non_Copyable {
    size_t used;
    size_t size;
    u8* base;
    const char* name;
};

struct Page : public Non_Copyable {
    u8* base;
};

struct Pages : public Non_Copyable {
    size_t total_count_cap;
    size_t allocated_count;
    Page* base;
    bool* in_use;
};

#define Allocate_For(arena, type) rcast<type*>(Allocate_(arena, sizeof(type)))
#define Allocate_Array(arena, type, count) \
    rcast<type*>(Allocate_(arena, sizeof(type) * (count)))

#define Allocate_Zeros_For(arena, type) rcast<type*>(Allocate_Zeros_(arena, sizeof(type)))
#define Allocate_Zeros_Array(arena, type, count) \
    rcast<type*>(Allocate_Zeros_(arena, sizeof(type) * (count)))

#define Deallocate_Array(arena, type, count) Deallocate_(arena, sizeof(type) * (count))

// TODO(hulvdan): Introduce the notion of `alignment` here!
// NOTE(hulvdan): Refer to Casey's memory allocation functions
// https://youtu.be/MvDUe2evkHg?list=PLEMXAbCVnmY6Azbmzj3BiC3QRYHE9QoG7&t=2121
u8* Allocate_(Arena& arena, size_t size) {
    Assert(size > 0);
    Assert(arena.size >= size);
    Assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;

#ifdef PROFILING
    // TODO(hulvdan): Изучить способы того, как можно прикрутить профилирование памяти с
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
    // TODO(hulvdan): См. выше
    //
    // Assert(arena.name != nullptr);
    // TracyFreeN(arena.base + arena.used, arena.name);
#endif
}

[[nodiscard]] inline u8* Align_Forward(u8* ptr, size_t alignment) noexcept {
    const auto addr = rcast<size_t>(ptr);
    const auto aligned_addr = (addr + (alignment - 1)) & -alignment;
    return rcast<u8*>(aligned_addr);
}

#define A_Active(node_) (node_)->active
#define A_Next(node_) (node_)->next
#define A_Base(node_) (node_)->base
#define A_Size(node_) (node_)->size

struct Allocation {
    u8* base;
    size_t size;
    size_t next;
    bool active;
};

// NOTE(hulvdan): `toc_pages` должны быть занулены!
struct Allocator : Non_Copyable {
    size_t toc_buffer_size;
    u8* toc_buffer;
    size_t data_buffer_size;
    u8* data_buffer;

    size_t current_allocations_count;
    size_t first_allocation_index;

    size_t max_toc_entries;

#ifdef PROFILING
    const char* name = nullptr;
#endif  // PROFILING

    Allocator(
        size_t a_toc_buffer_size,
        u8* a_toc_buffer,
        size_t a_data_buffer_size,
        u8* a_data_buffer  //
        )
        : toc_buffer(a_toc_buffer),
          data_buffer(a_data_buffer),
          toc_buffer_size(a_toc_buffer_size),
          data_buffer_size(a_data_buffer_size),
          current_allocations_count(0),
          first_allocation_index(0),
          max_toc_entries(a_toc_buffer_size / sizeof(Allocation))  //
    {
        FOR_RANGE(size_t, i, a_toc_buffer_size) {
            Assert(*(a_toc_buffer + i) == 0);
        }
    }

    ttuple<size_t, u8*> Allocate(size_t size, size_t alignment) {
        Assert(size > 0);
        Assert(alignment > 0);
        Assert(toc_buffer != nullptr);
        Assert(data_buffer != nullptr);

        if (current_allocations_count + 1 > max_toc_entries) {
            // TODO(hulvdan): Diagnostic
            Assert(false);
            return {size_t_max, nullptr};
        }

        auto nodes = rcast<Allocation*>(toc_buffer);

        auto previous_node_index = size_t_max;
        auto next_node_index = size_t_max;
        if (current_allocations_count > 0)
            next_node_index = first_allocation_index;

        Allocation* previous_node = nullptr;
        Allocation* next_node = nullptr;
        u8* base_ptr = Align_Forward(data_buffer, alignment);

        FOR_RANGE(size_t, i, current_allocations_count) {
            next_node = nodes + next_node_index;
            Assert(A_Active(next_node));

            if (base_ptr + size > A_Base(next_node)) {
                base_ptr =
                    Align_Forward(A_Base(next_node) + A_Size(next_node), alignment);

                previous_node = next_node;
                previous_node_index = next_node_index;
                next_node_index = A_Next(next_node);
                next_node = nullptr;
            } else
                break;
        }

        // Получение незаюзанной ноды
        size_t new_free_node_index = size_t_max;
        Allocation* new_free_node = nullptr;
        {
            FOR_RANGE(size_t, i, current_allocations_count + 1) {
                Allocation* n = nodes + i;
                if (A_Active(n))
                    continue;

                new_free_node = n;
                new_free_node_index = i;
                break;
            }
            Assert(new_free_node_index != size_t_max);
            Assert(new_free_node != nullptr);
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

    void Free(size_t key) {
        Assert(current_allocations_count > 0);
        Assert(key != size_t_max);

        auto nodes = rcast<Allocation*>(toc_buffer);
        Allocation* previous_node = nullptr;
        auto current_index = first_allocation_index;

        FOR_RANGE(size_t, i, current_allocations_count) {
            auto node = nodes + current_index;
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

#ifdef PROFILING
#define Set_Allocator_Name_If_Profiling(arena_, allocator_, format_, ...) \
    (allocator_).name = Allocate_Formatted_String((arena_), (format_), __VA_ARGS__)
#else  // PROFILING
#define Set_Allocator_Name_If_Profiling(arena_, allocator_, format_, ...)
#endif  // PROFILING
