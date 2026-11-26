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

#include <assert.h>
#include <cplat/base/platform.h>
#include <cplat/clock/timespec.h>
#include <limits.h>
#include <stddef.h>

#include "clock_internal.h"

static_assert(sizeof(cplat_timespec) == 16, "cplat: cplat_timespec must be 16 bytes");

#if defined(PLATFORM_LINUX)
/* Linux ではネイティブ struct timespec とのバイナリ レイアウト互換を担保する。 */
static_assert(sizeof(cplat_timespec) == sizeof(struct timespec),
              "cplat: cplat_timespec must have the same size as struct timespec");
static_assert(offsetof(cplat_timespec, tv_sec) == offsetof(struct timespec, tv_sec),
              "cplat: cplat_timespec::tv_sec must be at the same offset as struct timespec::tv_sec");
static_assert(offsetof(cplat_timespec, tv_nsec) == offsetof(struct timespec, tv_nsec),
              "cplat: cplat_timespec::tv_nsec must be at the same offset as struct timespec::tv_nsec");
#endif /* PLATFORM_LINUX */

/* 秒部とナノ秒から生じる繰り上げを、最大 5 項で合成する。 */
#define SECONDS_TERM_COUNT 5U

/* normalize で sum_seconds へ渡す項の個数 (tv_sec と繰り上げ)。 */
#define NORMALIZE_TERM_COUNT (2U)

/* combine_timespec で terms 配列に並べる各項のインデックス。 */
#define TERM_A_SEC          (0U)
#define TERM_B_SEC          (1U)
#define TERM_A_NSEC_CARRY   (2U)
#define TERM_B_NSEC_CARRY   (3U)
#define TERM_NSEC_SUM_CARRY (4U)

static_assert(SECONDS_TERM_COUNT <= (sizeof(unsigned int) * CHAR_BIT), "cplat: seconds term count exceeds mask width");
static_assert(SECONDS_TERM_COUNT == 5U, "cplat: combine_timespec fills 5 terms");

static_assert(sizeof(time_t) == sizeof(int64_t), "cplat: time_t must be 64-bit");
static_assert((time_t)-1 < (time_t)0, "cplat: time_t must be signed");

static uint64_t magnitude_of(const int64_t value);
static int sum_seconds(const int64_t *values, const unsigned int subtract_mask, const size_t count,
                       int64_t *seconds_out);
static void split_nanoseconds(const int64_t value, int64_t *carry_out, int64_t *nanoseconds_out);
static int combine_timespec(const cplat_timespec *a, const cplat_timespec *b, const int subtract,
                            cplat_timespec *value_out);

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_timespec_normalize(cplat_timespec *ts)
{
    int64_t carry;
    int64_t nanoseconds;
    int64_t seconds;
    int64_t terms[NORMALIZE_TERM_COUNT];
    int ret;
    if (ts == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    split_nanoseconds(ts->tv_nsec, &carry, &nanoseconds);
    terms[0] = (int64_t)ts->tv_sec;
    terms[1] = carry;
    ret = sum_seconds(terms, 0U, sizeof(terms) / sizeof(terms[0]), &seconds);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ts->tv_sec = (time_t)seconds;
    ts->tv_nsec = nanoseconds;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_timespec_add(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *sum_out)
{
    return combine_timespec(a, b, 0, sum_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_timespec_sub(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *difference_out)
{
    return combine_timespec(a, b, 1, difference_out);
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

int cplat_timespec_add_ms(const cplat_timespec *ts, const uint64_t timeout_ms, cplat_timespec *sum_out)
{
    cplat_timespec interval;
    /* UINT64_MAX / 1000 は INT64_MAX 以下なので、秒への変換は表現範囲内である。 */
    interval.tv_sec = (time_t)(timeout_ms / (uint64_t)CLOCK_MSEC_PER_SEC);
    interval.tv_nsec = (int64_t)(timeout_ms % (uint64_t)CLOCK_MSEC_PER_SEC) * CLOCK_NSEC_PER_MSEC;
    return combine_timespec(ts, &interval, 0, sum_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_timespec_diff_ms(const cplat_timespec *end, const cplat_timespec *start, int64_t *milliseconds_out)
{
    cplat_timespec difference;
    int64_t milliseconds;
    int64_t fraction;
    int ret;
    if (milliseconds_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_timespec_sub(end, start, &difference);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    fraction = difference.tv_nsec / CLOCK_NSEC_PER_MSEC;
    if (difference.tv_sec >= 0)
    {
        if ((uint64_t)difference.tv_sec > (uint64_t)INT64_MAX / (uint64_t)CLOCK_MSEC_PER_SEC)
        {
            return CPLAT_ERR_OUT_OF_RANGE;
        }
        milliseconds = (int64_t)difference.tv_sec * CLOCK_MSEC_PER_SEC;
        if (milliseconds > INT64_MAX - fraction)
        {
            return CPLAT_ERR_OUT_OF_RANGE;
        }
        milliseconds += fraction;
    }
    else
    {
        const uint64_t limit = (uint64_t)INT64_MAX + 1U;
        uint64_t magnitude = magnitude_of((int64_t)difference.tv_sec);
        /* 正規化した負の秒と正の端数を合わせ、INT64_MIN の境界も先に検査する。 */
        if (magnitude > (limit + (uint64_t)CLOCK_MSEC_PER_SEC - 1U) / (uint64_t)CLOCK_MSEC_PER_SEC)
        {
            return CPLAT_ERR_OUT_OF_RANGE;
        }
        magnitude = magnitude * (uint64_t)CLOCK_MSEC_PER_SEC - (uint64_t)fraction;
        if (magnitude > limit)
        {
            return CPLAT_ERR_OUT_OF_RANGE;
        }
        milliseconds = -(int64_t)(magnitude - 1U) - 1;
    }
    *milliseconds_out = milliseconds;
    return CPLAT_OK;
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

/* INT64_MIN の絶対値も符号なし型で表す。 */
static uint64_t magnitude_of(const int64_t value)
{
    if (value < 0)
    {
        return (uint64_t)(-(value + 1)) + 1U;
    }
    return (uint64_t)value;
}

/*
 * values の count 項を合計する。subtract_mask のビットが立つ項は減算する。
 * 呼び出し元は count を SECONDS_TERM_COUNT 以下にする。
 * 先に正負を相殺し、途中の加算だけで表現範囲を超える場合も正しい最終値を求める。
 */
static int sum_seconds(const int64_t *values, const unsigned int subtract_mask, const size_t count,
                       int64_t *seconds_out)
{
    uint64_t positive[SECONDS_TERM_COUNT] = {0};
    uint64_t negative[SECONDS_TERM_COUNT] = {0};
    uint64_t positive_sum = 0U;
    uint64_t negative_sum = 0U;
    const uint64_t negative_limit = (uint64_t)INT64_MAX + 1U;
    size_t i;
    size_t j;

    for (i = 0U; i < count; i++)
    {
        const int is_negative = (values[i] < 0) != ((subtract_mask & (1U << i)) != 0U);
        if (is_negative != 0)
        {
            negative[i] = magnitude_of(values[i]);
        }
        else
        {
            positive[i] = magnitude_of(values[i]);
        }
    }
    for (i = 0U; i < count; i++)
    {
        for (j = 0U; j < count; j++)
        {
            uint64_t cancel;
            if (positive[i] < negative[j])
            {
                cancel = positive[i];
            }
            else
            {
                cancel = negative[j];
            }
            positive[i] -= cancel;
            negative[j] -= cancel;
        }
    }
    for (i = 0U; i < count; i++)
    {
        if ((positive[i] > (uint64_t)INT64_MAX - positive_sum) || (negative[i] > negative_limit - negative_sum))
        {
            return CPLAT_ERR_OUT_OF_RANGE;
        }
        positive_sum += positive[i];
        negative_sum += negative[i];
    }
    if (negative_sum != 0U)
    {
        *seconds_out = -(int64_t)(negative_sum - 1U) - 1;
    }
    else
    {
        *seconds_out = (int64_t)positive_sum;
    }
    return CPLAT_OK;
}

/* 負の剰余は 1 秒借りて、0 以上のナノ秒へ変換する。 */
static void split_nanoseconds(const int64_t value, int64_t *carry_out, int64_t *nanoseconds_out)
{
    int64_t carry = value / CLOCK_NSEC_PER_SEC;
    int64_t nanoseconds = value % CLOCK_NSEC_PER_SEC;
    if (nanoseconds < 0)
    {
        carry -= 1;
        nanoseconds += CLOCK_NSEC_PER_SEC;
    }
    *carry_out = carry;
    *nanoseconds_out = nanoseconds;
}

/* ナノ秒の商は秒部へ、剰余は小さい値同士の演算へ分ける。 */
static int combine_timespec(const cplat_timespec *a, const cplat_timespec *b, const int subtract,
                            cplat_timespec *value_out)
{
    int64_t terms[SECONDS_TERM_COUNT];
    int64_t nanoseconds;
    int64_t seconds;
    unsigned int subtract_mask = 0U;
    int ret;
    if ((a == NULL) || (b == NULL) || (value_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    terms[TERM_A_SEC] = (int64_t)a->tv_sec;
    terms[TERM_B_SEC] = (int64_t)b->tv_sec;
    terms[TERM_A_NSEC_CARRY] = a->tv_nsec / CLOCK_NSEC_PER_SEC;
    terms[TERM_B_NSEC_CARRY] = b->tv_nsec / CLOCK_NSEC_PER_SEC;
    if (subtract != 0)
    {
        nanoseconds = (a->tv_nsec % CLOCK_NSEC_PER_SEC) - (b->tv_nsec % CLOCK_NSEC_PER_SEC);
        subtract_mask = (1U << TERM_B_SEC) | (1U << TERM_B_NSEC_CARRY);
    }
    else
    {
        nanoseconds = (a->tv_nsec % CLOCK_NSEC_PER_SEC) + (b->tv_nsec % CLOCK_NSEC_PER_SEC);
    }
    split_nanoseconds(nanoseconds, &terms[TERM_NSEC_SUM_CARRY], &nanoseconds);
    ret = sum_seconds(terms, subtract_mask, SECONDS_TERM_COUNT, &seconds);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    value_out->tv_sec = (time_t)seconds;
    value_out->tv_nsec = nanoseconds;
    return CPLAT_OK;
}
