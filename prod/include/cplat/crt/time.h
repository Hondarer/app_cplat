/**
 *******************************************************************************
 *  @file           time.h
 *  @brief          time 系の CRT 関数を抽象化する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/22
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

#ifndef CPLAT_CRT_TIME_H
#define CPLAT_CRT_TIME_H

#include <stddef.h>
#include <time.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          `gmtime_r` / `gmtime_s` のラッパー。UTC 時刻を変換します。
     *  @param[out]     utc_tm  変換結果の格納先。NULL を渡してはなりません。
     *  @param[in]      timep   変換するエポック秒へのポインター。NULL を渡してはなりません。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p utc_tm または @p timep が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         変換処理に失敗した場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_gmtime(struct tm *utc_tm, const time_t *timep);

    /**
     *  @brief          `localtime_r` / `localtime_s` のラッパー。ローカル時刻を変換します。
     *  @param[out]     local_tm  変換結果の格納先。NULL を渡してはなりません。
     *  @param[in]      timep     変換するエポック秒へのポインター。NULL を渡してはなりません。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p local_tm または @p timep が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         変換処理に失敗した場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_localtime(struct tm *local_tm, const time_t *timep);

    /**
     *  @brief          `ctime_r` / `ctime_s` のラッパー。エポック秒をローカル時刻の文字列へ変換します。
     *  @param[out]     buf       変換結果の格納先。NULL を渡してはなりません。
     *  @param[in]      buf_size  @p buf のサイズ [byte]。26 以上が必要です。
     *  @param[in]      timep     変換するエポック秒へのポインター。NULL を渡してはなりません。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p buf または @p timep が NULL の場合、または @p buf_size が 26 未満の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         変換処理に失敗した場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  変換結果は `Wed Jul  8 21:49:08 2026\n` 形式 (改行と終端文字を含む) の
     *  ローカル時刻文字列です。\n
     *  失敗時は @p buf の全体をゼロ化します (@p buf が NULL の場合を除く)。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_ctime(char *buf, size_t buf_size, const time_t *timep);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRT_TIME_H */
