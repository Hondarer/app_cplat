/**
 *******************************************************************************
 *  @file           clock_internal.h
 *  @brief          clock モジュール内で共有する時刻単位換算定数を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/22
 *  @version        1.0.0
 *
 *  本ヘッダーは `prod/libsrc/cplat/clock/` のモジュール私有ヘッダーです。\n
 *  同一ディレクトリ内の実装ファイルからのみ `#include "clock_internal.h"` で取り込みます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CLOCK_INTERNAL_H
#define CLOCK_INTERNAL_H

/** 1 秒あたりのミリ秒数 [ms/s] です。 */
#define CLOCK_MSEC_PER_SEC (1000LL)

/** 1 ミリ秒あたりのナノ秒数 [ns/ms] です。 */
#define CLOCK_NSEC_PER_MSEC (1000000LL)

/** 1 秒あたりのナノ秒数 [ns/s] です。 */
#define CLOCK_NSEC_PER_SEC (CLOCK_MSEC_PER_SEC * CLOCK_NSEC_PER_MSEC)

#endif /* CLOCK_INTERNAL_H */
