#define Ceil_Division(value, divisor) ((value) / (divisor) + ((value) % (divisor) != 0))

u16 Assert_Truncate_To_u16(size_t value) {
    assert(value <= u16_max);
    return (u16)value;
}

f32 Move_Towards(f32 value, f32 target, f32 diff) {
    assert(diff >= 0);
    auto d = target - value;
    if (d > 0)
        value += MIN(d, diff);
    else if (d < 0)
        value -= MAX(d, diff);
    return value;
}
