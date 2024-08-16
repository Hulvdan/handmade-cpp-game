const char* Text_Format(const char* text, ...) {
#if !defined(BF_TEXTFORMAT_MAX_SIZE)
#    define BF_TEXTFORMAT_MAX_SIZE 1024
#endif

    static char buffer[BF_TEXTFORMAT_MAX_SIZE];

    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    va_list args;
    va_start(args, text);
    vsnprintf(buffer, BF_TEXTFORMAT_MAX_SIZE, text, args);
    va_end(args);

    buffer[BF_TEXTFORMAT_MAX_SIZE - 1] = '\0';

    return buffer;
}

const char* Text_Format2(const char* text, ...) {
#if !defined(BF_TEXTFORMAT_MAX_SIZE)
#    define BF_TEXTFORMAT_MAX_SIZE 1024
#endif

    static char buffer[BF_TEXTFORMAT_MAX_SIZE];

    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    va_list args;
    va_start(args, text);
    vsnprintf(buffer, BF_TEXTFORMAT_MAX_SIZE, text, args);
    va_end(args);

    buffer[BF_TEXTFORMAT_MAX_SIZE - 1] = '\0';

    return buffer;
}

char* Allocate_Formatted_String(Arena& arena, const char* format, ...) {
    const auto MAX_N = 512;
    char       buf[MAX_N];

    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
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
