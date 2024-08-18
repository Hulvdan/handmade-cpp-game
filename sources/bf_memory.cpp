#ifndef Root_Allocator_Type
#    if BF_SANITIZATION_ENABLED
#        define Root_Allocator_Type  \
            DEBUG_Affix_Allocator<   \
                Malloc_Allocator,    \
                DEBUG_Stoopid_Affix, \
                DEBUG_Stoopid_Affix>
#    else
#        define Root_Allocator_Type Malloc_Allocator
#    endif
#endif

#define BF_MEMORY_COALESCE_(value, fallback) (((value) != nullptr) ? (value) : (fallback))

// Этим штукам в верхнем scope нужны `allocate`, `allocator_data`
#define ALLOC(n)                                                  \
    (BF_MEMORY_COALESCE_(allocator, Root_Allocator_Routine)(      \
        Allocator_Mode::Allocate, (n), 1, 0, 0, allocator_data, 0 \
    ))

#define ALLOC_ZEROS(n)        \
    [&]() {                   \
        auto addr = ALLOC(n); \
        memset(addr, 0, n);   \
        return addr;          \
    }()

#define REALLOC(new_bytes_size, old_bytes_size, old_ptr)      \
    (BF_MEMORY_COALESCE_(allocator, Root_Allocator_Routine))( \
        Allocator_Mode::Resize,                               \
        (new_bytes_size),                                     \
        1,                                                    \
        (old_bytes_size),                                     \
        (old_ptr),                                            \
        allocator_data,                                       \
        0                                                     \
    )

#define FREE(ptr, bytes_size)                                              \
    (BF_MEMORY_COALESCE_(allocator, Root_Allocator_Routine))(              \
        Allocator_Mode::Free, (bytes_size), 1, 0, (ptr), allocator_data, 0 \
    )

#define FREE_ALL                                                \
    (BF_MEMORY_COALESCE_(allocator, Root_Allocator_Routine))(   \
        Allocator_Mode::Free_All, 0, 1, 0, 0, allocator_data, 0 \
    )

#if (SANITIZATION_ENABLED == 1)
#    define SANITIZE                                              \
        (BF_MEMORY_COALESCE_(allocator, Root_Allocator_Routine))( \
            Allocator_Mode::Sanity, 0, 0, 0, 0, allocator_data, 0 \
        )
#else
#    define SANITIZE ((void)0)
#endif

#define MCTX Context* ctx
#define MCTX_ Context* /* ctx */

#define CTX_ALLOCATOR                      \
    auto& allocator      = ctx->allocator; \
    auto& allocator_data = ctx->allocator_data;

#define PUSH_CONTEXT(new_ctx, code) \
    STATEMENT({                     \
        auto ctx = (new_ctx);       \
        (code);                     \
    })

enum class Allocator_Mode {
    Allocate = 0,
    Resize,
    Free,
    Free_All,
    Sanity,
};

#define Allocator_function(name_)                   \
    /* NOLINTNEXTLINE(bugprone-macro-parentheses)*/ \
    void* name_(                                    \
        Allocator_Mode mode,                        \
        size_t         size,                        \
        size_t         alignment,                   \
        size_t         old_size,                    \
        void*          old_memory_ptr,              \
        void*          allocator_data,              \
        u64            options                      \
    )

using Allocator_function_t = Allocator_function((*));

struct Context {
    u32 thread_index = {};

    Allocator_function_t allocator      = {};
    void*                allocator_data = {};

    void*                   logger_data          = {};
    Logger_function_t       logger_routine       = {};
    Logger_Scope_function_t logger_scope_routine = {};
};

struct Blk {
    void*  ptr    = {};
    size_t length = {};

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
        // Из презентации Andrei Alexandrescu:
        //
        //     Используется MDFINAE (method definition failure is not an error).
        //     Не будет ошибки компиляции, если не будет вызываться
        //     `owns` для F, у которого не определён этот метод
        //
        return _p.Owns(b) || _f.Owns(b);
    }

    bool Sanity_Check() {
        return _p.Sanity_Check() && _f.Sanity_Check();
    }

private:
    P _p;
    F _f;
};

struct Null_Allocator {
    Blk Allocate(size_t) {
        return Blk(nullptr, 0);
    }

    void Deallocate(Blk b) {
        Assert(b.ptr == nullptr);
    }

    bool Owns(Blk b) {
        return b.ptr == nullptr;
    }

    bool Sanity_Check() {
        return true;
    }
};

template <size_t s>
struct Stack_Allocator {
    Stack_Allocator()
        : _buffer()
        , _current(_buffer) {}

    Blk Allocate(size_t n) {
        Blk result(_current, n);
        _current += n;
        return result;
    }

    void Deallocate(Blk b) {
        _current -= b.length;

        // Из презентации Andrei Alexandrescu:
        //
        //     if (b.ptr + Round_To_Aligned(n) == _current) {
        //         _current = b.ptr;
        //     }
    }

    bool Owns(Blk b) {
        return b.ptr >= _buffer && b.ptr < _buffer + s;
    }

    void Deallocate_All() {
        _current = _buffer;
    }

    bool Sanity_Check() {
        bool sane = _buffer != nullptr && _current != nullptr;
        Assert(sane);
        return sane;
    }

private:
    u8  _buffer[s];
    u8* _current;
};

struct Malloc_Allocator {
    Blk Allocate(size_t n) {
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
        auto result = Blk(malloc(n), n);
        return result;
    }

    void Deallocate(Blk b) {
        Assert(b.ptr != nullptr);
        Assert(b.length > 0);
        free(b.ptr);
    }

    void Deallocate_All() {
        NOT_SUPPORTED;
    }

    bool Sanity_Check() {
        return true;
    }
};

struct Freeable_Malloc_Allocator {
    Blk Allocate(size_t n) {
        auto result = Blk(malloc(n), n);
        _allocations.push_back(result);
        return result;
    }

    void Deallocate(Blk b) {
        Assert(b.ptr != nullptr);
        Assert(b.length > 0);

        {  // Забываем об адресе.
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

    bool Sanity_Check() {
        return true;
    }

private:
    std::vector<Blk> _allocations;
};

//
// Из презентации Andrei Alexandrescu:
//
//     using Localloc = Fallback_Allocator<Stack_Allocator<16384>, Malloc_Allocator>;
//
//     void fun(size_t n) {
//         Localloc a;
//         auto b = a.Allocate(n * sizeof(int));
//         SCOPE_EXIT { a.Deallocate(b); };
//         int* p = scast<int*>(b.ptr);
//         // ... use p[0] through p[n - 1]...
//     }
//
//     Freelist keeps list of previous allocations of a given size
//
//     - Add tolerance: min...max
//     - Allocate in batches
//     - Add upper bound: no more than top elems
//
//     Freelist<
//         A,   // parent allocator
//         17,  // Use list for object sizes 17...
//         32,  // .. through 32
//         8,   // Allocate 8 at a time
//         1024 // No more than 1024 remembered
//     > allocator;
//
template <class A, size_t min, size_t max, i32 min_allocations = 8, i32 top = 1024>
struct Freelist {
    static_assert(top > 0);
    static_assert(min_allocations > 0);
    static_assert(min_allocations <= top);
    static_assert(min <= max);

    Blk Allocate(size_t n) {
        // Если наш размер, а также есть предыдущие.
        // аллокации в freelist-е, возвращаем их.
        if ((min <= n) && (n <= max) && (_root.next != nullptr)) {
            Blk b(_root.next, n);
            _root.next = _root.next->next;
            return b;
        }

        // Пытаемся аллоцировать `min_allocations` раз.
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

    // NOTE: Это код Andrei Alexandrescu.
    // Странно выглядит, что тут проверка на `||`.
    // По-идее корректно будет, если мы пробежимся
    // по freelist-у и найдём указанную аллокацию.
    bool Owns(Blk b) {
        return ((min <= b.length) && (b.length <= max)) || _parent.Owns(b);
    }

    void Deallocate(Blk b) {
        // Если не заполнен список freelist-а, а также это наш размер,
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

    bool Sanity_Check() {
        return _parent.Sanity_Check();
    }

private:
    A _parent;
    struct Node {
        Node* next;
    } _root;
    i32 _remembered = 0;
};

#if BF_DEBUG

//
// Аффикс для установления размера выделенной памяти по её бокам.
// Логика выставления значения и валидации вкручена в DEBUG_Affix_Allocator.
//
struct DEBUG_Size_Affix {
    size_t n;

    DEBUG_Size_Affix(size_t an)
        : n(an) {}

    bool Validate() {
        return true;
    }
};

//
// Аффикс размером в 2048 байт с валидацией.
//
struct DEBUG_Stoopid_Affix {
    char data[2048] = {};

    DEBUG_Stoopid_Affix(size_t /* n */) {
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

//
// Аффикс аллокатор используется исключительно для дебага.
// Помогает с проверкой целостности памяти.
//
// Он устанавливает префикс и/или постфикс вокруг аллокаций,
// которые могут исполнять свою логику валидации данных.
//
// Штука медленная из-за трекинга и проверки целостости
// всех аффиксов каждой аллокации, когда вызывается `SANITIZE`.
//
template <class A, class Prefix = void, class Suffix = void>
struct DEBUG_Affix_Allocator {
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

        {  // Проверка, что раньше аллокации с таким же адресом не было.
            for (auto& allocation : _allocations) {
                Assert(allocation.ptr != blk.ptr);
            }
            _allocations.push_back(Blk(ptr, to_allocate));
        }

        {  // Устанавливаем аффиксы.
            if (!std::is_void_v<Prefix>) {
                std::construct_at<Prefix>((Prefix*)ptr, n);
                ptr += sizeof(Prefix);
            }

            if (!std::is_void_v<Suffix>) {
                std::construct_at<Suffix>((Suffix*)(ptr + n), n);
            }
        }

        auto result = Blk((void*)ptr, n);
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

        {  // Забываем об адресе.
            bool found = false;
            for (const auto& [aptr, alength] : _allocations) {
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

                if (std::is_same_v<Prefix, DEBUG_Size_Affix>) {
                    auto& saffix = *(DEBUG_Size_Affix*)affix;
                    Assert(saffix.n == blk_length_without_affixes);
                }
            }

            ptr += blk.length;

            if (!std::is_void_v<Suffix>) {
                auto affix = (Suffix*)(ptr - sizeof(Suffix));
                Assert(affix->Validate());

                if (std::is_same_v<Prefix, DEBUG_Size_Affix>) {
                    auto& saffix = *(DEBUG_Size_Affix*)affix;
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
#endif

//
// Аллокатор, похожий на тот, что схематично приведён в книге
// "Game Engine Gems 2. Ch. 26. A Highly Optimized Portable Memory Manager".
//
// NOTE: Аллоцирует 4*(block_size**2) байт памяти под 4*block_size блоков.
//
// Если block_size ==  32 => аллоцирует  4096 байт (128 блоков)
// Если block_size ==  64 => аллоцирует 16384 байт (256 блоков)
// Если block_size == 128 => аллоцирует 65536 байт (512 блоков)
//
// 1 блок (последний) всегда уходит на bookkeeping
//
//
// Из презентации Andrei Alexandrescu:
//
//     - Organized by constant-size blocks
//     - Tremendously simpler than malloc's heap
//     - Faster, too
//     - Layered on top of any memory hunk
//     - 1 bit/block sole overhead (!)
//     - Multithreading tenuous (needs full interlocking for >1 block)
//
//     TODO: Я хз, как Andrei Alexandrescu бы это реализовывал
//
//     TODO: Зачекать paper https://arxiv.org/pdf/2110.10357.pdf
//     "Fast Bitmap Fit: A CPU Cache Line friendly
//      memory allocator for single object allocations"
//
template <class A, size_t block_size>
class Bitmapped_Allocator {
    Blk Allocate(size_t n) {
        static_assert(Is_Power_Of_2(block_size));

        if (_blocks == nullptr) {
            _blocks          = _parent.Allocate(Total_Blocks_Count());
            _occupied        = _blocks + Last_Block_Offset();
            _allocation_bits = _occupied + block_size / 2;
            memset(_occupied, 0, block_size);
        }

        size_t required_blocks = Ceiled_Division(n, block_size);

        size_t location = 0;
        while (location <= Total_Blocks_Count() - required_blocks) {
            size_t available = 0;
            FOR_RANGE (size_t, i, required_blocks) {
                if (QUERY_BIT(_occupied, i))
                    break;

                available++;
            }

            if (available == required_blocks) {
                void* ptr = (void*)((u8*)_blocks + block_size * location);

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

        // Ensure this is the start of the allocation.
        Assert(b.ptr % block_size == 0);
        Assert(QUERY_BIT(_allocation_bits, block));

        // Unmarking allocation bit.
        UNMARK_BIT(_allocation_bits, block);

        // Unmarking occupied bits.
        FOR_RANGE (size_t, i, Ceiled_Division(b.length, block_size)) {
            Assert(QUERY_BIT(_occupied, block + i));
            UNMARK_BIT(_occupied, block + i);

            Assert(!QUERY_BIT(_allocation_bits, block + i));
        }
    }

    bool Sanity_Check() {
        // У блоков, отмеченных в качестве начальных для аллокаций,
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
    consteval size_t Last_Block_Offset() {
        return block_size * 4 - 1;
    }
    consteval size_t Total_Blocks_Count() {
        return block_size * 4;
    }

    A _parent;  // Аллокатор для аллокации памяти под блоки.

    void* _blocks;
    u8* _occupied;  // NOTE: Если бит = 1, значит, этот блок занят.
    u8* _allocation_bits;  // NOTE: Если бит = 1, значит, это начало новой аллокации.
};

//
// Из презентации Andrei Alexandrescu:
//
//     - Keep a list of allocators, grown lazily
//     - Coarse granularity
//     - Linear search upon allocation
//     - May return memory back
//
template <class A>
struct Cascading_Allocator {
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
        INVALID_PATH;
    }

    bool Sanity_Check() {
        for (auto& allocator : _allocators) {
            if (!allocator.Sanity_Check())
                return false;
        }
        return true;
    }

private:
    std::vector<A> _allocators;
};

// NOTE: Всё, что <= threshold идёт в A1. Остальное - в A2.
//
// Из презентации Andrei Alexandrescu:
//
//     - Could implement any "search" strategy
//     - Works only for a handful of size classes
//
//     typedef Segregator<
//         4096,
//         Segregator<
//             128,
//             Freelist<Malloc_Allocator, 0, 128>,
//             Freelist<Malloc_Allocator, 129, 512>
//         >,
//         Malloc_Allocator
//     > Custom_Allocator;
//
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

//
// Из презентации Andrei Alexandrescu:
//
//     template <class Allocator, size_t min, size_t max, size_t step>
//     struct Bucketizer {
//         // Linear Buckets:
//         //     [min + 0 * step, min + 1 * step),
//         //     [min + 1 * step, min + 2 * step),
//         //     [min + 2 * step, min + 3 * step)...
//         // Exponential Buckets:
//         //     [min * pow(step, 0), min * pow(step, 1)),
//         //     [min * pow(step, 1), min * pow(step, 2)],
//         //     [min * pow(step, 2), min * pow(step, 3)]...
//         //
//         // Within a bucket allocates the maximum size
//     };
//
//     template <class A, u32 flags>
//     class Allocator_With_Stats {
//         // Collect info about any other allocator
//         // Per-allocation and global
//         //     (primitive calls, failures, byte counts, high tide)
//         // Per-allocation
//         //     (caller file/line/function/time)
//     };
//
//     Слайд: Approach to copying.
//
//         - Allocator-dependent
//         - Some noncopyable & immovable: stack/in-situ allocator
//         - Some noncopyable, movable: non-stack regions
//         - Many refcounted, movable
//         - Perhaps other models, too
//
//     Слайд: Granularity.
//
//         - Where are types, factories, ... ?
//         - Design surprise: these are extricable
//         - Focus on Blk
//
//     Слайд: Realistic Example.
//
//         using A = Segregator<
//             8,
//             Freelist<Malloc_Allocator, 0, 8>,
//             128,
//             Bucketizer<FList, 1, 128, 16>,
//             256,
//             Bucketizer<FList, 129, 256, 32>,
//             512,
//             Bucketizer<FList, 257, 512, 64>,
//             1024,
//             Bucketizer<FList, 513, 1024, 128>,
//             2048,
//             Bucketizer<FList, 1025, 2048, 256>,
//             3584,
//             Bucketizer<FList, 2049, 3584, 512>,
//             4072 * 1024,
//             Cascading_Allocator<
//                 Fallback_Allocator<Bitmapped_Allocator<Malloc_Allocator, 32>,
//                 Null_Allocator>
//             >,
//             Malloc_Allocator
//         >;
//
//     Слайд: complete API.
//
//         namespace {
//             static constexpr unsigned alignment;
//             static constexpr Good_Size(size_t);
//             Blk Allocate(size_t);
//             bool Expand(Blk&, size_t delta);
//             void Reallocate(Blk&, size_t);
//             bool Owns(Blk); // optional
//             void Deallocate(Blk);
//             bool Sanity_Check();
//             // aligned versions:
//             // - Aligned_Malloc_Allocator
//             // - `posix_memalign` on Posix
//             // - `_aligned_malloc` on Windows
//             // - Regions support this as well!
//             Blk Aligned_Allocate(size_t, unsigned);
//             Blk Aligned_Reallocate(size_t, unsigned);
//
//             // Для stack аллокатора
//             Blk Allocate_All();
//             void Deallocate_All();
//         }
//
//     Слайд: Summary.
//
//         - Fresh approach from first principles
//         - Understanding history
//             - Otherwise: "... doomed to repeat it"
//         - Composability is key
//

global_var Root_Allocator_Type* root_allocator = nullptr;

// NOLINTNEXTLINE(misc-unused-parameters)
Allocator_function(Root_Allocator_Routine) {
    Assert(allocator_data == nullptr);

    switch (mode) {
    case Allocator_Mode::Allocate: {
        Assert(old_memory_ptr == nullptr);
        Assert(size > 0);
        Assert(old_size == 0);

        return root_allocator->Allocate(size).ptr;
    } break;

    case Allocator_Mode::Resize: {
        auto p = root_allocator->Allocate(size).ptr;
        memcpy(p, old_memory_ptr, old_size);
        root_allocator->Deallocate(Blk(old_memory_ptr, old_size));
        return p;
    } break;

    case Allocator_Mode::Free: {
        Assert(old_memory_ptr != nullptr);
        Assert(size > 0);

        root_allocator->Deallocate(Blk(old_memory_ptr, size));
        return nullptr;
    } break;

    case Allocator_Mode::Free_All: {
        root_allocator->Deallocate_All();
    } break;

    case Allocator_Mode::Sanity: {
        Assert(old_memory_ptr == nullptr);
        Assert(size == 0);
        Assert(old_size == 0);
        Assert(alignment == 0);

        root_allocator->Sanity_Check();
    } break;

    default:
        INVALID_PATH;
    }
    return nullptr;
}

// NOLINTNEXTLINE(misc-unused-parameters)
Allocator_function(Only_Once_Free_All_Root_Allocator) {
    switch (mode) {
    case Allocator_Mode::Allocate: {
        Assert(old_memory_ptr == nullptr);
        Assert(size > 0);
        Assert(old_size == 0);

        return Assert_Deref((Root_Allocator_Type*)allocator_data).Allocate(size).ptr;
    } break;

    case Allocator_Mode::Resize: {
        NOT_IMPLEMENTED;
    } break;

    case Allocator_Mode::Free: {
        Assert(old_memory_ptr != nullptr);
        Assert(size > 0);

        Assert_Deref((Root_Allocator_Type*)allocator_data)
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

        Assert_Deref((Root_Allocator_Type*)allocator_data).Sanity_Check();
    } break;

    default:
        INVALID_PATH;
    }
    return nullptr;
}

void Rect_Copy(u8* dest, u8* source, int stride, int rows, int bytes_per_line) {
    FOR_RANGE (int, i, rows) {
        memcpy(dest + i * bytes_per_line, source + i * stride, bytes_per_line);
    }
}
