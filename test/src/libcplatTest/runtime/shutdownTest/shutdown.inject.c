/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "shutdown.c"
#endif /* _IN_TEST_SRC */

#include "shutdown.inject.h"

void test_shutdown_reset_hook_once(void)
{
    const cplat_once_flag initial_once = {0};
    s_shutdown_hook_once = initial_once;
}

void test_shutdown_reset_prepare(void)
{
    const cplat_once_flag initial_once = {0};

    /* ロックの once を初期値へ戻す前に、生成済みのロックを破棄する。 */
    if (s_shutdown_lock != NULL)
    {
        cplat_local_lock_dispose(s_shutdown_lock);
        s_shutdown_lock = NULL;
    }

    s_shutdown_lock_once = initial_once;
    s_shutdown_lock_result = CPLAT_OK;
    s_shutdown_hook_result = CPLAT_OK;
}

#if defined(PLATFORM_LINUX)
void test_shutdown_signal_handler(const int signal_number)
{
    shutdown_signal_handler(signal_number);
}
#endif /* PLATFORM_LINUX */
