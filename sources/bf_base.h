#pragma once

#include <stdint.h>

#include "tracy/Tracy.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"
#include "glm/mat3x3.hpp"
#include "glm/vec2.hpp"

#define STATEMENT(statement) \
    do {                     \
        statement;           \
    } while (false)

#ifdef BF_INTERNAL
#    define BREAKPOINT STATEMENT({ __debugbreak(); })
#else  // BF_INTERNAL
#    define BREAKPOINT STATEMENT({})
#endif  // BF_INTERNAL

#ifdef BF_INTERNAL
static constexpr auto DEBUG_MAX_LEN = 512;

void DEBUG_Error(const char* text, ...) {
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    va_list args;
    va_start(args, text);
    char buf[DEBUG_MAX_LEN];
    vsnprintf(buf, DEBUG_MAX_LEN, text, args);

    ::OutputDebugStringA(buf);
    va_end(args);
}

void DEBUG_Print(const char* text, ...) {
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    va_list args;
    va_start(args, text);
    char buf[DEBUG_MAX_LEN];
    vsnprintf(buf, DEBUG_MAX_LEN, text, args);

    ::OutputDebugStringA(buf);
    va_end(args);
}

#else  // BF_INTERNAL
#    define DEBUG_Error(text_, ...)
#    define DEBUG_Print(text_, ...)
#endif  // BF_INTERNAL

// =============================================================
// INLINE
// =============================================================
// NOTE: Copied from `vendor/tracy/zstd/common/xxhash.h`
#if BF_NO_INLINE_HINTS /* disable inlining hints */
#    if defined(__GNUC__) || defined(__clang__)
#        define BF_FORCE_INLINE static __attribute__((unused))
#    else
#        define BF_FORCE_INLINE static
#    endif
#    define BF_NO_INLINE static
/* enable inlining hints */
#elif defined(__GNUC__) || defined(__clang__)
#    define BF_FORCE_INLINE static __inline__ __attribute__((always_inline, unused))
#    define BF_NO_INLINE static __attribute__((noinline))
#elif defined(_MSC_VER) /* Visual Studio */
#    define BF_FORCE_INLINE static __forceinline
#    define BF_NO_INLINE static __declspec(noinline)
#elif defined(__cplusplus) \
    || (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) /* C99 */
#    define BF_FORCE_INLINE static inline
#    define BF_NO_INLINE static
#else
#    define BF_FORCE_INLINE static
#    define BF_NO_INLINE static
#endif

// =============================================================
// OTHER SHIT
// =============================================================
using v2f   = glm::vec2;
using v2i   = glm::ivec2;
using v2u   = glm::uvec2;
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

using uint = unsigned int;
using u8   = uint8_t;
using i8   = int8_t;
using u16  = uint16_t;
using i16  = int16_t;
using u32  = uint32_t;
using i32  = int32_t;
using b32  = uint32_t;
using u64  = uint64_t;
using i64  = int64_t;
using f32  = float;
using f64  = double;

constexpr u8     u8_max     = std::numeric_limits<u8>::max();
constexpr u16    u16_max    = std::numeric_limits<u16>::max();
constexpr u32    u32_max    = std::numeric_limits<u32>::max();
constexpr u64    u64_max    = std::numeric_limits<u64>::max();
constexpr size_t size_t_max = std::numeric_limits<size_t>::max();

constexpr i8  i8_max  = std::numeric_limits<i8>::max();
constexpr i16 i16_max = std::numeric_limits<i16>::max();
constexpr i32 i32_max = std::numeric_limits<i32>::max();
constexpr i64 i64_max = std::numeric_limits<i64>::max();

constexpr i8  i8_min  = std::numeric_limits<i8>::min();
constexpr i16 i16_min = std::numeric_limits<i16>::min();
constexpr i32 i32_min = std::numeric_limits<i32>::min();
constexpr i64 i64_min = std::numeric_limits<i64>::min();

constexpr f32 f32_inf = std::numeric_limits<f32>::infinity();

#ifdef TESTS
#    define Assert(expr) REQUIRE(expr)
#    define Assert_False(expr) REQUIRE_FALSE(expr)
#else
#    include <cassert>
#    define Assert(expr) STATEMENT({ assert((bool)(expr)); })
#    define Assert_False(expr) STATEMENT({ assert(!(bool)(expr)); })
#endif

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
#    define Assert_Not_Null(value) (value)
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

#define FOR_RANGE(type, variable_name, max_value_exclusive)               \
    /* NOLINTBEGIN(bugprone-macro-parentheses) */                         \
    for (type variable_name = 0; (variable_name) < (max_value_exclusive); \
         variable_name++)                                                 \
    /* NOLINTEND(bugprone-macro-parentheses) */

//----------------------------------------------------------------------------------
// Defer.
//----------------------------------------------------------------------------------
template <typename F>
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct Defer_ {
    Defer_(F f)
        : f(f) {}
    ~Defer_() {
        f();
    }
    F f;
};

template <typename F>
Defer_<F> makeDefer_(F f) {
    return Defer_<F>(f);
};

#define defer_with_counter_(counter) defer_##counter
#define defer_(counter) defer_with_counter_(counter)

struct defer_dummy_ {};

template <typename F>
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
Defer_<F> operator+(defer_dummy_, F&& f) {
    return makeDefer_<F>(std::forward<F>(f));
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
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define defer auto defer_(__COUNTER__) = defer_dummy_() + [&]()

// =============================================================
// Other
// =============================================================
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

// Usage:
//
//     struct A : public Equatable<A> {
//         int field1;
//         int field2;
//
//         NOTE: Требуется реализовать этот метод
//         bool Equal_To(const A& v1, const A& v2) {
//             auto result = (
//                 v1.field1 == v2.field1
//                 && v1.field2 == v2.field2
//             );
//             return result;
//         }
//     }
template <typename Derived>
struct Equatable {
private:
    using Self_Type = Derived;

public:
    friend bool operator==(const Self_Type& v1, const Self_Type& v2) {
        return v1.Equal_To(v2);
    }
    // SHIT: Fuken `clang-tidy` requires this function to be specified
    friend bool operator!=(const Self_Type& v1, const Self_Type& v2) {
        return !v1.Equal_To(v2);
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
    [[nodiscard]] const Self_Type& _self() const {
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

    friend bool operator==(const Self_Type& v1, const Self_Type& v2) {
        return v1.Equal_To(v2);
    }
    // SHIT: Fuken `clang-tidy` requires this function to be specified
    friend bool operator!=(const Self_Type& v1, const Self_Type& v2) {
        return !v1.Equal_To(v2);
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
