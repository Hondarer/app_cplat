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
 *  (`time_t` 8 [byte] + `long` 8 [byte] = 16 [byte]) と同一のバイナリ レイアウトを持ちます。\n
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

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef TIMESPEC_H
#define TIMESPEC_H

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
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
     *  Linux x86-64 の `struct timespec` とバイナリ レイアウト互換 (16 [byte]) の時刻構造体です。\n
     *  絶対時刻 (Unix epoch 基準) にも単調クロック値にも使用でき、意味は値を生成した API 側で決まります。
     *
     *  @note           `tv_nsec` は 0 以上 999,999,999 以下に正規化して扱います。\n
     *                  演算後の正規化には cplat_timespec_normalize() を使用してください。
     */
    typedef struct cplat_timespec
    {
        time_t tv_sec; /**< 秒部。絶対時刻の場合は Unix epoch (1970-01-01T00:00:00Z) からの経過秒。 */

        /*
         * tv_nsec は Linux x86-64 の struct timespec::tv_nsec (long, 64 ビット) と
         * バイナリ レイアウトを一致させるため、コーディング規範の例外として固定幅型 int64_t を使用する。
         */
        int64_t tv_nsec; /**< ナノ秒部 (0 以上 999,999,999 以下)。 */
    } cplat_timespec;

    /**
     *  @brief          時刻値を正規化します。
     *  @param[in,out]  ts                正規化する時刻値。NULL は許可しません。
     *  @retval         CPLAT_OK                   計算に成功しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT 必須ポインターが NULL です。
     *  @retval         CPLAT_ERR_OUT_OF_RANGE     結果を表現できません。
     *
     *  ナノ秒部を 0 以上 999,999,999 以下へ正規化します。秒部の範囲を超える場合は変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_timespec_normalize(cplat_timespec *ts);

    /**
     *  @brief          2 つの時刻値の和を求めます。
     *  @param[in]      a                 加算元。NULL は許可しません。
     *  @param[in]      b                 加算値。NULL は許可しません。
     *  @param[out]     sum_out           正規化した和。a または b と同一でも構いません。NULL は許可しません。
     *  @retval         CPLAT_OK                   計算に成功しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT 必須ポインターが NULL です。
     *  @retval         CPLAT_ERR_OUT_OF_RANGE     結果を表現できません。
     *
     *  秒・ナノ秒を合成し、数学的な和を正規化して格納します。失敗時は出力を変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_timespec_add(const cplat_timespec *a, const cplat_timespec *b,
                                                  cplat_timespec *sum_out);

    /**
     *  @brief          2 つの時刻値の差を求めます。
     *  @param[in]      a                 被減算値。NULL は許可しません。
     *  @param[in]      b                 減算値。NULL は許可しません。
     *  @param[out]     difference_out    正規化した差。a または b と同一でも構いません。NULL は許可しません。
     *  @retval         CPLAT_OK                   計算に成功しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT 必須ポインターが NULL です。
     *  @retval         CPLAT_ERR_OUT_OF_RANGE     結果を表現できません。
     *
     *  a から b を引いた差を正規化して格納します。失敗時は出力を変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_timespec_sub(const cplat_timespec *a, const cplat_timespec *b,
                                                  cplat_timespec *difference_out);

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
     *  @brief          時刻値へミリ秒を加算します。
     *  @param[in]      ts                加算元。NULL は許可しません。
     *  @param[in]      timeout_ms        加算する時間 [ms]。
     *  @param[out]     sum_out           正規化した和。ts と同一でも構いません。NULL は許可しません。
     *  @retval         CPLAT_OK                   計算に成功しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT 必須ポインターが NULL です。
     *  @retval         CPLAT_ERR_OUT_OF_RANGE     結果を表現できません。
     *
     *  失敗時は出力を変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_timespec_add_ms(const cplat_timespec *ts, uint64_t timeout_ms,
                                                     cplat_timespec *sum_out);

    /**
     *  @brief          2 つの時刻値の差をミリ秒で求めます。
     *  @param[in]      end               終点。NULL は許可しません。
     *  @param[in]      start             起点。NULL は許可しません。
     *  @param[out]     milliseconds_out  end - start の差 [ms]。NULL は許可しません。
     *  @retval         CPLAT_OK                   計算に成功しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT 必須ポインターが NULL です。
     *  @retval         CPLAT_ERR_OUT_OF_RANGE     結果を表現できません。
     *
     *  ミリ秒未満は負方向へ切り捨てます。正常な差 0 と失敗は戻り値で区別します。失敗時は出力を変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_timespec_diff_ms(const cplat_timespec *end, const cplat_timespec *start,
                                                      int64_t *milliseconds_out);

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
