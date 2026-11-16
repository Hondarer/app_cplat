/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 変数にアクセスできます
 */
#ifndef PROMPT_LINUX_INJECT_H
#define PROMPT_LINUX_INJECT_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* prompt_linux.c のファイル内 static 変数 s_resize_pending へのアクセサー。
       この変数は SIGWINCH ハンドラーからのみ設定されるため、
       端末サイズ変更を伴わずにリサイズ通知の経路を検証するために使用する。 */
    extern void test_prompt_set_resize_pending(int value);

    /* prompt_linux.c のファイル内 static 変数 s_resize_pending の値を返す。 */
    extern int test_prompt_resize_pending(void);

    /* prompt_linux.c のファイル内 static 変数 s_sigwinch_installed の値を返す。 */
    extern int test_prompt_sigwinch_installed(void);

    /* prompt_linux.c のファイル内 static 変数 s_sigwinch_installed を設定する。 */
    extern void test_prompt_set_sigwinch_installed(int value);

    typedef int (*test_prompt_pipe2_fn)(int fds[2], int flags);

    /* prompt_linux.c の pipe2 呼び出しを差し替える。NULL 指定で既定実装へ戻す。 */
    extern void test_prompt_set_pipe2_fn(test_prompt_pipe2_fn fn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PROMPT_LINUX_INJECT_H */
