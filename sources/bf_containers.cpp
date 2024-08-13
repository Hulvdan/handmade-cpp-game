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

    [[nodiscard]] T* Add_Unsafe() {
        Assert(count < max_count);
        auto result = items + count;
        count++;
        return result;
    }
};

template <typename T>
struct Fixed_Size_Queue {
    size_t memory_size = 0;
    i32    count       = 0;
    T*     base        = nullptr;

    T* Enqueue() {
        Assert(memory_size >= (count + 1) * sizeof(T));
        auto result = base + count;
        count++;
        return result;
    }

    // PERF: Переписать на ring buffer! Много memmove происходит.
    T Dequeue() {
        // TODO: Test!
        Assert(base != nullptr);
        Assert(count > 0);

        T result = *base;
        count--;
        if (count > 0)
            memmove((void*)base, (void*)(base + 1), sizeof(T) * count);

        return result;
    }
};

// PERF: Переписать на ring buffer!
template <typename T>
struct Queue {
    T*  base      = nullptr;
    i32 count     = 0;
    u32 max_count = 0;

    Allocator_function((*allocator_)) = nullptr;
    void* allocator_data_             = nullptr;

    i32 Index_Of(const T& value) {
        FOR_RANGE (i32, i, count) {
            auto& v = base[i];
            if (v == value)
                return i;
        }

        return -1;
    }

    T* Enqueue(MCTX) {
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
            memcpy((void*)new_base, (void*)base, sizeof(T) * max_count);
            FREE(base, sizeof(T) * max_count);

            base      = new_base;
            max_count = new_max_count;
        }

        count++;
        return base + (count - 1);
    }

    T* Bulk_Enqueue(const u32 values_count, MCTX) {
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
            memcpy((void*)new_base, (void*)base, sizeof(T) * count);
            FREE(base, sizeof(T) * max_count);

            base      = new_base;
            max_count = new_max_count;
        }

        count += values_count;

        return base + (count - values_count);
    }

    // PERF: Много memmove происходит
    T Dequeue() {
        Assert(base != nullptr);
        Assert(count > 0);

        T result = *base;
        count--;
        if (count > 0)
            memmove(base, base + 1, sizeof(T) * count);

        return result;
    }

    void Remove_At(i32 i) {
        Assert(i >= 0);
        Assert(i < count);

        i32 delta_count = count - i - 1;
        Assert(delta_count >= 0);

        if (delta_count > 0) {
            memmove(base + i, base + i + 1, sizeof(T) * delta_count);
        }

        count--;
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

    Allocator_function((*allocator_)) = nullptr;
    void* allocator_data_             = nullptr;

    i32 Index_Of(const T& value) {
        FOR_RANGE (i32, i, count) {
            auto& v = base[i];
            if (v == value)
                return i;
        }

        return -1;
    }

    // TODO: Подумать о целесообразности.
    i32 Index_Of(const T& value, std::invocable<const T&, const T&, bool&> auto&& func) {
        bool found = false;
        FOR_RANGE (i32, i, count) {
            func(value, base[i], found);
            if (found)
                return i;
        }

        return -1;
    }

    i32 Index_By_Ptr(const T* const value_ptr) {
        Assert(base <= value_ptr);
        Assert(value_ptr < base + count * sizeof(T));
        Assert((value_ptr - base) % sizeof(T) == 0);
        return (value_ptr - base) / sizeof(T);
    }

    T* Vector_Occupy_Slot(MCTX) {
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

            auto old_size = sizeof(T) * max_count;
            auto old_ptr  = base;

            base = rcast<T*>(ALLOC(old_size * 2));
            memcpy((void*)base, (void*)old_ptr, old_size);
            FREE(old_ptr, old_size);

            max_count = new_max_count;
        }

        auto result = base + count;
        count += 1;

        return result;
    }

    void Remove_At(i32 i) {
        Assert(i >= 0);
        Assert(i < count);

        i32 delta_count = count - i - 1;
        Assert(delta_count >= 0);

        if (delta_count > 0)
            memmove(base + i, base + i + 1, sizeof(T) * delta_count);

        count--;
    }

    void Unordered_Remove_At(const i32 i) {
        Assert(i >= 0);
        Assert(i < count);

        if (i != count - 1)
            base[i] = base[count - 1];

        count--;
    }

    T Pop() {
        Assert(count > 0);

        T result = base[count - 1];
        count--;

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
            memcpy((void*)base, (void*)old_base, sizeof(T) * elements_count);
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

    Allocator_function((*allocator_)) = nullptr;
    void* allocator_data_             = nullptr;

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

    void Free(MCTX) {
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

        if (base == nullptr || max_count == 0) {
            auto ceiled_size = Ceil_To_Power_Of_2(size);
            base             = (void*)ALLOC(ceiled_size);
            max_count        = ceiled_size;
        }
        else if (count + size > max_count) {
            auto ceiled_size = Ceil_To_Power_Of_2(size);
            base             = (void*)REALLOC(ceiled_size, max_count, base);
            max_count        = ceiled_size;
        }
    }
};

template <typename T>
struct Sparse_Array_Of_Ids {
    T*  ids       = nullptr;
    i32 count     = 0;
    i32 max_count = 0;

    T* Add(MCTX) {
        if (max_count == count)
            Enlarge(ctx);

        auto result = ids + count;
        count++;
        return result;
    }

    void Unstable_Remove(const T id) {
        FOR_RANGE (i32, i, count) {
            if (ids[i] == id) {
                if (i != count - 1)
                    std::swap(ids[i], ids[count - 1]);

                count--;
                return;
            }
        }
        Assert(false);
    }

    bool Contains(const T& id) {
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

        memcpy((void*)ids, (void*)old_ids_ptr, old_ids_size);
        FREE(old_ids_ptr, old_ids_size);

        max_count = new_max_count;
    }

    void Reset() {
        count = 0;
    }
};

template <typename T, typename U>
struct Sparse_Array {
    T*  ids       = nullptr;
    U*  base      = nullptr;
    i32 count     = 0;
    i32 max_count = 0;

    std::tuple<T*, U*> Add(MCTX) {
        Assert(count >= 0);
        Assert(max_count >= 0);

        if (max_count == count)
            Enlarge(ctx);

        std::tuple<T*, U*> result = {ids + count, base + count};
        count++;
        return result;
    }

    U* Find(const T id) {
        FOR_RANGE (i32, i, count) {
            if (ids[i] == id)
                return base + i;
        }
        return nullptr;
    }

    // TODO: Подумать о целесообразности.
    i32 Index_Of(const T& value, std::invocable<const T&, const T&, bool&> auto&& func) {
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
                    ids[i]  = ids[count - 1];
                    base[i] = base[count - 1];
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

        i32 new_max_count = max_count * 2;
        if (new_max_count == 0)
            new_max_count = 8;
        Assert(max_count < new_max_count);  // NOTE: Ловим overflow

        auto old_ids_size  = sizeof(T) * max_count;
        auto old_base_size = sizeof(U) * max_count;
        auto old_ids_ptr   = ids;
        auto old_base_ptr  = base;

        ids  = rcast<T*>(ALLOC(sizeof(T) * new_max_count));
        base = rcast<U*>(ALLOC(sizeof(U) * new_max_count));

        if (old_ids_ptr)
            memcpy(ids, old_ids_ptr, old_ids_size);
        if (old_base_ptr)
            memcpy(base, old_base_ptr, old_base_size);

        if (old_ids_ptr)
            FREE(old_ids_ptr, old_ids_size);
        if (old_base_ptr)
            FREE(old_base_ptr, old_base_size);

        max_count = new_max_count;
    }

    void Reset() {
        count = 0;
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

    [[nodiscard]] Sparse_Array_Iterator begin() const {
        return {_container, _current};
    }
    [[nodiscard]] Sparse_Array_Iterator end() const {
        return {_container, _container->count};
    }

    [[nodiscard]] std::tuple<T, U*> Dereference() const {
        Assert(_current >= 0);
        Assert(_current < _container->count);
        auto result = std::tuple(_container->ids[_current], _container->base + _current);
        return result;
    }

    void Increment() {
        _current++;
    }

    [[nodiscard]] bool Equal_To(const Sparse_Array_Iterator& o) const {
        return _current == o._current;
    }

private:
    Sparse_Array<T, U>* _container;
    i32                 _current = 0;
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

    [[nodiscard]] Vector_Iterator begin() const {
        return {_container, _current};
    }
    [[nodiscard]] Vector_Iterator end() const {
        return {_container, _container->count};
    }

    [[nodiscard]] T* Dereference() const {
        Assert(_current >= 0);
        Assert(_current < _container->count);
        return _container->base + _current;
    }

    void Increment() {
        _current++;
    }

    [[nodiscard]] bool Equal_To(const Vector_Iterator& o) const {
        return _current == o._current;
    }

private:
    Vector<T>* _container;
    i32        _current = 0;
};

template <typename T, typename U>
BF_FORCE_INLINE auto Iter(Sparse_Array<T, U>* container) {
    return Sparse_Array_Iterator(container);
}

template <typename T>
BF_FORCE_INLINE auto Iter(Vector<T>* container) {
    return Vector_Iterator(container);
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
