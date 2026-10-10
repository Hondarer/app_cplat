/**
 *******************************************************************************
 *  @file           trace_syslog.c
 *  @brief          syslog へトレースを出力するプロバイダーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.0.0
 *
 *  Linux syslog ベースのトレース プロバイダーを提供します。\n
 *  /dev/log への UNIX ドメイン SOCK_DGRAM 送信で実装しています。
 *  send(MSG_DONTWAIT) を使用するため、ソケットが詰まっていても
 *  アプリケーションをブロックしません。送信失敗時はメッセージを
 *  drop し、低頻度バックオフでのみ再接続を試みます。\n
 *  環境変数 `SYSLOG_TEST_FD` が設定されている場合は /dev/log の代わりに
 *  その FD に RFC 3164 形式のメッセージを書き込みます (テスト用途)。\n
 *  環境変数は生成時に 1 回だけ参照し、書き込み時には参照しません。
 *
 *  @par            スレッド セーフ
 *  本モジュールはスレッド セーフです。\n
 *  fd・next_connect・backoff_sec の読み書きはすべて reconnect_lock で
 *  保護しています。sendto() は MSG_DONTWAIT で即時返るため、
 *  ロック保持中に実行しても問題ありません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <sys/socket.h>
    #include <sys/un.h>
    #include <unistd.h>
    #include <time.h>
    #include <errno.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

    #include <cplat/base/result.h>
    #include <cplat/clock/clock.h>
    #include <cplat/crt/stdio.h>
    #include <cplat/crt/stdlib.h>
    #include <cplat/crt/string.h>
    #include <cplat/runtime/process.h>
    #include <cplat/sync/sync.h>
    #include <cplat/trace/syslog.h>
    #include <cplat/trace/trace_common.h>
    #include <cplat/trace/backends/syslog/syslog_internal.h>

    /** /dev/log への UNIX ドメイン ソケット パス。 */
    #define DEVLOG_PATH "/dev/log"

    /** 初回バックオフ間隔 (秒)。 */
    #define BACKOFF_INIT_SEC 5

    /** バックオフ最大間隔 (秒)。 */
    #define BACKOFF_MAX_SEC 60

    /** メッセージ バッファー サイズ (RFC 3164 推奨最大長)。 */
    #define SYSLOG_BUF_SIZE 2048

    /** SYSLOG_TEST_FD 向けバッファー サイズ。timestamp と改行を加味する。 */
    #define SYSLOG_DEBUG_BUF_SIZE (SYSLOG_BUF_SIZE + CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 4)

/**
 *  @brief  syslog プロバイダー ハンドル構造体 (内部定義) です。
 */
struct cplat_syslog_sink
{
    /** openlog に相当する識別子文字列 (複製を保持)。 */
    char *ident;

    /**
     *  fd・next_connect・backoff_sec を保護する mutex。
     *  sendto() は MSG_DONTWAIT で即時返るため、ロック保持中に実行します。
     */
    cplat_local_lock *reconnect_lock;

    /** 次回接続試行を許可する最早時刻 (time_t)。reconnect_lock で保護。 */
    time_t next_connect;

    /** syslog facility 値 (例: LOG_USER = 8)。 */
    int facility;

    /** UNIX ドメイン ソケット fd。未接続時は -1。reconnect_lock で保護。 */
    int fd;

    /** 現在のバックオフ間隔 (秒)。reconnect_lock で保護。 */
    int backoff_sec;

    /** 生成時に確定したプロセス ID。 */
    uint32_t pid;

    /** SYSLOG_TEST_FD が生成時に設定されていた場合 1。 */
    int test_fd_exists;

    /** 生成時に SYSLOG_TEST_FD から解釈したテスト用 FD。未設定または解釈できない場合は -1。 */
    int test_fd;

    /** /dev/log の送信先アドレス。 */
    struct sockaddr_un address;

    /** 構造体末尾の暗黙パディングを防ぐ。 */
    uint16_t pad_end;
};

/**
 *  @brief  バックオフ値を次段階に進めます。ロック保持中に呼び出してください。
 */
static void advance_backoff(cplat_syslog_sink *h)
{
    int next = h->backoff_sec * 2;

    if (next > BACKOFF_MAX_SEC)
    {
        h->backoff_sec = BACKOFF_MAX_SEC;
    }
    else
    {
        h->backoff_sec = next;
    }
}

/**
 *  @brief  fd を閉じてバックオフを進めます。ロック保持中に呼び出してください。
 */
static void close_and_backoff_locked(cplat_syslog_sink *h)
{
    cplat_timespec now;

    close(h->fd);
    h->fd = -1;
    cplat_clock_get_realtime(&now);
    h->next_connect = now.tv_sec + h->backoff_sec;
    advance_backoff(h);
}

/**
 *  @brief  バックオフ期間を経過していればソケットを開く試みを行います。
 *          ロック保持中に呼び出すこと。
 */
static void try_open_socket_locked(cplat_syslog_sink *h)
{
    cplat_timespec now;
    int fd;

    cplat_clock_get_realtime(&now);
    if (now.tv_sec < h->next_connect)
    {
        return; /* バックオフ期間中 */
    }

    fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
    {
        h->next_connect = now.tv_sec + h->backoff_sec;
        advance_backoff(h);
        return;
    }

    h->fd = fd;
    /* バックオフは送信成功時にリセットする */
}

/**
 *  @brief  環境変数 SYSLOG_TEST_FD の有無とテスト用 FD を取得します。
 *  @param[out] exists_out  設定されている場合は 1、未設定の場合は 0 を格納します。
 *  @param[out] fd_out      解釈した FD を格納します。未設定または解釈できない場合は -1 を格納します。
 */
static void read_test_fd(int *exists_out, int *fd_out)
{
    char value[32];
    int fd;

    *exists_out = 0;
    *fd_out = -1;
    if (cplat_getenv("SYSLOG_TEST_FD", value, sizeof(value), exists_out, NULL) != CPLAT_OK || *exists_out == 0)
    {
        return;
    }
    if (cplat_parse_int(&fd, value, 10) == CPLAT_OK && fd >= 0)
    {
        *fd_out = fd;
    }
}

/**
 *  @brief  テスト用 FD へバッファーを書き込みます。FD が負の場合は何もしません。
 *  @param[in]  test_fd  書き込み先の FD。
 *  @param[in]  buf      書き込むバッファー。
 *  @param[in]  nbytes   書き込むバイト数。
 */
static void write_test_fd(const int test_fd, const char *buf, const size_t nbytes)
{
    ssize_t written;

    if (test_fd < 0)
    {
        return;
    }
    /* テスト用の経路であり、書き込みの失敗は報告しない。glibc の warn_unused_result は (void) で抑止できない。 */
    written = write(test_fd, buf, nbytes);
    (void)written;
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_syslog_sink *cplat_syslog_sink_create(const char *ident, const int facility)
{
    cplat_syslog_sink *handle;
    size_t len;

    if (ident == NULL)
    {
        return NULL;
    }

    handle = (cplat_syslog_sink *)cplat_malloc(sizeof(cplat_syslog_sink));
    if (handle == NULL)
    {
        return NULL;
    }

    len = strlen(ident) + 1;
    handle->ident = (char *)cplat_malloc(len);
    if (handle->ident == NULL)
    {
        cplat_free(handle);
        return NULL;
    }
    memcpy(handle->ident, ident, len);

    handle->facility = facility;
    handle->fd = -1;
    handle->next_connect = 0;
    handle->backoff_sec = BACKOFF_INIT_SEC;
    handle->pid = cplat_process_get_pid();
    handle->test_fd_exists = 0;
    handle->test_fd = -1;
    read_test_fd(&handle->test_fd_exists, &handle->test_fd);
    memset(&handle->address, 0, sizeof(handle->address));
    handle->address.sun_family = AF_UNIX;
    (void)cplat_strncpy(handle->address.sun_path, sizeof(handle->address.sun_path), DEVLOG_PATH,
                        sizeof(handle->address.sun_path) - 1u);
    if (cplat_local_lock_create(&handle->reconnect_lock) != CPLAT_OK)
    {
        cplat_free(handle->ident);
        cplat_free(handle);
        return NULL;
    }
    /* 初回接続を試みる (失敗しても構わない) */
    cplat_local_lock_lock(handle->reconnect_lock, CPLAT_SYNC_WAIT_FOREVER);
    try_open_socket_locked(handle);
    cplat_local_lock_unlock(handle->reconnect_lock);

    return handle;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_syslog_sink_write(cplat_syslog_sink *handle, const int level, const cplat_timespec *timestamp,
                            const char *message)
{
    char buf[SYSLOG_BUF_SIZE];
    char debug_buf[SYSLOG_DEBUG_BUF_SIZE];
    char timestamp_text[CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1];
    cplat_timespec resolved;
    const cplat_timespec *effective_timestamp = NULL;
    int fallback_used = 0;
    int prio;
    int n;
    int debug_len;
    ssize_t sent;

    if (handle == NULL || message == NULL)
    {
        return CPLAT_OK;
    }
    /* timestamp が NULL の場合はタイムスタンプなしで送信するため、解決は行わない */
    if (timestamp != NULL)
    {
        if (cplat_internal_trace_resolve_timestamp(timestamp, &resolved, &fallback_used) != 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        effective_timestamp = &resolved;
    }

    /* syslog priority = (facility & ~7) | (severity & 7) */
    prio = (handle->facility & ~7) | (level & 7);

    /* RFC 3164 形式: <PRI>TAG[PID]: MSG */
    n = snprintf(buf, sizeof(buf), "<%d>%s[%u]: %s", prio, handle->ident, handle->pid,
                 message); /* 置換対象外: 意図的な切り詰め */
    if (n < 0)
    {
        return CPLAT_OK;
    }
    if ((size_t)n >= sizeof(buf))
    {
        n = (int)(sizeof(buf) - 1);
    }

    /* SYSLOG_TEST_FD が設定されていればテスト用 FD に送信し、/dev/log へは送信しない。
       値は参照せず、設定の有無だけを判定する */
    if (handle->test_fd_exists != 0)
    {
        if (effective_timestamp != NULL && cplat_clock_format_realtime_iso8601_local(
                                               timestamp_text, sizeof(timestamp_text), effective_timestamp) == CPLAT_OK)
        {
            debug_len = snprintf(debug_buf, sizeof(debug_buf), "%s %.*s\n", timestamp_text, n,
                                 buf); /* 置換対象外: 意図的な切り詰め */
            if (debug_len < 0)
            {
                return CPLAT_ERR_UNKNOWN;
            }
            if ((size_t)debug_len >= sizeof(debug_buf))
            {
                debug_buf[sizeof(debug_buf) - 2] = '\n';
                debug_buf[sizeof(debug_buf) - 1] = '\0';
                debug_len = (int)(sizeof(debug_buf) - 1);
            }
            write_test_fd(handle->test_fd, debug_buf, (size_t)debug_len);
        }
        else
        {
            buf[n] = '\n';
            write_test_fd(handle->test_fd, buf, (size_t)(n + 1));
        }
        if (fallback_used)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        else
        {
            return CPLAT_OK;
        }
    }

    cplat_local_lock_lock(handle->reconnect_lock, CPLAT_SYNC_WAIT_FOREVER);

    /* ソケットが無ければ低頻度で再接続を試みる */
    if (handle->fd < 0)
    {
        try_open_socket_locked(handle);
        if (handle->fd < 0)
        {
            cplat_local_lock_unlock(handle->reconnect_lock);
            if (fallback_used)
            {
                return CPLAT_ERR_UNKNOWN;
            }
            else
            {
                return CPLAT_OK; /* drop */
            }
        }
    }

    /* sendto は MSG_DONTWAIT で即時返るため、ロック保持中に実行する */
    sent = sendto(handle->fd, buf, (size_t)n, MSG_DONTWAIT, (struct sockaddr *)&handle->address,
                  (socklen_t)sizeof(handle->address));

    if (sent < 0)
    {
        /* Linux では EWOULDBLOCK と EAGAIN が同値のため、1 つの判定で両方を扱う。
           see: https://man7.org/linux/man-pages/man3/errno.3.html */
        if (errno == EAGAIN)
        {
            /* 送信バッファー満杯: drop のみ、再接続不要 */
            cplat_local_lock_unlock(handle->reconnect_lock);
            if (fallback_used)
            {
                return CPLAT_ERR_UNKNOWN;
            }
            else
            {
                return CPLAT_OK;
            }
        }
        /* その他エラー (ENOENT, ECONNREFUSED 等): ソケットを閉じてバックオフ */
        close_and_backoff_locked(handle);
        cplat_local_lock_unlock(handle->reconnect_lock);
        if (fallback_used)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        else
        {
            return CPLAT_OK; /* drop */
        }
    }

    /* 送信成功: バックオフをリセット */
    handle->backoff_sec = BACKOFF_INIT_SEC;

    cplat_local_lock_unlock(handle->reconnect_lock);
    if (fallback_used)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    else
    {
        return CPLAT_OK;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_syslog_sink_dispose(cplat_syslog_sink *handle)
{
    if (handle == NULL)
    {
        return;
    }

    if (handle->fd >= 0)
    {
        close(handle->fd);
    }
    cplat_local_lock_dispose(handle->reconnect_lock);
    cplat_free(handle->ident);
    cplat_free(handle);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_syslog_sink_rename(cplat_syslog_sink *handle, const char *new_ident)
{
    char *dup;
    size_t len;
    int result;

    if (handle == NULL || new_ident == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    len = strlen(new_ident) + 1;
    dup = (char *)cplat_malloc(len);
    if (dup == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    memcpy(dup, new_ident, len);

    result = cplat_local_lock_lock(handle->reconnect_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (result != CPLAT_OK)
    {
        cplat_free(dup);
        return result;
    }
    cplat_free(handle->ident);
    handle->ident = dup;
    result = cplat_local_lock_unlock(handle->reconnect_lock);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_syslog_sink_dispose_on_shutdown(cplat_syslog_sink *handle)
{
    if (handle == NULL)
    {
        return;
    }

    if (handle->fd >= 0)
    {
        close(handle->fd);
    }
    cplat_local_lock_dispose(handle->reconnect_lock);
    cplat_free(handle->ident);
    cplat_free(handle);
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif
