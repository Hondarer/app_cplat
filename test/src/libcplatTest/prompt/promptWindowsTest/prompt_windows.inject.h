/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 変数・関数にアクセスできます
 */
#ifndef PROMPT_WINDOWS_INJECT_H
#define PROMPT_WINDOWS_INJECT_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include "prompt_platform.h"

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    typedef BOOL(WINAPI *test_prompt_windows_cancel_sync_io_fn)(HANDLE thread_handle);

    /* prompt_windows.c のファイル内 static 変数 s_cancel_sync_io_fn を差し替える。 */
    extern void test_prompt_windows_set_cancel_sync_io_fn(test_prompt_windows_cancel_sync_io_fn fn);

    /* s_cancel_sync_io_fn を既定の CancelSynchronousIo 実装へ戻す。 */
    extern void test_prompt_windows_reset_cancel_sync_io_fn(void);

    /* prompt_windows.c の static 関数 prompt_windows_begin_sync_input / end_sync_input を呼び出す。 */
    extern int test_prompt_windows_begin_sync_input(prompt_platform_context *context);
    extern int test_prompt_windows_end_sync_input(prompt_platform_context *context);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_WINDOWS */

#endif /* PROMPT_WINDOWS_INJECT_H */
