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
    int enable_result = cplat_pinned_prompt_status_enable(
        nullptr, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
        1); // [手順] - screen に nullptr を指定して cplat_pinned_prompt_status_enable() を呼び出す。
    int set_result = cplat_pinned_prompt_status_set(
        nullptr, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
        "status"); // [手順] - screen に nullptr を指定して cplat_pinned_prompt_status_set() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        enable_result); // [確認_異常系] - cplat_pinned_prompt_status_enable() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        set_result); // [確認_異常系] - cplat_pinned_prompt_status_set() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
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
    size_t null_len =
        test_pinned_prompt_cstr_len(nullptr); // [手順] - nullptr を指定して test_pinned_prompt_cstr_len() を呼び出す。
    size_t ascii_width = test_pinned_prompt_utf8_width(
        "A", 1U, 0U); // [手順] - ASCII 文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t combining_width =
        test_pinned_prompt_utf8_width(combining, sizeof(combining) - 1U,
                                      0U); // [手順] - 結合文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t cjk_width = test_pinned_prompt_utf8_width(
        cjk, sizeof(cjk) - 1U, 0U); // [手順] - CJK 文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t hiragana_width =
        test_pinned_prompt_utf8_width(hiragana, sizeof(hiragana) - 1U,
                                      0U); // [手順] - ひらがなを指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t katakana_width =
        test_pinned_prompt_utf8_width(katakana, sizeof(katakana) - 1U,
                                      0U); // [手順] - カタカナを指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t compatibility_width = test_pinned_prompt_utf8_width(
        compatibility_ideograph, sizeof(compatibility_ideograph) - 1U,
        0U); // [手順] - CJK 拡張文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t emoji_width = test_pinned_prompt_utf8_width(
        emoji, sizeof(emoji) - 1U, 0U); // [手順] - 4 バイト文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t invalid_width = test_pinned_prompt_utf8_width(
        "\xC2", 1U, 0U); // [手順] - 不完全な UTF-8 を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t sgr_len = test_pinned_prompt_ansi_len(
        sgr, sizeof(sgr) - 1U, 0U); // [手順] - ANSI SGR を指定して test_pinned_prompt_ansi_len() を呼び出す。
    size_t invalid_sgr_len =
        test_pinned_prompt_ansi_len(invalid_sgr, sizeof(invalid_sgr) - 1U,
                                    0U); // [手順] - 不正な SGR を指定して test_pinned_prompt_ansi_len() を呼び出す。

    // Assert
    EXPECT_EQ(0U,
              null_len); // [確認_正常系] - nullptr を指定した test_pinned_prompt_cstr_len() の戻り値が 0 であること。
    EXPECT_EQ(1U,
              ascii_width); // [確認_正常系] - "A" を指定した test_pinned_prompt_utf8_width() の戻り値が 1 であること。
    EXPECT_EQ(
        0U,
        combining_width); // [確認_正常系] - 結合文字を指定した test_pinned_prompt_utf8_width() の戻り値が 0 であること。
    EXPECT_EQ(
        2U, cjk_width); // [確認_正常系] - CJK 文字を指定した test_pinned_prompt_utf8_width() の戻り値が 2 であること。
    EXPECT_EQ(
        2U,
        hiragana_width); // [確認_正常系] - ひらがなを指定した test_pinned_prompt_utf8_width() の戻り値が 2 であること。
    EXPECT_EQ(
        2U,
        katakana_width); // [確認_正常系] - カタカナを指定した test_pinned_prompt_utf8_width() の戻り値が 2 であること。
    EXPECT_EQ(
        2U,
        compatibility_width); // [確認_正常系] - CJK 拡張文字を指定した test_pinned_prompt_utf8_width() の戻り値が 2 であること。
    EXPECT_EQ(
        1U,
        emoji_width); // [確認_正常系] - 絵文字を指定した test_pinned_prompt_utf8_width() の戻り値が 1 であること。(2FFFF 未満の絵文字の表示幅が 1 であることの確認)
    EXPECT_EQ(
        1U,
        invalid_width); // [確認_正常系] - 不完全な UTF-8 を指定した test_pinned_prompt_utf8_width() の戻り値が 1 であること。
    EXPECT_EQ(5U,
              sgr_len); // [確認_正常系] - ANSI SGR を指定した test_pinned_prompt_ansi_len() の戻り値が 5 であること。
    EXPECT_EQ(
        0U,
        invalid_sgr_len); // [確認_正常系] - 不正な ANSI シーケンスを指定した test_pinned_prompt_ansi_len() の戻り値が 0 であること。
}

// 固定プロンプトの表示幅計算が ANSI と全角文字を処理することの確認
TEST(pinnedPromptTest, static_display_helpers_skip_ansi_and_limit_columns)
{
    // Arrange
    const char text[] = "\x1B[31m日A";
    const size_t text_len = sizeof(text) - 1U;

    // Pre-Assert

    // Act
    size_t visible_one_col = test_pinned_prompt_visible_bytes(
        text, text_len, 0U, 1U); // [手順] - 表示列数 1 を指定して test_pinned_prompt_visible_bytes() を呼び出す。
    size_t visible_three_cols = test_pinned_prompt_visible_bytes(
        text, text_len, 0U, 3U); // [手順] - 表示列数 3 を指定して test_pinned_prompt_visible_bytes() を呼び出す。
    size_t display_width = test_pinned_prompt_display_width(
        text, text_len, 0U,
        text_len); // [手順] - ANSI を含む全範囲を指定して test_pinned_prompt_display_width() を呼び出す。
    size_t clipped_width = test_pinned_prompt_display_width(
        text, text_len, 0U,
        2U); // [手順] - ANSI 途中までの範囲を指定して test_pinned_prompt_display_width() を呼び出す。

    // Assert
    EXPECT_EQ(
        5U,
        visible_one_col); // [確認_正常系] - 1 列を指定した test_pinned_prompt_visible_bytes() の戻り値が 5 であること。(SGR と全角文字が 1 列に収まることの確認)
    EXPECT_EQ(
        text_len,
        visible_three_cols); // [確認_正常系] - 3 列を指定した test_pinned_prompt_visible_bytes() の戻り値が text_len と一致すること。(3 列制限で全内容が収まることの確認)
    EXPECT_EQ(
        3U,
        display_width); // [確認_正常系] - 全範囲を指定した test_pinned_prompt_display_width() の戻り値が 3 であること。(全角文字と ASCII の幅合計が 3 であることの確認)
    EXPECT_EQ(
        2U,
        clipped_width); // [確認_正常系] - 2 バイトまでを指定した test_pinned_prompt_display_width() の戻り値が 2 であること。(ANSI 途中までの範囲が通常文字 2 列として計算されることの確認)
}

// 非 TTY の readline が cplat_fgets の入力を返すことの確認
TEST(pinnedPromptTest, fallback_readline_strips_newline)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char input[] = "answer";
    char output[32] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(input, input + sizeof(input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_fgets() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets() から、"answer" を設定して CPLAT_OK を返す。

    // Act
    int result =
        cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                     nullptr); // [手順] - 非 TTY の状態で cplat_pinned_prompt_readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);              // [確認_正常系] - cplat_pinned_prompt_readline() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("answer", output); // [確認_正常系] - output が "answer" であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 非 TTY の readline が EOF を返すことの確認
TEST(pinnedPromptTest, fallback_readline_reports_eof)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char output[8] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _)).WillOnce(Return(CPLAT_ERR_EOF));
    // [Pre-Assert確認_異常系] - cplat_fgets() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets() から CPLAT_ERR_EOF を返す。

    // Act
    int result = cplat_pinned_prompt_readline(
        screen, output, sizeof(output),
        "prompt"); // [手順] - EOF を返す非 TTY の状態で cplat_pinned_prompt_readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF,
              result); // [確認_異常系] - cplat_pinned_prompt_readline() の戻り値が CPLAT_ERR_EOF であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 非 TTY の readline が行の切り詰めをバッファー不足として返すことの確認
TEST(pinnedPromptTest, fallback_readline_reports_buffer_too_small)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char empty[] = "";
    char output[8] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(empty, empty + sizeof(empty)), Return(CPLAT_ERR_BUFFER_TOO_SMALL)));
    // [Pre-Assert確認_異常系] - cplat_fgets() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets() から、空文字列を設定して CPLAT_ERR_BUFFER_TOO_SMALL を返す。

    // Act
    int actual_ret = cplat_pinned_prompt_readline(
        screen, output, sizeof(output),
        nullptr); // [手順] - 行が収まらない非 TTY の状態で cplat_pinned_prompt_readline() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_pinned_prompt_readline() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", output); // [確認_異常系] - output が "" であること。(切り詰め時に出力先が空になることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 固定プロンプトのステータス API が上下左右の有効な指定を処理することの確認
TEST(pinnedPromptTest, status_apis_accept_valid_positions_and_alignments)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。

    // Pre-Assert

    // Act
    int top_enable = cplat_pinned_prompt_status_enable(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
        1); // [手順] - 上部ステータスを有効にする指定で cplat_pinned_prompt_status_enable() を呼び出す。
    int bottom_disable = cplat_pinned_prompt_status_enable(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM,
        0); // [手順] - 下部ステータスを無効にする指定で cplat_pinned_prompt_status_enable() を呼び出す。
    int top_left = cplat_pinned_prompt_status_set(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
        "top"); // [手順] - 上部左側の内容を指定して cplat_pinned_prompt_status_set() を呼び出す。
    int top_right = cplat_pinned_prompt_status_set(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT,
        "right"); // [手順] - 上部右側の内容を指定して cplat_pinned_prompt_status_set() を呼び出す。
    int bottom_left = cplat_pinned_prompt_status_set(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
        "bottom"); // [手順] - 下部左側の内容を指定して cplat_pinned_prompt_status_set() を呼び出す。
    int bottom_right = cplat_pinned_prompt_status_set(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT,
        nullptr); // [手順] - 下部右側の内容に nullptr を指定して cplat_pinned_prompt_status_set() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        top_enable); // [確認_正常系] - 上部を有効にした cplat_pinned_prompt_status_enable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        bottom_disable); // [確認_正常系] - 下部を無効にした cplat_pinned_prompt_status_enable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        top_left); // [確認_正常系] - 上部左側を指定した cplat_pinned_prompt_status_set() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        top_right); // [確認_正常系] - 上部右側を指定した cplat_pinned_prompt_status_set() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        bottom_left); // [確認_正常系] - 下部左側を指定した cplat_pinned_prompt_status_set() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        bottom_right); // [確認_正常系] - 下部右側に nullptr を指定した cplat_pinned_prompt_status_set() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 非 TTY の初期値付き readline が、初期値を使わずに cplat_fgets の入力を返すことの確認
TEST(pinnedPromptTest, fallback_readline_with_initial_ignores_initial_text)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char input[] = "piped";
    char output[32] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(input, input + sizeof(input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_fgets() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets() から、"piped" を設定して CPLAT_OK を返す。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial(
        screen, output, sizeof(output), nullptr,
        "initial"); // [手順] - 初期値を指定し、非 TTY の状態で cplat_pinned_prompt_readline_with_initial() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_pinned_prompt_readline_with_initial() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "piped",
        output); // [確認_正常系] - output が "piped" であること。(初期値ではなく cplat_fgets() の入力が格納されたことの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 初期値付き readline が、制御文字を含む初期値と不正な引数を入力の前に拒否することの確認
TEST(pinnedPromptTest, readline_with_initial_rejects_invalid_initial_text_and_arguments)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_cplat> mock_cplat;
    char output[32] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - cplat_fgets() が呼び出されないこと。

    // Act
    int actual_ret_control = cplat_pinned_prompt_readline_with_initial(
        screen, output, sizeof(output), nullptr,
        "a\tb"); // [手順] - タブを含む初期値を指定して cplat_pinned_prompt_readline_with_initial() を呼び出す。
    int actual_ret_screen = cplat_pinned_prompt_readline_with_initial(
        nullptr, output, sizeof(output), nullptr,
        "a"); // [手順] - ハンドルに nullptr を指定して cplat_pinned_prompt_readline_with_initial() を呼び出す。
    int actual_ret_buf = cplat_pinned_prompt_readline_with_initial(
        screen, nullptr, sizeof(output), nullptr,
        "a"); // [手順] - 出力先に nullptr を指定して cplat_pinned_prompt_readline_with_initial() を呼び出す。
    int actual_ret_size = cplat_pinned_prompt_readline_with_initial(
        screen, output, 0U, nullptr,
        "a"); // [手順] - バイト数に 0 を指定して cplat_pinned_prompt_readline_with_initial() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_control); // [確認_異常系] - 制御文字を含む初期値を指定した cplat_pinned_prompt_readline_with_initial() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("", output); // [確認_異常系] - output が "" であること。(拒否時に出力先が空になることの確認)
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_screen); // [確認_異常系] - ハンドルに nullptr を指定した cplat_pinned_prompt_readline_with_initial() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_buf); // [確認_異常系] - 出力先に nullptr を指定した cplat_pinned_prompt_readline_with_initial() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_size); // [確認_異常系] - バイト数に 0 を指定した cplat_pinned_prompt_readline_with_initial() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

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
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - ioctl() から 1 回目は、列数 120・行数 40 の端末サイズを設定して 0 を返し、2 回目は -1 を返す。

    // Act
    test_pinned_prompt_get_size(
        &valid_cols, &valid_rows); // [手順] - ioctl() が成功する状態で test_pinned_prompt_get_size() を呼び出す。
    test_pinned_prompt_get_size(
        &fallback_cols, &fallback_rows); // [手順] - ioctl() が失敗する状態で test_pinned_prompt_get_size() を呼び出す。

    // Assert
    EXPECT_EQ(
        120,
        valid_cols); // [確認_正常系] - valid_cols が 120 であること。(ioctl() 成功時の列数が 120 であることの確認)
    EXPECT_EQ(40,
              valid_rows); // [確認_正常系] - valid_rows が 40 であること。(ioctl() 成功時の行数が 40 であることの確認)
    EXPECT_EQ(
        80,
        fallback_cols); // [確認_異常系] - fallback_cols が 80 であること。(ioctl() 失敗時の既定の列数が 80 であることの確認)
    EXPECT_EQ(
        24,
        fallback_rows); // [確認_異常系] - fallback_rows が 24 であること。(ioctl() 失敗時の既定の行数が 24 であることの確認)
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
    // [Pre-Assert確認_異常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 0・行数 0 の端末サイズを設定して 0 を返す。

    // Act
    test_pinned_prompt_get_size(
        &cols, &rows); // [手順] - ioctl() が列数と行数を 0 で返す状態で test_pinned_prompt_get_size() を呼び出す。

    // Assert
    EXPECT_EQ(80, cols); // [確認_異常系] - cols が 80 であること。(列数 0 が既定値 80 へ置き換わることの確認)
    EXPECT_EQ(24, rows); // [確認_異常系] - rows が 24 であること。(行数 0 が既定値 24 へ置き換わることの確認)
}

// Linux の raw モード移行と復帰が端末 API と SIGWINCH API を呼び出すことの確認
TEST(pinnedPromptTest, platform_raw_mode_enters_and_leaves_once)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_signal> mock_signal;
    struct termios original = {};

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr() が STDIN_FILENO を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset() が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset() から 0 を返す。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction() が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction() から 0 を返す。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - test_pinned_prompt_enter_raw() を呼び出す。
    test_pinned_prompt_enter_raw(
        screen); // [手順] - raw モード移行済みの状態で test_pinned_prompt_enter_raw() を呼び出す。
    int active_after_enter = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_leave_raw(screen); // [手順] - test_pinned_prompt_leave_raw() を呼び出す。
    test_pinned_prompt_leave_raw(
        screen); // [手順] - raw モード復帰済みの状態で test_pinned_prompt_leave_raw() を呼び出す。
    int active_after_leave = test_pinned_prompt_raw_active(screen);

    // Assert
    EXPECT_EQ(
        1,
        active_after_enter); // [確認_正常系] - raw モード移行後の test_pinned_prompt_raw_active() の戻り値が 1 であること。(raw モードが有効であることの確認)
    EXPECT_EQ(
        0,
        active_after_leave); // [確認_正常系] - raw モード復帰後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Linux の入力読み取りが EINTR とリサイズ通知を分類することの確認
TEST(pinnedPromptTest, platform_read_char_handles_resize_and_eof)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_正常系] - read() が STDIN_FILENO を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - read() から 1 回目は -1、2 回目は文字 B、3 回目は 0 バイトの読み取り結果を返す。

    // Act
    int resize_result = test_pinned_prompt_read_char(
        screen); // [手順] - EINTR とリサイズ通知がある状態で test_pinned_prompt_read_char() を呼び出す。
    int character_result = test_pinned_prompt_read_char(
        screen); // [手順] - 1 バイトの入力がある状態で test_pinned_prompt_read_char() を呼び出す。
    int eof_result = test_pinned_prompt_read_char(
        screen); // [手順] - 0 バイトの入力 (EOF) がある状態で test_pinned_prompt_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(
        -2,
        resize_result); // [確認_正常系] - EINTR とリサイズ通知があるときの test_pinned_prompt_read_char() の戻り値が -2 であること。(リサイズ通知であることの確認)
    EXPECT_EQ(
        0,
        test_pinned_prompt_resize_pending()); // [確認_正常系] - test_pinned_prompt_resize_pending() の戻り値が 0 であること。(リサイズ通知が消費されたことの確認)
    EXPECT_EQ(
        static_cast<int>('B'),
        character_result); // [確認_正常系] - 1 バイト入力の test_pinned_prompt_read_char() の戻り値が 'B' であること。
    EXPECT_EQ(
        -1,
        eof_result); // [確認_正常系] - 0 バイト入力の test_pinned_prompt_read_char() の戻り値が -1 であること。(EOF であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Linux の non-blocking 入力読み取りが select 結果を処理することの確認
TEST(pinnedPromptTest, platform_read_char_nb_handles_select_timeout)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    NiceMock<Mock_sys_select> mock_select;
    NiceMock<Mock_unistd> mock_unistd;
    unsigned char character = 'C';

    // Pre-Assert
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillOnce(Return(0)).WillOnce(Return(1));
    // [Pre-Assert確認_正常系] - select() が 2 回呼び出されること。
    // [Pre-Assert手順] - select() から 1 回目はタイムアウト (0)、2 回目は入力可 (1) を返す。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(DoAll(Invoke([character](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = character; }),
                        Return(static_cast<ssize_t>(1))));
    // [Pre-Assert確認_正常系] - read() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - read() から、文字 C を設定して 1 を返す。

    // Act
    int timeout_result = test_pinned_prompt_read_char_nb(
        screen); // [手順] - select() がタイムアウトする状態で test_pinned_prompt_read_char_nb() を呼び出す。
    int character_result = test_pinned_prompt_read_char_nb(
        screen); // [手順] - select() が入力可能を返す状態で test_pinned_prompt_read_char_nb() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        timeout_result); // [確認_正常系] - select() がタイムアウトしたときの test_pinned_prompt_read_char_nb() の戻り値が -1 であること。(入力がないことの確認)
    EXPECT_EQ(
        static_cast<int>('C'),
        character_result); // [確認_正常系] - 入力可能のときの test_pinned_prompt_read_char_nb() の戻り値が 'C' であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の readline が通常文字を受け取り Enter で入力を確定することの確認
TEST(pinnedPromptTest, tty_readline_accepts_character_and_enter)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr() が STDIN_FILENO を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset() が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset() から 0 を返す。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction() が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction() から 0 を返す。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 80・行数 24 の端末サイズを設定して 0 を返す。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(DoAll(Invoke([first](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = first; }),
                        Return(static_cast<ssize_t>(1))))
        .WillOnce(DoAll(Invoke([second](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = second; }),
                        Return(static_cast<ssize_t>(1))));
    // [Pre-Assert確認_正常系] - read() が STDIN_FILENO を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - read() から 1 回目は文字 a、2 回目は改行を 1 バイトずつ設定して 1 を返す。

    // Act
    int result = cplat_pinned_prompt_readline(
        screen, output, sizeof(output),
        "prompt> "); // [手順] - 文字 a と Enter の入力がある TTY の状態で cplat_pinned_prompt_readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_pinned_prompt_readline() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a",
                 output); // [確認_正常系] - output が "a" であること。(文字 a と Enter で確定した入力であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の readline が Ctrl-C をキャンセルとして返すことの確認
TEST(pinnedPromptTest, tty_readline_reports_canceled_on_ctrl_c)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_異常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_異常系] - tcsetattr() が STDIN_FILENO を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - sigemptyset() が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset() から 0 を返す。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_異常系] - sigaction() が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction() から 0 を返す。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_異常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 80・行数 24 の端末サイズを設定して 0 を返す。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(DoAll(Invoke([cancel](const char *, const int, const char *, const int, void *arg, const size_t)
                               { *static_cast<unsigned char *>(arg) = cancel; }),
                        Return(static_cast<ssize_t>(1))));
    // [Pre-Assert確認_異常系] - read() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - read() から、Ctrl-C を設定して 1 を返す。

    // Act
    int result = cplat_pinned_prompt_readline(
        screen, output, sizeof(output),
        "prompt> "); // [手順] - Ctrl-C の入力がある TTY の状態で cplat_pinned_prompt_readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CANCELED,
              result); // [確認_異常系] - cplat_pinned_prompt_readline() の戻り値が CPLAT_ERR_CANCELED であること。
    EXPECT_STREQ(
        "", output); // [確認_異常系] - output が "" であること。(Ctrl-C でキャンセルされたとき出力が空になることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 端末入力の各キー表現が内部キーへ分類されることの確認
TEST(pinnedPromptTest, read_key_classifies_control_and_escape_sequences)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_正常系] - read() が STDIN_FILENO を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - read() から、用意したキー列を 1 バイトずつ設定して 1 を順に返し、消費後は EINTR と EOF を返す。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillOnce(Return(0)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_正常系] - select() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - select() から 1 回目はタイムアウト (0)、以降は入力可 (1) を返す。

    // Act
    int char_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - ASCII 文字の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int char_value = ch;
    int enter_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 改行の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int backspace_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - DEL の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int control_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 制御文字の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int high_bit_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 8 ビット文字の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int clear_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 単独の ESC の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int unknown_escape_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 未知の ESC 文字列の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int up_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 上矢印の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int down_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 下矢印の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int right_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 右矢印の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int left_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 左矢印の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int home_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - Home の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int end_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - End の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int home_tilde_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - "\x1B[1~" の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int delete_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - "\x1B[3~" の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int end_tilde_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - "\x1B[4~" の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int invalid_tilde_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 不正な終端の入力がある状態で test_pinned_prompt_read_key() を呼び出す。
    int invalid_csi_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - 未知の CSI の入力がある状態で test_pinned_prompt_read_key() を呼び出す。

    test_pinned_prompt_set_resize_pending(1);
    errno = EINTR;
    int resize_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - リサイズ通知がある状態で test_pinned_prompt_read_key() を呼び出す。
    int eof_key = test_pinned_prompt_read_key(
        screen, &ch); // [手順] - EOF の入力がある状態で test_pinned_prompt_read_key() を呼び出す。

    // Assert
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_CHAR,
        char_key); // [確認_正常系] - ASCII 文字の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_CHAR であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_ENTER,
        enter_key); // [確認_正常系] - 改行の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_ENTER であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_BACKSPACE,
        backspace_key); // [確認_正常系] - DEL の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_BACKSPACE であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_UNKNOWN,
        control_key); // [確認_異常系] - 制御文字の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_UNKNOWN であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_CHAR,
        high_bit_key); // [確認_正常系] - 8 ビット文字の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_CHAR であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_CLEAR,
        clear_key); // [確認_正常系] - 単独の ESC の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_CLEAR であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_UNKNOWN,
        unknown_escape_key); // [確認_異常系] - 未知の ESC の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_UNKNOWN であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_UP,
        up_key); // [確認_正常系] - 上矢印の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_UP であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_DOWN,
        down_key); // [確認_正常系] - 下矢印の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_DOWN であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_RIGHT,
        right_key); // [確認_正常系] - 右矢印の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_RIGHT であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_LEFT,
        left_key); // [確認_正常系] - 左矢印の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_LEFT であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_HOME,
        home_key); // [確認_正常系] - Home の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_HOME であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_END,
        end_key); // [確認_正常系] - End の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_END であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_HOME,
        home_tilde_key); // [確認_正常系] - ESC [ 1 ~ の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_HOME であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_DELETE,
        delete_key); // [確認_正常系] - ESC [ 3 ~ の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_DELETE であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_END,
        end_tilde_key); // [確認_正常系] - ESC [ 4 ~ の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_END であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_UNKNOWN,
        invalid_tilde_key); // [確認_異常系] - 不正な終端の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_UNKNOWN であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_UNKNOWN,
        invalid_csi_key); // [確認_異常系] - 未知の CSI の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_UNKNOWN であること。
    EXPECT_EQ(
        char_value,
        static_cast<int>('A')); // [確認_正常系] - char_value が 'A' であること。(読み取った文字が A であることの確認)
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_RESIZE,
        resize_key); // [確認_正常系] - リサイズ通知の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_RESIZE であること。
    EXPECT_EQ(
        TEST_PINNED_PROMPT_KEY_EOF,
        eof_key); // [確認_正常系] - EOF の test_pinned_prompt_read_key() の戻り値が TEST_PINNED_PROMPT_KEY_EOF であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 行編集 static 関数が UTF-8 境界とカーソル エンドポイントを処理することの確認
TEST(pinnedPromptTest, edit_helpers_update_line_at_boundaries)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。

    // Pre-Assert

    // Act
    test_pinned_prompt_set_edit_line(
        screen, "ab"); // [手順] - ASCII の編集行を指定して test_pinned_prompt_set_edit_line() を呼び出す。
    test_pinned_prompt_insert_byte(screen, 'X'); // [手順] - X を指定して test_pinned_prompt_insert_byte() を呼び出す。
    test_pinned_prompt_backspace(
        screen); // [手順] - 編集行末尾に X がある状態で test_pinned_prompt_backspace() を呼び出す。
    test_pinned_prompt_set_edit_line(
        screen, "ab"); // [手順] - 削除対象を持つ編集行を指定して test_pinned_prompt_set_edit_line() を呼び出す。
    test_pinned_prompt_set_cursor(screen,
                                  1U); // [手順] - b の直前の位置を指定して test_pinned_prompt_set_cursor() を呼び出す。
    test_pinned_prompt_delete(
        screen); // [手順] - カーソルが b の直前にある状態で test_pinned_prompt_delete() を呼び出す。
    test_pinned_prompt_backspace(
        screen); // [手順] - カーソルが先頭にある状態で test_pinned_prompt_backspace() を呼び出す。
    test_pinned_prompt_set_edit_line(
        screen, "\xE6\x97\xA5"); // [手順] - UTF-8 の編集行を指定して test_pinned_prompt_set_edit_line() を呼び出す。
    test_pinned_prompt_backspace(
        screen); // [手順] - 編集行が UTF-8 文字の状態で test_pinned_prompt_backspace() を呼び出す。
    test_pinned_prompt_delete(
        screen); // [手順] - カーソルが編集行末尾にある状態で test_pinned_prompt_delete() を呼び出す。

    // Assert
    EXPECT_EQ(
        0U,
        test_pinned_prompt_edit_length(
            screen)); // [確認_正常系] - test_pinned_prompt_edit_length() の戻り値が 0 であること。(編集操作の後に編集行が空であることの確認)
    EXPECT_STREQ(
        "",
        test_pinned_prompt_edit_text(
            screen)); // [確認_正常系] - test_pinned_prompt_edit_text() の戻り値が "" であること。(編集操作の後に編集行が空であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

    #if defined(PLATFORM_LINUX)

// TTY 描画がステータス領域、区切り線、入力カーソルを描画することの確認
TEST(pinnedPromptTest, render_handles_status_regions_and_empty_layout)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize full_size = {};
    full_size.ws_col = 24U;
    full_size.ws_row = 8U;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([full_size](const char *, const int, const char *, const int, const unsigned long,
                                                 void *arg) { *static_cast<struct winsize *>(arg) = full_size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 24・行数 8 の端末サイズを設定して 0 を返す。

    // Act
    test_pinned_prompt_render_state(
        screen, 1, 1, 1, 1, "P> ", "abc", "TOP", "RIGHT", "BOTTOM",
        "BR"); // [手順] - 上下ステータスと入力行を指定して test_pinned_prompt_render_state() を呼び出す。
    test_pinned_prompt_render(
        screen); // [手順] - ステータス領域を含む TTY の状態で test_pinned_prompt_render() を呼び出す。
    test_pinned_prompt_render(
        screen); // [手順] - status_dirty が解除された状態で test_pinned_prompt_render() を呼び出す。
    test_pinned_prompt_render_state(
        screen, 1, 1, 1, 1, "", "", nullptr, nullptr, nullptr,
        nullptr); // [手順] - 空のプロンプトとステータスを指定して test_pinned_prompt_render_state() を呼び出す。
    test_pinned_prompt_render(screen); // [手順] - 空状態で test_pinned_prompt_render() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        test_pinned_prompt_edit_text(
            screen)); // [確認_正常系] - test_pinned_prompt_edit_text() の戻り値が "" であること。(空のプロンプトを描画した後も編集行が空であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の編集操作と同一呼び出し元の履歴参照が入力結果へ反映されることの確認
TEST(pinnedPromptTest, tty_readline_handles_editing_and_history)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(6).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr() が STDIN_FILENO を指定して 6 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).Times(3).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset() が 3 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset() から 0 を返す。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(6).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction() が SIGWINCH を指定して 6 回呼び出されること。
    // [Pre-Assert手順] - sigaction() から 0 を返す。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 80・行数 24 の端末サイズを設定して 0 を返す。
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
    // [Pre-Assert確認_正常系] - read() が STDIN_FILENO を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - read() から、用意したキー列を 1 バイトずつ設定して 1 を順に返す。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_正常系] - select() が任意の回数呼び出されること。
    // [Pre-Assert手順] - select() から入力可 (1) を返す。

    // Act
    int first_result = cplat_pinned_prompt_readline_at(
        screen, first_output, sizeof(first_output), "", "history.c",
        10); // [手順] - 編集キーを含む最初の入力がある状態で cplat_pinned_prompt_readline_at() を呼び出す。
    int second_result = cplat_pinned_prompt_readline_at(
        screen, second_output, sizeof(second_output), "", "history.c",
        10); // [手順] - 2 件目の入力を同じ呼び出し位置の履歴へ追加する状態で cplat_pinned_prompt_readline_at() を呼び出す。
    int third_result = cplat_pinned_prompt_readline_at(
        screen, third_output, sizeof(third_output), "", "history.c",
        10); // [手順] - 上下キーで履歴を参照する入力がある状態で cplat_pinned_prompt_readline_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        first_result); // [確認_正常系] - 1 件目の cplat_pinned_prompt_readline_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "c", first_output); // [確認_正常系] - first_output が "c" であること。(編集後の 1 件目の入力であることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        second_result); // [確認_正常系] - 2 件目の cplat_pinned_prompt_readline_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("d", second_output); // [確認_正常系] - second_output が "d" であること。
    EXPECT_EQ(
        CPLAT_OK,
        third_result); // [確認_正常系] - 履歴参照後の cplat_pinned_prompt_readline_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "",
        third_output); // [確認_正常系] - third_output が "" であること。(履歴の末尾から下へ移動すると保存行へ戻ったことの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY のリサイズ通知が readline の再描画後に入力を継続することの確認
TEST(pinnedPromptTest, tty_readline_continues_after_resize)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr() が STDIN_FILENO を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset() が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset() から 0 を返す。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction() が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction() から 0 を返す。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 80・行数 24 の端末サイズを設定して 0 を返す。
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
    // [Pre-Assert確認_正常系] - read() が STDIN_FILENO を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - read() から 1 回目は EINTR、以降は改行を返す。

    // Act
    int result = cplat_pinned_prompt_readline(
        screen, output, sizeof(output),
        ""); // [手順] - リサイズ通知後に Enter の入力がある状態で cplat_pinned_prompt_readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_pinned_prompt_readline() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", output); // [確認_正常系] - output が "" であること。(リサイズ後に確定した入力が空であることの確認)

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
    size_t at_end = test_pinned_prompt_utf8_width(
        "A", 1U, 1U); // [手順] - 文字列末尾の位置を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t two_byte_width = test_pinned_prompt_utf8_width(
        two_byte, sizeof(two_byte) - 1U,
        0U); // [手順] - 2 バイト文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t three_byte_width = test_pinned_prompt_utf8_width(
        three_byte, sizeof(three_byte) - 1U,
        0U); // [手順] - CJK 以外の 3 バイト文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t astral_wide_width = test_pinned_prompt_utf8_width(
        astral_wide, sizeof(astral_wide) - 1U,
        0U); // [手順] - 2FFFF を超える 4 バイト文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t truncated_three_width = test_pinned_prompt_utf8_width(
        truncated_three, sizeof(truncated_three) - 1U,
        0U); // [手順] - 不完全な 3 バイト文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t truncated_four_width = test_pinned_prompt_utf8_width(
        truncated_four, sizeof(truncated_four) - 1U,
        0U); // [手順] - 不完全な 4 バイト文字を指定して test_pinned_prompt_utf8_width() を呼び出す。
    size_t valid_sgr_len = test_pinned_prompt_ansi_len(
        valid_sgr, sizeof(valid_sgr) - 1U,
        0U); // [手順] - パラメーター付き SGR を指定して test_pinned_prompt_ansi_len() を呼び出す。
    size_t invalid_sgr_len = test_pinned_prompt_ansi_len(
        invalid_sgr, sizeof(invalid_sgr) - 1U,
        0U); // [手順] - 不正終端の SGR を指定して test_pinned_prompt_ansi_len() を呼び出す。
    size_t incomplete_sgr_len =
        test_pinned_prompt_ansi_len(incomplete_sgr, sizeof(incomplete_sgr) - 1U,
                                    0U); // [手順] - 未完了の SGR を指定して test_pinned_prompt_ansi_len() を呼び出す。
    size_t zero_visible = test_pinned_prompt_visible_bytes(
        "A", 1U, 0U, 0U); // [手順] - 表示列数 0 を指定して test_pinned_prompt_visible_bytes() を呼び出す。
    size_t clipped_display = test_pinned_prompt_display_width(
        "ABC", 3U, 1U, 8U); // [手順] - 終端を超える範囲を指定して test_pinned_prompt_display_width() を呼び出す。

    // Assert
    EXPECT_EQ(0U, at_end); // [確認_正常系] - 末尾位置の test_pinned_prompt_utf8_width() の戻り値が 0 であること。
    EXPECT_EQ(
        1U, two_byte_width); // [確認_正常系] - 2 バイト文字の test_pinned_prompt_utf8_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        three_byte_width); // [確認_正常系] - CJK 以外の 3 バイト文字の test_pinned_prompt_utf8_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        astral_wide_width); // [確認_正常系] - 2FFFF を超える 4 バイト文字の test_pinned_prompt_utf8_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        truncated_three_width); // [確認_異常系] - 不完全な 3 バイト文字の test_pinned_prompt_utf8_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        truncated_four_width); // [確認_異常系] - 不完全な 4 バイト文字の test_pinned_prompt_utf8_width() の戻り値が 1 であること。
    EXPECT_EQ(
        7U,
        valid_sgr_len); // [確認_正常系] - パラメーター付き SGR の test_pinned_prompt_ansi_len() の戻り値が 7 であること。
    EXPECT_EQ(
        0U, invalid_sgr_len); // [確認_異常系] - 不正終端 SGR の test_pinned_prompt_ansi_len() の戻り値が 0 であること。
    EXPECT_EQ(
        0U,
        incomplete_sgr_len); // [確認_異常系] - 未完了 SGR の test_pinned_prompt_ansi_len() の戻り値が 0 であること。
    EXPECT_EQ(
        0U, zero_visible); // [確認_正常系] - 表示列数 0 の test_pinned_prompt_visible_bytes() の戻り値が 0 であること。
    EXPECT_EQ(
        2U,
        clipped_display); // [確認_正常系] - 終端を超える範囲の test_pinned_prompt_display_width() の戻り値が 2 であること。(範囲を終端で切った表示幅が 2 であることの確認)
}

// 固定プロンプトの履歴補助関数が空状態と重複入力を処理することの確認
TEST(pinnedPromptTest, history_helpers_cover_empty_and_duplicate_entries)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。

    // Pre-Assert

    // Act
    test_pinned_prompt_history_edge_cases(
        screen); // [手順] - 空履歴、重複履歴、履歴移動の境界の状態で test_pinned_prompt_history_edge_cases() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "same",
        test_pinned_prompt_edit_text(
            screen)); // [確認_正常系] - test_pinned_prompt_edit_text() の戻り値が "same" であること。(履歴から復元した入力であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

    #if defined(PLATFORM_LINUX)

// 固定プロンプトの Linux raw モードが失敗と再入を処理することの確認
TEST(pinnedPromptTest, platform_raw_mode_handles_failures_and_reentry)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_reset_platform_state();
    NiceMock<Mock_termios> mock_termios;
    NiceMock<Mock_signal> mock_signal;
    struct termios original = {};

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(Return(-1))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_異常系] - tcgetattr() が STDIN_FILENO を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から 1 回目は -1 を返し、以降は元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).WillOnce(Return(-1)).WillRepeatedly(Return(0));
    // [Pre-Assert確認_異常系] - tcsetattr() が STDIN_FILENO を指定して 1 回以上呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 1 回目は -1 を返し、以降は 0 を返す。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset() が 1 回呼び出されること。
    // [Pre-Assert手順] - sigemptyset() から 0 を返す。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction() が SIGWINCH を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sigaction() から 0 を返す。

    // Act
    test_pinned_prompt_enter_raw(
        screen); // [手順] - tcgetattr() が失敗する状態で test_pinned_prompt_enter_raw() を呼び出す。
    int after_getattr_failure = test_pinned_prompt_raw_active(
        screen); // [手順] - tcgetattr() の失敗後の状態で test_pinned_prompt_raw_active() を呼び出す。
    test_pinned_prompt_enter_raw(
        screen); // [手順] - tcsetattr() が失敗する状態で test_pinned_prompt_enter_raw() を呼び出す。
    int after_setattr_failure = test_pinned_prompt_raw_active(
        screen); // [手順] - tcsetattr() の失敗後の状態で test_pinned_prompt_raw_active() を呼び出す。
    test_pinned_prompt_enter_raw(screen); // [手順] - test_pinned_prompt_enter_raw() を呼び出す。
    int after_enter = test_pinned_prompt_raw_active(
        screen); // [手順] - raw モード開始後の状態で test_pinned_prompt_raw_active() を呼び出す。
    test_pinned_prompt_enter_raw(screen); // [手順] - raw モード中の状態で test_pinned_prompt_enter_raw() を呼び出す。
    test_pinned_prompt_leave_raw(screen); // [手順] - test_pinned_prompt_leave_raw() を呼び出す。
    int after_leave = test_pinned_prompt_raw_active(
        screen); // [手順] - raw モード解除後の状態で test_pinned_prompt_raw_active() を呼び出す。
    test_pinned_prompt_leave_raw(
        screen); // [手順] - raw モード解除済みの状態で test_pinned_prompt_leave_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        after_getattr_failure); // [確認_異常系] - tcgetattr() 失敗後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)
    EXPECT_EQ(
        0,
        after_setattr_failure); // [確認_異常系] - tcsetattr() 失敗後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)
    EXPECT_EQ(
        1,
        after_enter); // [確認_正常系] - raw モード開始後の test_pinned_prompt_raw_active() の戻り値が 1 であること。(raw モードが有効であることの確認)
    EXPECT_EQ(
        0,
        after_leave); // [確認_正常系] - raw モード解除後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 固定プロンプトの Linux 入力が EINTR、非 EINTR、select 結果を分類することの確認
TEST(pinnedPromptTest, platform_read_helpers_handle_interrupt_and_select_results)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_異常系] - read() が STDIN_FILENO を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - read() から 1 回目は EINTR、2 回目は EAGAIN、3 回目は文字 Q を返す。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillOnce(Return(0)).WillOnce(Return(1));
    // [Pre-Assert確認_異常系] - select() が 2 回呼び出されること。
    // [Pre-Assert手順] - select() から 1 回目はタイムアウト (0)、2 回目は入力可 (1) を返す。

    // Act
    int non_eintr_result = test_pinned_prompt_read_char(
        screen); // [手順] - EINTR の後に EINTR 以外の read() の失敗がある状態で test_pinned_prompt_read_char() を呼び出す。
    int select_timeout_result = test_pinned_prompt_read_char_nb(
        screen); // [手順] - select() がタイムアウトする状態で test_pinned_prompt_read_char_nb() を呼び出す。
    int select_read_result = test_pinned_prompt_read_char_nb(
        screen); // [手順] - select() が成功し、1 文字を読み取れる状態で test_pinned_prompt_read_char_nb() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        non_eintr_result); // [確認_異常系] - 非 EINTR の read() 失敗時の test_pinned_prompt_read_char() の戻り値が -1 であること。
    EXPECT_EQ(
        -1,
        select_timeout_result); // [確認_異常系] - select() がタイムアウトしたときの test_pinned_prompt_read_char_nb() の戻り値が -1 であること。(入力がないことの確認)
    EXPECT_EQ(
        static_cast<int>('Q'),
        select_read_result); // [確認_正常系] - select() 成功後の test_pinned_prompt_read_char_nb() の戻り値が 'Q' であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 固定プロンプトの出力準備が非 TTY と表示中 TTY を処理することの確認
TEST(pinnedPromptTest, prepare_output_handles_tty_visibility)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize size = {};
    size.ws_col = 40U;
    size.ws_row = 8U;

    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 40・行数 8 の端末サイズを設定して 0 を返す。

    // Act
    test_pinned_prompt_set_tty(screen, 0); // [手順] - 非 TTY を指定して test_pinned_prompt_set_tty() を呼び出す。
    test_pinned_prompt_prepare_output(
        screen); // [手順] - 非 TTY の状態で test_pinned_prompt_prepare_output() を呼び出す。
    test_pinned_prompt_render_state(
        screen, 1, 1, 0, 0, "P> ", "text", nullptr, nullptr, nullptr,
        nullptr); // [手順] - 表示中の TTY 状態を指定して test_pinned_prompt_render_state() を呼び出す。
    test_pinned_prompt_prepare_output(
        screen); // [手順] - 表示中の TTY の状態で test_pinned_prompt_prepare_output() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(非 TTY と表示中の TTY の出力準備が完了したことの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

    #endif /* PLATFORM_LINUX */

// 書き込み API が引数、ストリーム、短い書き込みを分類することの確認
TEST(pinnedPromptTest, write_and_printf_handle_arguments_and_short_write)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    test_pinned_prompt_set_tty(screen, 0);
    NiceMock<Mock_stdio> mock_stdio;
    const char data[] = "abc";
    size_t written = 99U;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, _, _, _)).WillOnce(Return(2U)).WillRepeatedly(ReturnArg<5>());
    // [Pre-Assert確認_異常系] - fwrite() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - fwrite() から 1 回目は 2 バイト、以降は要求サイズを返す。

    // Act
    int invalid_screen = cplat_pinned_prompt_write(
        nullptr, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
        &written); // [手順] - ハンドルに nullptr を指定して cplat_pinned_prompt_write() を呼び出す。
    int invalid_data = cplat_pinned_prompt_write(
        screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, nullptr, 1U,
        &written); // [手順] - データに nullptr、サイズに正の値を指定して cplat_pinned_prompt_write() を呼び出す。
    int short_result = cplat_pinned_prompt_write(
        screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
        &written); // [手順] - 標準出力で短い書き込みになる状態で cplat_pinned_prompt_write() を呼び出す。
    int stdout_result =
        cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
                                  &written); // [手順] - 標準出力を指定して cplat_pinned_prompt_write() を呼び出す。
    int stderr_result =
        cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR, data, 3U,
                                  &written); // [手順] - 標準エラーを指定して cplat_pinned_prompt_write() を呼び出す。
    int empty_result = cplat_pinned_prompt_write(
        screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, nullptr, 0U,
        &written); // [手順] - データに nullptr、サイズに 0 を指定して cplat_pinned_prompt_write() を呼び出す。
    int printf_result =
        cplat_pinned_prompt_printf(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s-%d", "value",
                                   7); // [手順] - 書式付き文字列を指定して cplat_pinned_prompt_printf() を呼び出す。
    int null_fmt_result = cplat_pinned_prompt_printf(
        screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
        nullptr); // [手順] - 書式に nullptr を指定して cplat_pinned_prompt_printf() を呼び出す。
    int null_printf_result = cplat_pinned_prompt_printf(
        nullptr, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
        "x"); // [手順] - ハンドルに nullptr を指定して cplat_pinned_prompt_printf() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_screen); // [確認_異常系] - nullptr ハンドルの cplat_pinned_prompt_write() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_data); // [確認_異常系] - nullptr データの cplat_pinned_prompt_write() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        short_result); // [確認_異常系] - 短い書き込みの cplat_pinned_prompt_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_OK,
        stdout_result); // [確認_正常系] - 標準出力への全量の cplat_pinned_prompt_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        stderr_result); // [確認_正常系] - 標準エラーへの全量の cplat_pinned_prompt_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              empty_result); // [確認_正常系] - サイズ 0 の cplat_pinned_prompt_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        7,
        printf_result); // [確認_正常系] - 書式付きの cplat_pinned_prompt_printf() の戻り値が 7 であること。(書式 "value-7" の 7 バイトを書き込んだことの確認)
    EXPECT_EQ(
        0,
        null_fmt_result); // [確認_正常系] - nullptr 書式の cplat_pinned_prompt_printf() の戻り値が 0 であること。(空文字列を書き込んだことの確認)
    EXPECT_EQ(
        -1,
        null_printf_result); // [確認_異常系] - nullptr ハンドルの cplat_pinned_prompt_printf() の戻り値が -1 であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 書式付き readline がプロンプトを生成して fallback 入力へ渡すことの確認
TEST(pinnedPromptTest, readline_fmt_formats_and_accepts_null_format)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_正常系] - cplat_fgets() が 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets() から 1 回目は "first"、2 回目は "second" を設定して CPLAT_OK を返す。

    // Act
    int formatted_result =
        cplat_pinned_prompt_readline_fmt(screen, first_output, sizeof(first_output), "%s-%d", "p",
                                         3); // [手順] - 書式を指定して cplat_pinned_prompt_readline_fmt() を呼び出す。
    int null_format_result = cplat_pinned_prompt_readline_fmt(
        screen, second_output, sizeof(second_output),
        nullptr); // [手順] - 書式に nullptr を指定して cplat_pinned_prompt_readline_fmt() を呼び出す。
    int null_screen_result = cplat_pinned_prompt_readline_fmt_at(
        nullptr, second_output, sizeof(second_output), "file", 1, "%s",
        "x"); // [手順] - ハンドルに nullptr を指定して cplat_pinned_prompt_readline_fmt_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        formatted_result); // [確認_正常系] - 書式付きの cplat_pinned_prompt_readline_fmt() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("first", first_output); // [確認_正常系] - first_output が "first" であること。
    EXPECT_EQ(
        CPLAT_OK,
        null_format_result); // [確認_正常系] - nullptr 書式の cplat_pinned_prompt_readline_fmt() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("second", second_output); // [確認_正常系] - second_output が "second" であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_screen_result); // [確認_異常系] - nullptr ハンドルの cplat_pinned_prompt_readline_fmt_at() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// ステータス API が不正な位置と配置を拒否することの確認
TEST(pinnedPromptTest, status_apis_reject_invalid_position_and_alignment)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    int invalid_position_value = 99;
    int invalid_align_value = 99;
    const cplat_pinned_prompt_status_position invalid_position =
        static_cast<cplat_pinned_prompt_status_position>(invalid_position_value);
    const cplat_pinned_prompt_status_align invalid_align =
        static_cast<cplat_pinned_prompt_status_align>(invalid_align_value);

    // Pre-Assert

    // Act
    int invalid_enable = cplat_pinned_prompt_status_enable(
        screen, invalid_position, 1); // [手順] - 不正な位置を指定して cplat_pinned_prompt_status_enable() を呼び出す。
    int invalid_top_align = cplat_pinned_prompt_status_set(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, invalid_align,
        "x"); // [手順] - 上部と不正な配置を指定して cplat_pinned_prompt_status_set() を呼び出す。
    int invalid_bottom_align = cplat_pinned_prompt_status_set(
        screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, invalid_align,
        "x"); // [手順] - 下部と不正な配置を指定して cplat_pinned_prompt_status_set() を呼び出す。
    int invalid_set_position = cplat_pinned_prompt_status_set(
        screen, invalid_position, CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT,
        "x"); // [手順] - 不正な位置を指定して cplat_pinned_prompt_status_set() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_enable); // [確認_異常系] - 不正な位置の cplat_pinned_prompt_status_enable() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_top_align); // [確認_異常系] - 上部の不正な配置の cplat_pinned_prompt_status_set() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_bottom_align); // [確認_異常系] - 下部の不正な配置の cplat_pinned_prompt_status_set() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_set_position); // [確認_異常系] - 不正な位置の cplat_pinned_prompt_status_set() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

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
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr() が STDIN_FILENO を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。
    EXPECT_CALL(mock_signal, sigemptyset(_, _, _, _)).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigemptyset() が任意の回数呼び出されること。
    // [Pre-Assert手順] - sigemptyset() から 0 を返す。
    EXPECT_CALL(mock_signal, sigaction(_, _, _, SIGWINCH, _, _)).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - sigaction() が SIGWINCH を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - sigaction() から 0 を返す。
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillRepeatedly(DoAll(Invoke([size](const char *, const int, const char *, const int, const unsigned long,
                                            void *arg) { *static_cast<struct winsize *>(arg) = size; }),
                              Return(0)));
    // [Pre-Assert確認_正常系] - ioctl() が STDOUT_FILENO、TIOCGWINSZ を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ioctl() から、列数 80・行数 24 の端末サイズを設定して 0 を返す。
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
    // [Pre-Assert確認_正常系] - read() が STDIN_FILENO を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - read() から、指定した入力列を 1 バイトずつ設定して 1 を返し、終端では 0 を返す。
}
// [サブ手順終了]

// TTY の初期値付き readline で、初期値のまま Enter を押すと初期値が返ることの確認
TEST(pinnedPromptTest, tty_readline_with_initial_returns_initial_text_on_enter)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert手順] - read() から、入力として改行だけを返し、端末操作も成功させる。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial(
        screen, output, sizeof(output), "prompt> ",
        "edit 1 abc"); // [手順] - 初期値を指定して cplat_pinned_prompt_readline_with_initial() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_pinned_prompt_readline_with_initial() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("edit 1 abc",
                 output); // [確認_正常系] - output が "edit 1 abc" であること。(初期値がそのまま確定したことの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の初期値付き readline で、末尾のカーソルから初期値を編集できることの確認
TEST(pinnedPromptTest, tty_readline_with_initial_allows_editing_from_end)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert手順] - read() から、Backspace、"-3"、改行を順に返し、端末操作も成功させる。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial(
        screen, output, sizeof(output), "prompt> ",
        "arg.priority < 0"); // [手順] - 初期値を指定して cplat_pinned_prompt_readline_with_initial() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_pinned_prompt_readline_with_initial() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "arg.priority < -3",
        output); // [確認_正常系] - output が "arg.priority < -3" であること。(末尾の 0 が削除され、-3 が追加されたことの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の初期値付き readline で、履歴をさかのぼって戻ると初期値へ戻ることの確認
TEST(pinnedPromptTest, tty_readline_with_initial_restores_initial_text_after_history)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
        // [状態確認] - cplat_pinned_prompt_readline_at() の戻り値が CPLAT_OK であること。
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
    // [Pre-Assert手順] - read() から、上矢印、下矢印、改行を順に返し、端末操作も成功させる。
    EXPECT_CALL(mock_select, select(_, _, _, _, _, _, _, _)).WillRepeatedly(Return(1));
    // [Pre-Assert確認_正常系] - select() が任意の回数呼び出されること。
    // [Pre-Assert手順] - select() から入力可 (1) を返し、ESC の後続を同じキー入力として読ませる。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial_at(
        screen, output, sizeof(output), "", "initial", "history.c",
        1); // [手順] - 同じ呼び出し元の履歴を持つ状態で初期値を指定して cplat_pinned_prompt_readline_with_initial_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_pinned_prompt_readline_with_initial_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "initial",
        output); // [確認_正常系] - output が "initial" であること。(履歴を移動した後に退避した初期値へ戻ったことの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// TTY の初期値付き readline で、初期値のための編集バッファーの拡張に失敗した場合にメモリ不足を返すことの確認
TEST(pinnedPromptTest, tty_readline_with_initial_reports_out_of_memory_when_edit_buffer_expansion_fails)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
        // [状態確認] - cplat_pinned_prompt_readline_at() の戻り値が CPLAT_OK であること。
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
    // [Pre-Assert手順] - read() から、入力として改行だけを返し、端末操作も成功させる。
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_realloc() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_realloc() から nullptr を返す。

    // Act
    int result = cplat_pinned_prompt_readline_with_initial_at(
        screen, output, sizeof(output), "", long_initial.c_str(), "alloc.c",
        1); // [手順] - 長い初期値を指定して cplat_pinned_prompt_readline_with_initial_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        result); // [確認_異常系] - cplat_pinned_prompt_readline_with_initial_at() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_STREQ("", output); // [確認_異常系] - output が "" であること。(メモリ不足のとき出力先が空になることの確認)

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
    // [Pre-Assert確認_正常系] - GetStdHandle() が STD_OUTPUT_HANDLE を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - GetStdHandle() からダミーの標準出力ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, out_handle, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, PCONSOLE_SCREEN_BUFFER_INFO info)
            {
                fill_console_window(info, 120, 40);
                return TRUE;
            })
        .WillOnce(Return(FALSE));
    // [Pre-Assert確認_正常系] - GetConsoleScreenBufferInfo() が out_handle を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - GetConsoleScreenBufferInfo() から 1 回目は、列数 120・行数 40 のウィンドウを設定して TRUE を返し、2 回目は FALSE を返す。

    // Act
    test_pinned_prompt_get_size(
        &valid_cols,
        &valid_rows); // [手順] - コンソール情報の取得が成功する状態で test_pinned_prompt_get_size() を呼び出す。
    test_pinned_prompt_get_size(
        &fallback_cols,
        &fallback_rows); // [手順] - コンソール情報の取得が失敗する状態で test_pinned_prompt_get_size() を呼び出す。

    // Assert
    EXPECT_EQ(
        120,
        valid_cols); // [確認_正常系] - valid_cols が 120 であること。(GetConsoleScreenBufferInfo() 成功時の列数が 120 であることの確認)
    EXPECT_EQ(
        40,
        valid_rows); // [確認_正常系] - valid_rows が 40 であること。(GetConsoleScreenBufferInfo() 成功時の行数が 40 であることの確認)
    EXPECT_EQ(
        80,
        fallback_cols); // [確認_異常系] - fallback_cols が 80 であること。(GetConsoleScreenBufferInfo() 失敗時の既定の列数が 80 であることの確認)
    EXPECT_EQ(
        24,
        fallback_rows); // [確認_異常系] - fallback_rows が 24 であること。(GetConsoleScreenBufferInfo() 失敗時の既定の行数が 24 であることの確認)
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
    // [Pre-Assert確認_異常系] - GetStdHandle() が STD_OUTPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetStdHandle() から INVALID_HANDLE_VALUE を返す。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - GetConsoleScreenBufferInfo() が呼び出されないこと。

    // Act
    test_pinned_prompt_get_size(
        &cols, &rows); // [手順] - 無効な標準出力ハンドルの状態で test_pinned_prompt_get_size() を呼び出す。

    // Assert
    EXPECT_EQ(
        80,
        cols); // [確認_異常系] - cols が 80 であること。(無効な標準出力ハンドルのときの既定の列数が 80 であることの確認)
    EXPECT_EQ(
        24,
        rows); // [確認_異常系] - rows が 24 であること。(無効な標準出力ハンドルのときの既定の行数が 24 であることの確認)
}

// Windows の raw モード移行と復帰がコンソール API を呼び出すことの確認
TEST(pinnedPromptTest, platform_raw_mode_enters_and_leaves_once_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    Mock_windows mock_windows;
    HANDLE in_handle = (HANDLE)0x1234;
    DWORD orig_mode = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
    DWORD captured_mode = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(in_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetStdHandle() からダミーの標準入力ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(orig_mode), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetConsoleMode() が in_handle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetConsoleMode() から、元のコンソール モードを設定して TRUE を返す。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SaveArg<4>(&captured_mode), Return(TRUE)))
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - SetConsoleMode() が in_handle を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - SetConsoleMode() から TRUE を返す。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - test_pinned_prompt_enter_raw() を呼び出す。
    test_pinned_prompt_enter_raw(
        screen); // [手順] - raw モード移行済みの状態で test_pinned_prompt_enter_raw() を呼び出す。
    int active_after_enter = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_leave_raw(screen); // [手順] - test_pinned_prompt_leave_raw() を呼び出す。
    test_pinned_prompt_leave_raw(
        screen); // [手順] - raw モード復帰済みの状態で test_pinned_prompt_leave_raw() を呼び出す。
    int active_after_leave = test_pinned_prompt_raw_active(screen);

    // Assert
    EXPECT_EQ(
        1,
        active_after_enter); // [確認_正常系] - raw モード移行後の test_pinned_prompt_raw_active() の戻り値が 1 であること。(raw モードが有効であることの確認)
    EXPECT_EQ(
        0,
        active_after_leave); // [確認_正常系] - raw モード復帰後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)
    EXPECT_NE(
        0U,
        captured_mode &
            (DWORD)
                ENABLE_VIRTUAL_TERMINAL_INPUT); // [確認_正常系] - captured_mode & ENABLE_VIRTUAL_TERMINAL_INPUT が 0 以外であること。(仮想端末入力が有効化されたことの確認)
    EXPECT_EQ(0U, captured_mode & (DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT));
    // [確認_正常系] - captured_mode & (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT) が 0 であること。(エコー・行入力・Ctrl+C シグナル化が無効化されたことの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Windows の raw モードがハンドル無効、モード取得失敗、モード設定失敗を処理することの確認
TEST(pinnedPromptTest, platform_raw_mode_handles_failures_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
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
    // [Pre-Assert確認_異常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - GetStdHandle() から 1 回目は INVALID_HANDLE_VALUE を返し、以降はダミー ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(Return(FALSE))
        .WillOnce(DoAll(SetArgPointee<4>(0U), Return(TRUE)));
    // [Pre-Assert確認_異常系] - GetConsoleMode() が in_handle を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - GetConsoleMode() から 1 回目は FALSE を返し、2 回目は元モード 0 を設定して TRUE を返す。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _)).WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - SetConsoleMode() が in_handle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - SetConsoleMode() から FALSE を返す。

    // Act
    test_pinned_prompt_enter_raw(
        screen); // [手順] - 無効な標準入力ハンドルの状態で test_pinned_prompt_enter_raw() を呼び出す。
    after_invalid_handle = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_enter_raw(
        screen); // [手順] - GetConsoleMode() が失敗する状態で test_pinned_prompt_enter_raw() を呼び出す。
    after_get_mode_failure = test_pinned_prompt_raw_active(screen);
    test_pinned_prompt_enter_raw(
        screen); // [手順] - SetConsoleMode() が失敗する状態で test_pinned_prompt_enter_raw() を呼び出す。
    after_set_mode_failure = test_pinned_prompt_raw_active(screen);

    // Assert
    EXPECT_EQ(
        0,
        after_invalid_handle); // [確認_異常系] - 無効ハンドル後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)
    EXPECT_EQ(
        0,
        after_get_mode_failure); // [確認_異常系] - GetConsoleMode() 失敗後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)
    EXPECT_EQ(
        0,
        after_set_mode_failure); // [確認_異常系] - SetConsoleMode() 失敗後の test_pinned_prompt_raw_active() の戻り値が 0 であること。(raw モードが無効であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Windows の入力読み取りがタイムアウト、失敗、文字、EOF を分類することの確認
TEST(pinnedPromptTest, platform_read_char_handles_timeout_and_eof_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    Mock_windows mock_windows;
    HANDLE in_handle = (HANDLE)0x1234;
    int timeout_result = 0;
    int wait_fail_result = 0;
    int character_result = 0;
    int eof_result = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(in_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetStdHandle() からダミーの標準入力ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(0U), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetConsoleMode() が in_handle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetConsoleMode() から、元のコンソール モードを設定して TRUE を返す。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _)).WillOnce(Return(TRUE)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - SetConsoleMode() が in_handle を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - SetConsoleMode() から TRUE を返す。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, in_handle, 100U))
        .WillOnce(Return(WAIT_TIMEOUT))
        .WillOnce(Return(WAIT_FAILED))
        .WillOnce(Return(WAIT_OBJECT_0))
        .WillOnce(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - WaitForSingleObject() が in_handle、100 を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - WaitForSingleObject() から WAIT_TIMEOUT、WAIT_FAILED、WAIT_OBJECT_0、WAIT_OBJECT_0 の順に返す。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, in_handle, _, 1U, _, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read, LPOVERLAPPED)
            {
                *static_cast<char *>(buffer) = 'B';
                *bytes_read = 1U;
                return TRUE;
            })
        .WillOnce(DoAll(SetArgPointee<6>(0U), Return(TRUE)));
    // [Pre-Assert確認_正常系] - ReadFile() が in_handle、1 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - ReadFile() から 1 回目は、文字 B を設定して TRUE を返し、2 回目は、読み取りバイト数に 0 を設定して TRUE を返す。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - 入力待ちの前の状態で test_pinned_prompt_enter_raw() を呼び出す。
    timeout_result = test_pinned_prompt_read_char(
        screen); // [手順] - WAIT_TIMEOUT になる状態で test_pinned_prompt_read_char() を呼び出す。
    wait_fail_result = test_pinned_prompt_read_char(
        screen); // [手順] - WAIT_FAILED になる状態で test_pinned_prompt_read_char() を呼び出す。
    character_result = test_pinned_prompt_read_char(
        screen); // [手順] - 1 バイトの入力がある状態で test_pinned_prompt_read_char() を呼び出す。
    eof_result = test_pinned_prompt_read_char(
        screen); // [手順] - 0 バイトの入力 (EOF) がある状態で test_pinned_prompt_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(
        -2,
        timeout_result); // [確認_正常系] - WAIT_TIMEOUT の test_pinned_prompt_read_char() の戻り値が -2 であること。(タイムアウトであることの確認)
    EXPECT_EQ(
        -1,
        wait_fail_result); // [確認_異常系] - WAIT_FAILED の test_pinned_prompt_read_char() の戻り値が -1 であること。(入力待ちの失敗であることの確認)
    EXPECT_EQ(
        static_cast<int>('B'),
        character_result); // [確認_正常系] - 1 バイト入力の test_pinned_prompt_read_char() の戻り値が 'B' であること。
    EXPECT_EQ(
        -1,
        eof_result); // [確認_正常系] - 0 バイト入力の test_pinned_prompt_read_char() の戻り値が -1 であること。(EOF であることの確認)

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// Windows の non-blocking 入力読み取りが待ち結果を処理することの確認
TEST(pinnedPromptTest, platform_read_char_nb_handles_wait_timeout_on_windows)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr); // [状態] - ハンドルを用意する。
    ASSERT_NE(nullptr, screen);                                     // [状態確認] - screen が nullptr 以外であること。
    Mock_windows mock_windows;
    HANDLE in_handle = (HANDLE)0x1234;
    int timeout_result = 0;
    int character_result = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(in_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetStdHandle() からダミーの標準入力ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, in_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(0U), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetConsoleMode() が in_handle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetConsoleMode() から、元のコンソール モードを設定して TRUE を返す。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, in_handle, _)).WillOnce(Return(TRUE)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - SetConsoleMode() が in_handle を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - SetConsoleMode() から TRUE を返す。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, in_handle, 50U))
        .WillOnce(Return(WAIT_TIMEOUT))
        .WillOnce(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - WaitForSingleObject() が in_handle、50 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - WaitForSingleObject() から 1 回目は WAIT_TIMEOUT、2 回目は WAIT_OBJECT_0 を返す。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, in_handle, 100U)).WillOnce(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - WaitForSingleObject() が in_handle、100 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WaitForSingleObject() から WAIT_OBJECT_0 を返す。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, in_handle, _, 1U, _, _))
        .WillOnce(
            [](const char *, const int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read, LPOVERLAPPED)
            {
                *static_cast<char *>(buffer) = 'C';
                *bytes_read = 1U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - ReadFile() が in_handle、1 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ReadFile() から、文字 C を設定して TRUE を返す。

    // Act
    test_pinned_prompt_enter_raw(screen); // [手順] - 入力待ちの前の状態で test_pinned_prompt_enter_raw() を呼び出す。
    timeout_result = test_pinned_prompt_read_char_nb(
        screen); // [手順] - 50 ms の待機がタイムアウトする状態で test_pinned_prompt_read_char_nb() を呼び出す。
    character_result = test_pinned_prompt_read_char_nb(
        screen); // [手順] - 入力可能な状態で test_pinned_prompt_read_char_nb() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        timeout_result); // [確認_正常系] - 50 ms 待ちがタイムアウトしたときの test_pinned_prompt_read_char_nb() の戻り値が -1 であること。(入力がないことの確認)
    EXPECT_EQ(
        static_cast<int>('C'),
        character_result); // [確認_正常系] - 入力可能なときの test_pinned_prompt_read_char_nb() の戻り値が 'C' であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

#endif /* PLATFORM_WINDOWS */
