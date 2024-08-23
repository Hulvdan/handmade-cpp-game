// Usage:
//
//     void function(Human& human, MCTX) {
//         CTX_LOGGER;
//         LOG_SCOPE;
//         LOG_DEBUG("human.moving %d.%d", human.moving.pos.x, human.moving.pos.y);
//     }

// TODO: ВЫЧИСТИТЬ ЭТОТ СРАЧ
// Набор функций для отбрасывания абсолютного пути файла. Оставляем только название.
consteval const char* str_end(const char* str) {
    return *str ? str_end(str + 1) : str;
}

consteval bool str_slant(const char* str) {
    return *str == '/' ? true : (*str ? str_slant(str + 1) : false);
}

consteval const char* r_slant(const char* str) {
    return *str == '/' ? (str + 1) : r_slant(str - 1);
}
consteval const char* unix_file_name(const char* str) {
    return str_slant(str) ? r_slant(str_end(str)) : str;
}

consteval const char* str_end2(const char* str) {
    return *str ? str_end2(str + 1) : str;
}

consteval bool str_slant2(const char* str) {
    return *str == '\\' ? true : (*str ? str_slant2(str + 1) : false);
}

consteval const char* r_slant2(const char* str) {
    return *str == '\\' ? (str + 1) : r_slant2(str - 1);
}
consteval const char* windows_file_name(const char* str) {
    return str_slant2(str) ? r_slant2(str_end2(str)) : str;
}

consteval const char* extract_file_name(const char* str) {
    return windows_file_name(unix_file_name(str));
}

// Возвращает индекс символа открывающей скобки в названии функции.
consteval int index_of_brace_in_function_name(const char* str) {
    int n = 0;
    while (*str != '(' && *str != '\0') {
        n++;
        str++;
    }
    return n;
}

consteval int string_length(const char* str) {
    int n = 0;
    while (*str)
        n++;
    return n;
}

struct Tracing_Logger {
    static constexpr int MAX_BUFFER_SIZE = 4096;

    int current_indentation = {};
    int collapse_number     = {};

    char* previous_buffer      = {};
    int   previous_indentation = {};

    char* rendering_buffer = {};
    char* trash_buffer     = {};
    char* trash_buffer2    = {};

    bool initialized = {};

    void* file = {};
};

void Initialize_Tracing_Logger(Tracing_Logger& logger, Arena& arena) {
    Assert(!logger.initialized);
    logger.previous_buffer
        = Allocate_Zeros_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);
    logger.rendering_buffer
        = Allocate_Zeros_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);
    logger.trash_buffer
        = Allocate_Zeros_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);
    logger.trash_buffer2
        = Allocate_Zeros_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);
    logger.initialized = true;
}

BF_FORCE_INLINE void Assert_String_Copy_With_Indentation(
    char*             destination_buffer,
    const char* const message,
    const int         max_dest_size,
    const int         indentation
) {
    Assert(indentation >= 0);
    Assert(max_dest_size >= 0);

    // NOTE: Подсчёт длины сообщения с учётом 0 символа
    int n = 1;
    {
        auto m = message;
        while (*m != '\0') {
            m++;
            n++;
        }
        Assert(indentation + n <= max_dest_size);
    }

    // NOTE: Прописываем таблуляцию и сообщение в буфер
    memset(destination_buffer, '\t', indentation);
    memcpy(destination_buffer + indentation, message, n);
}

BF_FORCE_INLINE void common_log(
    Tracing_Logger&                    data,
    const char*                        message,
    std::invocable<const char*> auto&& log_function
) {
    if (data.current_indentation == 0) {
        log_function(message);
        return;
    }

    Assert_String_Copy_With_Indentation(
        data.trash_buffer,
        message,
        Tracing_Logger::MAX_BUFFER_SIZE,
        data.current_indentation
    );
    log_function(data.trash_buffer);
}

void Log_To_File(void** file, const char* text) {
    if (*file == nullptr)
        *file = global_library_integration_data->Open_File("latest.log");

    global_library_integration_data->Write_To_File(*file, text);
}

Logger_function(Tracing_Logger_Routine) {
    auto& data = *(Tracing_Logger*)logger_data;

    // NOTE: Засчитываем сообщение в collapse, если оно идентично предыдущему
    if (strcmp(data.previous_buffer, message) == 0) {
        data.collapse_number++;
        return;
    }

    // NOTE: Логирование collapse секции
    if (data.collapse_number > 0) {
        snprintf(
            data.trash_buffer,
            Tracing_Logger::MAX_BUFFER_SIZE,
            "<-- %d more -->",
            data.collapse_number
        );

        data.collapse_number = 0;

        // NOTE: Отбиваем сообщение табами, если нужно
        auto buffer_to_log = data.trash_buffer;
        if (data.previous_indentation > 0) {
            buffer_to_log = data.trash_buffer2;
            Assert_String_Copy_With_Indentation(
                buffer_to_log,
                data.trash_buffer,
                Tracing_Logger::MAX_BUFFER_SIZE,
                data.previous_indentation
            );
        }

        DEBUG_Print(buffer_to_log);
        Log_To_File(&data.file, buffer_to_log);
    }

    // NOTE: Запоминаем сообщение в качестве предыдущего
    int n = 1;
    {
        auto m = message;
        while (*m != '\0') {
            n++;
            m++;
        }
    }

    Assert(n <= Tracing_Logger::MAX_BUFFER_SIZE);
    memcpy(data.previous_buffer, (u8*)message, n);
    data.previous_indentation = data.current_indentation;

    switch (log_type) {
    case Log_Type::DEBUG:
        common_log(data, message, [&data](const char* message) {
            DEBUG_Print("D:\t%s", message);
            Log_To_File(&data.file, Text_Format2("D:\t%s", message));
        });
        break;
    case Log_Type::INFO:
        common_log(data, message, [&data](const char* message) {
            DEBUG_Print("I:\t%s", message);
            Log_To_File(&data.file, Text_Format2("I:\t%s", message));
        });
        break;
    case Log_Type::WARN:
        common_log(data, message, [&data](const char* message) {
            DEBUG_Print("W:\t%s", message);
            Log_To_File(&data.file, Text_Format2("W:\t%s", message));
        });
        break;
    case Log_Type::ERR:
        common_log(data, message, [&data](const char* message) {
            DEBUG_Print("E:\t%s", message);
            Log_To_File(&data.file, Text_Format2("E:\t%s", message));
        });
        break;
    default:
        INVALID_PATH;
    }
}

bool operator==(const std::source_location& a, const std::source_location& b) {
    return (
        a.line() == b.line()               //
        && a.column() == b.column()        //
        && a.file_name() == b.file_name()  //
        && a.function_name() == b.function_name()
    );
}

Logger_Scope_function(Tracing_Logger_Scope_Routine) {
    Assert(logger_data != nullptr);
    auto  logger_routine = Tracing_Logger_Routine;
    auto& data           = *(Tracing_Logger*)logger_data;

    if (!push) {
        data.current_indentation--;
        return;
    }

    logger_routine(logger_data, Log_Type::INFO, location);
    data.current_indentation++;
}

template <auto N>
consteval auto extract_substring(const char* src) {
    std::array<char, N + 1> res{};
    std::copy_n(src, N, res.data());
    res.data()[N] = '\0';
    return res;
}

#define LOG_SCOPE                                                                         \
    if (logger_scope_routine != nullptr) {                                                \
        constexpr const auto  loc = std::source_location::current();                      \
        constexpr const auto  n   = index_of_brace_in_function_name(loc.function_name()); \
        constexpr const auto  function_name = extract_substring<n>(loc.function_name());  \
        constexpr const char* file_n        = extract_file_name(loc.file_name());         \
                                                                                          \
        const char* scope_name = Text_Format(                                             \
            "[%s:%d:%d:%s]", file_n, loc.line(), loc.column(), function_name.data()       \
        );                                                                                \
                                                                                          \
        auto scope_routine = ((Logger_Scope_function_t)logger_scope_routine);             \
        scope_routine(logger_data, true, scope_name);                                     \
    }                                                                                     \
                                                                                          \
    defer {                                                                               \
        if (logger_scope_routine != nullptr) {                                            \
            auto scope_routine = ((Logger_Scope_function_t)logger_scope_routine);         \
            scope_routine(logger_data, false, nullptr);                                   \
        }                                                                                 \
    };

#if 1
#    define SCOPED_LOG_INIT(mes)                           \
        LOG_INFO(Text_Format("%s...", mes));               \
        defer {                                            \
            LOG_DEBUG(Text_Format("%s... Success!", mes)); \
        };
#else
#    define SCOPED_LOG_INIT(mes) ((void*)0)
#endif
