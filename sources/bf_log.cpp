#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/sinks/sink.h"

// TODO: ВЫЧИСТИТЬ ЭТОТ СРАЧ
// Набор функций для отбрасывания абсолютного пути файла. Оставляем только название.
consteval const char* str_end(const char* str) { return *str ? str_end(str + 1) : str; }

consteval bool str_slant(const char* str) {
    return *str == '/' ? true : (*str ? str_slant(str + 1) : false);
}

consteval const char* r_slant(const char* str) {
    return *str == '/' ? (str + 1) : r_slant(str - 1);
}
consteval const char* unix_file_name(const char* str) {
    return str_slant(str) ? r_slant(str_end(str)) : str;
}

consteval const char* str_end2(const char* str) { return *str ? str_end2(str + 1) : str; }

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

// NOTE: index_of_brace_in_function_name возвращает
// индекс символа открывающей скобки в названии функции.
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

// Sink, логи которого видны в консоли Visual Studio.
class Sink : public spdlog::sinks::sink {
public:
    Sink(spdlog::color_mode mode = spdlog::color_mode::automatic) {
#ifdef _WIN32
        handle_                         = ::GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD console_mode              = 0;
        in_console_                     = ::GetConsoleMode(handle_, &console_mode) != 0;
        CONSOLE_SCREEN_BUFFER_INFO info = {};
        ::GetConsoleScreenBufferInfo(handle_, &info);
        colors_[spdlog::level::trace] = FOREGROUND_INTENSITY;
        colors_[spdlog::level::debug]
            = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        colors_[spdlog::level::info] = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
        colors_[spdlog::level::warn]
            = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN;
        colors_[spdlog::level::err] = FOREGROUND_INTENSITY | FOREGROUND_RED;
        colors_[spdlog::level::critical]
            = FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE;
        colors_[spdlog::level::off] = info.wAttributes;
#endif
        set_color_mode(mode);
    }

    Sink(const Sink& other)            = delete;
    Sink& operator=(const Sink& other) = delete;

    ~Sink() override = default;

    void log(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        std::lock_guard      lock{mutex_};
        formatter_->format(msg, formatted);
        if (should_color_ && msg.color_range_end > msg.color_range_start) {
            print_range(formatted, 0, msg.color_range_start);
            print_range(formatted, msg.color_range_start, msg.color_range_end, msg.level);
            print_range(formatted, msg.color_range_end, formatted.size());
        }
        else {
            print_range(formatted, 0, formatted.size());
        }
        fflush(stdout);

        auto s   = formatted.size();
        auto d   = formatted.data();
        d[s - 2] = '\0';
        d[s - 3] = '\n';
        DEBUG_Print(formatted.data());
    }

    void set_pattern(const std::string& pattern) final {
        std::lock_guard lock{mutex_};
        formatter_ = std::make_unique<spdlog::pattern_formatter>(pattern);
    }

    void set_formatter(std::unique_ptr<spdlog::formatter> formatter) override {
        std::lock_guard lock{mutex_};
        formatter_ = std::move(formatter);
    }

    void flush() override {
        std::lock_guard lock{mutex_};
        fflush(stdout);
    }

    void set_color_mode(spdlog::color_mode mode) {
        std::lock_guard lock{mutex_};
        switch (mode) {
        case spdlog::color_mode::always:
            should_color_ = true;
            break;
        case spdlog::color_mode::automatic:
#if _WIN32
            should_color_
                = spdlog::details::os::in_terminal(stdout) || IsDebuggerPresent();
#else
            should_color_ = spdlog::details::os::in_terminal(stdout)
                            && spdlog::details::os::is_color_terminal();
#endif
            break;
        case spdlog::color_mode::never:
            should_color_ = false;
            break;
        }
    }

private:
    void print_range(const spdlog::memory_buf_t& formatted, size_t start, size_t end) {
#ifdef _WIN32
        if (in_console_) {
            auto data = formatted.data() + start;
            auto size = static_cast<DWORD>(end - start);
            while (size > 0) {
                DWORD written = 0;
                if (!::WriteFile(handle_, data, size, &written, nullptr) || written == 0
                    || written > size) {
                    SPDLOG_THROW(spdlog::spdlog_ex(
                        "sink: print_range failed. GetLastError(): "
                        + std::to_string(::GetLastError())
                    ));
                }
                size -= written;
            }
            return;
        }
#endif
        fwrite(formatted.data() + start, sizeof(char), end - start, stdout);
    }

    void print_range(
        const spdlog::memory_buf_t& formatted,
        size_t                      start,
        size_t                      end,
        spdlog::level::level_enum   level
    ) {
#ifdef _WIN32
        if (in_console_) {
            ::SetConsoleTextAttribute(handle_, colors_[level]);
            print_range(formatted, start, end);
            ::SetConsoleTextAttribute(handle_, colors_[spdlog::level::off]);
            return;
        }
#endif
    }

    std::mutex                         mutex_;
    std::unique_ptr<spdlog::formatter> formatter_;
#ifdef _WIN32
    std::array<DWORD, 7> colors_;
    bool                 in_console_ = true;
    HANDLE               handle_     = nullptr;
#endif
    bool should_color_ = false;
};

enum class Log_Type { DEBUG = 0, INFO, WARN, ERR };

#define Logger__Function(name_) \
    void name_(void* logger_data, Log_Type log_type, const char* message)
#define Logger_Tracing__Function(name_) \
    void name_(void* logger_data, bool push, const char* location)

using Logger_Function_Type         = Logger__Function((*));
using Logger_Tracing_Function_Type = Logger_Tracing__Function((*));

struct Tracing_Logger {
    static constexpr int MAX_BUFFER_SIZE = 4096;

    int current_indentation;
    int collapse_number;

    char* previous_buffer;
    int   previous_indentation;

    spdlog::logger spdlog_logger;
    Sink           sink;

    char* rendering_buffer;
    char* trash_buffer;
    char* trash_buffer2;

    Tracing_Logger(Arena& arena)
        : current_indentation(0)
        , collapse_number(0)
        , previous_indentation(0)
        , spdlog_logger("", spdlog::sink_ptr(&sink))
        , sink(Sink())  //
    {
        previous_buffer  = Allocate_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);
        rendering_buffer = Allocate_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);
        trash_buffer     = Allocate_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);
        trash_buffer2    = Allocate_Array(arena, char, Tracing_Logger::MAX_BUFFER_SIZE);

        *previous_buffer = '\0';

        sink.set_level(spdlog::level::level_enum::trace);
        spdlog_logger.set_level(spdlog::level::level_enum::trace);

        // "[%H:%M:%S %z] [%n] [%^%L%$] [thread %t] %v\n"
        // sink.set_pattern("[%H:%M:%S %z] [%n] [%^%L%$] [thread %t] %v\n\0");
        spdlog_logger.set_pattern("[%L] %v\n\0");
        sink.set_pattern("[%L] %v\n\0");
    }
};

#if 1

using Root_Logger_Type = Tracing_Logger;
#define MAKE_LOGGER(logger_ptr, arena) \
    { (logger_ptr) = std::construct_at((logger_ptr), (arena)); }

#else

// NOTE: void* = отключение логирования
using Root_Logger_Type = void*;
#define MAKE_LOGGER(logger_ptr, arena) (void)0

#endif

global_var Root_Logger_Type* root_logger = nullptr;

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

Logger__Function(Tracing_Logger_Routine) {
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

        data.spdlog_logger.info(buffer_to_log);
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

    // NOTE: Логируем через spdlog
    switch (log_type) {
    case Log_Type::DEBUG:
        common_log(data, message, [&data](const char* message) {
            data.spdlog_logger.debug(message);
        });
        break;
    case Log_Type::INFO:
        common_log(data, message, [&data](const char* message) {
            data.spdlog_logger.info(message);
        });
        break;
    case Log_Type::WARN:
        common_log(data, message, [&data](const char* message) {
            data.spdlog_logger.warn(message);
        });
        break;
    case Log_Type::ERR:
        common_log(data, message, [&data](const char* message) {
            data.spdlog_logger.error(message);
        });
        break;
    default:
        INVALID_PATH;
    }
}

#define _LOG_COMMON(log_type_, message_, ...)                              \
    [&] {                                                                  \
        if (logger_routine != nullptr) {                                   \
            auto str = spdlog::fmt_lib::vformat(                           \
                (message_), spdlog::fmt_lib::make_format_args(__VA_ARGS__) \
            );                                                             \
            logger_routine(logger_data, (log_type_), str.c_str());         \
        }                                                                  \
    }()

#define LOG_DEBUG(message_, ...) _LOG_COMMON(Log_Type::DEBUG, (message_), ##__VA_ARGS__)
#define LOG_INFO(message_, ...) _LOG_COMMON(Log_Type::INFO, (message_), ##__VA_ARGS__)
#define LOG_WARN(message_, ...) _LOG_COMMON(Log_Type::WARN, (message_), ##__VA_ARGS__)
#define LOG_ERROR(message_, ...) _LOG_COMMON(Log_Type::ERR, (message_), ##__VA_ARGS__)

bool operator==(const std::source_location& a, const std::source_location& b) {
    return (
        a.line() == b.line()               //
        && a.column() == b.column()        //
        && a.file_name() == b.file_name()  //
        && a.function_name() == b.function_name()
    );
}

Logger_Tracing__Function(Tracing_Logger_Tracing_Routine) {
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
    std::array<char, N + 1> res = {};
    std::copy_n(src, N, res.data());
    res.data()[N] = '\0';
    return res;
}

// TODO: А можно ли как-то убрать const_cast тут?
// TODO: Вычислять `static constexpr const scope_name = ...`.
#define LOG_TRACING_SCOPE                                                                  \
    if (logger_tracing_routine != nullptr) {                                               \
        constexpr const auto   loc = std::source_location::current();                      \
        constexpr const auto   n   = index_of_brace_in_function_name(loc.function_name()); \
        constexpr const auto   function_name = extract_substring<n>(loc.function_name());  \
        constexpr const char*  file_n        = extract_file_name(loc.file_name());         \
        constexpr const size_t nn            = fmt::formatted_size(                        \
            FMT_COMPILE("[{}:{}:{}:{}]"),                                       \
            file_n,                                                             \
            loc.line(),                                                         \
            loc.column(),                                                       \
            function_name.data()                                                \
        );                                                                      \
        constexpr char scope_name[nn + 1] = {};                                            \
        fmt::format_to(                                                                    \
            const_cast<char*>(scope_name),                                                 \
            FMT_COMPILE("[{}:{}:{}:{}]"),                                                  \
            file_n,                                                                        \
            loc.line(),                                                                    \
            loc.column(),                                                                  \
            function_name.data()                                                           \
        );                                                                                 \
        const_cast<char*>(scope_name)[nn] = '\0';                                          \
        static_assert(scope_name != nullptr);                                              \
                                                                                           \
        logger_tracing_routine(logger_data, true, scope_name);                             \
    }                                                                                      \
                                                                                           \
    defer {                                                                                \
        if (logger_tracing_routine != nullptr)                                             \
            logger_tracing_routine(logger_data, false, nullptr);                           \
    };

#define CTX_LOGGER                                              \
    auto& logger_routine         = ctx->logger_routine;         \
    auto& logger_tracing_routine = ctx->logger_tracing_routine; \
    auto& logger_data            = ctx->logger_data;
