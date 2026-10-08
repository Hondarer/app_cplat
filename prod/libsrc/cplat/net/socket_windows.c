/**
 *******************************************************************************
 *  @file           socket_windows.c
 *  @brief          cplat/net/socket.h が宣言する IPv4 ソケットの生成、接続、
 *                  送受信、待機を行う API の Windows 実装を提供します。
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <errno.h>
    #include <string.h>

    #include <cplat/base/error_internal.h>
    #include <cplat/base/result.h>
    #include <cplat/net/socket.h>
    #include <cplat/net/socket_internal.h>
    #include <cplat/sync/sync.h>

/** Winsock の初期化を 1 回に限定するためのフラグ。 */
static cplat_once_flag s_startup_once = {0};

/** WSAStartup() の結果。0 は成功、それ以外は Winsock エラー コード。 */
static unsigned long s_startup_error = 0UL;

/** WSAStartup() が成功したことを示す値。 */
static int s_startup_done = 0;

/**
 *  @brief          Winsock を 1 回だけ初期化します。
 */
static void startup_once(void)
{
    WSADATA wsa_data = {0};
    int startup_result;

    startup_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup_result != 0)
    {
        /* WSAStartup() は失敗理由を戻り値で返す。この時点では WSAGetLastError() を
           呼び出せないため、戻り値をそのまま記録する。
           see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsastartup */
        s_startup_error = (unsigned long)startup_result;
        return;
    }

    s_startup_done = 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_socket_startup(cplat_error *detail_out)
{
    cplat_call_once(&s_startup_once, startup_once);

    if (s_startup_done == 0)
    {
        return cplat_internal_error_report_winsock_error(detail_out, s_startup_error);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_socket_cleanup(void)
{
    if (s_startup_done == 0)
    {
        return;
    }

    (void)WSACleanup();
    s_startup_done = 0;
}

/**
 *  @brief          共通のエンドポイント表現を sockaddr_in へ変換します。
 *  @param[in]      endpoint 変換元のエンドポイント。
 *  @param[out]     native   変換後の sockaddr_in の格納先。
 */
static void endpoint_to_native(const cplat_ipv4_endpoint *endpoint, struct sockaddr_in *native)
{
    memset(native, 0, sizeof(*native));
    native->sin_family = AF_INET;
    memcpy(&native->sin_addr.S_un.S_addr, &endpoint->address, sizeof(native->sin_addr.S_un.S_addr));
    memcpy(&native->sin_port, &endpoint->port, sizeof(native->sin_port));
}

/**
 *  @brief          sockaddr_in を共通のエンドポイント表現へ変換します。
 *  @param[in]      native   変換元の sockaddr_in。
 *  @param[out]     endpoint 変換後のエンドポイントの格納先。
 */
static void endpoint_from_native(const struct sockaddr_in *native, cplat_ipv4_endpoint *endpoint)
{
    memset(endpoint, 0, sizeof(*endpoint));
    memcpy(&endpoint->address, &native->sin_addr.S_un.S_addr, sizeof(endpoint->address));
    memcpy(&endpoint->port, &native->sin_port, sizeof(endpoint->port));
}

/**
 *  @brief          直前の Winsock エラーを記録します。
 *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
 *  @return         共通結果コードを返します。
 */
static int report_last_winsock_error(cplat_error *detail_out)
{
    return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)WSAGetLastError());
}

/**
 *  @brief          connect の非同期継続状態を共通結果コードへ変換します。
 *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
 *  @param[in]      error_code WSAGetLastError() が返した値。
 *  @return         共通結果コードを返します。
 *
 *  WSAEWOULDBLOCK は一般のソケット操作では WOULD_BLOCK ですが、
 *  非ブロッキング connect では接続処理が継続中であることを表します。
 *  生の Winsock エラーは detail_out に保持し、戻り値だけを IN_PROGRESS へ正規化します。
 *  see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-connect
 */
static int report_connect_winsock_error(cplat_error *detail_out, const unsigned long error_code)
{
    if ((error_code == (unsigned long)WSAEWOULDBLOCK) || (error_code == (unsigned long)WSAEINPROGRESS) ||
        (error_code == (unsigned long)WSAEALREADY))
    {
        return cplat_internal_error_report_winsock_error_as(detail_out, error_code, CPLAT_ERR_IN_PROGRESS);
    }

    return cplat_internal_error_report_winsock_error(detail_out, error_code);
}

/**
 *  @brief          ソケット オプションへ整数値を設定します。
 *  @param[in]      sock       対象のソケット。
 *  @param[in]      level      オプションの階層。
 *  @param[in]      optname    オプション名。
 *  @param[in]      value      設定する値。
 *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
 *  @return         共通結果コードを返します。
 */
static int set_int_option(cplat_socket sock, int level, int optname, int value, cplat_error *detail_out)
{
    if (sock == CPLAT_INVALID_SOCKET)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    if (setsockopt((SOCKET)sock, level, optname, (const char *)&value, (int)sizeof(value)) == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    return cplat_internal_error_report_success(detail_out);
}

/**
 *  @brief          単一のソケットに対して WSAPoll による待機を行います。
 *  @param[in]      sock       対象のソケット。
 *  @param[in]      events     待機するイベント。
 *  @param[in]      timeout_ms タイムアウト時間 (ミリ秒)。
 *  @param[out]     ready_out  条件が成立した場合に 1、タイムアウトした場合に 0 を格納します。
 *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
 *  @return         共通結果コードを返します。
 */
static int wait_single(cplat_socket sock, short events, int timeout_ms, int *ready_out, cplat_error *detail_out)
{
    WSAPOLLFD poll_fd;
    int poll_result;

    if ((sock == CPLAT_INVALID_SOCKET) || (ready_out == NULL))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *ready_out = 0;

    poll_fd.fd = (SOCKET)sock;
    poll_fd.events = events;
    poll_fd.revents = 0;

    poll_result = WSAPoll(&poll_fd, (ULONG)1, timeout_ms);
    if (poll_result == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    if ((poll_result > 0) && ((poll_fd.revents & events) != 0))
    {
        *ready_out = 1;
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_open(const cplat_socket_kind kind, cplat_socket *sock_out, cplat_error *detail_out)
{
    int native_type;
    int startup_result;
    SOCKET native_socket;

    if (sock_out == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *sock_out = CPLAT_INVALID_SOCKET;

    if (kind == CPLAT_SOCKET_TCP)
    {
        native_type = SOCK_STREAM;
    }
    else if (kind == CPLAT_SOCKET_UDP)
    {
        native_type = SOCK_DGRAM;
    }
    else
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    startup_result = cplat_internal_socket_startup(detail_out);
    if (startup_result != CPLAT_OK)
    {
        return startup_result;
    }

    native_socket = socket(AF_INET, native_type, 0);
    if (native_socket == INVALID_SOCKET)
    {
        return report_last_winsock_error(detail_out);
    }

    *sock_out = (cplat_socket)native_socket;

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_socket_close(const cplat_socket sock)
{
    cplat_error saved;

    if (sock == CPLAT_INVALID_SOCKET)
    {
        return;
    }

    /* 解放経路で呼び出しても呼び出し前の診断情報が失われないように保存と復元を行う。 */
    cplat_error_get_last(&saved);
    (void)closesocket((SOCKET)sock);
    cplat_error_set_last(&saved);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_socket_shutdown(const cplat_socket sock)
{
    if (sock == CPLAT_INVALID_SOCKET)
    {
        return;
    }

    /* 失敗しても呼び出し側に取るべき手段がないため、結果は参照しない。 */
    (void)shutdown((SOCKET)sock, SD_BOTH);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_bind(const cplat_socket sock, const cplat_ipv4_endpoint *endpoint, cplat_error *detail_out)
{
    struct sockaddr_in native;

    if ((sock == CPLAT_INVALID_SOCKET) || (endpoint == NULL))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    endpoint_to_native(endpoint, &native);

    if (bind((SOCKET)sock, (const struct sockaddr *)&native, (int)sizeof(native)) == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_listen(const cplat_socket sock, const int backlog, cplat_error *detail_out)
{
    int native_backlog = backlog;

    if ((sock == CPLAT_INVALID_SOCKET) || (backlog < 0))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    if (backlog == CPLAT_SOCKET_BACKLOG_DEFAULT)
    {
        native_backlog = SOMAXCONN;
    }

    if (listen((SOCKET)sock, native_backlog) == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_accept(const cplat_socket sock, cplat_ipv4_endpoint *peer_out, cplat_socket *sock_out,
                        cplat_error *detail_out)
{
    struct sockaddr_in native = {0};
    int native_len = (int)sizeof(native);
    SOCKET accepted;

    if ((sock == CPLAT_INVALID_SOCKET) || (sock_out == NULL))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *sock_out = CPLAT_INVALID_SOCKET;

    accepted = accept((SOCKET)sock, (struct sockaddr *)&native, &native_len);
    if (accepted == INVALID_SOCKET)
    {
        return report_last_winsock_error(detail_out);
    }

    if (peer_out != NULL)
    {
        endpoint_from_native(&native, peer_out);
    }

    *sock_out = (cplat_socket)accepted;

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_connect(const cplat_socket sock, const cplat_ipv4_endpoint *endpoint, cplat_error *detail_out)
{
    struct sockaddr_in native;

    if ((sock == CPLAT_INVALID_SOCKET) || (endpoint == NULL))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    endpoint_to_native(endpoint, &native);

    if (connect((SOCKET)sock, (const struct sockaddr *)&native, (int)sizeof(native)) == SOCKET_ERROR)
    {
        return report_connect_winsock_error(detail_out, (unsigned long)WSAGetLastError());
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_get_pending_error(const cplat_socket sock, cplat_error *detail_out)
{
    int pending = 0;
    int pending_len = (int)sizeof(pending);

    if (sock == CPLAT_INVALID_SOCKET)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    if (getsockopt((SOCKET)sock, SOL_SOCKET, SO_ERROR, (char *)&pending, &pending_len) == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    if (pending != 0)
    {
        return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)pending);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_set_nonblocking(const cplat_socket sock, const int enable, cplat_error *detail_out)
{
    u_long mode = 0UL;

    if (sock == CPLAT_INVALID_SOCKET)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    if (enable != 0)
    {
        mode = 1UL;
    }

    /* Windows には fcntl(F_SETFL) 相当がないため、FIONBIO で切り替える。
       see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-ioctlsocket */
    if (ioctlsocket((SOCKET)sock, (long)FIONBIO, &mode) == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_set_reuse_address(const cplat_socket sock, const int enable, cplat_error *detail_out)
{
    int value = 0;

    if (enable != 0)
    {
        value = 1;
    }

    return set_int_option(sock, SOL_SOCKET, SO_REUSEADDR, value, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_set_broadcast(const cplat_socket sock, const int enable, cplat_error *detail_out)
{
    int value = 0;

    if (enable != 0)
    {
        value = 1;
    }

    return set_int_option(sock, SOL_SOCKET, SO_BROADCAST, value, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_set_multicast_interface(const cplat_socket sock, const uint32_t interface_address,
                                         cplat_error *detail_out)
{
    struct in_addr value = {0};

    if (sock == CPLAT_INVALID_SOCKET)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    memcpy(&value.S_un.S_addr, &interface_address, sizeof(value.S_un.S_addr));

    if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&value, (int)sizeof(value)) == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_join_multicast_group(const cplat_socket sock, const uint32_t group_address,
                                      const uint32_t interface_address, cplat_error *detail_out)
{
    struct ip_mreq request = {0};

    if (sock == CPLAT_INVALID_SOCKET)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    memcpy(&request.imr_multiaddr.S_un.S_addr, &group_address, sizeof(request.imr_multiaddr.S_un.S_addr));
    memcpy(&request.imr_interface.S_un.S_addr, &interface_address, sizeof(request.imr_interface.S_un.S_addr));

    if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&request, (int)sizeof(request)) ==
        SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_leave_multicast_group(const cplat_socket sock, const uint32_t group_address,
                                       const uint32_t interface_address, cplat_error *detail_out)
{
    struct ip_mreq request = {0};

    if (sock == CPLAT_INVALID_SOCKET)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    memcpy(&request.imr_multiaddr.S_un.S_addr, &group_address, sizeof(request.imr_multiaddr.S_un.S_addr));
    memcpy(&request.imr_interface.S_un.S_addr, &interface_address, sizeof(request.imr_interface.S_un.S_addr));

    if (setsockopt((SOCKET)sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char *)&request, (int)sizeof(request)) ==
        SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_send(const cplat_socket sock, const void *buf, const size_t len, size_t *sent_out,
                      cplat_error *detail_out)
{
    int transferred;

    if ((sock == CPLAT_INVALID_SOCKET) || (buf == NULL) || (sent_out == NULL) || (len > CPLAT_SOCKET_MAX_TRANSFER))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *sent_out = 0U;

    transferred = send((SOCKET)sock, (const char *)buf, (int)len, 0);
    if (transferred == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    /* OS が要求量を超える転送量を返すことはないが、出力値を守るため異常として扱う。 */
    if ((size_t)transferred > len)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }

    *sent_out = (size_t)transferred;

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_recv(const cplat_socket sock, void *buf, const size_t len, size_t *received_out,
                      cplat_error *detail_out)
{
    int transferred;

    if ((sock == CPLAT_INVALID_SOCKET) || (buf == NULL) || (received_out == NULL) || (len > CPLAT_SOCKET_MAX_TRANSFER))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *received_out = 0U;

    transferred = recv((SOCKET)sock, (char *)buf, (int)len, 0);
    if (transferred == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    /* OS が要求量を超える転送量を返すことはないが、出力値を守るため異常として扱う。 */
    if ((size_t)transferred > len)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }

    *received_out = (size_t)transferred;

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_sendto(const cplat_socket sock, const void *buf, const size_t len, const cplat_ipv4_endpoint *endpoint,
                        size_t *sent_out, cplat_error *detail_out)
{
    struct sockaddr_in native;
    int transferred;

    if ((sock == CPLAT_INVALID_SOCKET) || (buf == NULL) || (endpoint == NULL) || (sent_out == NULL) ||
        (len > CPLAT_SOCKET_MAX_TRANSFER))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *sent_out = 0U;
    endpoint_to_native(endpoint, &native);

    transferred =
        sendto((SOCKET)sock, (const char *)buf, (int)len, 0, (const struct sockaddr *)&native, (int)sizeof(native));
    if (transferred == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    /* OS が要求量を超える転送量を返すことはないが、出力値を守るため異常として扱う。 */
    if ((size_t)transferred > len)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }

    *sent_out = (size_t)transferred;

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_recvfrom(const cplat_socket sock, void *buf, const size_t len, cplat_ipv4_endpoint *peer_out,
                          size_t *received_out, cplat_error *detail_out)
{
    struct sockaddr_in native = {0};
    int native_len = (int)sizeof(native);
    int transferred;

    if ((sock == CPLAT_INVALID_SOCKET) || (buf == NULL) || (received_out == NULL) || (len > CPLAT_SOCKET_MAX_TRANSFER))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *received_out = 0U;

    transferred = recvfrom((SOCKET)sock, (char *)buf, (int)len, 0, (struct sockaddr *)&native, &native_len);
    if (transferred == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    /* OS が要求量を超える転送量を返すことはないが、出力値を守るため異常として扱う。 */
    if ((size_t)transferred > len)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }

    if (peer_out != NULL)
    {
        endpoint_from_native(&native, peer_out);
    }

    *received_out = (size_t)transferred;

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_send_all(const cplat_socket sock, const void *buf, const size_t len, cplat_error *detail_out)
{
    const char *cursor = (const char *)buf;
    size_t sent = 0U;

    if ((sock == CPLAT_INVALID_SOCKET) || (buf == NULL) || (len > CPLAT_SOCKET_MAX_TRANSFER))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    while (sent < len)
    {
        const size_t remaining = len - sent;
        int transferred = send((SOCKET)sock, cursor + sent, (int)remaining, 0);

        if (transferred == SOCKET_ERROR)
        {
            return report_last_winsock_error(detail_out);
        }
        if (transferred == 0)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        /* OS が要求量を超える転送量を返すことはないが、残量の計算を守るため異常として扱う。 */
        if ((size_t)transferred > remaining)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }

        sent += (size_t)transferred;
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_recv_all(const cplat_socket sock, void *buf, const size_t len, cplat_error *detail_out)
{
    char *cursor = (char *)buf;
    size_t received = 0U;

    if ((sock == CPLAT_INVALID_SOCKET) || (buf == NULL) || (len > CPLAT_SOCKET_MAX_TRANSFER))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    while (received < len)
    {
        const size_t remaining = len - received;
        int transferred = recv((SOCKET)sock, cursor + received, (int)remaining, 0);

        if (transferred == SOCKET_ERROR)
        {
            return report_last_winsock_error(detail_out);
        }
        if (transferred == 0)
        {
            return cplat_internal_error_report_errno_as(detail_out, 0, CPLAT_ERR_EOF);
        }
        /* OS が要求量を超える転送量を返すことはないが、残量の計算を守るため異常として扱う。 */
        if ((size_t)transferred > remaining)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }

        received += (size_t)transferred;
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_readable(const cplat_socket sock, const int timeout_ms, int *ready_out, cplat_error *detail_out)
{
    return wait_single(sock, (short)POLLRDNORM, timeout_ms, ready_out, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_writable(const cplat_socket sock, const int timeout_ms, int *ready_out, cplat_error *detail_out)
{
    return wait_single(sock, (short)POLLWRNORM, timeout_ms, ready_out, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_readable_multi(const cplat_socket *socks, const size_t count, const int timeout_ms,
                                     unsigned char *ready_out, cplat_error *detail_out)
{
    WSAPOLLFD poll_fds[CPLAT_SOCKET_WAIT_MAX];
    size_t valid_count = 0U;
    size_t index;
    int poll_result;

    if ((socks == NULL) || (ready_out == NULL) || (count == 0U) || (count > CPLAT_SOCKET_WAIT_MAX))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    for (index = 0U; index < count; ++index)
    {
        ready_out[index] = 0U;
        if (socks[index] != CPLAT_INVALID_SOCKET)
        {
            poll_fds[valid_count].fd = (SOCKET)socks[index];
            poll_fds[valid_count].events = (short)POLLRDNORM;
            poll_fds[valid_count].revents = 0;
            ++valid_count;
        }
    }

    if (valid_count == 0U)
    {
        /* 有効なソケットがない場合も timeout_ms だけ待つ。即座に返すと呼び出し側の
           ポーリング ループが待機なしで回り続けるため。 */
        if (timeout_ms > 0)
        {
            cplat_sleep_ms(timeout_ms);
        }
        return cplat_internal_error_report_success(detail_out);
    }

    poll_result = WSAPoll(poll_fds, (ULONG)valid_count, timeout_ms);
    if (poll_result == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    valid_count = 0U;
    for (index = 0U; index < count; ++index)
    {
        if (socks[index] != CPLAT_INVALID_SOCKET)
        {
            const short revents = poll_fds[valid_count].revents;

            if ((revents & (POLLRDNORM | POLLERR | POLLHUP | POLLNVAL)) != 0)
            {
                ready_out[index] = 1U;
            }
            ++valid_count;
        }
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_shutdown_receive(cplat_socket *sock_inout, cplat_error *detail_out)
{
    if ((sock_inout == NULL) || (*sock_inout == CPLAT_INVALID_SOCKET))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    /* Windows は受信方向の半クローズでは待機中の recv が解除されないため、
       ソケットを閉じて呼び出し側へ無効値を書き戻す。 */
    if (closesocket((SOCKET)*sock_inout) == SOCKET_ERROR)
    {
        return report_last_winsock_error(detail_out);
    }

    *sock_inout = CPLAT_INVALID_SOCKET;

    return cplat_internal_error_report_success(detail_out);
}

#endif /* PLATFORM_WINDOWS */
