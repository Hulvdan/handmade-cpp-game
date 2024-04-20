#pragma once

// ============================== //
//             Arena              //
// ============================== //
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

// ============================== //
//             Other              //
// ============================== //
[[nodiscard]] BF_FORCE_INLINE u8* Align_Forward(u8* ptr, size_t alignment) noexcept {
    const auto addr         = rcast<size_t>(ptr);
    const auto aligned_addr = (addr + (alignment - 1)) & -alignment;
    return rcast<u8*>(aligned_addr);
}

#define TEMP_USAGE(arena)                     \
    auto _arena_used_ = (arena).used;         \
    defer {                                   \
        Assert((arena).used >= _arena_used_); \
        (arena).used = _arena_used_;          \
    };

// NOTE: Этим штукам в верхнем scope нужны `allocate`, `allocator_data`
#define ALLOC(n) \
    Assert_Not_Null(allocator)(Allocator_Mode::Allocate, (n), 1, 0, 0, allocator_data, 0)

#define FREE(ptr, n)                                                             \
    Assert_Not_Null(allocator)(                                                  \
        Allocator_Mode::Free, sizeof(*ptr) * (n), 1, 0, (ptr), allocator_data, 0 \
    )

#define FREE_ALL \
    Assert_Not_Null(allocator)(Allocator_Mode::Free_All, 0, 1, 0, 0, allocator_data, 0)

#if 0
#define SANITIZE \
    Assert_Not_Null(allocator)(Allocator_Mode::Sanity, 0, 0, 0, 0, allocator_data, 0)
#else
#define SANITIZE [] {}()
#endif

#define MCTX Context* ctx

#define CTX_ALLOCATOR                      \
    auto& allocator      = ctx->allocator; \
    auto& allocator_data = ctx->allocator_data;

#define PUSH_CONTEXT(new_ctx, code) \
    {                               \
        auto ctx = (new_ctx);       \
        (code);                     \
    }

template <typename T>
void Vector_Unordered_Remove_At(std::vector<T>& container, i32 i);

// ==============================

enum class Allocator_Mode {
    Allocate = 0,
    Resize,
    Free,
    Free_All,
    Sanity,
};

#define Allocator__Function(name_)     \
    void* name_(                       \
        Allocator_Mode mode,           \
        size_t         size,           \
        size_t         alignment,      \
        size_t         old_size,       \
        void*          old_memory_ptr, \
        void*          allocator_data, \
        u64            options         \
    )

using Allocator_Function_Type = Allocator__Function((*));

struct Context {
    u32                     thread_index;
    Allocator_Function_Type allocator;
    void*                   allocator_data;

    // NOTE: Сюда можно ещё пихнуть и другие данные,
    // например, что-нибудь для логирования

    Context(
        u32                     a_thread_index,
        Allocator_Function_Type a_allocator,
        void*                   a_allocator_data
    )
        : thread_index(a_thread_index)
        , allocator(a_allocator)
        , allocator_data(a_allocator_data) {}
};

struct Blk {
    void*  ptr;
    size_t length;

    Blk(void* a_ptr, size_t a_length) : ptr(a_ptr), length(a_length) {}

    friend bool operator==(const Blk& a, const Blk& b) {
        return a.ptr == b.ptr && a.length == b.length;
    }
};

template <class P, class F>
struct Fallback_Allocator {
    Blk Allocate(size_t n) {
        Blk r = _p.Allocate(n);
        if (!r.ptr)
            r = _f.Allocate(n);
        return r;
    }

    void Deallocate(Blk b) {
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

struct Null_Allocator {
    Blk Allocate(size_t) { return Blk(nullptr, 0); }

    void Deallocate(Blk b) { Assert(b.ptr == nullptr); }

    bool Owns(Blk b) { return b.ptr == nullptr; }

    bool Sanity_Check() { return true; }
};

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

struct Malloc_Allocator {
    Blk Allocate(size_t n) { return Blk(malloc(n), n); }

    void Deallocate(Blk b) {
        Assert(b.ptr != nullptr);
        Assert(b.length > 0);
        free(b.ptr);
    }

    void Deallocate_All() { NOT_SUPPORTED; }

    bool Sanity_Check() { return true; }
};

struct Freeable_Malloc_Allocator {
    Blk Allocate(size_t n) {
        auto res = Blk(malloc(n), n);
        _allocations.push_back(res);
        return res;
    }

    void Deallocate(Blk b) {
        Assert(b.ptr != nullptr);
        Assert(b.length > 0);

        {  // NOTE: Забываем об адресе
            bool found = false;
            for (auto& allocation : _allocations) {
                if (allocation.ptr == b.ptr && allocation.length == b.length) {
                    Assert(!found);
                    found = true;
                }
            }
            Assert(found);

            auto& v = _allocations;
            v.erase(std::remove(v.begin(), v.end(), b), v.end());
        }

        free(b.ptr);
    }

    void Deallocate_All() {
        for (auto& [ptr, _] : _allocations)
            free(ptr);
        _allocations.clear();
    }

    bool Sanity_Check() { return true; }

private:
    std::vector<Blk> _allocations;
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

template <class A, size_t min, size_t max, i32 min_allocations = 8, i32 top = 1024>
struct Freelist {
    static_assert(top > 0);
    static_assert(min_allocations > 0);
    static_assert(min_allocations <= top);
    static_assert(min <= max);

    Blk Allocate(size_t n) {
        // NOTE: Если наш размер, а также есть предыдущие
        // аллокации в freelist-е, возвращаем их.
        if ((min <= n) && (n <= max) && (_root.next != nullptr)) {
            Blk b(_root.next, n);
            _root.next = _root.next->next;
            return b;
        }

        // NOTE: Пытаемся аллоцировать `min_allocations` раз.
        // Одну аллокацию возвращаем, остальные (если смогли) сохраняем в Freelist.
        auto [ptr, _] = _parent.Allocate(n);

        if ((ptr != nullptr) && (min <= n) && (n <= max)) {
            FOR_RANGE (i32, i, min_allocations - 1) {
                auto allocated_block = _parent.Allocate(n);
                if (allocated_block.ptr == nullptr)
                    break;

                Assert(allocated_block.length == n);

                auto p = (Node*)allocated_block.ptr;

                p->next    = _root.next;
                _root.next = p;
                _remembered++;
            }
        }

        return Blk(ptr, n);
    }

    // TODO: Это код Andrei Alexandrescu.
    // Странно выглядит, что тут проверка на ||.
    // По-идее корректно будет, если мы пробежимся
    // по freelist-у и найдём указанную аллокацию.
    bool Owns(Blk b) {
        return ((min <= b.length) && (b.length <= max)) || _parent.Owns(b);
    }

    void Deallocate(Blk b) {
        // NOTE: Если не заполнен список freelist-а, а также это наш размер,
        // тогда не вызываем free аллокатора, а добавляем в freelist.
        if ((_remembered < top) && (min <= b.length) && (b.length <= max)) {
            auto p     = (Node*)b.ptr;
            p->next    = _root.next;
            _root.next = p;

            _remembered -= 1;
            return;
        }

        _parent.Deallocate(b);
    }

    void Deallocate_All() {
        _remembered = 0;
        _root.next  = nullptr;

        _parent.Deallocate_All();
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

struct Size_Affix {
    size_t n;

    Size_Affix(size_t an) : n(an) {}

    bool Validate() { return true; }
};

template <class A, class Prefix = void, class Suffix = void>
struct Affix_Allocator {
    // From Andrei:
    // - Add optional prefix and suffix
    // - Construct/destroy appropriately
    // - Uses: debug, stats, info, ...

    Blk Allocate(size_t n) {
        auto to_allocate = n;

        if (!std::is_void_v<Prefix>)
            to_allocate += sizeof(Prefix);
        if (!std::is_void_v<Suffix>)
            to_allocate += sizeof(Suffix);

        auto blk = _parent.Allocate(to_allocate);
        auto ptr = (u8*)blk.ptr;

        if (ptr == nullptr)
            return Blk(nullptr, 0);

        {  // NOTE: Проверка, что раньше аллокации с таким же адресом не было
            for (auto& allocation : _allocations) {
                Assert(allocation.ptr != blk.ptr);
            }
            _allocations.push_back(Blk(ptr, to_allocate));
        }

        {  // NOTE: Устанавливаем аффиксы
            if (!std::is_void_v<Prefix>) {
                std::construct_at<Prefix>((Prefix*)ptr, n);
                ptr += sizeof(Prefix);
            }

            if (!std::is_void_v<Suffix>) {
                std::construct_at<Suffix>((Suffix*)(ptr + n), n);
            }
        }

        auto res = Blk((void*)ptr, n);
        return res;
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
        auto ptr = (u8*)b.ptr;

        if (ptr == nullptr) {
            Assert(b.length == 0);
            return _parent.Deallocate(b);
        }

        if (!std::is_void_v<Prefix>) {
            ptr -= sizeof(Prefix);
            b.length += sizeof(Prefix);
        }
        if (!std::is_void_v<Suffix>) {
            b.length += sizeof(Suffix);
        }

        b.ptr = ptr;

        {  // NOTE: Забываем об адресе
            bool found = false;
            for (auto [aptr, alength] : _allocations) {
                if (aptr == b.ptr) {
                    Assert(!found);
                    Assert(alength == b.length);
                    found = true;
                }
            }
            Assert(found);

            auto& v = _allocations;
            v.erase(std::remove(v.begin(), v.end(), b), v.end());
        }

        _parent.Deallocate(b);
    }

    void Deallocate_All() {
        _parent.Deallocate_All();
        _allocations.clear();
    }

    bool Sanity_Check() {
        for (auto blk : _allocations) {
            auto blk_length_without_affixes = blk.length;
            if (!std::is_void_v<Prefix>)
                blk_length_without_affixes -= sizeof(Prefix);
            if (!std::is_void_v<Suffix>)
                blk_length_without_affixes -= sizeof(Suffix);

            auto ptr = (u8*)blk.ptr;

            if (!std::is_void_v<Prefix>) {
                auto affix = (Prefix*)ptr;
                Assert(affix->Validate());

                if (std::is_same_v<Prefix, Size_Affix>) {
                    auto& saffix = *(Size_Affix*)affix;
                    Assert(saffix.n == blk_length_without_affixes);
                }
            }

            ptr += blk.length;

            if (!std::is_void_v<Suffix>) {
                auto affix = (Suffix*)(ptr - sizeof(Suffix));
                Assert(affix->Validate());

                if (std::is_same_v<Prefix, Size_Affix>) {
                    auto& saffix = *(Size_Affix*)affix;
                    Assert(saffix.n == blk_length_without_affixes);
                }
            }
        }

        return _parent.Sanity_Check();
    }

private:
    A _parent;

    std::vector<Blk> _allocations;
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

struct Stoopid_Affix {
    char data[2048];

    Stoopid_Affix(size_t n) {
        FOR_RANGE (int, i, 2048 / 4) {
            data[i * 4 + 0] = (char)124;
            data[i * 4 + 1] = (char)125;
            data[i * 4 + 2] = (char)126;
            data[i * 4 + 3] = (char)127;
        }
    }

    bool Validate() {
        FOR_RANGE (int, i, 2048 / 4) {
            auto a1 = data[i * 4 + 0] == (char)124;
            auto a2 = data[i * 4 + 1] == (char)125;
            auto a3 = data[i * 4 + 2] == (char)126;
            auto a4 = data[i * 4 + 3] == (char)127;

            Assert(a1);
            Assert(a2);
            Assert(a3);
            Assert(a4);

            if (!(a1 && a2 && a3 && a4))
                return false;
        }
        return true;
    }
};

#ifndef Root_Allocator_Type

#if 1
#define Root_Allocator_Type \
    Affix_Allocator<Malloc_Allocator, Stoopid_Affix, Stoopid_Affix>
#else
#define Root_Allocator_Type Malloc_Allocator
#endif

#endif  // Root_Allocator_Type

global Root_Allocator_Type* root_allocator = nullptr;

Allocator__Function(Root_Allocator_Routine) {
    switch (mode) {
    case Allocator_Mode::Allocate: {
        Assert(old_memory_ptr == nullptr);
        Assert(size > 0);
        Assert(old_size == 0);

        return Assert_Deref((Root_Allocator_Type*)root_allocator).Allocate(size).ptr;
    } break;

    case Allocator_Mode::Resize: {
        NOT_IMPLEMENTED;
    } break;

    case Allocator_Mode::Free: {
        Assert(old_memory_ptr != nullptr);
        Assert(size > 0);

        Assert_Deref((Root_Allocator_Type*)root_allocator)
            .Deallocate(Blk(old_memory_ptr, size));
        return nullptr;
    } break;

    case Allocator_Mode::Free_All: {
        Assert_Deref((Root_Allocator_Type*)root_allocator).Deallocate_All();
    } break;

    case Allocator_Mode::Sanity: {
        Assert(old_memory_ptr == nullptr);
        Assert(size == 0);
        Assert(old_size == 0);
        Assert(alignment == 0);

        Assert_Deref((Root_Allocator_Type*)root_allocator).Sanity_Check();
    } break;

    default:
        INVALID_PATH;
    }
    return nullptr;
}

Allocator__Function(Only_Once_Free_All_Root_Allocator) {
    switch (mode) {
    case Allocator_Mode::Allocate: {
        Assert(old_memory_ptr == nullptr);
        Assert(size > 0);
        Assert(old_size == 0);

        return Assert_Deref((Root_Allocator_Type*)root_allocator).Allocate(size).ptr;
    } break;

    case Allocator_Mode::Resize: {
        NOT_IMPLEMENTED;
    } break;

    case Allocator_Mode::Free: {
        Assert(old_memory_ptr != nullptr);
        Assert(size > 0);

        Assert_Deref((Root_Allocator_Type*)root_allocator)
            .Deallocate(Blk(old_memory_ptr, size));
        return nullptr;
    } break;

    case Allocator_Mode::Free_All: {
        NOT_SUPPORTED;
    } break;

    case Allocator_Mode::Sanity: {
        Assert(old_memory_ptr == nullptr);
        Assert(size == 0);
        Assert(old_size == 0);
        Assert(alignment == 0);

        Assert_Deref((Root_Allocator_Type*)root_allocator).Sanity_Check();
    } break;

    default:
        INVALID_PATH;
    }
    return nullptr;
}

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
