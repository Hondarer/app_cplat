/**
 *******************************************************************************
 *  @file           endpoint_windows.c
 *  @brief          cplat/net/endpoint.h が宣言する IPv4 の通信エンドポイント API のうち、
 *                  アドレス解析、名前解決、文字列整形の Windows 実装を提供します。
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
    #include <cplat/net/endpoint.h>
    #include <cplat/net/socket_internal.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ipv4_parse(const char *text, uint32_t *address_out)
{
    struct in_addr parsed;

    if ((text == NULL) || (address_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_internal_socket_startup(NULL) != CPLAT_OK)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* inet_pton は形式が正しい場合にだけ 1 を返す。0 は形式不正、負値は AF 不正を表す。
       see: https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-inet_pton */
    if (inet_pton(AF_INET, text, &parsed) != 1)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    memcpy(address_out, &parsed.S_un.S_addr, sizeof(*address_out));

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ipv4_resolve(const char *text, uint32_t *address_out, cplat_error *detail_out)
{
    ADDRINFOA hints = {0};
    PADDRINFOA resolved = NULL;
    const struct sockaddr_in *sin = NULL;
    int startup_result;
    int gai_result;

    if ((text == NULL) || (address_out == NULL))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    startup_result = cplat_internal_socket_startup(detail_out);
    if (startup_result != CPLAT_OK)
    {
        return startup_result;
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    gai_result = getaddrinfo(text, NULL, &hints, &resolved);
    if (gai_result != 0)
    {
        if (resolved != NULL)
        {
            freeaddrinfo(resolved);
        }
        return cplat_internal_error_report_gai_error(detail_out, gai_result);
    }

    if (resolved == NULL)
    {
        return cplat_internal_error_report_gai_error(detail_out, WSAHOST_NOT_FOUND);
    }

    /* 複数アドレスが返された場合は先頭を採用する */
    sin = (const struct sockaddr_in *)(const void *)resolved->ai_addr;
    memcpy(address_out, &sin->sin_addr.S_un.S_addr, sizeof(*address_out));

    freeaddrinfo(resolved);

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ipv4_to_string(const uint32_t address, char *buffer, const size_t buffer_size, cplat_error *detail_out)
{
    struct in_addr value;
    int startup_result;

    if ((buffer == NULL) || (buffer_size == 0U))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    if (buffer_size < (size_t)CPLAT_IPV4_ADDR_STRLEN)
    {
        return cplat_internal_error_report_errno_as(detail_out, ERANGE, CPLAT_ERR_BUFFER_TOO_SMALL);
    }

    startup_result = cplat_internal_socket_startup(detail_out);
    if (startup_result != CPLAT_OK)
    {
        return startup_result;
    }

    memcpy(&value.S_un.S_addr, &address, sizeof(value.S_un.S_addr));

    if (inet_ntop(AF_INET, &value, buffer, buffer_size) == NULL)
    {
        return cplat_internal_error_report_winsock_error(detail_out, (unsigned long)WSAGetLastError());
    }

    return cplat_internal_error_report_success(detail_out);
}

#endif /* PLATFORM_WINDOWS */
