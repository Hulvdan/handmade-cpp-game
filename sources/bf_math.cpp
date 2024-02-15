#include "math.h"

i32 Ceil_To_i32(f32 value) {
    return (i32)ceilf(value);
}

u16 Assert_Truncate_To_u16(size_t value) {
    assert(value <= u16_max);
    return (u16)value;
}
