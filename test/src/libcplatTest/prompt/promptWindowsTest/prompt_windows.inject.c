/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 変数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 変数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "prompt_windows.c"
#endif /* _IN_TEST_SRC */

#include "prompt_windows.inject.h"

#if defined(PLATFORM_WINDOWS)

void test_prompt_windows_set_cancel_sync_io_fn(test_prompt_windows_cancel_sync_io_fn fn)
{
    s_cancel_sync_io_fn = (fn != NULL) ? (prompt_windows_cancel_sync_io_fn)fn : prompt_windows_default_cancel_sync_io;
}

void test_prompt_windows_reset_cancel_sync_io_fn(void)
{
    s_cancel_sync_io_fn = prompt_windows_default_cancel_sync_io;
}

int test_prompt_windows_begin_sync_input(prompt_platform_context *context)
{
    return prompt_windows_begin_sync_input(context);
}

int test_prompt_windows_end_sync_input(prompt_platform_context *context)
{
    return prompt_windows_end_sync_input(context);
}

#endif /* PLATFORM_WINDOWS */
