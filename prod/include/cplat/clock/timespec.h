/**
 *******************************************************************************
 *  @file           timespec.h
 *  @brief          プラットフォーム互換の標準時刻型 cplat_timespec と時刻計算 API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/10
 *  @version        1.0.0
 *
 *  Linux x86-64 の `struct timespec` とバイナリ レイアウト互換の時刻型 cplat_timespec と、\n
 *  正規化・加減算・比較・ミリ秒変換などの時刻計算 API を提供します。
 *
 *  @section        timespec_layout レイアウト互換の方針
 *
 *  cplat_timespec は、x86-64 上で Linux の `struct timespec`
 *  (`time_t` 8 バイト + `long` 8 バイト = 16 バイト) と同一のバイナリ レイアウトを持ちます。\n
 *  Windows の UCRT `struct timespec` は `tv_nsec` が `long` (32 ビット) でレイアウトが異なるため、
 *  OS を問わず同一表現となる本型を標準時刻型として使用します。
 *
 *  ネイティブ `struct timespec` を要求する OS API (pthread の absolute deadline など) との境界では、
 *  cplat_timespec_to_native() / cplat_timespec_from_native() で変換します。\n
 *  変換関数以外の箇所で `struct timespec` とのキャスト・混用を行ってはなりません。
 *
 *  @section        timespec_usage 使い分けの指針
 *
 *  - **時刻の保持・受け渡し** → cplat_timespec を使用する。\n
 *    絶対時刻 (cplat_clock_get_realtime()) にも単調クロック値 (cplat_clock_get_monotonic()) にも使用します。
 *  - **加減算・比較** → cplat_timespec_add() / cplat_timespec_sub() / cplat_timespec_cmp() を使用する。\n
 *    手動での秒・ナノ秒演算は正規化漏れの原因となるため行いません。
 *  - **ミリ秒ベースの deadline / 経過時間** → cplat_timespec_add_ms() / cplat_timespec_diff_ms() を使用する。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef TIMESPEC_H
#define TIMESPEC_H

#include <cplat/base/platform.h>
#include <cplat/cplat_export.h>
#include <stdint.h>
#include <time.h>

/**
 *  @ingroup        CPLAT_CLOCK
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          プラットフォーム互換の標準時刻型です。
     *
     *  Linux x86-64 の `struct timespec` とバイナリ レイアウト互換 (16 バイト) の時刻構造体です。\n
     *  絶対時刻 (Unix epoch 基準) にも単調クロック値にも使用でき、意味は値を生成した API 側で決まります。
     *
     *  @note           `tv_nsec` は 0 以上 999,999,999 以下に正規化して扱います。\n
     *                  演算後の正規化には cplat_timespec_normalize() を使用してください。
     */
    typedef struct cplat_timespec
    {
        time_t tv_sec; /**< 秒部。絶対時刻の場合は Unix epoch (1970-01-01T00:00:00Z) からの経過秒。 */
        /* tv_nsec は Linux x86-64 の struct timespec::tv_nsec (long, 64 ビット) と
           バイナリ レイアウトを一致させるため、コーディング規範の例外として固定幅型 int64_t を使用する。 */
        int64_t tv_nsec; /**< ナノ秒部 (0 以上 999,999,999 以下)。 */
    } cplat_timespec;

    /**
     *  @brief          時刻値を正規化します。
     *  @param[in,out]  ts  正規化対象。NULL の場合は無処理で戻ります。
     *
     *  `tv_nsec` が 0 以上 999,999,999 以下に収まるよう、秒部との繰り上げ・繰り下げを行います。\n
     *  `tv_nsec` が負の場合も正の値へ正規化します (例: {1, -1} は {0, 999999999} になります)。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_timespec_normalize(cplat_timespec *ts);

    /**
     *  @brief          2 つの時刻値の和を求めます (result = a + b)。
     *  @param[in]      a       加算元。NULL の場合は無処理で戻ります。
     *  @param[in]      b       加算値。NULL の場合は無処理で戻ります。
     *  @param[out]     result  正規化済みの和の格納先。NULL の場合は無処理で戻ります。@p a / @p b と同一でも構いません。
     *
     *  絶対時刻に相対時間を加算して deadline を作る用途などに使用します。\n
     *  結果は正規化済み (`tv_nsec` が 0 以上 999,999,999 以下) です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_timespec_add(const cplat_timespec *a, const cplat_timespec *b,
                                                   cplat_timespec *result);

    /**
     *  @brief          2 つの時刻値の差を求めます (result = a - b)。
     *  @param[in]      a       被減算値。NULL の場合は無処理で戻ります。
     *  @param[in]      b       減算値。NULL の場合は無処理で戻ります。
     *  @param[out]     result  正規化済みの差の格納先。NULL の場合は無処理で戻ります。@p a / @p b と同一でも構いません。
     *
     *  経過時間の算出などに使用します。\n
     *  結果は正規化済みであり、@p a が @p b より過去の場合は `tv_sec` が負になります
     *  (`tv_nsec` は常に 0 以上 999,999,999 以下です)。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_timespec_sub(const cplat_timespec *a, const cplat_timespec *b,
                                                   cplat_timespec *result);

    /**
     *  @brief          2 つの時刻値を比較します。
     *  @param[in]      a  比較対象。NULL を指定できます。
     *  @param[in]      b  比較対象。NULL を指定できます。
     *  @return         @p a が過去なら -1、等しければ 0、@p a が未来なら 1。
     *
     *  秒部を先に比較し、同値の場合のみナノ秒部を比較します。\n
     *  比較前に両者が正規化済みであることが前提です。\n
     *  @p a または @p b が NULL の場合は 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_timespec_cmp(const cplat_timespec *a, const cplat_timespec *b);

    /**
     *  @brief          時刻値へミリ秒を加算します (result = ts + timeout_ms)。
     *  @param[in]      ts          加算元。NULL の場合は無処理で戻ります。
     *  @param[in]      timeout_ms  加算するミリ秒値。
     *  @param[out]     result      正規化済みの和の格納先。NULL の場合は無処理で戻ります。@p ts と同一でも構いません。
     *
     *  タイムアウトの absolute deadline を生成する用途などに使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_timespec_add_ms(const cplat_timespec *ts, uint64_t timeout_ms,
                                                      cplat_timespec *result);

    /**
     *  @brief          2 つの時刻値の差をミリ秒で求めます (end - start)。
     *  @param[in]      end    終点の時刻値。NULL を指定できます。
     *  @param[in]      start  起点の時刻値。NULL を指定できます。
     *  @return         経過ミリ秒 (ミリ秒未満は切り捨て)。@p end が @p start より過去の場合は負値。
     *
     *  経過時間の測定やタイムアウト判定に使用します。\n
     *  ミリ秒未満の端数は数直線上の負方向へ切り捨てます (床関数)。\n
     *  @p end または @p start が NULL の場合は 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int64_t CPLAT_API cplat_timespec_diff_ms(const cplat_timespec *end, const cplat_timespec *start);

    /**
     *  @brief          cplat_timespec をネイティブの `struct timespec` へ変換します。
     *  @param[in]      ts      変換元。NULL の場合は無処理で戻ります。
     *  @param[out]     native  変換結果の格納先。NULL の場合は無処理で戻ります。
     *
     *  pthread の absolute deadline など、OS API が要求する `struct timespec` を生成する境界で使用します。\n
     *  Windows では `struct timespec::tv_nsec` が `long` (32 ビット) のため縮小変換になりますが、
     *  正規化済みの `tv_nsec` (0 以上 999,999,999 以下) は表現範囲内であり値は保たれます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_timespec_to_native(const cplat_timespec *ts, struct timespec *native);

    /**
     *  @brief          ネイティブの `struct timespec` を cplat_timespec へ変換します。
     *  @param[in]      native  変換元。NULL の場合は無処理で戻ります。
     *  @param[out]     ts      変換結果の格納先。NULL の場合は無処理で戻ります。
     *
     *  OS API から受け取った `struct timespec` を標準時刻型へ取り込む境界で使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_timespec_from_native(const struct timespec *native, cplat_timespec *ts);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* TIMESPEC_H */
