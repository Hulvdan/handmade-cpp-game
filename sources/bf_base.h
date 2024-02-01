#pragma once
#include "glm/gtx/matrix_transform_2d.hpp"
#include "glm/mat3x3.hpp"
#include "glm/vec2.hpp"

using v2f = glm::vec2;
using v2i = glm::ivec2;

#include "bf_types.h"

#define scast static_cast

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
_privDefer<F> _defer_func(F f)
{
    return _privDefer<F>(f);
}
#define _DEFER_1(x, y) x##y
#define _DEFER_2(x, y) _DEFER_1(x, y)
#define _DEFER_3(x) _DEFER_2(x, __COUNTER__)
#define DEFER(code) auto _DEFER_3(_defer_) = _defer_func([&]() { code; })
