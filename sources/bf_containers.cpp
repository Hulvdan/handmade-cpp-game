#define CONTAINER_ALLOCATOR                          \
    auto  allocator      = container.allocator;      \
    void* allocator_data = container.allocator_data; \
    {                                                \
        if (allocator == nullptr) {                  \
            Assert(allocator_data == nullptr);       \
            allocator      = ctx->allocator;         \
            allocator_data = ctx->allocator_data;    \
        }                                            \
    }

// PERF: Переписать на ring buffer!
template <typename T>
struct Fixed_Size_Queue {
    size_t memory_size;
    i32    count;
    T*     base;
};

// PERF: Переписать на ring buffer!
template <typename T>
struct Queue {
    u32 max_count;
    i32 count;
    T*  base;

    Allocator__Function((*allocator));
    void* allocator_data;
};

// PERF: Переписать на ring buffer!
// TODO: Дока для чего сделал это
template <typename T>
struct Grid_Of_Queues {
    u32* max_counts;
    i32* counts;
    T*   bases;

    Allocator__Function((*allocator));
    void* allocator_data;
};

template <typename T>
struct Vector {
    T*  base;
    i32 count;
    u32 max_count;

    Allocator__Function((*allocator));
    void* allocator_data;
};

// TODO: Дока для чего сделал это
template <typename T>
struct Grid_Of_Vectors {
    T**  bases;
    i32* counts;
    u32* max_counts;

    Allocator__Function((*allocator));
    void* allocator_data;
};

using Bucket_Index = u32;

template <typename T>
struct Bucket {
    void* occupied;
    void* data;

    i32          count;
    Bucket_Index bucket_index;
};

struct Bucket_Locator {
    Bucket_Index bucket_index;
    u32          slot_index;
};

// TODO:
// 1) Нужно ли реализовать expandable количество бакетов?
template <typename T>
struct Bucket_Array : Non_Copyable {
    Allocator__Function((*allocator));
    void* allocator_data;

    Bucket<T>*    buckets;
    Bucket_Index* unfull_buckets;

    i64 count;
    i32 items_per_bucket;

    Bucket_Index buckets_count;
    Bucket_Index used_buckets_count;
    Bucket_Index unfull_buckets_count;
};

template <typename T>
void Enqueue(Fixed_Size_Queue<T>& container, const T value) {
    Assert(container.memory_size >= (container.count + 1) * sizeof(T));

    *(container.base + container.count) = value;
    container.count++;
}

// PERF: Много memmove происходит
template <typename T>
T Dequeue(Fixed_Size_Queue<T>& container) {
    // TODO: Test!
    Assert(container.base != nullptr);
    Assert(container.count > 0);

    T res = *container.base;
    container.count -= 1;
    if (container.count > 0)
        memmove(container.base, container.base + 1, sizeof(T) * container.count);

    return res;
}

template <typename T>
i32 Queue_Find(Queue<T>& container, T value) {
    FOR_RANGE (i32, i, container.count) {
        auto& v = *(container.base + i);
        if (v == value)
            return i;
    }

    return -1;
}

template <typename T>
void Enqueue(Queue<T>& container, const T value, MCTX)
    requires std::is_trivially_copyable_v<T>
{
    CONTAINER_ALLOCATOR;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);
        container.max_count = 8;
        container.base      = (T*)ALLOC(sizeof(T) * container.max_count);
    }
    else if (container.max_count == container.count) {
        u32 new_max_count = container.max_count * 2;
        Assert(container.max_count < new_max_count);  // NOTE: Ловим overflow

        auto new_ptr = (T*)ALLOC(sizeof(T) * new_max_count);
        memcpy(new_ptr, container.base, container.max_count * sizeof(T));
        FREE(container.base, container.max_count);

        container.base      = new_ptr;
        container.max_count = new_max_count;
    }

    *(container.base + container.count) = value;
    container.count++;
}

template <typename T>
void Enqueue(
    Grid_Of_Queues<T>& container,
    const T            value,
    const v2i16        pos,
    const i16          stride,
    MCTX
) {
    Assert(stride > 0);
    Assert(pos.x >= 0);
    Assert(pos.y >= 0);

    CONTAINER_ALLOCATOR;

    auto& max_count = *(container.max_counts + pos.y * stride + pos.x);
    auto& count     = *(container.counts + pos.y * stride + pos.x);
    auto& base      = *(container.bases + pos.y * stride + pos.x);

    if (base == nullptr) {
        Assert(max_count == 0);
        Assert(count == 0);
        max_count = 8;
        base      = (T*)ALLOC(sizeof(T) * max_count);
    }
    else if (max_count == count) {
        u32 new_max_count = max_count * 2;
        Assert(max_count < new_max_count);  // NOTE: Ловим overflow

        auto new_ptr = (T*)ALLOC(sizeof(T) * new_max_count);
        memcpy(new_ptr, base, max_count * sizeof(T));
        FREE(base, max_count);

        base      = new_ptr;
        max_count = new_max_count;
    }

    *(base + count) = value;
    count++;
}

template <typename T>
void Bulk_Enqueue(Queue<T>& container, const T* values, const u32 values_count, MCTX)
    requires std::is_trivially_copyable_v<T>
{
    CONTAINER_ALLOCATOR;

    // TODO: Test!
    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = MAX(Ceil_To_Power_Of_2(values_count), 8);
        container.base      = (T*)ALLOC(sizeof(T) * container.max_count);
    }
    else if (container.max_count < container.count + values_count) {
        u32 new_max_count = Ceil_To_Power_Of_2(container.max_count + values_count);
        Assert(container.max_count < new_max_count);  // NOTE: Ловим overflow

        auto new_ptr = (T*)ALLOC(sizeof(T) * new_max_count);
        memcpy(new_ptr, container.base, sizeof(T) * container.count);
        FREE(container.base, container.max_count);

        container.base      = new_ptr;
        container.max_count = new_max_count;
    }

    memcpy(container.base + container.count, values, sizeof(T) * values_count);
    container.count += values_count;
}

// PERF: Много memmove происходит
template <typename T>
T Dequeue(Queue<T>& container) {
    Assert(container.base != nullptr);
    Assert(container.count > 0);

    T res = *container.base;
    container.count -= 1;
    if (container.count > 0)
        memmove(container.base, container.base + 1, sizeof(T) * container.count);

    return res;
}

template <typename T>
i32 Vector_Find(tvector<T>& container, T value) {
    i32 i = 0;
    for (auto& v : container) {
        if (v == value)
            return i;

        i++;
    }
    return -1;
}

template <typename T>
i32 Vector_Find(Vector<T>& container, T value) {
    FOR_RANGE (i32, i, container.count) {
        auto& v = *(container.base + i);
        if (v == value)
            return i;
    }

    return -1;
}

template <typename T>
i32 Vector_Find_Ptr(Vector<T>& container, T* value_ptr) {
    Assert(container.base <= value_ptr);
    Assert(value_ptr < container.base + container.count * sizeof(T));
    Assert((value_ptr - container.base) % sizeof(T) == 0);
    return (value_ptr - container.base) / sizeof(T);
}

template <typename T>
    requires std::is_trivially_copyable_v<T>
void Queue_Remove_At(Queue<T>& container, i32 i) {
    Assert(i >= 0);
    Assert(i < container.count);

    i32 delta_count = container.count - i - 1;
    Assert(delta_count >= 0);

    if (delta_count > 0) {
        memmove(container.base + i, container.base + i + 1, sizeof(T) * delta_count);
    }

    container.count -= 1;
}

template <typename T>
i32 Vector_Add(Vector<T>& container, const T& value, MCTX) {
    CONTAINER_ALLOCATOR;

    i32 locator = container.count;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = 8;
        container.base      = (T*)ALLOC(sizeof(T) * container.max_count);
    }
    else if (container.max_count == container.count) {
        u32 new_max_count = container.max_count * 2;
        Assert(container.max_count < new_max_count);  // NOTE: Ловим overflow

        auto old_size = sizeof(T) * container.max_count;
        auto old_ptr  = container.base;

        container.base = rcast<T*>(ALLOC(old_size * 2));
        memcpy(container.base, old_ptr, old_size);
        FREE(old_ptr, container.max_count);

        container.max_count = new_max_count;
    }

    *(container.base + container.count) = value;
    container.count += 1;

    return locator;
}

template <typename T>
i32 Vector_Add(Vector<T>& container, T&& value, MCTX) {
    CONTAINER_ALLOCATOR;

    i32 locator = container.count;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = 8;
        container.base      = (T*)ALLOC(sizeof(T) * container.max_count);
    }
    else if (container.max_count == container.count) {
        u32 new_max_count = container.max_count * 2;
        Assert(container.max_count < new_max_count);  // NOTE: Ловим overflow

        auto old_size = sizeof(T) * container.max_count;
        auto old_ptr  = container.base;

        container.base = rcast<T*>(ALLOC(old_size * 2));
        memcpy(container.base, old_ptr, old_size);
        FREE(old_ptr, container.max_count);

        container.max_count = new_max_count;
    }

    *(container.base + container.count) = value;
    container.count += 1;

    return locator;
}

// NOTE: stride - это не байтовый stride.
// Это, к примеру, для Grid_Of_Vectors было бы количество клеток по ширине карты.
template <typename T>
BF_FORCE_INLINE T* Get_By_Stride(T* const values, const v2i16 pos, const i16 stride) {
    return values + pos.y * stride + pos.x;
}

template <typename T>
i32 Vector_Add(
    Grid_Of_Vectors<T>& container,
    const T&            value,
    const v2i16         pos,
    const i16           stride,
    MCTX
) {
    CONTAINER_ALLOCATOR;

    auto& base      = *Get_By_Stride(container.bases, pos, stride);
    auto& max_count = *Get_By_Stride(container.max_counts, pos, stride);
    auto& count     = *Get_By_Stride(container.counts, pos, stride);
    i32   locator   = count;

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
        FREE(old_ptr, max_count);

        max_count = new_max_count;
    }

    *(base + count) = value;
    count += 1;

    return locator;
}

template <typename T>
i32 Vector_Add(
    Grid_Of_Vectors<T>& container,
    T&&                 value,
    const v2i16         pos,
    const i16           stride,
    MCTX
) {
    CONTAINER_ALLOCATOR;

    auto& base      = *Get_By_Stride(container.bases, pos, stride);
    auto& max_count = *Get_By_Stride(container.max_counts, pos, stride);
    auto& count     = *Get_By_Stride(container.counts, pos, stride);
    i32   locator   = count;

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
        FREE(old_ptr, max_count);

        max_count = new_max_count;
    }

    *(base + count) = std::move(value);
    count += 1;

    return locator;
}

template <typename T>
void Vector_Remove_At(Vector<T>& container, i32 i) {
    Assert(i >= 0);
    Assert(i < container.count);

    i32 delta_count = container.count - i - 1;
    Assert(delta_count >= 0);

    if (delta_count > 0) {
        memmove(container.base + i, container.base + i + 1, sizeof(T) * delta_count);
    }

    container.count -= 1;
}

template <typename T>
void Vector_Unordered_Remove_At(tvector<T>& container, const i32 i) {
    Assert(i >= 0);
    Assert(i < container.size());

    if (i != container.size() - 1)
        std::swap(container[i], container[container.size() - 1]);

    container.pop_back();
}

template <typename T>
void Vector_Unordered_Remove_At(
    Grid_Of_Vectors<T>& container,
    const i32           i,
    const v2i16         pos,
    const i16           stride
) {
    Assert(i >= 0);
    auto& count = *Get_By_Stride(container.counts, pos, stride);
    Assert(i < count);

    auto base = *Get_By_Stride(container.bases, pos, stride);

    if (i != count - 1)
        std::swap(base[i], base[count - 1]);

    count -= 1;
}

template <typename T>
T Vector_Pop(Vector<T>& vec) {
    Assert(vec.count > 0);

    T result = *(vec.base + vec.count - 1);
    vec.count -= 1;

    return result;
}

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
Bucket<T>* Add_Bucket(Bucket_Array<T>& container, MCTX) {
    CONTAINER_ALLOCATOR;

    Assert(container.unfull_buckets_count == 0);
    Assert(container.items_per_bucket > 0);
    Assert(container.buckets_count > 0);

    if (container.buckets == nullptr) {  // NOTE: Следовательно, это первый вызов.
        Assert(container.unfull_buckets == nullptr);

        typedef Bucket<T> arr_type;

        constexpr auto align      = alignof(arr_type*);
        auto           alloc_size = container.buckets_count * sizeof(arr_type);

        container.buckets = rcast<Bucket<T>*>(ALLOC(alloc_size));
        if (container.buckets == nullptr) {
            Assert(false);
            return nullptr;
        }

        container.unfull_buckets
            = rcast<Bucket_Index*>(ALLOC(container.buckets_count * sizeof(Bucket_Index)));
        if (container.unfull_buckets == nullptr) {
            Assert(false);
            return nullptr;
        }
    }

    Assert(container.buckets != nullptr);
    Assert(container.unfull_buckets != nullptr);

    auto new_bucket          = container.buckets + container.used_buckets_count;
    new_bucket->bucket_index = container.used_buckets_count;
    new_bucket->count        = 0;

    auto occupied_bytes_count = Ceil_Division(container.items_per_bucket, 8);
    auto occupied             = rcast<u8*>(ALLOC(occupied_bytes_count));
    if (occupied == nullptr) {
        Assert(false);
        return nullptr;
    }
    memset(occupied, 0, occupied_bytes_count);

    auto data = rcast<u8*>(ALLOC(sizeof(T) * container.items_per_bucket));
    if (data == nullptr) {
        Assert(false);
        return nullptr;
    }

    new_bucket->occupied = occupied;
    new_bucket->data     = data;

    *(container.unfull_buckets + container.unfull_buckets_count)
        = container.used_buckets_count;
    container.used_buckets_count++;
    container.unfull_buckets_count++;

    return new_bucket;
}

template <typename T>
ttuple<Bucket_Locator, T*> Find_And_Occupy_Empty_Slot(Bucket_Array<T>& arr, MCTX) {
    if (arr.unfull_buckets_count == 0)
        Add_Bucket(arr, ctx);
    // TODO: Some kind of error handling!
    Assert(arr.unfull_buckets_count > 0);

    Bucket_Index bucket_index = *(arr.unfull_buckets + 0);
    auto         bucket_ptr   = arr.buckets + bucket_index;

    int index = -1;
    FOR_RANGE (int, i, arr.items_per_bucket) {
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
    Assert(bucket_ptr->count <= arr.items_per_bucket);

    arr.count += 1;

    if (bucket_ptr->count == arr.items_per_bucket) {
        u32* found = nullptr;
        for (auto bucket_index_ptr = arr.unfull_buckets;
             bucket_index_ptr < arr.unfull_buckets + arr.unfull_buckets_count;
             bucket_index_ptr++) {
            if (*bucket_index_ptr == bucket_ptr->bucket_index) {
                found = bucket_index_ptr;
                break;
            }
        }
        *found = *(arr.unfull_buckets + arr.unfull_buckets_count - 1);
        arr.unfull_buckets_count -= 1;
    }

    Bucket_Locator locator = {};
    locator.bucket_index   = bucket_ptr->bucket_index;
    locator.slot_index     = index;

    return {locator, scast<T*>(bucket_ptr->data) + index};
}

template <typename T>
Bucket_Locator Bucket_Array_Add(Bucket_Array<T>& arr, T& item, MCTX) {
    CTX_ALLOCATOR;
    auto [locator, ptr] = Find_And_Occupy_Empty_Slot(arr, ctx);

    *ptr = item;

    return locator;
}

template <typename T>
T* Bucket_Array_Find(Bucket_Array<T> arr, Bucket_Locator locator) {
    auto& bucket = *(arr.all_buckets + locator.bucket_index);
    Assert(Bucket_Occupied(bucket, locator.slot_index));
    auto result = scast<T*>(bucket.data) + locator.slot_index;
    return result;
}

template <typename T>
void Bucket_Array_Remove(Bucket_Array<T>& arr, Bucket_Locator& locator) {
    Assert(locator.bucket_index < arr.used_buckets_count);
    auto& bucket = *(arr.buckets + locator.bucket_index);
    Assert(Bucket_Occupied(bucket, locator.slot_index));

    bool was_full = (bucket.count == arr.items_per_bucket);

    BUCKET_UNMARK_OCCUPIED(bucket, locator.slot_index);

    bucket.count -= 1;
    arr.count -= 1;

    if (was_full) {
        const auto found_index = Array_Find(
            arr.unfull_buckets, arr.unfull_buckets_count, bucket.bucket_index
        );
        Assert(found_index == -1);
        *(arr.unfull_buckets + arr.unfull_buckets_count) = bucket.bucket_index;
        arr.unfull_buckets_count++;
    }
}

template <typename T>
struct Bucket_Array_Iterator : public Iterator_Facade<Bucket_Array_Iterator<T>> {
    Bucket_Array_Iterator() = delete;

    Bucket_Array_Iterator(Bucket_Array<T>* arr) : Bucket_Array_Iterator(arr, 0, 0) {}

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
    Bucket_Array_Iterator end() const { return {_arr, 0, _arr->used_buckets_count}; }

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
        Assert(false);
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

    ttuple<Bucket_Locator, T*> Dereference() const {
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
        Assert(false);
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
    Vector_Iterator(Vector<T>* container) : Vector_Iterator(container, 0) {}
    Vector_Iterator(Vector<T>* container, i32 current)
        : _current(current)
        , _container(container)  //
    {
        Assert(container != nullptr);
    }

    Vector_Iterator begin() const { return {_container, _current}; }
    Vector_Iterator end() const { return {_container, _container->count}; }

    T* Dereference() const {
        Assert(_current >= 0);
        Assert(_current < _container->count);
        return _container->base + _current * sizeof(T);
    }

    void Increment() { _current++; }

    bool Equal_To(const Vector_Iterator& o) const { return _current == o._current; }

private:
    Vector<T>* _container;
    i32        _current = 0;
};

template <typename T>
struct TVector_Iterator : public Iterator_Facade<TVector_Iterator<T>> {
    TVector_Iterator() = delete;
    TVector_Iterator(tvector<T>* container) : TVector_Iterator(container, 0) {}
    TVector_Iterator(tvector<T>* container, u64 current)
        : _current(current), _container(container) {
        Assert(container != nullptr);
    }

    TVector_Iterator begin() const { return {_container, _current}; }
    TVector_Iterator end() const { return {_container, scast<u64>(_container->size())}; }

    T* Dereference() const {
        Assert(_current >= 0);
        Assert(_current < _container->size());
        return scast<T*>(_container->data()) + _current;
    }

    void Increment() { _current++; }

    bool Equal_To(const TVector_Iterator& o) const { return _current == o._current; }

private:
    tvector<T>* _container;
    u64         _current = 0;
};

// NOTE: Grid_Of_Vectors_Iterator - это итератор для
// прохождения по элементам на конкретной клетке.
template <typename T>
struct Grid_Of_Vectors_Iterator : public Iterator_Facade<Grid_Of_Vectors_Iterator<T>> {
    Grid_Of_Vectors_Iterator() = delete;
    Grid_Of_Vectors_Iterator(Grid_Of_Vectors<T>* container, v2i16 pos, i16 stride)
        : Grid_Of_Vectors_Iterator(container, 0, pos, stride) {}
    Grid_Of_Vectors_Iterator(
        Grid_Of_Vectors<T>* container,
        i32                 current,
        v2i16               pos,
        i16                 stride  //
    )
        : _container(container)
        , _current(current)
        , _pos(pos)
        , _stride(stride)  //
    {
        Assert(container != nullptr);
    }

    Grid_Of_Vectors_Iterator begin() const {
        return {_container, _current, _pos, _stride};
    }
    Grid_Of_Vectors_Iterator end() const {
        const i32 count = *Get_By_Stride(_container->counts, _pos, _stride);
        return {_container, count, _pos, _stride};
    }

    T* Dereference() const {
        const auto count = *Get_By_Stride(_container->counts, _pos, _stride);
        Assert(_current >= 0);
        Assert(_current < count);
        T* base = *Get_By_Stride(_container->bases, _pos, _stride);
        return base + _current * sizeof(T);
    }

    void Increment() { _current++; }

    bool Equal_To(const Grid_Of_Vectors_Iterator& o) const {
        return _current == o._current  //
               && _pos == o._pos       //
               && _stride == o._stride;

        // TODO: Подумать. Если не нужно сравнивать несколько разных итераторов
        // между собой, т.е., когда pos и stride не изменяются,
        // можно ли просто раскомментить это?
        // return _current == o._current;
    }

private:
    Grid_Of_Vectors<T>* const _container;
    i32                       _current = 0;
    const v2i16               _pos;
    const i16                 _stride;
};

template <typename T>
BF_FORCE_INLINE auto Iter(Bucket_Array<T>* container) {
    return Bucket_Array_Iterator(container);
}

template <typename T>
BF_FORCE_INLINE auto Iter(tvector<T>* container) {
    return TVector_Iterator(container);
}

template <typename T>
BF_FORCE_INLINE auto Iter(Grid_Of_Vectors<T>* container, v2i16 pos, i16 stride) {
    return Grid_Of_Vectors_Iterator(container, pos, stride);
}

template <typename T>
BF_FORCE_INLINE auto Iter_With_Locator(Bucket_Array<T>* arr) {
    return Bucket_Array_With_Locator_Iterator(arr);
}

template <typename T>
BF_FORCE_INLINE void Container_Reset(Queue<T>& container) {
    container.count = 0;
}

template <typename T>
BF_FORCE_INLINE void Container_Reset(tvector<T>& container) {
    container.clear();
}

template <typename T>
BF_FORCE_INLINE void Container_Reset(Vector<T>& container) {
    container.count = 0;
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
