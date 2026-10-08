#include <testfw.h>
#include <cplat/base/platform.h>
#if defined(PLATFORM_LINUX)
    #include <mock_ioctl.h>
    #include <mock_signal.h>
    #include <mock_termios.h>
    #include <mock_unistd.h>
    #include <sys/mock_select.h>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif
#include <mock_cplat.h>
#include <mock_stdio.h>
#include <cplat/base/result.h>
#include <cplat/prompt/pinned_prompt.h>

#include "pinned_prompt.inject.h"

#include <memory>
#include <string>

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::ReturnArg;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::SetArrayArgument;

// 固定プロンプトのステータス API が不正なハンドルを分類することの確認
TEST(pinnedPromptTest, status_apis_reject_null_screen)
{
    // Arrange

    // Pre-Assert

    // Act
    int enable_result =
        cplat_pinned_prompt_status_enable(NULL, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
                                          1); // [手順] - screen に NULL を渡して上部ステータス領域を有効にする。
    int set_result = cplat_pinned_prompt_status_set(
        NULL, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
        "status"); // [手順] - screen に NULL を渡して上部左側の表示内容を設定する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        enable_result); // [確認_異常系] - cplat_pinned_prompt_status_enable の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        set_result); // [確認_異常系] - cplat_pinned_prompt_status_set の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 固定プロンプトの文字列長ヘルパーが NULL を 0 として扱うことの確認
TEST(pinnedPromptTest, static_text_helpers_cover_empty_and_utf8_cases)
{
    // Arrange
    const char combining[] = "\xCC\x81";
    const char cjk[] = "\xE6\x97\xA5";
    const char hiragana[] = "\xE3\x81\x82";
    const char katakana[] = "\xE3\x82\xA2";
    const char compatibility_ideograph[] = "\xE3\x90\x80";
    const char emoji[] = "\xF0\x9F\x98\x80";
    const char sgr[] = "\x1B[31mX";
    const char invalid_sgr[] = "\x1B[X";

    // Pre-Assert

    // Act
    size_t null_len = test_pinned_prompt_cstr_len(NULL);             // [手順] - NULL の文字列長を取得する。
    size_t ascii_width = test_pinned_prompt_utf8_width("A", 1U, 0U); // [手順] - ASCII の表示幅を取得する。
    size_t combining_width =
        test_pinned_prompt_utf8_width(combining, sizeof(combining) - 1U, 0U); // [手順] - 結合文字の表示幅を取得する。
    size_t cjk_width =
        test_pinned_prompt_utf8_width(cjk, sizeof(cjk) - 1U, 0U); // [手順] - CJK 文字の表示幅を取得する。
    size_t hiragana_width =
        test_pinned_prompt_utf8_width(hiragana, sizeof(hiragana) - 1U, 0U); // [手順] - ひらがなの表示幅を取得する。
    size_t katakana_width =
        test_pinned_prompt_utf8_width(katakana, sizeof(katakana) - 1U, 0U); // [手順] - カタカナの表示幅を取得する。
    size_t compatibility_width = test_pinned_prompt_utf8_width(
        compatibility_ideograph, sizeof(compatibility_ideograph) - 1U, 0U); // [手順] - CJK 拡張文字の表示幅を取得する。
    size_t emoji_width =
        test_pinned_prompt_utf8_width(emoji, sizeof(emoji) - 1U, 0U); // [手順] - 4 バイト文字の表示幅を取得する。
    size_t invalid_width =
        test_pinned_prompt_utf8_width("\xC2", 1U, 0U); // [手順] - 不完全な UTF-8 の表示幅を取得する。
    size_t sgr_len = test_pinned_prompt_ansi_len(sgr, sizeof(sgr) - 1U, 0U); // [手順] - ANSI SGR 長を取得する。
    size_t invalid_sgr_len =
        test_pinned_prompt_ansi_len(invalid_sgr, sizeof(invalid_sgr) - 1U, 0U); // [手順] - 不正 SGR 長を取得する。

    // Assert
    EXPECT_EQ(0U, null_len);            // [確認_正常系] - NULL の文字列長が 0 であること。
    EXPECT_EQ(1U, ascii_width);         // [確認_正常系] - ASCII の表示幅が 1 であること。
    EXPECT_EQ(0U, combining_width);     // [確認_正常系] - 結合文字の表示幅が 0 であること。
    EXPECT_EQ(2U, cjk_width);           // [確認_正常系] - CJK 文字の表示幅が 2 であること。
    EXPECT_EQ(2U, hiragana_width);      // [確認_正常系] - ひらがなの表示幅が 2 であること。
    EXPECT_EQ(2U, katakana_width);      // [確認_正常系] - カタカナの表示幅が 2 であること。
    EXPECT_EQ(2U, compatibility_width); // [確認_正常系] - CJK 拡張文字の表示幅が 2 であること。
    EXPECT_EQ(1U, emoji_width);         // [確認_正常系] - 2FFFF 未満の絵文字の表示幅が 1 であること。
    EXPECT_EQ(1U, invalid_width);       // [確認_正常系] - 不完全な UTF-8 の表示幅が 1 であること。
    EXPECT_EQ(5U, sgr_len);             // [確認_正常系] - ANSI SGR シーケンスの長さが 5 であること。
    EXPECT_EQ(0U, invalid_sgr_len);     // [確認_正常系] - 不正な ANSI シーケンスの長さが 0 であること。
}

// 固定プロンプトの表示幅計算が ANSI と全角文字を処理することの確認
TEST(pinnedPromptTest, static_display_helpers_skip_ansi_and_limit_columns)
{
    // Arrange
    const char text[] = "\x1B[31m日A";
    const size_t text_len = sizeof(text) - 1U;

    // Pre-Assert

    // Act
    size_t visible_one_col =
        test_pinned_prompt_visible_bytes(text, text_len, 0U, 1U); // [手順] - 1 列に収まる可視バイト数を取得する。
    size_t visible_three_cols =
        test_pinned_prompt_visible_bytes(text, text_len, 0U, 3U); // [手順] - 3 列に収まる可視バイト数を取得する。
    size_t display_width = test_pinned_prompt_display_width(text, text_len, 0U,
                                                            text_len); // [手順] - ANSI を含む全範囲の表示幅を取得する。
    size_t clipped_width =
        test_pinned_prompt_display_width(text, text_len, 0U, 2U); // [手順] - ANSI 途中までの範囲の表示幅を取得する。

    // Assert
    EXPECT_EQ(5U, visible_one_col);          // [確認_正常系] - SGR と全角文字が 1 列制限で収まること。
    EXPECT_EQ(text_len, visible_three_cols); // [確認_正常系] - 3 列制限で全内容が収まること。
    EXPECT_EQ(3U, display_width);            // [確認_正常系] - 全角文字と ASCII の幅合計が 3 であること。
    EXPECT_EQ(2U, clipped_width);            // [確認_正常系] - ANSI 途中までの範囲が通常文字 2 列として計算されること。
}

// 非 TTY の readline が cplat_fgets の入力を返すことの確認
TEST(pinnedPromptTest, fallback_readline_strips_newline)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char input[] = "answer";
    char output[32] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(input, input + sizeof(input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_fgets が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から "answer" を返却する。

    // Act
    int result = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                              NULL); // [手順] - 非 TTY の readline を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);              // [確認_正常系] - fallback readline の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("answer", output); // [確認_正常系] - cplat_fgets が返した "answer" が格納されること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 非 TTY の readline が EOF を返すことの確認
TEST(pinnedPromptTest, fallback_readline_reports_eof)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char output[8] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _)).WillOnce(Return(CPLAT_ERR_EOF));
    // [Pre-Assert確認_異常系] - cplat_fgets が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から CPLAT_ERR_EOF を返却する。

    // Act
    int result = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                              "prompt"); // [手順] - EOF を返す非 TTY readline を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF,
              result); // [確認_異常系] - EOF の readline が CPLAT_ERR_EOF を返すこと。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 非 TTY の readline が行の切り詰めをバッファー不足として返すことの確認
TEST(pinnedPromptTest, fallback_readline_reports_buffer_too_small)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char empty[] = "";
    char output[8] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(empty, empty + sizeof(empty)), Return(CPLAT_ERR_BUFFER_TOO_SMALL)));
    // [Pre-Assert確認_異常系] - cplat_fgets が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から CPLAT_ERR_BUFFER_TOO_SMALL と空文字列を返却する。

    // Act
    int actual_ret = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                                  NULL); // [手順] - 行が収まらない非 TTY readline を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);    // [確認_異常系] - 切り詰め時の readline が CPLAT_ERR_BUFFER_TOO_SMALL を返すこと。
    EXPECT_STREQ("", output); // [確認_異常系] - 切り詰め時に出力先が空文字列であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 固定プロンプトのステータス API が上下左右の有効な指定を処理することの確認
TEST(pinnedPromptTest, status_apis_accept_valid_positions_and_alignments)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int top_enable = cplat_pinned_prompt_status_enable(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
                                                       1); // [手順] - 上部ステータスを有効にする。
    int bottom_disable = cplat_pinned_prompt_status_enable(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM,
                                                           0); // [手順] - 下部ステータスを無効にする。
    int top_left = cplat_pinned_prompt_status_set(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
                                                  CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
                                                  "top"); // [手順] - 上部左側の内容を設定する。
    int top_right = cplat_pinned_prompt_status_set(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
                                                   CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT,
                                                   "right"); // [手順] - 上部右側の内容を設定する。
    int bottom_left = cplat_pinned_prompt_status_set(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM,
                                                     CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
                                                     "bottom"); // [手順] - 下部左側の内容を設定する。
    int bottom_right = cplat_pinned_prompt_status_set(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM,
                                                      CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT,
                                                      NULL); // [手順] - 下部右側の内容を NULL で消去する。

    // Assert
    EXPECT_EQ(CPLAT_OK, top_enable);     // [確認_正常系] - 上部有効化の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, bottom_disable); // [確認_正常系] - 下部無効化の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, top_left);       // [確認_正常系] - 上部左側設定の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, top_right);      // [確認_正常系] - 上部右側設定の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, bottom_left);    // [確認_正常系] - 下部左側設定の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, bottom_right);   // [確認_正常系] - NULL による下部右側消去の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 非 TTY の初期値付き readline が、初期値を使わずに cplat_fgets の入力を返すことの確認
TEST(pinnedPromptTest, fallback_readline_with_initial_ignores_initial_text)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char input[] = "piped";
    char output[32] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(input, input + sizeof(input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_fgets が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から "piped" を返却する。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial(
        screen, output, sizeof(output), NULL,
        "initial"); // [手順] - 初期値を指定して非 TTY の readline を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);   // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("piped", output); // [確認_正常系] - 初期値ではなく cplat_fgets の入力が格納されること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 初期値付き readline が、制御文字を含む初期値と不正な引数を入力の前に拒否することの確認
TEST(pinnedPromptTest, readline_with_initial_rejects_invalid_initial_text_and_arguments)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char output[32] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 入力を読み取らないこと。

    // Act
    int actual_ret_control = cplat_pinned_prompt_readline_with_initial(screen, output, sizeof(output), NULL,
                                                                       "a\tb"); // [手順] - タブを含む初期値を指定する。
    int actual_ret_screen = cplat_pinned_prompt_readline_with_initial(NULL, output, sizeof(output), NULL,
                                                                      "a"); // [手順] - ハンドルに NULL を指定する。
    int actual_ret_buf = cplat_pinned_prompt_readline_with_initial(screen, NULL, sizeof(output), NULL,
                                                                   "a"); // [手順] - 出力先に NULL を指定する。
    int actual_ret_size =
        cplat_pinned_prompt_readline_with_initial(screen, output, 0U, NULL, "a"); // [手順] - バイト数に 0 を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_control); // [確認_異常系] - 制御文字を含む初期値が拒否されること。
    EXPECT_STREQ("", output);                                  // [確認_異常系] - 出力先が空文字列であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_screen);  // [確認_異常系] - ハンドル NULL が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_buf);     // [確認_異常系] - 出力先 NULL が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_size);    // [確認_異常系] - バイト数 0 が拒否されること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#if defined(PLATFORM_LINUX)

// 端末サイズ取得が ioctl の成功値とサイズ値を検証することの確認
TEST(pinnedPromptTest, platform_get_size_uses_valid_ioctl_dimensions)
{
    // Arrange
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize valid_size = {};
    valid_size.ws_col = 120U;
    valid_size.ws_row = 40U;
    int valid_cols = 0;
    int valid_rows = 0;
    int fallback_cols = 0;
    int fallback_rows = 0;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillOnce(DoAll(Invoke([valid_size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = valid_size; }),
                        Return(0)))
        .WillOnce(Return(-1));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は列数 120・行数 40 を返却し、2 回目は -1 を返却する。

    // Act
    test_pinned_prompt_get_size(&valid_cols, &valid_rows);       // [手順] - 正常な ioctl から端末サイズを取得する。
    test_pinned_prompt_get_size(&fallback_cols, &fallback_rows); // [手順] - ioctl 失敗時の既定端末サイズを取得する。

    // Assert
    EXPECT_EQ(120, valid_cols);   // [確認_正常系] - ioctl 成功時の列数が 120 であること。
    EXPECT_EQ(40, valid_rows);    // [確認_正常系] - ioctl 成功時の行数が 40 であること。
    EXPECT_EQ(80, fallback_cols); // [確認_異常系] - ioctl 失敗時の列数が 80 であること。
    EXPECT_EQ(24, fallback_rows); // [確認_異常系] - ioctl 失敗時の行数が 24 であること。
}

// 端末サイズの 0 値が既定値へフォールバックすることの確認
TEST(pinnedPromptTest, platform_get_size_rejects_zero_ioctl_dimensions)
{
    // Arrange
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize zero_size = {};
    int cols = 0;
    int rows = 0;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillOnce(DoAll(Invoke([zero_size](const char *, const int, const char *, const int, const unsigned long,
                                           void *arg) { *static_cast<struct winsize *>(arg) = zero_size; }),
                        Return(0)));
    // [Pre-Assert確認_異常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 0・行数 0 の端末サイズを返却する。

    // Act
    test_pinned_prompt_get_size(&cols, &rows); // [手順] - 列数と行数が 0 の ioctl 結果を処理する。

    // Assert
    EXPECT_EQ(80, cols); // [確認_異常系] - 列数 0 が既定値 80 へ置き換わること。
    EXPECT_EQ(24, rows); // [確認_異常系] - 行数 0 が既定値 24 へ置き換わること。
}

// Linux の raw モード移行と復帰が端末 API と SIGWINCH API を呼び出すことの確認
TEST(pinnedPromptTest, platform_raw_mode_enters_and_leaves_once)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_signal> mock_signal;
    struct termios original = {};

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr から元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset から 0 を返却する。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction から 0 を返却する。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - raw モードへ移行する。
    test_pinned_prompt_enter_raw(screen); // [手順] - raw モード移行済みの状態で再度移行する。
    int active_after_enter = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_leave_raw(screen); // [手順] - raw モードから復帰する。
    test_pinned_prompt_leave_raw(screen); // [手順] - raw モード復帰済みの状態で再度復帰する。
    int active_after_leave = test_pinned_prompt_raw_active(screen);

    // Assert
    EXPECT_EQ(1, active_after_enter); // [確認_正常系] - raw モード移行後の状態が有効であること。
    EXPECT_EQ(0, active_after_leave); // [確認_正常系] - raw モード復帰後の状態が無効であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Linux の入力読み取りが EINTR とリサイズ通知を分類することの確認
TEST(pinnedPromptTest, platform_read_char_handles_resize_and_eof)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_unistd> mock_unistd;
    unsigned char character = 'B';
    test_pinned_prompt_set_resize_pending(1);
    errno = EINTR;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(Return(static_cast<ssize_t>(-1)))
        .WillOnce(DoAll(Invoke([character](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = character; }),
                        Return(static_cast<ssize_t>(1))))
        .WillOnce(Return(static_cast<ssize_t>(0)));
    // [Pre-Assert確認_正常系] - read が標準入力に対し 3 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は -1、2 回目は文字 B、3 回目は 0 バイトを返却する。

    // Act
    int resize_result = test_pinned_prompt_read_char(screen); // [手順] - EINTR とリサイズ通知がある状態で入力を読む。
    int character_result = test_pinned_prompt_read_char(screen); // [手順] - 1 バイト入力を読み取る。
    int eof_result = test_pinned_prompt_read_char(screen);       // [手順] - 0 バイト入力を EOF として読み取る。

    // Assert
    EXPECT_EQ(-2, resize_result);                       // [確認_正常系] - リサイズ通知が -2 として返ること。
    EXPECT_EQ(0, test_pinned_prompt_resize_pending());  // [確認_正常系] - 通知状態が消費されること。
    EXPECT_EQ(static_cast<int>('B'), character_result); // [確認_正常系] - 読み取った文字 B が返ること。
    EXPECT_EQ(-1, eof_result);                          // [確認_正常系] - 0 バイト読み取りが EOF として返ること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Linux の non-blocking 入力読み取りが select 結果を処理することの確認
TEST(pinnedPromptTest, platform_read_char_nb_handles_select_timeout)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_sys_select> mock_select;
    NiceMock<Mock_unistd> mock_unistd;
    unsigned char character = 'C';

    // Pre-Assert
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillOnce(Return(0)).WillOnce(Return(1));
    // [Pre-Assert確認_正常系] - select が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目はタイムアウト (0)、2 回目は入力可 (1) を返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(DoAll(Invoke([character](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = character; }),
                        Return(static_cast<ssize_t>(1))));
    // [Pre-Assert確認_正常系] - read が標準入力に対し 1 回呼び出されること。
    // [Pre-Assert手順] - read から文字 C を返却する。

    // Act
    int timeout_result =
        test_pinned_prompt_read_char_nb(screen); // [手順] - select がタイムアウトする状態で入力を読む。
    int character_result =
        test_pinned_prompt_read_char_nb(screen); // [手順] - select が入力可能を返す状態で入力を読む。

    // Assert
    EXPECT_EQ(-1, timeout_result);                      // [確認_正常系] - select の 0 が EOF として返ること。
    EXPECT_EQ(static_cast<int>('C'), character_result); // [確認_正常系] - select 後の入力文字 C が返ること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の readline が通常文字を受け取り Enter で入力を確定することの確認
TEST(pinnedPromptTest, tty_readline_accepts_character_and_enter)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    struct termios original = {};
    struct winsize size = {};
    char output[16] = {};
    unsigned char first = 'a';
    unsigned char second = '\n';
    size.ws_col = 80;
    size.ws_row = 24;

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr から元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset から 0 を返却する。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction から 0 を返却する。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 80・行数 24 の端末サイズを返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(DoAll(Invoke([first](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = first; }),
                        Return(static_cast<ssize_t>(1))))
        .WillOnce(DoAll(Invoke([second](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = second; }),
                        Return(static_cast<ssize_t>(1))));
    // [Pre-Assert確認_正常系] - read が標準入力に対し 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は文字 a、2 回目は改行を返却する。

    // Act
    int result = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                              "prompt> "); // [手順] - TTY の readline へ文字 a と Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);         // [確認_正常系] - TTY の cplat_pinned_prompt_readline が CPLAT_OK を返すこと。
    EXPECT_STREQ("a", output); // [確認_正常系] - 確定した入力が "a" であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の readline が Ctrl-C をキャンセルとして返すことの確認
TEST(pinnedPromptTest, tty_readline_reports_canceled_on_ctrl_c)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    struct termios original = {};
    struct winsize size = {};
    char output[16] = "stale";
    unsigned char cancel = 0x03U;
    size.ws_col = 80;
    size.ws_row = 24;

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_異常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr から元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_異常系] - tcsetattr が標準入力を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - sigemptyset が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset から 0 を返却する。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_異常系] - sigaction が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction から 0 を返却する。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_異常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 80・行数 24 の端末サイズを返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(DoAll(Invoke([cancel](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = cancel; }),
                        Return(static_cast<ssize_t>(1))));
    // [Pre-Assert確認_異常系] - read が標準入力に対し 1 回呼び出されること。
    // [Pre-Assert手順] - read から Ctrl-C を返却する。

    // Act
    int result = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                              "prompt> "); // [手順] - TTY の readline へ Ctrl-C を入力する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CANCELED,
              result); // [確認_異常系] - Ctrl-C の cplat_pinned_prompt_readline が CPLAT_ERR_CANCELED を返すこと。
    EXPECT_STREQ("", output); // [確認_異常系] - キャンセル時の出力が空文字列になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 端末入力の各キー表現が内部キーへ分類されることの確認
TEST(pinnedPromptTest, read_key_classifies_control_and_escape_sequences)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_select> mock_select;
    const unsigned char input[] = {'A', '\n', 0x7FU, 0x01U, 0x80U, 0x1BU, 0x1BU, 'x', 0x1BU, '[', 'A',   0x1BU,
                                   '[', 'B',  0x1BU, '[',   'C',   0x1BU, '[',   'D', 0x1BU, '[', 'H',   0x1BU,
                                   '[', 'F',  0x1BU, '[',   '1',   '~',   0x1BU, '[', '3',   '~', 0x1BU, '[',
                                   '4', '~',  0x1BU, '[',   '1',   'x',   0x1BU, '[', 'Z'};
    size_t input_pos = 0U;
    int ch = -1;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillRepeatedly(Invoke(
            [&input, &input_pos](const char *, const int, const char *, const int, void *arg, const size_t)
            {
                if (input_pos < sizeof(input))
                {
                    *static_cast<unsigned char *>(arg) = input[input_pos++];
                    return static_cast<ssize_t>(1);
                }
                errno = (input_pos == sizeof(input)) ? EINTR : 0;
                input_pos++;
                return (input_pos == sizeof(input) + 1U) ? static_cast<ssize_t>(-1) : static_cast<ssize_t>(0);
            }));
    // [Pre-Assert確認_正常系] - read が標準入力に対しキー分類用の各入力で呼び出されること。
    // [Pre-Assert手順] - 用意したキー列を順に返却し、消費後は EINTR と EOF を返却する。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillOnce(Return(0)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_正常系] - select が単独 ESC 判定と後続シーケンスで呼び出されること。
    // [Pre-Assert手順] - 1 回目はタイムアウト (0)、以降は入力可 (1) を返却する。

    // Act
    int char_key = test_pinned_prompt_read_key(screen, &ch); // [手順] - ASCII 文字をキー分類する。
    int char_value = ch;
    int enter_key = test_pinned_prompt_read_key(screen, &ch);          // [手順] - 改行をキー分類する。
    int backspace_key = test_pinned_prompt_read_key(screen, &ch);      // [手順] - DEL をキー分類する。
    int control_key = test_pinned_prompt_read_key(screen, &ch);        // [手順] - 制御文字をキー分類する。
    int high_bit_key = test_pinned_prompt_read_key(screen, &ch);       // [手順] - 8 ビット文字をキー分類する。
    int clear_key = test_pinned_prompt_read_key(screen, &ch);          // [手順] - 単独の ESC をキー分類する。
    int unknown_escape_key = test_pinned_prompt_read_key(screen, &ch); // [手順] - 未知の ESC 文字列を分類する。
    int up_key = test_pinned_prompt_read_key(screen, &ch);             // [手順] - 上矢印を分類する。
    int down_key = test_pinned_prompt_read_key(screen, &ch);           // [手順] - 下矢印を分類する。
    int right_key = test_pinned_prompt_read_key(screen, &ch);          // [手順] - 右矢印を分類する。
    int left_key = test_pinned_prompt_read_key(screen, &ch);           // [手順] - 左矢印を分類する。
    int home_key = test_pinned_prompt_read_key(screen, &ch);           // [手順] - Home を分類する。
    int end_key = test_pinned_prompt_read_key(screen, &ch);            // [手順] - End を分類する。
    int home_tilde_key = test_pinned_prompt_read_key(screen, &ch);     // [手順] - ESC [ 1 ~ を分類する。
    int delete_key = test_pinned_prompt_read_key(screen, &ch);         // [手順] - ESC [ 3 ~ を分類する。
    int end_tilde_key = test_pinned_prompt_read_key(screen, &ch);      // [手順] - ESC [ 4 ~ を分類する。
    int invalid_tilde_key = test_pinned_prompt_read_key(screen, &ch);  // [手順] - 不正な終端を分類する。
    int invalid_csi_key = test_pinned_prompt_read_key(screen, &ch);    // [手順] - 未知の CSI を分類する。

    test_pinned_prompt_set_resize_pending(1);
    errno = EINTR;
    int resize_key = test_pinned_prompt_read_key(screen, &ch); // [手順] - リサイズ通知をキー分類する。
    int eof_key = test_pinned_prompt_read_key(screen, &ch);    // [手順] - EOF をキー分類する。

    // Assert
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_CHAR, char_key);              // [確認_正常系] - ASCII 文字が CHAR になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_ENTER, enter_key);            // [確認_正常系] - 改行が ENTER になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_BACKSPACE, backspace_key);    // [確認_正常系] - DEL が BACKSPACE になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UNKNOWN, control_key);        // [確認_異常系] - 制御文字が UNKNOWN になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_CHAR, high_bit_key);          // [確認_正常系] - 8 ビット文字が CHAR になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_CLEAR, clear_key);            // [確認_正常系] - 単独 ESC が CLEAR になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UNKNOWN, unknown_escape_key); // [確認_異常系] - 未知 ESC が UNKNOWN になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UP, up_key);                  // [確認_正常系] - 上矢印が UP になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_DOWN, down_key);              // [確認_正常系] - 下矢印が DOWN になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_RIGHT, right_key);            // [確認_正常系] - 右矢印が RIGHT になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_LEFT, left_key);              // [確認_正常系] - 左矢印が LEFT になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_HOME, home_key);              // [確認_正常系] - Home が HOME になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_END, end_key);                // [確認_正常系] - End が END になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_HOME, home_tilde_key);        // [確認_正常系] - ESC [ 1 ~ が HOME になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_DELETE, delete_key);          // [確認_正常系] - ESC [ 3 ~ が DELETE になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_END, end_tilde_key);          // [確認_正常系] - ESC [ 4 ~ が END になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UNKNOWN, invalid_tilde_key);  // [確認_異常系] - 不正終端が UNKNOWN になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_UNKNOWN, invalid_csi_key);    // [確認_異常系] - 未知 CSI が UNKNOWN になること。
    EXPECT_EQ(char_value, static_cast<int>('A'));                  // [確認_正常系] - CHAR の値が A になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_RESIZE, resize_key);          // [確認_正常系] - リサイズ通知が RESIZE になること。
    EXPECT_EQ(TEST_PINNED_PROMPT_KEY_EOF, eof_key);                // [確認_正常系] - EOF が EOF になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 行編集 static 関数が UTF-8 境界とカーソルエンドポイントを処理することの確認
TEST(pinnedPromptTest, edit_helpers_update_line_at_boundaries)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    test_pinned_prompt_set_edit_line(screen, "ab");           // [手順] - ASCII の編集行を設定する。
    test_pinned_prompt_insert_byte(screen, 'X');              // [手順] - 編集行末尾へ X を挿入する。
    test_pinned_prompt_backspace(screen);                     // [手順] - 編集行末尾の X を削除する。
    test_pinned_prompt_set_edit_line(screen, "ab");           // [手順] - 削除対象を持つ編集行を設定する。
    test_pinned_prompt_set_cursor(screen, 1U);                // [手順] - カーソルを b の直前へ移動する。
    test_pinned_prompt_delete(screen);                        // [手順] - カーソル位置の b を削除する。
    test_pinned_prompt_backspace(screen);                     // [手順] - カーソル先頭で backspace を実行する。
    test_pinned_prompt_set_edit_line(screen, "\xE6\x97\xA5"); // [手順] - UTF-8 の編集行を設定する。
    test_pinned_prompt_backspace(screen);                     // [手順] - UTF-8 文字を境界単位で削除する。
    test_pinned_prompt_delete(screen);                        // [手順] - 編集行末尾で delete を実行する。

    // Assert
    EXPECT_EQ(0U, test_pinned_prompt_edit_length(screen));  // [確認_正常系] - 編集行の長さが 0 になること。
    EXPECT_STREQ("", test_pinned_prompt_edit_text(screen)); // [確認_正常系] - 編集行が空になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

    #if defined(PLATFORM_LINUX)

// TTY 描画がステータス領域、区切り線、入力カーソルを描画することの確認
TEST(pinnedPromptTest, render_handles_status_regions_and_empty_layout)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize full_size = {};
    full_size.ws_col = 24U;
    full_size.ws_row = 8U;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([full_size](const char *, const int, const char *, const int, const unsigned long,
                                                 void *arg) { *static_cast<struct winsize *>(arg) = full_size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 24・行数 8 の端末サイズを返却する。

    // Act
    test_pinned_prompt_render_state(screen, 1, 1, 1, 1, "P> ", "abc", "TOP", "RIGHT", "BOTTOM",
                                    "BR"); // [手順] - 上下ステータスと入力行を描画する。
    test_pinned_prompt_render(screen);     // [手順] - ステータス領域を含む TTY 描画を実行する。
    test_pinned_prompt_render(screen);     // [手順] - status_dirty が解除された状態で再描画する。
    test_pinned_prompt_render_state(screen, 1, 1, 1, 1, "", "", NULL, NULL, NULL,
                                    NULL); // [手順] - 空のプロンプトとステータスを描画する。
    test_pinned_prompt_render(screen);     // [手順] - 空状態の描画を実行する。

    // Assert
    EXPECT_STREQ("", test_pinned_prompt_edit_text(screen)); // [確認_正常系] - 空状態の編集行が空文字列であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の編集操作と同一呼び出し元の履歴参照が入力結果へ反映されることの確認
TEST(pinnedPromptTest, tty_readline_handles_editing_and_history)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_select> mock_select;
    struct termios original = {};
    struct winsize size = {};
    const unsigned char input[] = {'a',   'b', 0x1BU, '[',   'D', 0x7FU, 'c',   0x1BU, '[', '3',   '~', '\n', 'd', '\n',
                                   0x1BU, '[', 'A',   0x1BU, '[', 'A',   0x1BU, '[',   'B', 0x1BU, '[', 'B',  '\n'};
    size_t input_pos = 0U;
    char first_output[16] = {};
    char second_output[16] = {};
    char third_output[16] = {};
    size.ws_col = 80U;
    size.ws_row = 24U;

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .Times(3)
        .WillRepeatedly(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr から元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(6).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 6 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).Times(3).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset が 3 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset から 0 を返却する。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(6).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction が SIGWINCH を指定して 6 回呼び出されること。
    // [Pre-Assert手順] - sigaction から 0 を返却する。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 80・行数 24 の端末サイズを返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillRepeatedly(Invoke(
            [&input, &input_pos](const char *, const int, const char *, const int, void *arg, const size_t)
            {
                if (input_pos >= sizeof(input))
                {
                    return static_cast<ssize_t>(0);
                }
                *static_cast<unsigned char *>(arg) = input[input_pos++];
                return static_cast<ssize_t>(1);
            }));
    // [Pre-Assert確認_正常系] - read が標準入力に対し編集キーと履歴キーの入力で呼び出されること。
    // [Pre-Assert手順] - 用意したキー列を順に返却する。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_正常系] - select がエスケープシーケンスの後続判定で呼び出されること。
    // [Pre-Assert手順] - select から入力可 (1) を返却する。

    // Act
    int first_result = cplat_pinned_prompt_readline_at(screen, first_output, sizeof(first_output), "", "history.c",
                                                       10); // [手順] - 編集キーを含む最初の入力を確定する。
    int second_result = cplat_pinned_prompt_readline_at(screen, second_output, sizeof(second_output), "", "history.c",
                                                        10); // [手順] - 2 件目の入力を同じ履歴へ追加する。
    int third_result = cplat_pinned_prompt_readline_at(screen, third_output, sizeof(third_output), "", "history.c",
                                                       10); // [手順] - 上下キーで履歴を参照して入力を確定する。

    // Assert
    EXPECT_EQ(CPLAT_OK, first_result);  // [確認_正常系] - 1 件目の readline が CPLAT_OK を返すこと。
    EXPECT_STREQ("c", first_output);    // [確認_正常系] - 編集後の 1 件目の入力が c であること。
    EXPECT_EQ(CPLAT_OK, second_result); // [確認_正常系] - 2 件目の readline が CPLAT_OK を返すこと。
    EXPECT_STREQ("d", second_output);   // [確認_正常系] - 2 件目の入力が d であること。
    EXPECT_EQ(CPLAT_OK, third_result);  // [確認_正常系] - 履歴参照後の readline が CPLAT_OK を返すこと。
    EXPECT_STREQ("", third_output);     // [確認_正常系] - 履歴の末尾から下へ移動すると保存行へ戻ること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY のリサイズ通知が readline の再描画後に入力を継続することの確認
TEST(pinnedPromptTest, tty_readline_continues_after_resize)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    struct termios original = {};
    struct winsize size = {};
    char output[8] = {};
    int read_count = 0;
    size.ws_col = 80U;
    size.ws_row = 24U;
    test_pinned_prompt_set_resize_pending(1);

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr から元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset から 0 を返却する。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction から 0 を返却する。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 80・行数 24 の端末サイズを返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillRepeatedly(Invoke(
            [&read_count](const char *, const int, const char *, const int, void *arg, const size_t)
            {
                if (read_count++ == 0)
                {
                    errno = EINTR;
                    return static_cast<ssize_t>(-1);
                }
                *static_cast<unsigned char *>(arg) = '\n';
                return static_cast<ssize_t>(1);
            }));
    // [Pre-Assert確認_正常系] - read が標準入力に対しリサイズ後の再読取りで呼び出されること。
    // [Pre-Assert手順] - 1 回目は EINTR、以降は改行を返却する。

    // Act
    int result = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                              ""); // [手順] - リサイズ通知後に Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - リサイズ後の readline が CPLAT_OK を返すこと。
    EXPECT_STREQ("", output);    // [確認_正常系] - リサイズ後に確定した入力が空であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

    #endif /* PLATFORM_LINUX */

// 固定プロンプトの文字幅と ANSI 解析が境界値を処理することの確認
TEST(pinnedPromptTest, static_text_helpers_cover_boundary_sequences)
{
    // Arrange
    const char two_byte[] = "\xC3\xA9";
    const char three_byte[] = "\xE2\x82\xAC";
    const char astral_wide[] = "\xF0\xB0\x80\x80";
    const char truncated_three[] = "\xE3\x81";
    const char truncated_four[] = "\xF0\x9F\x98";
    const char valid_sgr[] = "\x1B[1;31mX";
    const char invalid_sgr[] = "\x1B[1x";
    const char incomplete_sgr[] = "\x1B[1";

    // Pre-Assert

    // Act
    size_t at_end = test_pinned_prompt_utf8_width("A", 1U, 1U); // [手順] - 文字列末尾の表示幅を取得する。
    size_t two_byte_width =
        test_pinned_prompt_utf8_width(two_byte, sizeof(two_byte) - 1U, 0U); // [手順] - 2 バイト文字の表示幅を取得する。
    size_t three_byte_width = test_pinned_prompt_utf8_width(three_byte, sizeof(three_byte) - 1U,
                                                            0U); // [手順] - CJK 以外の 3 バイト文字の表示幅を取得する。
    size_t astral_wide_width = test_pinned_prompt_utf8_width(
        astral_wide, sizeof(astral_wide) - 1U, 0U); // [手順] - 2FFFF を超える 4 バイト文字の表示幅を取得する。
    size_t truncated_three_width = test_pinned_prompt_utf8_width(
        truncated_three, sizeof(truncated_three) - 1U, 0U); // [手順] - 不完全な 3 バイト文字の表示幅を取得する。
    size_t truncated_four_width = test_pinned_prompt_utf8_width(
        truncated_four, sizeof(truncated_four) - 1U, 0U); // [手順] - 不完全な 4 バイト文字の表示幅を取得する。
    size_t valid_sgr_len = test_pinned_prompt_ansi_len(valid_sgr, sizeof(valid_sgr) - 1U,
                                                       0U); // [手順] - パラメーター付き SGR の長さを取得する。
    size_t invalid_sgr_len = test_pinned_prompt_ansi_len(invalid_sgr, sizeof(invalid_sgr) - 1U,
                                                         0U); // [手順] - 不正終端 SGR の長さを取得する。
    size_t incomplete_sgr_len = test_pinned_prompt_ansi_len(incomplete_sgr, sizeof(incomplete_sgr) - 1U,
                                                            0U); // [手順] - 未完了 SGR の長さを取得する。
    size_t zero_visible =
        test_pinned_prompt_visible_bytes("A", 1U, 0U, 0U); // [手順] - 表示列数 0 の可視バイト数を取得する。
    size_t clipped_display =
        test_pinned_prompt_display_width("ABC", 3U, 1U, 8U); // [手順] - 終端を超える表示幅を取得する。

    // Assert
    EXPECT_EQ(0U, at_end);                // [確認_正常系] - 末尾位置の UTF-8 表示幅が 0 であること。
    EXPECT_EQ(1U, two_byte_width);        // [確認_正常系] - 2 バイト文字の表示幅が 1 であること。
    EXPECT_EQ(1U, three_byte_width);      // [確認_正常系] - CJK 以外の 3 バイト文字の表示幅が 1 であること。
    EXPECT_EQ(1U, astral_wide_width);     // [確認_正常系] - 2FFFF を超える 4 バイト文字の表示幅が 1 であること。
    EXPECT_EQ(1U, truncated_three_width); // [確認_異常系] - 不完全な 3 バイト文字の表示幅が 1 であること。
    EXPECT_EQ(1U, truncated_four_width);  // [確認_異常系] - 不完全な 4 バイト文字の表示幅が 1 であること。
    EXPECT_EQ(7U, valid_sgr_len);         // [確認_正常系] - パラメーター付き SGR の長さが 7 であること。
    EXPECT_EQ(0U, invalid_sgr_len);       // [確認_異常系] - 不正終端 SGR の長さが 0 であること。
    EXPECT_EQ(0U, incomplete_sgr_len);    // [確認_異常系] - 未完了 SGR の長さが 0 であること。
    EXPECT_EQ(0U, zero_visible);          // [確認_正常系] - 列数 0 の可視バイト数が 0 であること。
    EXPECT_EQ(2U, clipped_display);       // [確認_正常系] - 範囲を終端で切った表示幅が 2 であること。
}

// 固定プロンプトの履歴補助関数が空状態と重複入力を処理することの確認
TEST(pinnedPromptTest, history_helpers_cover_empty_and_duplicate_entries)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    test_pinned_prompt_history_edge_cases(screen); // [手順] - 空履歴、重複履歴、履歴移動の境界を処理する。

    // Assert
    EXPECT_STREQ("same",
                 test_pinned_prompt_edit_text(screen)); // [確認_正常系] - 履歴から復元した入力が "same" であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

    #if defined(PLATFORM_LINUX)

// 固定プロンプトの Linux raw モードが失敗と再入を処理することの確認
TEST(pinnedPromptTest, platform_raw_mode_handles_failures_and_reentry)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_signal> mock_signal;
    struct termios original = {};

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(Return(-1))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_異常系] - tcgetattr が標準入力を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は -1 を返却し、以降は元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).WillOnce(Return(-1)).WillRepeatedly(Return(0));
    // [Pre-Assert確認_異常系] - tcsetattr が標準入力を指定して呼び出されること。
    // [Pre-Assert手順] - 1 回目は -1 を返却し、以降は 0 を返却する。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset から 0 を返却する。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction から 0 を返却する。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - tcgetattr 失敗を含む raw モード移行を行う。
    int after_getattr_failure =
        test_pinned_prompt_raw_active(screen); // [手順] - tcgetattr 失敗後の raw 状態を取得する。
    test_pinned_prompt_enter_raw(screen);      // [手順] - tcsetattr 失敗を含む raw モード移行を行う。
    int after_setattr_failure =
        test_pinned_prompt_raw_active(screen);               // [手順] - tcsetattr 失敗後の raw 状態を取得する。
    test_pinned_prompt_enter_raw(screen);                    // [手順] - raw モードを正常に開始する。
    int after_enter = test_pinned_prompt_raw_active(screen); // [手順] - raw モード開始後の状態を取得する。
    test_pinned_prompt_enter_raw(screen);                    // [手順] - raw モード中に再度開始する。
    test_pinned_prompt_leave_raw(screen);                    // [手順] - raw モードを解除する。
    int after_leave = test_pinned_prompt_raw_active(screen); // [手順] - raw モード解除後の状態を取得する。
    test_pinned_prompt_leave_raw(screen);                    // [手順] - raw モード解除済みの状態で再度解除する。

    // Assert
    EXPECT_EQ(0, after_getattr_failure); // [確認_異常系] - tcgetattr 失敗後の raw 状態が無効であること。
    EXPECT_EQ(0, after_setattr_failure); // [確認_異常系] - tcsetattr 失敗後の raw 状態が無効であること。
    EXPECT_EQ(1, after_enter);           // [確認_正常系] - raw モード開始後の状態が有効であること。
    EXPECT_EQ(0, after_leave);           // [確認_正常系] - raw モード解除後の状態が無効であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 固定プロンプトの Linux 入力が EINTR、非 EINTR、select 結果を分類することの確認
TEST(pinnedPromptTest, platform_read_helpers_handle_interrupt_and_select_results)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_select> mock_select;
    int read_count = 0;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(Invoke(
            [&read_count](const char *, const int, const char *, const int, void *, const size_t)
            {
                read_count++;
                errno = EINTR;
                return static_cast<ssize_t>(-1);
            }))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, void *, const size_t)
            {
                errno = EAGAIN;
                return static_cast<ssize_t>(-1);
            }))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, void *arg, const size_t)
            {
                *static_cast<unsigned char *>(arg) = static_cast<unsigned char>('Q');
                return static_cast<ssize_t>(1);
            }));
    // [Pre-Assert確認_異常系] - read が標準入力に対し 3 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は EINTR、2 回目は EAGAIN、3 回目は文字 Q を返却する。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillOnce(Return(0)).WillOnce(Return(1));
    // [Pre-Assert確認_異常系] - select が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目はタイムアウト (0)、2 回目は入力可 (1) を返却する。

    // Act
    int non_eintr_result = test_pinned_prompt_read_char(screen); // [手順] - EINTR 後に非 EINTR read 失敗を処理する。
    int select_timeout_result = test_pinned_prompt_read_char_nb(screen); // [手順] - select のタイムアウトを処理する。
    int select_read_result = test_pinned_prompt_read_char_nb(screen);    // [手順] - select 成功後に 1 文字を読み取る。

    // Assert
    EXPECT_EQ(-1, non_eintr_result);                      // [確認_異常系] - 非 EINTR read 失敗の結果が -1 であること。
    EXPECT_EQ(-1, select_timeout_result);                 // [確認_異常系] - select タイムアウトの結果が -1 であること。
    EXPECT_EQ(static_cast<int>('Q'), select_read_result); // [確認_正常系] - select 成功後の入力文字が Q であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 固定プロンプトの出力準備が非 TTY と表示中 TTY を処理することの確認
TEST(pinnedPromptTest, prepare_output_handles_tty_visibility)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize size = {};
    size.ws_col = 40U;
    size.ws_row = 8U;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl が STDOUT_FILENO と TIOCGWINSZ を指定して呼び出されること。
    // [Pre-Assert手順] - ioctl から列数 40・行数 8 の端末サイズを返却する。

    // Act
    test_pinned_prompt_set_tty(screen, 0);     // [手順] - 非 TTY 状態へ変更する。
    test_pinned_prompt_prepare_output(screen); // [手順] - 非 TTY の出力準備を行う。
    test_pinned_prompt_render_state(screen, 1, 1, 0, 0, "P> ", "text", NULL, NULL, NULL,
                                    NULL);     // [手順] - 表示中の TTY 状態を用意する。
    test_pinned_prompt_prepare_output(screen); // [手順] - 表示中 TTY の出力準備を行う。

    // Assert
    SUCCEED(); // [確認_正常系] - 非 TTY と表示中 TTY の出力準備が完了すること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

    #endif /* PLATFORM_LINUX */

// 書き込み API が引数、ストリーム、短い書き込みを分類することの確認
TEST(pinnedPromptTest, write_and_printf_handle_arguments_and_short_write)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_stdio> mock_stdio;
    const char data[] = "abc";
    size_t written = 99U;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, _, _, _)).WillOnce(Return(2U)).WillRepeatedly(ReturnArg<5>());
    // [Pre-Assert確認_異常系] - fwrite が短い書き込みと全量書き込みで呼び出されること。
    // [Pre-Assert手順] - 1 回目は 2 バイト、以降は要求サイズを返却する。

    // Act
    int invalid_screen = cplat_pinned_prompt_write(NULL, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
                                                   &written); // [手順] - NULL ハンドルで書き込む。
    int invalid_data = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, NULL, 1U,
                                                 &written); // [手順] - NULL データを正のサイズで書き込む。
    int short_result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
                                                 &written); // [手順] - 標準出力へ短い書き込みを行う。
    int stdout_result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
                                                  &written); // [手順] - 標準出力へ全量を書き込む。
    int stderr_result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR, data, 3U,
                                                  &written); // [手順] - 標準エラーへ全量を書き込む。
    int empty_result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, NULL, 0U,
                                                 &written); // [手順] - NULL データをサイズ 0 で書き込む。
    int printf_result = cplat_pinned_prompt_printf(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s-%d", "value",
                                                   7); // [手順] - 書式付き文字列を書き込む。
    int null_fmt_result = cplat_pinned_prompt_printf(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
                                                     NULL); // [手順] - NULL 書式で空文字列を書き込む。
    int null_printf_result = cplat_pinned_prompt_printf(NULL, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
                                                        "x"); // [手順] - NULL ハンドルで書式付き書き込みを行う。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_screen); // [確認_異常系] - NULL ハンドルの write が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_data);                    // [確認_異常系] - NULL データの write が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, short_result); // [確認_異常系] - 短い write が UNKNOWN になること。
    EXPECT_EQ(CPLAT_OK, stdout_result);         // [確認_正常系] - stdout への全量 write が OK になること。
    EXPECT_EQ(CPLAT_OK, stderr_result);         // [確認_正常系] - stderr への全量 write が OK になること。
    EXPECT_EQ(CPLAT_OK, empty_result);          // [確認_正常系] - サイズ 0 の write が OK になること。
    EXPECT_EQ(7, printf_result);                // [確認_正常系] - printf が書き込んだ 7 バイトを返すこと。
    EXPECT_EQ(0, null_fmt_result);              // [確認_正常系] - NULL 書式の printf が 0 を返すこと。
    EXPECT_EQ(-1, null_printf_result);          // [確認_異常系] - NULL ハンドルの printf が -1 を返すこと。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 書式付き readline がプロンプトを生成して fallback 入力へ渡すことの確認
TEST(pinnedPromptTest, readline_fmt_formats_and_accepts_null_format)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char first_input[] = "first";
    char second_input[] = "second";
    char first_output[16] = {};
    char second_output[16] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(first_input, first_input + sizeof(first_input)), Return(CPLAT_OK)))
        .WillOnce(DoAll(SetArrayArgument<0>(second_input, second_input + sizeof(second_input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_fgets が書式付き readline と NULL 書式の各経路で呼び出されること。
    // [Pre-Assert手順] - 1 回目は "first"、2 回目は "second" を返却する。

    // Act
    int formatted_result = cplat_pinned_prompt_readline_fmt(screen, first_output, sizeof(first_output), "%s-%d", "p",
                                                            3); // [手順] - 書式付き readline を呼び出す。
    int null_format_result = cplat_pinned_prompt_readline_fmt(screen, second_output, sizeof(second_output),
                                                              NULL); // [手順] - NULL 書式の readline を呼び出す。
    int null_screen_result =
        cplat_pinned_prompt_readline_fmt_at(NULL, second_output, sizeof(second_output), "file", 1, "%s",
                                            "x"); // [手順] - NULL ハンドルの書式付き readline を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, formatted_result);   // [確認_正常系] - 書式付き readline が OK を返すこと。
    EXPECT_STREQ("first", first_output);     // [確認_正常系] - 書式付き readline の入力が first になること。
    EXPECT_EQ(CPLAT_OK, null_format_result); // [確認_正常系] - NULL 書式の readline が OK を返すこと。
    EXPECT_STREQ("second", second_output);   // [確認_正常系] - NULL 書式の入力が second になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_screen_result); // [確認_異常系] - NULL ハンドルの readline_fmt が INVALID_ARGUMENT になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// ステータス API が不正な位置と配置を拒否することの確認
TEST(pinnedPromptTest, status_apis_reject_invalid_position_and_alignment)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    int invalid_position_value = 99;
    int invalid_align_value = 99;
    const cplat_pinned_prompt_status_position invalid_position =
        static_cast<cplat_pinned_prompt_status_position>(invalid_position_value);
    const cplat_pinned_prompt_status_align invalid_align =
        static_cast<cplat_pinned_prompt_status_align>(invalid_align_value);

    // Pre-Assert

    // Act
    int invalid_enable =
        cplat_pinned_prompt_status_enable(screen, invalid_position, 1); // [手順] - 不正な位置を有効化する。
    int invalid_top_align =
        cplat_pinned_prompt_status_set(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, invalid_align,
                                       "x"); // [手順] - 上部へ不正な配置を設定する。
    int invalid_bottom_align =
        cplat_pinned_prompt_status_set(screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, invalid_align,
                                       "x"); // [手順] - 下部へ不正な配置を設定する。
    int invalid_set_position =
        cplat_pinned_prompt_status_set(screen, invalid_position, CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
                                       "x"); // [手順] - 不正な位置へ内容を設定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_enable); // [確認_異常系] - 不正位置の status_enable が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_top_align); // [確認_異常系] - 上部の不正配置が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_bottom_align); // [確認_異常系] - 下部の不正配置が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              invalid_set_position); // [確認_異常系] - 不正位置の status_set が INVALID_ARGUMENT になること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

/**
 *  @brief          TTY の readline へ、指定したバイト列を 1 バイトずつ入力するモックを設定します。
 *
 *  端末設定、SIGWINCH、端末サイズの取得は成功として扱います。
 */
// [サブ手順 名前=pinnedPromptTest.expect_tty_input]
static void expect_tty_input(Mock_ioctl &mock_ioctl, Mock_signal &mock_signal, Mock_termios &mock_termios,
                             Mock_unistd &mock_unistd, const std::string &bytes)
{
    struct termios original = {};
    struct winsize size = {};
    auto position = std::make_shared<size_t>(0U);

    size.ws_col = 80;
    size.ws_row = 24;
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert手順] - 端末設定の取得に成功し、元の設定を返す。
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO の端末設定を取得する引数で 1 回呼び出されること。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).WillRepeatedly(Return(0));
    // [Pre-Assert手順] - 端末設定の変更に成功する。
    // [Pre-Assert確認_正常系] - 端末設定の変更で tcsetattr() に STDIN_FILENO が渡されること。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillRepeatedly(Return(0));
    // [Pre-Assert手順] - シグナル集合の初期化に成功する。
    // [Pre-Assert確認_正常系] - シグナル集合の初期化で sigemptyset() が呼び出されること。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).WillRepeatedly(Return(0));
    // [Pre-Assert手順] - SIGWINCH のハンドラー登録に成功する。
    // [Pre-Assert確認_正常系] - sigaction() が SIGWINCH の設定を変更する引数で呼び出されること。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert手順] - 端末サイズとして 80 列、24 行を返す。
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO と TIOCGWINSZ を引数に端末サイズを取得すること。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillRepeatedly(Invoke(
            [bytes, position](const char *, const int, const char *, const int, void *arg, const size_t) -> ssize_t
            {
                if (*position >= bytes.size())
                {
                    return 0; /* 入力の終端 */
                }
                *static_cast<unsigned char *>(arg) = static_cast<unsigned char>(bytes[*position]);
                (*position)++;
                return 1;
            }));
    // [Pre-Assert手順] - 指定した入力列を 1 バイトずつ返し、終端では 0 を返す。
    // [Pre-Assert確認_正常系] - 端末入力を読む read() に STDIN_FILENO が渡されること。
}
// [サブ手順終了]

// TTY の初期値付き readline で、初期値のまま Enter を押すと初期値が返ることの確認
TEST(pinnedPromptTest, tty_readline_with_initial_returns_initial_text_on_enter)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    char output[32] = {};

    // Pre-Assert
    // [サブ手順参照 名前=pinnedPromptTest.expect_tty_input]
    expect_tty_input(mock_ioctl, mock_signal, mock_termios, mock_unistd, "\n");
    // 端末設定、シグナル、端末サイズ、入力の 6 つの呼び出し期待を満たすこと。
    // [Pre-Assert手順] - 端末操作を成功させ、入力として改行だけを返却する。

    // Act
    int result =
        cplat_pinned_prompt_readline_with_initial(screen, output, sizeof(output), "prompt> ",
                                                  "edit 1 abc"); // [手順] - 初期値を指定して Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);        // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("edit 1 abc", output); // [確認_正常系] - 初期値がそのまま確定すること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の初期値付き readline で、末尾のカーソルから初期値を編集できることの確認
TEST(pinnedPromptTest, tty_readline_with_initial_allows_editing_from_end)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    char output[32] = {};

    // Pre-Assert
    // [サブ手順参照 名前=pinnedPromptTest.expect_tty_input]
    expect_tty_input(mock_ioctl, mock_signal, mock_termios, mock_unistd, "\x7F-3\n");
    // 端末設定、シグナル、端末サイズ、入力の 6 つの呼び出し期待を満たすこと。
    // [Pre-Assert手順] - 端末操作を成功させ、Backspace、"-3"、改行を返却する。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial(screen, output, sizeof(output), "prompt> ",
                                                           "arg.priority < 0"); // [手順] - 初期値を編集して確定する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);               // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("arg.priority < -3", output); // [確認_正常系] - 末尾の 0 が削除され、-3 が追加されること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の初期値付き readline で、履歴をさかのぼって戻ると初期値へ戻ることの確認
TEST(pinnedPromptTest, tty_readline_with_initial_restores_initial_text_after_history)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    char first_output[32] = {};
    char output[32] = {};

    {
        NiceMock<Mock_ioctl> mock_ioctl;
        NiceMock<Mock_signal> mock_signal;
        NiceMock<Mock_termios> mock_termios;
        NiceMock<Mock_unistd> mock_unistd;

        // [サブ手順参照 名前=pinnedPromptTest.expect_tty_input]
        expect_tty_input(mock_ioctl, mock_signal, mock_termios, mock_unistd, "first\n");
        // 端末設定、シグナル、端末サイズ、入力の 6 つの呼び出し期待を満たすこと。
        ASSERT_EQ(CPLAT_OK, cplat_pinned_prompt_readline_at(screen, first_output, sizeof(first_output), "", "history.c",
                                                            1)); // [状態] - "first" を履歴へ登録する。
        // [状態確認] - readline の戻り値が CPLAT_OK であること。
    }
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_select> mock_select;

    // Pre-Assert
    // [サブ手順参照 名前=pinnedPromptTest.expect_tty_input]
    expect_tty_input(mock_ioctl, mock_signal, mock_termios, mock_unistd, "\x1B[A\x1B[B\n");
    // 端末設定、シグナル、端末サイズ、入力の 6 つの呼び出し期待を満たすこと。
    // [Pre-Assert手順] - 端末操作を成功させ、上矢印、下矢印、改行を返却する。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_正常系] - select がエスケープシーケンスの後続判定で呼び出されること。
    // [Pre-Assert手順] - select から入力可 (1) を返却し、ESC の後続を同じキー入力として読ませる。

    // Act
    int result =
        cplat_pinned_prompt_readline_with_initial_at(screen, output, sizeof(output), "", "initial", "history.c",
                                                     1); // [手順] - 同じ呼び出し元の履歴で初期値付きの入力を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);     // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("initial", output); // [確認_正常系] - 退避した初期値へ戻ること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の初期値付き readline で、初期値のための編集バッファーの拡張に失敗した場合にメモリ不足を返すことの確認
TEST(pinnedPromptTest, tty_readline_with_initial_reports_out_of_memory_when_edit_buffer_expansion_fails)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_reset_platform_state();
    char first_output[32] = {};
    char output[32] = "stale";
    std::string long_initial(300U, 'i'); // [状態] - 編集バッファーの初期容量を超える長さの初期値を用意する。

    {
        NiceMock<Mock_ioctl> mock_ioctl;
        NiceMock<Mock_signal> mock_signal;
        NiceMock<Mock_termios> mock_termios;
        NiceMock<Mock_unistd> mock_unistd;

        // [サブ手順参照 名前=pinnedPromptTest.expect_tty_input]
        expect_tty_input(mock_ioctl, mock_signal, mock_termios, mock_unistd, "x\n");
        // 端末設定、シグナル、端末サイズ、入力の 6 つの呼び出し期待を満たすこと。
        ASSERT_EQ(CPLAT_OK,
                  cplat_pinned_prompt_readline_at(screen, first_output, sizeof(first_output), "", "alloc.c",
                                                  1)); // [状態] - 同じ呼び出し位置で履歴のコンテキストを確保する。
        // [状態確認] - readline の戻り値が CPLAT_OK であること。
    }
    NiceMock<Mock_ioctl> mock_ioctl;
    NiceMock<Mock_signal> mock_signal;
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    // [サブ手順参照 名前=pinnedPromptTest.expect_tty_input]
    expect_tty_input(mock_ioctl, mock_signal, mock_termios, mock_unistd, "\n");
    // 端末設定、シグナル、端末サイズ、入力の 6 つの呼び出し期待を満たすこと。
    // [Pre-Assert手順] - 端末操作を成功させる。
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_realloc が編集バッファーの拡張のために 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial_at(screen, output, sizeof(output), "", long_initial.c_str(),
                                                              "alloc.c", 1); // [手順] - 長い初期値で 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, result); // [確認_異常系] - CPLAT_ERR_OUT_OF_MEMORY が返ること。
    EXPECT_STREQ("", output);                   // [確認_異常系] - 出力先が空文字列であること。

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

// 端末サイズ取得が GetConsoleScreenBufferInfo の成功値と失敗を検証することの確認
TEST(pinnedPromptTest, platform_get_size_uses_valid_console_buffer_info)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE out_handle = (HANDLE)0x1234;
    int valid_cols = 0;
    int valid_rows = 0;
    int fallback_cols = 0;
    int fallback_rows = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).Times(2).WillRepeatedly(Return(out_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle が STD_OUTPUT_HANDLE を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - ダミーの標準出力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, out_handle, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, PCONSOLE_SCREEN_BUFFER_INFO info)
            {
                fill_console_window(info, 120, 40);
                return TRUE;
            })
        .WillOnce(Return(FALSE));
    // [Pre-Assert確認_正常系] - GetConsoleScreenBufferInfo が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は列数 120・行数 40 を返却し、2 回目は FALSE を返却する。

    // Act
    test_pinned_prompt_get_size(&valid_cols, &valid_rows); // [手順] - 正常なコンソール情報から端末サイズを取得する。
    test_pinned_prompt_get_size(&fallback_cols,
                                &fallback_rows); // [手順] - コンソール情報取得失敗時の既定端末サイズを取得する。

    // Assert
    EXPECT_EQ(120, valid_cols);   // [確認_正常系] - GetConsoleScreenBufferInfo 成功時の列数が 120 であること。
    EXPECT_EQ(40, valid_rows);    // [確認_正常系] - GetConsoleScreenBufferInfo 成功時の行数が 40 であること。
    EXPECT_EQ(80, fallback_cols); // [確認_異常系] - GetConsoleScreenBufferInfo 失敗時の列数が 80 であること。
    EXPECT_EQ(24, fallback_rows); // [確認_異常系] - GetConsoleScreenBufferInfo 失敗時の行数が 24 であること。
}

// 標準出力ハンドルが無効な場合に既定の端末サイズへフォールバックすることの確認
TEST(pinnedPromptTest, platform_get_size_falls_back_when_stdout_handle_invalid)
{
    // Arrange
    Mock_windows mock_windows;
    int cols = 0;
    int rows = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).WillOnce(Return(INVALID_HANDLE_VALUE));
    // [Pre-Assert確認_異常系] - GetStdHandle が STD_OUTPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - INVALID_HANDLE_VALUE を返却する。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - GetConsoleScreenBufferInfo が呼び出されないこと。

    // Act
    test_pinned_prompt_get_size(&cols, &rows); // [手順] - 無効な標準出力ハンドルから端末サイズを取得する。

    // Assert
    EXPECT_EQ(80, cols); // [確認_異常系] - 無効ハンドル時の列数が 80 であること。
    EXPECT_EQ(24, rows); // [確認_異常系] - 無効ハンドル時の行数が 24 であること。
}

// Windows の raw モード移行と復帰がコンソール API を呼び出すことの確認
TEST(pinnedPromptTest, platform_raw_mode_enters_and_leaves_once_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    Mock_windows mock_windows;
    HANDLE in_handle = (HANDLE)0x1234;
    DWORD orig_mode = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
    DWORD captured_mode = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(in_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ダミーの標準入力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(orig_mode), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetConsoleMode が 1 回呼び出されること。
    // [Pre-Assert手順] - 元のコンソール モードを返却する。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SaveArg<4>(&captured_mode), Return(TRUE)))
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - SetConsoleMode が開始と解除で 2 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - raw モードへ移行する。
    test_pinned_prompt_enter_raw(screen); // [手順] - raw モード移行済みの状態で再度移行する。
    int active_after_enter = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_leave_raw(screen); // [手順] - raw モードから復帰する。
    test_pinned_prompt_leave_raw(screen); // [手順] - raw モード復帰済みの状態で再度復帰する。
    int active_after_leave = test_pinned_prompt_raw_active(screen);

    // Assert
    EXPECT_EQ(1, active_after_enter); // [確認_正常系] - raw モード移行後の状態が有効であること。
    EXPECT_EQ(0, active_after_leave); // [確認_正常系] - raw モード復帰後の状態が無効であること。
    EXPECT_NE(0U,
              captured_mode & (DWORD)ENABLE_VIRTUAL_TERMINAL_INPUT); // [確認_正常系] - 仮想端末入力が有効化されること。
    EXPECT_EQ(0U, captured_mode & (DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT));
    // [確認_正常系] - エコー・行入力・Ctrl+C シグナル化が無効化されること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Windows の raw モードがハンドル無効、モード取得失敗、モード設定失敗を処理することの確認
TEST(pinnedPromptTest, platform_raw_mode_handles_failures_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    Mock_windows mock_windows;
    HANDLE in_handle = (HANDLE)0x1234;
    int after_invalid_handle = 0;
    int after_get_mode_failure = 0;
    int after_set_mode_failure = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(INVALID_HANDLE_VALUE))
        .WillOnce(Return(in_handle))
        .WillOnce(Return(in_handle));
    // [Pre-Assert確認_異常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は INVALID_HANDLE_VALUE、以降はダミー ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(Return(FALSE))
        .WillOnce(DoAll(SetArgPointee<4>(0U), Return(TRUE)));
    // [Pre-Assert確認_異常系] - GetConsoleMode が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は FALSE、2 回目は元モード 0 を返却する。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _)).WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - SetConsoleMode が 1 回呼び出されること。
    // [Pre-Assert手順] - FALSE を返却する。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - 無効な標準入力ハンドルで raw モードへ移行する。
    after_invalid_handle = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_enter_raw(screen); // [手順] - GetConsoleMode 失敗状態で raw モードへ移行する。
    after_get_mode_failure = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_enter_raw(screen); // [手順] - SetConsoleMode 失敗状態で raw モードへ移行する。
    after_set_mode_failure = test_pinned_prompt_raw_active(screen);

    // Assert
    EXPECT_EQ(0, after_invalid_handle);   // [確認_異常系] - 無効ハンドル後の raw 状態が無効であること。
    EXPECT_EQ(0, after_get_mode_failure); // [確認_異常系] - GetConsoleMode 失敗後の raw 状態が無効であること。
    EXPECT_EQ(0, after_set_mode_failure); // [確認_異常系] - SetConsoleMode 失敗後の raw 状態が無効であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Windows の入力読み取りがタイムアウト、失敗、文字、EOF を分類することの確認
TEST(pinnedPromptTest, platform_read_char_handles_timeout_and_eof_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    Mock_windows mock_windows;
    HANDLE in_handle = (HANDLE)0x1234;
    int timeout_result = 0;
    int wait_fail_result = 0;
    int character_result = 0;
    int eof_result = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(in_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ダミーの標準入力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(0U), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetConsoleMode が 1 回呼び出されること。
    // [Pre-Assert手順] - 元のコンソール モードを返却する。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _)).WillOnce(Return(TRUE)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - SetConsoleMode が開始と解除で 2 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, in_handle, 100U))
        .WillOnce(Return(WAIT_TIMEOUT))
        .WillOnce(Return(WAIT_FAILED))
        .WillOnce(Return(WAIT_OBJECT_0))
        .WillOnce(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - WaitForSingleObject が 100 ms 待ちで 4 回呼び出されること。
    // [Pre-Assert手順] - TIMEOUT、FAILED、OBJECT_0、OBJECT_0 の順に返却する。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, in_handle, _, 1U, _, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read, LPOVERLAPPED)
            {
                *static_cast<char *>(buffer) = 'B';
                *bytes_read = 1U;
                return TRUE;
            })
        .WillOnce(DoAll(SetArgPointee<6>(0U), Return(TRUE)));
    // [Pre-Assert確認_正常系] - ReadFile が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は文字 B、2 回目は 0 バイトを返却する。

    // Act
    test_pinned_prompt_enter_raw(screen);                    // [手順] - 入力待ちの前に raw モードへ移行する。
    timeout_result = test_pinned_prompt_read_char(screen);   // [手順] - WAIT_TIMEOUT の入力待ちを処理する。
    wait_fail_result = test_pinned_prompt_read_char(screen); // [手順] - WAIT_FAILED の入力待ちを処理する。
    character_result = test_pinned_prompt_read_char(screen); // [手順] - 1 バイト入力を読み取る。
    eof_result = test_pinned_prompt_read_char(screen);       // [手順] - 0 バイト入力を EOF として読み取る。

    // Assert
    EXPECT_EQ(-2, timeout_result);                      // [確認_正常系] - WAIT_TIMEOUT が -2 として返ること。
    EXPECT_EQ(-1, wait_fail_result);                    // [確認_異常系] - WAIT_FAILED が -1 として返ること。
    EXPECT_EQ(static_cast<int>('B'), character_result); // [確認_正常系] - 読み取った文字 B が返ること。
    EXPECT_EQ(-1, eof_result);                          // [確認_正常系] - 0 バイト読み取りが EOF として返ること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Windows の non-blocking 入力読み取りが待ち結果を処理することの確認
TEST(pinnedPromptTest, platform_read_char_nb_handles_wait_timeout_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(NULL); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - ハンドルが非 NULL であること。
    Mock_windows mock_windows;
    HANDLE in_handle = (HANDLE)0x1234;
    int timeout_result = 0;
    int character_result = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(in_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ダミーの標準入力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(0U), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetConsoleMode が 1 回呼び出されること。
    // [Pre-Assert手順] - 元のコンソール モードを返却する。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _)).WillOnce(Return(TRUE)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - SetConsoleMode が開始と解除で 2 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, in_handle, 50U))
        .WillOnce(Return(WAIT_TIMEOUT))
        .WillOnce(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - WaitForSingleObject が 50 ms 待ちで 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は TIMEOUT、2 回目は OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, in_handle, 100U)).WillOnce(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - 文字読み取りのために WaitForSingleObject が 100 ms 待ちで 1 回呼び出されること。
    // [Pre-Assert手順] - OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, in_handle, _, 1U, _, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read, LPOVERLAPPED)
            {
                *static_cast<char *>(buffer) = 'C';
                *bytes_read = 1U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - ReadFile が 1 回呼び出されること。
    // [Pre-Assert手順] - 文字 C を返却する。

    // Act
    test_pinned_prompt_enter_raw(screen);                     // [手順] - 入力待ちの前に raw モードへ移行する。
    timeout_result = test_pinned_prompt_read_char_nb(screen); // [手順] - 50 ms 待ちがタイムアウトする状態で入力を読む。
    character_result = test_pinned_prompt_read_char_nb(screen); // [手順] - 入力可能な状態で 1 文字を読む。

    // Assert
    EXPECT_EQ(-1, timeout_result);                      // [確認_正常系] - 50 ms 待ちの TIMEOUT が -1 として返ること。
    EXPECT_EQ(static_cast<int>('C'), character_result); // [確認_正常系] - 読み取った文字 C が返ること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#endif /* PLATFORM_WINDOWS */
