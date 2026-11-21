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

#include <assert.h>
#include <string.h>

#include <cplat/base/char_code.h>
#include <cplat/base/platform.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_LINUX)
    #include <stdio.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <syslog.h>
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
        /* DllMain / destructor 向け syslog 送信バッファーの容量 [byte] (終端改行を含む)。制約: 1 以上とすること。変更時は切り詰め長だけが変わる。 */
        #define CPLAT_DLLMAIN_SYSLOG_BUF_SIZE 512U

        /* RFC 3164 の PRI 値 (LOG_USER | LOG_INFO)。 */
        #define CPLAT_DLLMAIN_SYSLOG_PRI      (LOG_USER | LOG_INFO)

/**
 *  @brief  /dev/log へ RFC 3164 形式の INFO メッセージを非ブロッキングで送信します。
 *
 *  constructor / destructor コンテキストでも安全に使用できるよう、
 *  syslog() API は使用しません。毎回ソケットを開いて即時送信し、
 *  失敗時は破棄します。priority = LOG_USER(8) | LOG_INFO(6) = 14。
 */
static void dllmain_syslog_send(const char *msg)
{
    char buf[CPLAT_DLLMAIN_SYSLOG_BUF_SIZE];
    struct sockaddr_un sa;
    int fd;
    int n;
    int enabled = 0;

    if (cplat_getenv("ENABLE_DLLMAIN_CPLAT_INFO_MSG", NULL, 0, &enabled, NULL) != 0 || enabled == 0)
    {
        return;
    }

    /* priority は facility LOG_USER | severity LOG_INFO である。 */
    n = snprintf(buf, sizeof(buf), "<%d>cplat[%d]: %s", CPLAT_DLLMAIN_SYSLOG_PRI, (int)getpid(), msg);
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
    if (cplat_syslog_test_fd_write(buf, (size_t)(n + 1)))
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
        (void)close(fd); /* 送信を諦める経路の後始末であり、失敗を伝える先がないため、戻り値は明示的に破棄する */
        return;
    }

    /* 送信失敗時はメッセージを破棄するため、戻り値は明示的に破棄する。 */
    (void)sendto(fd, buf, (size_t)n, MSG_DONTWAIT, (struct sockaddr *)&sa, (socklen_t)sizeof(sa));
    (void)close(fd); /* 送信後の後始末であり、失敗を伝える先がないため、戻り値は明示的に破棄する */
}
        #define CPLAT_DLLMAIN_INFO_MSG(msg)   dllmain_syslog_send(msg)
    #elif defined(PLATFORM_WINDOWS)
        /*
         * OutputDebugStringW へ渡す診断メッセージ バッファーの要素数 [wchar_t 要素数] (終端 NUL を含む)。
         * 制約: 接尾辞長 + 2 以上とすること。変更時は出力の切り詰め上限だけが変わる。
         */
        #define CPLAT_DLLMAIN_DEBUG_BUF_LEN       1024U

        /* バッファー超過時に末尾へ付与する省略記号。 */
        #define CPLAT_DLLMAIN_TRUNCATE_SUFFIX     L" ..."

        /* 省略記号の wchar_t 要素数 (終端 NUL を除く)。 */
        #define CPLAT_DLLMAIN_TRUNCATE_SUFFIX_LEN ((sizeof(CPLAT_DLLMAIN_TRUNCATE_SUFFIX) / sizeof(wchar_t)) - 1U)

        /* 切り詰め時に本文として変換する最大 wchar_t 要素数。 */
        #define CPLAT_DLLMAIN_DEBUG_TRUNCATE_MAX  (CPLAT_DLLMAIN_DEBUG_BUF_LEN - CPLAT_DLLMAIN_TRUNCATE_SUFFIX_LEN - 1U)

        /* 補助平面のコード ポイント 1 文字が占める UTF-16 の wchar_t 要素数。 */
        #define CPLAT_DLLMAIN_SURROGATE_WCHAR_COUNT 2

static_assert(CPLAT_DLLMAIN_DEBUG_BUF_LEN > (CPLAT_DLLMAIN_TRUNCATE_SUFFIX_LEN + 1U),
              "cplat: debug buffer length must exceed truncate suffix");

/*
 * UTF-8 の診断メッセージをワイド文字列へ変換し、OutputDebugStringW() で出力する。
 */
static void dllmain_output_debug_msg(const char *msg)
{
    wchar_t buf[CPLAT_DLLMAIN_DEBUG_BUF_LEN];
    int len;
    int enabled = 0;

    if (cplat_getenv("ENABLE_DLLMAIN_CPLAT_INFO_MSG", NULL, 0, &enabled, NULL) != 0 || enabled == 0)
    {
        return;
    }

    len = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL, 0);
    if (len <= 0)
    {
        return;
    }

    if (len <= (int)CPLAT_DLLMAIN_DEBUG_BUF_LEN)
    {
        /* バッファーに収まる場合はそのまま変換する。変換に失敗した場合は、未初期化の buf を出力しない。 */
        if (MultiByteToWideChar(CP_UTF8, 0, msg, -1, buf, (int)CPLAT_DLLMAIN_DEBUG_BUF_LEN) <= 0)
        {
            return;
        }
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

            if (c < CPLAT_ASCII_BOUND_U8)
            {
                cb = (int)CPLAT_UTF8_BYTES_1;
            }
            else if (c < CPLAT_UTF8_LEAD3_MIN)
            {
                cb = (int)CPLAT_UTF8_BYTES_2;
            }
            else if (c < CPLAT_UTF8_LEAD4_MIN)
            {
                cb = (int)CPLAT_UTF8_BYTES_3;
            }
            else
            {
                cb = (int)CPLAT_UTF8_BYTES_4;
            }

            int cw; /* U+10000 以上はサロゲート ペアで wchar_t 2 個 */

            if (cb == (int)CPLAT_UTF8_BYTES_4)
            {
                cw = CPLAT_DLLMAIN_SURROGATE_WCHAR_COUNT;
            }
            else
            {
                cw = 1;
            }

            if ((wc_count + cw) > (int)CPLAT_DLLMAIN_DEBUG_TRUNCATE_MAX)
            {
                break;
            }

            wc_count += cw;
            byte_pos += cb;
        }

        written = MultiByteToWideChar(CP_UTF8, 0, msg, byte_pos, buf, (int)CPLAT_DLLMAIN_DEBUG_TRUNCATE_MAX);
        if (written <= 0)
        {
            return;
        }

        memcpy(&buf[written], CPLAT_DLLMAIN_TRUNCATE_SUFFIX, sizeof(CPLAT_DLLMAIN_TRUNCATE_SUFFIX));
    }

    OutputDebugStringW(buf);
}
        #define CPLAT_DLLMAIN_INFO_MSG(msg)         dllmain_output_debug_msg(msg)
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
