/**
 *******************************************************************************
 *  @file           error_message.c
 *  @brief          結果コードと OS エラー値の文字列化を実装します。
 *
 *  共通結果コードの名称と、errno および Win32 エラー コードのメッセージを提供します。
 *
 *******************************************************************************
 */

#include <cplat/base/error_message.h>
#include <cplat/crt/stdlib.h>
#include <cplat/base/error_message_internal.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>

#include <cplat/crt/string.h>

#include <errno.h>
#include <string.h>

/* gai_strerror() を参照するために取り込む。EAI_* の文字列化に必要となる。 */
#if defined(PLATFORM_LINUX)
    #include <netdb.h>
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/wchar_conv.h>
    #include <stdlib.h>
#endif /* PLATFORM_WINDOWS */

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_result_to_string(const int result)
{
    const char *text;

    switch (result)
    {
    case CPLAT_OK:
        text = "success";
        break;
    case CPLAT_ERR_UNKNOWN:
        text = "unknown error";
        break;
    case CPLAT_ERR_INVALID_ARGUMENT:
        text = "invalid argument";
        break;
    case CPLAT_ERR_UNSUPPORTED:
        text = "unsupported";
        break;
    case CPLAT_ERR_PERMISSION_DENIED:
        text = "permission denied";
        break;
    case CPLAT_ERR_DUPLICATE_DEFINITION:
        text = "duplicate definition";
        break;
    case CPLAT_ERR_DUPLICATE_KEY:
        text = "duplicate key";
        break;
    case CPLAT_ERR_SIGNATURE_MISMATCH:
        text = "signature mismatch";
        break;
    case CPLAT_ERR_VERSION_MISMATCH:
        text = "version mismatch";
        break;
    case CPLAT_ERR_OUT_OF_MEMORY:
        text = "out of memory";
        break;
    case CPLAT_ERR_BUSY:
        text = "resource busy";
        break;
    case CPLAT_ERR_TIMEOUT:
        text = "timeout";
        break;
    case CPLAT_ERR_LIMIT_EXCEEDED:
        text = "limit exceeded";
        break;
    case CPLAT_ERR_BUFFER_TOO_SMALL:
        text = "buffer too small";
        break;
    case CPLAT_ERR_CORRUPT_DESCRIPTOR:
        text = "corrupt descriptor";
        break;
    case CPLAT_ERR_STORAGE_FULL:
        text = "storage full";
        break;
    case CPLAT_ERR_UNKNOWN_OPTION:
        text = "unknown option";
        break;
    case CPLAT_ERR_MISSING_VALUE:
        text = "missing value";
        break;
    case CPLAT_ERR_UNEXPECTED_VALUE:
        text = "unexpected value";
        break;
    case CPLAT_ERR_INVALID_INTEGER:
        text = "invalid integer";
        break;
    case CPLAT_ERR_OUT_OF_RANGE:
        text = "out of range";
        break;
    case CPLAT_ERR_MISSING_REQUIRED:
        text = "missing required item";
        break;
    case CPLAT_ERR_DUPLICATE_OPTION:
        text = "duplicate option";
        break;
    case CPLAT_ERR_TOO_MANY_ARGUMENTS:
        text = "too many arguments";
        break;
    case CPLAT_ERR_TOO_MANY_OCCURRENCES:
        text = "too many occurrences";
        break;
    case CPLAT_ERR_INVALID_PATTERN:
        text = "invalid pattern";
        break;
    case CPLAT_ERR_INVALID_ENCODING:
        text = "invalid encoding";
        break;
    case CPLAT_ERR_MALFORMED_DEFINITION:
        text = "malformed definition";
        break;
    case CPLAT_ERR_EOF:
        text = "end of input";
        break;
    case CPLAT_ERR_CANCELED:
        text = "canceled";
        break;
    case CPLAT_ERR_IN_PROGRESS:
        text = "in progress";
        break;
    default:
        text = "unknown result code";
        break;
    }

    return text;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_error_message(char *buf, const size_t buf_size, const cplat_error *error)
{
    int result;

    if ((buf == NULL) || (buf_size == 0U) || (error == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (error->domain == CPLAT_ERROR_DOMAIN_NONE)
    {
        const char message[] = "no error";
        size_t copy_size = sizeof(message);

        if (copy_size > buf_size)
        {
            copy_size = buf_size;
        }
        memcpy(buf, message, copy_size);
        buf[copy_size - 1U] = '\0';
        result = CPLAT_OK;
    }
    else if ((error->domain == CPLAT_ERROR_DOMAIN_ERRNO) || (error->domain == CPLAT_ERROR_DOMAIN_SOCKET_ERRNO))
    {
        result = cplat_internal_errno_message(buf, buf_size, (int)error->code);
    }
    else if (error->domain == CPLAT_ERROR_DOMAIN_GAI)
    {
#if defined(PLATFORM_LINUX)
        /* glibc の gai_strerror() はスレッド セーフであり、静的な定数文字列を返す。
           see: https://man7.org/linux/man-pages/man3/gai_strerror.3.html */
        result = cplat_strcpy(buf, buf_size, gai_strerror((int)error->code));
#elif defined(PLATFORM_WINDOWS)
        /* Windows の EAI_* は Winsock エラー コードと同一値のため FormatMessage で
           文字列化できる。gai_strerrorA() は静的バッファーを使用しスレッド セーフでない。
           see: https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-gai_strerrora */
        result = cplat_internal_win32_error_message(buf, buf_size, error->code);
#else
        buf[0] = '\0';
        result = CPLAT_ERR_INVALID_ARGUMENT;
#endif /* PLATFORM_ */
    }
    else if ((error->domain == CPLAT_ERROR_DOMAIN_WINDOWS) || (error->domain == CPLAT_ERROR_DOMAIN_WINSOCK))
    {
        /* Winsock エラーも Win32 と同じく FormatMessage が文字列化できる。
           see: https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2 */
#if defined(PLATFORM_WINDOWS)
        result = cplat_internal_win32_error_message(buf, buf_size, error->code);
#else
        buf[0] = '\0';
        result = CPLAT_ERR_INVALID_ARGUMENT;
#endif
    }
    else
    {
        buf[0] = '\0';
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_errno_message(char *buf, const size_t buf_size, const int errno_value)
{
    if (buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    buf[0] = '\0';

#if defined(PLATFORM_LINUX)
    {
    /* strerror_r には 2 系統がある。XSI 準拠版は int を返して buf へ書き込み、    */
    /* GNU 拡張版は char * を返す (buf を使わず静的文字列を返すことがある)。       */
    /* glibc は _GNU_SOURCE 定義時のみ GNU 版を提供する。                          */
    /* see: https://man7.org/linux/man-pages/man3/strerror_r.3.html                */
    #if defined(__GLIBC__) && defined(_GNU_SOURCE)
        const char *message = strerror_r(errno_value, buf, buf_size);

        if (message == NULL)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        if (message != buf)
        {
            size_t len = strlen(message);

            if (len >= buf_size)
            {
                len = buf_size - 1U;
            }
            memcpy(buf, message, len);
            buf[len] = '\0';
        }
        return CPLAT_OK;
    #else
        /* XSI 版は成功で 0 を返す。ERANGE は切り詰めを表すため成功として扱う。 */
        int xsi_result = strerror_r(errno_value, buf, buf_size);

        if (xsi_result != 0 && xsi_result != ERANGE)
        {
            buf[0] = '\0';
            return CPLAT_ERR_UNKNOWN;
        }
        return CPLAT_OK;
    #endif /* __GLIBC__ && _GNU_SOURCE */
    }
#elif defined(PLATFORM_WINDOWS)
    {
        /* strerror_s はバッファーが不足する場合も切り詰めて NUL 終端する。 */
        if (strerror_s(buf, buf_size, errno_value) != 0)
        {
            buf[0] = '\0';
            return CPLAT_ERR_UNKNOWN;
        }
        return CPLAT_OK;
    }
#endif /* PLATFORM_ */
}

#if defined(PLATFORM_WINDOWS)

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_win32_error_message(char *buf, const size_t buf_size, const unsigned long error_code)
{
    wchar_t *wmessage = NULL;
    DWORD wlen;
    char *utf8 = NULL;
    size_t len;

    if (buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    buf[0] = '\0';

    /* ALLOCATE_BUFFER 指定時、lpBuffer には確保先ポインターのアドレスを渡す。 */
    /* see: https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-formatmessagew */
    wlen =
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, (DWORD)error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&wmessage, 0, NULL);
    if (wlen == 0 || wmessage == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /* FormatMessageW のメッセージは末尾に改行を含むため取り除く */
    while (wlen > 0 && (wmessage[wlen - 1] == L'\r' || wmessage[wlen - 1] == L'\n'))
    {
        wmessage[wlen - 1] = L'\0';
        wlen--;
    }

    utf8 = cplat_wstr_to_utf8_alloc(wmessage);
    LocalFree(wmessage);
    if (utf8 == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    len = strlen(utf8);
    if (len >= buf_size)
    {
        len = buf_size - 1U;
    }
    memcpy(buf, utf8, len);
    buf[len] = '\0';
    cplat_free(utf8);

    return CPLAT_OK;
}

#endif /* PLATFORM_WINDOWS */
