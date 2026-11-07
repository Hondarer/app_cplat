/**
 *******************************************************************************
 *  @file           bench_timer.h
 *  @brief          反復回数を自動調整して 1 反復あたりの所要時間を測定する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/29
 *  @version        1.0.0
 *
 *  Windows の単調増加クロックは `GetTickCount64()` に基づき分解能が約 15 [ms] であるため、
 *  1 回の測定区間が指定時間以上になるまで反復回数を倍増させ、
 *  クロック分解能の影響を相対的に小さくします。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef BENCH_TIMER_PRIVATE_H
#define BENCH_TIMER_PRIVATE_H

#include <stddef.h>
#include <stdint.h>

/**
 *  @brief  測定結果です。
 */
typedef struct bench_timing
{
    uint64_t iterations; /**< 1 試行あたりの反復回数。 */
    uint64_t median_ns;  /**< 1 反復あたりの所要時間の中央値 [ns]。 */
    uint64_t min_ns;     /**< 1 反復あたりの所要時間の最小値 [ns]。 */
    uint64_t max_ns;     /**< 1 反復あたりの所要時間の最大値 [ns]。 */
} bench_timing;

/**
 *  @brief          測定対象を 1 反復ぶん実行するコールバックです。
 *  @param[in,out]  arg  呼び出し側が指定した任意のデータ。
 *  @return         成功時は 0、失敗時は 0 以外を返します。
 */
typedef int (*bench_iteration_fn)(void *arg);

/** @brief 1 試行あたりの反復回数の上限です。 */
#define BENCH_TIMER_MAX_ITERATIONS (1024U * 1024U * 64U)

/** @brief 1 ケースあたりの試行回数の上限です。 */
#define BENCH_TIMER_MAX_TRIALS 32

/**
 *  @brief          測定対象を繰り返し実行し、1 反復あたりの所要時間を求めます。
 *  @param[in]      fn               測定対象を 1 反復ぶん実行するコールバック。NULL を渡してはなりません。
 *  @param[in,out]  arg              @p fn へ渡すデータ。
 *  @param[in]      min_duration_ms  1 試行の測定区間の下限 [ms]。1 以上を指定します。
 *  @param[in]      trial_count      試行回数。1 以上 @ref BENCH_TIMER_MAX_TRIALS 以下を指定します。
 *  @param[in]      fixed_iterations 反復回数を固定する場合はその値、自動調整する場合は 0 を指定します。
 *  @param[out]     timing           測定結果の格納先。NULL を渡してはなりません。
 *  @return         成功時は 0、引数不正または @p fn の失敗時は -1 を返します。
 *
 *  最初にウォーム アップとして @p fn を 1 回実行します (この結果は測定に含めません)。\n
 *  続いて、測定区間が @p min_duration_ms 以上になるまで反復回数を 1 から倍増させ、
 *  確定した反復回数で @p trial_count 回の試行を行います。\n
 *  @p fixed_iterations に 0 以外を指定した場合は倍増を行わず、その値を反復回数として使用します。
 */
int bench_timer_measure(bench_iteration_fn fn, void *arg, uint64_t min_duration_ms, size_t trial_count,
                        uint64_t fixed_iterations, bench_timing *timing);

/**
 *  @brief          試行ごとに前処理を挟み、1 反復だけを測定します。
 *  @param[in]      fn           測定対象を 1 反復ぶん実行するコールバック。NULL を渡してはなりません。
 *  @param[in,out]  arg          @p fn へ渡すデータ。
 *  @param[in]      prepare      各試行の直前に実行するコールバック。NULL を渡してはなりません。
 *  @param[in,out]  prepare_arg  @p prepare へ渡すデータ。
 *  @param[in]      trial_count  試行回数。1 以上 @ref BENCH_TIMER_MAX_TRIALS 以下を指定します。
 *  @param[out]     timing       測定結果の格納先。NULL を渡してはなりません。
 *  @return         成功時は 0、引数不正または各コールバックの失敗時は -1 を返します。
 *
 *  ページ キャッシュを破棄した状態 (cold) の測定に使用します。\n
 *  反復の繰り返しもウォーム アップも行わないため、1 反復の所要時間がクロック分解能に対して
 *  十分に長いことを呼び出し側で確認してください。
 */
int bench_timer_measure_cold(bench_iteration_fn fn, void *arg, bench_iteration_fn prepare, void *prepare_arg,
                             size_t trial_count, bench_timing *timing);

#endif /* BENCH_TIMER_PRIVATE_H */
