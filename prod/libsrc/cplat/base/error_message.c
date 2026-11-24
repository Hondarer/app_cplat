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
    case CPLAT_ERR_IDENTITY_MISMATCH:
        text = "identity mismatch";
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
    case CPLAT_ERR_WOULD_BLOCK:
        text = "operation would block";
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
        /*
         * glibc の gai_strerror() はスレッド セーフであり、静的な定数文字列を返す。
         * see: https://man7.org/linux/man-pages/man3/gai_strerror.3.html
         */
        result = cplat_strcpy(buf, buf_size, gai_strerror((int)error->code));
#elif defined(PLATFORM_WINDOWS)
        /*
         * Windows の EAI_* は Winsock エラー コードと同一値のため FormatMessage で
         * 文字列化できる。gai_strerrorA() は静的バッファーを使用しスレッド セーフでない。
         * see: https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-gai_strerrora
         */
        result = cplat_internal_win32_error_message(buf, buf_size, error->code);
#else
        buf[0] = '\0';
        result = CPLAT_ERR_INVALID_ARGUMENT;
#endif /* PLATFORM_ */
    }
    else if ((error->domain == CPLAT_ERROR_DOMAIN_WINDOWS) || (error->domain == CPLAT_ERROR_DOMAIN_WINSOCK))
    {
        /*
         * Winsock エラーも Win32 と同じく FormatMessage が文字列化できる。
         * see: https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
         */
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

#if defined(PLATFORM_LINUX)
/* GNU 版の strerror_r (char * を返す) の結果を、buf へ収める。message が buf と異なる場合は複製する。 */
static int errno_message_from_gnu(const char *message, char *buf, const size_t buf_size)
{
    if (message == NULL)
    {
        buf[0] = '\0';
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
}

/* XSI 版の strerror_r (int を返す) の結果を、結果コードへ変換する。ERANGE は切り詰めを表すため成功として扱う。 */
static int errno_message_from_xsi(const int xsi_result, char *buf, const size_t buf_size)
{
    (void)buf_size; /* XSI 版は buf への書き込みを strerror_r 側で完了しているため */

    if (xsi_result != 0 && xsi_result != ERANGE)
    {
        buf[0] = '\0';
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}
#endif /* PLATFORM_LINUX */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_errno_message(char *buf, const size_t buf_size, const int errno_value)
{
    if (buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 失敗しても呼び出し元が空文字列を読めるよう、先に終端する。 */
    buf[0] = '\0';

#if defined(PLATFORM_LINUX)
    /*
     * strerror_r には 2 系統がある。どちらが宣言されるかは、機能検査マクロと、テストの mock (int を返す) の有無で
     * 決まるため、戻り値の型で選ぶ。_Generic の制御式は評価されず、呼び出しは引数の 1 回だけ評価される。
     * see: https://man7.org/linux/man-pages/man3/strerror_r.3.html
     */
    return _Generic(strerror_r(errno_value, buf, buf_size),
                    char *: errno_message_from_gnu,
                    default: errno_message_from_xsi)(strerror_r(errno_value, buf, buf_size), buf, buf_size);
#elif defined(PLATFORM_WINDOWS)
    {
        /*
         * strerror_s はバッファーが不足する場合も切り詰めて NUL 終端する。
         * see: https://learn.microsoft.com/cpp/c-runtime-library/reference/strerror-s-strerror-s-wcserror-s-wcserror-s
         */
        int crt_ret = strerror_s(buf, buf_size, errno_value);
        buf[buf_size - 1U] = '\0';

        /* 切り詰めても利用可能なメッセージが得られた場合は、公開契約に従い保持する。 */
        if ((crt_ret != 0) && (buf[0] == '\0') && (buf_size > 1U))
        {
            return CPLAT_ERR_UNKNOWN;
        }

        return CPLAT_OK;
    }
#else
    (void)errno_value;
    return CPLAT_ERR_UNSUPPORTED;
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
    int rc;

    if (buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 失敗しても呼び出し元が空文字列を読めるよう、先に終端する。 */
    buf[0] = '\0';

    /*
     * FORMAT_MESSAGE_ALLOCATE_BUFFER のとき、lpBuffer には確保先ポインターのアドレスを渡す。
     * see: https://learn.microsoft.com/windows/win32/api/winbase/nf-winbase-formatmessagew
     */
    wlen =
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, (DWORD)error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&wmessage, 0, NULL);
    if (wlen == 0 || wmessage == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /* FormatMessageW のメッセージは末尾に改行を含むため取り除く。 */
    while (wlen > 0 && (wmessage[wlen - 1] == L'\r' || wmessage[wlen - 1] == L'\n'))
    {
        wmessage[wlen - 1] = L'\0';
        wlen--;
    }

    rc = cplat_wstr_to_utf8_alloc(&utf8, wmessage);
    /*
     * ALLOCATE_BUFFER で確保したワイド文字列は UTF-8 へ複製済みで、解放の失敗で結果が変わらないため、
     * 戻り値は明示的に破棄する。
     */
    (void)LocalFree(wmessage);

    if (rc != CPLAT_OK)
    {
        return rc;
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
