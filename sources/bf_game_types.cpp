#pragma once

// ============================================================= //
//                     Forward Declarations                      //
// ============================================================= //
// SHIT: Oh, god, I hate this shit
struct Game_State;

#ifdef BF_CLIENT
struct Game_Renderer_State;
struct Loaded_Texture;
#endif

// ============================================================= //
//                        Data Structures                        //
// ============================================================= //

#define ALLOCATE__FUNCTION(name) void* name(size_t n, size_t alignment)
#define FREE__FUNCTION(name) void name(void* mem)

struct Allocator_Functions {
    ALLOCATE__FUNCTION((*allocate));
    FREE__FUNCTION((*free));
};

#define CHECK_CONTAINER_ALLOCATOR_FUNCTIONS                        \
    {                                                              \
        Assert(container.allocator_functions.allocate != nullptr); \
        Assert(container.allocator_functions.free != nullptr);     \
    }

// ----- Queues -----

// PERF: Переписать на ring buffer!
template <typename T>
struct Fixed_Size_Queue {
    size_t memory_size;
    i32 count;
    T* base;
};

// PERF: Переписать на ring buffer!
template <typename T>
    requires std::is_trivially_copyable_v<T>
struct Queue {
    u32 max_count;
    i32 count;
    T* base;

    Allocator_Functions allocator_functions;
};

// PERF: Переписать на ring buffer!
template <typename T, typename Allocation_Tag>
    requires std::is_trivially_copyable_v<T>
struct Static_Allocation_Queue {
    u32 max_count;
    i32 count;
    T* base;

    static Allocator_Functions allocator_functions;
};

template <typename T>
struct Vector {
    T* base;
    i32 count;
    u32 max_count;

    Allocator_Functions allocator_functions;
};

template <typename T, typename Allocation_Tag>
// requires std::is_trivially_copyable_v<T>
struct Static_Allocation_Vector {
    T* base;
    i32 count;
    u32 max_count;

    static Allocator_Functions allocator_functions;
};

template <typename T, typename Allocation_Tag>
    requires std::is_trivially_copyable_v<T>
Allocator_Functions Static_Allocation_Queue<T, Allocation_Tag>::allocator_functions = {};

template <typename T, typename Allocation_Tag>
// requires std::is_trivially_copyable_v<T>
Allocator_Functions Static_Allocation_Vector<T, Allocation_Tag>::allocator_functions = {};

using Bucket_Index = u32;

template <typename T>
struct Bucket {
    void* occupied;
    void* data;

    i32 count;
    Bucket_Index bucket_index;
};

struct Bucket_Locator {
    Bucket_Index bucket_index;
    u32 slot_index;
};

// TODO:
// 1) Нужно ли реализовать expandable количество бакетов?
template <typename T>
struct Bucket_Array : Non_Copyable {
    Allocator_Functions allocator_functions;

    Bucket<T>* buckets;
    Bucket_Index* unfull_buckets;

    i64 count;
    i32 items_per_bucket;

    Bucket_Index buckets_count;
    Bucket_Index used_buckets_count;
    Bucket_Index unfull_buckets_count;
};

template <typename T>
void Enqueue(Fixed_Size_Queue<T>& container, const T value) {
    // TODO: Test!

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

template <typename T, typename Allocation_Tag>
void* Queue_Allocate(
    Static_Allocation_Queue<T, Allocation_Tag>& container,
    i32 bytes,
    i32 alignment
) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    auto result = rcast<T*>(container.allocator_functions.allocate(bytes, alignment));
    return result;
}

template <typename T>
void* Queue_Allocate(Queue<T>& container, i32 bytes, i32 alignment) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    auto result = rcast<T*>(container.allocator_functions.allocate(bytes, alignment));
    return result;
}

template <typename T, typename Allocation_Tag>
void Queue_Free(Static_Allocation_Queue<T, Allocation_Tag>& container, void* ptr) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    container.allocator_functions.free(ptr);
}

template <typename T>
void Queue_Free(Queue<T>& container, void* ptr) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    container.allocator_functions.free(ptr);
}

template <typename T>
void Enqueue(Queue<T>& container, const T value)
    requires std::is_trivially_copyable_v<T>
{
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);
        container.max_count = 8;
        container.base = rcast<T*>(container.allocator_functions.allocate(
            container.max_count * sizeof(T), alignof(T)
        ));
    }
    else if (container.max_count == container.count) {
        u32 doubled_max_count = container.max_count * 2;
        Assert(container.max_count < doubled_max_count);
        auto size = sizeof(T) * container.max_count;
        auto old_ptr = container.base;

        container.base
            = rcast<T*>(container.allocator_functions.allocate(size * 2, alignof(T)));
        memcpy(container.base, old_ptr, size);
        container.allocator_functions.free(old_ptr);

        container.max_count *= 2;
    }

    *(container.base + container.count) = value;
    container.count++;
}

template <typename T>
void Bulk_Enqueue(Queue<T>& container, const T* values, const u32 values_count)
    requires std::is_trivially_copyable_v<T>
{
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    // TODO: Test!
    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = MAX(Ceil_To_Power_Of_2(values_count), 8);
        size_t memory_size = container.max_count * sizeof(T);
        Assert(memory_size / sizeof(T) == container.max_count);  // NOTE: Ловим overflow

        container.base
            = rcast<T*>(container.allocator_functions.allocate(memory_size, alignof(T)));
    }
    else if (container.max_count < container.count + values_count) {
        auto old_ptr = container.base;
        u32 new_max_count = Ceil_To_Power_Of_2(container.max_count + values_count);
        size_t old_memory_size = sizeof(T) * container.max_count;
        size_t new_memory_size = sizeof(T) * new_max_count;

        // NOTE: Ловим overflow
        Assert(container.max_count < new_max_count);
        Assert(old_memory_size < new_memory_size);

        container.base = rcast<T*>(
            container.allocator_functions.allocate(new_memory_size, alignof(T))
        );
        memcpy(container.base, old_ptr, sizeof(T) * container.count);
        container.allocator_functions.free(old_ptr);

        container.max_count = new_max_count;
    }

    memcpy(container.base + container.count, values, sizeof(T) * values_count);
    container.count += values_count;
}

template <typename T, typename Allocation_Tag>
void Bulk_Enqueue(
    Static_Allocation_Queue<T, Allocation_Tag>& container,
    const T* values,
    const u32 values_count
)
    requires std::is_trivially_copyable_v<T>
{
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    // TODO: Test!
    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = MAX(Ceil_To_Power_Of_2(values_count), 8);
        size_t memory_size = container.max_count * sizeof(T);
        Assert(memory_size / sizeof(T) == container.max_count);  // NOTE: Ловим overflow

        container.base
            = rcast<T*>(container.allocator_functions.allocate(memory_size, alignof(T)));
    }
    else if (container.max_count < container.count + values_count) {
        auto old_ptr = container.base;
        u32 new_max_count = Ceil_To_Power_Of_2(container.max_count + values_count);
        size_t old_memory_size = sizeof(T) * container.max_count;
        size_t new_memory_size = sizeof(T) * new_max_count;

        // NOTE: Ловим overflow
        Assert(container.max_count < new_max_count);
        Assert(old_memory_size < new_memory_size);

        container.base = rcast<T*>(
            container.allocator_functions.allocate(new_memory_size, alignof(T))
        );
        memcpy(container.base, old_ptr, sizeof(T) * container.count);
        container.allocator_functions.free(old_ptr);

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

template <typename T, typename Allocation_Tag>
void Enqueue(Static_Allocation_Queue<T, Allocation_Tag>& container, const T value) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);
        container.max_count = 8;
        container.base = rcast<T*>(container.allocator_functions.allocate(
            container.max_count * sizeof(T), alignof(T)
        ));
    }
    else if (container.max_count == container.count) {
        u32 doubled_max_count = container.max_count * 2;
        Assert(container.max_count < doubled_max_count);  // Поймаем overflow
        auto size = sizeof(T) * container.max_count;
        auto old_ptr = container.base;

        // TODO: Почитать про realloc
        container.base
            = rcast<T*>(container.allocator_functions.allocate(size * 2, alignof(T)));
        memcpy(container.base, old_ptr, size);
        container.allocator_functions.free(old_ptr);

        container.max_count *= 2;
    }

    *(container.base + container.count) = value;
    container.count++;
}

// PERF: Много memmove происходит
template <typename T, typename Allocation_Tag>
T Dequeue(Static_Allocation_Queue<T, Allocation_Tag>& container) {
    // TODO: Test!
    Assert(container.base != nullptr);
    Assert(container.count > 0);

    T res = *container.base;
    container.count -= 1;
    if (container.count > 0)
        memmove(container.base, container.base + 1, sizeof(T) * container.count);

    return res;
}

template <typename T, typename Allocation_Tag>
i32 Queue_Find(Static_Allocation_Queue<T, Allocation_Tag>& container, T value) {
    FOR_RANGE(i32, i, container.count) {
        auto& v = *(container.base + i);
        if (v == value)
            return i;
    }

    return -1;
}

template <typename T, typename Allocation_Tag>
i32 Vector_Find(Static_Allocation_Vector<T, Allocation_Tag>& container, T value) {
    FOR_RANGE(i32, i, container.count) {
        auto& v = *(container.base + i);
        if (v == value)
            return i;
    }

    return -1;
}

template <typename T, typename Allocation_Tag>
i32 Vector_Find_Ptr(
    Static_Allocation_Vector<T, Allocation_Tag>& container,
    T* value_ptr
) {
    Assert(container.base <= value_ptr);
    Assert(value_ptr < container.base + container.count * sizeof(T));
    Assert((value_ptr - container.base) % sizeof(T) == 0);
    return (value_ptr - container.base) / sizeof(T);
}

template <typename T>
i32 Vector_Find(Vector<T>& container, T value) {
    FOR_RANGE(i32, i, container.count) {
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

template <typename T, typename Allocation_Tag>
    requires std::is_trivially_copyable_v<T>
void Remove_From_Queue_At(Static_Allocation_Queue<T, Allocation_Tag>& container, i32 i) {
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
i32 Vector_Add(Vector<T>& container, T& value) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    i32 locator = container.count;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = 8;
        container.base = rcast<T*>(container.allocator_functions.allocate(
            container.max_count * sizeof(T), alignof(T)
        ));
    }
    else if (container.max_count == container.count) {
        u32 doubled_max_count = container.max_count * 2;
        Assert(container.max_count < doubled_max_count);  // NOTE: Ловим overflow
        auto size = sizeof(T) * container.max_count;
        auto old_ptr = container.base;

        container.base
            = rcast<T*>(container.allocator_functions.allocate(size * 2, alignof(T)));
        memcpy(container.base, old_ptr, size);
        container.allocator_functions.free(old_ptr);

        container.max_count *= 2;
    }

    *(container.base + container.count) = value;
    container.count += 1;

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

template <typename T, typename Allocation_Tag>
void Vector_Remove_At(Static_Allocation_Vector<T, Allocation_Tag>& container, i32 i) {
    Assert(i >= 0);
    Assert(i < container.count);

    i32 delta_count = container.count - i - 1;
    Assert(delta_count >= 0);

    if (delta_count > 0) {
        memmove(container.base + i, container.base + i + 1, sizeof(T) * delta_count);
    }

    container.count -= 1;
}

template <typename T, typename Allocation_Tag>
i32 Vector_Add(Static_Allocation_Vector<T, Allocation_Tag>& container, T& value) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    i32 locator = container.count;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = 8;
        container.base = rcast<T*>(container.allocator_functions.allocate(
            container.max_count * sizeof(T), alignof(T)
        ));
    }
    else if (container.max_count == container.count) {
        u32 doubled_max_count = container.max_count * 2;
        Assert(container.max_count < doubled_max_count);  // NOTE: Ловим overflow
        auto size = sizeof(T) * container.max_count;
        auto old_ptr = container.base;

        container.base
            = rcast<T*>(container.allocator_functions.allocate(size * 2, alignof(T)));
        memcpy(container.base, old_ptr, size);
        container.allocator_functions.free(old_ptr);

        container.max_count *= 2;
    }

    *(container.base + container.count) = value;
    container.count += 1;

    return locator;
}

template <typename T, typename Allocation_Tag>
i32 Vector_Add(Static_Allocation_Vector<T, Allocation_Tag>& container, T&& value) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    i32 locator = container.count;

    if (container.base == nullptr) {
        Assert(container.max_count == 0);
        Assert(container.count == 0);

        container.max_count = 8;
        container.base = rcast<T*>(container.allocator_functions.allocate(
            container.max_count * sizeof(T), alignof(T)
        ));
    }
    else if (container.max_count == container.count) {
        u32 doubled_max_count = container.max_count * 2;
        Assert(container.max_count < doubled_max_count);  // NOTE: Ловим overflow
        auto size = sizeof(T) * container.max_count;
        auto old_ptr = container.base;

        container.base
            = rcast<T*>(container.allocator_functions.allocate(size * 2, alignof(T)));
        memcpy(container.base, old_ptr, size);
        container.allocator_functions.free(old_ptr);

        container.max_count *= 2;
    }

    *(container.base + container.count) = value;
    container.count += 1;

    return locator;
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
    FOR_RANGE(u32, i, n) {
        auto& v = *(values + n);
        if (v == value)
            return true;
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
Bucket<T>* Add_Bucket(Bucket_Array<T>& container) {
    CHECK_CONTAINER_ALLOCATOR_FUNCTIONS;

    Assert(container.unfull_buckets_count == 0);
    Assert(container.items_per_bucket > 0);
    Assert(container.buckets_count > 0);

    // NOTE: Это код инициализации. Подумать, не нужно
    // ли его инициализировать в самом начале его создания

    if (container.buckets == nullptr) {  // NOTE: Следовательно, это первый вызов.
        Assert(container.unfull_buckets == nullptr);

        typedef Bucket<T> arr_type;

        constexpr auto align = alignof(arr_type*);
        auto alloc_size = container.buckets_count * sizeof(arr_type);

        container.buckets
            = rcast<Bucket<T>*>(container.allocator_functions.allocate(alloc_size, align)
            );
        if (container.buckets == nullptr) {
            Assert(false);
            return nullptr;
        }

        container.unfull_buckets
            = rcast<Bucket_Index*>(container.allocator_functions.allocate(
                container.buckets_count * sizeof(Bucket_Index), alignof(Bucket_Index)
            ));
        if (container.unfull_buckets == nullptr) {
            Assert(false);
            return nullptr;
        }
    }

    Assert(container.buckets != nullptr);
    Assert(container.unfull_buckets != nullptr);

    auto new_bucket = container.buckets + container.used_buckets_count;
    new_bucket->bucket_index = container.used_buckets_count;
    new_bucket->count = 0;

    auto occupied_bytes_count = Ceil_Division(container.items_per_bucket, 8);
    auto occupied
        = rcast<u8*>(container.allocator_functions.allocate(occupied_bytes_count, 1));
    if (occupied == nullptr) {
        Assert(false);
        return nullptr;
    }
    memset(occupied, 0, occupied_bytes_count);

    auto data = rcast<u8*>(container.allocator_functions.allocate(
        sizeof(T) * container.items_per_bucket, alignof(T)
    ));
    if (data == nullptr) {
        Assert(false);
        return nullptr;
    }

    new_bucket->occupied = occupied;
    new_bucket->data = data;

    *(container.unfull_buckets + container.unfull_buckets_count)
        = container.used_buckets_count;
    container.used_buckets_count++;
    container.unfull_buckets_count++;

    return new_bucket;
}

template <typename T>
ttuple<Bucket_Locator, T*> Find_And_Occupy_Empty_Slot(Bucket_Array<T>& arr) {
    if (arr.unfull_buckets_count == 0)
        Add_Bucket(arr);
    // TODO: Some kind of error handling!
    Assert(arr.unfull_buckets_count > 0);

    Bucket_Index bucket_index = *(arr.unfull_buckets + 0);
    auto bucket_ptr = arr.buckets + bucket_index;

    int index = -1;
    FOR_RANGE(int, i, arr.items_per_bucket) {
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
    locator.bucket_index = bucket_ptr->bucket_index;
    locator.slot_index = index;

    return {locator, scast<T*>(bucket_ptr->data) + index};
}

template <typename T>
Bucket_Locator Bucket_Array_Add(Bucket_Array<T>& arr, T& item) {
    auto [locator, ptr] = Find_And_Occupy_Empty_Slot(arr);
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
        auto exists = Array_Find(
            arr.unfull_buckets, arr.unfull_buckets_count, bucket.bucket_index
        );
        Assert(!exists);
        *(arr.unfull_buckets + arr.unfull_buckets_count) = bucket.bucket_index;
        arr.unfull_buckets_count++;
    }
}

template <typename T>
class Bucket_Array_Iterator : public Iterator_Facade<Bucket_Array_Iterator<T>> {
public:
    Bucket_Array_Iterator() = delete;

    Bucket_Array_Iterator(Bucket_Array<T>* arr) : Bucket_Array_Iterator(arr, 0, 0) {}

    Bucket_Array_Iterator(
        Bucket_Array<T>* arr,
        i32 current,
        Bucket_Index current_bucket  //
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
        FOR_RANGE(int, _GUARD_, 256) {
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

    bool Equal_To(const Bucket_Array_Iterator& o) const {
        return _current == o._current && _current_bucket == o._current_bucket;
    }

private:
    int _Get_Current_Bucket_Count() {
        auto& bucket = *(_arr->buckets + _current_bucket);
        return bucket.count;
    }

    Bucket_Array<T>* _arr;
    i32 _current = 0;
    Bucket_Index _current_bucket = 0;
    u16 _current_bucket_count = 0;
};

template <typename T>
class Bucket_Array_With_Locator_Iterator
    : public Iterator_Facade<Bucket_Array_With_Locator_Iterator<T>> {
public:
    Bucket_Array_With_Locator_Iterator() = delete;

    Bucket_Array_With_Locator_Iterator(Bucket_Array<T>* arr)
        : Bucket_Array_With_Locator_Iterator(arr, 0, 0) {}

    Bucket_Array_With_Locator_Iterator(
        Bucket_Array<T>* arr,
        i32 current,
        Bucket_Index current_bucket  //
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
        FOR_RANGE(int, _GUARD_, 256) {
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
    i32 _current = 0;
    Bucket_Index _current_bucket = 0;
    u16 _current_bucket_count = 0;
};

template <typename T>
class Vector_Iterator : public Iterator_Facade<Vector_Iterator<T>> {
public:
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
        return _container->base + _container->count - 1;
    }

    void Increment() { _current++; }

    bool Equal_To(const Vector_Iterator& o) const { return _current == o._current; }

private:
    Vector<T>* _container;
    i32 _current = 0;
};

template <typename T, typename Allocation_Tag>
class Static_Allocation_Vector_Iterator
    : public Iterator_Facade<Static_Allocation_Vector_Iterator<T, Allocation_Tag>> {
public:
    Static_Allocation_Vector_Iterator() = delete;

    Static_Allocation_Vector_Iterator(
        Static_Allocation_Vector<T, Allocation_Tag>* container
    )
        : Static_Allocation_Vector_Iterator(container, 0) {}

    Static_Allocation_Vector_Iterator(
        Static_Allocation_Vector<T, Allocation_Tag>* container,
        i32 current
    )
        : _current(current)
        , _container(container)  //
    {
        Assert(container != nullptr);
    }

    Static_Allocation_Vector_Iterator begin() const { return {_container, _current}; }
    Static_Allocation_Vector_Iterator end() const {
        return {_container, _container->count};
    }

    T* Dereference() const {
        Assert(_current >= 0);
        Assert(_current < _container->count);
        return _container->base + _container->count - 1;
    }

    void Increment() { _current++; }

    bool Equal_To(const Static_Allocation_Vector_Iterator& o) const {
        return _current == o._current;
    }

private:
    Static_Allocation_Vector<T, Allocation_Tag>* _container;
    i32 _current = 0;
};

template <typename T>
auto Iter(Bucket_Array<T>* arr) {
    return Bucket_Array_Iterator(arr);
}

template <typename T, typename Allocation_Tag>
auto Iter(Static_Allocation_Vector<T, Allocation_Tag>* container) {
    return Static_Allocation_Vector_Iterator(container);
}

template <typename T>
auto Iter_With_Locator(Bucket_Array<T>* arr) {
    return Bucket_Array_With_Locator_Iterator(arr);
}

template <typename T>
BF_FORCE_INLINE void Container_Reset(Queue<T>& container) {
    container.count = 0;
}

template <typename T, typename Allocation_Tag>
BF_FORCE_INLINE void Container_Reset(  //
    Static_Allocation_Queue<T, Allocation_Tag>& container
) {
    container.count = 0;
}

template <typename T>
BF_FORCE_INLINE void Container_Reset(Vector<T>& container) {
    container.count = 0;
}

template <typename T, typename Allocation_Tag>
BF_FORCE_INLINE void Container_Reset(
    Static_Allocation_Vector<T, Allocation_Tag>& container
) {
    container.count = 0;
}

template <typename T>
BF_FORCE_INLINE void Container_Reset(Bucket_Array<T>& container) {
    container.count = 0;
    container.unfull_buckets_count = container.used_buckets_count;

    FOR_RANGE(Bucket_Index, i, container.unfull_buckets_count) {
        *(container.unfull_buckets + i) = i;
    }

    auto occupied_bytes_count = Ceil_Division(container.items_per_bucket, 8);
    FOR_RANGE(Bucket_Index, i, container.used_buckets_count) {
        auto& bucket = *(container.buckets + i);

        if (bucket.count > 0) {
            memset(bucket.occupied, 0, occupied_bytes_count);
            bucket.count = 0;
        }
    }
    container.used_buckets_count = 0;
}

// ============================================================= //
//                          Game Logic                           //
// ============================================================= //
enum class Direction {
    Right = 0,
    Up = 1,
    Left = 2,
    Down = 3,
};

#define FOR_DIRECTION(var_name)                             \
    for (auto var_name = (Direction)0; (int)(var_name) < 4; \
         var_name = (Direction)((int)(var_name) + 1))

v2i16 As_Offset(Direction dir) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    return v2i16_adjacent_offsets[(unsigned int)dir];
}

Direction Opposite(Direction dir) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    return (Direction)(((u8)(dir) + 2) % 4);
}

using Graph_Nodes_Count = u16;

struct Graph : public Non_Copyable {
    Graph_Nodes_Count nodes_count;
    u8* nodes;  // 0b0000DLUR

    v2i16 size;
    v2i16 offset;

    // TODO: Calculate it
    v2i16 center;
};

BF_FORCE_INLINE bool Graph_Contains(const Graph& graph, v2i16 pos) {
    auto off = pos - graph.offset;
    bool result = Pos_Is_In_Bounds(off, graph.size);
    return result;
}

BF_FORCE_INLINE u8 Graph_Node(const Graph& graph, v2i16 pos) {
    auto off = pos - graph.offset;
    Assert(Pos_Is_In_Bounds(off, graph.size));

    u8* node_ptr = graph.nodes + off.y * graph.size.x + off.x;
    u8 result = *node_ptr;
    return result;
}

struct Human;
struct Building;
struct Scriptable_Resource;
struct Graph_Segment;

enum class Map_Resource_Booking_Type {
    Construction,
    Processing,
};

struct Map_Resource_Booking {
    Map_Resource_Booking_Type type;
    Building* building;
    i32 priority;
};

struct Graph_Segment;

struct Map_Resource {
    using ID = u32;

    ID id;
    Scriptable_Resource* scriptable;

    v2i16 pos;

    Map_Resource_Booking* booking;

    struct Transportation_Segments_Allocation_Tag {};
    Static_Allocation_Vector<Graph_Segment*, Transportation_Segments_Allocation_Tag>
        transportation_segments;

    struct Transportation_Vertices_Allocation_Tag {};
    Static_Allocation_Vector<v2i16, Transportation_Vertices_Allocation_Tag>
        transportation_vertices;

    Human* targeted_human;
    Human* carrying_human;
};

// NOTE: Сегмент - это несколько склеенных друг с другом клеток карты,
// на которых может находиться один человек, который перетаскивает предметы.
//
// Сегменты формируются между зданиями, дорогами и флагами.
// Обозначим их клетки следующими символами:
//
// B - Building - Здание.       "Вершинная" клетка
// F - Flag     - Флаг.         "Вершинная" клетка
// r - Road     - Дорога.    Не "вершинная" клетка
// . - Empty    - Пусто.
//
// Сегменты формируются между "вершинными" клетками, соединёнными дорогами.
//
// Примеры:
//
// 1)  rr - не сегмент, т.к. это просто 2 дороги - невершинные клетки.
//
// 2)  BB - не сегмент. Тут 2 здания соединены рядом, но между ними нет дороги.
//
// 3)  BrB - сегмент. 2 здания. Между ними есть дорога.
//
// 4)  BB
//     rr - сегмент. Хоть 2 здания стоят рядом, между ними можно пройти по дороге снизу.
//
// 5)  B.B
//     r.r
//     rrr - сегмент. Аналогично предыдущему, просто дорога чуть длиннее.
//
// 6)  BF - не сегмент. Тут здание и флаг соединены рядом, но между ними нет дороги.
//
// 7)  BrF - сегмент. Здание и флаг, между которыми дорога.
//
// 8)  FrF - сегмент. 2 флага, между которыми дорога.
//
// 9)  BrFrB - это уже 2 разных сегмента. Первый - BrF, второй - FrB.
//             При замене флага (F) на дорогу (r) эти 2 сегмента сольются в один - BrrrB.
//
struct Graph_Segment : public Non_Copyable {
    Graph_Nodes_Count vertices_count;
    v2i16* vertices;  // NOTE: Вершинные клетки графа (флаги, здания)

    Graph graph;
    Bucket_Locator locator;

    Human* assigned_human;
    struct Linked_Segments_Tag {};
    Static_Allocation_Vector<Graph_Segment*, Linked_Segments_Tag> linked_segments;

    struct Resources_To_Transport {};
    Static_Allocation_Queue<Map_Resource, Resources_To_Transport> resources_to_transport;
};

struct Graph_Segment_Precalculated_Data {
    // TODO: Reimplement `CalculatedGraphPathData` calculation from the old repo
};

[[nodiscard]] bool Graph_Node_Has(u8 node, Direction d) {
    Assert((u8)d >= 0);
    Assert((u8)d < 4);
    return node & (1 << (u8)d);
}

[[nodiscard]] u8 Graph_Node_Mark(u8 node, Direction d, b32 value) {
    Assert((u8)d >= 0);
    Assert((u8)d < 4);
    auto dir = (u8)d;

    if (value)
        node |= (u8)(1 << dir);
    else
        node &= (u8)(15 ^ (1 << dir));

    Assert(node < 16);
    return node;
}

void Graph_Update(Graph& graph, v2i16 pos, Direction dir, bool value) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    Assert(graph.offset.x == 0);
    Assert(graph.offset.y == 0);
    auto& node = *(graph.nodes + pos.y * graph.size.x + pos.x);

    bool node_is_zero_but_wont_be_after = (node == 0) && value;
    bool node_is_not_zero_but_will_be
        = (!value) && (node != 0) && (Graph_Node_Mark(node, dir, false) == 0);

    if (node_is_zero_but_wont_be_after)
        graph.nodes_count += 1;
    else if (node_is_not_zero_but_will_be)
        graph.nodes_count -= 1;

    node = Graph_Node_Mark(node, dir, value);
}

using Scriptable_Resource_ID = u16;
global Scriptable_Resource_ID global_forest_resource_id = 1;

using Player_ID = u8;
using Building_ID = u32;
using Human_ID = u32;

enum class Building_Type {
    City_Hall,
    Harvest,
    Plant,
    Fish,
    Produce,
};

struct Scriptable_Building : public Non_Copyable {
    const char* name;
    Building_Type type;

#ifdef BF_CLIENT
    Loaded_Texture* texture;
#endif  // BF_CLIENT

    Scriptable_Resource_ID harvestable_resource_id;

    f32 human_spawning_delay;
};

enum class Human_Type {
    Transporter,
    Constructor,
    Employee,
};

struct Human_Moving_Component {
    v2i16 pos;
    f32 elapsed;
    f32 progress;
    v2f from;

    toptional<v2i16> to;
    struct Human_Moving_Component_Allocation_Tag {};
    Static_Allocation_Queue<v2i16, Human_Moving_Component_Allocation_Tag> path;
};

enum class Moving_In_The_World_State {
    None = 0,
    Moving_To_The_City_Hall,
    Moving_To_Destination,
};

struct Moving_Inside_Segment {
    //
};

// struct Main_Controller {
//     // Building_Database bdb;
//     // Human_Database db;
//     Moving_In_The_World moving_in_the_world;
//     Moving_Inside_Segment moving_inside_segment;
//     // Moving_Resources moving_resources;
//     // Construction_Controller construction_controller;
//     // Employee_Controller employee_controller;
//     // Human_Data data;
// };

struct Main_Controller;

struct Moving_In_The_World {
    Moving_In_The_World_State state;
    Main_Controller* controller;
};

enum class Human_Main_State {
    None = 0,

    // Common
    Moving_In_The_World,

    // Transporter
    Moving_Inside_Segment,
    Moving_Resource,

    // Builder
    Building,

    // Employee
    Employee,
};

struct Building;

struct Human : public Non_Copyable {
    Player_ID player_id;
    Human_Moving_Component moving;

    Graph_Segment* segment;
    Human_Type type;

    Human_Main_State state;
    Moving_In_The_World_State state_moving_in_the_world;

    // Human_ID id;
    Building* building;
    // Moving_Resources_State state_moving_resources;
    //
    // f32 moving_resources__picking_up_resource_elapsed;
    // f32 moving_resources__picking_up_resource_progress;
    // f32 moving_resources__placing_resource_elapsed;
    // f32 moving_resources__placing_resource_progress;
    //
    // Map_Resource* moving_resources__targeted_resource;
    //
    // f32 harvesting_elapsed;
    // i32 current_behaviour_id = -1;
    //
    // Employee_Behaviour_Set behaviour_set;
    // f32 processing_elapsed;
    //
    Human(const Human&& o) {
        player_id = std::move(o.player_id);
        moving = std::move(o.moving);
        segment = std::move(o.segment);
        type = std::move(o.type);
        state = std::move(o.state);
        state_moving_in_the_world = std::move(o.state_moving_in_the_world);
        building = std::move(o.building);
    }

    Human& operator=(Human&& o) {
        player_id = std::move(o.player_id);
        moving = std::move(o.moving);
        segment = std::move(o.segment);
        type = std::move(o.type);
        state = std::move(o.state);
        state_moving_in_the_world = std::move(o.state_moving_in_the_world);
        building = std::move(o.building);
        return *this;
    }
};

struct Resource_To_Book : public Non_Copyable {
    Scriptable_Resource_ID scriptable_id;
    u8 amount;
};

struct Building : public Non_Copyable {
    Player_ID player_id;

    Human* constructor;
    Human* employee;

    Scriptable_Building* scriptable;

    size_t resources_to_book_count;
    Resource_To_Book* resources_to_book;
    v2i16 pos;

    Building_ID id;
    f32 time_since_human_was_created;

    bool employee_is_inside;
    bool is_constructed;

    // Bucket_Locator locator;
    // f32 time_since_item_was_placed;
};

enum class Terrain {
    None = 0,
    Grass,
};

struct Terrain_Tile : public Non_Copyable {
    Terrain terrain;
    // NOTE: Height starts at 0
    int height;
    bool is_cliff;

    i16 resource_amount;
};

// NOTE: Upon editing ensure `Validate_Element_Tile` remains correct
enum class Element_Tile_Type {
    None = 0,
    Road = 1,
    Building = 2,
    Flag = 3,
};

struct Element_Tile : public Non_Copyable {
    Element_Tile_Type type;
    Building* building;
    Player_ID player_id;
};

void Validate_Element_Tile(Element_Tile& tile) {
    Assert((int)tile.type >= 0);
    Assert((int)tile.type <= 3);

    if (tile.type == Element_Tile_Type::Building)
        Assert(tile.building != nullptr);
    else
        Assert(tile.building == nullptr);
}

struct Scriptable_Resource : public Non_Copyable {
    const char* name;
};

// NOTE: `scriptable` is `null` when `amount` = 0
struct Terrain_Resource {
    Scriptable_Resource_ID scriptable_id;

    u8 amount;
};

enum class Item_To_Build_Type {
    None,
    Road,
    Flag,
    Building,
};

struct Item_To_Build : public Non_Copyable {
    Item_To_Build_Type type;
    Scriptable_Building* scriptable_building;

    Item_To_Build(Item_To_Build_Type a_type, Scriptable_Building* a_scriptable_building)
        : type(a_type), scriptable_building(a_scriptable_building) {}
};

static const Item_To_Build Item_To_Build_Road(Item_To_Build_Type::Road, nullptr);
static const Item_To_Build Item_To_Build_Flag(Item_To_Build_Type::Flag, nullptr);

enum class Human_Removal_Reason {
    Transporter_Returned_To_City_Hall,
    Employee_Reached_Building,
};

struct Game_Map_Data {
    f32 human_moving_one_tile_duration;
};

struct Game_Map : public Non_Copyable {
    v2i16 size;
    Terrain_Tile* terrain_tiles;
    Terrain_Resource* terrain_resources;
    Element_Tile* element_tiles;

    Bucket_Array<Building> buildings;
    Bucket_Array<Graph_Segment> segments;
    Bucket_Array<Human> humans;

    Bucket_Array<Human> humans_to_add;

    using Human_To_Remove = ttuple<Human_Removal_Reason, Human*, Bucket_Locator>;
    struct Human_To_Remove_Tag {};
    Static_Allocation_Vector<Human_To_Remove, Human_To_Remove_Tag> humans_to_remove;

    struct Segments_Queue_Tag {};
    Static_Allocation_Queue<Graph_Segment*, Segments_Queue_Tag> segments_that_need_humans;

    Allocator* segment_vertices_allocator;
    Allocator* graph_nodes_allocator;

    Game_Map_Data* data;
};

template <typename T>
struct Observer : public Non_Copyable {
    size_t count;
    T* functions;
};

// Usage:
//     INVOKE_OBSERVER(state.On_Item_Built, (state, game_map, pos, item))
#define INVOKE_OBSERVER(observer, code)                 \
    {                                                   \
        FOR_RANGE(size_t, i, observer.count) {          \
            auto& function = *(observer.functions + i); \
            function code;                              \
        }                                               \
    }

// Usage:
//     On_Item_Built__Function((*callbacks[])) = {
//         Renderer__On_Item_Built,
//     };
//     INITIALIZE_OBSERVER_WITH_CALLBACKS(state.On_Item_Built, callbacks, arena);
#define INITIALIZE_OBSERVER_WITH_CALLBACKS(observer, callbacks, arena)                 \
    {                                                                                  \
        (observer).count = sizeof(callbacks) / sizeof(callbacks[0]);                   \
        (observer).functions                                                           \
            = (decltype((observer).functions))(Allocate_((arena), sizeof(callbacks))); \
        memcpy((observer).functions, callbacks, sizeof(callbacks));                    \
    }

#define On_Item_Built__Function(name_) \
    void name_(Game_State& state, v2i16 pos, const Item_To_Build& item)

struct Game_State : public Non_Copyable {
    bool hot_reloaded;
    u16 dll_reloads_count;

    f32 offset_x;
    f32 offset_y;

    v2f player_pos;
    Game_Map game_map;

    size_t scriptable_resources_count;
    Scriptable_Resource* scriptable_resources;
    size_t scriptable_buildings_count;
    Scriptable_Building* scriptable_buildings;

    Scriptable_Building* scriptable_building_city_hall;
    Scriptable_Building* scriptable_building_lumberjacks_hut;

    Arena arena;
    Arena non_persistent_arena;  // Gets flushed on DLL reloads
    Arena trash_arena;  // Use for transient calculations

    Pages pages;

#ifdef BF_CLIENT
    Game_Renderer_State* renderer_state;
#endif  // BF_CLIENT

    Observer<On_Item_Built__Function((*))> On_Item_Built;
};

struct Game_Memory : public Non_Copyable {
    bool is_initialized;
    Game_State state;
};

enum class Tile_Updated_Type {
    Road_Placed,
    Flag_Placed,
    Flag_Removed,
    Road_Removed,
    Building_Placed,
    Building_Removed,
};

#ifdef BF_CLIENT
// ============================================================= //
//                    CLIENT. Game Rendering                     //
// ============================================================= //
using BF_Texture_ID = u32;

struct Loaded_Texture {
    BF_Texture_ID id;
    v2i size;
    u8* base;
};

using Tile_ID = u32;

enum class Tile_State_Check {
    Skip,
    Excluded,
    Included,
};

struct Tile_Rule {
    BF_Texture_ID texture_id;
    Tile_State_Check states[8];
};

struct Smart_Tile {
    Tile_ID id;
    BF_Texture_ID fallback_texture_id;

    int rules_count;
    Tile_Rule* rules;
};

struct Load_Smart_Tile_Result {
    bool success;
};

struct Tilemap {
    Tile_ID* tiles;
};

struct UI_Sprite_Params {
    bool smart_stretchable;
    v2i stretch_paddings_h;
    v2i stretch_paddings_v;
};

struct BF_Color {
    f32 r;
    f32 g;
    f32 b;
};

static constexpr BF_Color BF_Color_White = {1, 1, 1};
static constexpr BF_Color BF_Color_Black = {0, 0, 0};
static constexpr BF_Color BF_Color_Red = {1, 0, 0};
static constexpr BF_Color BF_Color_Green = {0, 1, 0};
static constexpr BF_Color BF_Color_Blue = {0, 0, 1};
static constexpr BF_Color BF_Color_Yellow = {1, 1, 0};
static constexpr BF_Color BF_Color_Cyan = {0, 1, 1};
static constexpr BF_Color BF_Color_Magenta = {1, 0, 1};

struct Game_UI_State : public Non_Copyable {
    UI_Sprite_Params buildables_panel_params;
    Loaded_Texture buildables_panel_background;
    Loaded_Texture buildables_placeholder_background;

    u16 buildables_count;
    Item_To_Build* buildables;

    i8 selected_buildable_index;
    BF_Color selected_buildable_color;
    BF_Color not_selected_buildable_color;
    v2f buildables_panel_sprite_anchor;
    v2f buildables_panel_container_anchor;
    f32 buildables_panel_in_scale;
    f32 scale;

    v2i padding;
    i8 placeholders;
    i16 placeholders_gap;
};

struct Game_Renderer_State : public Non_Copyable {
    Game_UI_State* ui_state;
    Game_Bitmap* bitmap;

    Smart_Tile grass_smart_tile;
    Smart_Tile forest_smart_tile;
    Tile_ID forest_top_tile_id;

    Loaded_Texture human_texture;
    Loaded_Texture grass_textures[17];
    Loaded_Texture forest_textures[3];
    Loaded_Texture road_textures[16];
    Loaded_Texture flag_textures[4];

    int tilemaps_count;
    Tilemap* tilemaps;

    u8 terrain_tilemaps_count;
    u8 resources_tilemap_index;
    u8 element_tilemap_index;

    v2i mouse_pos;

    bool panning;
    v2f pan_pos;
    v2f pan_offset;
    v2i pan_start_pos;
    f32 zoom_target;
    f32 zoom;

    f32 cell_size;

    bool shaders_compilation_failed;
    GLint ui_shader_program;
};
#endif  // BF_CLIENT

u8* Book_Single_Page(Pages& pages) {
    // NOTE: If there exists allocated page that is not in use -> return it
    FOR_RANGE(u32, i, pages.allocated_count) {
        bool& in_use = *(pages.in_use + i);
        if (!in_use) {
            in_use = true;
            return (pages.base + i)->base;
        }
    }

    // NOTE: Allocating more pages and mapping them
    Assert(pages.allocated_count < pages.total_count_cap);

    auto pages_to_allocate = OS_DATA.min_pages_per_allocation;
    auto allocation_address = OS_DATA.Allocate_Pages(pages_to_allocate);

    FOR_RANGE(u32, i, pages_to_allocate) {
        auto& page = *(pages.base + pages.allocated_count + i);
        page.base = allocation_address + (ptrd)i * OS_DATA.page_size;
    }

    // NOTE: Booking the first page that we allocated and returning it
    Page* result = pages.base + (ptrd)pages.allocated_count;

    *(pages.in_use + pages.allocated_count) = true;
    pages.allocated_count += pages_to_allocate;

    return result->base;
}
