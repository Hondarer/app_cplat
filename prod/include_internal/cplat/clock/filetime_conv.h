/**
 *******************************************************************************
 *  @file           filetime_conv.h
 *  @brief          Windows の FILETIME と cplat_timespec を相互変換する内部 API を提供します。
 *
 *  FILETIME は 1601-01-01T00:00:00Z を起点とする 100 [ns] 単位の値です。
 *  Unix epoch (1970-01-01T00:00:00Z) を起点とする cplat_timespec との間で、
 *  起点の差と単位の差を吸収します。
 *
 *  変換は Windows でのみ必要なため、本ヘッダーは PLATFORM_WINDOWS でのみ内容を持ちます。
 *  実体を持つ翻訳単位を増やさないよう、変換は static inline 関数として提供します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは複数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_CLOCK_FILETIME_CONV_INTERNAL_H
#define CPLAT_CLOCK_FILETIME_CONV_INTERNAL_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/compiler.h>
    #include <cplat/base/windows_sdk.h>
    #include <cplat/clock/timespec.h>
    #include <stdint.h>

    #define CPLAT_FILETIME_UNITS_PER_SEC    (10000000ULL)   /**< FILETIME 単位 (100 [ns]) / 秒。 */
    #define CPLAT_NSEC_PER_FILETIME_UNIT    (100ULL)        /**< ナノ秒 / FILETIME 単位。 */
    #define CPLAT_FILETIME_EPOCH_OFFSET_SEC (11644473600LL) /**< 1601-01-01 → 1970-01-01 の差 [s]。 */

/**
 *  @brief          FILETIME を cplat_timespec へ変換します。
 *  @param[in]      filetime  変換元。NULL を渡してはなりません。
 *  @param[out]     ts        変換結果の格納先。NULL を渡してはなりません。
 *
 *  1601-01-01 起点の 100 [ns] 単位を、Unix epoch 起点の秒とナノ秒へ分解します。
 */
static inline void cplat_internal_filetime_to_timespec(const FILETIME *filetime, cplat_timespec *ts)
{
    ULARGE_INTEGER uli;

    uli.LowPart = filetime->dwLowDateTime;
    uli.HighPart = filetime->dwHighDateTime;

    ts->tv_sec = (time_t)(uli.QuadPart / CPLAT_FILETIME_UNITS_PER_SEC) - CPLAT_FILETIME_EPOCH_OFFSET_SEC;
    ts->tv_nsec = (int64_t)((uli.QuadPart % CPLAT_FILETIME_UNITS_PER_SEC) * CPLAT_NSEC_PER_FILETIME_UNIT);
}

/**
 *  @brief          cplat_timespec を FILETIME へ変換します。
 *  @param[in]      ts            変換元。NULL を渡してはなりません。
 *  @param[out]     filetime_out  変換結果の格納先。NULL を渡してはなりません。
 *
 *  ナノ秒部は 100 [ns] 単位へ切り捨てます。FILETIME の分解能が 100 [ns] であるためです。
 *
 *  @attention      Unix epoch より前の時刻のうち、FILETIME の起点 (1601-01-01) より前になる値は
 *                  表現できません。この場合は 0 を格納します。
 */
static inline void cplat_internal_timespec_to_filetime(const cplat_timespec *ts, FILETIME *filetime_out)
{
    const int64_t filetime_sec = (int64_t)ts->tv_sec + CPLAT_FILETIME_EPOCH_OFFSET_SEC;
    ULARGE_INTEGER uli;

    if (filetime_sec < 0)
    {
        filetime_out->dwLowDateTime = 0;
        filetime_out->dwHighDateTime = 0;
        return;
    }

    uli.QuadPart = (ULONGLONG)filetime_sec * CPLAT_FILETIME_UNITS_PER_SEC;
    uli.QuadPart += (ULONGLONG)(ts->tv_nsec / (int64_t)CPLAT_NSEC_PER_FILETIME_UNIT);

    filetime_out->dwLowDateTime = uli.LowPart;
    filetime_out->dwHighDateTime = uli.HighPart;
}

#endif /* PLATFORM_WINDOWS */

#endif /* CPLAT_CLOCK_FILETIME_CONV_INTERNAL_H */
