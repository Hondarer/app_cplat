/**
 *******************************************************************************
 *  @file           trace_common.h
 *  @brief          トレース機能の共通ヘルパー (内部共有) を宣言します。
 *
 *  tracer 本体と各バックエンド (file/syslog など) で共通の、
 *  タイムスタンプ解決とトレース レベル表現の変換を提供します。
 *  実装は trace_common.c の 1 箇所に集約します。
 *******************************************************************************
 */
#ifndef CPLAT_TRACE_COMMON_H
#define CPLAT_TRACE_COMMON_H

#include <stddef.h>

#include <cplat/clock/clock.h>
#include <cplat/trace/tracer.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     *  @brief          トレース出力に使用するタイムスタンプを解決します。
     *  @param[in]      timestamp          呼び出し側が渡した明示タイムスタンプ。NULL 可。
     *  @param[out]     resolved_out       解決後のタイムスタンプ格納先。
     *  @param[out]     fallback_used_out  不正タイムスタンプから現在時刻へ代替した場合 1。NULL 可。
     *  @return         成功時は CPLAT_OK、出力先が NULL の場合は CPLAT_ERR_INVALID_ARGUMENT、
     *                  取得した時刻が不正な場合は CPLAT_ERR_UNKNOWN、
     *                  時刻取得の失敗時は取得関数の結果コードを返します。
     *
     *  timestamp が有効な場合はそれを、NULL または不正な場合は現在時刻を
     *  resolved_out へ格納します。不正な値からの代替時のみ fallback_used_out に 1 を
     *  設定します (NULL は代替と見なさない)。
     */
    int cplat_internal_trace_resolve_timestamp(const cplat_timespec *timestamp, cplat_timespec *resolved_out,
                                               int *fallback_used_out);

    /**
     *  @brief          実時刻を ISO 8601 ローカル時刻文字列としてバッファーへ書き込みます。
     *  @param[out]     dest       書き込み先バッファー。
     *  @param[in]      dest_size  バッファーのバイト数 (CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1 以上を推奨)。
     *  @param[in]      timestamp  使用する実時刻。
     *  @return         タイムスタンプ不正時は CPLAT_ERR_INVALID_ARGUMENT を返します。それ以外の場合は
     *                  cplat_clock_format_realtime_iso8601_local() の戻り値を返します。
     */
    int cplat_internal_trace_format_local_timestamp(char *dest, size_t dest_size, const cplat_timespec *timestamp);

    /**
     *  @brief          トレース レベルをレベル文字に変換します。
     *  @param[in]      level  変換元のトレース レベル。
     *  @return         対応するレベル文字 ('C'/'E'/'W'/'I'/'V'/'D')。範囲外は 'D'。
     */
    char cplat_internal_trace_level_char(cplat_trace_level level);

#ifdef __cplusplus
}
#endif

#endif /* CPLAT_TRACE_COMMON_H */
