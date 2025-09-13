#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static inline const char* curr_time(void)
{
    static _Thread_local char buffer[64];

    struct timespec ts;
    struct tm tm_info;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_info);

    snprintf(buffer,
        sizeof(buffer),
        "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        tm_info.tm_year + 1900,
        tm_info.tm_mon + 1,
        tm_info.tm_mday,
        tm_info.tm_hour,
        tm_info.tm_min,
        tm_info.tm_sec,
        ts.tv_nsec / 1000000);

    return buffer;
}

static inline void log_print(
    FILE* out, const char* level, const char* filepath, int line, const char* fmt, ...)
{
    size_t offset = (size_t)YACW_BASE_DIR_LEN;
    const char* fileptr = filepath;

    size_t flen = strlen(filepath);
    if (offset < flen) {
        fileptr = filepath + offset;
    }

    fprintf(out, "%23s %5s | %15s:%4d: ", curr_time(), level, fileptr, line);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);

    fprintf(out, "\n");
}

#define LOG_INFO(format, ...) log_print(stdout, "INFO", __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) log_print(stderr, "ERROR", __FILE__, __LINE__, format, ##__VA_ARGS__)

#endif // LOG_H
