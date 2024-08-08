#define Ceiled_Division(value, divisor) ((value) / (divisor) + ((value) % (divisor) != 0))

#define Lerp(a, b, t) ((a) * (1 - (t)) + (b) * (t))

template <typename T>
T Abs(T value) {
    return std::abs(value);
}

int Ceil(f32 value) {
    return std::ceil(value);
}

#define X(ret, in)                           \
    ret Assert_Truncate_To_##ret(in value) { \
        Assert(value >= 0);                  \
        Assert(value <= (in)ret##_max);      \
        auto result = (ret)value;            \
        return result;                       \
    }

X(u32, u64);
X(u32, i64);
X(u16, u64);
X(u16, i64);
X(u16, u32);
X(u16, i32);
X(u8, u64);
X(u8, i64);
X(u8, u32);
X(u8, i32);
X(u8, u16);
X(u8, i16);

#undef X

#define X(ret, in)                           \
    ret Assert_Truncate_To_##ret(in value) { \
        Assert(value >= (in)ret##_min);      \
        Assert(value <= (in)ret##_max);      \
        auto result = (ret)value;            \
        return result;                       \
    }

X(i32, u64);
X(i32, i64);
X(i32, u32);
X(i16, u64);
X(i16, i64);
X(i16, u32);
X(i16, i32);
X(i16, u16);
X(i8, u64);
X(i8, i64);
X(i8, u32);
X(i8, i32);
X(i8, u16);
X(i8, i16);
X(i8, u8);

#undef X

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
    STATEMENT({                                             \
        u8& byte = *((u8*)(bytes_ptr) + ((bit_index) / 8)); \
        byte     = byte | (1 << ((bit_index) % 8));         \
    })

#define UNMARK_BIT(bytes_ptr, bit_index)                    \
    STATEMENT({                                             \
        u8& byte = *((u8*)(bytes_ptr) + ((bit_index) / 8)); \
        byte &= 0xFF - (1 << ((bit_index) % 8));            \
    })

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

// Проверка на то, является ли число степенью двойки.
// При передаче указателя на power, там окажется значение этой степени.
bool Is_Power_Of_2(int number, u32* power = nullptr) {
    if (number < 2)
        return false;

    if (power)
        *power = 0;

    while (number > 1) {
        if (number & 1)
            return false;

        number >>= 1;
        if (power)
            *power += 1;
    }

    return true;
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
u32 Longest_Meaningful_Path(v2i16 size) {
    i32 a = MIN(size.x, size.y);
    i32 b = MAX(size.x, size.y);

    i32 v1 = a / 2 + Ceiled_Division(a, 2) * b;
    i32 v2 = b / 2 + Ceiled_Division(b, 2) * a;

    i32 result = MAX(v1, v2);
    return result;
}
