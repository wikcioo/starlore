#pragma once

#define ENABLE_TRACE_LOG 1

#if defined(DEBUG)
    #define ENABLE_DEBUG_LOG 1
#endif

#define ENABLE_INFO_LOG 1
#define ENABLE_WARN_LOG 1

typedef enum log_level
{
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_e;

void logger_log_output(log_level_e level, const char *message, ...);

#if defined(ENABLE_TRACE_LOG)
    #define LOG_TRACE(message, ...) logger_log_output(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
    #define LOG_TRACE(message, ...)
#endif

#if defined(ENABLE_DEBUG_LOG)
    #define LOG_DEBUG(message, ...) logger_log_output(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(message, ...)
#endif

#if defined(ENABLE_INFO_LOG)
    #define LOG_INFO(message, ...) logger_log_output(LOG_LEVEL_INFO,  message, ##__VA_ARGS__)
#else
    #define LOG_INFO(message, ...)
#endif

#if defined(ENABLE_WARN_LOG)
    #define LOG_WARN(message, ...) logger_log_output(LOG_LEVEL_WARN,  message, ##__VA_ARGS__)
#else
    #define LOG_WARN(message, ...)
#endif

#define LOG_ERROR(message, ...) logger_log_output(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#define LOG_FATAL(message, ...) logger_log_output(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
