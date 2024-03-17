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

using v2f = glm::vec2;
using v2i = glm::ivec2;

const v2i v2i_adjacent_offsets[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

constexpr v2i v2i_zero = v2i(0, 0);
constexpr v2i v2i_one = v2i(1, 1);
constexpr v2i v2i_right = v2i(1, 0);
constexpr v2i v2i_up = v2i(0, 1);
constexpr v2i v2i_left = v2i(-1, 0);
constexpr v2i v2i_bottom = v2i(0, -1);

constexpr v2f v2f_zero = v2f(0, 0);
constexpr v2f v2f_one = v2f(1, 1);
constexpr v2f v2f_right = v2f(1, 0);
constexpr v2f v2f_up = v2f(0, 1);
constexpr v2f v2f_left = v2f(-1, 0);
constexpr v2f v2f_bottom = v2f(0, -1);

using v3f = glm::vec3;
using v3i = glm::ivec3;

#define local_persist static
#define global static

#include "bf_types.h"

#ifdef TESTS
#include <cassert>
#define Shit_Assert(expr) assert(expr)
#define Assert(expr) REQUIRE(expr)
#else  // TESTS
#include <cassert>
#define Assert(expr) assert(expr)
#define Shit_Assert(expr) Assert(expr)
#endif  // TESTS

#define INVALID_PATH Assert(false)
#define NOT_IMPLEMENTED Assert(false)

#define scast static_cast
#define rcast reinterpret_cast

static constexpr f32 BF_PI = 3.14159265359f;
static constexpr f32 BF_2PI = 6.28318530718f;
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// NOTE(hulvdan): bounds are EXCLUSIVE
#define Pos_Is_In_Bounds(pos, bounds) \
    (!((pos).x < 0 || (pos).x >= (bounds).x || (pos).y < 0 || (pos).y >= (bounds).y))

#define FOR_RANGE(type, variable_name, max_value_exclusive) \
    for (type variable_name = 0; (variable_name) < (max_value_exclusive); variable_name++)

// ============================================================= //
//                             Defer                             //
// ============================================================= //
template <typename F>
struct _privDefer {
    F f;
    _privDefer(F f) : f(f) {}
    ~_privDefer() { f(); }
};
template <typename F>
_privDefer<F> _defer_func(F f) {
    return _privDefer<F>(f);
}
#define _DEFER_1(x, y) x##y
#define _DEFER_2(x, y) _DEFER_1(x, y)
#define _DEFER_3(x) _DEFER_2(x, __COUNTER__)
#define DEFER(code) auto _DEFER_3(_defer_) = _defer_func([&]() { code; })

// ============================================================= //
//                             Other                             //
// ============================================================= //
struct Non_Copyable {
    Non_Copyable() = default;
    Non_Copyable(const Non_Copyable&) = delete;
    Non_Copyable& operator=(const Non_Copyable&) = delete;
};

template <typename T>
void Initialize_As_Zeros(T& value) {
    memset(&value, 0, sizeof(T));
}

// ============================================================= //
//                         Guarded While                         //
// ============================================================= //
#define BF_CAT_(a, b) a##b
#define BF_CAT(a, b) BF_CAT_(a, b)
#define BF_VARNAME(Var) BF_CAT(Var, __LINE__)

// NOTE(hulvdan): Сие есть мой способ написания цикла while,
// который грохается при разработке, если есть подозрение на бесконечный цикл.
//
// Тело цикла исполняется максимум `MIN(count_, 65534)` раз.
//
// Usage:
//
// 1) Assertion error
// guarded_while(16) {
//     ... infinite loop
// }
//
// 2) No assertion error
// guarded_while(16) {
//     break;
// }
//
// TODO(hulvdan): Shit_Assert вызывает assert из <cassert>.
// Придумать способ вызова Doctest-ового assert-а вместо этого. А этот удалить
#define guarded_while(count_)                                                         \
    for (u16 BF_VARNAME(guard_) = 0; BF_VARNAME(guard_) < u16_max;                    \
         Shit_Assert((count_) < u16_max), Shit_Assert(BF_VARNAME(guard_) < (count_)), \
             BF_VARNAME(guard_)++)

// ============================================================= //
//                           Iterators                           //
// ============================================================= //
//
// NOTE(hulvdan): Proudly taken from
// https://vector-of-bool.github.io/2020/06/13/cpp20-iter-facade.html
template <class Reference>
struct Arrow_Proxy {
    Reference r;
    Reference* operator->() { return &r; }
};

template <typename Derived>
struct Iterator_Facade {
protected:
    using self_type = Derived;

private:
    self_type& _self() { return scast<self_type&>(*this); }
    const self_type& _self() const { return scast<const self_type&>(*this); }

public:
    decltype(auto) operator*() const { return _self().dereference(); }

    auto operator->() const {
        decltype(auto) ref = **this;
        if constexpr (std::is_reference_v<decltype(ref)>)
            return std::addressof(ref);
        else
            return Arrow_Proxy<Derived>(std::move(ref));
    }

    friend bool operator==(const self_type& left, const self_type& right) {
        return left.equal_to(right);
    }
    // SHIT(hulvdan): Fuken `clang-tidy` requires this function to be specified
    friend bool operator!=(const self_type& left, const self_type& right) {
        return !left.equal_to(right);
    }

    self_type& operator++() {
        _self().increment();
        return _self();
    }

    self_type operator++(int) {
        auto copy = _self();
        ++*this;
        return copy;
    }
};
