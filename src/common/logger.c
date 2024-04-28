#include "logger.h"

#include <stdio.h>
#include <stdarg.h>

#include "defines.h"

static const char *levels_str[] = { "trace", "debug", "info", "warn", "error", "fatal" };

void logger_log_output(log_level_e level, const char *message, ...)
{
    FILE *stream = level > LOG_LEVEL_WARN ? stderr : stdout;

    va_list args;
    va_start(args, message);
    fprintf(stream, "%s: ", levels_str[level]);
    vfprintf(stream, message, args);
    fprintf(stream, "\n");
    va_end(args);
}

void report_assertion_failure(const char *expression, const char *message, const char *file, i32 line)
{
    LOG_FATAL("assertion failure - expression (%s), message: '%s' in %s:%d",
              expression, (message == 0 ? "<empty>" : message), file, line);
}
