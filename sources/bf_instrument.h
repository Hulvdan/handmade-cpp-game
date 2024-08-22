#define ASSERT_SLOW (1 && BF_DEBUG)

#define BF_SANITIZATION_ENABLED (1 && BF_DEBUG)
#define BF_HUMAN_SANITIZATION 0

#if BF_HUMAN_SANITIZATION
#    define SANITIZE_HUMAN SANITIZE
#else
#    define SANITIZE_HUMAN ((void)0)
#endif