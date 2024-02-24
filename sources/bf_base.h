#pragma once
#include "tracy/Tracy.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"
#include "glm/mat3x3.hpp"
#include "glm/vec2.hpp"

#ifdef BF_INTERNAL
#define BREAKPOINT __debugbreak()
#else
#define BREAKPOINT
#endif

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

#else
#define DEBUG_Error(text_, ...)
#define DEBUG_Print(text_, ...)
#endif

using v2f = glm::vec2;
using v2i = glm::ivec2;
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

#define UNREACHABLE assert(false)

#define scast static_cast
#define rcast reinterpret_cast

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define Lerp(a, b, t) ((a) * (1 - (t)) + (b) * (t))

#define FOR_RANGE(type, variable_name, max_value_exclusive) \
    for (type variable_name = 0; (variable_name) < (max_value_exclusive); variable_name++)

// NOTE(hulvdan): bounds are EXCLUSIVE
#define Pos_Is_In_Bounds(pos, bounds) \
    (!((pos).x < 0 || (pos).x >= (bounds).x || (pos).y < 0 || (pos).y >= (bounds).y))

static constexpr f32 BF_PI = 3.14159265359f;
static constexpr f32 BF_2PI = 6.28318530718f;

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

struct Non_Copyable {
    Non_Copyable() = default;
    Non_Copyable(const Non_Copyable&) = delete;
    Non_Copyable& operator=(const Non_Copyable&) = delete;
};

template <typename T>
void Initialize_As_Zeros(T& value) {
    memset(&value, 0, sizeof(T));
}
