/**
 *******************************************************************************
 *  @file           syslog_test.h
 *  @brief          syslog のテストを補助する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/05
 *  @version        1.0.0
 *
 *  テスト実行時に環境変数 `SYSLOG_TEST_FD` が設定されていれば、
 *  syslog 経路のメッセージをその FD に書き込みます。\n
 *  `shared_lib_lifecycle.h` を利用する外部モジュールからインクルードして使用します。
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

#ifndef SYSLOG_TEST_H
#define SYSLOG_TEST_H

#include <cplat/base/platform.h>
#include <cplat/cplat_export.h>

#if defined(PLATFORM_LINUX)

    #include <stdlib.h>
    #include <unistd.h>

    #ifdef __cplusplus
extern "C"
{
    #endif

    /**
     *  @ingroup        CPLAT_TEST
     *  @{
     */

    /**
     *  @brief          SYSLOG_TEST_FD 環境変数が設定されていればテスト用 FD へ書き込み、通常送信の抑止を通知します。
     *
     *  `SYSLOG_TEST_FD` が設定されている場合は、テスト FD への `write()` 成否や変換結果にかかわらず
     *  1 を返します。\n
     *  戻り値 1 はテスト FD への書き込み成功ではなく、実 `/dev/log` への送信を抑止する指示です。
     *  @param[in]      buf     送信するバッファー (呼び出し元で整形済みの 1 行分データ)。
     *  @param[in]      nbytes  送信バイト数。
     *  @return         `SYSLOG_TEST_FD` が設定されており、通常の `/dev/log` 送信を抑止する場合は 1 を返します。\n
     *                  環境変数が未設定で、通常の `/dev/log` 送信を続行する場合は 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  環境変数 `SYSLOG_TEST_FD` を参照します。\n
     *  他スレッドが環境変数を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に環境変数を変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_syslog_test_fd_write(const char *buf, size_t nbytes);
    #ifdef __cplusplus
}
    #endif

    /** @} */

#endif /* PLATFORM_LINUX */

#endif /* SYSLOG_TEST_H */
