/**
 *******************************************************************************
 *  @file           result.c
 *  @brief          OS エラー値を共通結果コードへ変換する内部 API を実装します。
 *
 *  errno および Windows の GetLastError() の値のうち、汎用的に対応が
 *  決まるものだけを共通結果コードへ変換します。\n
 *  モジュールに固有の解釈は、本 API を呼び出す前段で各モジュールが判定します。
 *
 *******************************************************************************
 */

#include <cplat/base/result_internal.h>
#include <cplat/base/result.h>

#include <errno.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_result_from_errno(const int errno_value)
{
    int result;

    if (errno_value == EINVAL)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if ((errno_value == ENOENT) || (errno_value == ESRCH))
    {
        result = CPLAT_ERR_NOT_FOUND;
    }
    else if ((errno_value == EACCES) || (errno_value == EPERM))
    {
        result = CPLAT_ERR_PERMISSION_DENIED;
    }
    else if (errno_value == ETIMEDOUT)
    {
        result = CPLAT_ERR_TIMEOUT;
    }
    else if ((errno_value == EBUSY) || (errno_value == EAGAIN))
    {
        result = CPLAT_ERR_BUSY;
    }
    else if (errno_value == ENOMEM)
    {
        result = CPLAT_ERR_OUT_OF_MEMORY;
    }
    else if ((errno_value == ENAMETOOLONG) || (errno_value == ERANGE))
    {
        result = CPLAT_ERR_BUFFER_TOO_SMALL;
    }
#if defined(ENOTSUP)
    else if (errno_value == ENOTSUP)
    {
        result = CPLAT_ERR_UNSUPPORTED;
    }
#endif
#if defined(EOPNOTSUPP) && (!defined(ENOTSUP) || (EOPNOTSUPP != ENOTSUP))
    else if (errno_value == EOPNOTSUPP)
    {
        result = CPLAT_ERR_UNSUPPORTED;
    }
#endif
    else if (errno_value == ENOSYS)
    {
        result = CPLAT_ERR_UNSUPPORTED;
    }
    else
    {
        result = CPLAT_ERR_UNKNOWN;
    }

    return result;
}

#if defined(PLATFORM_WINDOWS)
/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_result_from_winsock_error(const unsigned long error_code)
{
    int result;

    if ((error_code == (unsigned long)WSAEINVAL) || (error_code == (unsigned long)WSAEFAULT))
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if (error_code == (unsigned long)WSAEACCES)
    {
        result = CPLAT_ERR_PERMISSION_DENIED;
    }
    else if (error_code == (unsigned long)WSAETIMEDOUT)
    {
        result = CPLAT_ERR_TIMEOUT;
    }
    else if (error_code == (unsigned long)WSAEWOULDBLOCK)
    {
        result = CPLAT_ERR_BUSY;
    }
    else if (error_code == (unsigned long)WSAENOBUFS)
    {
        result = CPLAT_ERR_OUT_OF_MEMORY;
    }
    else if (error_code == (unsigned long)WSAEMSGSIZE)
    {
        result = CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    else if ((error_code == (unsigned long)WSAEOPNOTSUPP) || (error_code == (unsigned long)WSAEAFNOSUPPORT) ||
             (error_code == (unsigned long)WSAEPROTONOSUPPORT))
    {
        result = CPLAT_ERR_UNSUPPORTED;
    }
    else if (error_code == (unsigned long)WSAHOST_NOT_FOUND)
    {
        result = CPLAT_ERR_NOT_FOUND;
    }
    else
    {
        result = CPLAT_ERR_UNKNOWN;
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_result_from_windows_error(const unsigned long error_code)
{
    int result;

    if (error_code == ERROR_INVALID_PARAMETER)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if ((error_code == ERROR_FILE_NOT_FOUND) || (error_code == ERROR_PATH_NOT_FOUND))
    {
        result = CPLAT_ERR_NOT_FOUND;
    }
    else if ((error_code == ERROR_ACCESS_DENIED) || (error_code == ERROR_PRIVILEGE_NOT_HELD))
    {
        result = CPLAT_ERR_PERMISSION_DENIED;
    }
    else if ((error_code == WAIT_TIMEOUT) || (error_code == ERROR_TIMEOUT))
    {
        result = CPLAT_ERR_TIMEOUT;
    }
    else if (error_code == ERROR_BUSY)
    {
        result = CPLAT_ERR_BUSY;
    }
    else if ((error_code == ERROR_NOT_ENOUGH_MEMORY) || (error_code == ERROR_OUTOFMEMORY))
    {
        result = CPLAT_ERR_OUT_OF_MEMORY;
    }
    else if (error_code == ERROR_INSUFFICIENT_BUFFER)
    {
        result = CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    else if ((error_code == ERROR_NOT_SUPPORTED) || (error_code == ERROR_CALL_NOT_IMPLEMENTED))
    {
        result = CPLAT_ERR_UNSUPPORTED;
    }
    else
    {
        result = CPLAT_ERR_UNKNOWN;
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_errno_from_windows_error(const unsigned long error_code)
{
    int errno_value;

    if (error_code == ERROR_INVALID_PARAMETER)
    {
        errno_value = EINVAL;
    }
    else if ((error_code == ERROR_ACCESS_DENIED) || (error_code == ERROR_PRIVILEGE_NOT_HELD))
    {
        errno_value = EACCES;
    }
    else if ((error_code == ERROR_FILE_NOT_FOUND) || (error_code == ERROR_PATH_NOT_FOUND))
    {
        errno_value = ENOENT;
    }
    else if (error_code == ERROR_FILE_EXISTS || error_code == ERROR_ALREADY_EXISTS)
    {
        errno_value = EEXIST;
    }
    else if ((error_code == ERROR_NOT_ENOUGH_MEMORY) || (error_code == ERROR_OUTOFMEMORY))
    {
        errno_value = ENOMEM;
    }
    else if ((error_code == ERROR_INSUFFICIENT_BUFFER) || (error_code == ERROR_BUFFER_OVERFLOW))
    {
        errno_value = ENAMETOOLONG;
    }
    else if (error_code == ERROR_BUSY)
    {
        errno_value = EBUSY;
    }
    else if ((error_code == WAIT_TIMEOUT) || (error_code == ERROR_TIMEOUT))
    {
        errno_value = ETIMEDOUT;
    }
    else
    {
        errno_value = EIO;
    }

    return errno_value;
}
#endif
