/**
 *******************************************************************************
 *  @file           socket_watch_windows.c
 *  @brief          Windows の解除可能なソケット待機を実装します。
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <assert.h>
    #include <cplat/base/windows_sdk.h>
    #include <errno.h>
    #include <stdint.h>
    #include <string.h>

    #include <cplat/base/error_internal.h>
    #include <cplat/base/result.h>
    #include <cplat/base/wait.h>
    #include <cplat/clock/clock.h>
    #include <cplat/crt/stdlib.h>
    #include <cplat/net/socket.h>
    #include <cplat/net/socket_internal.h>

#define SOCKET_WATCH_CANCEL_CHECK_MS 50U
#define SOCKET_WATCH_NETWORK_EVENTS (FD_READ | FD_ACCEPT | FD_CLOSE | FD_WRITE | FD_CONNECT)

static_assert((2U * CPLAT_SOCKET_WATCH_WAIT_MAX + 1U) <= WSA_MAXIMUM_WAIT_EVENTS,
              "cplat: Windows のソケット待機イベント数が上限を超えています。");

struct cplat_socket_wait_cancel
{
    WSAEVENT event;
    volatile LONG signaled;
};

struct cplat_socket_watch
{
    SOCKET sock;
    WSAEVENT event_shared;
    WSAEVENT event_read;
    WSAEVENT event_write;
    CRITICAL_SECTION lock;
    int lock_initialized;
    long pending;
    int writable;
    uint64_t write_gen;
    int connect_done;
    int connect_consumed;
    int connect_error;
    int closed;
    int event_error;
    int event_error_read;
    int event_error_write;
    int internal_result;
    cplat_error internal_detail;
    int event_read_usable;
    int event_write_usable;
};

/*
 * 各登録はネットワーク通知用 E_s、受信側起床用 E_r、送信側起床用 E_w の手動リセット イベントを持ちます。
 * 内部スレッドは作らず、3 つのイベントと CRITICAL_SECTION でネットワーク通知の取得と方向別の待機を同期します。
 *
 * E_s は WSAEventSelect だけに結び付け、WSASetEvent / WSAResetEvent で直接操作しません。
 * WSAEnumNetworkEvents(sock, E_s, ...) は、イベントの取得と E_s のリセットを lock 内で行い、
 * FD_READ / FD_ACCEPT、FD_WRITE、FD_CONNECT、FD_CLOSE と各エラーを全方向の共有状態へ保存します。
 * 保存した closed、イベント エラー、列挙失敗は登録を解くまで消費しません。
 *
 * 待機側は lock 内で自身の条件を確認し、条件がなければ同じ lock 内で E_r または E_w だけをリセットします。
 * 共有イベントを取得した側は、lock を離れる前に状態を更新し、条件のある方向のイベントを通知します。
 * そのため、送信側が E_w をリセットした直後に受信側が FD_WRITE を取得しても、送信側の起床は失われません。
 * 受信側が E_r をリセットした直後に送信側が FD_READ / FD_ACCEPT を取得した場合も同様です。
 * FD_CLOSE とエラーは両方向へ保持して通知し、後続側の列挙が空でも保存状態から起床します。
 * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect
 * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumnetworkevents
 * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasetevent
 * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaresetevent
 * see: https://learn.microsoft.com/en-us/windows/win32/sync/event-objects
 *
 * 受信待機の対象は、有効な登録数を N、解除オブジェクトの有無を C (0 または 1) として 2 * N + C 個です。
 * 最大 16 登録と解除を含めて 33 個を上限とし、同時に複数の登録の lock は保持しません。
 * WSAWaitForMultipleEvents は最小インデックスの 1 件だけを報告するため、各反復で全登録の E_s を
 * 待ち時間 0 で処理してから、全登録の受信条件を集めます。
 * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsawaitformultipleevents
 */

typedef enum socket_watch_direction
{
    SOCKET_WATCH_READ,
    SOCKET_WATCH_WRITE,
    SOCKET_WATCH_CONNECT
} socket_watch_direction;

static int cancel_is_signaled(const cplat_socket_wait_cancel *cancel);
static void watch_lock(cplat_socket_watch *watch);
static void watch_unlock(cplat_socket_watch *watch);
static void store_internal_error_locked(cplat_socket_watch *watch, const int winsock_error);
static void signal_directions_locked(cplat_socket_watch *watch);
static int enumerate_network_events_locked(cplat_socket_watch *watch);
static int process_shared_event(cplat_socket_watch *watch, int *wait_error);
static int poll_current_state(cplat_socket_watch *watch, const short requested_events, cplat_error *detail_out);
static int inspect_direction(cplat_socket_watch *watch, const socket_watch_direction direction, int *ready_out,
                             int *terminal_out, int *connect_error_out, cplat_error *detail_out);
static int get_deadline_remaining(const int timeout_ms, const uint64_t deadline_ms, uint64_t *remaining_ms);
static int wait_native_events(WSAEVENT *events, const DWORD count, cplat_socket_wait_cancel *cancel,
                              const int timeout_ms, const uint64_t deadline_ms, cplat_error *detail_out);
static DWORD make_read_wait_events(cplat_socket_watch *const *watches, const size_t count,
                                   cplat_socket_wait_cancel *cancel, WSAEVENT *events);
static DWORD make_one_wait_events(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel,
                                  WSAEVENT *events);
static int wait_readable_many(cplat_socket_watch *const *watches, const size_t count,
                              cplat_socket_wait_cancel *cancel, const int timeout_ms,
                              unsigned char *ready_out, cplat_error *detail_out);
static int wait_one_direction_with_deadline(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel,
                                            const int timeout_ms, const uint64_t deadline_ms,
                                            const socket_watch_direction direction, int *terminal_out,
                                            cplat_error *detail_out);
static int wait_one_direction(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel, const int timeout_ms,
                              const socket_watch_direction direction, int *terminal_out, cplat_error *detail_out);
static int send_once(cplat_socket_watch *watch, const void *buf, const size_t len, size_t *sent_out,
                     const int use_sendto, const struct sockaddr *address, const int address_len,
                     const int retry_eintr, int *interrupted_out, cplat_error *detail_out);
static int transfer_all(cplat_socket_watch *watch, void *buf, const size_t len, cplat_socket_wait_cancel *cancel,
                        const int timeout_ms, size_t *transferred_out, const int sending, cplat_error *detail_out);

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_cancel_create(cplat_socket_wait_cancel **cancel_out, cplat_error *detail_out)
{
    cplat_socket_wait_cancel *cancel;

    if (cancel_out == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *cancel_out = NULL;
    if (cplat_internal_socket_startup(detail_out) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    cancel = (cplat_socket_wait_cancel *)cplat_malloc(sizeof(*cancel));
    if (cancel == NULL)
    {
        return cplat_internal_error_report_windows_error_as(detail_out, ERROR_NOT_ENOUGH_MEMORY,
                                                            CPLAT_ERR_OUT_OF_MEMORY);
    }
    cancel->event = WSACreateEvent();
    if (cancel->event == WSA_INVALID_EVENT)
    {
        const int saved_error = WSAGetLastError();
        cplat_free(cancel);
        return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)saved_error);
    }
    cancel->signaled = 0;
    *cancel_out = cancel;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_cancel_signal(cplat_socket_wait_cancel *cancel, cplat_error *detail_out)
{
    if (cancel == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    InterlockedExchange(&cancel->signaled, 1);
    /* 失敗しても原子的な通知状態を保持し、最大 50 [ms] の状態確認で待機を解除します。 */
    if (!WSASetEvent(cancel->event))
    {
        const int saved_error = WSAGetLastError();
        (void)saved_error;
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_cancel_reset(cplat_socket_wait_cancel *cancel, cplat_error *detail_out)
{
    if (cancel == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    /* 手動リセット イベントを先に戻し、失敗時は原子的な通知状態を維持します。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaresetevent
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsacreateevent
     * see: https://learn.microsoft.com/en-us/windows/win32/sync/event-objects
     */
    if (!WSAResetEvent(cancel->event))
    {
        const int saved_error = WSAGetLastError();
        return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)saved_error);
    }
    InterlockedExchange(&cancel->signaled, 0);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_socket_wait_cancel_dispose(cplat_socket_wait_cancel *cancel)
{
    cplat_error saved;

    if (cancel == NULL)
    {
        return;
    }
    cplat_error_get_last(&saved);
    /* 解放関数であり、失敗を受けても利用側に取れる手段がありません。 */
    (void)WSACloseEvent(cancel->event);
    cplat_free(cancel);
    cplat_error_set_last(&saved);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_attach(const cplat_socket sock, cplat_socket_watch **watch_out, cplat_error *detail_out)
{
    cplat_socket_watch *watch;
    int last_error;

    if (sock == CPLAT_INVALID_SOCKET || watch_out == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *watch_out = NULL;
    if (cplat_internal_socket_startup(detail_out) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /* WSAEventSelect はソケットを非ブロッキングにし、選択中の登録を置き換えるため最後に呼びます。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect
     */
    watch = (cplat_socket_watch *)cplat_malloc(sizeof(*watch));
    if (watch == NULL)
    {
        return cplat_internal_error_report_windows_error_as(detail_out, ERROR_NOT_ENOUGH_MEMORY,
                                                            CPLAT_ERR_OUT_OF_MEMORY);
    }
    memset(watch, 0, sizeof(*watch));
    watch->sock = (SOCKET)sock;
    watch->event_shared = WSA_INVALID_EVENT;
    watch->event_read = WSA_INVALID_EVENT;
    watch->event_write = WSA_INVALID_EVENT;
    watch->writable = 1;
    watch->event_read_usable = 1;
    watch->event_write_usable = 1;

    if (!InitializeCriticalSectionAndSpinCount(&watch->lock, 0U))
    {
        const DWORD saved_error = GetLastError();
        cplat_free(watch);
        return cplat_internal_error_report_windows_error(detail_out, (unsigned long)saved_error);
    }
    watch->lock_initialized = 1;
    watch->event_shared = WSACreateEvent();
    if (watch->event_shared == WSA_INVALID_EVENT)
    {
        last_error = WSAGetLastError();
        goto cleanup;
    }
    watch->event_read = WSACreateEvent();
    if (watch->event_read == WSA_INVALID_EVENT)
    {
        last_error = WSAGetLastError();
        goto cleanup;
    }
    watch->event_write = WSACreateEvent();
    if (watch->event_write == WSA_INVALID_EVENT)
    {
        last_error = WSAGetLastError();
        goto cleanup;
    }

    /* 登録は資源をすべて準備した最後に行い、失敗時はソケット状態を変えません。 */
    if (WSAEventSelect(watch->sock, watch->event_shared, SOCKET_WATCH_NETWORK_EVENTS) == SOCKET_ERROR)
    {
        last_error = WSAGetLastError();
        goto cleanup;
    }

    *watch_out = watch;
    return cplat_internal_error_report_success(detail_out);

cleanup:
    if (watch->event_write != WSA_INVALID_EVENT)
    {
        /* 未公開の準備資源を回収します。 */
        (void)WSACloseEvent(watch->event_write);
    }
    if (watch->event_read != WSA_INVALID_EVENT)
    {
        (void)WSACloseEvent(watch->event_read);
    }
    if (watch->event_shared != WSA_INVALID_EVENT)
    {
        (void)WSACloseEvent(watch->event_shared);
    }
    if (watch->lock_initialized != 0)
    {
        DeleteCriticalSection(&watch->lock);
    }
    cplat_free(watch);
    return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)last_error);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_socket_watch_detach(cplat_socket_watch *watch)
{
    cplat_error saved;

    if (watch == NULL)
    {
        return;
    }
    cplat_error_get_last(&saved);
    /* 登録を解く時点では公開関数の待機者がいない契約です。 */
    (void)WSAEventSelect(watch->sock, NULL, 0);
    (void)WSACloseEvent(watch->event_write);
    (void)WSACloseEvent(watch->event_read);
    (void)WSACloseEvent(watch->event_shared);
    DeleteCriticalSection(&watch->lock);
    cplat_free(watch);
    cplat_error_set_last(&saved);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_wait_readable_multi(cplat_socket_watch *const *watches, const size_t count,
                                           cplat_socket_wait_cancel *cancel, const int timeout_ms,
                                           unsigned char *ready_out, cplat_error *detail_out)
{
    size_t valid_count = 0U;

    if (watches == NULL || count == 0U || count > CPLAT_SOCKET_WATCH_WAIT_MAX || timeout_ms < 0 || ready_out == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    for (size_t index = 0U; index < count; ++index)
    {
        ready_out[index] = 0U;
        if (watches[index] != NULL)
        {
            if (watches[index]->sock == INVALID_SOCKET)
            {
                return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
            }
            ++valid_count;
        }
    }
    if (valid_count == 0U && cancel == NULL && timeout_ms == CPLAT_WAIT_FOREVER)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    return wait_readable_many(watches, count, cancel, timeout_ms, ready_out, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_wait_writable(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel,
                                     const int timeout_ms, cplat_error *detail_out)
{
    int terminal;

    if (watch == NULL || watch->sock == INVALID_SOCKET || timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    return wait_one_direction(watch, cancel, timeout_ms, SOCKET_WATCH_WRITE, &terminal, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_send(cplat_socket_watch *watch, const void *buf, const size_t len, size_t *sent_out,
                            cplat_error *detail_out)
{
    if (watch == NULL || watch->sock == INVALID_SOCKET || buf == NULL || sent_out == NULL ||
        len > CPLAT_SOCKET_MAX_TRANSFER)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *sent_out = 0U;
    return send_once(watch, buf, len, sent_out, 0, NULL, 0, 1, NULL, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_sendto(cplat_socket_watch *watch, const void *buf, const size_t len,
                              const cplat_ipv4_endpoint *endpoint, size_t *sent_out, cplat_error *detail_out)
{
    struct sockaddr_in native;
    if (watch == NULL || watch->sock == INVALID_SOCKET || buf == NULL || endpoint == NULL || sent_out == NULL ||
        len > CPLAT_SOCKET_MAX_TRANSFER)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    memset(&native, 0, sizeof(native));
    native.sin_family = AF_INET;
    memcpy(&native.sin_addr.S_un.S_addr, &endpoint->address, sizeof(native.sin_addr.S_un.S_addr));
    memcpy(&native.sin_port, &endpoint->port, sizeof(native.sin_port));
    *sent_out = 0U;
    return send_once(watch, buf, len, sent_out, 1, (const struct sockaddr *)&native, (int)sizeof(native), 1, NULL,
                     detail_out);
}

/*
 * Windows は、Registered I/O (RIOSendEx) で送信を 1 回の呼び出しにまとめず、sendto() を先頭から順に繰り返す。
 * WSA_FLAG_REGISTERED_IO 付きの UDP ソケットでは、bind() は成功するが、WSAEventSelect()、sendto()、recvfrom()、
 * select() が WSAENOTSOCK で失敗する (Windows 11 10.0.26300 での実測、2026-11-25)。
 * 登録は WSAEventSelect() で送受信の通知を待ち、利用側は通常の受信と送信の関数も使うため、RIO のソケットにできない。
 * see: https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_riosendex
 * see: https://learn.microsoft.com/en-us/windows/win32/api/mswsock/nc-mswsock-lpfn_riocreaterequestqueue
 */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_sendto_multi(cplat_socket_watch *watch, const cplat_socket_datagram *datagrams,
                                    const size_t count, size_t *sent_count_out, cplat_error *detail_out)
{
    size_t index;

    if (sent_count_out != NULL)
    {
        *sent_count_out = 0U;
    }
    if (watch == NULL || watch->sock == INVALID_SOCKET || datagrams == NULL || sent_count_out == NULL || count == 0U ||
        count > CPLAT_SOCKET_SENDTO_MULTI_MAX)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    for (index = 0U; index < count; index++)
    {
        if (datagrams[index].buf == NULL || datagrams[index].endpoint == NULL ||
            datagrams[index].len > CPLAT_SOCKET_MAX_TRANSFER)
        {
            return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
        }
    }

    for (index = 0U; index < count; index++)
    {
        struct sockaddr_in native;
        size_t sent = 0U;
        int result;

        memset(&native, 0, sizeof(native));
        native.sin_family = AF_INET;
        memcpy(&native.sin_addr.S_un.S_addr, &datagrams[index].endpoint->address, sizeof(native.sin_addr.S_un.S_addr));
        memcpy(&native.sin_port, &datagrams[index].endpoint->port, sizeof(native.sin_port));
        /* WSAEWOULDBLOCK の場合の送信可能の状態の更新は、send_once() が行う。 */
        result = send_once(watch, datagrams[index].buf, datagrams[index].len, &sent, 1,
                           (const struct sockaddr *)&native, (int)sizeof(native), 1, NULL, detail_out);
        if (result != CPLAT_OK)
        {
            if (index == 0U)
            {
                return result;
            }
            break;
        }

        /* 全量を送れなかったデータグラムは送れなかったものとし、そこで止める。 */
        if (sent != datagrams[index].len)
        {
            if (index == 0U)
            {
                return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }
            break;
        }
        *sent_count_out = index + 1U;
    }

    /* 途中で止めた場合も、送れたデータグラムがあるため成功を返す。止めた理由は返さない。 */
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_send_all(cplat_socket_watch *watch, const void *buf, const size_t len,
                                cplat_socket_wait_cancel *cancel, const int timeout_ms, size_t *sent_out,
                                cplat_error *detail_out)
{
    size_t transferred = 0U;
    const int result = transfer_all(watch, (void *)buf, len, cancel, timeout_ms, &transferred, 1, detail_out);
    if (sent_out != NULL)
    {
        *sent_out = transferred;
    }
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_recv_all(cplat_socket_watch *watch, void *buf, const size_t len,
                                cplat_socket_wait_cancel *cancel, const int timeout_ms, size_t *received_out,
                                cplat_error *detail_out)
{
    size_t transferred = 0U;
    const int result = transfer_all(watch, buf, len, cancel, timeout_ms, &transferred, 0, detail_out);
    if (received_out != NULL)
    {
        *received_out = transferred;
    }
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_connect(cplat_socket_watch *watch, const cplat_ipv4_endpoint *endpoint,
                               cplat_socket_wait_cancel *cancel, const int timeout_ms, cplat_error *detail_out)
{
    struct sockaddr_in native;
    int connect_result;

    if (watch == NULL || watch->sock == INVALID_SOCKET || endpoint == NULL || timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    if (cancel_is_signaled(cancel))
    {
        return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
    }
    memset(&native, 0, sizeof(native));
    native.sin_family = AF_INET;
    memcpy(&native.sin_addr.S_un.S_addr, &endpoint->address, sizeof(native.sin_addr.S_un.S_addr));
    memcpy(&native.sin_port, &endpoint->port, sizeof(native.sin_port));

    /* 非ブロッキング接続の完了と失敗は FD_CONNECT の通知で確認します。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-connect
     */
    connect_result = connect(watch->sock, (const struct sockaddr *)&native, (int)sizeof(native));
    if (connect_result == 0)
    {
        watch_lock(watch);
        watch->connect_done = 1;
        watch->connect_consumed = 1;
        watch->connect_error = 0;
        watch_unlock(watch);
        return cplat_internal_error_report_success(detail_out);
    }
    {
        const int saved_error = WSAGetLastError();
        if (saved_error != WSAEWOULDBLOCK && saved_error != WSAEINPROGRESS && saved_error != WSAEALREADY)
        {
            return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)saved_error);
        }
    }
    return wait_one_direction(watch, cancel, timeout_ms, SOCKET_WATCH_CONNECT, NULL, detail_out);
}

/**
 *  @brief          解除状態を Interlocked 操作で acquire します。
 */
static int cancel_is_signaled(const cplat_socket_wait_cancel *cancel)
{
    return (cancel != NULL) && (InterlockedCompareExchange((volatile LONG *)&cancel->signaled, 0, 0) != 0);
}

/**
 *  @brief          共有状態のロックを取得します。
 */
static void watch_lock(cplat_socket_watch *watch)
{
    EnterCriticalSection(&watch->lock);
}

/**
 *  @brief          共有状態のロックを解放します。
 */
static void watch_unlock(cplat_socket_watch *watch)
{
    LeaveCriticalSection(&watch->lock);
}

/**
 *  @brief          Winsock エラーを登録へ保存し、両方向の待機者を起床させます。
 */
static void store_internal_error_locked(cplat_socket_watch *watch, const int winsock_error)
{
    watch->internal_result = cplat_internal_error_report_winsock_error(&watch->internal_detail,
                                                                        (unsigned long)winsock_error);
    /* 状態を保持し、50 [ms] の有限確認で起床通知の失敗を回収します。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsasetevent
     * see: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-setevent
     */
    if (watch->event_read_usable != 0)
    {
        (void)WSASetEvent(watch->event_read);
    }
    if (watch->event_write_usable != 0)
    {
        (void)WSASetEvent(watch->event_write);
    }
}

/**
 *  @brief          受信と送信の状態から、それぞれの待機イベントを通知します。
 */
static void signal_directions_locked(cplat_socket_watch *watch)
{
    const int read_ready = (watch->pending != 0) || (watch->closed != 0) || (watch->event_error != 0) ||
                           (watch->event_error_read != 0) || (watch->internal_result != CPLAT_OK);
    const int write_ready = (watch->writable != 0) || (watch->closed != 0) || (watch->event_error != 0) ||
                            (watch->event_error_write != 0) || (watch->internal_result != CPLAT_OK);

    /* 状態を先に保存しているため、SetEvent の失敗は有限確認で回収できます。 */
    if (read_ready != 0 && watch->event_read_usable != 0 && !WSASetEvent(watch->event_read))
    {
        const int saved_error = WSAGetLastError();
        (void)saved_error;
    }
    if (write_ready != 0 && watch->event_write_usable != 0 && !WSASetEvent(watch->event_write))
    {
        const int saved_error = WSAGetLastError();
        (void)saved_error;
    }
}

/**
 *  @brief          ネットワーク通知の全方向状態をロック内で取り込みます。
 *  @return         成功時は CPLAT_OK、失敗時は保存した共通結果コードを返します。
 *
 *  WSAEnumNetworkEvents は記録の取り出しとネットワーク イベントのリセットを不可分に行います。
 *  see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumnetworkevents
 */
static int enumerate_network_events_locked(cplat_socket_watch *watch)
{
    WSANETWORKEVENTS events;

    if (watch->internal_result != CPLAT_OK)
    {
        return watch->internal_result;
    }

    memset(&events, 0, sizeof(events));
    if (WSAEnumNetworkEvents(watch->sock, watch->event_shared, &events) == SOCKET_ERROR)
    {
        const int saved_error = WSAGetLastError();
        store_internal_error_locked(watch, saved_error);
        return watch->internal_result;
    }

    if ((events.lNetworkEvents & FD_READ) != 0)
    {
        watch->pending |= FD_READ;
        if (events.iErrorCode[FD_READ_BIT] != 0)
        {
            watch->event_error_read = events.iErrorCode[FD_READ_BIT];
            watch->event_error = events.iErrorCode[FD_READ_BIT];
        }
    }
    if ((events.lNetworkEvents & FD_ACCEPT) != 0)
    {
        watch->pending |= FD_ACCEPT;
        if (events.iErrorCode[FD_ACCEPT_BIT] != 0)
        {
            watch->event_error_read = events.iErrorCode[FD_ACCEPT_BIT];
            watch->event_error = events.iErrorCode[FD_ACCEPT_BIT];
        }
    }
    if ((events.lNetworkEvents & FD_WRITE) != 0)
    {
        watch->writable = 1;
        ++watch->write_gen;
        if (events.iErrorCode[FD_WRITE_BIT] != 0)
        {
            watch->event_error_write = events.iErrorCode[FD_WRITE_BIT];
            watch->event_error = events.iErrorCode[FD_WRITE_BIT];
        }
    }
    if ((events.lNetworkEvents & FD_CONNECT) != 0)
    {
        watch->connect_done = 1;
        watch->connect_error = events.iErrorCode[FD_CONNECT_BIT];
        if (watch->connect_error != 0)
        {
            watch->event_error = watch->connect_error;
        }
        watch->writable = 1;
        ++watch->write_gen;
    }
    if ((events.lNetworkEvents & FD_CLOSE) != 0)
    {
        watch->closed = 1;
        if (events.iErrorCode[FD_CLOSE_BIT] != 0)
        {
            watch->event_error = events.iErrorCode[FD_CLOSE_BIT];
        }
    }

    signal_directions_locked(watch);
    return CPLAT_OK;
}

/**
 *  @brief          共有ネットワーク イベントが通知済みならロック内で取り込みます。
 *  @param[out]     wait_error Winsock の待機失敗。NULL 可。
 *  @return         取り込んだ場合は 1、通知なしは 0、待機失敗は -1 を返します。
 */
static int process_shared_event(cplat_socket_watch *watch, int *wait_error)
{
    DWORD wait_result;
    WSAEVENT event = watch->event_shared;

    wait_result = WSAWaitForMultipleEvents(1U, &event, FALSE, 0U, FALSE);
    if (wait_result == WSA_WAIT_TIMEOUT)
    {
        return 0;
    }
    if (wait_result == WSA_WAIT_FAILED)
    {
        *wait_error = WSAGetLastError();
        return -1;
    }
    if (wait_result != WSA_WAIT_EVENT_0)
    {
        *wait_error = WSAEINVAL;
        return -1;
    }

    watch_lock(watch);
    (void)enumerate_network_events_locked(watch);
    watch_unlock(watch);
    return 1;
}

/**
 *  @brief          WSAPoll の即時確認結果をロック内の共有状態へ反映します。
 *  @return         成功時は CPLAT_OK、失敗時は対応する結果コードを返します。
 */
static int poll_current_state(cplat_socket_watch *watch, const short requested_events, cplat_error *detail_out)
{
    WSAPOLLFD descriptor;
    int poll_result;
    int saved_error;

    memset(&descriptor, 0, sizeof(descriptor));
    descriptor.fd = watch->sock;
    descriptor.events = requested_events;
    poll_result = WSAPoll(&descriptor, 1U, 0);
    if (poll_result == SOCKET_ERROR)
    {
        saved_error = WSAGetLastError();
        return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)saved_error);
    }

    watch_lock(watch);
    if ((descriptor.revents & POLLNVAL) != 0)
    {
        watch_unlock(watch);
        return cplat_internal_error_report_winsock_error_as(detail_out, WSAEINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    if ((descriptor.revents & (POLLERR | POLLHUP)) != 0)
    {
        watch->closed = 1;
    }
    if ((requested_events & POLLRDNORM) != 0 && (descriptor.revents & (POLLRDNORM | POLLIN)) != 0)
    {
        watch->pending |= FD_READ;
    }
    if ((requested_events & POLLWRNORM) != 0 && (descriptor.revents & (POLLWRNORM | POLLOUT)) != 0)
    {
        watch->writable = 1;
        ++watch->write_gen;
    }
    signal_directions_locked(watch);
    watch_unlock(watch);
    return CPLAT_OK;
}

/**
 *  @brief          登録の状態から待機条件を取り込み、必要なら方向イベントをリセットします。
 *  @param[out]     ready_out 終了条件が成立した場合に 1 を格納します。
 *  @param[out]     terminal_out 終了またはエラー状態を格納します。NULL 可。
 *  @param[out]     connect_error_out FD_CONNECT のエラー。NULL 可。
 *  @param[out]     detail_out 失敗詳細。NULL 可。
 *  @return         共通結果コードを返します。
 */
static int inspect_direction(cplat_socket_watch *watch, const socket_watch_direction direction, int *ready_out,
                             int *terminal_out, int *connect_error_out, cplat_error *detail_out)
{
    int has_condition;
    int result = CPLAT_OK;

    *ready_out = 0;
    if (terminal_out != NULL)
    {
        *terminal_out = 0;
    }
    if (connect_error_out != NULL)
    {
        *connect_error_out = 0;
    }

    watch_lock(watch);
    if (watch->internal_result != CPLAT_OK)
    {
        if (detail_out != NULL)
        {
            *detail_out = watch->internal_detail;
        }
        result = watch->internal_result;
        cplat_error_set_last(&watch->internal_detail);
        watch_unlock(watch);
        return result;
    }

    if (direction == SOCKET_WATCH_READ)
    {
        has_condition = (watch->pending != 0) || (watch->closed != 0) || (watch->event_error != 0) ||
                        (watch->event_error_read != 0);
        if (has_condition != 0)
        {
            watch->pending = 0;
        }
        else if (watch->event_read_usable != 0 && !WSAResetEvent(watch->event_read))
        {
            const int saved_error = WSAGetLastError();
            (void)saved_error;
            watch->event_read_usable = 0;
        }
    }
    else if (direction == SOCKET_WATCH_WRITE)
    {
        has_condition = (watch->writable != 0) || (watch->closed != 0) || (watch->event_error != 0) ||
                        (watch->event_error_write != 0);
        if (has_condition == 0 && watch->event_write_usable != 0 && !WSAResetEvent(watch->event_write))
        {
            const int saved_error = WSAGetLastError();
            (void)saved_error;
            watch->event_write_usable = 0;
        }
    }
    else
    {
        has_condition = (watch->connect_done != 0) || (watch->closed != 0) || (watch->event_error != 0);
        if (has_condition != 0 && watch->connect_done != 0)
        {
            if (connect_error_out != NULL)
            {
                *connect_error_out = watch->connect_error;
            }
            watch->connect_consumed = 1;
        }
        else if (has_condition == 0 && watch->event_write_usable != 0 && !WSAResetEvent(watch->event_write))
        {
            const int saved_error = WSAGetLastError();
            (void)saved_error;
            watch->event_write_usable = 0;
        }
    }

    if (terminal_out != NULL)
    {
        *terminal_out = (watch->closed != 0) || (watch->event_error != 0) || (watch->event_error_read != 0) ||
                        (watch->event_error_write != 0);
    }
    *ready_out = has_condition;
    watch_unlock(watch);
    return result;
}

/**
 *  @brief          相対タイムアウトから絶対期限と残り時間を扱います。
 */
static int get_deadline_remaining(const int timeout_ms, const uint64_t deadline_ms, uint64_t *remaining_ms)
{
    uint64_t now;

    if (timeout_ms == CPLAT_WAIT_FOREVER)
    {
        *remaining_ms = UINT64_MAX;
        return 1;
    }
    if (timeout_ms == CPLAT_NO_WAIT)
    {
        *remaining_ms = 0U;
        return 0;
    }
    if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
    {
        return -1;
    }
    if (now >= deadline_ms)
    {
        *remaining_ms = 0U;
        return 0;
    }
    *remaining_ms = deadline_ms - now;
    return 1;
}

/**
 *  @brief          Windows の通知イベントを最大 50 [ms] 待ちます。
 *  @return         成功時は CPLAT_OK、失敗時は共通結果コードを返します。
 *
 *  WSAWaitForMultipleEvents は最小インデックスの通知だけを返すため、復帰後に全登録を再検査します。
 *  see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsawaitformultipleevents
 */
static int wait_native_events(WSAEVENT *events, const DWORD count, cplat_socket_wait_cancel *cancel,
                              const int timeout_ms, const uint64_t deadline_ms, cplat_error *detail_out)
{
    uint64_t remaining_ms;
    DWORD wait_ms;
    int remaining_result;

    if (cancel_is_signaled(cancel))
    {
        return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
    }

    remaining_result = get_deadline_remaining(timeout_ms, deadline_ms, &remaining_ms);
    if (remaining_result < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }
    if (remaining_result == 0)
    {
        return cplat_internal_error_report_winsock_error_as(detail_out, WSAETIMEDOUT, CPLAT_ERR_TIMEOUT);
    }

    wait_ms = (timeout_ms == CPLAT_WAIT_FOREVER || remaining_ms > SOCKET_WATCH_CANCEL_CHECK_MS)
                  ? SOCKET_WATCH_CANCEL_CHECK_MS
                  : (DWORD)remaining_ms;

    if (count == 0U)
    {
        Sleep(wait_ms);
        return cplat_internal_error_report_success(detail_out);
    }

    {
        const DWORD wait_result = WSAWaitForMultipleEvents(count, events, FALSE, wait_ms, FALSE);
        if (wait_result == WSA_WAIT_FAILED)
        {
            const int saved_error = WSAGetLastError();
            if (cancel_is_signaled(cancel))
            {
                return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED,
                                                                     CPLAT_ERR_CANCELED);
            }
            return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)saved_error);
        }
        if (wait_result != WSA_WAIT_TIMEOUT && wait_result < WSA_WAIT_EVENT_0 + count)
        {
            return cplat_internal_error_report_success(detail_out);
        }
    }
    return cplat_internal_error_report_success(detail_out);
}

/**
 *  @brief          受信待機の対象イベントを組み立てます。
 */
static DWORD make_read_wait_events(cplat_socket_watch *const *watches, const size_t count,
                                   cplat_socket_wait_cancel *cancel, WSAEVENT *events)
{
    DWORD used = 0U;

    if (cancel != NULL)
    {
        events[used++] = cancel->event;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        cplat_socket_watch *watch = watches[index];
        int duplicate = 0;
        if (watch == NULL)
        {
            continue;
        }
        for (size_t prior = 0U; prior < index; ++prior)
        {
            if (watches[prior] == watch)
            {
                duplicate = 1;
                break;
            }
        }
        if (duplicate != 0)
        {
            continue;
        }

        watch_lock(watch);
        if (watch->event_read_usable != 0)
        {
            events[used++] = watch->event_read;
        }
        events[used++] = watch->event_shared;
        watch_unlock(watch);
    }
    return used;
}

/**
 *  @brief          送信または接続待機の対象イベントを組み立てます。
 */
static DWORD make_one_wait_events(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel,
                                  WSAEVENT *events)
{
    DWORD used = 0U;

    if (cancel != NULL)
    {
        events[used++] = cancel->event;
    }
    watch_lock(watch);
    if (watch->event_write_usable != 0)
    {
        events[used++] = watch->event_write;
    }
    events[used++] = watch->event_shared;
    watch_unlock(watch);
    return used;
}

/**
 *  @brief          受信可能な登録を待機します。
 */
static int wait_readable_many(cplat_socket_watch *const *watches, const size_t count,
                              cplat_socket_wait_cancel *cancel, const int timeout_ms,
                              unsigned char *ready_out, cplat_error *detail_out)
{
    WSAEVENT wait_events[2U * CPLAT_SOCKET_WATCH_WAIT_MAX + 1U];
    uint64_t deadline_ms = 0U;
    uint64_t now;

    if (timeout_ms != CPLAT_WAIT_FOREVER && timeout_ms != CPLAT_NO_WAIT)
    {
        if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        deadline_ms = now + (uint64_t)timeout_ms;
    }

    for (;;)
    {
        int any_ready = 0;
        int first_wait_error = 0;

        if (cancel_is_signaled(cancel))
        {
            memset(ready_out, 0, count);
            return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
        }

        for (size_t index = 0U; index < count; ++index)
        {
            int wait_error = 0;
            if (watches[index] == NULL)
            {
                continue;
            }
            if (process_shared_event(watches[index], &wait_error) < 0)
            {
                first_wait_error = wait_error;
                break;
            }
        }
        if (first_wait_error != 0)
        {
            if (cancel_is_signaled(cancel))
            {
                memset(ready_out, 0, count);
                return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED,
                                                                     CPLAT_ERR_CANCELED);
            }
            return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)first_wait_error);
        }

        if (timeout_ms == CPLAT_NO_WAIT)
        {
            for (size_t index = 0U; index < count; ++index)
            {
                if (watches[index] != NULL)
                {
                    const int poll_result = poll_current_state(watches[index], (short)(POLLRDNORM | POLLIN), detail_out);
                    if (poll_result != CPLAT_OK)
                    {
                        return poll_result;
                    }
                }
            }
        }

        if (cancel_is_signaled(cancel))
        {
            memset(ready_out, 0, count);
            return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
        }
        memset(ready_out, 0, count);
        for (size_t index = 0U; index < count; ++index)
        {
            int is_ready;
            if (watches[index] == NULL)
            {
                continue;
            }
            const int result = inspect_direction(watches[index], SOCKET_WATCH_READ, &is_ready, NULL, NULL, detail_out);
            if (result != CPLAT_OK)
            {
                return result;
            }
            if (is_ready != 0)
            {
                for (size_t duplicate = 0U; duplicate < count; ++duplicate)
                {
                    if (watches[duplicate] == watches[index])
                    {
                        ready_out[duplicate] = 1U;
                    }
                }
                any_ready = 1;
            }
        }
        if (cancel_is_signaled(cancel))
        {
            memset(ready_out, 0, count);
            return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
        }
        if (any_ready != 0)
        {
            return cplat_internal_error_report_success(detail_out);
        }

        {
            uint64_t remaining_ms;
            const int deadline_result = get_deadline_remaining(timeout_ms, deadline_ms, &remaining_ms);
            if (deadline_result < 0)
            {
                return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }
            if (deadline_result == 0)
            {
                return cplat_internal_error_report_winsock_error_as(detail_out, WSAETIMEDOUT, CPLAT_ERR_TIMEOUT);
            }
        }
        if (timeout_ms == CPLAT_NO_WAIT)
        {
            return cplat_internal_error_report_winsock_error_as(detail_out, WSAETIMEDOUT, CPLAT_ERR_TIMEOUT);
        }

        {
            const DWORD wait_count = make_read_wait_events(watches, count, cancel, wait_events);
            const int wait_result = wait_native_events(wait_events, wait_count, cancel, timeout_ms, deadline_ms,
                                                       detail_out);
            if (wait_result != CPLAT_OK)
            {
                return wait_result;
            }
        }
    }
}

/**
 *  @brief          送信可能または接続完了まで 1 件の登録を待機します。
 */
static int wait_one_direction_with_deadline(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel,
                                            const int timeout_ms, const uint64_t deadline_ms,
                                            const socket_watch_direction direction, int *terminal_out,
                                            cplat_error *detail_out)
{
    WSAEVENT wait_events[3];
    if (terminal_out != NULL)
    {
        *terminal_out = 0;
    }

    for (;;)
    {
        int wait_error = 0;
        int is_ready;
        int terminal = 0;
        int connect_error = 0;
        int result;

        if (cancel_is_signaled(cancel))
        {
            return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
        }
        if (process_shared_event(watch, &wait_error) < 0)
        {
            if (cancel_is_signaled(cancel))
            {
                return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED,
                                                                     CPLAT_ERR_CANCELED);
            }
            return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)wait_error);
        }

        if (timeout_ms == CPLAT_NO_WAIT && direction != SOCKET_WATCH_CONNECT)
        {
            result = poll_current_state(watch, direction == SOCKET_WATCH_READ ? (short)(POLLRDNORM | POLLIN)
                                                                              : (short)(POLLWRNORM | POLLOUT),
                                       detail_out);
            if (result != CPLAT_OK)
            {
                return result;
            }
        }

        result = inspect_direction(watch, direction, &is_ready, &terminal, &connect_error, detail_out);
        if (result != CPLAT_OK)
        {
            return result;
        }
        if (terminal_out != NULL)
        {
            *terminal_out = terminal;
        }
        if (cancel_is_signaled(cancel))
        {
            return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
        }
        if (is_ready != 0)
        {
            if (direction == SOCKET_WATCH_CONNECT)
            {
                if (connect_error != 0)
                {
                    return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)connect_error);
                }
                if (terminal != 0)
                {
                    int terminal_error;
                    watch_lock(watch);
                    terminal_error = watch->event_error != 0 ? watch->event_error :
                                     watch->event_error_write != 0 ? watch->event_error_write : 0;
                    watch_unlock(watch);
                    if (terminal_error != 0)
                    {
                        return cplat_internal_error_report_winsock_error(detail_out,
                                                                         (unsigned long)terminal_error);
                    }
                    return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
                }
            }
            return cplat_internal_error_report_success(detail_out);
        }

        {
            uint64_t remaining_ms;
            const int deadline_result = get_deadline_remaining(timeout_ms, deadline_ms, &remaining_ms);
            if (deadline_result < 0)
            {
                return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }
            if (deadline_result == 0)
            {
                return cplat_internal_error_report_winsock_error_as(detail_out, WSAETIMEDOUT, CPLAT_ERR_TIMEOUT);
            }
        }
        if (timeout_ms == CPLAT_NO_WAIT)
        {
            return cplat_internal_error_report_winsock_error_as(detail_out, WSAETIMEDOUT, CPLAT_ERR_TIMEOUT);
        }

        {
            const DWORD wait_count = make_one_wait_events(watch, cancel, wait_events);
            const int wait_result = wait_native_events(wait_events, wait_count, cancel, timeout_ms, deadline_ms,
                                                       detail_out);
            if (wait_result != CPLAT_OK)
            {
                return wait_result;
            }
        }
    }
}

/**
 *  @brief          相対タイムアウトから絶対期限を一度だけ求めて待機します。
 */
static int wait_one_direction(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel, const int timeout_ms,
                              const socket_watch_direction direction, int *terminal_out, cplat_error *detail_out)
{
    uint64_t deadline_ms = 0U;
    uint64_t now;

    if (timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    if (timeout_ms != CPLAT_WAIT_FOREVER && timeout_ms != CPLAT_NO_WAIT)
    {
        if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        deadline_ms = now + (uint64_t)timeout_ms;
    }

    return wait_one_direction_with_deadline(watch, cancel, timeout_ms, deadline_ms, direction, terminal_out,
                                            detail_out);
}

/**
 *  @brief          送信可能の世代を記録して、非ブロッキング送信を行います。
 */
static int send_once(cplat_socket_watch *watch, const void *buf, const size_t len, size_t *sent_out,
                     const int use_sendto, const struct sockaddr *address, const int address_len,
                     const int retry_eintr, int *interrupted_out, cplat_error *detail_out)
{
    uint64_t observed_generation;
    int result;

    watch_lock(watch);
    observed_generation = watch->write_gen;
    watch_unlock(watch);
    if (interrupted_out != NULL)
    {
        *interrupted_out = 0;
    }

    for (;;)
    {
        int transferred;
        if (use_sendto != 0)
        {
            transferred = sendto(watch->sock, (const char *)buf, (int)len, 0, address, address_len);
        }
        else
        {
            transferred = send(watch->sock, (const char *)buf, (int)len, 0);
        }
        if (transferred != SOCKET_ERROR)
        {
            if (transferred < 0 || (size_t)transferred > len)
            {
                return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }
            *sent_out = (size_t)transferred;
            return cplat_internal_error_report_success(detail_out);
        }

        result = WSAGetLastError();
        if (result == WSAEINTR)
        {
            if (retry_eintr != 0)
            {
                continue;
            }
            if (interrupted_out != NULL)
            {
                *interrupted_out = 1;
                *sent_out = 0U;
                return cplat_internal_error_report_success(detail_out);
            }
        }
        if (result == WSAEWOULDBLOCK)
        {
            watch_lock(watch);
            if (watch->write_gen == observed_generation)
            {
                watch->writable = 0;
                if (watch->closed == 0 && watch->event_error == 0 && watch->event_error_write == 0 &&
                    (watch->connect_done == 0 || watch->connect_consumed != 0) && watch->event_write_usable != 0 &&
                    !WSAResetEvent(watch->event_write))
                {
                    const int saved_error = WSAGetLastError();
                    (void)saved_error;
                    watch->event_write_usable = 0;
                }
            }
            watch_unlock(watch);
        }
        return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)result);
    }
}

/**
 *  @brief          送受信の全量を解除と期限を確認しながら転送します。
 */
static int transfer_all(cplat_socket_watch *watch, void *buf, const size_t len, cplat_socket_wait_cancel *cancel,
                        const int timeout_ms, size_t *transferred_out, const int sending, cplat_error *detail_out)
{
    size_t transferred = 0U;
    uint64_t deadline_ms = 0U;
    uint64_t now;
    int terminal_hint = 0;

    if (transferred_out != NULL)
    {
        *transferred_out = 0U;
    }
    if (watch == NULL || watch->sock == INVALID_SOCKET || buf == NULL || transferred_out == NULL ||
        len > CPLAT_SOCKET_MAX_TRANSFER || timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    if (timeout_ms != CPLAT_WAIT_FOREVER)
    {
        if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        deadline_ms = now + (uint64_t)timeout_ms;
    }

    for (;;)
    {
        uint64_t remaining_ms;
        int deadline_result;
        int transferred_once;
        const size_t remaining = len - transferred;
        int terminal = 0;

        if (cancel_is_signaled(cancel))
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_winsock_error_as(detail_out, WSA_E_CANCELLED, CPLAT_ERR_CANCELED);
        }
        if (transferred == len)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_success(detail_out);
        }
        deadline_result = get_deadline_remaining(timeout_ms, deadline_ms, &remaining_ms);
        if (deadline_result < 0)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        if (deadline_result == 0)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_winsock_error_as(detail_out, WSAETIMEDOUT, CPLAT_ERR_TIMEOUT);
        }

        if (sending != 0)
        {
            size_t sent = 0U;
            int interrupted = 0;
            const int send_result = send_once(watch, (const uint8_t *)buf + transferred, remaining, &sent, 0, NULL, 0,
                                              0, &interrupted, detail_out);
            if (interrupted != 0)
            {
                continue;
            }
            if (send_result == CPLAT_ERR_WOULD_BLOCK)
            {
                transferred_once = -1;
            }
            else if (send_result != CPLAT_OK)
            {
                *transferred_out = transferred;
                return send_result;
            }
            else
            {
                if (sent == 0U)
                {
                    *transferred_out = transferred;
                    return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
                }
                transferred_once = (int)sent;
            }
        }
        else
        {
            transferred_once = recv(watch->sock, (char *)buf + transferred, (int)remaining, 0);
            if (transferred_once == SOCKET_ERROR)
            {
                const int saved_error = WSAGetLastError();
                if (saved_error == WSAEINTR)
                {
                    continue;
                }
                if (saved_error != WSAEWOULDBLOCK)
                {
                    *transferred_out = transferred;
                    return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)saved_error);
                }
                transferred_once = -1;
            }
            else if (transferred_once == 0)
            {
                *transferred_out = transferred;
                return cplat_internal_error_report_winsock_error_as(detail_out, WSAESHUTDOWN, CPLAT_ERR_EOF);
            }
        }

        if (transferred_once == -1)
        {
            if (terminal_hint != 0)
            {
                *transferred_out = transferred;
                return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }
            deadline_result = get_deadline_remaining(timeout_ms, deadline_ms, &remaining_ms);
            if (deadline_result <= 0)
            {
                *transferred_out = transferred;
                return deadline_result == 0 ?
                           cplat_internal_error_report_winsock_error_as(detail_out, WSAETIMEDOUT,
                                                                        CPLAT_ERR_TIMEOUT) :
                           cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }
            {
                const int wait_result = wait_one_direction_with_deadline(
                    watch, cancel, timeout_ms, deadline_ms,
                    sending != 0 ? SOCKET_WATCH_WRITE : SOCKET_WATCH_READ, &terminal, detail_out);
                if (wait_result != CPLAT_OK)
                {
                    *transferred_out = transferred;
                    return wait_result;
                }
                terminal_hint = terminal;
            }
            continue;
        }
        if (transferred_once < 0 || (size_t)transferred_once > remaining)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        transferred += (size_t)transferred_once;
        terminal_hint = 0;
    }
}

#endif /* PLATFORM_WINDOWS */
