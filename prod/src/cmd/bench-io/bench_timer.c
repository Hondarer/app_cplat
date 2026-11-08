/**
 *******************************************************************************
 *  @file           bench_timer.c
 *  @brief          反復回数の自動調整と所要時間の集計を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/29
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/clock/clock.h>
#include <cplat/clock/timespec.h>

#include "bench_timer.h"

/** @brief 1 [s] あたりのナノ秒数です。 */
#define BENCH_NS_PER_SEC 1000000000LL

/** @brief 1 [ms] あたりのナノ秒数です。 */
#define BENCH_NS_PER_MS 1000000LL

/**
 *  @brief          2 点間の経過時間をナノ秒で求めます。
 *  @param[in]      start  開始時刻。NULL を渡してはなりません。
 *  @param[in]      end    終了時刻。NULL を渡してはなりません。
 *  @param[out]     nanoseconds_out 経過時間 [ns]。負の差は 0 を格納します。
 *  @return         成功時は 0、時刻差または変換の範囲外は -1 を返します。
 *
 *  `cplat_timespec_diff_ms()` はミリ秒単位のため、
 *  ナノ秒の分解能を保つ目的で `cplat_timespec_sub()` の結果から自前で算出します。
 */
static int elapsed_ns(const cplat_timespec *start, const cplat_timespec *end, uint64_t *nanoseconds_out)
{
    cplat_timespec difference;
    uint64_t seconds;
    uint64_t nanoseconds;
    if (cplat_timespec_sub(end, start, &difference) != CPLAT_OK)
    {
        return -1;
    }
    if (difference.tv_sec < 0)
    {
        *nanoseconds_out = 0U;
        return 0;
    }
    seconds = (uint64_t)difference.tv_sec;
    nanoseconds = (uint64_t)difference.tv_nsec;
    if (seconds > (UINT64_MAX - nanoseconds) / (uint64_t)BENCH_NS_PER_SEC)
    {
        return -1;
    }
    *nanoseconds_out = seconds * (uint64_t)BENCH_NS_PER_SEC + nanoseconds;
    return 0;
}

/**
 *  @brief          指定回数だけ測定対象を実行し、区間の所要時間を求めます。
 *  @param[in]      fn          測定対象を 1 反復ぶん実行するコールバック。NULL を渡してはなりません。
 *  @param[in,out]  arg         @p fn へ渡すデータ。
 *  @param[in]      iterations  反復回数。1 以上を指定します。
 *  @param[out]     total_ns    区間の所要時間 [ns] の格納先。NULL を渡してはなりません。
 *  @return         成功時は 0、@p fn の失敗時は -1 を返します。
 */
static int run_block(bench_iteration_fn fn, void *arg, uint64_t iterations, uint64_t *total_ns)
{
    cplat_timespec start;
    cplat_timespec end;
    uint64_t index;

    if (cplat_clock_get_monotonic(&start) != CPLAT_OK)
    {
        return -1;
    }

    for (index = 0U; index < iterations; index++)
    {
        if (fn(arg) != 0)
        {
            return -1;
        }
    }

    if (cplat_clock_get_monotonic(&end) != CPLAT_OK)
    {
        return -1;
    }

    return elapsed_ns(&start, &end, total_ns);
}

/**
 *  @brief          ナノ秒値の配列を昇順に並べ替えます。
 *  @param[in,out]  values  並べ替える配列。NULL を渡してはなりません。
 *  @param[in]      count   要素数。
 *
 *  要素数が試行回数 (最大 @ref BENCH_TIMER_MAX_TRIALS) と小さいため、単純な挿入ソートを使用します。
 */
static void sort_ascending(uint64_t *values, size_t count)
{
    size_t index;

    for (index = 1U; index < count; index++)
    {
        uint64_t target = values[index];
        size_t position = index;

        while (position > 0U && values[position - 1U] > target)
        {
            values[position] = values[position - 1U];
            position--;
        }

        values[position] = target;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int bench_timer_measure(bench_iteration_fn fn, void *arg, uint64_t min_duration_ms, size_t trial_count,
                        uint64_t fixed_iterations, bench_timing *timing)
{
    uint64_t samples[BENCH_TIMER_MAX_TRIALS];
    uint64_t iterations = 1U;
    uint64_t threshold_ns;
    size_t trial;

    if (fn == NULL || timing == NULL || min_duration_ms == 0U || trial_count == 0U ||
        trial_count > (size_t)BENCH_TIMER_MAX_TRIALS)
    {
        return -1;
    }

    threshold_ns = min_duration_ms * (uint64_t)BENCH_NS_PER_MS;

    /* ウォーム アップ。初回のページ フォールトやメタデータ読み込みを測定から除く。 */
    if (fn(arg) != 0)
    {
        return -1;
    }

    if (fixed_iterations != 0U)
    {
        iterations = fixed_iterations;
    }
    else
    {
        /* 測定区間がクロック分解能に対して十分な長さになるまで反復回数を倍増する。 */
        for (;;)
        {
            uint64_t total_ns = 0U;

            if (run_block(fn, arg, iterations, &total_ns) != 0)
            {
                return -1;
            }

            if (total_ns >= threshold_ns)
            {
                break;
            }

            if (iterations >= (uint64_t)BENCH_TIMER_MAX_ITERATIONS)
            {
                break;
            }

            iterations = iterations * 2U;
        }
    }

    /* 確定した反復回数で試行し、1 反復あたりの所要時間を試料にする。 */
    for (trial = 0U; trial < trial_count; trial++)
    {
        uint64_t total_ns = 0U;

        if (run_block(fn, arg, iterations, &total_ns) != 0)
        {
            return -1;
        }

        samples[trial] = total_ns / iterations;
    }

    sort_ascending(samples, trial_count);

    timing->iterations = iterations;
    timing->min_ns = samples[0];
    timing->max_ns = samples[trial_count - 1U];
    timing->median_ns = samples[trial_count / 2U];

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int bench_timer_measure_cold(bench_iteration_fn fn, void *arg, bench_iteration_fn prepare, void *prepare_arg,
                             size_t trial_count, bench_timing *timing)
{
    uint64_t samples[BENCH_TIMER_MAX_TRIALS];
    size_t trial;

    if (fn == NULL || prepare == NULL || timing == NULL || trial_count == 0U ||
        trial_count > (size_t)BENCH_TIMER_MAX_TRIALS)
    {
        return -1;
    }

    for (trial = 0U; trial < trial_count; trial++)
    {
        uint64_t total_ns = 0U;

        if (prepare(prepare_arg) != 0)
        {
            return -1;
        }

        if (run_block(fn, arg, 1U, &total_ns) != 0)
        {
            return -1;
        }

        samples[trial] = total_ns;
    }

    sort_ascending(samples, trial_count);

    timing->iterations = 1U;
    timing->min_ns = samples[0];
    timing->max_ns = samples[trial_count - 1U];
    timing->median_ns = samples[trial_count / 2U];

    return 0;
}
