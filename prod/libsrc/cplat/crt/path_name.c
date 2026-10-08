/**
 *******************************************************************************
 *  @file           path_name.c
 *  @brief          パス文字列からベース名・親ディレクトリ・拡張子を取り出す API を実装します。
 *
 *  basename / dirname / extension / セパレータ補完付き結合を提供します。\n
 *  プラットフォーム分岐を持たない純粋な文字列処理です。
 *
 *******************************************************************************
 */

#include <cplat/crt/path.h>
#include <cplat/base/error_internal.h>
#include <cplat/base/result.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

static int path_is_sep(const char c)
{
    return c == '/' || c == '\\';
}

static int copy_path_name_text(char *path_out, const size_t path_size, cplat_error *detail_out, const char *text)
{
    size_t len;

    if (path_out == NULL || path_size == 0u || text == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    len = strlen(text);
    if (len + 1u > path_size)
    {
        path_out[0] = '\0';
        return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
    }

    memcpy(path_out, text, len + 1u);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_path_basename(const char *path)
{
    const char *last_sep = NULL;
    const char *p;

    if (path == NULL)
    {
        return NULL;
    }

    for (p = path; *p != '\0'; ++p)
    {
        if (path_is_sep(*p))
        {
            last_sep = p;
        }
    }

    if (last_sep == NULL)
    {
        return path;
    }

    return last_sep + 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_dirname(char *path_out, const size_t path_size, cplat_error *detail_out, const char *path)
{
    const char *end;
    const char *last_sep;
    const char *p;
    size_t len;

    if (path_out == NULL || path_size == 0u || path == NULL || path[0] == '\0')
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    /* 末尾のセパレータ群を除去する (ルートのみの場合は残す) */
    end = path + strlen(path);
    while (end > path + 1 && path_is_sep(*(end - 1)))
    {
        --end;
    }

    last_sep = NULL;
    for (p = path; p < end; ++p)
    {
        if (path_is_sep(*p))
        {
            last_sep = p;
        }
    }

    if (last_sep == NULL)
    {
        return copy_path_name_text(path_out, path_size, detail_out, ".");
    }

    /* 最後のセパレータより前がすべてセパレータの場合 (例: "/name") はルートを返す */
    if (last_sep == path)
    {
        return copy_path_name_text(path_out, path_size, detail_out, PLATFORM_PATH_SEP);
    }

    len = (size_t)(last_sep - path);
    if (len + 1u > path_size)
    {
        path_out[0] = '\0';
        return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
    }

    for (p = path; p < last_sep; ++p)
    {
        if (path_is_sep(*p))
        {
            path_out[p - path] = PLATFORM_PATH_SEP_CHR;
        }
        else
        {
            path_out[p - path] = *p;
        }
    }
    path_out[len] = '\0';

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_path_extension(const char *path)
{
    const char *base;
    const char *dot;
    const char *p;

    if (path == NULL)
    {
        return NULL;
    }

    base = cplat_path_basename(path);
    dot = NULL;
    for (p = base; *p != '\0'; ++p)
    {
        if (*p == '.')
        {
            dot = p;
        }
    }

    if (dot == NULL || dot == base)
    {
        return path + strlen(path);
    }

    return dot;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_strip_extension(char *path_out, const size_t path_size, cplat_error *detail_out, const char *path)
{
    const char *ext;
    size_t len;

    if (path_out == NULL || path_size == 0u || path == NULL || path[0] == '\0')
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    ext = cplat_path_extension(path);
    len = (size_t)(ext - path);

    if (len + 1u > path_size)
    {
        path_out[0] = '\0';
        return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
    }

    memcpy(path_out, path, len);
    path_out[len] = '\0';

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vpath_join_n(char *path_out, const size_t path_size, cplat_error *detail_out, const size_t part_count,
                       va_list args)
{
    size_t required_size = 1u;
    size_t offset = 0u;
    size_t idx;
    int need_sep = 0;
    va_list args_copy;

    if (path_out == NULL || path_size == 0u || part_count == 0u)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    path_out[0] = '\0';

    /* 1st pass: 引数検証と必要サイズの算出 (セパレータ補完分も含める) */
    va_copy(args_copy, args);
    {
        int have_prev_nonempty = 0;
        int prev_ends_with_sep = 0;

        for (idx = 0u; idx < part_count; ++idx)
        {
            const char *part = va_arg(args_copy, const char *);
            size_t part_len;
            size_t skip_leading = 0u;
            int part_starts_with_sep;

            if (part == NULL)
            {
                va_end(args_copy);
                return cplat_internal_error_report_errno(detail_out, EINVAL);
            }

            part_len = strlen(part);
            if (part_len == 0u)
            {
                continue;
            }

            part_starts_with_sep = path_is_sep(part[0]);

            if (have_prev_nonempty)
            {
                if (prev_ends_with_sep && part_starts_with_sep)
                {
                    /* 前後とも境界がセパレータ: 重複を畳むため先頭 1 文字を捨てる */
                    skip_leading = 1u;
                }
                else if (!prev_ends_with_sep && !part_starts_with_sep)
                {
                    /* どちらもセパレータでない: 1 つ補完する。
                       required_size はすでに終端 '\0' の 1 バイトを見込んでいるため、
                       補完後も path_size に収まるかを "以上" で判定する必要がある。 */
                    if (required_size >= path_size)
                    {
                        va_end(args_copy);
                        return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
                    }
                    required_size += 1u;
                }
            }

            if (part_len - skip_leading > path_size - required_size)
            {
                va_end(args_copy);
                return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            }
            required_size += part_len - skip_leading;

            have_prev_nonempty = 1;
            prev_ends_with_sep = path_is_sep(part[part_len - 1u]);
        }
    }
    va_end(args_copy);

    /* 2nd pass: 書き込み */
    for (idx = 0u; idx < part_count; ++idx)
    {
        const char *part = va_arg(args, const char *);
        size_t part_len = strlen(part);
        size_t skip_leading = 0u;

        if (part_len == 0u)
        {
            continue;
        }

        if (offset > 0u)
        {
            int prev_ends_with_sep = path_is_sep(path_out[offset - 1u]);
            int part_starts_with_sep = path_is_sep(part[0]);

            if (prev_ends_with_sep && part_starts_with_sep)
            {
                skip_leading = 1u;
            }
            else if (!prev_ends_with_sep && !part_starts_with_sep)
            {
                need_sep = 1;
            }
        }

        if (need_sep)
        {
            path_out[offset] = PLATFORM_PATH_SEP_CHR;
            offset += 1u;
            need_sep = 0;
        }

        memcpy(path_out + offset, part + skip_leading, part_len - skip_leading);
        offset += part_len - skip_leading;
    }

    path_out[offset] = '\0';

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_join_n(char *path_out, const size_t path_size, cplat_error *detail_out, const size_t part_count, ...)
{
    int result;
    va_list args;

    va_start(args, part_count);
    result = cplat_vpath_join_n(path_out, path_size, detail_out, part_count, args);
    va_end(args);

    return result;
}
