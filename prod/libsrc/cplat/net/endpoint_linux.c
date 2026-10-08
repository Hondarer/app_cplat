/**
 *******************************************************************************
 *  @file           endpoint_linux.c
 *  @brief          cplat/net/endpoint.h が宣言する IPv4 の通信エンドポイント API のうち、
 *                  アドレス解析、名前解決、文字列整形の Linux 実装を提供します。
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <arpa/inet.h>
    #include <errno.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <string.h>
    #include <sys/socket.h>

    #include <cplat/base/error_internal.h>
    #include <cplat/base/result.h>
    #include <cplat/net/endpoint.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ipv4_parse(const char *text, uint32_t *address_out)
{
    struct in_addr parsed;

    if ((text == NULL) || (address_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* inet_pton は形式が正しい場合にだけ 1 を返す。0 は形式不正、負値は AF 不正を表す。
       see: https://pubs.opengroup.org/onlinepubs/9699919799/functions/inet_ntop.html */
    if (inet_pton(AF_INET, text, &parsed) != 1)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    memcpy(address_out, &parsed.s_addr, sizeof(*address_out));

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ipv4_resolve(const char *text, uint32_t *address_out, cplat_error *detail_out)
{
    struct addrinfo hints = {0};
    struct addrinfo *resolved = NULL;
    const struct sockaddr_in *sin = NULL;
    int gai_result;

    if ((text == NULL) || (address_out == NULL))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    /* EAI_SYSTEM は errno に要因が入る。シグナルによる中断であれば再試行し、
       利用者へ中断を観測させない。 */
    do
    {
        gai_result = getaddrinfo(text, NULL, &hints, &resolved);
    } while ((gai_result == EAI_SYSTEM) && (errno == EINTR));

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
        return cplat_internal_error_report_gai_error(detail_out, EAI_NONAME);
    }

    /* 複数アドレスが返された場合は先頭を採用する */
    sin = (const struct sockaddr_in *)(const void *)resolved->ai_addr;
    memcpy(address_out, &sin->sin_addr.s_addr, sizeof(*address_out));

    freeaddrinfo(resolved);

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ipv4_to_string(const uint32_t address, char *buffer, const size_t buffer_size, cplat_error *detail_out)
{
    struct in_addr value;

    if ((buffer == NULL) || (buffer_size == 0U))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    if (buffer_size < (size_t)CPLAT_IPV4_ADDR_STRLEN)
    {
        return cplat_internal_error_report_errno_as(detail_out, ERANGE, CPLAT_ERR_BUFFER_TOO_SMALL);
    }

    memcpy(&value.s_addr, &address, sizeof(value.s_addr));

    if (inet_ntop(AF_INET, &value, buffer, (socklen_t)buffer_size) == NULL)
    {
        return cplat_internal_error_report_socket_errno(detail_out, errno);
    }

    return cplat_internal_error_report_success(detail_out);
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif /* PLATFORM_ */
