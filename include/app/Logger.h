#pragma once

// must same as syslog.h
#define AM_LOG_LEVEL_DEBUG 7
#define AM_LOG_LEVEL_INFO 6
#define AM_LOG_LEVEL_NOTICE 5
#define AM_LOG_LEVEL_WARN 4
#define AM_LOG_LEVEL_ERROR 3

#ifdef CONFIG_ACTIVITY_SERVICE_LOG_LEVEL
#define AM_LOG_LEVEL CONFIG_ACTIVITY_SERVICE_LOG_LEVEL
#else
#define AM_LOG_LEVEL AM_LOG_LEVEL_INFO
#endif

#ifndef __NuttX__
#define am_log_print(level, fmt, ...) printf(fmt, __VA_ARGS__)
#else
#include <syslog.h>
#define am_log_print(level, fmt, ...) syslog(level, fmt, ##__VA_ARGS__)
#endif

#define AM_LOG(LEVEL, fmt, ...)                               \
    {                                                         \
        if (AM_LOG_LEVEL >= LEVEL) {                          \
            am_log_print(LEVEL, "[AMS] " fmt, ##__VA_ARGS__); \
        }                                                     \
    }

#ifdef CONFIG_ACTIVITY_SERVICE_LOG_WITH_COLOR
#define ACOLOR_GRAY "\x1b[90m"
#define ACOLOR_RED "\x1b[31m"
#define ACOLOR_GREEN "\x1b[32m"
#define ACOLOR_YELLOW "\x1b[93m"
#define ACOLOR_BLUE "\x1b[34m"
#define ACOLOR_RESET "\x1b[0m"
#else
#define ACOLOR_GRAY ""
#define ACOLOR_RED ""
#define ACOLOR_GREEN ""
#define ACOLOR_YELLOW ""
#define ACOLOR_BLUE ""
#define ACOLOR_RESET ""
#endif

// Redefining the Android LOG for using AM_LOG
#ifdef ALOGT
#undef ALOGT
#endif
#ifdef ALOGD
#undef ALOGD
#endif
#ifdef ALOGI
#undef ALOGI
#endif
#ifdef ALOGW
#undef ALOGW
#endif
#ifdef ALOGE
#undef ALOGE
#endif

#define ALOGD(fmt, ...) AM_LOG(AM_LOG_LEVEL_DEBUG, ACOLOR_GRAY fmt ACOLOR_RESET, ##__VA_ARGS__)
#define ALOGI(fmt, ...) AM_LOG(AM_LOG_LEVEL_INFO, ACOLOR_GREEN fmt ACOLOR_RESET, ##__VA_ARGS__)
#define ALOGN(fmt, ...) AM_LOG(AM_LOG_LEVEL_NOTICE, ACOLOR_BLUE fmt ACOLOR_RESET, ##__VA_ARGS__)
#define ALOGW(fmt, ...) AM_LOG(AM_LOG_LEVEL_WARN, ACOLOR_YELLOW fmt ACOLOR_RESET, ##__VA_ARGS__)
#define ALOGE(fmt, ...) AM_LOG(AM_LOG_LEVEL_ERROR, ACOLOR_RED fmt ACOLOR_RESET, ##__VA_ARGS__)
