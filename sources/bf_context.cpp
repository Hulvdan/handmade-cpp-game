struct Context {
    u32 thread_index = {};

    void* allocator      = {};
    void* allocator_data = {};

    void* logger_data          = {};
    void* logger_routine       = {};
    void* logger_scope_routine = {};
};

#define MCTX Context* ctx
#define MCTX_ Context* /* ctx */

#define CTX_LOGGER                                                      \
    auto logger_data          = ctx->logger_data;                       \
    auto logger_routine       = (Logger_function_t)ctx->logger_routine; \
    auto logger_scope_routine = (Logger_Scope_function_t)ctx->logger_scope_routine;

#define CTX_ALLOCATOR                                           \
    auto allocator      = (Allocator_function_t)ctx->allocator; \
    auto allocator_data = ctx->allocator_data;

#define PUSH_CONTEXT(new_ctx, code) \
    STATEMENT({                     \
        auto ctx = (new_ctx);       \
        (code);                     \
    })
