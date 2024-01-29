#pragma once
#include "glm/gtx/matrix_transform_2d.hpp"
#include "glm/mat3x3.hpp"
#include "glm/vec2.hpp"

using v2f = glm::vec2;
using v2i = glm::ivec2;

// NOTE(hulvdan): Size is EXCLUSIVE
#define PosIsInBounds(pos, bounds) \
    (!((pos).x < 0 || (pos).x >= (bounds).x || (pos).y < 0 || (pos).y >= (bounds).y))

#include "bf_types.h"

#define FOR_RANGE(type, variable_name, max_value_exclusive) \
    for (type variable_name = 0; (variable_name) < (max_value_exclusive); variable_name++)

static constexpr f32 BF_PI = 3.14159265359f;
static constexpr f32 BF_2PI = 6.28318530718f;
