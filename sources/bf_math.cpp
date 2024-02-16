#define Ceil_Division(value, divisor) ((value) / (divisor) + ((value) % (divisor) != 0))

u16 Assert_Truncate_To_u16(size_t value) {
    assert(value <= u16_max);
    return (u16)value;
}
