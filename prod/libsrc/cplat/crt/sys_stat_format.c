/**
 *******************************************************************************
 *  @file           sys_stat_format.c
 *  @brief          書式指定パスに対応したファイル情報 API を実装します。
 *
 *  共通のパス書式処理を使用して stat と mkdir の抽象 API を呼び出します。
 *
 *******************************************************************************
 */

#include <cplat/crt/sys/stat.h>
#include <cplat/crt/path.h>

#include <cplat/base/result.h>
#include <cplat/base/error_internal.h>
#include <cplat/crt/path_format.h>

#include <errno.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vstat_fmt(cplat_file_stat_t *buf, cplat_error *detail_out, const char *format, va_list args)
{
    char filename[PLATFORM_PATH_UTF8_MAX] = {0};
    int format_error;

    if (buf == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    if (cplat_internal_vformat_path(filename, sizeof(filename), format, args, &format_error) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, format_error);
    }

    return cplat_stat(buf, detail_out, filename);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_stat_fmt(cplat_file_stat_t *buf, cplat_error *detail_out, const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = cplat_vstat_fmt(buf, detail_out, format, args);
    va_end(args);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vmkdir_fmt(cplat_error *detail_out, const char *format, va_list args)
{
    char filename[PLATFORM_PATH_UTF8_MAX] = {0};
    int format_error;

    if (cplat_internal_vformat_path(filename, sizeof(filename), format, args, &format_error) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, format_error);
    }

    return cplat_mkdir(filename, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_mkdir_fmt(cplat_error *detail_out, const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = cplat_vmkdir_fmt(detail_out, format, args);
    va_end(args);

    return result;
}
