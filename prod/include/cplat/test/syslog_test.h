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

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef SYSLOG_TEST_H
#define SYSLOG_TEST_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <stdlib.h>
    #include <unistd.h>

/**
 *  @ingroup        CPLAT_TEST
 *  @{
 */

/**
 *  @brief          SYSLOG_TEST_FD 環境変数が設定されていれば、その FD にバッファーを書き込みます。
 *
 *  テスト用パイプ FD への送信のみを行います。\n
 *  呼び出し元は戻り値が 1 の場合、/dev/log への送信をスキップしてください。
 *  @param[in]      buf     送信するバッファー (呼び出し元で整形済みの 1 行分データ)。
 *  @param[in]      nbytes  送信バイト数。
 *  @return         テスト FD に書き込み、/dev/log への送信を省略する場合は 1 を返します。\n
 *                  環境変数が未設定で、通常の /dev/log 送信を続行する場合は 0 を返します。
 *
 *  @par            スレッド セーフ
 *  本関数は条件付きスレッド セーフです。\n
 *  環境変数 `SYSLOG_TEST_FD` を参照します。\n
 *  他スレッドが環境変数を同時に変更しない場合は、同時に実行できます。\n
 *  他スレッドが同時に環境変数を変更する場合は、呼び出し側で同期してください。
 */
static int syslog_test_fd_write(const char *buf, size_t nbytes)
{
    const char *fd_str = getenv("SYSLOG_TEST_FD");
    int test_fd;

    if (fd_str == NULL)
    {
        return 0;
    }

    test_fd = atoi(fd_str);
    if (test_fd >= 0)
    {
        (void)write(test_fd, buf, nbytes);
    }
    return 1;
}

    /** @} */

#endif /* PLATFORM_LINUX */

#endif /* SYSLOG_TEST_H */
