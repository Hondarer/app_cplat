/**
 *******************************************************************************
 *  @file           stdio_temp.c
 *  @brief          一時ファイルを生成する API を実装します。
 *
 *  競合しない一時ファイルを生成し、ファイル ストリームと絶対パスを返します。
 *
 *******************************************************************************
 */

#include <cplat/crt/stdio.h>
#include <cplat/crt/path.h>

#include <cplat/crt/wchar_conv.h>
#include <cplat/win32/wchar_conv_internal.h>
#include <cplat/crt/stdlib.h>
#include <cplat/base/error_internal.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <fcntl.h>
    #include <stdlib.h>
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include "crt.h"
    #include <cplat/crt/temp_file_internal.h>
    #include <share.h>
    #include <stdlib.h>
    #include <wchar.h>
#endif /* PLATFORM_ */

/*
 * prefix の有効文字数上限。一時ファイルの名前は、Linux では prefix と mkostemp() の 6 文字、
 * Windows では prefix と乱数の 16 進表記の 16 文字と ".tmp" であり、両方で prefix の先頭 3 文字までを採用する。
 */
#define CPLAT_TEMP_PREFIX_MAX 3u
#if defined(PLATFORM_WINDOWS)
static_assert(CPLAT_TEMP_PREFIX_MAX == CPLAT_INTERNAL_TEMP_FILE_PREFIX_MAX,
              "cplat: CPLAT_TEMP_PREFIX_MAX must match the internal temp file prefix limit");
#endif /* PLATFORM_WINDOWS */

/* prefix 省略時に用いる既定の接頭辞 */
static const char s_default_temp_prefix[] = "cu_";
static_assert((sizeof(s_default_temp_prefix) - 1U) <= CPLAT_TEMP_PREFIX_MAX,
              "cplat: default temp prefix exceeds CPLAT_TEMP_PREFIX_MAX");

/* Doxygen コメントは、ヘッダーに記載 */

FILE *cplat_fopen_temp(const char *prefix, const char *modes, char *path_out, const size_t path_size,
                       cplat_error *detail_out)
{
    if (modes == NULL || path_out == NULL || path_size == 0u)
    {
        cplat_internal_error_record_errno(detail_out, EINVAL);
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
            pfx = s_default_temp_prefix;
        }

        /*
         * TMPDIR が本バッファーに収まらない場合、切り詰めた値では誤ったディレクトリに
         * 一時ファイルを作成するため、失敗として扱う。
         */
        if (cplat_getenv("TMPDIR", tmpdir_buf, sizeof(tmpdir_buf), NULL, NULL) == CPLAT_ERR_BUFFER_TOO_SMALL)
        {
            cplat_internal_error_record_errno(detail_out, ENAMETOOLONG);
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
            cplat_internal_error_record_errno(detail_out, ENAMETOOLONG);
            return NULL;
        }

        errno = 0;
        fd = mkostemp(path_out, O_CLOEXEC);
        if (fd == -1)
        {
            const int errno_value = errno;

            cplat_internal_error_record_errno(detail_out, errno_value);
            return NULL;
        }

        fp = fdopen(fd, modes);
        if (fp == NULL)
        {
            int saved = errno;
            /* fdopen の失敗を返す経路の後始末であり、失敗しても返す結果が変わらないため、戻り値は明示的に破棄する。 */
            (void)close(fd);
            (void)unlink(path_out);
            cplat_internal_error_record_errno(detail_out, saved);
            return NULL;
        }

        cplat_internal_error_record_success(detail_out);
        return fp;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t *wfile = NULL;
        wchar_t wprefix[CPLAT_TEMP_PREFIX_MAX + 1u];
        wchar_t wmodes[CRT_WMODES_CAPACITY];
        FILE *fp = NULL;
        errno_t err;
        size_t converted;
        int rc;

        {
            const char *pfx;

            if (prefix != NULL && prefix[0] != '\0')
            {
                pfx = prefix;
            }
            else
            {
                pfx = s_default_temp_prefix;
            }

            err = mbstowcs_s(&converted, wprefix, sizeof(wprefix) / sizeof(wprefix[0]), pfx, _TRUNCATE);

            /* STRUNCATE は 4 文字以上の prefix が CPLAT_TEMP_PREFIX_MAX に切り詰められた場合であり、正常として扱う。 */
            if (err != 0 && err != STRUNCATE)
            {
                cplat_internal_error_record_errno(detail_out, EINVAL);
                return NULL;
            }
        }

        /*
         * 一時ディレクトリの取得 (GetTempPath2W / GetTempPathW の実行時選択) と、一意な名前の空ファイルの作成を
         * 内部の共通関数へ集約する。一時ディレクトリが長い場合は、拡張パスの接頭辞 (\\?\) 付きのパスが返る。
         */
        rc = cplat_internal_create_temp_file_w(wprefix, &wfile, detail_out);
        if (rc != CPLAT_OK)
        {
            return NULL;
        }

        /*
         * 作成した一時ファイルの削除は、失敗を返す経路の後始末であり、削除できなくても返す結果が変わらないため、
         * この分岐の DeleteFileW の戻り値は明示的に破棄する。
         */
        rc = cplat_wpath_to_utf8(path_out, path_size, NULL, wfile);
        if (rc != CPLAT_OK)
        {
            (void)DeleteFileW(wfile);
            cplat_free(wfile);
            cplat_internal_error_record_errno(detail_out, cplat_internal_errno_from_conv_result(rc, 1));
            return NULL;
        }

        err = mbstowcs_s(&converted, wmodes, sizeof(wmodes) / sizeof(wmodes[0]), modes, _TRUNCATE);
        if (err != 0)
        {
            (void)DeleteFileW(wfile);
            cplat_free(wfile);
            cplat_internal_error_record_errno(detail_out, EINVAL);
            return NULL;
        }

        /*
         * Linux 側 (fdopen() 経由) と挙動を揃えるため _wfsopen() と _SH_DENYNO を採用する。
         * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fsopen-wfsopen
         */
        errno = 0;
        fp = _wfsopen(wfile, wmodes, _SH_DENYNO);
        if (fp == NULL)
        {
            int saved = errno;
            (void)DeleteFileW(wfile);
            cplat_free(wfile);
            cplat_internal_error_record_errno(detail_out, saved);
            return NULL;
        }

        cplat_free(wfile);
        cplat_internal_error_record_success(detail_out);
        return fp;
    }
#endif /* PLATFORM_ */
}
