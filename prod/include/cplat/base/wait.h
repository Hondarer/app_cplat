/**
 *******************************************************************************
 *  @file           wait.h
 *  @brief          期限付きの待機関数へ渡す待ち時間の定数を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/15
 *  @version        1.0.0
 *
 *  cplat の待機 API は、無期限に待つ関数と期限付きで待つ関数 (`*_timed_*`) を分けています。\n
 *  本ヘッダーの定数は、期限付きの関数の `timeout_ms` に渡す特別な値です。\n
 *  呼び出し元から受け取った待ち時間をそのまま渡す関数が、値によって呼び出す関数を
 *  切り替えずに済むよう、期限付きの関数は無期限と即時の指定も受け付けます。
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

#ifndef CPLAT_BASE_WAIT_H
#define CPLAT_BASE_WAIT_H

#include <limits.h>

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

/** 期限付きの待機関数で、期限なしに待つことを表す値 (INT_MAX)。 */
#define CPLAT_WAIT_FOREVER INT_MAX

/** 期限付きの待機関数で、待たずに戻ることを表す値 (0 [ms])。 */
#define CPLAT_NO_WAIT 0

/** @} */

#endif /* CPLAT_BASE_WAIT_H */
