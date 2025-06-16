#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static inline const char* curr_time(void)
{
    static char buffer[64];

    struct timespec ts;
    struct tm tm_info;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_info);

    snprintf(buffer, sizeof(buffer),
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

#define LOG_INFO(format, ...) \
    fprintf(stdout, "[%23s] [%5s] %15s:%4d: " format "\n", curr_time(), "INFO", __FILE__ + YACW_BASE_DIR_LEN, __LINE__, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) \
    fprintf(stderr, "[%23s] [%5s] %15s:%4d: " format "\n", curr_time(), "ERROR", __FILE__ + YACW_BASE_DIR_LEN, __LINE__, ##__VA_ARGS__)

#endif // LOG_H