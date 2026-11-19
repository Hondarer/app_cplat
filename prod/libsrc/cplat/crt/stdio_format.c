/**
 *******************************************************************************
 *  @file           stdio_format.c
 *  @brief          書式指定パスに対応した stdio API を実装します。
 *
 *  共通のパス書式処理を使用してファイルのオープンと削除を行います。
 *
 *******************************************************************************
 */

#include <cplat/crt/stdio.h>
#include <cplat/crt/path.h>

#include <cplat/crt/path_format.h>
#include <cplat/base/error_internal.h>

#include <errno.h>

/* Doxygen コメントは、ヘッダーに記載 */

FILE *cplat_vfopen_fmt(const char *modes, cplat_error *detail_out, const char *format, va_list args)
{
    char filename[PLATFORM_PATH_MAX] = {0};
    int format_error;

    if (modes == NULL)
    {
        cplat_internal_error_record_errno(detail_out, EINVAL);
        return NULL;
    }

    if (cplat_internal_vformat_path(filename, sizeof(filename), format, args, &format_error) != 0)
    {
        cplat_internal_error_record_errno(detail_out, format_error);
        return NULL;
    }

    return cplat_fopen(filename, modes, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

FILE *cplat_fopen_fmt(const char *modes, cplat_error *detail_out, const char *format, ...)
{
    FILE *result;
    va_list args;

    va_start(args, format);
    result = cplat_vfopen_fmt(modes, detail_out, format, args);
    va_end(args);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vremove_fmt(cplat_error *detail_out, const char *format, va_list args)
{
    char filename[PLATFORM_PATH_MAX] = {0};
    int format_error;

    if (cplat_internal_vformat_path(filename, sizeof(filename), format, args, &format_error) != 0)
    {
        cplat_internal_error_record_errno(detail_out, format_error);
        return -1;
    }

    return cplat_remove(filename, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_remove_fmt(cplat_error *detail_out, const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = cplat_vremove_fmt(detail_out, format, args);
    va_end(args);

    return result;
}
