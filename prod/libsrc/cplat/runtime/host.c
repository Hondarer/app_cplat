/**
 *******************************************************************************
 *  @file           host.c
 *  @brief          ホスト識別情報を取得する API を実装します。
 *
 *  現在のホストの DNS ホスト名を UTF-8 で取得します。
 *
 *******************************************************************************
 */

#include <cplat/runtime/host.h>

#include <cplat/base/platform.h>
#include <cplat/base/result_internal.h>
#include <cplat/crt/string.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>

#if defined(PLATFORM_LINUX)
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/wchar_conv.h>
    #include <windns.h>

static_assert(CPLAT_HOST_NAME_MAX >= DNS_MAX_NAME_BUFFER_LENGTH, "cplat: host name limit");
#endif /* PLATFORM_ */

static_assert(CPLAT_HOST_NAME_MAX <= (unsigned int)INT_MAX, "cplat: host name limit must fit in int");

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_host_get_name(char *name_out, const size_t name_size)
{
    if (name_out == NULL || name_size == 0 || name_size > (size_t)INT_MAX)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_LINUX)
    {
        char tmp[CPLAT_HOST_NAME_MAX];

        /*
         * gethostname() はバッファー不足時の振る舞いが実装で分かれる。
         * POSIX.1 は切り詰め時の NUL 終端を未規定とし、glibc 実装は ENAMETOOLONG で失敗する場合がある。
         * 内部バッファーへ取得して末尾を NUL 終端してから呼び出し側へ写す。
         * see: https://man7.org/linux/man-pages/man2/gethostname.2.html
         */
        if (gethostname(tmp, sizeof(tmp)) != 0)
        {
            name_out[0] = '\0';
            return cplat_internal_result_from_errno(errno);
        }

        tmp[sizeof(tmp) - 1] = '\0';
        return cplat_strcpy(name_out, name_size, tmp);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wname[CPLAT_HOST_NAME_MAX];

        /* CPLAT_HOST_NAME_MAX は 256 であり、DWORD の上限より小さい。 */
        DWORD wname_count = (DWORD)(sizeof(wname) / sizeof(wname[0]));

        /*
         * Winsock の gethostname() は WSAStartup() が必要であり、名前空間プロバイダーが応答しない場合は
         * NetBIOS 名へ代替される。DNS ホスト名を直接 UTF-16 で取得して UTF-8 へ変換する。
         * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-gethostname
         * see: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getcomputernameexw
         */
        if (GetComputerNameExW(ComputerNameDnsHostname, wname, &wname_count) == 0)
        {
            const DWORD error_code = GetLastError();

            name_out[0] = '\0';

            /*
             * GetComputerNameExW() はバッファー不足を ERROR_MORE_DATA で返す。
             * cplat_internal_result_from_windows_error() は ERROR_INSUFFICIENT_BUFFER だけを
             * バッファー不足にするため、ここで結果コードへ変換する。
             * see: https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getcomputernameexw
             */
            if (error_code == ERROR_MORE_DATA)
            {
                return CPLAT_ERR_BUFFER_TOO_SMALL;
            }

            return cplat_internal_result_from_windows_error(error_code);
        }

        return cplat_wstr_to_utf8(name_out, name_size, NULL, wname);
    }
#else
    name_out[0] = '\0';

    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}
