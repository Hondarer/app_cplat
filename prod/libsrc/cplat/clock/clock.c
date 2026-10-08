/**
 *******************************************************************************
 *  @file           clock.c
 *  @brief          プラットフォームを抽象化してクロックを取得する機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/19
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/clock.h>
#include <cplat/clock/filetime_conv.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/time.h>
#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <time.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif /* PLATFORM_ */

/* 変換定数 */
#define MSEC_PER_SEC  (1000ULL)      /* ミリ秒 / 秒 */
#define NSEC_PER_SEC  (1000000000LL) /* ナノ秒 / 秒 */
#define NSEC_PER_MSEC (1000000ULL)   /* ナノ秒 / ミリ秒 */
#define SEC_PER_DAY   (86400LL)      /* 秒 / 日 */

static const char s_iso8601_local_fallback[] = "0000-00-00T00:00:00.000+00:00";
static const char s_iso8601_utc_fallback[] = "0000-00-00T00:00:00.000Z";

static void clock_write_fallback(char *buf, const size_t buf_size, const char *fallback)
{
    if (buf == NULL || buf_size == 0)
    {
        return;
    }

    (void)cplat_snprintf(buf, buf_size, "%s", fallback);
}

static int64_t clock_days_from_civil(const int year, const unsigned month, const unsigned day)
{
    int adjusted_year = year - (month <= 2);
    int era_base;
    int era;
    unsigned yoe;
    unsigned month_offset;
    unsigned doy;
    unsigned doe;

    if (adjusted_year >= 0)
    {
        era_base = adjusted_year;
    }
    else
    {
        era_base = adjusted_year - 399;
    }
    era = era_base / 400;

    yoe = (unsigned)(adjusted_year - era * 400);

    if (month > 2)
    {
        month_offset = (unsigned)-3;
    }
    else
    {
        month_offset = 9;
    }
    doy = (153 * (month + month_offset) + 2) / 5 + day - 1;

    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static void clock_utc_offset_minutes(const struct tm *local_tm, const struct tm *utc_tm, int *offset_minutes)
{
    int64_t local_days;
    int64_t utc_days;
    int local_seconds;
    int utc_seconds;
    int64_t delta_seconds;

    local_days =
        clock_days_from_civil(local_tm->tm_year + 1900, (unsigned)local_tm->tm_mon + 1, (unsigned)local_tm->tm_mday);
    utc_days = clock_days_from_civil(utc_tm->tm_year + 1900, (unsigned)utc_tm->tm_mon + 1, (unsigned)utc_tm->tm_mday);
    local_seconds = local_tm->tm_hour * 3600 + local_tm->tm_min * 60 + local_tm->tm_sec;
    utc_seconds = utc_tm->tm_hour * 3600 + utc_tm->tm_min * 60 + utc_tm->tm_sec;
    delta_seconds = (local_days - utc_days) * SEC_PER_DAY + (local_seconds - utc_seconds);

    *offset_minutes = (int)(delta_seconds / 60);
}

static int clock_format_iso8601_utc_from_tm(char *buf, const size_t buf_size, const struct tm *utc_tm,
                                            const int64_t tv_nsec)
{
    if (buf == NULL || buf_size < (size_t)(CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1))
    {
        return -1;
    }

    if (cplat_snprintf(buf, buf_size, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", utc_tm->tm_year + 1900, utc_tm->tm_mon + 1,
                       utc_tm->tm_mday, utc_tm->tm_hour, utc_tm->tm_min, utc_tm->tm_sec,
                       (int)(tv_nsec / 1000000)) != CPLAT_OK)
    {
        return -1;
    }
    return 0;
}

static int clock_format_iso8601_local_from_tm(char *buf, const size_t buf_size, const struct tm *local_tm,
                                              const int64_t tv_nsec, const int offset_minutes)
{
    char offset_sign;
    int abs_offset_minutes;
    int offset_hours;
    int offset_mins;

    if (buf == NULL || buf_size < (size_t)(CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1))
    {
        return -1;
    }

    if (offset_minutes < 0)
    {
        offset_sign = '-';
        abs_offset_minutes = -offset_minutes;
    }
    else
    {
        offset_sign = '+';
        abs_offset_minutes = offset_minutes;
    }
    offset_hours = abs_offset_minutes / 60;
    offset_mins = abs_offset_minutes % 60;

    if (cplat_snprintf(buf, buf_size, "%04d-%02d-%02dT%02d:%02d:%02d.%03d%c%02d:%02d", local_tm->tm_year + 1900,
                       local_tm->tm_mon + 1, local_tm->tm_mday, local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec,
                       (int)(tv_nsec / 1000000), offset_sign, offset_hours, offset_mins) != CPLAT_OK)
    {
        return -1;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

uint64_t cplat_clock_get_monotonic_ms(void)
{
#if defined(PLATFORM_LINUX)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * MSEC_PER_SEC + (uint64_t)ts.tv_nsec / NSEC_PER_MSEC;
#elif defined(PLATFORM_WINDOWS)
    return (uint64_t)GetTickCount64();
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_clock_get_monotonic(cplat_timespec *ts)
{
#if defined(PLATFORM_LINUX)
    struct timespec native;
    clock_gettime(CLOCK_MONOTONIC, &native);
    cplat_timespec_from_native(&native, ts);
#elif defined(PLATFORM_WINDOWS)
    ULONGLONG ms = GetTickCount64();
    ts->tv_sec = (time_t)(ms / MSEC_PER_SEC);
    ts->tv_nsec = (int64_t)((ms % MSEC_PER_SEC) * NSEC_PER_MSEC);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_clock_get_realtime(cplat_timespec *ts)
{
#if defined(PLATFORM_LINUX)
    struct timespec native;
    clock_gettime(CLOCK_REALTIME, &native);
    cplat_timespec_from_native(&native, ts);
#elif defined(PLATFORM_WINDOWS)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    cplat_internal_filetime_to_timespec(&ft, ts);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_clock_get_realtime_utc(struct tm *utc_tm, int32_t *tv_nsec)
{
    cplat_timespec realtime_ts;

    cplat_clock_get_realtime(&realtime_ts);
    /* 正規化済みの tv_nsec (0 以上 999,999,999 以下) は int32_t の表現範囲内に収まる */
    *tv_nsec = (int32_t)realtime_ts.tv_nsec;

    if (cplat_gmtime(utc_tm, &realtime_ts.tv_sec) != 0)
    {
        memset(utc_tm, 0, sizeof(*utc_tm));
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_clock_format_realtime_iso8601_local(char *buf, const size_t buf_size, const cplat_timespec *timestamp)
{
    struct tm local_tm;
    struct tm utc_tm;
    int offset_minutes;

    if (timestamp == NULL || timestamp->tv_nsec < 0 || timestamp->tv_nsec >= NSEC_PER_SEC)
    {
        clock_write_fallback(buf, buf_size, s_iso8601_local_fallback);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_localtime(&local_tm, &timestamp->tv_sec) != 0 || cplat_gmtime(&utc_tm, &timestamp->tv_sec) != 0)
    {
        clock_write_fallback(buf, buf_size, s_iso8601_local_fallback);
        return CPLAT_ERR_UNKNOWN;
    }
    clock_utc_offset_minutes(&local_tm, &utc_tm, &offset_minutes);
    if (clock_format_iso8601_local_from_tm(buf, buf_size, &local_tm, timestamp->tv_nsec, offset_minutes) != 0)
    {
        clock_write_fallback(buf, buf_size, s_iso8601_local_fallback);
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_clock_format_realtime_iso8601_utc(char *buf, const size_t buf_size, const cplat_timespec *timestamp)
{
    struct tm utc_tm;

    if (timestamp == NULL || timestamp->tv_nsec < 0 || timestamp->tv_nsec >= NSEC_PER_SEC)
    {
        clock_write_fallback(buf, buf_size, s_iso8601_utc_fallback);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_gmtime(&utc_tm, &timestamp->tv_sec) != 0 ||
        clock_format_iso8601_utc_from_tm(buf, buf_size, &utc_tm, timestamp->tv_nsec) != 0)
    {
        clock_write_fallback(buf, buf_size, s_iso8601_utc_fallback);
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_clock_get_realtime_deadline_ms(const uint64_t timeout_ms, struct timespec *abs_timeout)
{
    cplat_timespec now;
    cplat_timespec deadline;

    cplat_clock_get_realtime(&now);
    cplat_timespec_add_ms(&now, timeout_ms, &deadline);
    cplat_timespec_to_native(&deadline, abs_timeout);
}
