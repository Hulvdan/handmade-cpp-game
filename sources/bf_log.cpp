enum class Log_Type { DEBUG = 0, INFO, WARN, CRIT };

#define Logger__Function(name_) void name_(void* logger_data, Log_Type log_type, ...)
#define Logger_Tracing__Function(name_) void name_(void* logger_data, bool push, ...)

using Logger_Function_Type         = Logger__Function((*));
using Logger_Tracing_Function_Type = Logger_Tracing__Function((*));

// NOTE: Для отключения логирования по-умолчанию, нужно указать `void*`
using Root_Logger_Type = void*;

global Root_Logger_Type* root_logger = nullptr;

struct Tracing_Logger {
    int current_indentation;
    int collapse_number;

    enum Previous_Type {
        PREVIOUS_IS_SOURCE_LOCATION,
        PREVIOUS_IS_STRING,
    };
    Previous_Type previous_type;
    u8*           previous_buffer;
    size_t        previous_buffer_size;
};

Logger__Function(Tracing_Logger_Routine) {
    auto& data = *(Tracing_Logger*)logger_data;

    data.previous_type = Tracing_Logger::PREVIOUS_IS_SOURCE_LOCATION;
}

#define _LOG_COMMON(log_type_, ...)                        \
    [&] {                                                  \
        if (logger != nullptr)                             \
            logger(logger_data, (log_type_), __VA_ARGS__); \
    }()
#define LOG_DEBUG(...) _LOG_COMMON(Log_Type::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) _LOG_COMMON(Log_Type::INFO, __VA_ARGS__)
#define LOG_WARN(...) _LOG_COMMON(Log_Type::WARN, __VA_ARGS__)
#define LOG_CRIT(...) _LOG_COMMON(Log_Type::CRIT, __VA_ARGS__)

bool operator==(const std::source_location& a, const std::source_location& b) {
    return (
        a.line() == b.line()               //
        && a.column() == b.column()        //
        && a.file_name() == b.file_name()  //
        && a.function_name() == b.function_name()
    );
}

Logger_Tracing__Function(Tracing_Logger_Tracing_Routine) {
    auto  logger = Tracing_Logger_Routine;
    auto& data   = *(Tracing_Logger*)logger_data;

    if (push) {
        va_list args;
        va_start(args, push);
        std::source_location loc = va_arg(args, std::source_location);
        va_end(args);

        data.current_indentation++;
        if ((data.previous_type == Tracing_Logger::PREVIOUS_IS_SOURCE_LOCATION)
            && (loc == *(std::source_location*)data.previous_buffer)  //
        ) {
            data.collapse_number++;
        }
        else {
            // LOG_INFO(std::source_location);
            data.previous_type = Tracing_Logger::PREVIOUS_IS_SOURCE_LOCATION;
        }
    }
    else {
        data.current_indentation--;
    }
}

#define LOG_TRACING_SCOPE                                              \
    {                                                                  \
        CTX_LOGGER;                                                    \
        Tracing_Logger_Tracing_Routine(                                \
            logger_data, true, std::source_location::current()         \
        );                                                             \
        defer { Tracing_Logger_Tracing_Routine(logger_data, false); }; \
    }

#define CTX_LOGGER                   \
    auto& logger      = ctx->logger; \
    auto& logger_data = ctx->logger_data;
