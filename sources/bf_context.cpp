struct Context {
    u32 thread_index = {};

    void*     allocator_data = {};
    void_func allocator      = {};  // NOTE: Allocator_function_t

    void*     logger_data          = {};
    void_func logger_routine       = {};  // NOTE: Logger_function_t
    void_func logger_scope_routine = {};  // NOTE: Logger_Scope_function_t
};

#define MCTX Context* ctx
#define MCTX_ Context* /* ctx */

#define CTX_ALLOCATOR                                           \
    auto allocator      = (Allocator_function_t)ctx->allocator; \
    auto allocator_data = ctx->allocator_data;

#define CTX_LOGGER                                                      \
    auto logger_data          = ctx->logger_data;                       \
    auto logger_routine       = (Logger_function_t)ctx->logger_routine; \
    auto logger_scope_routine = (Logger_Scope_function_t)ctx->logger_scope_routine;
