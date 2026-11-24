/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 変数にアクセスできます
 */
#ifndef CONSOLE_INJECT_H
#define CONSOLE_INJECT_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if defined(PLATFORM_WINDOWS)
    /* 初期化済みの状態にし、復元する元の設定を与える。 */
    extern void test_console_set_initialized_state(unsigned int input_cp, unsigned int output_cp,
                                                   unsigned long stdout_mode, unsigned long stderr_mode);

    /* 初期化の状態、元の設定、終了処理の登録の once を初期値へ戻す。 */
    extern void test_console_reset_state(void);

    /* 初期化済みであれば 1 を返す。 */
    extern int test_console_get_initialized(void);

    /* 診断ログ パスの組み立て結果を確認するためのアクセサー。 */
    extern int test_console_build_diag_log_path(wchar_t *path_out, size_t path_len);
#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CONSOLE_INJECT_H */
