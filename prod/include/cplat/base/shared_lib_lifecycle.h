/**
 *******************************************************************************
 *  @file           shared_lib_lifecycle.h
 *  @brief          共有ライブラリのロード時とアンロード時に使用する共通フックを提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.1.0
 *
 *  このヘッダーをインクルードした .c ファイルに対して、プラットフォームごとの
 *  ライブラリ ロード・アンロード フックを提供します。\n
 *  `cplat` 本体はこの仕組みを使用しません。共有ライブラリ利用者が必要に応じて
 *  外部モジュール側で使用する補助ヘッダーです。
 *
 *  インクルード元の .c ファイルは以下の関数を定義する必要があります。
 *  - onLoad() : ライブラリ ロード時に呼び出される関数
 *  - onUnload(int process_terminating)
 *      process_terminating = 0: 明示アンロードまたは通常の destructor
 *      process_terminating = 1: Windows のプロセス終了による DETACH
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

#ifndef SHARED_LIB_LIFECYCLE_H
#define SHARED_LIB_LIFECYCLE_H

#include <cplat/base/platform.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_LINUX)
    #include <stdio.h>
    #include <string.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
    #include <cplat/crt/string.h>
    #include <cplat/test/syslog_test.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif /* PLATFORM_ */

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#ifdef DOXYGEN
    /**
     *  @brief          ロード/アンロード コンテキスト向けの診断メッセージ出力マクロです。
     *
     *  `DllMain` および constructor / destructor コンテキストでは
     *  使用できる API が制限されるため、このマクロは制約下でも比較的
     *  安全な最小限の出力経路を提供します。\n
     *  環境変数 `ENABLE_DLLMAIN_CPLAT_INFO_MSG` が設定されている
     *  場合にのみ出力します。未設定時は何も出力しません。\n
     *  Linux では、環境変数 `SYSLOG_TEST_FD` が設定されていれば
     *  その FD に RFC 3164 形式のメッセージを書き込みます。
     *  設定されていない場合は `/dev/log` へ UNIX ドメイン SOCK_DGRAM で
     *  RFC 3164 形式のメッセージを `MSG_DONTWAIT` で送信します。
     *  送信に失敗した場合はメッセージを drop します。\n
     *  Windows では `MultiByteToWideChar` で UTF-8 変換後に
     *  `OutputDebugStringW(...)` を使用します。
     *
     *  @param[in]      msg 出力する null 終端 UTF-8 文字列。
     *
     *  @warning        Windows では変換後のワイド文字列が 1024 文字を超える場合、
     *                  1019 文字で切り捨てて末尾に `" ..."` を付与して出力します。
     */
    #define CPLAT_DLLMAIN_INFO_MSG(msg)
#else /* !DOXYGEN */
    #if defined(PLATFORM_LINUX)
/**
 *  @brief  /dev/log へ RFC 3164 形式の INFO メッセージを非ブロッキングで送信します。
 *
 *  constructor / destructor コンテキストでも安全に使用できるよう、
 *  syslog() API は使用しません。毎回ソケットを開いて即時送信し、
 *  失敗時は破棄します。priority = LOG_USER(8) | LOG_INFO(6) = 14。
 */
static void dllmain_syslog_send(const char *msg)
{
    char buf[512];
    struct sockaddr_un sa;
    int fd;
    int n;
    int enabled = 0;

    if (cplat_getenv("ENABLE_DLLMAIN_CPLAT_INFO_MSG", NULL, 0, &enabled, NULL) != 0 || enabled == 0)
    {
        return;
    }

    /* priority 14 は facility LOG_USER(1<<3) | severity LOG_INFO(6) である。 */
    n = snprintf(buf, sizeof(buf), "<14>cplat[%d]: %s", (int)getpid(), msg);
    if (n <= 0)
    {
        return;
    }

    if ((size_t)n >= sizeof(buf))
    {
        n = (int)(sizeof(buf) - 1);
    }

    buf[n] = '\n';

    /* SYSLOG_TEST_FD が設定されていればテスト用 FD に送信し、/dev/log へは送信しない。 */
    if (syslog_test_fd_write(buf, (size_t)(n + 1)))
    {
        return;
    }

    fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        return;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;

    if (cplat_strcpy(sa.sun_path, sizeof(sa.sun_path), "/dev/log") != 0)
    {
        close(fd);
        return;
    }

    /* 送信失敗時はメッセージを破棄するため、戻り値を無視する。 */
    (void)sendto(fd, buf, (size_t)n, MSG_DONTWAIT, (struct sockaddr *)&sa, (socklen_t)sizeof(sa));
    close(fd);
}
        #define CPLAT_DLLMAIN_INFO_MSG(msg) dllmain_syslog_send(msg)
    #elif defined(PLATFORM_WINDOWS)
/*
 * UTF-8 の診断メッセージをワイド文字列へ変換し、OutputDebugStringW() で出力する。
 */
static void dllmain_output_debug_msg(const char *msg)
{
    wchar_t buf[1024];
    int len;
    int enabled = 0;

    if (cplat_getenv("ENABLE_DLLMAIN_CPLAT_INFO_MSG", NULL, 0, &enabled, NULL) != 0 || enabled == 0)
    {
        return;
    }

    len = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL, 0);
    if (len <= 0)
        return;

    if (len <= 1024)
    {
        /* バッファーに収まる場合はそのまま変換する。 */
        MultiByteToWideChar(CP_UTF8, 0, msg, -1, buf, 1024);
    }
    else
    {
        /*
         * バッファーを超える場合は、UTF-8 バイト列を走査して wchar_t 1019 個に収まるバイト数を求める。
         * 残りの wchar_t 4 個と NUL 終端で " ..." を付与する。
         */
        int byte_pos = 0;
        int wc_count = 0;
        int written;

        while (msg[byte_pos] != '\0')
        {
            unsigned char c = (unsigned char)msg[byte_pos];
            int cb;

            if (c < 0x80u)
            {
                cb = 1;
            }
            else if (c < 0xE0u)
            {
                cb = 2;
            }
            else if (c < 0xF0u)
            {
                cb = 3;
            }
            else
            {
                cb = 4;
            }

            int cw; /* U+10000 以上はサロゲート ペアで wchar_t 2 個 */

            if (cb == 4)
            {
                cw = 2;
            }
            else
            {
                cw = 1;
            }

            if (wc_count + cw > 1019)
                break;
            wc_count += cw;
            byte_pos += cb;
        }

        written = MultiByteToWideChar(CP_UTF8, 0, msg, byte_pos, buf, 1019);
        if (written <= 0)
            return;
        buf[written] = L' ';
        buf[written + 1] = L'.';
        buf[written + 2] = L'.';
        buf[written + 3] = L'.';
        buf[written + 4] = L'\0';
    }

    OutputDebugStringW(buf);
}
        #define CPLAT_DLLMAIN_INFO_MSG(msg) dllmain_output_debug_msg(msg)
    #endif /* PLATFORM_ */
#endif     /* DOXYGEN */

/**
 *  @brief          ライブラリ ロード時に呼び出されます。
 *
 *  インクルード元の .c ファイルでこの関数を定義してください。
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフではありません。\n
 *  ローダー コンテキストの単一スレッドから呼び出されます。
 */
static void onLoad(void);

/**
 *  @brief          ライブラリ アンロード時に呼び出されます。
 *  @param[in]      process_terminating プロセス終了による呼び出しの場合は 1、
 *                  明示的なアンロードまたは通常の destructor の場合は 0。
 *
 *  インクルード元の .c ファイルでこの関数を定義してください。
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフではありません。\n
 *  ローダー コンテキストの単一スレッドから呼び出されます。
 */
static void onUnload(int process_terminating);

#if defined(PLATFORM_LINUX)

/**
 *  @brief          Linux constructor 属性によるライブラリ ロード フックです。
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフではありません。\n
 *  constructor コンテキストの単一スレッドから呼び出されます。
 */
__attribute__((constructor)) static void dllmain_on_load(void)
{
    CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onLoad enter");
    onLoad();
    CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onLoad leave");
}

/**
 *  @brief          Linux destructor 属性によるライブラリ アンロード フックです。
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフではありません。\n
 *  destructor コンテキストの単一スレッドから呼び出されます。
 */
__attribute__((destructor)) static void dllmain_on_unload(void)
{
    CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onUnload enter");
    onUnload(0);
    CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onUnload leave");
}

#elif defined(PLATFORM_WINDOWS)

/**
 *  @brief          Windows DllMain によるライブラリ ロード/アンロード フックです。
 *  @param[in]      hinstDLL DLL のモジュール ハンドル。
 *  @param[in]      fdwReason 呼び出し理由 (DLL_PROCESS_ATTACH など)。
 *  @param[in]      lpvReserved 予約。プロセス終了時は非 NULL。
 *  @return         常に TRUE を返します。
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフではありません。\n
 *  OS がローダー ロックを保持した状態で単一スレッドから呼び出します。
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    (void)hinstDLL;

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onLoad enter");
        onLoad();
        CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onLoad leave");
        break;
    case DLL_PROCESS_DETACH:
        CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onUnload enter");
        {
            int process_terminating;

            /*
             * lpvReserved が非 NULL の場合はプロセス終了に伴うアンロードであり、
             * NULL の場合は FreeLibrary() による明示的なアンロードである。
             * see: https://learn.microsoft.com/en-us/windows/win32/dlls/dllmain
             */
            if (lpvReserved != NULL)
            {
                process_terminating = 1;
            }
            else
            {
                process_terminating = 0;
            }

            onUnload(process_terminating);
        }

        CPLAT_DLLMAIN_INFO_MSG("shared_lib_lifecycle: onUnload leave");
        break;
    default:
        break;
    }

    return TRUE;
}

#endif /* PLATFORM_ */

/** @} */

#endif /* SHARED_LIB_LIFECYCLE_H */
