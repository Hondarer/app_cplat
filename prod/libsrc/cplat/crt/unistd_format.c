/**
 *******************************************************************************
 *  @file           unistd_format.c
 *  @brief          書式指定パスに対応したアクセス確認 API を実装します。
 *
 *  共通のパス書式処理を使用して cplat_access() を呼び出します。
 *
 *******************************************************************************
 */

#include <cplat/crt/unistd.h>
#include <cplat/crt/path.h>

#include <cplat/crt/path_format.h>
#include <cplat/base/error_internal.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vaccess_fmt(const int mode, cplat_error *detail_out, const char *format, va_list args)
{
    char filename[PLATFORM_PATH_UTF8_MAX] = {0};
    int format_error;

    if (cplat_internal_vformat_path(filename, sizeof(filename), format, args, &format_error) != 0)
    {
        cplat_internal_error_record_errno(detail_out, format_error);
        return -1;
    }

    return cplat_access(filename, mode, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_access_fmt(const int mode, cplat_error *detail_out, const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = cplat_vaccess_fmt(mode, detail_out, format, args);
    va_end(args);

    return result;
}
