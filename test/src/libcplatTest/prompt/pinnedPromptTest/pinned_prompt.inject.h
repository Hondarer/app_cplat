/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef PINNED_PROMPT_INJECT_H
#define PINNED_PROMPT_INJECT_H

#include <stddef.h>

#include <cplat/prompt/pinned_prompt.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* pinned_prompt.c の static 関数 cstr_len へのアクセサー。 */
    extern size_t test_pinned_prompt_cstr_len(const char *text);

    /* 非 TTY のフォールバック経路を選択するためのテスト用状態変更。 */
    extern void test_pinned_prompt_set_tty(cplat_pinned_prompt *screen, int is_tty);

    /* 内部キー分類のテスト用値です。 */
    enum test_pinned_prompt_key
    {
        TEST_PINNED_PROMPT_KEY_CHAR = 0,
        TEST_PINNED_PROMPT_KEY_TAB,
        TEST_PINNED_PROMPT_KEY_ENTER,
        TEST_PINNED_PROMPT_KEY_BACKSPACE,
        TEST_PINNED_PROMPT_KEY_DELETE,
        TEST_PINNED_PROMPT_KEY_LEFT,
        TEST_PINNED_PROMPT_KEY_RIGHT,
        TEST_PINNED_PROMPT_KEY_UP,
        TEST_PINNED_PROMPT_KEY_DOWN,
        TEST_PINNED_PROMPT_KEY_HOME,
        TEST_PINNED_PROMPT_KEY_END,
        TEST_PINNED_PROMPT_KEY_CTRL_C,
        TEST_PINNED_PROMPT_KEY_CLEAR,
        TEST_PINNED_PROMPT_KEY_RESIZE,
        TEST_PINNED_PROMPT_KEY_UNKNOWN,
        TEST_PINNED_PROMPT_KEY_ERROR,
        TEST_PINNED_PROMPT_KEY_EOF
    };

    /* 端末入力のキー分類へ直接アクセスする。 */
    extern int test_pinned_prompt_read_key(cplat_pinned_prompt *screen, int *ch_out);

    /* 行編集 static 関数を個別に検証する。 */
    extern void test_pinned_prompt_set_edit_line(cplat_pinned_prompt *screen, const char *line);
    extern void test_pinned_prompt_set_cursor(cplat_pinned_prompt *screen, size_t cursor);
    extern void test_pinned_prompt_insert_byte(cplat_pinned_prompt *screen, int ch);
    extern void test_pinned_prompt_backspace(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_delete(cplat_pinned_prompt *screen);
    extern const char *test_pinned_prompt_edit_text(const cplat_pinned_prompt *screen);
    extern size_t test_pinned_prompt_edit_length(const cplat_pinned_prompt *screen);

    /* TTY 描画 static 関数を検証するための状態設定と描画。 */
    extern void test_pinned_prompt_render_state(cplat_pinned_prompt *screen, int is_tty, int prompt_visible,
                                                int status_top_enabled, int status_bottom_enabled, const char *prompt,
                                                const char *edit_line, const char *top_left, const char *top_right,
                                                const char *bottom_left, const char *bottom_right);
    extern void test_pinned_prompt_render(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_resize(cplat_pinned_prompt *screen);

    /* 履歴と出力準備の境界経路へ直接アクセスする。 */
    extern void test_pinned_prompt_history_edge_cases(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_prepare_output(cplat_pinned_prompt *screen);

    /* 内部状態と描画ヘルパーの境界条件を個別に検証する。 */
    extern void test_pinned_prompt_set_internal_state(cplat_pinned_prompt *screen, int cols, int rows,
                                                      int previous_main_bottom_row, int prompt_visible,
                                                      int status_top_enabled, int status_bottom_enabled, size_t cursor,
                                                      size_t view_start);
    extern size_t test_pinned_prompt_view_start(const cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_set_mutex_active(cplat_pinned_prompt *screen, int mutex_active);
    extern void test_pinned_prompt_set_input_limits(cplat_pinned_prompt *screen, size_t history_max,
                                                    size_t input_max_bytes);
    extern void test_pinned_prompt_lock_and_unlock(cplat_pinned_prompt *screen);
    extern int test_pinned_prompt_set_prompt(cplat_pinned_prompt *screen, const char *prompt_string);
    extern void test_pinned_prompt_adjust_view(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_layout(cplat_pinned_prompt *screen, int *prompt_row, int *prompt_separator_row,
                                          int *main_bottom_row, int *show_top_status, int *show_bottom_status);
    extern void test_pinned_prompt_clear_control_area(cplat_pinned_prompt *screen, int main_bottom_row);
    extern void test_pinned_prompt_hide(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_finish(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_cleanup_terminal(cplat_pinned_prompt *screen);
    extern int test_pinned_prompt_format(cplat_pinned_prompt *screen, const char *format, ...);
    extern int test_pinned_prompt_set_status_content(cplat_pinned_prompt *screen, const char *content);

    /* 履歴の内部状態を構築して境界経路を検証する。 */
    extern void test_pinned_prompt_history_null_inputs(cplat_pinned_prompt *screen);
    extern int test_pinned_prompt_history_fill(cplat_pinned_prompt *screen, size_t count);
    extern int test_pinned_prompt_history_count(const cplat_pinned_prompt *screen);
    extern int test_pinned_prompt_history_failure_state(cplat_pinned_prompt *screen, const char *file, int line);
    extern void test_pinned_prompt_history_null_entry_paths(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_history_release_entries(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_render_status(cplat_pinned_prompt *screen, int row, const char *left,
                                                 const char *right);
    extern void test_pinned_prompt_history_next_null(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_history_next_null_entry(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_set_status_dirty(cplat_pinned_prompt *screen, int dirty);
    extern void test_pinned_prompt_destroy_mutex(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_history_add_after_null_last(cplat_pinned_prompt *screen);

    /* 共通層の境界で移行・復元失敗、または指定段階の入力失敗を注入する。 */
    extern void test_pinned_prompt_failures(int enter_result, int leave_result);
    /* 入力列は呼び出し中に生存する配列です。NULL で実 OS 処理への委譲に戻します。 */
    extern void test_pinned_prompt_input_results(const int *results, size_t count);

    /* raw モード状態を取得する。 */
    extern int test_pinned_prompt_raw_active(const cplat_pinned_prompt *screen);

    /* raw モードの後処理を伴う API の失敗経路を直接開始する。 */
    extern void test_pinned_prompt_set_raw_active(cplat_pinned_prompt *screen, int active);

    /* 端末サイズ取得へ直接アクセスする。 */
    extern void test_pinned_prompt_get_size(int *cols, int *rows);

    /* raw モード移行・復帰へ直接アクセスする。 */
    extern void test_pinned_prompt_enter_raw(cplat_pinned_prompt *screen);
    extern void test_pinned_prompt_leave_raw(cplat_pinned_prompt *screen);

    /* 端末入力の blocking/non-blocking 読み取りへ直接アクセスする。 */
    extern int test_pinned_prompt_read_char(cplat_pinned_prompt *screen);
    extern int test_pinned_prompt_read_char_nb(cplat_pinned_prompt *screen);

    /* 標準入出力の TTY 判定へ直接アクセスする。 */
    extern int test_pinned_prompt_platform_is_tty(void);

#if defined(PLATFORM_LINUX)
    /* Linux プラットフォーム層の static 状態をテスト開始状態へ戻す。 */
    extern void test_pinned_prompt_reset_platform_state(void);

    /* EINTR とリサイズ通知の経路を指定するための状態変更。 */
    extern void test_pinned_prompt_set_resize_pending(int value);

    /* リサイズ通知状態を取得する。 */
    extern int test_pinned_prompt_resize_pending(void);

    /* Linux プラットフォーム分岐の状態を個別に設定、呼び出す。 */
    extern void test_pinned_prompt_set_sigwinch_installed(int installed);
    extern void test_pinned_prompt_raise_resize_handler(void);
#endif /* PLATFORM_LINUX */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PINNED_PROMPT_INJECT_H */
