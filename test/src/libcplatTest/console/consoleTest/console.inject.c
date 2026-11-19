/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 変数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "console.c"
#endif /* _IN_TEST_SRC */

#include "console.inject.h"

#if defined(PLATFORM_WINDOWS)

void test_console_set_initialized_state(const unsigned int input_cp, const unsigned int output_cp,
                                        const unsigned long stdout_mode, const unsigned long stderr_mode)
{
    cplat_atomic_store_i32(&s_initialized, 1, CPLAT_MEMORY_ORDER_SEQ_CST);
    s_orig_input_cp = (UINT)input_cp;
    s_orig_output_cp = (UINT)output_cp;
    s_orig_stdout_mode = (DWORD)stdout_mode;
    s_orig_stderr_mode = (DWORD)stderr_mode;
}

void test_console_reset_state(void)
{
    const cplat_once_flag initial_once = {0};

    cplat_atomic_store_i32(&s_initialized, 0, CPLAT_MEMORY_ORDER_SEQ_CST);
    cplat_atomic_store_i32(&s_attached_parent, 0, CPLAT_MEMORY_ORDER_SEQ_CST);
    s_orig_input_cp = 0;
    s_orig_output_cp = 0;
    s_orig_stdout_mode = 0;
    s_orig_stderr_mode = 0;
    s_console_shutdown_once = initial_once;
}

int test_console_get_initialized(void)
{
    return (int)cplat_atomic_load_i32(&s_initialized, CPLAT_MEMORY_ORDER_SEQ_CST);
}

#endif /* PLATFORM_WINDOWS */
