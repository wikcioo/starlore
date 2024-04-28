#pragma once

#include "defines.h"

#if defined(__clang__) || defined(__GNUC__)
    #define DEBUG_BREAK() __builtin_trap()
#else
    #error "Only clang and gcc compilers are supported!"
#endif

void report_assertion_failure(const char *expression, const char *message, const char *file, i32 line);

#if defined(ENABLE_ASSERTIONS)
    #define ASSERT(expr)                                                \
        {                                                               \
            if (!(expr)) {                                              \
                report_assertion_failure(#expr, 0, __FILE__, __LINE__); \
                DEBUG_BREAK();                                          \
            }                                                           \
        }

    #define ASSERT_MSG(expr, message)                                         \
        {                                                                     \
            if (!(expr)) {                                                    \
                report_assertion_failure(#expr, message, __FILE__, __LINE__); \
                DEBUG_BREAK();                                                \
            }                                                                 \
        }
#else
    #define ASSERT(expr)
    #define ASSERT_MSG(expr, message)
#endif
