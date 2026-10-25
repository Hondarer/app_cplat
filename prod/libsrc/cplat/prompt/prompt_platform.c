/**
 *******************************************************************************
 *  @file           prompt_platform.c
 *  @brief          プロンプトの端末判定と取得・復元を共通化します。
 *******************************************************************************
 */
#include "prompt_platform.h"
#include <cplat/console/console.h>
#include <cplat/crt/unistd.h>

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_is_tty(void)
{
    return cplat_isatty(CPLAT_STREAM_STDIN) && cplat_isatty(CPLAT_STREAM_STDOUT);
}

/* Doxygen コメントは、ヘッダーに記載 */

void prompt_platform_get_size(int *cols_out, int *rows_out)
{
    if (prompt_platform_os_get_size(cols_out, rows_out) != 0)
    {
        *cols_out = 80;
        *rows_out = 24;
        return;
    }
    if (*cols_out <= 0)
    {
        *cols_out = 80;
    }
    if (*rows_out <= 0)
    {
        *rows_out = 24;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_enter_raw(prompt_platform_context *context)
{
    if (prompt_platform_leave_raw(context) != 0)
    {
        return -1;
    }
    if (prompt_platform_os_enter_raw(context) == 0)
    {
        return 0;
    }
    if (prompt_platform_leave_raw(context) != 0)
    {
        return -1;
    }
    return 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_leave_raw(prompt_platform_context *context)
{
    return prompt_platform_os_leave_raw(context);
}
