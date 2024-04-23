#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/sinks/sink.h"

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

        // TODO: Команда ниже ожидает \0 на конце
        // ::OutputDebugStringA(formatted.data());

        DEBUG_Print(formatted.data());
        DEBUG_Print("\n");
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

enum class Log_Type { DEBUG = 0, INFO, WARN, CRIT };

#define Logger__Function(name_) \
    void name_(void* logger_data, Log_Type log_type, const char* message)
#define Logger_Tracing__Function(name_) void name_(void* logger_data, bool push, ...)

using Logger_Function_Type         = Logger__Function((*));
using Logger_Tracing_Function_Type = Logger_Tracing__Function((*));

struct Tracing_Logger {
    static constexpr int MAX_BUFFER_SIZE = 4096;

    int current_indentation;
    int collapse_number;

    enum Previous_Type {
        PREVIOUS_IS_SOURCE_LOCATION,
        PREVIOUS_IS_STRING,
    };
    Previous_Type previous_type;
    u8*           previous_buffer;
    size_t        previous_buffer_size;

    spdlog::logger spdlog_logger;
    Sink           sink;

    Tracing_Logger(Arena& arena)
        : current_indentation(0)
        , collapse_number(0)
        , previous_type(Previous_Type::PREVIOUS_IS_SOURCE_LOCATION)
        , previous_buffer(nullptr)
        , previous_buffer_size(0)
        , spdlog_logger("example_logger", spdlog::sink_ptr(&sink))  //
        , sink(Sink())                                              //
    {
        previous_buffer = Allocate_Array(arena, u8, Tracing_Logger::MAX_BUFFER_SIZE);

        sink.set_level(spdlog::level::level_enum::trace);
        spdlog_logger.set_level(spdlog::level::level_enum::trace);
        spdlog_logger.set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v\n\0");
        sink.set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v\n\0");
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

Logger__Function(Tracing_Logger_Routine) {
    auto& data = *(Tracing_Logger*)logger_data;

    data.previous_type = Tracing_Logger::PREVIOUS_IS_SOURCE_LOCATION;

    switch (log_type) {
    case Log_Type::DEBUG:
        data.spdlog_logger.debug(message);
        break;

    case Log_Type::INFO:
        data.spdlog_logger.info(message);
        break;

    case Log_Type::WARN:
        data.spdlog_logger.warn(message);
        break;

    case Log_Type::CRIT:
        data.spdlog_logger.error(message);
        break;

    default:
        INVALID_PATH;
    }
}

#define _LOG_COMMON(log_type_, message_, ...)                                \
    [&] {                                                                    \
        if (logger_routine != nullptr) {                                     \
            auto str = spdlog::fmt_lib::vformat(                             \
                (message_), ##spdlog::fmt_lib::make_format_args(__VA_ARGS__) \
            );                                                               \
            logger_routine(logger_data, (log_type_), str.c_str());           \
        }                                                                    \
    }()

#define LOG_DEBUG(message_, ...) _LOG_COMMON(Log_Type::DEBUG, (message_), ##__VA_ARGS__)
#define LOG_INFO(message_, ...) _LOG_COMMON(Log_Type::INFO, (message_), ##__VA_ARGS__)
#define LOG_WARN(message_, ...) _LOG_COMMON(Log_Type::WARN, (message_), ##__VA_ARGS__)
#define LOG_CRIT(message_, ...) _LOG_COMMON(Log_Type::CRIT, (message_), ##__VA_ARGS__)

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

    if (push) {
        va_list args;
        va_start(args, push);
        std::source_location loc = va_arg(args, std::source_location);
        va_end(args);

        data.current_indentation++;
        if (data.previous_buffer != nullptr  //
            && (data.previous_type == Tracing_Logger::PREVIOUS_IS_SOURCE_LOCATION)
            && (loc == *(std::source_location*)data.previous_buffer)  //
        ) {
            data.collapse_number++;
        }
        else {
            char rendered_loc[Tracing_Logger::MAX_BUFFER_SIZE];
            snprintf(
                rendered_loc,
                Tracing_Logger::MAX_BUFFER_SIZE,
                "[%d:%d:%s:%s]",
                loc.line(),
                loc.column(),
                loc.file_name(),
                loc.function_name()
            );

            logger_routine(logger_data, Log_Type::INFO, rendered_loc);

            memcpy(data.previous_buffer, (u8*)(&loc), sizeof(loc));
            data.previous_type = Tracing_Logger::PREVIOUS_IS_SOURCE_LOCATION;
        }
    }
    else {
        data.current_indentation--;
    }
}

#define LOG_TRACING_SCOPE                                                           \
    if (logger_tracing_routine != nullptr)                                          \
        logger_tracing_routine(logger_data, true, std::source_location::current()); \
                                                                                    \
    defer {                                                                         \
        if (logger_tracing_routine != nullptr)                                      \
            logger_tracing_routine(logger_data, false);                             \
    };

#define CTX_LOGGER                                              \
    auto& logger_routine         = ctx->logger_routine;         \
    auto& logger_tracing_routine = ctx->logger_tracing_routine; \
    auto& logger_data            = ctx->logger_data;
