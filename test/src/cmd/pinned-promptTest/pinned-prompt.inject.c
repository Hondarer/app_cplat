/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "pinned-prompt.c"
#endif /* _IN_TEST_SRC */

#include "pinned-prompt.inject.h"

int test_pinned_prompt_cli_run_worker(cplat_pinned_prompt *screen, int *thread_running_out)
{
    pinned_prompt_cli_worker worker;

    worker_init(&worker, screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "stdout");

    if (worker_ensure_sync(&worker) != 0)
    {
        return -1;
    }

    /* worker_start() と同じく、スレッドの実行中の状態から周期の処理を始める。 */
    worker.thread_running = 1;
    worker_thread_proc(&worker);

    if (thread_running_out != NULL)
    {
        *thread_running_out = worker.thread_running;
    }

    cplat_condvar_dispose(worker.condvar);
    cplat_local_lock_dispose(worker.mutex);

    return 0;
}

void test_pinned_prompt_cli_process_status(cplat_pinned_prompt *screen, const char *arg)
{
    pinned_prompt_cli_session session;

    memset(&session, 0, sizeof(session));
    session.screen = screen;
    process_status(&session, arg);
}