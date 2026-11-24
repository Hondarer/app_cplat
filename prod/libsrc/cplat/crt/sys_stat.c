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
#include <cplat/win32/wchar_conv_internal.h>
#include "crt.h"

#include <stddef.h>
#include <errno.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <sys/stat.h>
    #include <unistd.h> /* rmdir */

    /* ディレクトリ作成時の既定パーミッション。仕様は prod/include/cplat/crt/sys/stat.h (cplat_mkdir) および
     * docs/api-cheatsheet.md を参照。 */
    #define CRT_MKDIR_MODE (0755)
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/clock/filetime_conv.h>
    #include <direct.h>

    /* Windows ドライブ文字ルート先頭 ("C:\\") の長さ [byte] */
    #define DRIVE_ROOT_PREFIX_LEN (3U)

    /* Windows UNC パス先頭 ("\\\\") の長さ [byte] */
    #define UNC_PREFIX_LEN        (2U)
#endif /* PLATFORM_ */

/**
 *  @brief          結果コードが、パスの変換の失敗によるものかを判定します。
 *  @param[in]      result 判定する結果コード。
 *  @return         引数の誤り、メモリ不足、容量の不足、不正な文字コードのいずれかであれば 0 以外、
 *                  それ以外であれば 0 を返します。
 */
static int is_path_conversion_error(const int result)
{
    return result == CPLAT_ERR_INVALID_ARGUMENT || result == CPLAT_ERR_OUT_OF_MEMORY ||
           result == CPLAT_ERR_BUFFER_TOO_SMALL || result == CPLAT_ERR_INVALID_ENCODING;
}

/*
 * cplat_stat() が返したファイル情報がディレクトリを表すかを判定する。
 */
static int is_directory_stat(const cplat_file_stat_t *st)
{
#if defined(PLATFORM_LINUX)
    return (S_ISDIR(st->st_mode) != 0) ? 1 : 0;
#elif defined(PLATFORM_WINDOWS)
    return ((st->st_mode & _S_IFMT) == _S_IFDIR) ? 1 : 0;
#endif /* PLATFORM_ */
}

/**
 *  @brief          指定されたディレクトリが存在することを確認し、なければ生成します。
 *  @param[in]      dir 対象ディレクトリのパス (UTF-8)。
 *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
 *                  エラー詳細を設定せず、返却しません。
 *  @return         ディレクトリが存在または作成できた場合は @ref CPLAT_OK 、
 *                  ディレクトリ以外が存在する場合は @ref CPLAT_ERR_UNKNOWN (詳細 @c ENOTDIR )、
 *                  それ以外の失敗時は @c cplat_stat() または @c cplat_mkdir() の結果コードを返します。
 *
 *  まず cplat_stat() で存在を確認し、ディレクトリであれば成功、ディレクトリ以外であれば
 *  ENOTDIR (要因 @ref CPLAT_CAUSE_NOT_A_DIRECTORY ) で失敗します。\n
 *  cplat_stat() が失敗した場合は cplat_mkdir() を試行します。
 *  cplat_mkdir() が競合生成等で失敗した場合は他プロセスによる生成を確認するため局所変数へ
 *  cplat_stat() を呼び直します。再確認が失敗しても作成の失敗理由 (権限不足など) が詳細エラーから
 *  失われないよう、再確認で詳細エラー (detail_out および TLS) を上書きせず、
 *  cplat_mkdir() の結果コードと詳細エラーをそのまま返します。
 */
static int ensure_one_dir(const char *dir, cplat_error *detail_out)
{
    cplat_file_stat_t st;
    cplat_error recheck_detail;
    int mkdir_result;
    int recheck_result;
    int result;

    /* 1. すでに存在する場合はディレクトリであることを確かめる。 */
    result = cplat_stat(&st, detail_out, dir);
    if (result == CPLAT_OK)
    {
        if (is_directory_stat(&st))
        {
            return CPLAT_OK;
        }

        return cplat_internal_error_report_errno(detail_out, ENOTDIR);
    }
    if (is_path_conversion_error(result))
    {
        return result;
    }

    /* 2. 存在しない (または stat 失敗) ため生成する。 */
    mkdir_result = cplat_mkdir(dir, detail_out);
    if (mkdir_result == CPLAT_OK)
    {
        return CPLAT_OK;
    }
    if (is_path_conversion_error(mkdir_result))
    {
        return mkdir_result;
    }

    /*
     * 3. mkdir 失敗: 別プロセスによる競合生成の可能性があるため再確認する。
     * 再確認の失敗で作成の失敗理由 (権限不足など) を上書きしないよう、詳細エラーは局所変数へ受ける。
     */
    recheck_result = cplat_stat(&st, &recheck_detail, dir);
    if (recheck_result == CPLAT_OK)
    {
        if (is_directory_stat(&st))
        {
            return cplat_internal_error_report_success(detail_out);
        }

        return cplat_internal_error_report_errno(detail_out, ENOTDIR);
    }

    return mkdir_result;
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
 * ファイル属性から直接取得した UTC 時刻で stat 構造体の時刻フィールドを上書きする。
 * UCRT の _wstat64() は FILETIME (UTC) を SystemTimeToTzSpecificLocalTime() で現地時刻へ
 * 変換してから loctotime() で time_t へ戻す。OS のタイム ゾーンと CRT の TZ が食い違うと、
 * Linux の stat() および cplat_file_*_modified_timestamp が返す Unix epoch UTC と
 * 秒部に差異が生じる。
 * see: https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times
 * see: https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltime
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

/*
 * パスのルート接頭辞の長さを算出する。
 * ドライブ レター接頭辞、UNC サーバー・共有名接頭辞、またはルート セパレーターの長さ [byte] を返す。
 * ルート接頭辞を持たない相対パスの場合は 0 を返す。
 * 区切りの正規化後に "//?/" または "//./" で始まるパスは解析の対象外で、'?' または '.' をサーバー名として扱う。
 * see: https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
 */
static size_t path_root_prefix_len(const char *path)
{
#if defined(PLATFORM_WINDOWS)
    if (is_ascii_alpha(path[0]) && path[1] == ':' && path[2] == PLATFORM_PATH_SEP_CHR)
    {
        return DRIVE_ROOT_PREFIX_LEN;
    }

    if (path[0] == PLATFORM_PATH_SEP_CHR && path[1] == PLATFORM_PATH_SEP_CHR)
    {
        size_t i = UNC_PREFIX_LEN;

        /* サーバー名部分を走査する。 */
        while (path[i] != '\0' && path[i] != PLATFORM_PATH_SEP_CHR)
        {
            i++;
        }

        if (path[i] == '\0')
        {
            return i;
        }

        /* サーバー名と共有名の間のセパレーターをスキップする。 */
        while (path[i] == PLATFORM_PATH_SEP_CHR)
        {
            i++;
        }

        if (path[i] == '\0')
        {
            return i;
        }

        /* 共有名部分を走査する。 */
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
        int rc;

        rc = cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), NULL, path);
        if (rc != CPLAT_OK)
        {
            return cplat_internal_error_report_errno(detail_out, cplat_internal_errno_from_conv_result(rc, 1));
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
    char buf[PLATFORM_PATH_UTF8_MAX];
    size_t path_len;
    size_t root_len;
    size_t i;
    int result;

    if (path == NULL || path[0] == '\0')
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_WINDOWS)
    {
        int check_result = crt_check_wpath_length(path, detail_out);
        if (check_result != CPLAT_OK)
        {
            return check_result;
        }
    }
#endif /* PLATFORM_WINDOWS */

    path_len = strlen(path);
    if (path_len >= (size_t)PLATFORM_PATH_UTF8_MAX)
    {
        return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
    }

    memcpy(buf, path, path_len + 1);

#if defined(PLATFORM_WINDOWS)
    cplat_path_normalize_sep(buf);
#endif /* PLATFORM_WINDOWS */

    root_len = path_root_prefix_len(buf);

    /* パスを親階層から順に辿り、中間ディレクトリを順次生成する。 */
    for (i = root_len; i < path_len; i++)
    {
        if (buf[i] == PLATFORM_PATH_SEP_CHR)
        {
            if (i > root_len && buf[i - 1u] != PLATFORM_PATH_SEP_CHR)
            {
                /* 中間ディレクトリを一時終端して生成する。 */
                buf[i] = '\0';

                result = ensure_one_dir(buf, detail_out);
                if (result != CPLAT_OK)
                {
                    return result;
                }

                buf[i] = PLATFORM_PATH_SEP_CHR;
            }
        }
    }

    /* 末尾要素 (= パス全体) を生成する。 */
    return ensure_one_dir(buf, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_stat(cplat_file_stat_t *stat_out, cplat_error *detail_out, const char *path)
{
    int result;

    if (stat_out == NULL || path == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    errno = 0;
#if defined(PLATFORM_LINUX)
    result = stat(path, stat_out);
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];
        int rc;

        rc = cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), NULL, path);
        if (rc != CPLAT_OK)
        {
            return cplat_internal_error_report_errno(detail_out, cplat_internal_errno_from_conv_result(rc, 1));
        }

        result = _wstat64(wpath, stat_out);
        if (result == 0)
        {
            overlay_utc_file_times(wpath, stat_out);
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
    result = mkdir(path, CRT_MKDIR_MODE);
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];
        int rc;

        rc = cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), NULL, path);
        if (rc != CPLAT_OK)
        {
            return cplat_internal_error_report_errno(detail_out, cplat_internal_errno_from_conv_result(rc, 1));
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
    /*
     * glibc は S_IFMT と S_IFREG を __USE_MISC の内側に置くため、
     * 厳密 ISO モードでも参照できる S_ISREG() を使用する。
     */
    return S_ISREG(file_stat->st_mode) != 0;
#elif defined(PLATFORM_WINDOWS)
    /* MSVC の <sys/stat.h> に S_ISREG() はないため、種別ビットの比較で判定する。 */
    return (file_stat->st_mode & _S_IFMT) == _S_IFREG;
#endif /* PLATFORM_ */
}
