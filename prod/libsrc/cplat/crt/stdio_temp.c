/**
 *******************************************************************************
 *  @file           stdio_temp.c
 *  @brief          一時ファイルを生成する API を実装します。
 *
 *  競合しない一時ファイルを生成し、ファイル ストリームと絶対パスを返します。
 *
 *******************************************************************************
 */

#define _GNU_SOURCE

#include <cplat/crt/stdio.h>
#include <cplat/crt/path.h>

#include <cplat/crt/wchar_conv.h>
#include <cplat/crt/stdlib.h>
#include <cplat/base/error_internal.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <fcntl.h>
    #include <stdlib.h>
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <share.h>
    #include <stdlib.h>
    #include <wchar.h>
#endif /* PLATFORM_ */

/* prefix の有効文字数上限 (Windows GetTempFileNameW の制約に準拠) */
#define CPLAT_TEMP_PREFIX_MAX 3u

/* Doxygen コメントは、ヘッダーに記載 */

FILE *cplat_fopen_temp(const char *prefix, const char *modes, char *path_out, const size_t path_size,
                       cplat_error *detail_out)
{
    if (modes == NULL || path_out == NULL || path_size == 0u)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return NULL;
    }

#if defined(PLATFORM_LINUX)
    {
        char tmpdir_buf[PLATFORM_PATH_MAX];
        const char *tmpdir;
        char pfx_buf[CPLAT_TEMP_PREFIX_MAX + 1u];
        const char *pfx;
        int fd;
        FILE *fp;

        if (prefix != NULL)
        {
            if (strlen(prefix) > CPLAT_TEMP_PREFIX_MAX)
            {
                memcpy(pfx_buf, prefix, CPLAT_TEMP_PREFIX_MAX);
                pfx_buf[CPLAT_TEMP_PREFIX_MAX] = '\0';
                pfx = pfx_buf;
            }
            else
            {
                pfx = prefix;
            }
        }
        else
        {
            pfx = "cu_";
        }

        /* TMPDIR が本バッファーに収まらない場合、切り詰めた値では誤ったディレクトリに
           一時ファイルを作成するため、失敗として扱う */
        if (cplat_getenv("TMPDIR", tmpdir_buf, sizeof(tmpdir_buf), NULL, NULL) == CPLAT_ERR_BUFFER_TOO_SMALL)
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return NULL;
        }
        if (tmpdir_buf[0] == '\0')
        {
            tmpdir = "/tmp";
        }
        else
        {
            tmpdir = tmpdir_buf;
        }

        if (cplat_snprintf(path_out, path_size, "%s" PLATFORM_PATH_SEP "%sXXXXXX", tmpdir, pfx) != CPLAT_OK)
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return NULL;
        }

        errno = 0;
        fd = mkostemp(path_out, O_CLOEXEC);
        if (fd == -1)
        {
            const int errno_value = errno;

            (void)cplat_internal_error_report_errno(detail_out, errno_value);
            return NULL;
        }

        fp = fdopen(fd, modes);
        if (fp == NULL)
        {
            int saved = errno;
            close(fd);
            unlink(path_out);
            (void)cplat_internal_error_report_errno(detail_out, saved);
            return NULL;
        }
        (void)cplat_internal_error_report_success(detail_out);
        return fp;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wdir[MAX_PATH];
        wchar_t wfile[MAX_PATH];
        wchar_t wprefix[CPLAT_TEMP_PREFIX_MAX + 1u];
        wchar_t wmodes[64];
        FILE *fp = NULL;
        errno_t err;
        size_t converted;
        DWORD dwret;
        UINT uret;

        /* 以降の GetTempFileNameW / DeleteFileW へワイドのまま渡すため、
           *U ラッパーを経由しない (UTF-8 への往復変換が増えるだけになる) */
        dwret = GetTempPathW((DWORD)(sizeof(wdir) / sizeof(wdir[0])), wdir);
        if (dwret == 0u)
        {
            const DWORD error_code = GetLastError();

            (void)cplat_internal_error_report_windows_error(detail_out, error_code);
            return NULL;
        }
        if (dwret > (DWORD)(sizeof(wdir) / sizeof(wdir[0])))
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return NULL;
        }

        {
            const char *pfx;
            if (prefix != NULL && prefix[0] != '\0')
            {
                pfx = prefix;
            }
            else
            {
                pfx = "cu_";
            }
            err = mbstowcs_s(&converted, wprefix, sizeof(wprefix) / sizeof(wprefix[0]), pfx, _TRUNCATE);
            /* STRUNCATE: 4 文字以上の prefix が CPLAT_TEMP_PREFIX_MAX に切り詰められた場合。正常扱い。 */
            if (err != 0 && err != STRUNCATE)
            {
                (void)cplat_internal_error_report_errno(detail_out, EINVAL);
                return NULL;
            }
        }

        uret = GetTempFileNameW(wdir, wprefix, 0u, wfile);
        if (uret == 0u)
        {
            const DWORD error_code = GetLastError();

            (void)cplat_internal_error_report_windows_error(detail_out, error_code);
            return NULL;
        }

        if (cplat_wpath_to_utf8(path_out, path_size, wfile) < 0)
        {
            DeleteFileW(wfile);
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return NULL;
        }

        err = mbstowcs_s(&converted, wmodes, sizeof(wmodes) / sizeof(wmodes[0]), modes, _TRUNCATE);
        if (err != 0)
        {
            DeleteFileW(wfile);
            (void)cplat_internal_error_report_errno(detail_out, EINVAL);
            return NULL;
        }

        /* Linux 側 (fdopen 経由) と挙動をそろえるため _wfsopen + _SH_DENYNO を採用する。
         * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fsopen-wfsopen */
        errno = 0;
        fp = _wfsopen(wfile, wmodes, _SH_DENYNO);
        if (fp == NULL)
        {
            int saved = errno;
            DeleteFileW(wfile);
            (void)cplat_internal_error_report_errno(detail_out, saved);
            return NULL;
        }
        (void)cplat_internal_error_report_success(detail_out);
        return fp;
    }
#endif /* PLATFORM_ */
}
