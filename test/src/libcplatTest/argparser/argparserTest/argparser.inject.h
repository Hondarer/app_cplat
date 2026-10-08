/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef ARGPARSER_INJECT_H
#define ARGPARSER_INJECT_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* argparser.c のファイル内 static 関数 argparser_is_valid_short_name へのアクセサー。
       判定関数の各条件を直接確認するため、inject 経由で呼び出す。 */
    extern int test_argparser_is_valid_short_name(const char *name);

    /* argparser.c のファイル内 static 関数 argparser_is_valid_long_name へのアクセサー。 */
    extern int test_argparser_is_valid_long_name(const char *name);

    /* argparser.c のファイル内 static 関数 argparser_default_dispose_on_shutdown へのアクセサー。 */
    extern void test_argparser_default_dispose_on_shutdown(const cplat_shutdown_event *event, void *context);

    /* argparser.c のファイル内 static 関数 argparser_default_acquire へのアクセサー。 */
    extern cplat_argparser *test_argparser_default_acquire(int argc, char *const *argv,
                                                           const cplat_argparser_options *options, int reset_existing);

    /* argparser.c のファイル内 static 関数 argparser_apply_args へのアクセサー。
       解析対象の引数だけを差し替えて再解析する経路を、登録をやり直さずに確認するために使用する。 */
    extern void test_argparser_apply_args(cplat_argparser *parser, int argc, char *const *argv);

    /* argparser.c のファイル内共有パーサー状態をテスト間で初期化する関数。 */
    extern void test_argparser_reset_default(void);

    /* usage の再計算でバッファー不足を発生させるためのテスト用置換関数。 */
    extern char *test_argparser_replace_program_description(cplat_argparser *parser, char *description);

    /* 登録エラーの各メッセージ分岐を確認するためのテスト用設定関数。 */
    extern void test_argparser_set_register_error_result(cplat_argparser *parser, size_t index, int result);

    /* 解析エラーの各メッセージ分岐を確認するためのテスト用設定関数。 */
    extern void test_argparser_set_last_error(cplat_argparser *parser, int result);

    /* 登録エラー対象の既定値分岐を確認するためのテスト用設定関数。 */
    extern void test_argparser_clear_register_error_target(cplat_argparser *parser, size_t index);

    /* 登録結果記録の早期終了条件を直接確認するためのテスト用関数。 */
    extern void test_argparser_record_register_result(cplat_argparser *parser, int result);

    /* 短い名前の長さ不一致枝を確認するため、登録済み short_name を差し替える。 */
    extern void test_argparser_replace_short_name(cplat_argparser *parser, size_t index, const char *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ARGPARSER_INJECT_H */
