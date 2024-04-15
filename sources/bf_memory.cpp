struct Arena : public Non_Copyable {
    size_t      used;
    size_t      size;
    u8*         base;
    const char* name;
};

struct Page : public Non_Copyable {
    u8* base;
};

struct Pages : public Non_Copyable {
    size_t total_count_cap;
    size_t allocated_count;
    Page*  base;
    bool*  in_use;
};

#define Allocate_For(arena, type) rcast<type*>(Allocate_(arena, sizeof(type)))
#define Allocate_Array(arena, type, count) \
    rcast<type*>(Allocate_(arena, sizeof(type) * (count)))

#define Allocate_Zeros_For(arena, type) rcast<type*>(Allocate_Zeros_(arena, sizeof(type)))
#define Allocate_Zeros_Array(arena, type, count) \
    rcast<type*>(Allocate_Zeros_(arena, sizeof(type) * (count)))

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

[[nodiscard]] BF_FORCE_INLINE u8* Align_Forward(u8* ptr, size_t alignment) noexcept {
    const auto addr         = rcast<size_t>(ptr);
    const auto aligned_addr = (addr + (alignment - 1)) & -alignment;
    return rcast<u8*>(aligned_addr);
}

// #define A_Active(node_) (node_)->active
// #define A_Next(node_) (node_)->next
// #define A_Base(node_) (node_)->base
// #define A_Size(node_) (node_)->size
//
// struct Allocation {
//     u8* base;
//     size_t size;
//     size_t next;
//     bool active;
// };
//
// NOTE: `toc_pages` должны быть занулены!
// struct Allocator : Non_Copyable {
//     size_t toc_buffer_size;
//     u8* toc_buffer;
//     size_t data_buffer_size;
//     u8* data_buffer;
//
//     size_t current_allocations_count;
//     size_t first_allocation_index;
//
//     size_t max_toc_entries;
//
// #ifdef PROFILING
//     const char* name = nullptr;
// #endif  // PROFILING
//
//     Allocator(
//         size_t a_toc_buffer_size,
//         u8* a_toc_buffer,
//         size_t a_data_buffer_size,
//         u8* a_data_buffer
//     )
//         : toc_buffer(a_toc_buffer)
//         , data_buffer(a_data_buffer)
//         , toc_buffer_size(a_toc_buffer_size)
//         , data_buffer_size(a_data_buffer_size)
//         , current_allocations_count(0)
//         , first_allocation_index(0)
//         , max_toc_entries(a_toc_buffer_size / sizeof(Allocation)) {
//         FOR_RANGE (size_t, i, a_toc_buffer_size) {
//             Assert(*(a_toc_buffer + i) == 0);
//         }
//     }
//
//     u8* Allocate(size_t size) { return Allocate(size, 1); }
//
//     u8* Allocate(size_t size, size_t alignment) {
//         Assert(size > 0);
//         Assert(alignment > 0);
//         Assert(toc_buffer != nullptr);
//         Assert(data_buffer != nullptr);
//
//         if (current_allocations_count + 1 > max_toc_entries) {
//             // TODO: Diagnostic
//             Assert(false);
//             return nullptr;
//         }
//
//         auto nodes = rcast<Allocation*>(toc_buffer);
//
//         auto previous_node_index = size_t_max;
//         auto next_node_index = size_t_max;
//         if (current_allocations_count > 0)
//             next_node_index = first_allocation_index;
//
//         Allocation* previous_node = nullptr;
//         Allocation* next_node = nullptr;
//         u8* base_ptr = Align_Forward(data_buffer, alignment);
//
//         FOR_RANGE (size_t, i, current_allocations_count) {
//             next_node = nodes + next_node_index;
//             Assert(A_Active(next_node));
//
//             if (base_ptr + size > A_Base(next_node)) {
//                 base_ptr
//                     = Align_Forward(A_Base(next_node) + A_Size(next_node), alignment);
//
//                 previous_node = next_node;
//                 previous_node_index = next_node_index;
//                 next_node_index = A_Next(next_node);
//                 next_node = nullptr;
//             }
//             else
//                 break;
//         }
//
//         // Получение незаюзанной ноды
//         size_t new_free_node_index = size_t_max;
//         Allocation* new_free_node = nullptr;
//         {
//             FOR_RANGE (size_t, i, current_allocations_count + 1) {
//                 Allocation* n = nodes + i;
//                 if (A_Active(n))
//                     continue;
//
//                 new_free_node = n;
//                 new_free_node_index = i;
//                 break;
//             }
//             Assert(new_free_node_index != size_t_max);
//             Assert(new_free_node != nullptr);
//         }
//
//         A_Active(new_free_node) = true;
//         A_Size(new_free_node) = size;
//         A_Base(new_free_node) = base_ptr;
//         A_Next(new_free_node) = next_node_index;
//
//         if (previous_node == nullptr)
//             first_allocation_index = new_free_node_index;
//         else
//             A_Next(previous_node) = new_free_node_index;
//
//         current_allocations_count++;
//         return base_ptr;
//     }
//
//     void Free(u8* ptr) {
//         Assert(current_allocations_count > 0);
//         Assert(ptr != nullptr);
//
//         auto nodes = rcast<Allocation*>(toc_buffer);
//         Allocation* previous_node = nullptr;
//         auto current_index = first_allocation_index;
//
//         FOR_RANGE (size_t, i, current_allocations_count) {
//             auto node = nodes + current_index;
//             if (node->base == ptr) {
//                 if (previous_node != nullptr)
//                     A_Next(previous_node) = A_Next(node);
//
//                 if (first_allocation_index == current_index) {
//                     if (A_Next(node) != size_t_max)
//                         first_allocation_index = A_Next(node);
//                     else
//                         first_allocation_index = 0;
//                 }
//
//                 A_Active(node) = false;
//                 current_allocations_count--;
//                 return;
//             }
//
//             previous_node = node;
//             current_index = A_Next(node);
//         }
//
//         INVALID_PATH;
//     }
// };
//
// #ifdef PROFILING
// #define Set_Allocator_Name_If_Profiling(arena_, allocator_, format_, ...) \
//     (allocator_).name = Allocate_Formatted_String((arena_), (format_), __VA_ARGS__)
// #else  // PROFILING
// #define Set_Allocator_Name_If_Profiling(arena_, allocator_, format_, ...)
// #endif  // PROFILING

// NOTE: Стырено с https://en.cppreference.com/w/cpp/named_req/Allocator
#ifdef SHIT_MEMORY_DEBUG
static std::vector<std::tuple<void*, size_t>> bf_debug_allocations = {};
#endif  // SHIT_MEMORY_DEBUG

enum class Allocator_Mode {
    Allocate = 0,
    Resize,
    Free,
    Free_All,
};

#define Allocator__Function(name_)     \
    void name_(                        \
        Allocator_Mode mode,           \
        size_t         size,           \
        size_t         alignment,      \
        size_t         old_size,       \
        void*          old_memory_ptr, \
        void*          allocator_data, \
        u64            options         \
    )

using Allocator_Function_Type = Allocator__Function((*));

#define Alloc(allocator_function, n) \
    (allocator_function)(Allocator_Mode::Allocate, (n), 1, 0, 0, allocator_data, 0)

#define Free(allocator_function, ptr, n) \
    (allocator_function)(Allocator_Mode::Free, (n), 1, 0, (ptr), allocator_data, 0)

#define Free_All(allocator_function, n) \
    (allocator_function)(Allocator_Mode::Free, (n), 1, 0, 0, allocator_data, 0)

// Aligned versions

#define Aligned_Alloc(allocator_function, n, alignment)                     \
    (allocator_function)(                                                   \
        Allocator_Mode::Allocate, (n), (alignment), 0, 0, allocator_data, 0 \
    )

#define Aligned_Free(allocator_function, ptr, n, alignment)                 \
    (allocator_function)(                                                   \
        Allocator_Mode::Free, (n), (alignment), 0, (ptr), allocator_data, 0 \
    )

#define Aligned_Free_All(allocator_function, n, alignment) \
    (allocator_function)(Allocator_Mode::Free, (n), (alignment), 0, 0, allocator_data, 0)

struct Context {
    u32 thread_index;

    Allocator_Function_Type allocator;
    void*                   allocator_data;
};

#define mctx Context* ctx
#define Allocator_Get(allocator_function)

template <typename T, template <typename> typename _Allocator>
void Vector_Unordered_Remove_At(std::vector<T, _Allocator>& container, i32 i);

// template <typename T>
// struct Game_Map_Allocator {
//     typedef T value_type;
//
//     Game_Map_Allocator() = default;
//
//     template <typename U>
//     constexpr Game_Map_Allocator(const Game_Map_Allocator<U>&) noexcept {};
//
//     [[nodiscard]] T* allocate(size_t n) {
//         Assert(n <= size_t_max / sizeof(T));
//         Assert((n * sizeof(T)) / sizeof(T) == n);
//
//         auto p = scast<T*>(_aligned_malloc(n * sizeof(T), alignof(T)));
//         // auto p = scast<T*>(malloc(n * sizeof(T)));
//
// #ifdef SHIT_MEMORY_DEBUG
//         bf_debug_allocations.push_back({p, n});
// #endif  // SHIT_MEMORY_DEBUG
//
//         Assert(p);
//
//         report(p, n);
//         return p;
//     }
//
//     void deallocate(T* p, size_t n) noexcept {
// #ifdef SHIT_MEMORY_DEBUG
//         {
//             bool found = false;
//             size_t existing_n = 0;
//
//             i32 i = 0;
//             for (auto& [pp, nn] : bf_debug_allocations) {
//                 if (p == pp) {
//                     found = true;
//                     existing_n = nn;
//                     break;
//                 }
//                 i++;
//             }
//             Assert(found);
//             Assert(existing_n == n);
//             Vector_Unordered_Remove_At(bf_debug_allocations, i);
//         }
// #endif  // SHIT_MEMORY_DEBUG
//
//         report(p, n, false);
//
//         _aligned_free(p);
//         // free(p);
//     }
//
// private:
//     void report(T* p, size_t n, bool allocated = true) const {
//         // NOTE: Здесь мы можем трекать операции аллокации / деаллокации
//     }
// };
//
// template <class T, class U>
// bool operator==(const Game_Map_Allocator<T>&, const Game_Map_Allocator<U>&) {
//     return true;
// }
//
// template <class T, class U>
// bool operator!=(const Game_Map_Allocator<T>&, const Game_Map_Allocator<U>&) {
//     return false;
// }

// ==============================

struct Blk {
    void*  ptr;
    size_t length;

    Blk(void* a_ptr, size_t a_length) : ptr(a_ptr), length(a_length) {}
};

// Blk Custom_Malloc(size_t size);
//
// void Custom_Free(Blk block);

// ===== Simplest composite allocator =====

// NOTE: COMPLETED
template <class P, class F>
struct Fallback_Allocator {
    Blk Allocate(size_t n) {
        Blk r = _p.Allocate(n);
        if (!r.ptr)
            r = _f.Allocate(n);
        return r;
    }

    void Deallocate(Blk b) {
        // if (P::Owns(b))
        //     P::Deallocate(b);
        // else
        //     F::Deallocate(b);
        if (_p.Owns(b))
            _p.Deallocate(b);
        else
            _f.Deallocate(b);
    }

    bool Owns(Blk b) {
        // NOTE: Используется MDFINAE (method definition is not an error).
        // Не будет ошибки компиляции, если не будет вызываться
        // `owns` для F, у которого не определён этот метод
        return _p.Owns(b) || _f.Owns(b);
    }

    bool Sanity_Check() { return _p.Sanity_Check() && _f.Sanity_Check(); }

private:
    P _p;
    F _f;
};

// ===== Good citizenry =====

// NOTE: COMPLETED
struct Null_Allocator {
    Blk Allocate(size_t) { return Blk(nullptr, 0); }

    void Deallocate(Blk b) { Assert(b.ptr == nullptr); }

    bool Owns(Blk b) { return b.ptr == nullptr; }

    bool Sanity_Check() { return true; }
};

// ===== Suddenly =====

template <size_t s>
struct Stack_Allocator {
    Stack_Allocator() : _buffer(), _current(_buffer) {}

    Blk Allocate(size_t n) {
        Blk result(_current, n);
        _current += n;
        return result;
    }

    void Deallocate(Blk b) {
        _current -= b.length;

        // TODO: Andrei Alexandrescu:
        // if (b.ptr + Round_To_Aligned(n) == _current) {
        //     _current = b.ptr;
        // }
    }

    bool Owns(Blk b) { return b.ptr >= _buffer && b.ptr < _buffer + s; }

    void Deallocate_All() { _current = _buffer; }

    bool Sanity_Check() {
        bool sane = _buffer != nullptr && _current != nullptr;
        Assert(sane);
        return sane;
    }

private:
    u8  _buffer[s];
    u8* _current;
};

// ===== Use =====

// NOTE: COMPLETED
struct Malloc_Allocator {
    Blk Allocate(size_t n) { return Blk(malloc(n), n); }

    void Deallocate(Blk b) {
        Assert(b.ptr != nullptr);
        Assert(b.length > 0);
        free(b.ptr);
    }

    bool Sanity_Check() { return true; }
};
// using Localloc = Fallback_Allocator<Stack_Allocator<16384>, Malloc_Allocator>;

// void fun(size_t n) {
//     Localloc a;
//     auto b = a.Allocate(n * sizeof(int));
//     SCOPE_EXIT { a.Deallocate(b); };
//     int* p = scast<int*>(b.ptr);
//     // ... use p[0] through p[n - 1]...
// }

// ===== Freelist =====

// Keeps list of previous allocations of a given size

// - Add tolerance: min...max
// - Allocate in batches
// - Add upper bound: no more than top elems
//
// Freelist<
//     A,   // parent allocator
//     17,  // Use list for object sizes 17...
//     32,  // .. through 32
//     8,   // Allocate 8 at a time
//     1024 // No more than 1024 remembered
// > alloc8r;
//
// NOTE: COMPLETED
// TODO: TEST
template <class A, size_t min, size_t max, i32 min_allocations = 8, i32 top = 1024>
struct Freelist {
    Blk Allocate(size_t n) {
        // TODO: Поднять assert-ы
        static_assert(top > 0);
        static_assert(min_allocations > 0);
        static_assert(min_allocations <= top);
        static_assert(min <= max);

        // NOTE: Если наш размер, а также есть предыдущие
        // аллокации в freelist-е, возвращаем их.
        if (min <= n && n <= max && _root) {
            Blk b(_root, n);
            _root = _root.next;
            return b;
        }

        // NOTE: Пытаемся аллоцировать `min_allocations` раз.
        // Одну аллокацию возвращаем, остальные (если смогли) сохраняем в Freelist.
        void* ptr = _parent.Allocate(n);

        if (ptr != nullptr && min <= n && n <= max) {
            FOR_RANGE (i32, i, min_allocations - 1) {
                auto p = (Node*)_parent.Allocate(n);
                if (p == nullptr)
                    break;

                p->next = _root;
                _root   = p;
                _remembered++;
            }
        }

        return ptr;
    }

    // TODO: Это код Andrei Alexandrescu.
    // Странно выглядит, что тут проверка на ||.
    // По-идее корректно будет, если мы пробежимся
    // по freelist-у и найдём указанную аллокацию.
    bool Owns(Blk b) { return (min <= b.length && b.length <= max) || _parent.Owns(b); }

    void Deallocate(Blk b) {
        // NOTE: Если не заполнен список freelist-а, а также это наш размер,
        // тогда не вызываем free аллокатора, а добавляем в freelist.
        if (_remembered < top && min <= b.length && b.length <= max) {
            auto p  = (Node*)b.ptr;
            p->next = _root;
            _root   = p;

            _remembered -= 1;
            return;
        }

        _parent.Deallocate(b);
    }

    bool Sanity_Check() { return _parent.Sanity_Check(); }

private:
    A _parent;
    struct Node {
        Node* next;
    } _root;
    i32 _remembered = 0;
};

// ===== Adding Affixes =====

// NOTE: COMPLETED
template <class A, class Prefix = void, class Suffix = void>
struct Affix_Allocator {
    // Add optional prefix and suffix
    // Construct/destroy appropriately
    // Uses: debug, stats, info, ...

    Blk Allocate(size_t n) {
        auto to_allocate = n;

        if (!std::is_void_v<Prefix>)
            to_allocate += sizeof(Prefix);
        if (!std::is_void_v<Suffix>)
            to_allocate += sizeof(Suffix);

        void* ptr = _parent.Allocate(to_allocate);
        if (ptr == nullptr)
            return Blk(nullptr, 0);

        if (!std::is_void_v<Prefix>) {
            std::construct_at<Prefix>(ptr, n);
            ptr += sizeof(Prefix);
        }

        if (!std::is_void_v<Suffix>)
            std::construct_at<Suffix>(ptr + n, n);

        Blk result(ptr, n);
        return result;
    }

    bool Owns(Blk b) {
        if (b.ptr == nullptr) {
            Assert(b.length == 0);
            return _parent.Owns(b);
        }

        if (!std::is_void_v<Prefix>) {
            b.ptr -= sizeof(Prefix);
            b.length += sizeof(Prefix);
        }
        if (!std::is_void_v<Suffix>)
            b.length += sizeof(Suffix);

        return _parent.Owns(b);
    }

    void Deallocate(Blk b) {
        if (b.ptr == nullptr) {
            Assert(b.length == 0);
            return _parent.Deallocate(b);
        }

        if (!std::is_void_v<Prefix>) {
            b.ptr -= sizeof(Prefix);
            b.length += sizeof(Prefix);
        }
        if (!std::is_void_v<Suffix>)
            b.length += sizeof(Suffix);

        _parent.Deallocate(b);
    }

    bool Sanity_Check() {
        // TODO: Прикрутить трекинг аллокаций для тестов.
        // Проверять вокруг них целостность аффиксов.

        return _parent.Sanity_Check();
    }

private:
    A _parent;
};

// template <class A, u32 flags>
// class Allocator_With_Stats {
//     // Collect info about any other allocator
//     // Per-allocation and global
//     //     (primitive calls, failures, byte counts, high tide)
//     // Per-allocation
//     //     (caller file/line/function/time)
// };

// ===== Bitmapped Block =====

constexpr bool Is_Power_Of_2(const size_t number) {
    size_t n = 1;

    while (n < number)
        n *= 2;

    return number == n;
}

// NOTE: Small аллокатор, похожий на тот, что схематично приведён в книге
// "Game Engine Gems 2. Ch. 26. A Highly Optimized Portable Memory Manager".
// `dlmalloc` подробно не изучал, когда эту штуку писал.
//
// NOTE: Аллоцирует 4*(block_size**2) байт памяти под 4*block_size блоков.
//
// Если block_size ==  32 => аллоцирует  4096 байт (128 блоков)
// Если block_size ==  64 => аллоцирует 16384 байт (256 блоков)
// Если block_size == 128 => аллоцирует 65536 байт (512 блоков)
//
// 1 блок (последний) всегда уходит на bookkeeping
//
template <class A, size_t block_size>
class Bitmapped_Allocator {
    // Andrei Alexandrescu:
    // - Organized by constant-size blocks
    // - Tremendously simpler than malloc's heap
    // - Faster, too
    // - Layered on top of any memory hunk
    // - 1 bit/block sole overhead (!)
    // - Multithreading tenuous (needs full interlocking for >1 block)
    //
    // TODO: Я хз, как Andrei Alexandrescu бы это реализовывал
    //
    // TODO: Зачекать paper https://arxiv.org/pdf/2110.10357.pdf
    // "Fast Bitmap Fit: A CPU Cache Line friendly
    //  memory allocator for single object allocations"

    Blk Allocate(size_t n) {
        static_assert(Is_Power_Of_2(block_size));

        if (_blocks == nullptr) {
            _blocks          = _parent.Allocate(Total_Blocks_Count());
            _occupied        = _blocks + Last_Block_Offset();
            _allocation_bits = _occupied + block_size / 2;
            memset(_occupied, 0, block_size);
        }

        size_t required_blocks = Ceil_Division(n, block_size);

        size_t location = 0;
        while (location <= Total_Blocks_Count() - required_blocks) {
            size_t available = 0;
            FOR_RANGE (size_t, i, required_blocks) {
                if (QUERY_BIT(_occupied, i))
                    break;

                available++;
            }

            if (available == required_blocks) {
                void* ptr = (void*)_blocks + block_size * location;

                Assert(!QUERY_BIT(_allocation_bits, location));
                MARK_BIT(_allocation_bits, location);

                FOR_RANGE (size_t, i, required_blocks) {
                    Assert(!QUERY_BIT(_occupied, location + i));
                    MARK_BIT(_occupied, location + i);
                }

                return Blk(ptr, n);
            }
            else
                location = location + available + 1;
        }

        return Blk(nullptr, 0);
    }

    bool Owns(Blk b) {
        return (b.ptr >= _blocks) && (b.ptr < _blocks + Last_Block_Offset());
    }

    void Deallocate(Blk b) {
        if (b.ptr == nullptr) {
            Assert(b.length == 0);
            return;
        }

        size_t block = (b.ptr - _blocks) / block_size;
        Assert(block < Total_Blocks_Count());

        // Ensure this is the start of the allocation
        Assert(b.ptr % block_size == 0);
        Assert(QUERY_BIT(_allocation_bits, block));

        // Unmarking allocation bit
        UNMARK_BIT(_allocation_bits, block);

        // Unmarking occupied bits
        FOR_RANGE (size_t, i, Ceil_Division(b.length, block_size)) {
            Assert(QUERY_BIT(_occupied, block + i));
            UNMARK_BIT(_occupied, block + i);

            Assert(!QUERY_BIT(_allocation_bits, block + i));
        }
    }

    bool Sanity_Check() {
        // NOTE: У блоков, отмеченных в качестве начальных для аллокаций,
        // обязательно должно стоять значение в _occupied бите.
        FOR_RANGE (size_t, i, Total_Blocks_Count()) {
            if (QUERY_BIT(_allocation_bits, i)) {
                Assert(QUERY_BIT(_occupied, i));
                return false;
            }
        }

        return true;
    }

private:
    consteval size_t Last_Block_Offset() { return block_size * 4 - 1; }
    consteval size_t Total_Blocks_Count() { return block_size * 4; }

    A _parent;  // NOTE: Аллокатор для аллокации блоков

    void* _blocks;
    u8* _occupied;  // NOTE: Если бит = 1, значит, этот блок занят
    u8* _allocation_bits;  // NOTE: Если бит = 1, значит, это начало новой аллокации
};

template <class A>
struct Cascading_Allocator {
    // From Andrei Alexandrescu:
    // - Keep a list of allocators, grown lazily
    // - Coarse granularity
    // - Linear search upon allocation
    // - May return memory back
    Blk Allocate(size_t n) {
        for (auto& allocator : _allocators) {
            auto p = allocator.Allocate(n);
            if (p != nullptr)
                return p;
        }

        auto& allocator = _allocators.emplace_back();
        auto  result    = allocator.Allocate(n);
        return result;
    }

    bool Owns(Blk b) {
        for (auto& allocator : _allocators) {
            if (allocator.Owns(b))
                return true;
        }
        return false;
    }

    void Deallocate(Blk b) {
        for (auto& allocator : _allocators) {
            if (allocator.Owns(b)) {
                allocator.Deallocate(b);
                return;
            }
        }
        Assert(false);
    }

    bool Sanity_Check() {
        for (auto& allocator : _allocators) {
            if (!allocator.Sanity_Check())
                return false;
        }
        return true;
    }

private:
    // TODO: убрать vector. Добавить новый аллокатор-параметр шаблона,
    // который будет аллоцировать и реаллоцировать массив
    tvector<A> _allocators;
};

// auto a = Cascading_Allocator({
// return Heap<...>();
// });

// ===== Segregator =====

// NOTE: COMPLETED
// NOTE: Всё, что <= threshold идёт в A1. Остальное - в A2.
template <size_t threshold, class A1, class A2>
struct Segregator {
    Blk Allocate(size_t n) {
        if (n <= threshold)
            return _parent1.Allocate(n);
        else
            return _parent2.Allocate(n);
    }

    bool Owns(Blk b) {
        if (b.length <= threshold)
            return _parent1.Owns(b);
        else
            return _parent2.Owns(b);
    }

    void Deallocate(Blk b) {
        if (b.length <= threshold)
            return _parent1.Deallocate(b);
        else
            return _parent2.Deallocate(b);
    }

    bool Sanity_Check() {
        if (!_parent1.Sanity_Check())
            return false;

        if (!_parent2.Sanity_Check())
            return false;

        return true;
    }

private:
    A1 _parent1;
    A2 _parent2;
};

// ===== Segregator: self-composable! =====

// - Could implement any "search" strategy
// - Works only for a handful of size classes
// typedef Segregator<
//     4096,
//     Segregator<128, Freelist<Malloc_Allocator, 0, 128>, Freelist<Malloc_Allocator, 129,
//     512>>, Malloc_Allocator> Custom_Allocator;

// ===== Bucketizer: Size Classes =====

// template <class Allocator, size_t min, size_t max, size_t step>
// struct Bucketizer {
//     // Linear Buckets:
//     //     [min + 0 * step, min + 1 * step),
//     //     [min + 1 * step, min + 2 * step),
//     //     [min + 2 * step, min + 3 * step)...
//     // Exponential Buckets:
//     //     [min * pow(step, 0), min * pow(step, 1)),
//     //     [min * pow(step, 1), min * pow(step, 2)],
//     //     [min * pow(step, 2), min * pow(step, 3)]...
//     //
//     // Within a bucket allocates the maximum size
// };

// ===== We're done! =====
// Most major allocation tropes covered

// ===== Approach to copying =====
// - Allocator-dependent
// - Some noncopyable & immovable: stack/in-situ allocator
// - Some noncopyable, movable: non-stack regions
// - Many refcounted, movable
// - Perhaps other models, too

// ===== Granularity =====
// - Where are types, factories, ... ?
// - Design surprise: these are extricable
// - Focus on Blk

// ===== Realistic Example =====
// using A = Segregator<
//     8,
//     Freelist<Malloc_Allocator, 0, 8>,
//     128,
//     Bucketizer<FList, 1, 128, 16>,
//     256,
//     Bucketizer<FList, 129, 256, 32>,
//     512,
//     Bucketizer<FList, 257, 512, 64>,
//     1024,
//     Bucketizer<FList, 513, 1024, 128>,
//     2048,
//     Bucketizer<FList, 1025, 2048, 256>,
//     3584,
//     Bucketizer<FList, 2049, 3584, 512>,
//     4072 * 1024,
//     Cascading_Allocator<
//         Fallback_Allocator<Bitmapped_Allocator<Malloc_Allocator, 32>, Null_Allocator>>,
//     Malloc_Allocator>;

// ========================

// using Game_Map_Allocator = Malloc_Allocator;

// ========================

// template <class A>
// using FList = Freelist<A, 0, size_t_max>;
//
// using Game_Map_Allocator = Segregator<
//     8,
//     FList<  //
//         Cascading_Allocator<  //
//             Bitmapped_Allocator<Malloc_Allocator, 32>>>,
//     Fallback_Allocator<  //
//         FList<  //
//             Cascading_Allocator<  //
//                 Bitmapped_Allocator<Malloc_Allocator, 128>>>,
//         Malloc_Allocator>>;

using Game_Map_Allocator = Affix_Allocator<Malloc_Allocator>;

// ========================

// ===== Complete API =====
// namespace {
//
// static constexpr unsigned alignment;
// static constexpr Good_Size(size_t);
// Blk Allocate(size_t);
// bool Expand(Blk&, size_t delta);
// void Reallocate(Blk&, size_t);
// bool Owns(Blk); // optional
// void Deallocate(Blk);
// bool Sanity_Check();
// // aligned versions:
// // - Aligned_Malloc_Allocator
// // - `posix_memalign` on Posix
// // - `_aligned_malloc` on Windows
// // - Regions support this as well!
// Blk Aligned_Allocate(size_t, unsigned);
// Blk Aligned_Reallocate(size_t, unsigned);
//
// // Для stack аллокатора
// Blk Allocate_All();
// void Deallocate_All();
//
//
// }  // namespace

// ===== Summary =====
// - Fresh approach from first principles
// - Understanding history
//     - Otherwise: "... doomed to repeat it"
// - Composability is key
