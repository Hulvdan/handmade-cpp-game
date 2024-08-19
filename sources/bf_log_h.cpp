enum class Log_Type {
    DEBUG = 0,
    INFO,
    WARN,
    ERR,
};

#define Logger_function(name_) \
    void name_(void* logger_data, Log_Type log_type, const char* message)
#define Logger_Scope_function(name_) \
    void name_(void* logger_data, bool push, const char* location)

using Logger_function_t       = Logger_function((*));
using Logger_Scope_function_t = Logger_Scope_function((*));

#define _LOG_COMMON(log_type_, ...)                                             \
    STATEMENT({                                                                 \
        if (logger_routine) {                                                   \
            const auto str = Text_Format(__VA_ARGS__);                          \
            ((Logger_function_t)logger_routine)(logger_data, (log_type_), str); \
        }                                                                       \
    })

#define LOG_DEBUG(...) _LOG_COMMON(Log_Type::DEBUG, __VA_ARGS__)
#define LOG_INFO(...) _LOG_COMMON(Log_Type::INFO, __VA_ARGS__)
#define LOG_WARN(...) _LOG_COMMON(Log_Type::WARN, __VA_ARGS__)
#define LOG_ERROR(...) _LOG_COMMON(Log_Type::ERR, __VA_ARGS__)
