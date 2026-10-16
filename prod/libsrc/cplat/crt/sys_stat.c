/**
 *******************************************************************************
 *  @file           sys_stat.c
 *  @brief          ファイル情報の取得とディレクトリ生成を行う API を実装します。
 *
 *  UTF-8 パスに対応した stat、mkdir、再帰的ディレクトリ生成を提供します。
 *
 *******************************************************************************
 */

#include <cplat/crt/sys/stat.h>
#include <cplat/crt/path.h>

#include <cplat/base/result.h>
#include <cplat/base/error_internal.h>
#include <cplat/crt/wchar_conv.h>

#include <stddef.h>
#include <errno.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <sys/stat.h>
    #include <unistd.h> /* rmdir */
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/clock/filetime_conv.h>
    #include <direct.h>
#endif /* PLATFORM_ */

/**
 *  @brief          指定されたディレクトリが存在することを確認し、なければ生成します。
 *  @param[in]      dir 対象ディレクトリのパス (UTF-8)。
 *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
 *                  エラー詳細を設定せず、返却しません。
 *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します。
 *
 *  cplat_mkdir が競合生成で @ref CPLAT_ERR_UNKNOWN を返す場合も cplat_stat で再確認して
 *  ディレクトリが存在すれば成功とみなします。
 */
static int ensure_one_dir(const char *dir, cplat_error *detail_out)
{
    cplat_file_stat_t st;

    /* すでに存在する場合は成功 */
    if (cplat_stat(&st, detail_out, dir) == CPLAT_OK)
    {
        return CPLAT_OK;
    }

    /* 存在しないので生成する */
    if (cplat_mkdir(dir, detail_out) == CPLAT_OK)
    {
        return CPLAT_OK;
    }

    /* mkdir 失敗: 競合生成の可能性があるため再確認する */
    if (cplat_stat(&st, detail_out, dir) == CPLAT_OK)
    {
        return CPLAT_OK;
    }

    return CPLAT_ERR_UNKNOWN;
}

#if defined(PLATFORM_WINDOWS)
static int is_ascii_alpha(const char ch)
{
    if (ch >= 'A' && ch <= 'Z')
    {
        return 1;
    }
    if (ch >= 'a' && ch <= 'z')
    {
        return 1;
    }
    return 0;
}

/*
 *  UCRT の _wstat64 は FILETIME (UTC) を SystemTimeToTzSpecificLocalTime で現地時刻へ
 *  変換してから loctotime で time_t へ戻す。OS のタイム ゾーンと CRT の TZ が食い違うと、
 *  Linux の stat および cplat_file_*_modified_timestamp が返す Unix epoch UTC と
 *  秒部に差異が生じる。
 *  see: https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times
 *  see: https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltime
 */
static void overlay_utc_file_times(const wchar_t *wpath, cplat_file_stat_t *buf)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    cplat_timespec timestamp;

    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &attributes))
    {
        return;
    }

    cplat_internal_filetime_to_timespec(&attributes.ftLastAccessTime, &timestamp);
    buf->st_atime = timestamp.tv_sec;
    cplat_internal_filetime_to_timespec(&attributes.ftLastWriteTime, &timestamp);
    buf->st_mtime = timestamp.tv_sec;
    cplat_internal_filetime_to_timespec(&attributes.ftCreationTime, &timestamp);
    buf->st_ctime = timestamp.tv_sec;
}
#endif /* PLATFORM_WINDOWS */

static size_t path_root_prefix_len(const char *path)
{
#if defined(PLATFORM_WINDOWS)
    if (is_ascii_alpha(path[0]) && path[1] == ':' && path[2] == PLATFORM_PATH_SEP_CHR)
    {
        return 3u;
    }

    if (path[0] == PLATFORM_PATH_SEP_CHR && path[1] == PLATFORM_PATH_SEP_CHR)
    {
        size_t i = 2u;

        while (path[i] != '\0' && path[i] != PLATFORM_PATH_SEP_CHR)
        {
            i++;
        }
        if (path[i] == '\0')
        {
            return i;
        }
        while (path[i] == PLATFORM_PATH_SEP_CHR)
        {
            i++;
        }
        if (path[i] == '\0')
        {
            return i;
        }
        while (path[i] != '\0' && path[i] != PLATFORM_PATH_SEP_CHR)
        {
            i++;
        }
        return i;
    }
#endif /* PLATFORM_WINDOWS */

    if (path[0] == PLATFORM_PATH_SEP_CHR)
    {
        return 1u;
    }

    return 0u;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_rmdir(const char *path, cplat_error *detail_out)
{
    int result;

    if (path == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    errno = 0;
#if defined(PLATFORM_LINUX)
    result = rmdir(path);
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];

        if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), path) < 0)
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }

        result = _wrmdir(wpath);
    }
#endif /* PLATFORM_ */

    if (result != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_makedirs(const char *path, cplat_error *detail_out)
{
    char buf[PLATFORM_PATH_MAX];
    size_t path_len;
    size_t root_len;
    size_t i;

    if (path == NULL || path[0] == '\0')
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    path_len = strlen(path);
    if (path_len >= (size_t)PLATFORM_PATH_MAX)
    {
        return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
    }

    /* パスをローカル バッファーに複製する */
    memcpy(buf, path, path_len + 1);

#if defined(PLATFORM_WINDOWS)
    cplat_path_normalize_sep(buf);
#endif /* PLATFORM_WINDOWS */

    root_len = path_root_prefix_len(buf);

    for (i = root_len; i < path_len; i++)
    {
        if (buf[i] == PLATFORM_PATH_SEP_CHR)
        {
            if (i > root_len && buf[i - 1u] != PLATFORM_PATH_SEP_CHR)
            {
                /* 中間ディレクトリを一時終端して生成する */
                buf[i] = '\0';
                if (ensure_one_dir(buf, detail_out) != CPLAT_OK)
                {
                    return CPLAT_ERR_UNKNOWN;
                }
                buf[i] = PLATFORM_PATH_SEP_CHR;
            }
        }
    }

    /* 末尾要素 (= パス全体) を生成する */
    return ensure_one_dir(buf, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_stat(cplat_file_stat_t *buf, cplat_error *detail_out, const char *path)
{
    int result;

    if (buf == NULL || path == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    errno = 0;
#if defined(PLATFORM_LINUX)
    result = stat(path, buf);
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];

        if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), path) < 0)
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }

        result = _wstat64(wpath, buf);
        if (result == 0)
        {
            overlay_utc_file_times(wpath, buf);
        }
    }
#endif /* PLATFORM_ */

    if (result != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_mkdir(const char *path, cplat_error *detail_out)
{
    int result;

    if (path == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    errno = 0;
#if defined(PLATFORM_LINUX)
    result = mkdir(path, 0755);
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];

        if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), path) < 0)
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }

        result = _wmkdir(wpath);
    }
#endif /* PLATFORM_ */

    if (result != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_file_stat_is_regular(const cplat_file_stat_t *file_stat)
{
    if (file_stat == NULL)
    {
        return 0;
    }

#if defined(PLATFORM_LINUX)
    /* glibc は S_IFMT と S_IFREG を __USE_MISC の内側に置くため、
     * 厳密 ISO モードでも参照できる S_ISREG を使用する。 */
    return S_ISREG(file_stat->st_mode) != 0;
#elif defined(PLATFORM_WINDOWS)
    /* MSVC の <sys/stat.h> に S_ISREG はないため、種別ビットの比較で判定する。 */
    return (file_stat->st_mode & _S_IFMT) == _S_IFREG;
#endif /* PLATFORM_ */
}
