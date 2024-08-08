// Returns the offset from `filedata` to a newline character (`\r` or `\n`).
// Returns STRINGS_EOF if reaches the end.
// Returns STRINGS_ZEROBYTE if finds `\0`.
const i32 STRINGS_EOF      = -1;
const i32 STRINGS_ZEROBYTE = -2;

i32 Find_Newline(const u8* buffer, const u32 size) {
    Assert(size > 0);

    i32 offset = 0;
    while (offset < size) {
        u8 character = *(buffer + offset);

        if (character == '\0')
            return STRINGS_ZEROBYTE;

        if (character == '\n' || character == '\r')
            return offset;

        offset++;
    }

    return STRINGS_EOF;
}

i32 Find_Newline_Or_EOF(const u8* buffer, const u32 size) {
    Assert(size > 0);

    i32 offset = 0;
    while (offset < size) {
        u8 character = *(buffer + offset);

        if (character == '\0' || character == '\n' || character == '\r')
            return offset;

        offset++;
    }

    return offset;
}

// Returns the offset from `filedata` to a newline character (not `\r` and not `\n`).
// Returns STRINGS_EOF if reaches the end.
// Returns STRINGS_ZEROBYTE if finds `\0`.
i32 Find_Not_Newline(const u8* buffer, const u32 size) {
    Assert(size > 0);

    i32 offset = 0;
    while (offset < size) {
        u8 character = *(buffer + offset);

        if (character == '\0')
            return STRINGS_ZEROBYTE;

        if (character != '\n' && character != '\r')
            return offset;

        offset++;
    }

    return STRINGS_EOF;
}

char* Allocate_Formatted_String(Arena& arena, const char* format, ...) {
    const auto MAX_N = 512;
    char       buf[MAX_N];

    va_list args;
    va_start(args, format);
    auto n = vsnprintf(buf, MAX_N, format, args);
    va_end(args);

    Assert(n >= 0);
    auto n_wo_zero = MIN(n, MAX_N - 1);

    auto allocated_string = Allocate_Array(arena, char, n_wo_zero + 1);
    memcpy(allocated_string, buf, n_wo_zero);
    *(allocated_string + n_wo_zero) = '\0';
    return allocated_string;
}
