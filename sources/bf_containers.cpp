#define CONTAINER_ALLOCATOR                           \
    auto  allocator      = container.allocator_;      \
    void* allocator_data = container.allocator_data_; \
    {                                                 \
        if (allocator == nullptr) {                   \
            Assert(allocator_data == nullptr);        \
            allocator      = ctx->allocator;          \
            allocator_data = ctx->allocator_data;     \
        }                                             \
    }

#define CONTAINER_MEMBER_ALLOCATOR                \
    auto  allocator      = allocator_;            \
    void* allocator_data = allocator_data_;       \
    {                                             \
        if (allocator == nullptr) {               \
            Assert(allocator_data == nullptr);    \
            allocator      = ctx->allocator;      \
            allocator_data = ctx->allocator_data; \
        }                                         \
    }

template <typename T>
struct Fixed_Size_Slice {
    i32 count     = 0;
    i32 max_count = 0;
    T*  items     = nullptr;

    Fixed_Size_Slice()                           = default;
    Fixed_Size_Slice(const Fixed_Size_Slice<T>&) = delete;
    Fixed_Size_Slice(Fixed_Size_Slice<T>&&)      = default;

    void Add_Unsafe(T&& value) {
        Assert(count < max_count);
        items[count] = value;
        count++;
    }

    void Add_Unsafe(const T& value) {
        Assert(count < max_count);
        items[count] = value;
        count++;
    }
};

// PERF: Переписать на ring buffer!
template <typename T>
struct Fixed_Size_Queue {
    size_t memory_size = 0;
    i32    count       = 0;
    T*     base        = nullptr;

    Fixed_Size_Queue()                           = default;
    Fixed_Size_Queue(const Fixed_Size_Queue<T>&) = delete;
    Fixed_Size_Queue(Fixed_Size_Queue<T>&&)      = default;

    void Enqueue_Unsafe(const T value) {
        Assert(memory_size >= (count + 1) * sizeof(T));

        base[count] = value;
        count++;
    }

    // PERF: Много memmove происходит
    T Dequeue() {
        // TODO: Test!
        Assert(base != nullptr);
        Assert(count > 0);

        T res = *base;
        count -= 1;
        if (count > 0)
            memmove(base, base + 1, sizeof(T) * count);

        return res;
    }
};

// PERF: Переписать на ring buffer!
template <typename T>
struct Queue {
    T*  base      = nullptr;
    i32 count     = 0;
    u32 max_count = 0;

    Allocator__Function((*allocator_)) = nullptr;
    void* allocator_data_              = nullptr;

    Queue()                            = default;
    Queue(const Queue& other) noexcept = delete;

    Queue(Queue&& other) noexcept
        : base(other.base)
        , count(other.count)
        , max_count(other.max_count)
        , allocator_(other.allocator_)
        , allocator_data_(other.allocator_data_)  //
    {
        other.base            = nullptr;
        other.count           = 0;
        other.max_count       = 0;
        other.allocator_      = nullptr;
        other.allocator_data_ = nullptr;
    }

    i32 Index_Of(const T& value) {
        FOR_RANGE (i32, i, count) {
            auto& v = base[i];
            if (v == value)
                return i;
        }

        return -1;
    }

    i32 Index_Of(T&& value) {
        FOR_RANGE (i32, i, count) {
            auto& v = base[i];
            if (v == value)
                return i;
        }

        return -1;
    }

    void Enqueue(const T value, MCTX) {
        CONTAINER_MEMBER_ALLOCATOR;

        if (base == nullptr) {
            Assert(max_count == 0);
            Assert(count == 0);
            max_count = 8;
            base      = (T*)ALLOC(sizeof(T) * max_count);
        }
        else if (max_count == count) {
            u32 new_max_count = max_count * 2;
            Assert(max_count < new_max_count);  // NOTE: Ловим overflow

            auto new_base = (T*)ALLOC(sizeof(T) * new_max_count);
            memcpy(new_base, base, sizeof(T) * max_count);
            FREE(base, sizeof(T) * max_count);

            base      = new_base;
            max_count = new_max_count;
        }

        base[count] = value;
        count++;
    }

    void Bulk_Enqueue(const T* values, const u32 values_count, MCTX) {
        CONTAINER_MEMBER_ALLOCATOR;

        // TODO: Test!
        if (base == nullptr) {
            Assert(max_count == 0);
            Assert(count == 0);

            max_count = MAX(Ceil_To_Power_Of_2(values_count), 8);
            base      = (T*)ALLOC(sizeof(T) * max_count);
        }
        else if (max_count < count + values_count) {
            u32 new_max_count = Ceil_To_Power_Of_2(max_count + values_count);
            Assert(max_count < new_max_count);  // NOTE: Ловим overflow

            auto new_base = (T*)ALLOC(sizeof(T) * new_max_count);
            memcpy(new_base, base, sizeof(T) * count);
            FREE(base, sizeof(T) * max_count);

            base      = new_base;
            max_count = new_max_count;
        }

        memcpy(base + count, values, sizeof(T) * values_count);
        count += values_count;
    }

    // PERF: Много memmove происходит
    T Dequeue() {
        Assert(base != nullptr);
        Assert(count > 0);

        T res = *base;
        count -= 1;
        if (count > 0)
            memmove(base, base + 1, sizeof(T) * count);

        return res;
    }

    void Remove_At(i32 i) {
        Assert(i >= 0);
        Assert(i < count);

        i32 delta_count = count - i - 1;
        Assert(delta_count >= 0);

        if (delta_count > 0) {
            memmove(base + i, base + i + 1, sizeof(T) * delta_count);
        }

        count -= 1;
    }

    void Reset() {
        count = 0;
    }
};

template <typename T>
struct Vector {
    T*  base      = nullptr;
    i32 count     = 0;
    u32 max_count = 0;

    Allocator__Function((*allocator_)) = nullptr;
    void* allocator_data_              = nullptr;

    Vector()               = default;
    Vector(Vector&& other) = default;

    Vector(const Vector& other) = delete;

    i32 Index_Of(T value) {
        FOR_RANGE (i32, i, count) {
            auto& v = base[i];
            if (v == value)
                return i;
        }

        return -1;
    }

    i32 Index_Of(T value, std::invocable<const T&, const T&, bool&> auto&& func) {
        bool found = false;
        FOR_RANGE (i32, i, count) {
            func(value, base[i], found);
            if (found)
                return i;
        }

        return -1;
    }

    i32 Index_By_Ptr(T* value_ptr) {
        Assert(base <= value_ptr);
        Assert(value_ptr < base + count * sizeof(T));
        Assert((value_ptr - base) % sizeof(T) == 0);
        return (value_ptr - base) / sizeof(T);
    }

    i32 Add(const T& value, MCTX) {
        CONTAINER_MEMBER_ALLOCATOR;

        i32 locator = count;

        if (base == nullptr) {
            Assert(max_count == 0);
            Assert(count == 0);

            max_count = 8;
            base      = (T*)ALLOC(sizeof(T) * max_count);
        }
        else if (max_count == count) {
            u32 new_max_count = max_count * 2;
            Assert(max_count < new_max_count);  // NOTE: Ловим overflow

            auto old_size = sizeof(T) * max_count;
            auto old_ptr  = base;

            base = rcast<T*>(ALLOC(old_size * 2));
            memcpy(base, old_ptr, old_size);
            FREE(old_ptr, sizeof(T) * max_count);

            max_count = new_max_count;
        }

        base[count] = value;
        count += 1;

        return locator;
    }

    i32 Add(T&& value, MCTX) {
        CONTAINER_MEMBER_ALLOCATOR;

        i32 locator = count;

        if (base == nullptr) {
            Assert(max_count == 0);
            Assert(count == 0);

            max_count = 8;
            base      = (T*)ALLOC(sizeof(T) * max_count);
        }
        else if (max_count == count) {
            u32 new_max_count = max_count * 2;
            Assert(max_count < new_max_count);  // Ловим overflow

            auto old_size = sizeof(T) * max_count;
            auto old_ptr  = base;

            base = rcast<T*>(ALLOC(old_size * 2));
            memcpy(base, old_ptr, old_size);
            FREE(old_ptr, sizeof(T) * max_count);

            max_count = new_max_count;
        }

        memcpy(base + count, &value, sizeof(value));
        // *(base + count) = value;
        count += 1;

        return locator;
    }

    void Remove_At(i32 i) {
        Assert(i >= 0);
        Assert(i < count);

        i32 delta_count = count - i - 1;
        Assert(delta_count >= 0);

        if (delta_count > 0)
            memmove(base + i, base + i + 1, sizeof(T) * delta_count);

        count -= 1;
    }

    void Unordered_Remove_At(const i32 i) {
        Assert(i >= 0);
        Assert(i < count);

        if (i != count - 1)
            std::swap(base[i], base[count - 1]);

        Pop();
    }

    T Pop() {
        Assert(count > 0);

        T result = base[count - 1];
        count -= 1;

        return result;
    }

    // Изменение максимального количества элементов,
    // которое вектор сможет содержать без реаллокации.
    void Resize(u32 elements_count, MCTX) {
        CTX_ALLOCATOR;

        if (max_count < elements_count) {
            base = rcast<T*>(
                REALLOC(sizeof(T) * elements_count, sizeof(T) * max_count, base)
            );
        }
        else if (max_count > elements_count) {
            auto old_max_count = max_count;
            max_count          = elements_count;
            auto old_base      = base;
            base               = rcast<T*>(ALLOC(sizeof(T) * elements_count));
            memcpy(base, old_base, sizeof(T) * elements_count);
            FREE(old_base, sizeof(T) * old_max_count);
        }
    }

    // Вектор сможет содержать как минимум столько элементов без реаллокации.
    void Reserve(u32 elements_count, MCTX) {
        CTX_ALLOCATOR;

        if (base == nullptr) {
            base      = rcast<T*>(ALLOC(sizeof(T) * elements_count));
            max_count = elements_count;
            return;
        }

        if (max_count < elements_count) {
            base = rcast<T*>(
                REALLOC(sizeof(T) * elements_count, sizeof(T) * max_count, base)
            );
            max_count = elements_count;
        }
    }

    void Reset() {
        count = 0;
    }
};

struct Memory_Buffer {
    void*  base      = nullptr;
    size_t count     = 0;
    size_t max_count = 0;

    Allocator__Function((*allocator_)) = nullptr;
    void* allocator_data_              = nullptr;

    explicit Memory_Buffer(MCTX) {
        if (ctx->allocator != nullptr) {
            allocator_      = ctx->allocator;
            allocator_data_ = ctx->allocator_data;
        }
    }

    void Add(void* ptr, size_t size, MCTX) {
        Assert(size > 0);
        Assert(ptr != nullptr);

        CONTAINER_MEMBER_ALLOCATOR;

        if (base == nullptr) {
            max_count = MAX(64, Ceil_To_Power_Of_2(size));
            base      = ALLOC(max_count);
        }
        else if (count + size > max_count) {
            auto old_max_count = max_count;
            max_count          = Ceil_To_Power_Of_2(count + size);
            base               = REALLOC(max_count, old_max_count, base);
        }

        memcpy((void*)((u8*)(base) + count), (u8*)ptr, size);
        count += size;
    }

    void Add_Unsafe(void* ptr, size_t size) {
        Assert(size > 0);
        Assert(ptr != nullptr);

        memcpy((void*)((u8*)(base) + count), (u8*)ptr, size);
        count += size;
    }

    void Reset() {
        count = 0;
    }

    void Deinit(MCTX) {
        CONTAINER_MEMBER_ALLOCATOR;

        if (base != nullptr)
            FREE((u8*)base, max_count);

        count     = 0;
        max_count = 0;
        base      = nullptr;
    }

    void Reserve(size_t size, MCTX) {
        Assert(size > 0);

        CONTAINER_MEMBER_ALLOCATOR;

        if (base == nullptr) {
            auto ceiled_size = Ceil_To_Power_Of_2(size);
            base             = (void*)ALLOC(ceiled_size);
            max_count        = ceiled_size;
        }
        else if (count + size > max_count) {
            auto ceiled_size = Ceil_To_Power_Of_2(size);
            base             = (void*)REALLOC(ceiled_size, size, base);
            max_count        = ceiled_size;
        }
    }
};

template <typename T, typename U>
struct HashMap {
    i32 allocated = 0;
    i32 count     = 0;

    struct Slot {
        bool occupied;
        u32  hash;
        T    key;
        U    value;
    };

    Vector<Slot> slots;

    static constexpr int SIZE_MIN = 32;

    Allocator__Function((*allocator_)) = nullptr;
    void* allocator_data_              = nullptr;

    HashMap(MCTX)
        : slots() {
        if (ctx->allocator != nullptr) {
            allocator_      = ctx->allocator;
            allocator_data_ = ctx->allocator_data;
        }
    }

    void Set(const T& key, const U& value) {
        // TODO
    }
    void Set(const T&& key, const U& value) {
        // TODO
    }
    void Set(const T& key, U&& value) {
        // TODO
    }
    void Set(const T&& key, U&& value) {
        // TODO
    }

    void Remove(const T& key) {
        // TODO
    }

    void Remove(const T&& key) {
        // TODO
    }

    bool Contains(const T& key) {
        // TODO
        return false;
    }

    bool Contains(const T&& key) {
        // TODO
        return false;
    }

    void Reset() {
        // TODO
    }
};

template <typename T>
struct Sparse_Array_Of_Ids {
    T*  ids       = nullptr;
    i32 count     = 0;
    i32 max_count = 0;

    Sparse_Array_Of_Ids(i32 max_count_, MCTX)
        : max_count(max_count_)  //
    {
        CTX_ALLOCATOR;
        ids   = rcast<T*>(ALLOC(sizeof(T) * max_count));
        count = 0;
    }

    void Add(const T id, MCTX) {
        Assert(!Contains(this, id));

        if (max_count == count)
            Enlarge(ctx);

        *(ids + count) = id;
        count += 1;
    }

    void Unstable_Remove(const T id) {
        FOR_RANGE (i32, i, count) {
            if (ids[i] == id) {
                if (i != count - 1) {
                    std::swap(ids[i], ids[count - 1]);
                }
                count--;
                return;
            }
        }
        Assert(false);
    }

    bool Contains(const T id) {
        FOR_RANGE (int, i, count) {
            if (ids[i] == id)
                return true;
        }
        return false;
    }

    void Enlarge(MCTX) {
        CTX_ALLOCATOR;

        auto new_max_count = max_count * 2;
        Assert(max_count < new_max_count);  // NOTE: Ловим overflow

        auto old_ids_size = sizeof(T) * max_count;
        auto old_ids_ptr  = ids;

        memcpy(ids, old_ids_ptr, old_ids_size);
        FREE(old_ids_ptr, old_ids_size);

        max_count = new_max_count;
    }

    void Reset() {
        count = 0;
    }
};

template <typename T, typename U>
struct Sparse_Array {
    T*  ids   = nullptr;
    U*  base  = nullptr;
    i32 count = 0;
    i32 max_count;

    Sparse_Array(i32 max_count_, MCTX)
        : max_count(max_count_)
        , count(0)  //
    {
        CTX_ALLOCATOR;
        ids  = rcast<T*>(ALLOC(sizeof(T) * max_count));
        base = rcast<U*>(ALLOC(sizeof(U) * max_count));
    }
    Sparse_Array(const Sparse_Array& other) = delete;
    Sparse_Array(Sparse_Array&& other)      = delete;

    U* Add(const T id, const U& value, MCTX) {
        Assert(!Contains(id));
        Assert(count >= 0);
        Assert(max_count >= 0);

        if (max_count == count)
            Enlarge(ctx);

        ids[count]  = id;
        base[count] = value;
        count += 1;
        return base + count - 1;
    }

    U* Add(const T id, U&& value, MCTX) {
        Assert(!Contains(id));
        Assert(count >= 0);
        Assert(max_count >= 0);

        if (max_count == count)
            Enlarge(ctx);

        ids[count] = id;
        std::construct_at(base + count, std::move(value));
        count += 1;

        return base + count - 1;
    }

    U* Occupy(const T id, MCTX) {
        Assert(!Contains(id));
        Assert(count >= 0);
        Assert(max_count >= 0);

        if (max_count == count)
            Enlarge(ctx);

        ids[count] = id;
        count += 1;

        return base + count - 1;
    }

    U* Find(const T id) {
        FOR_RANGE (i32, i, count) {
            if (ids[i] == id)
                return base + i;
        }
        return nullptr;
    }

    i32 Index_Of(T value, std::invocable<const T&, const T&, bool&> auto&& func) {
        bool found = false;
        FOR_RANGE (i32, i, count) {
            func(value, base[i], found);
            if (found)
                return i;
        }

        return -1;
    }

    void Unstable_Remove(const T id) {
        FOR_RANGE (i32, i, count) {
            if (ids[i] == id) {
                if (i != count - 1) {
                    std::swap(ids[i], ids[count - 1]);

                    U b;
                    memcpy(&b, base + i, sizeof(U));
                    memcpy(base + i, base + count - 1, sizeof(U));
                    memcpy(&base + count - 1, &b, sizeof(U));
                }
                count--;
                return;
            }
        }
        INVALID_PATH;
    }

    bool Contains(const T id) {
        FOR_RANGE (int, i, count) {
            if (ids[i] == id)
                return true;
        }
        return false;
    }

    void Enlarge(MCTX) {
        CTX_ALLOCATOR;

        u32 new_max_count = max_count * 2;
        Assert(max_count < new_max_count);  // NOTE: Ловим overflow

        auto old_ids_size  = sizeof(T) * max_count;
        auto old_base_size = sizeof(U) * max_count;
        auto old_ids_ptr   = ids;
        auto old_base_ptr  = base;

        ids  = rcast<T*>(ALLOC(old_ids_size * 2));
        base = rcast<U*>(ALLOC(old_base_size * 2));
        memcpy(ids, old_ids_ptr, old_ids_size);
        memcpy(base, old_base_ptr, old_base_size);
        FREE(old_ids_ptr, old_ids_size);
        FREE(old_base_ptr, old_base_size);

        max_count = new_max_count;
    }

    void Reset() {
        count = 0;
    }
};

using Bucket_Index = u32;

template <typename T>
struct Bucket {
    void* occupied;
    void* data;

    i32          count;
    Bucket_Index bucket_index;
};

struct Bucket_Locator : Equatable<Bucket_Locator> {
    Bucket_Index bucket_index;
    u32          slot_index;

    constexpr Bucket_Locator()                            = default;
    constexpr Bucket_Locator(Bucket_Locator&& other)      = default;
    constexpr Bucket_Locator(const Bucket_Locator& other) = default;

    constexpr Bucket_Locator(Bucket_Index a_bucket_index, u32 a_slot_index)
        : bucket_index(a_bucket_index)
        , slot_index(a_slot_index) {}
    constexpr Bucket_Locator& operator=(const Bucket_Locator&) = default;

    // Bucket_Locator(Bucket_Locator&& other)
    //     : bucket_index(other.bucket_index), slot_index(other.slot_index) {}

    bool Equal_To(const Bucket_Locator& o) const {
        return bucket_index == o.bucket_index && slot_index == o.slot_index;
    }
};

constexpr const Bucket_Locator Incorrect_Bucket_Locator(-1, -1);

template <typename T>
struct Bucket_Array : Non_Copyable {
    Bucket<T>*    buckets;
    Bucket_Index* unfull_buckets;

    i64 count            = 0;
    i32 items_per_bucket = 0;

    Bucket_Index buckets_count        = 0;
    Bucket_Index used_buckets_count   = 0;
    Bucket_Index unfull_buckets_count = 0;

    Allocator__Function((*allocator_)) = nullptr;
    void* allocator_data_              = nullptr;

    Bucket_Locator Add(T& item, MCTX) {
        CONTAINER_MEMBER_ALLOCATOR;
        auto [locator, ptr] = Find_And_Occupy_Empty_Slot(ctx);

        *ptr = item;

        return locator;
    }

    T* Find(Bucket_Locator& locator) {
        // TODO: Больше проверок!
        auto& bucket = buckets[locator.bucket_index];
        Assert(Bucket_Occupied(bucket, locator.slot_index));
        auto result = scast<T*>(bucket.data) + locator.slot_index;
        return result;
    }

    void Remove(Bucket_Locator& locator) {
        Assert(locator.bucket_index < used_buckets_count);
        auto& bucket = buckets[locator.bucket_index];
        Assert(Bucket_Occupied(bucket, locator.slot_index));

        bool was_full = (bucket.count == items_per_bucket);

        BUCKET_UNMARK_OCCUPIED(bucket, locator.slot_index);

        bucket.count -= 1;
        count -= 1;

        if (was_full) {
            const auto found_index
                = Array_Find(unfull_buckets, unfull_buckets_count, bucket.bucket_index);
            Assert(found_index == -1);
            unfull_buckets[unfull_buckets_count] = bucket.bucket_index;
            unfull_buckets_count++;
        }
    }

    Bucket<T>* Add_Bucket(MCTX) {
        CONTAINER_MEMBER_ALLOCATOR;

        Assert(unfull_buckets_count == 0);
        Assert(items_per_bucket > 0);
        Assert(buckets_count > 0);

        if (buckets == nullptr) {  // NOTE: Следовательно, это первый вызов.
            Assert(unfull_buckets == nullptr);

            typedef Bucket<T> arr_type;

            constexpr auto align      = alignof(arr_type*);
            auto           alloc_size = buckets_count * sizeof(arr_type);

            buckets = rcast<Bucket<T>*>(ALLOC(alloc_size));
            if (buckets == nullptr) {
                INVALID_PATH;
                return nullptr;
            }

            unfull_buckets
                = rcast<Bucket_Index*>(ALLOC(buckets_count * sizeof(Bucket_Index)));
            if (unfull_buckets == nullptr) {
                INVALID_PATH;
                return nullptr;
            }
        }

        Assert(buckets != nullptr);
        Assert(unfull_buckets != nullptr);

        auto new_bucket          = buckets + used_buckets_count;
        new_bucket->bucket_index = used_buckets_count;
        new_bucket->count        = 0;

        auto occupied_bytes_count = Ceil_Division(items_per_bucket, 8);
        auto occupied             = rcast<u8*>(ALLOC(occupied_bytes_count));
        if (occupied == nullptr) {
            INVALID_PATH;
            return nullptr;
        }
        memset(occupied, 0, occupied_bytes_count);

        auto data = rcast<u8*>(ALLOC(sizeof(T) * items_per_bucket));
        if (data == nullptr) {
            INVALID_PATH;
            return nullptr;
        }

        new_bucket->occupied = occupied;
        new_bucket->data     = data;

        unfull_buckets[unfull_buckets_count] = used_buckets_count;
        used_buckets_count++;
        unfull_buckets_count++;

        return new_bucket;
    }

    std::tuple<Bucket_Locator, T*> Find_And_Occupy_Empty_Slot(MCTX) {
        if (unfull_buckets_count == 0)
            Add_Bucket(ctx);
        // TODO: Some kind of error handling!
        Assert(unfull_buckets_count > 0);

        Bucket_Index bucket_index = unfull_buckets[0];
        auto         bucket_ptr   = buckets + bucket_index;

        int index = -1;
        FOR_RANGE (int, i, items_per_bucket) {
            // PERF: We can record the first non-empty index in the occupied list?
            u8 occupied = Bucket_Occupied(*bucket_ptr, i);
            if (!occupied) {
                index = i;
                break;
            }
        }

        Assert(index != -1);
        BUCKET_MARK_OCCUPIED(*bucket_ptr, index);

        bucket_ptr->count += 1;
        Assert(bucket_ptr->count <= items_per_bucket);

        count += 1;

        if (bucket_ptr->count == items_per_bucket) {
            u32* found = nullptr;
            for (auto bucket_index_ptr = unfull_buckets;
                 bucket_index_ptr < unfull_buckets + unfull_buckets_count;
                 bucket_index_ptr++) {
                if (*bucket_index_ptr == bucket_ptr->bucket_index) {
                    found = bucket_index_ptr;
                    break;
                }
            }
            Assert(found != nullptr);
            *found = unfull_buckets[unfull_buckets_count - 1];
            unfull_buckets_count -= 1;
        }

        Bucket_Locator locator{bucket_ptr->bucket_index, (u32)index};
        return {locator, scast<T*>(bucket_ptr->data) + index};
    }
};

// ----- Array Functions -----

template <typename T>
i32 Array_Find(T* values, u32 n, T& value) {
    FOR_RANGE (u32, i, n) {
        auto& v = *(values + n);
        if (v == value)
            return i;
    }
    return -1;
}

// ----- Bucket Array -----

#if 1
template <typename T>
BF_FORCE_INLINE u8 Bucket_Occupied(Bucket<T>& bucket_ref, u32 index) {
    u8 result = QUERY_BIT((bucket_ref).occupied, (index));
    return result;
}

#define BUCKET_MARK_OCCUPIED(bucket_ref, index) MARK_BIT((bucket_ref).occupied, (index))
#define BUCKET_UNMARK_OCCUPIED(bucket_ref, index) \
    UNMARK_BIT((bucket_ref).occupied, (index))
#else
// NOTE: Здесь можно будет переписать
// на использование просто bool, если понадобится
#endif

template <typename T>
struct Bucket_Array_Iterator : public Iterator_Facade<Bucket_Array_Iterator<T>> {
    Bucket_Array_Iterator() = delete;

    Bucket_Array_Iterator(Bucket_Array<T>* arr)
        : Bucket_Array_Iterator(arr, 0, 0) {}

    Bucket_Array_Iterator(
        Bucket_Array<T>* arr,
        i32              current,
        Bucket_Index     current_bucket  //
    )
        : _current(current)
        , _current_bucket(current_bucket)
        , _arr(arr)  //
    {
        Assert(arr != nullptr);
    }

    Bucket_Array_Iterator begin() const {
        Bucket_Array_Iterator iter = {_arr, _current, _current_bucket};

        if (_arr->used_buckets_count) {
            iter._current -= 1;
            iter._current_bucket_count = iter._Get_Current_Bucket_Count();
            iter.Increment();
        }

        return iter;
    }
    Bucket_Array_Iterator end() const {
        return {_arr, 0, _arr->used_buckets_count};
    }

    T* Dereference() const {
        Assert(_current_bucket < _arr->used_buckets_count);
        Assert(_current < _arr->items_per_bucket);

        auto& bucket = *(_arr->buckets + _current_bucket);
        Assert(Bucket_Occupied(bucket, _current));

        auto result = scast<T*>(bucket.data) + _current;
        return result;
    }

    void Increment() {
        FOR_RANGE (int, _GUARD_, 512) {
            _current++;
            if (_current >= _arr->items_per_bucket) {
                _current = 0;
                _current_bucket++;

                if (_current_bucket == _arr->used_buckets_count)
                    return;

                _current_bucket_count = _Get_Current_Bucket_Count();
            }

            Bucket<T>& bucket = *(_arr->buckets + _current_bucket);
            if (Bucket_Occupied(bucket, _current))
                return;
        }
        INVALID_PATH;
    }

    bool Equal_To(const Bucket_Array_Iterator& o) const {
        return _current == o._current && _current_bucket == o._current_bucket;
    }

private:
    int _Get_Current_Bucket_Count() {
        auto& bucket = *(_arr->buckets + _current_bucket);
        return bucket.count;
    }

    Bucket_Array<T>* _arr;
    i32              _current              = 0;
    Bucket_Index     _current_bucket       = 0;
    u16              _current_bucket_count = 0;
};

template <typename T, typename U>
struct Sparse_Array_Iterator : public Iterator_Facade<Sparse_Array_Iterator<T, U>> {
    Sparse_Array_Iterator() = delete;
    Sparse_Array_Iterator(Sparse_Array<T, U>* container)
        : Sparse_Array_Iterator(container, 0) {}
    Sparse_Array_Iterator(Sparse_Array<T, U>* container, i32 current)
        : _current(current)
        , _container(container)  //
    {
        Assert(container != nullptr);
    }

    Sparse_Array_Iterator begin() const {
        return {_container, _current};
    }
    Sparse_Array_Iterator end() const {
        return {_container, _container->count};
    }

    std::tuple<T, U*> Dereference() const {
        Assert(_current >= 0);
        Assert(_current < _container->count);
        return std::tuple(_container->ids[_current], _container->base + _current);
    }

    void Increment() {
        _current++;
    }

    bool Equal_To(const Sparse_Array_Iterator& o) const {
        return _current == o._current;
    }

private:
    Sparse_Array<T, U>* _container;
    i32                 _current = 0;
};

template <typename T>
struct Bucket_Array_With_Locator_Iterator
    : public Iterator_Facade<Bucket_Array_With_Locator_Iterator<T>> {
    Bucket_Array_With_Locator_Iterator() = delete;

    Bucket_Array_With_Locator_Iterator(Bucket_Array<T>* arr)
        : Bucket_Array_With_Locator_Iterator(arr, 0, 0) {}

    Bucket_Array_With_Locator_Iterator(
        Bucket_Array<T>* arr,
        i32              current,
        Bucket_Index     current_bucket  //
    )
        : _current(current)
        , _current_bucket(current_bucket)
        , _arr(arr)  //
    {
        Assert(arr != nullptr);
    }

    Bucket_Array_With_Locator_Iterator begin() const {
        Bucket_Array_With_Locator_Iterator iter = {_arr, _current, _current_bucket};

        if (_arr->used_buckets_count) {
            iter._current -= 1;
            iter._current_bucket_count = iter._Get_Current_Bucket_Count();
            iter.Increment();
        }

        return iter;
    }
    Bucket_Array_With_Locator_Iterator end() const {
        return {_arr, 0, _arr->used_buckets_count};
    }

    std::tuple<Bucket_Locator, T*> Dereference() const {
        Assert(_current_bucket < _arr->used_buckets_count);
        Assert(_current < _arr->items_per_bucket);
        Assert(_current >= 0);

        auto& bucket = *(_arr->buckets + _current_bucket);
        Assert(Bucket_Occupied(bucket, _current));

        auto result = scast<T*>(bucket.data) + _current;
        return {{_current_bucket, (u32)_current}, result};
    }

    void Increment() {
        FOR_RANGE (int, _GUARD_, 256) {
            _current++;
            if (_current >= _current_bucket_count) {
                _current = 0;
                _current_bucket++;

                if (_current_bucket == _arr->used_buckets_count)
                    return;

                _current_bucket_count = _Get_Current_Bucket_Count();
            }

            Bucket<T>& bucket = *(_arr->buckets + _current_bucket);
            if (Bucket_Occupied(bucket, _current))
                return;
        }
        INVALID_PATH;
    }

    bool Equal_To(const Bucket_Array_With_Locator_Iterator& o) const {
        return _current == o._current && _current_bucket == o._current_bucket;
    }

private:
    int _Get_Current_Bucket_Count() {
        auto& bucket = *(_arr->buckets + _current_bucket);
        return bucket.count;
    }

    Bucket_Array<T>* _arr;
    i32              _current              = 0;
    Bucket_Index     _current_bucket       = 0;
    u16              _current_bucket_count = 0;
};

template <typename T>
struct Vector_Iterator : public Iterator_Facade<Vector_Iterator<T>> {
    Vector_Iterator() = delete;
    Vector_Iterator(Vector<T>* container)
        : Vector_Iterator(container, 0) {}
    Vector_Iterator(Vector<T>* container, i32 current)
        : _current(current)
        , _container(container)  //
    {
        Assert(container != nullptr);
    }

    Vector_Iterator begin() const {
        return {_container, _current};
    }
    Vector_Iterator end() const {
        return {_container, _container->count};
    }

    T* Dereference() const {
        Assert(_current >= 0);
        Assert(_current < _container->count);
        return _container->base + _current;
    }

    void Increment() {
        _current++;
    }

    bool Equal_To(const Vector_Iterator& o) const {
        return _current == o._current;
    }

private:
    Vector<T>* _container;
    i32        _current = 0;
};

template <typename T>
BF_FORCE_INLINE auto Iter(Bucket_Array<T>* container) {
    return Bucket_Array_Iterator(container);
}
template <typename T, typename U>
BF_FORCE_INLINE auto Iter(Sparse_Array<T, U>* container) {
    return Sparse_Array_Iterator(container);
}

template <typename T>
BF_FORCE_INLINE auto Iter(Vector<T>* container) {
    return Vector_Iterator(container);
}

template <typename T>
BF_FORCE_INLINE auto Iter_With_ID(Bucket_Array<T>* arr) {
    return Bucket_Array_With_Locator_Iterator(arr);
}

template <typename T>
BF_FORCE_INLINE void Container_Reset(Bucket_Array<T>& container) {
    container.count                = 0;
    container.unfull_buckets_count = container.used_buckets_count;

    FOR_RANGE (Bucket_Index, i, container.unfull_buckets_count) {
        *(container.unfull_buckets + i) = i;
    }

    auto occupied_bytes_count = Ceil_Division(container.items_per_bucket, 8);
    FOR_RANGE (Bucket_Index, i, container.used_buckets_count) {
        auto& bucket = *(container.buckets + i);

        if (bucket.count > 0) {
            memset(bucket.occupied, 0, occupied_bytes_count);
            bucket.count = 0;
        }
    }
    container.used_buckets_count = 0;
}

template <typename T, typename U>
T* Sparse_Array_Find(Sparse_Array<T, U>& arr, U id) {
    FOR_RANGE (int, i, arr.count) {
        if (arr.ids[i] == id)
            return arr.base[i];
    }
    INVALID_PATH;
    return nullptr;
}
