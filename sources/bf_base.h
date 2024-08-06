#pragma once
#include "tracy/Tracy.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"
#include "glm/mat3x3.hpp"
#include "glm/vec2.hpp"

#ifdef BF_INTERNAL
#define BREAKPOINT __debugbreak()
#else  // BF_INTERNAL
#define BREAKPOINT
#endif  // BF_INTERNAL

#ifdef BF_INTERNAL
static constexpr auto DEBUG_MAX_LEN = 512;

void DEBUG_Error(const char* text, ...) {
    va_list args;
    va_start(args, text);
    char buf[DEBUG_MAX_LEN];
    vsnprintf(buf, DEBUG_MAX_LEN, text, args);

    ::OutputDebugStringA(buf);
    va_end(args);
}

void DEBUG_Print(const char* text, ...) {
    va_list args;
    va_start(args, text);
    char buf[DEBUG_MAX_LEN];
    vsnprintf(buf, DEBUG_MAX_LEN, text, args);

    ::OutputDebugStringA(buf);
    va_end(args);
}

#else  // BF_INTERNAL
#define DEBUG_Error(text_, ...)
#define DEBUG_Print(text_, ...)
#endif  // BF_INTERNAL

// =============================================================
// INLINE
// =============================================================
// NOTE: Copied from `vendor/tracy/zstd/common/xxhash.h`
#if BF_NO_INLINE_HINTS /* disable inlining hints */
#if defined(__GNUC__) || defined(__clang__)
#define BF_FORCE_INLINE static __attribute__((unused))
#else
#define BF_FORCE_INLINE static
#endif
#define BF_NO_INLINE static
/* enable inlining hints */
#elif defined(__GNUC__) || defined(__clang__)
#define BF_FORCE_INLINE static __inline__ __attribute__((always_inline, unused))
#define BF_NO_INLINE static __attribute__((noinline))
#elif defined(_MSC_VER) /* Visual Studio */
#define BF_FORCE_INLINE static __forceinline
#define BF_NO_INLINE static __declspec(noinline)
#elif defined(__cplusplus) \
    || (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) /* C99 */
#define BF_FORCE_INLINE static inline
#define BF_NO_INLINE static
#else
#define BF_FORCE_INLINE static
#define BF_NO_INLINE static
#endif

// =============================================================
// OTHER SHIT
// =============================================================
using v2f   = glm::vec2;
using v2i   = glm::ivec2;
using v2i16 = glm::vec<2, int16_t, glm::defaultp>;
using v3f   = glm::vec3;
using v3i   = glm::ivec3;

const v2i16 v2i16_adjacent_offsets[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
const v2i16 v2i16_adjacent_offsets_including_0[5]
    = {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}};

constexpr v2i v2i_zero   = v2i(0, 0);
constexpr v2i v2i_one    = v2i(1, 1);
constexpr v2i v2i_right  = v2i(1, 0);
constexpr v2i v2i_up     = v2i(0, 1);
constexpr v2i v2i_left   = v2i(-1, 0);
constexpr v2i v2i_bottom = v2i(0, -1);

constexpr v2i16 v2i16_zero   = v2i16(0, 0);
constexpr v2i16 v2i16_one    = v2i16(1, 1);
constexpr v2i16 v2i16_right  = v2i16(1, 0);
constexpr v2i16 v2i16_up     = v2i16(0, 1);
constexpr v2i16 v2i16_left   = v2i16(-1, 0);
constexpr v2i16 v2i16_bottom = v2i16(0, -1);

constexpr v2f v2f_zero   = v2f(0, 0);
constexpr v2f v2f_one    = v2f(1, 1);
constexpr v2f v2f_right  = v2f(1, 0);
constexpr v2f v2f_up     = v2f(0, 1);
constexpr v2f v2f_left   = v2f(-1, 0);
constexpr v2f v2f_bottom = v2f(0, -1);

constexpr v3i v3i_zero = v3i(0, 0, 0);
constexpr v3i v3i_one  = v3i(1, 1, 1);

constexpr v3f v3f_zero = v3f(0, 0, 0);
constexpr v3f v3f_one  = v3f(1, 1, 1);

#define local_persist static
#define global_var static

#include "bf_types.h"

#ifdef TESTS
#define Assert(expr) REQUIRE(expr)
#define Assert_False(expr) REQUIRE_FALSE(expr)
#else  // TESTS
#include <cassert>
#define Assert(expr) assert(expr)
#define Assert_False(expr) assert(!((bool)(expr)))
#endif  // TESTS

template <typename T>
BF_FORCE_INLINE T& Assert_Deref(T* value) {
    Assert(value != nullptr);
    return *value;
}

#if 1
template <typename T>
BF_FORCE_INLINE T* Assert_Not_Null(T* value) {
    Assert(value != nullptr);
    return value;
}
#else
#define Assert_Not_Null(value) value
#endif

#define INVALID_PATH Assert(false)
#define NOT_IMPLEMENTED Assert(false)
#define NOT_SUPPORTED Assert(false)

#define scast static_cast
#define rcast reinterpret_cast

static constexpr f32 BF_PI  = 3.14159265359f;
static constexpr f32 BF_2PI = 6.28318530718f;
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// NOTE: bounds are EXCLUSIVE
#define Pos_Is_In_Bounds(pos, bounds) \
    (!((pos).x < 0 || (pos).x >= (bounds).x || (pos).y < 0 || (pos).y >= (bounds).y))

#define FOR_RANGE(type, variable_name, max_value_exclusive) \
    for (type variable_name = 0; (variable_name) < (max_value_exclusive); variable_name++)

// =============================================================
// Defer
// =============================================================
template <typename F>
struct _Defer {
    _Defer(F f)
        : f(f) {}
    ~_Defer() {
        f();
    }
    F f;
};

template <typename F>
BF_FORCE_INLINE _Defer<F> _makeDefer(F f) {
    return _Defer<F>(f);
};

#define __defer(counter) defer_##counter
#define _defer(counter) __defer(counter)

struct _defer_dummy {};
template <typename F>
BF_FORCE_INLINE _Defer<F> operator+(_defer_dummy, F&& f) {
    return _makeDefer<F>(std::forward<F>(f));
}

// Usage:
//     {
//         defer { printf("Deferred\n"); };
//         printf("Normal\n");
//     }
//
// Output:
//     Normal
//     Deferred
//
#define defer auto _defer(__COUNTER__) = _defer_dummy() + [&]()

// =============================================================
// Other
// =============================================================
struct Non_Copyable {
    Non_Copyable()                               = default;
    Non_Copyable(const Non_Copyable&)            = delete;
    Non_Copyable& operator=(const Non_Copyable&) = delete;
};

template <typename T>
BF_FORCE_INLINE void Initialize_As_Zeros(T& value) {
    memset(&value, 0, sizeof(T));
}

// =============================================================
// Iterators
// =============================================================
//
// NOTE: Proudly taken from
// https://vector-of-bool.github.io/2020/06/13/cpp20-iter-facade.html
template <class Reference>
struct Arrow_Proxy {
    Reference  r;
    Reference* operator->() {
        return &r;
    }
};

template <typename Derived>
struct Equatable {
protected:
    using Self_Type = Derived;

public:
    friend bool operator==(const Self_Type& left, const Self_Type& right) {
        return left.Equal_To(right);
    }
    // SHIT: Fuken `clang-tidy` requires this function to be specified
    friend bool operator!=(const Self_Type& left, const Self_Type& right) {
        return !left.Equal_To(right);
    }
};

template <typename Derived>
struct Iterator_Facade {
protected:
    using Self_Type = Derived;

private:
    Self_Type& _self() {
        return scast<Self_Type&>(*this);
    }
    const Self_Type& _self() const {
        return scast<const Self_Type&>(*this);
    }

public:
    decltype(auto) operator*() const {
        return _self().Dereference();
    }

    auto operator->() const {
        decltype(auto) ref = **this;
        if constexpr (std::is_reference_v<decltype(ref)>)
            return std::addressof(ref);
        else
            return Arrow_Proxy<Derived>(std::move(ref));
    }

    friend bool operator==(const Self_Type& left, const Self_Type& right) {
        return left.Equal_To(right);
    }
    // SHIT: Fuken `clang-tidy` requires this function to be specified
    friend bool operator!=(const Self_Type& left, const Self_Type& right) {
        return !left.Equal_To(right);
    }

    Self_Type& operator++() {
        _self().Increment();
        return _self();
    }

    Self_Type operator++(int) {
        auto copy = _self();
        ++*this;
        return copy;
    }
};
