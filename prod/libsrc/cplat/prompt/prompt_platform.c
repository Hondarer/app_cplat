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
    /* OS から端末サイズを取得できない場合は既定値 (80 列・24 行) を設定する。 */
    if (prompt_platform_os_get_size(cols_out, rows_out) != 0)
    {
        *cols_out = 80;
        *rows_out = 24;
        return;
    }

    /* 幅または高さが 0 以下の場合は個別に既定値で補う。 */
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
    /* 前回の未完了の復元項目があれば復元してから移行を試行する。 */
    if (prompt_platform_leave_raw(context) != 0)
    {
        return -1;
    }

    /* OS の raw モード設定に成功した場合は正常終了する。 */
    if (prompt_platform_os_enter_raw(context) == 0)
    {
        return 0;
    }

    /* 移行に失敗した場合は変更された設定を復元し、復元成功なら 1、復元未完なら -1 を返す。 */
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
