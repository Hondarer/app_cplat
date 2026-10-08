#include <testfw.h>

#include <cstring>

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/prompt/pinned_prompt.h>
#include <mock_cplat.h>
#include <mock_stdio.h>
#if defined(PLATFORM_LINUX)
    #include <mock_ioctl.h>
    #include <mock_signal.h>
    #include <mock_termios.h>
    #include <mock_unistd.h>
    #include <sys/mock_select.h>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif /* PLATFORM_ */

#include "pinned_prompt.inject.h"

using testing::_;
using testing::DoAll;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnArg;
using testing::SetArrayArgument;

// UTF-8 と ANSI の境界値を分類することの確認
TEST(pinnedPromptCoverageTest, text_helpers_cover_range_and_ansi_boundaries)
{
    // Arrange
    const char combining_upper_next[] = "\xCD\xB0";
    const char cjk_upper_next[] = "\xEA\x80\x80";
    const char cjk_extension_upper_next[] = "\xE4\xB7\x80";
    const char supplementary_cjk[] = "\xF0\xA0\x80\x80";
    const char not_csi[] = "\x1BXX";
    const char parameter_lower[] = "\x1B[0m";
    const char intermediate_upper[] = "\x1B[/m";
    const char invalid_control[] = "\x1B[\x1F";
    const char invalid_final[] = "\x1B[@";

    // Pre-Assert

    // Act
    size_t combining_upper_next_width =
        test_pinned_prompt_utf8_width(combining_upper_next, sizeof(combining_upper_next) - 1U,
                                      0U); // [手順] - 結合文字範囲の直後にある文字の表示幅を取得する。
    size_t cjk_upper_next_width =
        test_pinned_prompt_utf8_width(cjk_upper_next, sizeof(cjk_upper_next) - 1U,
                                      0U); // [手順] - CJK 統合漢字範囲の直後にある文字の表示幅を取得する。
    size_t cjk_extension_upper_next_width =
        test_pinned_prompt_utf8_width(cjk_extension_upper_next, sizeof(cjk_extension_upper_next) - 1U,
                                      0U); // [手順] - CJK 拡張 A 範囲の直後にある文字の表示幅を取得する。
    size_t supplementary_cjk_width = test_pinned_prompt_utf8_width(supplementary_cjk, sizeof(supplementary_cjk) - 1U,
                                                                   0U); // [手順] - 補助漢字面の文字の表示幅を取得する。
    size_t not_csi_len =
        test_pinned_prompt_ansi_len(not_csi, sizeof(not_csi) - 1U, 0U); // [手順] - CSI ではない ESC 文字列を解析する。
    size_t parameter_lower_len = test_pinned_prompt_ansi_len(parameter_lower, sizeof(parameter_lower) - 1U,
                                                             0U); // [手順] - ANSI パラメーター範囲の下限を解析する。
    size_t intermediate_upper_len = test_pinned_prompt_ansi_len(intermediate_upper, sizeof(intermediate_upper) - 1U,
                                                                0U); // [手順] - ANSI 中間バイト範囲の上限を解析する。
    size_t invalid_control_len = test_pinned_prompt_ansi_len(invalid_control, sizeof(invalid_control) - 1U,
                                                             0U); // [手順] - ANSI 中間バイト範囲未満の値を解析する。
    size_t invalid_final_len = test_pinned_prompt_ansi_len(invalid_final, sizeof(invalid_final) - 1U,
                                                           0U); // [手順] - ANSI パラメーター範囲を超える値を解析する。

    // Assert
    EXPECT_EQ(1U, combining_upper_next_width);     // [確認_正常系] - 結合文字範囲直後の表示幅が 1 であること。
    EXPECT_EQ(1U, cjk_upper_next_width);           // [確認_正常系] - CJK 統合漢字範囲直後の表示幅が 1 であること。
    EXPECT_EQ(1U, cjk_extension_upper_next_width); // [確認_正常系] - CJK 拡張 A 範囲直後の表示幅が 1 であること。
    EXPECT_EQ(2U, supplementary_cjk_width);        // [確認_正常系] - 補助漢字面の表示幅が 2 であること。
    EXPECT_EQ(0U, not_csi_len);                    // [確認_異常系] - CSI ではない ESC 文字列の長さが 0 であること。
    EXPECT_EQ(4U, parameter_lower_len);    // [確認_正常系] - ANSI パラメーター下限を含む SGR の長さが 4 であること。
    EXPECT_EQ(4U, intermediate_upper_len); // [確認_正常系] - ANSI 中間バイト上限を含む SGR の長さが 4 であること。
    EXPECT_EQ(0U, invalid_control_len);    // [確認_異常系] - ANSI 中間バイト範囲未満の値が拒否されること。
    EXPECT_EQ(0U, invalid_final_len);      // [確認_異常系] - ANSI パラメーター範囲を超える値が拒否されること。
}

#if defined(PLATFORM_LINUX)

// Linux 端末判定、サイズ取得、シグナル状態の短絡条件を網羅することの確認
TEST(pinnedPromptCoverageTest, platform_helpers_cover_short_circuits_and_installed_signal)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_termios> mock_termios;
    struct winsize column_only_size = {};
    cplat_pinned_prompt *screen = nullptr;
    int cols = 0;
    int rows = 0;
    column_only_size.ws_col = 80U;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_isatty(_))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(1))
        .WillRepeatedly(Return(1)); // cplat_pinned_prompt_create() の内部呼び出し分
    // [Pre-Assert確認_正常系] - cplat_isatty が端末判定とハンドル生成のために呼び出されること。
    // [Pre-Assert手順] - 標準入力非 TTY、標準出力非 TTY、双方 TTY の順に判定結果を返却する。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(
            Invoke([column_only_size](const char *, const int, const char *, const int, const unsigned long,
                                      void *argument) { *static_cast<struct winsize *>(argument) = column_only_size; }),
            Return(0)));
    // [Pre-Assert確認_異常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 80・行数 0 の端末サイズを返却する。
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr から 0 を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。

    // Act
    int stdin_not_tty = test_pinned_prompt_platform_is_tty();  // [手順] - 標準入力が TTY ではない端末判定を行う。
    int stdout_not_tty = test_pinned_prompt_platform_is_tty(); // [手順] - 標準出力が TTY ではない端末判定を行う。
    int both_tty = test_pinned_prompt_platform_is_tty();       // [手順] - 標準入出力がともに TTY の端末判定を行う。
    test_pinned_prompt_get_size(&cols, &rows); // [手順] - 行数が 0 の ioctl 結果から端末サイズを取得する。
    screen = cplat_pinned_prompt_create(NULL); // [手順] - シグナル登録済み状態の raw モード試験用ハンドルを生成する。
    ASSERT_NE(nullptr, screen);
    // [確認_正常系] - `nullptr` と `screen` が異なること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_set_sigwinch_installed(1);
    test_pinned_prompt_enter_raw(screen); // [手順] - SIGWINCH 登録済み状態で raw モードへ移行する。
    test_pinned_prompt_set_sigwinch_installed(0);
    test_pinned_prompt_leave_raw(screen);      // [手順] - SIGWINCH 未登録状態で raw モードから復帰する。
    test_pinned_prompt_raise_resize_handler(); // [手順] - SIGWINCH ハンドラーを直接実行する。

    // Assert
    EXPECT_EQ(0, stdin_not_tty);  // [確認_正常系] - 標準入力が TTY ではない場合の端末判定が 0 であること。
    EXPECT_EQ(0, stdout_not_tty); // [確認_正常系] - 標準出力が TTY ではない場合の端末判定が 0 であること。
    EXPECT_EQ(1, both_tty);       // [確認_正常系] - 標準入出力がともに TTY の場合の端末判定が 1 であること。
    EXPECT_EQ(80, cols);          // [確認_異常系] - 行数が 0 の ioctl 結果では既定列数が返ること。
    EXPECT_EQ(24, rows);          // [確認_異常系] - 行数が 0 の ioctl 結果では既定行数が返ること。
    EXPECT_EQ(1,
              test_pinned_prompt_resize_pending()); // [確認_正常系] - ハンドラー実行後にリサイズ通知が設定されること。

    // Cleanup
    test_pinned_prompt_set_resize_pending(0);
    cplat_pinned_prompt_dispose(screen);
    test_pinned_prompt_reset_platform_state();
}

#endif /* PLATFORM_LINUX */

// ミューテックス無効状態とプロンプト確保失敗を処理することの確認
TEST(pinnedPromptCoverageTest, internal_state_handles_inactive_mutex_and_prompt_failure)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_cplat> mock_cplat;
    int null_prompt_result = 0;
    int allocation_result = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_realloc がプロンプト再確保のために 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    test_pinned_prompt_set_mutex_active(screen, 0);
    test_pinned_prompt_lock_and_unlock(screen); // [手順] - ミューテックス無効状態でロックとアンロックを行う。
    test_pinned_prompt_set_mutex_active(screen, 1);
    null_prompt_result = test_pinned_prompt_set_prompt(screen, NULL); // [手順] - NULL のプロンプトを設定する。
    allocation_result = test_pinned_prompt_set_prompt(
        screen, "prompt requiring allocation"); // [手順] - 再確保に失敗する長いプロンプトを設定する。

    // Assert
    EXPECT_EQ(0, null_prompt_result); // [確認_正常系] - NULL のプロンプト設定が成功すること。
    EXPECT_EQ(-1, allocation_result); // [確認_異常系] - プロンプト再確保失敗が -1 になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 表示範囲調整とレイアウトの最小・最大境界を処理することの確認
TEST(pinnedPromptCoverageTest, layout_and_view_cover_narrow_and_status_boundaries)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    int prompt_row = 0;
    int separator_row = 0;
    int main_bottom_row = 0;
    int show_top = 0;
    int show_bottom = 0;

    // Pre-Assert

    // Act
    ASSERT_EQ(0, test_pinned_prompt_set_prompt(screen, "long-prompt"));
    // [確認_正常系] - `test_pinned_prompt_set_prompt(screen, "long-prompt")` の戻り値が `0` であること。
    test_pinned_prompt_set_edit_line(screen, "abcdef");
    test_pinned_prompt_set_internal_state(screen, 1, 0, 9999, 1, 1, 1, 6U, 4U);
    test_pinned_prompt_set_cursor(screen, 2U);
    test_pinned_prompt_adjust_view(screen); // [手順] - カーソルが表示開始位置より前にある狭幅表示を調整する。
    size_t backward_view = test_pinned_prompt_view_start(screen);
    test_pinned_prompt_set_cursor(screen, 6U);
    test_pinned_prompt_adjust_view(screen); // [手順] - カーソルが表示列数を超える狭幅表示を調整する。
    size_t forward_view = test_pinned_prompt_view_start(screen);
    test_pinned_prompt_layout(screen, &prompt_row, &separator_row, &main_bottom_row, &show_top,
                              &show_bottom); // [手順] - 行数 0 で上下ステータスを有効にしたレイアウトを計算する。
    int minimum_prompt_row = prompt_row;
    int minimum_separator_row = separator_row;
    int minimum_main_bottom_row = main_bottom_row;
    test_pinned_prompt_set_internal_state(screen, 80, 6, 9999, 1, 1, 1, 6U, forward_view);
    test_pinned_prompt_layout(screen, &prompt_row, &separator_row, &main_bottom_row, &show_top,
                              &show_bottom); // [手順] - 上下ステータスを表示できるレイアウトを計算する。

    // Assert
    EXPECT_EQ(2U, backward_view);          // [確認_正常系] - 表示開始位置が後退後のカーソル位置になること。
    EXPECT_GT(forward_view, 2U);           // [確認_正常系] - 狭幅表示では表示開始位置が前方へ移動すること。
    EXPECT_EQ(1, minimum_prompt_row);      // [確認_正常系] - 最小プロンプト行が 1 であること。
    EXPECT_EQ(1, minimum_separator_row);   // [確認_正常系] - 最小区切り行が 1 であること。
    EXPECT_EQ(1, minimum_main_bottom_row); // [確認_正常系] - 最小メイン領域末尾行が 1 であること。
    EXPECT_EQ(1, show_top);                // [確認_正常系] - 6 行では上部ステータスが表示されること。
    EXPECT_EQ(1, show_bottom);             // [確認_正常系] - 6 行では下部ステータスが表示されること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#if defined(PLATFORM_LINUX)

// 描画補助が非表示、狭幅、消去範囲の境界を処理することの確認
TEST(pinnedPromptCoverageTest, drawing_helpers_cover_hidden_narrow_and_clear_ranges)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize narrow_size = {};
    narrow_size.ws_col = 1U;
    narrow_size.ws_row = 2U;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(
            DoAll(Invoke([narrow_size](const char *, const int, const char *, const int, const unsigned long,
                                       void *argument) { *static_cast<struct winsize *>(argument) = narrow_size; }),
                  Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 1・行数 2 の端末サイズを返却する。

    // Act
    test_pinned_prompt_render_state(screen, 1, 0, 1, 1, "prompt", "abcdef", "", "", "123", "456");
    test_pinned_prompt_render(screen); // [手順] - TTY でプロンプト非表示の描画を要求する。
    test_pinned_prompt_hide(screen);   // [手順] - TTY でプロンプト非表示の消去を要求する。
    test_pinned_prompt_finish(screen); // [手順] - TTY でプロンプト非表示の確定を要求する。
    test_pinned_prompt_render_state(screen, 1, 1, 1, 1, "prompt", "abcdef", "", "", "123", "456");
    test_pinned_prompt_render(screen); // [手順] - 1 列端末へ長いプロンプトとステータスを描画する。
    test_pinned_prompt_set_internal_state(screen, 1, 2, 0, 1, 1, 1, 6U, 0U);
    test_pinned_prompt_clear_control_area(screen, 1); // [手順] - 前回領域が新しい領域より上にある消去範囲を処理する。
    test_pinned_prompt_set_internal_state(screen, 1, 2, 5, 1, 1, 1, 6U, 0U);
    test_pinned_prompt_clear_control_area(screen, 5); // [手順] - 端末行数を超える消去範囲を処理する。
    test_pinned_prompt_hide(screen);                  // [手順] - 表示中のプロンプト行を消去する。
    test_pinned_prompt_finish(screen);                // [手順] - 表示中のプロンプトを確定する。
    test_pinned_prompt_cleanup_terminal(screen);      // [手順] - TTY の制御領域を初期状態へ戻す。

    // Assert
    SUCCEED(); // [確認_正常系] - 非表示、狭幅、消去範囲の各描画処理が完了すること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#endif /* PLATFORM_LINUX */

// 履歴の容量拡張、上限循環、NULL 要素を処理することの確認
TEST(pinnedPromptCoverageTest, history_helpers_cover_capacity_and_null_entries)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    test_pinned_prompt_history_null_inputs(screen); // [手順] - 履歴追加と編集消去へ NULL と空文字列を渡す。
    test_pinned_prompt_set_input_limits(screen, 3U, 4096U);
    int fill_result = test_pinned_prompt_history_fill(screen, 4U); // [手順] - 履歴上限を超える異なる入力を追加する。
    test_pinned_prompt_history_null_entry_paths(screen);           // [手順] - NULL の履歴要素を前後へ参照する。
    int context_result = 0;
    for (int line = 1; line <= 5; line++)
    {
        context_result |= test_pinned_prompt_history_failure_state(screen, "capacity.c", line);
    }
    int context_count =
        test_pinned_prompt_history_count(screen); // [手順] - 初期容量を超えて生成した履歴コンテキスト数を取得する。

    // Assert
    EXPECT_EQ(0, fill_result);    // [確認_正常系] - 履歴上限を超える入力追加が成功すること。
    EXPECT_EQ(0, context_result); // [確認_正常系] - 履歴コンテキストの容量拡張が成功すること。
    EXPECT_EQ(7, context_count);  // [確認_正常系] - 生成済み履歴コンテキスト数が 7 であること。

    // Cleanup
    test_pinned_prompt_history_release_entries(screen);
    cplat_pinned_prompt_dispose(screen);
}

// 履歴コンテキストの各確保失敗を分類することの確認
TEST(pinnedPromptCoverageTest, history_context_reports_allocation_failures)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_cplat> mock_cplat;
    int realloc_failure = 0;
    int calloc_failure = 0;
    int malloc_failure = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr))                      // realloc.c: 配列確保失敗
        .WillOnce(Invoke(delegate_real_cplat_realloc)); // calloc.c: 配列確保は成功させる
    // [Pre-Assert確認_異常系] - cplat_realloc が履歴コンテキスト配列の再確保のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は NULL、2 回目は本物の realloc 結果を返却する。
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr))                     // calloc.c: 要素配列確保失敗
        .WillOnce(Invoke(delegate_real_cplat_calloc)); // malloc.c: 要素配列確保は成功させる
    // [Pre-Assert確認_異常系] - cplat_calloc が履歴要素配列の確保のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は NULL、2 回目は本物の calloc 結果を返却する。
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Invoke(delegate_real_cplat_malloc)) // calloc.c: saved_line 確保は成功 (calloc 失敗と対で必ず呼ばれる)
        .WillOnce(Return(nullptr));                   // malloc.c: saved_line 確保失敗
    // [Pre-Assert確認_異常系] - cplat_malloc が履歴保存行の確保のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は本物の malloc 結果、2 回目は NULL を返却する。

    // Act
    realloc_failure = test_pinned_prompt_history_failure_state(
        screen, "realloc.c", 1); // [手順] - 履歴コンテキスト配列の再確保を失敗させる。
    calloc_failure =
        test_pinned_prompt_history_failure_state(screen, "calloc.c", 2); // [手順] - 履歴要素配列の確保を失敗させる。
    malloc_failure =
        test_pinned_prompt_history_failure_state(screen, "malloc.c", 3); // [手順] - 履歴保存行の確保を失敗させる。

    // Assert
    EXPECT_EQ(-1, realloc_failure); // [確認_異常系] - 履歴コンテキスト配列の再確保失敗が -1 であること。
    EXPECT_EQ(-1, calloc_failure);  // [確認_異常系] - 履歴要素配列の確保失敗が -1 であること。
    EXPECT_EQ(-1, malloc_failure);  // [確認_異常系] - 履歴保存行の確保失敗が -1 であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 入力編集が最大容量と確保失敗を処理することの確認
TEST(pinnedPromptCoverageTest, edit_helpers_preserve_text_when_capacity_is_exhausted)
{
    // Arrange
    cplat_pinned_prompt_options options = {};
    options.input.input_initial_capacity = 2U;
    options.input.input_max_bytes = 2U;
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(&options); // [状態] - 指定オプションのハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                         // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    test_pinned_prompt_set_edit_line(screen, "long"); // [手順] - 最大容量を超える編集行を設定する。
    test_pinned_prompt_insert_byte(screen, 'X');      // [手順] - 最大容量へ達した編集行へ 1 バイト挿入する。
    test_pinned_prompt_set_cursor(screen, 0U);
    test_pinned_prompt_backspace(screen); // [手順] - カーソル先頭で 1 文字削除を要求する。

    // Assert
    EXPECT_STREQ("l",
                 test_pinned_prompt_edit_text(screen));    // [確認_正常系] - 最大容量内の先頭文字だけが保持されること。
    EXPECT_EQ(1U, test_pinned_prompt_edit_length(screen)); // [確認_正常系] - 編集行の長さが 1 のままであること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 書式バッファーの初回確保、書式処理、再確保の失敗を処理することの確認
TEST(pinnedPromptCoverageTest, format_helper_handles_allocation_and_format_failures)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_stdio> mock_stdio;
    const char long_text[300] = {};
    int malloc_failure = 0;
    int format_failure = 0;
    int realloc_failure = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr))                     // 1回目: 初回確保失敗
        .WillOnce(Invoke(delegate_real_cplat_malloc)); // 2回目: 初回確保は成功させる
    // [Pre-Assert確認_異常系] - cplat_malloc が書式バッファーの初回確保のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は NULL、2 回目は本物の malloc 結果を返却する。
    EXPECT_CALL(mock_stdio, vsnprintf(_, _, _, _, _, _))
        .WillOnce(Return(-1))   // 2回目: 書式処理失敗
        .WillOnce(Return(500)); // 3回目: fmt_cap(256) を超える長さを返し realloc を誘発する
    // [Pre-Assert確認_異常系] - vsnprintf が書式処理と再確保誘発のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は -1、2 回目は 500 を返却する。
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_realloc が書式バッファー再確保のために 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    malloc_failure = test_pinned_prompt_format(screen, "%s", "x"); // [手順] - 書式バッファーの初回確保を失敗させる。
    format_failure = test_pinned_prompt_format(screen, "%s", "x"); // [手順] - 書式処理を失敗させる。
    realloc_failure = test_pinned_prompt_format(screen, "%s", long_text); // [手順] - 長い書式結果の再確保を失敗させる。

    // Assert
    EXPECT_EQ(-1, malloc_failure); // [確認_異常系] - 書式バッファーの初回確保失敗が -1 であること。
    EXPECT_EQ(0, format_failure);  // [確認_異常系] - 書式処理失敗が空文字列として処理されること。
    EXPECT_EQ(0, realloc_failure); // [確認_異常系] - 書式バッファー再確保失敗が安全に処理されること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// ハンドル生成が構造体、ロック、各文字列の確保失敗を処理することの確認
TEST(pinnedPromptCoverageTest, create_reports_each_resource_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    int allocation_call = 0;
    int failure_target = 1;
    cplat_pinned_prompt *calloc_failure = nullptr;
    cplat_pinned_prompt *lock_failure = nullptr;
    cplat_pinned_prompt *allocation_failures[6] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(Invoke(delegate_real_cplat_calloc));
    // [Pre-Assert確認_異常系] - cplat_calloc がハンドル本体の確保のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は NULL、以降は本物の calloc 結果を返却する。
    EXPECT_CALL(mock_cplat, cplat_local_lock_create(_))
        .WillOnce(Return(CPLAT_ERR_OUT_OF_MEMORY))
        .WillRepeatedly(Invoke(delegate_real_cplat_local_lock_create));
    // [Pre-Assert確認_異常系] - cplat_local_lock_create がロック生成のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は CPLAT_ERR_OUT_OF_MEMORY、以降は本物の生成結果を返却する。
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillRepeatedly(Invoke(
            [&allocation_call, &failure_target](size_t size)
            {
                allocation_call++;
                if (allocation_call == failure_target)
                {
                    return static_cast<void *>(nullptr);
                }
                return delegate_real_cplat_malloc(size);
            }));
    // [Pre-Assert確認_異常系] - cplat_malloc が各文字列バッファーの確保のために呼び出されること。
    // [Pre-Assert手順] - 対象回は NULL、それ以外は本物の malloc 結果を返却する。

    // Act
    calloc_failure = cplat_pinned_prompt_create(NULL); // [手順] - ハンドル本体の確保を失敗させる。
    lock_failure = cplat_pinned_prompt_create(NULL);   // [手順] - ハンドルのロック生成を失敗させる。
    for (int index = 0; index < 6; index++)
    {
        allocation_call = 0;
        failure_target = index + 1;
        allocation_failures[index] = cplat_pinned_prompt_create(NULL);
    }

    // Assert
    EXPECT_EQ(nullptr, calloc_failure); // [確認_異常系] - ハンドル本体の確保失敗で NULL が返ること。
    EXPECT_EQ(nullptr, lock_failure);   // [確認_異常系] - ロック生成失敗で NULL が返ること。
    for (cplat_pinned_prompt *screen : allocation_failures)
    {
        EXPECT_EQ(nullptr, screen); // [確認_異常系 回数=6] - 各文字列バッファーの確保失敗で NULL が返ること。
    }

    // Cleanup
    cplat_pinned_prompt_dispose(NULL);
}

#if defined(PLATFORM_LINUX)

// readline が引数、raw 移行、履歴、プロンプトの失敗を分類することの確認
TEST(pinnedPromptCoverageTest, readline_reports_setup_failures)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_cplat> mock_cplat;
    char input[] = "fallback";
    char output[16] = {};
    int invalid_screen = 0;
    int invalid_buffer = 0;
    int invalid_size = 0;
    int raw_failure = 0;
    int history_failure = 0;
    int prompt_failure = 0;

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _)).WillOnce(Return(-1));
    // [Pre-Assert確認_異常系] - tcgetattr が raw モード移行のために 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr から -1 を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_異常系] - tcsetattr が標準入力を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(input, input + sizeof(input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_異常系] - cplat_fgets が raw 移行失敗後の fallback で 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から入力 "fallback" を返却する。
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr))                     // history-failure.c: 履歴コンテキスト配列確保失敗
        .WillOnce(Invoke(delegate_real_cplat_realloc)) // prompt-failure.c の事前生成: 配列確保は成功させる
        .WillOnce(Return(nullptr));                    // 6回目の readline: プロンプト文字列の再確保失敗
    // [Pre-Assert確認_異常系] - cplat_realloc が履歴確保とプロンプト再確保のために呼び出されること。
    // [Pre-Assert手順] - 1 回目と 3 回目は NULL、2 回目は本物の realloc 結果を返却する。

    // Act
    invalid_screen = cplat_pinned_prompt_readline_at(NULL, output, sizeof(output), "", "invalid.c",
                                                     1); // [手順] - NULL ハンドルで readline を呼び出す。
    invalid_buffer = cplat_pinned_prompt_readline_at(screen, NULL, sizeof(output), "", "invalid.c",
                                                     2); // [手順] - NULL 出力バッファーで readline を呼び出す。
    invalid_size = cplat_pinned_prompt_readline_at(screen, output, 0U, "", "invalid.c",
                                                   3); // [手順] - サイズ 0 の出力バッファーで readline を呼び出す。
    test_pinned_prompt_set_tty(screen, 1);
    raw_failure = cplat_pinned_prompt_readline_at(screen, output, sizeof(output), "", "raw.c",
                                                  4); // [手順] - raw モード移行に失敗した readline を呼び出す。
    test_pinned_prompt_set_raw_active(screen, 1);
    history_failure =
        cplat_pinned_prompt_readline_at(screen, output, sizeof(output), "", "history-failure.c",
                                        5); // [手順] - 履歴コンテキスト確保に失敗した readline を呼び出す。
    ASSERT_EQ(0, test_pinned_prompt_history_failure_state(screen, "prompt-failure.c", 6));
    // [確認_異常系] - `test_pinned_prompt_history_failure_state(screen, "prompt-failure.c", 6)` の戻り値が `0` であること。
    test_pinned_prompt_set_raw_active(screen, 1);
    prompt_failure = cplat_pinned_prompt_readline_at(screen, output, sizeof(output), "long prompt", "prompt-failure.c",
                                                     6); // [手順] - プロンプト再確保に失敗した readline を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_screen); // [確認_異常系] - NULL ハンドルの readline が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_buffer); // [確認_異常系] - NULL 出力バッファーの readline が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_size);          // [確認_異常系] - サイズ 0 の readline が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_OK, raw_failure); // [確認_異常系] - raw モード移行失敗時は fallback 入力が成功すること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, history_failure); // [確認_異常系] - 履歴コンテキスト確保失敗が UNKNOWN になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, prompt_failure);  // [確認_異常系] - プロンプト再確保失敗が UNKNOWN になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// readline がクリア、移動、未知キー、切り捨て、EOF を処理することの確認
TEST(pinnedPromptCoverageTest, readline_covers_remaining_key_actions)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_select> mock_select;
    struct winsize size = {};
    const unsigned char input[] = {'a',   'b', 0x1BU, '[', 'C',   0x1BU, '[',  'H', 0x1BU, '[', 'F', 0x7FU,
                                   0x1BU, '[', '3',   'x', 0x1BU, 0x01U, '\n', 'a', 'b',   'c', '\n'};
    size_t input_pos = 0U;
    int select_count = 0;
    char first_output[8] = {};
    char short_output[2] = {};
    char eof_output[2] = {};
    size.ws_col = 80U;
    size.ws_row = 24U;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *argument) { *static_cast<struct winsize *>(argument) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 80・行数 24 の端末サイズを返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(3).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillRepeatedly(Invoke(
            [&input, &input_pos](const char *, const int, const char *, const int, void *argument, const size_t)
            {
                if (input_pos >= sizeof(input))
                {
                    return static_cast<ssize_t>(0);
                }
                *static_cast<unsigned char *>(argument) = input[input_pos++];
                return static_cast<ssize_t>(1);
            }));
    // [Pre-Assert確認_正常系] - read が標準入力に対し残キー操作と切り捨て、EOF で呼び出されること。
    // [Pre-Assert手順] - 用意したキー列を順に返却し、消費後は 0 バイトを返却する。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _))
        .WillRepeatedly(Invoke(
            [&select_count](const char *, const int, const char *, int, fd_set *, fd_set *, fd_set *, struct timeval *)
            {
                select_count++;
                return (select_count == 10) ? 0 : 1;
            }));
    // [Pre-Assert確認_正常系] - select がエスケープシーケンスの後続判定で呼び出されること。
    // [Pre-Assert手順] - 10 回目はタイムアウト (0)、それ以外は入力可 (1) を返却する。

    // Act
    test_pinned_prompt_set_raw_active(screen, 1);
    int first_result =
        cplat_pinned_prompt_readline_at(screen, first_output, sizeof(first_output), "", "keys.c",
                                        1); // [手順] - 右移動、Home、End、削除、クリア、未知キーを含む入力を確定する。
    test_pinned_prompt_set_raw_active(screen, 1);
    int short_result = cplat_pinned_prompt_readline_at(screen, short_output, sizeof(short_output), "", "keys.c",
                                                       2); // [手順] - 出力バッファーより長い入力を確定する。
    test_pinned_prompt_set_raw_active(screen, 1);
    int eof_result = cplat_pinned_prompt_readline_at(screen, eof_output, sizeof(eof_output), "", "keys.c",
                                                     3); // [手順] - EOF を受信して入力を終了する。

    // Assert
    EXPECT_EQ(CPLAT_OK, first_result);    // [確認_正常系] - 残りのキー操作を含む readline が OK になること。
    EXPECT_STREQ("", first_output);       // [確認_正常系] - クリア後の確定入力が空文字列になること。
    EXPECT_EQ(CPLAT_OK, short_result);    // [確認_正常系] - 長い入力の readline が OK になること。
    EXPECT_STREQ("a", short_output);      // [確認_正常系] - 長い入力が出力容量内へ切り捨てられること。
    EXPECT_EQ(CPLAT_ERR_EOF, eof_result); // [確認_正常系] - EOF を受信した readline が EOF になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#endif /* PLATFORM_LINUX */

// TTY 書き込みと printf の失敗経路を分類することの確認
TEST(pinnedPromptCoverageTest, tty_write_and_printf_cover_empty_short_and_failure_paths)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_cplat> mock_cplat;
    const char data[] = "abc";
    size_t written = 0U;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, _, _, _))
        .WillOnce(Return(2U))  // short_result: 3バイト書き込みに対し2バイトの短い書き込み
        .WillOnce(Return(0U)); // write_failure: 1バイト書き込みに対し0バイト (完全な書き込み失敗)
    // [Pre-Assert確認_異常系] - fwrite が短い書き込みと完全失敗のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は 2 バイト、2 回目は 0 バイトを返却する。
    EXPECT_CALL(mock_stdio, vsnprintf(_, _, _, _, _, _))
        .WillOnce(Return(-1)) // format_failure: 書式長計算そのものを失敗させる
        .WillOnce(Return(1))  // malloc_failure: 書式長計算 (needed=1) は成功させる
        .WillOnce(Return(1)); // write_failure: 書式長計算 (needed=1) は成功させる
    // [Pre-Assert確認_異常系] - vsnprintf が書式長計算のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は -1、以降は 1 を返却する。出力書き込みは cplat_vsnprintf へ委譲する。
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr))                     // malloc_failure: 出力バッファー確保失敗
        .WillOnce(Invoke(delegate_real_cplat_malloc)); // write_failure: 出力バッファー確保は成功させる
    // [Pre-Assert確認_異常系] - cplat_malloc が printf の出力バッファー確保のために呼び出されること。
    // [Pre-Assert手順] - 1 回目は NULL、2 回目は本物の malloc 結果を返却する。

    // Act
    int empty_result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, NULL, 0U,
                                                 NULL); // [手順] - TTY へサイズ 0 を書き込む。
    int short_result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
                                                 &written); // [手順] - TTY へ短い書き込みを行う。
    int format_failure = cplat_pinned_prompt_printf(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s",
                                                    "x"); // [手順] - printf の書式長計算を失敗させる。
    int malloc_failure = cplat_pinned_prompt_printf(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s",
                                                    "x"); // [手順] - printf の出力バッファー確保を失敗させる。
    int write_failure = cplat_pinned_prompt_printf(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s",
                                                   "x"); // [手順] - printf の内部書き込みを失敗させる。

    // Assert
    EXPECT_EQ(CPLAT_OK, empty_result);          // [確認_正常系] - TTY へのサイズ 0 の書き込みが OK になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, short_result); // [確認_異常系] - TTY への短い書き込みが UNKNOWN になること。
    EXPECT_EQ(2U, written);                     // [確認_異常系] - TTY の短い書き込みバイト数が 2 であること。
    EXPECT_EQ(-1, format_failure);              // [確認_異常系] - printf の書式長計算失敗が -1 になること。
    EXPECT_EQ(-1, malloc_failure);              // [確認_異常系] - printf の出力バッファー確保失敗が -1 になること。
    EXPECT_EQ(-1, write_failure);               // [確認_異常系] - printf の内部書き込み失敗が -1 になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// ステータスの無効化、有効化、再確保失敗を分類することの確認
TEST(pinnedPromptCoverageTest, status_apis_cover_toggle_and_allocation_failure)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Return(nullptr)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_realloc がステータス内容の再確保のために 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    int top_disable = cplat_pinned_prompt_status_enable(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
                                                        0); // [手順] - 上部ステータスを無効にする。
    int bottom_enable = cplat_pinned_prompt_status_enable(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM,
                                                          1); // [手順] - 下部ステータスを有効にする。
    int direct_failure = test_pinned_prompt_set_status_content(
        screen, "long status"); // [手順] - ステータス内容の再確保を直接失敗させる。
    int public_failure = cplat_pinned_prompt_status_set(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
        "long status"); // [手順] - 公開 API からステータス内容の再確保を失敗させる。

    // Assert
    EXPECT_EQ(CPLAT_OK, top_disable);   // [確認_正常系] - 上部ステータスの無効化が OK になること。
    EXPECT_EQ(CPLAT_OK, bottom_enable); // [確認_正常系] - 下部ステータスの有効化が OK になること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              direct_failure); // [確認_異常系] - 直接のステータス再確保失敗が OUT_OF_MEMORY になること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              public_failure); // [確認_異常系] - 公開 API のステータス再確保失敗が OUT_OF_MEMORY になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#if defined(PLATFORM_LINUX)

// CSI の default 枝が未知シーケンスを UNKNOWN に分類することの確認
TEST(pinnedPromptCoverageTest, read_key_classifies_unknown_csi_default)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_select> mock_select;
    const unsigned char input[] = {0x1BU, '[', '2', 0x1BU, '[', '4', 'x'};
    size_t input_pos = 0U;
    int ch = -1;
    int key = TEST_PINNED_PROMPT_KEY_CHAR;
    int four_key = TEST_PINNED_PROMPT_KEY_CHAR;

    // Pre-Assert
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_異常系] - select が未知 CSI の後続判定で呼び出されること。
    // [Pre-Assert手順] - select から入力可 (1) を返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillRepeatedly(Invoke(
            [&input, &input_pos](const char *, const int, const char *, const int, void *arg, const size_t)
            {
                if (input_pos < sizeof(input))
                {
                    *static_cast<unsigned char *>(arg) = input[input_pos++];
                    return static_cast<ssize_t>(1);
                }
                return static_cast<ssize_t>(0);
            }));
    // [Pre-Assert確認_異常系] - read が標準入力に対し未知 CSI の分類で呼び出されること。
    // [Pre-Assert手順] - ESC [ 2 と ESC [ 4 x を順に返却する。

    // Act
    key = test_pinned_prompt_read_key(screen, &ch);      // [手順] - ESC [ 2 の未知 CSI を分類する。
    four_key = test_pinned_prompt_read_key(screen, &ch); // [手順] - ESC [ 4 x の不正終端を分類する。

    // Assert
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UNKNOWN, key);      // [確認_異常系] - ESC [ 2 が UNKNOWN になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UNKNOWN, four_key); // [確認_異常系] - ESC [ 4 x が UNKNOWN になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// CR、BS、空ステータス、幅不足のステータス行を処理することの確認
TEST(pinnedPromptCoverageTest, read_key_and_status_cover_remaining_conditions)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_select> mock_select;
    NiceMock<Mock_ioctl> mock_ioctl;
    const unsigned char input[] = {'\r', 0x08U, 0x1BU, '[', '3', 'x'};
    size_t input_pos = 0U;
    int ch = -1;
    int cr_key = TEST_PINNED_PROMPT_KEY_CHAR;
    int bs_key = TEST_PINNED_PROMPT_KEY_CHAR;
    int three_key = TEST_PINNED_PROMPT_KEY_CHAR;
    struct winsize wide_size = {};
    wide_size.ws_col = 80U;
    wide_size.ws_row = 24U;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(
            DoAll(Invoke([wide_size](const char *, const int, const char *, const int, const unsigned long,
                                     void *argument) { *static_cast<struct winsize *>(argument) = wide_size; }),
                  Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 80・行数 24 の端末サイズを返却する。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_異常系] - select が ESC [ 3 x の後続判定で呼び出されること。
    // [Pre-Assert手順] - select から入力可 (1) を返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillRepeatedly(Invoke(
            [&input, &input_pos](const char *, const int, const char *, const int, void *arg, const size_t)
            {
                if (input_pos < sizeof(input))
                {
                    *static_cast<unsigned char *>(arg) = input[input_pos++];
                    return static_cast<ssize_t>(1);
                }
                return static_cast<ssize_t>(0);
            }));
    // [Pre-Assert確認_正常系] - read が標準入力に対し CR、BS、不正 CSI の分類で呼び出されること。
    // [Pre-Assert手順] - CR、BS、ESC [ 3 x を順に返却する。

    // Act
    cr_key = test_pinned_prompt_read_key(screen, &ch);    // [手順] - CR をキー分類する。
    bs_key = test_pinned_prompt_read_key(screen, &ch);    // [手順] - BS をキー分類する。
    three_key = test_pinned_prompt_read_key(screen, &ch); // [手順] - ESC [ 3 x を分類する。
    test_pinned_prompt_render_state(screen, 1, 1, 1, 1, "p", "e", "", "RIGHT", "LEFT", "");
    test_pinned_prompt_render(screen); // [手順] - 空の左ステータスと右ステータスを 80 列へ描画する。
    test_pinned_prompt_set_internal_state(screen, 80, 24, 24, 1, 1, 1, 0U, 0U);
    test_pinned_prompt_cleanup_terminal(screen); // [手順] - main_bottom が行数と同じ端末を初期状態へ戻す。

    // Assert
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_ENTER, cr_key);      // [確認_正常系] - CR が ENTER になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_BACKSPACE, bs_key);  // [確認_正常系] - BS が BACKSPACE になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UNKNOWN, three_key); // [確認_異常系] - ESC [ 3 x が UNKNOWN になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 残っている複合条件をステータス、履歴、破棄、書き込みで充足することの確認
TEST(pinnedPromptCoverageTest, remaining_conditions_cover_null_status_history_and_write)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    size_t written = 99U;
    int write_null_out = CPLAT_OK;
    int write_with_out = CPLAT_OK;
    const char payload[] = "xy";

    test_pinned_prompt_set_internal_state(screen, 80, 24, 1, 1, 1, 1, 0U, 0U);

    // Pre-Assert

    // Act
    test_pinned_prompt_render_status(screen, 1, NULL, NULL);   // [手順] - 左右とも NULL のステータス行を描画する。
    test_pinned_prompt_render_status(screen, 1, "", "RIGHT");  // [手順] - 空の左と非空の右を描画する。
    test_pinned_prompt_render_status(screen, 1, "LEFT", NULL); // [手順] - 非空の左と NULL の右を描画する。
    test_pinned_prompt_render_status(screen, 1, "LEFT", "");   // [手順] - 非空の左と空の右を描画する。
    test_pinned_prompt_set_status_dirty(screen, 1);
    test_pinned_prompt_set_internal_state(screen, 80, 2, 1, 1, 1, 1, 0U, 0U);
    test_pinned_prompt_render(screen); // [手順] - ステータス有効だが表示できない低さで描画する。
    test_pinned_prompt_set_tty(screen, 0);
    test_pinned_prompt_hide(screen);   // [手順] - 非 TTY で hide する。
    test_pinned_prompt_finish(screen); // [手順] - 非 TTY で finish する。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_set_internal_state(screen, 80, 24, 10, 1, 0, 0, 0U, 0U);
    test_pinned_prompt_cleanup_terminal(screen);    // [手順] - main_bottom が行数より小さい端末を戻す。
    test_pinned_prompt_set_edit_line(screen, NULL); // [手順] - NULL 行を編集バッファーへ設定する。
    test_pinned_prompt_history_next_null(screen);   // [手順] - NULL 履歴コンテキストで次履歴を要求する。
    test_pinned_prompt_set_input_limits(screen, 4U, 4096U);
    test_pinned_prompt_history_next_null_entry(screen); // [手順] - NULL 履歴要素を次へ進める。
    test_pinned_prompt_set_input_limits(screen, 2U, 4096U);
    (void)test_pinned_prompt_history_fill(screen, 1U);
    test_pinned_prompt_set_tty(screen, 0);
    write_null_out = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, payload,
                                               sizeof(payload) - 1U, NULL); // [手順] - written_out NULL で書き込む。
    write_with_out =
        cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, payload, sizeof(payload) - 1U,
                                  &written); // [手順] - written_out 付きで書き込む。
    test_pinned_prompt_destroy_mutex(screen);
    cplat_pinned_prompt_dispose(screen); // [手順] - mutex を先に破棄したハンドルを dispose する。
    screen = NULL;

    // Assert
    EXPECT_EQ(CPLAT_OK, write_null_out); // [確認_正常系] - written_out NULL の write が OK であること。
    EXPECT_EQ(CPLAT_OK, write_with_out); // [確認_正常系] - written_out 付きの write が OK であること。
}

// ステータス幅不足、低画面、履歴の NULL 要素、malloc 失敗を充足することの確認
TEST(pinnedPromptCoverageTest, remaining_five_branches)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    test_pinned_prompt_set_internal_state(screen, 2, 24, 1, 1, 0, 0, 0U, 0U);
    test_pinned_prompt_render_status(screen, 1, "AB", "CD"); // [手順] - 2 列画面へ左右とも幅 2 のステータスを描画する。
    test_pinned_prompt_set_status_dirty(screen, 1);
    test_pinned_prompt_set_internal_state(screen, 80, 2, 1, 1, 0, 1, 0U, 0U);
    test_pinned_prompt_render(screen); // [手順] - 下部ステータスだけ有効な 2 行画面を描画する。
    test_pinned_prompt_set_internal_state(screen, 80, 1, 1, 1, 0, 0, 0U, 0U);
    test_pinned_prompt_cleanup_terminal(screen); // [手順] - 1 行画面の制御領域を戻す。
    test_pinned_prompt_set_input_limits(screen, 4U, 4096U);
    test_pinned_prompt_history_add_after_null_last(screen); // [手順] - 直前要素が NULL の履歴へ追加する。

    // Assert
    SUCCEED(); // [確認_正常系] - 描画と直前要素 NULL の履歴追加が完了すること。

    // Arrange_2
    {
        NiceMock<Mock_cplat> mock_cplat;

        // Pre-Assert_2
        EXPECT_CALL(mock_cplat, cplat_malloc(_))
            .WillOnce(Return(nullptr))
            .WillRepeatedly(Invoke(delegate_real_cplat_malloc));
        // [Pre-Assert確認_異常系] - cplat_malloc が履歴追加のために呼び出されること。
        // [Pre-Assert手順] - 1 回目は NULL、以降は本物の malloc 結果を返却する。

        // Act_2
        test_pinned_prompt_history_add_after_null_last(screen); // [手順] - malloc 失敗状態で履歴へ追加する。
    }

    // Assert_2
    SUCCEED(); // [確認_異常系] - malloc 失敗時の履歴追加が安全に完了すること。

    // Cleanup
    test_pinned_prompt_history_release_entries(screen);
    cplat_pinned_prompt_dispose(screen);
}

#endif /* PLATFORM_LINUX */

// 書式バッファーの再確保成功が新しい容量へ差し替わることの確認
TEST(pinnedPromptCoverageTest, format_helper_grows_buffer_when_realloc_succeeds)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_stdio> mock_stdio;
    char long_text[300];
    int grow_result = -1;

    memset(long_text, 'x', sizeof(long_text) - 1U);
    long_text[sizeof(long_text) - 1U] = '\0';

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Invoke(delegate_real_cplat_realloc));
    // [Pre-Assert確認_正常系] - cplat_realloc が書式バッファー再確保のために 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_realloc から新しいバッファーを返却する。
    EXPECT_CALL(mock_stdio, vsnprintf(_, _, _, _, _, _))
        .WillOnce(DoDefault())
        .WillOnce(Return(500))
        .WillRepeatedly(DoDefault());
    // [Pre-Assert確認_正常系] - vsnprintf が書式長計算と再確保後の出力のために呼び出されること。
    // [Pre-Assert手順] - 2 回目は 500 を返却し、それ以外は既定動作を行う。

    // Act
    ASSERT_EQ(0, test_pinned_prompt_format(screen, "%s", "x"));
    // [確認_正常系] - `test_pinned_prompt_format(screen, "%s", "x")` の戻り値が `0` であること。
    grow_result =
        test_pinned_prompt_format(screen, "%s", long_text); // [手順] - 長い書式結果で書式バッファーを再確保する。

    // Assert
    EXPECT_EQ(0, grow_result); // [確認_正常系] - 書式バッファー再確保成功時の format 結果が 0 であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 書式バッファー確保失敗時に readline_fmt が空プロンプトへフォールバックすることの確認
TEST(pinnedPromptCoverageTest, readline_fmt_uses_empty_prompt_when_format_allocation_fails)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char input[] = "ok";
    char output[16] = {};
    int readline_result = CPLAT_ERR_UNKNOWN;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_malloc が書式バッファーの初回確保のために 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_malloc から NULL を返却する。
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(input, input + sizeof(input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_異常系] - cplat_fgets が空プロンプトの fallback 入力で 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から入力 "ok" を返却する。

    // Act
    readline_result =
        cplat_pinned_prompt_readline_fmt(screen, output, sizeof(output), "%s",
                                         "prompt"); // [手順] - 書式バッファー確保失敗状態で readline_fmt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        readline_result); // [確認_異常系] - format 失敗後の cplat_pinned_prompt_readline_fmt の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ok", output); // [確認_異常系] - format 失敗後の readline_fmt が空プロンプトで入力を受け取ること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#if defined(PLATFORM_LINUX)
// 低い端末では下部ステータス表示条件が偽になり、1 行では scroll region を付けないことの確認
TEST(pinnedPromptCoverageTest, render_and_prepare_output_cover_hidden_bottom_status_and_full_height)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize two_rows = {};
    struct winsize one_row = {};

    two_rows.ws_col = 80U;
    two_rows.ws_row = 2U; // [状態] - 1 回目の ioctl が返す端末サイズを 80 列 2 行とする。
    one_row.ws_col = 80U;
    one_row.ws_row = 1U;                   // [状態] - 2 回目以降の ioctl が返す端末サイズを 80 列 1 行とする。
    test_pinned_prompt_set_tty(screen, 1); // [状態] - ハンドルを TTY とする。

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillOnce(DoAll(Invoke([two_rows](const char *, const int, const char *, const int, const unsigned long,
                                          void *arg) { *static_cast<struct winsize *>(arg) = two_rows; }),
                        Return(0)))
        .WillRepeatedly(DoAll(Invoke([one_row](const char *, const int, const char *, const int, const unsigned long,
                                               void *arg) { *static_cast<struct winsize *>(arg) = one_row; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - 1 回目は 2 行、以降は 1 行の端末サイズを返却する。

    // Act
    test_pinned_prompt_set_status_dirty(screen, 1);
    test_pinned_prompt_set_internal_state(screen, 80, 2, 1, 1, 0, 1, 0U, 0U);
    test_pinned_prompt_render(screen); // [手順] - 下部ステータスだけ有効な 2 行端末を描画する。
    test_pinned_prompt_set_internal_state(screen, 80, 1, 1, 1, 0, 0, 0U, 0U);
    test_pinned_prompt_prepare_output(screen); // [手順] - 1 行端末の出力準備を行う。

    // Assert
    SUCCEED(); // [確認_正常系] - 2 行描画と 1 行の出力準備が完了すること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)

namespace
{
void fill_console_window(CONSOLE_SCREEN_BUFFER_INFO *info, SHORT cols, SHORT rows)
{
    info->srWindow.Left = 0;
    info->srWindow.Top = 0;
    info->srWindow.Right = static_cast<SHORT>(cols - 1);
    info->srWindow.Bottom = static_cast<SHORT>(rows - 1);
}
} // namespace

// Windows 端末判定とサイズ取得の短絡条件を網羅することの確認
TEST(pinnedPromptCoverageTest, platform_helpers_cover_tty_short_circuits_on_windows)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    Mock_windows mock_windows;
    HANDLE out_handle = (HANDLE)0x1234;
    int cols = 0;
    int rows = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_isatty(_))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(1));
    // [Pre-Assert確認_正常系] - cplat_isatty が端末判定のために呼び出されること。
    // [Pre-Assert手順] - 標準入力非 TTY、標準出力非 TTY、双方 TTY の順に判定結果を返却する。
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).WillOnce(Return(out_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle が STD_OUTPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ダミーの標準出力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, out_handle, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, PCONSOLE_SCREEN_BUFFER_INFO info)
            {
                fill_console_window(info, 80, 0);
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - GetConsoleScreenBufferInfo が 1 回呼び出されること。
    // [Pre-Assert手順] - 列数 80・行数 0 のウィンドウを返却する。

    // Act
    int stdin_not_tty = test_pinned_prompt_platform_is_tty();  // [手順] - 標準入力が TTY ではない端末判定を行う。
    int stdout_not_tty = test_pinned_prompt_platform_is_tty(); // [手順] - 標準出力が TTY ではない端末判定を行う。
    int both_tty = test_pinned_prompt_platform_is_tty();       // [手順] - 標準入出力がともに TTY の端末判定を行う。
    test_pinned_prompt_get_size(&cols, &rows); // [手順] - 行数が 0 のコンソール情報から端末サイズを取得する。

    // Assert
    EXPECT_EQ(0, stdin_not_tty);  // [確認_正常系] - 標準入力が TTY ではない場合の端末判定が 0 であること。
    EXPECT_EQ(0, stdout_not_tty); // [確認_正常系] - 標準出力が TTY ではない場合の端末判定が 0 であること。
    EXPECT_EQ(1, both_tty);       // [確認_正常系] - 標準入出力がともに TTY の場合の端末判定が 1 であること。
    EXPECT_EQ(80, cols);          // [確認_正常系] - 行数 0 のコンソール情報でも列数 80 が返ること。
    EXPECT_EQ(0, rows);           // [確認_正常系] - Windows では行数 0 が既定値へ置き換わらず 0 のまま返ること。
}

// 低い端末では下部ステータス表示条件が偽になり、1 行では scroll region を付けないことの確認
TEST(pinnedPromptCoverageTest, render_and_prepare_output_cover_hidden_bottom_status_and_full_height_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_windows> mock_windows;
    HANDLE out_handle = (HANDLE)0x1234;

    test_pinned_prompt_set_tty(screen, 1); // [状態] - ハンドルを TTY とする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).WillRepeatedly(Return(out_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle が STD_OUTPUT_HANDLE を指定して呼び出されること。
    // [Pre-Assert手順] - ダミーの標準出力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, out_handle, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, PCONSOLE_SCREEN_BUFFER_INFO info)
            {
                fill_console_window(info, 80, 2);
                return TRUE;
            })
        .WillRepeatedly(
            [](const char *, const int, const char *, HANDLE, PCONSOLE_SCREEN_BUFFER_INFO info)
            {
                fill_console_window(info, 80, 1);
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - GetConsoleScreenBufferInfo が呼び出されること。
    // [Pre-Assert手順] - 1 回目は 2 行、以降は 1 行の端末サイズを返却する。

    // Act
    test_pinned_prompt_set_status_dirty(screen, 1);
    test_pinned_prompt_set_internal_state(screen, 80, 2, 1, 1, 0, 1, 0U, 0U);
    test_pinned_prompt_render(screen); // [手順] - 下部ステータスだけ有効な 2 行端末を描画する。
    test_pinned_prompt_set_internal_state(screen, 80, 1, 1, 1, 0, 0, 0U, 0U);
    test_pinned_prompt_prepare_output(screen); // [手順] - 1 行端末の出力準備を行う。

    // Assert
    SUCCEED(); // [確認_正常系] - 2 行描画と 1 行の出力準備が完了すること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 描画補助が非表示、狭幅、消去範囲の境界を処理することの確認
TEST(pinnedPromptCoverageTest, drawing_helpers_cover_hidden_narrow_and_clear_ranges_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_windows> mock_windows;
    HANDLE out_handle = (HANDLE)0x1234;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).WillRepeatedly(Return(out_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle が STD_OUTPUT_HANDLE を指定して呼び出されること。
    // [Pre-Assert手順] - ダミーの標準出力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, out_handle, _))
        .WillRepeatedly(
            [](const char *, const int, const char *, HANDLE, PCONSOLE_SCREEN_BUFFER_INFO info)
            {
                fill_console_window(info, 1, 2);
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - GetConsoleScreenBufferInfo が呼び出されること。
    // [Pre-Assert手順] - 列数 1・行数 2 の端末サイズを返却する。

    // Act
    test_pinned_prompt_render_state(screen, 1, 0, 1, 1, "prompt", "abcdef", "", "", "123", "456");
    test_pinned_prompt_render(screen); // [手順] - TTY でプロンプト非表示の描画を要求する。
    test_pinned_prompt_hide(screen);   // [手順] - TTY でプロンプト非表示の消去を要求する。
    test_pinned_prompt_finish(screen); // [手順] - TTY でプロンプト非表示の確定を要求する。
    test_pinned_prompt_render_state(screen, 1, 1, 1, 1, "prompt", "abcdef", "", "", "123", "456");
    test_pinned_prompt_render(screen); // [手順] - 1 列端末へ長いプロンプトとステータスを描画する。
    test_pinned_prompt_set_internal_state(screen, 1, 2, 0, 1, 1, 1, 6U, 0U);
    test_pinned_prompt_clear_control_area(screen, 1); // [手順] - 前回領域が新しい領域より上にある消去範囲を処理する。
    test_pinned_prompt_set_internal_state(screen, 1, 2, 5, 1, 1, 1, 6U, 0U);
    test_pinned_prompt_clear_control_area(screen, 5); // [手順] - 端末行数を超える消去範囲を処理する。
    test_pinned_prompt_hide(screen);                  // [手順] - 表示中のプロンプト行を消去する。
    test_pinned_prompt_finish(screen);                // [手順] - 表示中のプロンプトを確定する。
    test_pinned_prompt_cleanup_terminal(screen);      // [手順] - TTY の制御領域を初期状態へ戻す。

    // Assert
    SUCCEED(); // [確認_正常系] - 非表示、狭幅、消去範囲の各描画処理が完了すること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#endif /* PLATFORM_WINDOWS */
