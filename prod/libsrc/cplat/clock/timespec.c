/**
 *******************************************************************************
 *  @file           timespec.c
 *  @brief          プラットフォーム互換の標準時刻型 cplat_timespec の時刻計算機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/10
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/clock/timespec.h>
#include <stddef.h>

/* 変換定数 */
#define MSEC_PER_SEC  (1000LL)
#define NSEC_PER_SEC  (1000000000LL)
#define NSEC_PER_MSEC (1000000LL)

_Static_assert(sizeof(cplat_timespec) == 16, "cplat: cplat_timespec must be 16 bytes");

#if defined(PLATFORM_LINUX)
/* Linux ではネイティブ struct timespec とのバイナリ レイアウト互換を担保する。 */
_Static_assert(sizeof(cplat_timespec) == sizeof(struct timespec),
               "cplat: cplat_timespec must have the same size as struct timespec");
_Static_assert(offsetof(cplat_timespec, tv_sec) == offsetof(struct timespec, tv_sec),
               "cplat: cplat_timespec::tv_sec must be at the same offset as struct timespec::tv_sec");
_Static_assert(offsetof(cplat_timespec, tv_nsec) == offsetof(struct timespec, tv_nsec),
               "cplat: cplat_timespec::tv_nsec must be at the same offset as struct timespec::tv_nsec");
#endif /* PLATFORM_LINUX */

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_timespec_normalize(cplat_timespec *ts)
{
    if (ts == NULL)
    {
        return;
    }

    if (ts->tv_nsec >= NSEC_PER_SEC || ts->tv_nsec < 0)
    {
        int64_t carry_sec = ts->tv_nsec / NSEC_PER_SEC;
        int64_t nsec = ts->tv_nsec % NSEC_PER_SEC;

        /*
         * C の整数除算は 0 方向へ切り捨て、剰余の符号は被除数に従う。
         * tv_nsec が負のとき剰余も負になるため、1 [s] 借りてナノ秒を 0 以上にする。
         * see: https://en.cppreference.com/w/c/language/operator_arithmetic
         */
        if (nsec < 0)
        {
            carry_sec -= 1;
            nsec += NSEC_PER_SEC;
        }

        ts->tv_sec += (time_t)carry_sec;
        ts->tv_nsec = nsec;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_timespec_add(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result)
{
    if (a == NULL || b == NULL || result == NULL)
    {
        return;
    }

    cplat_timespec sum;

    sum.tv_sec = a->tv_sec + b->tv_sec;
    sum.tv_nsec = a->tv_nsec + b->tv_nsec;
    cplat_timespec_normalize(&sum);

    *result = sum;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_timespec_sub(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result)
{
    if (a == NULL || b == NULL || result == NULL)
    {
        return;
    }

    cplat_timespec diff;

    diff.tv_sec = a->tv_sec - b->tv_sec;
    diff.tv_nsec = a->tv_nsec - b->tv_nsec;
    cplat_timespec_normalize(&diff);

    *result = diff;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_timespec_cmp(const cplat_timespec *a, const cplat_timespec *b)
{
    if (a == NULL || b == NULL)
    {
        return 0;
    }

    if (a->tv_sec < b->tv_sec)
    {
        return -1;
    }

    if (a->tv_sec > b->tv_sec)
    {
        return 1;
    }

    if (a->tv_nsec < b->tv_nsec)
    {
        return -1;
    }

    if (a->tv_nsec > b->tv_nsec)
    {
        return 1;
    }

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_timespec_add_ms(const cplat_timespec *ts, const uint64_t timeout_ms, cplat_timespec *result)
{
    if (ts == NULL || result == NULL)
    {
        return;
    }

    cplat_timespec sum;

    sum.tv_sec = ts->tv_sec + (time_t)(timeout_ms / (uint64_t)MSEC_PER_SEC);
    sum.tv_nsec = ts->tv_nsec + (int64_t)(timeout_ms % (uint64_t)MSEC_PER_SEC) * NSEC_PER_MSEC;
    cplat_timespec_normalize(&sum);

    *result = sum;
}

/* Doxygen コメントは、ヘッダーに記載 */

int64_t cplat_timespec_diff_ms(const cplat_timespec *end, const cplat_timespec *start)
{
    if (end == NULL || start == NULL)
    {
        return 0;
    }

    cplat_timespec diff;

    cplat_timespec_sub(end, start, &diff);

    /* diff は正規化済み (tv_nsec は 0 以上) のため、この式は負方向への切り捨て (床関数) になる。 */
    return (int64_t)diff.tv_sec * MSEC_PER_SEC + diff.tv_nsec / NSEC_PER_MSEC;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_timespec_to_native(const cplat_timespec *ts, struct timespec *native)
{
    if (ts == NULL || native == NULL)
    {
        return;
    }

    native->tv_sec = ts->tv_sec;

    /*
     * POSIX struct timespec::tv_nsec (long) との境界キャスト。
     * Windows では long が 32 ビットだが、正規化済みの tv_nsec は表現範囲内に収まる。
     */
    native->tv_nsec = (long)ts->tv_nsec;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_timespec_from_native(const struct timespec *native, cplat_timespec *ts)
{
    if (native == NULL || ts == NULL)
    {
        return;
    }

    ts->tv_sec = native->tv_sec;
    ts->tv_nsec = (int64_t)native->tv_nsec;
}
