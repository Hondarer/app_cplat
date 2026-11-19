/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef PINNED_PROMPT_CLI_INJECT_H
#define PINNED_PROMPT_CLI_INJECT_H

#include <cplat/prompt/pinned_prompt.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* 同期オブジェクトを用意したワーカーで worker_thread_proc() を呼び出し、終了後の thread_running を返す。 */
    extern int test_pinned_prompt_cli_run_worker(cplat_pinned_prompt *screen, int *thread_running_out);

    /* screen を持つセッションで process_status() を呼び出す。 */
    extern void test_pinned_prompt_cli_process_status(cplat_pinned_prompt *screen, const char *arg);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PINNED_PROMPT_CLI_INJECT_H */