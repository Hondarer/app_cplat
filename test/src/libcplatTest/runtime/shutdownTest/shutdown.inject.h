/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef SHUTDOWN_INJECT_H
#define SHUTDOWN_INJECT_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    extern void test_shutdown_reset_hook_once(void);

#if defined(PLATFORM_LINUX)
    extern void test_shutdown_signal_handler(int signal_number);
#endif /* PLATFORM_LINUX */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SHUTDOWN_INJECT_H */
