/**
 *******************************************************************************
 *  @file           path.c
 *  @brief          UTF-8 パスを操作する API を実装します。
 *
 *  絶対パス化、区切り文字の正規化、パス比較、連結、一時ディレクトリ取得を提供します。
 *
 *******************************************************************************
 */

#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/wchar_conv.h>
#include <cplat/base/error_internal.h>
#include <cplat/base/result.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#if defined(PLATFORM_LINUX)
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
#endif

static int copy_path_text(char *path_out, const size_t path_size, cplat_error *detail_out, const char *text)
{
    size_t len;

    len = strlen(text);
    if (len + 1u > path_size)
    {
        path_out[0] = '\0';
        return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
    }

    memcpy(path_out, text, len + 1u);
    return cplat_internal_error_report_success(detail_out);
}

static int compare_normalized_paths(const char *lhs, const char *rhs)
{
#if defined(PLATFORM_WINDOWS)
    while (*lhs != '\0' && *rhs != '\0')
    {
        if (tolower((unsigned char)*lhs) != tolower((unsigned char)*rhs))
        {
            return 0;
        }
        lhs++;
        rhs++;
    }
    if (*lhs == '\0' && *rhs == '\0')
    {
        return 1;
    }
    else
    {
        return 0;
    }
#else
    if (strcmp(lhs, rhs) == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
#endif
}

#if defined(PLATFORM_LINUX)
static int normalize_absolute_posix_path(char *path)
{
    size_t read_idx;
    size_t write_idx;
    size_t *restore_points;
    size_t restore_count = 0u;

    restore_points = (size_t *)cplat_calloc(PLATFORM_PATH_MAX, sizeof(*restore_points));
    if (restore_points == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    write_idx = 1u;
    read_idx = 1u;

    while (path[read_idx] != '\0')
    {
        size_t seg_start;
        size_t seg_len;

        while (path[read_idx] == PLATFORM_PATH_SEP_CHR)
        {
            read_idx++;
        }

        seg_start = read_idx;
        while (path[read_idx] != '\0' && path[read_idx] != PLATFORM_PATH_SEP_CHR)
        {
            read_idx++;
        }
        seg_len = read_idx - seg_start;

        if (seg_len == 0u)
        {
            break;
        }

        if (seg_len == 1u && path[seg_start] == '.')
        {
            continue;
        }

        if (seg_len == 2u && path[seg_start] == '.' && path[seg_start + 1u] == '.')
        {
            if (restore_count > 0u)
            {
                write_idx = restore_points[--restore_count];
            }
            continue;
        }

        restore_points[restore_count++] = write_idx;
        if (write_idx > 1u)
        {
            path[write_idx++] = PLATFORM_PATH_SEP_CHR;
        }
        memmove(path + write_idx, path + seg_start, seg_len);
        write_idx += seg_len;
    }

    path[write_idx] = '\0';
    cplat_free(restore_points);
    return CPLAT_OK;
}

static int build_absolute_posix_path(char *path_out, const size_t path_size, cplat_error *detail_out, const char *path)
{
    if (path[0] == PLATFORM_PATH_SEP_CHR)
    {
        return copy_path_text(path_out, path_size, detail_out, path);
    }

    {
        char cwd[PLATFORM_PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            const int errno_value = errno;

            path_out[0] = '\0';
            return cplat_internal_error_report_errno(detail_out, errno_value);
        }

        if (cplat_snprintf(path_out, path_size, "%s/%s", cwd, path) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }
    }

    return cplat_internal_error_report_success(detail_out);
}
#endif /* PLATFORM_LINUX */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vpath_concat_n(char *path_out, const size_t path_size, cplat_error *detail_out, const size_t part_count,
                         va_list args)
{
    size_t required_size = 1u;
    size_t offset = 0u;
    size_t idx;
    va_list args_copy;

    if (path_out == NULL || path_size == 0u || part_count == 0u)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    path_out[0] = '\0';

    va_copy(args_copy, args);
    for (idx = 0u; idx < part_count; ++idx)
    {
        const char *part = va_arg(args_copy, const char *);
        size_t part_len;
        if (part == NULL)
        {
            va_end(args_copy);
            return cplat_internal_error_report_errno(detail_out, EINVAL);
        }

        part_len = strlen(part);
        if (part_len > path_size - required_size)
        {
            va_end(args_copy);
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }
        required_size += part_len;
    }
    va_end(args_copy);

    for (idx = 0u; idx < part_count; ++idx)
    {
        const char *part = va_arg(args, const char *);
        size_t part_len = strlen(part);
        memcpy(path_out + offset, part, part_len);
        offset += part_len;
    }
    path_out[offset] = '\0';

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

char *cplat_path_normalize_sep(char *path)
{
    char *p;
    for (p = path; *p != '\0'; ++p)
    {
        if (*p == '\\')
        {
            *p = PLATFORM_PATH_SEP_CHR;
        }
    }
    return path;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_get_full(char *path_out, const size_t path_size, cplat_error *detail_out, const char *path)
{
    if (path_out == NULL || path_size == 0u || path == NULL || path[0] == '\0')
    {
        if (path_out != NULL && path_size > 0u)
        {
            path_out[0] = '\0';
        }
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        char candidate[PLATFORM_PATH_MAX];
        char resolved[PLATFORM_PATH_MAX];

        int build_result = build_absolute_posix_path(candidate, sizeof(candidate), detail_out, path);
        if (build_result != CPLAT_OK)
        {
            path_out[0] = '\0';
            return build_result;
        }

        cplat_path_normalize_sep(candidate);
        {
            int normalize_result = normalize_absolute_posix_path(candidate);
            if (normalize_result != CPLAT_OK)
            {
                path_out[0] = '\0';
                return cplat_internal_error_report_errno(detail_out, ENOMEM);
            }
        }

        if (realpath(candidate, resolved) != NULL)
        {
            return copy_path_text(path_out, path_size, detail_out, resolved);
        }

        return copy_path_text(path_out, path_size, detail_out, candidate);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        char normalized_input[PLATFORM_PATH_MAX];
        wchar_t wpath[PLATFORM_PATH_MAX];
        wchar_t wfull[PLATFORM_PATH_MAX];
        DWORD needed;

        int copy_result = copy_path_text(normalized_input, sizeof(normalized_input), detail_out, path);
        if (copy_result != CPLAT_OK)
        {
            path_out[0] = '\0';
            return copy_result;
        }
        cplat_path_normalize_sep(normalized_input);

        if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), normalized_input) < 0)
        {
            path_out[0] = '\0';
            return cplat_internal_error_report_errno(detail_out, EINVAL);
        }

        needed = GetFullPathNameW(wpath, (DWORD)(sizeof(wfull) / sizeof(wfull[0])), wfull, NULL);
        if (needed == 0u)
        {
            const DWORD error_code = GetLastError();

            path_out[0] = '\0';
            return cplat_internal_error_report_windows_error(detail_out, error_code);
        }
        if (needed >= (DWORD)(sizeof(wfull) / sizeof(wfull[0])))
        {
            path_out[0] = '\0';
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }

        if (cplat_wpath_to_utf8(path_out, path_size, wfull) < 0)
        {
            path_out[0] = '\0';
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }
    }
    return cplat_internal_error_report_success(detail_out);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_equal(const char *lhs, const char *rhs, int *equal_out, cplat_error *detail_out)
{
    char lhs_full[PLATFORM_PATH_MAX];
    char rhs_full[PLATFORM_PATH_MAX];
    int ret;

    if (equal_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    ret = cplat_path_get_full(lhs_full, sizeof(lhs_full), detail_out, lhs);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    ret = cplat_path_get_full(rhs_full, sizeof(rhs_full), detail_out, rhs);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    *equal_out = compare_normalized_paths(lhs_full, rhs_full);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_get_temp_dir(char *path_out, const size_t path_size, cplat_error *detail_out)
{
    if (path_out == NULL || path_size == 0u)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        char tmpdir_buf[PLATFORM_PATH_MAX];
        const char *tmpdir;
        size_t len;

        /* TMPDIR が設定されていても本バッファーに収まらない場合は、切り詰めた値を
           一時ディレクトリとして採用すると誤ったパスを返すため、失敗として扱う */
        if (cplat_getenv("TMPDIR", tmpdir_buf, sizeof(tmpdir_buf), NULL, NULL) == CPLAT_ERR_BUFFER_TOO_SMALL)
        {
            path_out[0] = '\0';
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }
        if (tmpdir_buf[0] == '\0')
        {
            tmpdir = "/tmp";
        }
        else
        {
            tmpdir = tmpdir_buf;
        }

        len = strlen(tmpdir);
        while (len > 1u && tmpdir[len - 1u] == PLATFORM_PATH_SEP_CHR)
        {
            --len;
        }

        if (cplat_snprintf(path_out, path_size, "%.*s", (int)len, tmpdir) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }
        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wdir[PLATFORM_PATH_MAX];
        DWORD dwret;
        size_t len;

        /* 取得結果は直後に cplat_wpath_to_utf8 で変換するため、
           GetTempPathU を新設せず W 版と既存の変換関数を組み合わせる */
        dwret = GetTempPathW((DWORD)(sizeof(wdir) / sizeof(wdir[0])), wdir);
        if (dwret == 0u)
        {
            const DWORD error_code = GetLastError();

            return cplat_internal_error_report_windows_error(detail_out, error_code);
        }
        if (dwret >= (DWORD)(sizeof(wdir) / sizeof(wdir[0])))
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }

        if (cplat_wpath_to_utf8(path_out, path_size, wdir) < 0)
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }

        /* GetTempPathW は末尾 '\' を付けて返す。変換後は '/' になるので除去する */
        len = strlen(path_out);
        while (len > 1u && path_out[len - 1u] == PLATFORM_PATH_SEP_CHR)
        {
            path_out[--len] = '\0';
        }
        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_concat_n(char *path_out, const size_t path_size, cplat_error *detail_out, const size_t part_count, ...)
{
    int result;
    va_list args;

    va_start(args, part_count);
    result = cplat_vpath_concat_n(path_out, path_size, detail_out, part_count, args);
    va_end(args);

    return result;
}
