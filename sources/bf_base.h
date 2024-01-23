#pragma once
#include "glm/vec2.hpp"

using v2 = glm::vec2;
using v2i = glm::ivec2;

// NOTE(hulvdan): Size is EXCLUSIVE
#define PosIsInBounds(pos, bounds) \
    (!((pos).x < 0 || (pos).x >= (bounds).x || (pos).y < 0 || (pos).y >= (bounds).y))

#include "bf_types.h"
