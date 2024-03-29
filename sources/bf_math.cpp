#define Ceil_Division(value, divisor) ((value) / (divisor) + ((value) % (divisor) != 0))

#define Lerp(a, b, t) ((a) * (1 - (t)) + (b) * (t))

u16 Assert_Truncate_To_u16(size_t value) {
    Assert(value <= u16_max);
    return (u16)value;
}

f32 Move_Towards(f32 value, f32 target, f32 diff) {
    Assert(diff >= 0);
    auto d = target - value;
    if (d > 0)
        value += MIN(d, diff);
    else if (d < 0)
        value -= MAX(d, diff);
    return value;
}

#define QUERY_BIT(bytes_ptr, bit_index) \
    ((*((u8*)(bytes_ptr) + ((bit_index) / 8))) & (1 << ((bit_index) % 8)))

#define MARK_BIT(bytes_ptr, bit_index)                      \
    {                                                       \
        u8& byte = *((u8*)(bytes_ptr) + ((bit_index) / 8)); \
        byte = byte | (1 << ((bit_index) % 8));             \
    }

#define UNMARK_BIT(bytes_ptr, bit_index)                    \
    {                                                       \
        u8& byte = *((u8*)(bytes_ptr) + ((bit_index) / 8)); \
        byte &= 0xFF - (1 << ((bit_index) % 8));            \
    }

// Usage Examples:
//     32 -> 32
//     26 -> 32
//     13 -> 16
//     8 -> 8
//     0 -> ASSERT
//     2147483648 and above -> ASSERT
u32 Ceil_To_Power_Of_2(u32 value) {
    Assert(value <= 2147483648);
    Assert(value != 0);

    u32 power = 1;
    while (power < value)
        power *= 2;

    return power;
}

//
// NOTE:
// Подсчёт самого длинного пути без циклов,
// который может быть выдан при расчёте кратчайшего пути.
//
// В геометрическом смысле его длина
// равна количеству клеток с буквой R - 20.
//
//     R.RRR.RRR
//     R.R.R.R.R
//     R.R.R.R.R
//     RRR.RRR.R
//
u32 Longest_Meaningful_Path(v2i16 gsize) {
    i32 a = MIN(gsize.x, gsize.y);
    i32 b = MAX(gsize.x, gsize.y);

    i32 v1 = a / 2 + Ceil_Division(a, 2) * b;
    i32 v2 = b / 2 + Ceil_Division(b, 2) * a;

    i32 result = MAX(v1, v2);
    return result;
}
