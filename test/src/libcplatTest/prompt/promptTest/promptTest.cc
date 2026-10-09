#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/result.h>
#include <cplat/prompt/prompt.h>
#include <cplat/prompt/prompt_internal.h>

#include <cstring>
#include <string>

#include "promptPlatformFake.h"

class promptTest : public Test
{
  protected:
    cplat_prompt *prompt_ = NULL;

    // [サブ手順 名前=promptTest.SetUp]
    void SetUp() override
    {
        promptFakeReset();
        prompt_ = cplat_prompt_create(NULL);
        ASSERT_NE((cplat_prompt *)NULL, prompt_);
        // [状態確認] - `(cplat_prompt *)NULL` と `prompt_` が異なること。
        /* 端末に接続していない実行環境でも対話パスを通すため、TTY 状態を直接立てる */
        prompt_->is_tty = 1;
    }
    // [サブ手順終了]

    // [サブ手順 名前=promptTest.TearDown]
    void TearDown() override
    {
        cplat_prompt_dispose(prompt_);
        prompt_ = NULL;
    }
    // [サブ手順終了]

    /* 入力列を設定して 1 行読み取る */
    int readline(const std::string &input, char *buf, size_t buf_size, const char *file = "promptTest.cc",
                 int line = 1)
    {
        promptFakeSetInput(input);
        return cplat_prompt_readline_at(prompt_, buf, buf_size, ">> ", file, line);
    }
};

/*
 * cplat_prompt_create / cplat_prompt_dispose
 */

// 既定オプションでハンドルが生成されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, create_applies_default_options)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_prompt *handle = cplat_prompt_create(NULL); // [手順] - options に NULL を指定して生成する。

    // Assert
    ASSERT_NE((cplat_prompt *)NULL,
              handle); // [確認_正常系] - cplat_prompt_create の戻り値が NULL でないこと。
    EXPECT_EQ((size_t)CPLAT_PROMPT_HISTORY_DEFAULT,
              handle->history_max); // [確認_正常系] - 履歴上限に既定値が設定されること。
    EXPECT_EQ((size_t)CPLAT_PROMPT_INPUT_BYTES_DEFAULT,
              handle->input_max_bytes); // [確認_正常系] - 入力上限に既定値が設定されること。

    // Cleanup
    cplat_prompt_dispose(handle);
}
// [サブ手順参照 名前=promptTest.TearDown]

// 指定したオプションが反映されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, create_applies_given_options)
{
    // Arrange
    cplat_prompt_options options = {};

    options.history_max = 4u;
    options.input_initial_capacity = 8u;
    options.input_max_bytes = 64u; // [状態] - 履歴 4 件、初期容量 8、入力上限 64 のオプションを用意する。

    // Pre-Assert

    // Act
    cplat_prompt *handle = cplat_prompt_create(&options); // [手順] - オプションを指定して生成する。

    // Assert
    ASSERT_NE((cplat_prompt *)NULL, handle); // [確認_正常系] - 戻り値が NULL でないこと。
    EXPECT_EQ(4u, handle->history_max);         // [確認_正常系] - 履歴上限が 4 であること。
    EXPECT_EQ(8u, handle->edit_cap);            // [確認_正常系] - 編集バッファーの初期容量が 8 であること。
    EXPECT_EQ(64u, handle->input_max_bytes);    // [確認_正常系] - 入力上限が 64 であること。

    // Cleanup
    cplat_prompt_dispose(handle);
}
// [サブ手順参照 名前=promptTest.TearDown]

// NULL ハンドルの破棄が安全であることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, dispose_accepts_null)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_prompt_dispose(NULL); // [手順] - NULL を指定して cplat_prompt_dispose を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - クラッシュせずに完了すること。
}
// [サブ手順参照 名前=promptTest.TearDown]

/*
 * cplat_prompt_readline_at (引数検証)
 */

// 不正な引数が拒否されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_rejects_invalid_arguments)
{
    // Arrange
    char buf[16]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_prompt = cplat_prompt_readline_at(NULL, buf, sizeof(buf), ">> ", "f", 1);
    int actual_ret_null_buf = cplat_prompt_readline_at(prompt_, NULL, sizeof(buf), ">> ", "f", 1);
    int actual_ret_zero_size =
        cplat_prompt_readline_at(prompt_, buf, 0u, ">> ", "f", 1); // [手順] - prompt、buf、buf_size に不正値を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_prompt); // [確認_異常系] - prompt が NULL のとき CPLAT_ERR_INVALID_ARGUMENT が返ること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_buf); // [確認_異常系] - buf が NULL のとき CPLAT_ERR_INVALID_ARGUMENT が返ること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_zero_size); // [確認_異常系] - buf_size が 0 のとき CPLAT_ERR_INVALID_ARGUMENT が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

/*
 * cplat_prompt_readline_at (キー処理)
 */

// 入力した文字列が Enter で確定することの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_returns_typed_line_on_enter)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("abc\r", buf, sizeof(buf)); // [手順] - "abc" と Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);   // [確認_正常系] - cplat_prompt_readline_at の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);      // [確認_正常系] - 入力した "abc" が返ること。
    EXPECT_EQ(1, promptFakeEnterRawCount()); // [確認_正常系] - raw モードへ 1 回移行すること。
    EXPECT_EQ(1, promptFakeLeaveRawCount()); // [確認_正常系] - raw モードを 1 回解除すること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// EOF で入力が打ち切られることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_returns_eof_when_input_ends)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("ab", buf, sizeof(buf)); // [手順] - Enter を含まない入力を与えて末尾まで読ませる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF, actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_EOF であること。
    EXPECT_STREQ("", buf);            // [確認_異常系] - 出力バッファーが空文字列になること。
    EXPECT_EQ(1, promptFakeLeaveRawCount()); // [確認_異常系] - raw モードが解除されること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// Ctrl+C で入力が取り消されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_returns_canceled_on_ctrl_c)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("ab\x03", buf, sizeof(buf)); // [手順] - "ab" に続けて Ctrl+C (0x03) を入力する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CANCELED, actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_CANCELED であること。
    EXPECT_STREQ("", buf);                 // [確認_異常系] - 出力バッファーが空文字列になること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// Backspace でカーソル直前の 1 文字が削除されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_backspace_removes_previous_character)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("abc\x7F\r", buf, sizeof(buf)); // [手順] - "abc" の後に Backspace (0x7F) と Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ab", buf);     // [確認_正常系] - 末尾の 1 文字が削除された "ab" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 行頭での Backspace が何もしないことの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_backspace_at_head_does_nothing)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x7F" "a\r", buf, sizeof(buf)); // [手順] - 行頭で Backspace を入力してから "a" と Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);      // [確認_正常系] - Backspace が無視されて "a" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 左矢印と Delete でカーソル位置の文字が削除されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_delete_removes_character_at_cursor)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("abc\x1B[D\x1B[3~\r", buf,
                       sizeof(buf)); // [手順] - "abc" の後に左矢印、Delete、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ab", buf);     // [確認_正常系] - カーソル位置の 'c' が削除された "ab" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 左矢印で戻った位置に文字が挿入されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_inserts_at_cursor_after_left_arrow)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("ac\x1B[D" "b\r", buf, sizeof(buf)); // [手順] - "ac" の後に左矢印、"b"、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - カーソル位置へ挿入された "abc" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 右矢印でカーソルが進むことの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_right_arrow_moves_cursor_forward)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("ac\x1B[D\x1B[C" "b\r", buf,
                       sizeof(buf)); // [手順] - "ac" の後に左矢印、右矢印、"b"、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("acb", buf);    // [確認_正常系] - カーソルが末尾へ戻り "acb" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// Home と End でカーソルが行頭と行末へ移動することの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_home_and_end_move_cursor_to_line_edges)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_home = readline("bc\x1B[H" "a\r", buf, sizeof(buf)); // [手順] - "bc" の後に Home、"a"、Enter を入力する。
    std::string after_home(buf);
    int actual_ret_end = readline("bc\x1B[H\x1B[F" "d\r", buf,
                           sizeof(buf)); // [手順] - "bc" の後に Home、End、"d"、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_home);     // [確認_正常系] - Home を含む呼び出しの戻り値が CPLAT_OK であること。
    EXPECT_EQ("abc", after_home);         // [確認_正常系] - 行頭へ挿入された "abc" が返ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_end);      // [確認_正常系] - End を含む呼び出しの戻り値が CPLAT_OK であること。
    EXPECT_STREQ("bcd", buf);             // [確認_正常系] - 行末へ挿入された "bcd" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// ESC 単押しで編集中の行が消去されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_clears_line_on_single_escape)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("abc\x1B", buf, sizeof(buf)); // [手順] - "abc" の後に ESC を入力し、以降の入力を与えない。

    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF, actual_ret); // [確認_異常系] - 行消去の後 EOF に達するため CPLAT_ERR_EOF が返ること。
    EXPECT_STREQ("", buf);            // [確認_異常系] - 出力バッファーが空文字列になること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 未対応のエスケープ シーケンスが無視されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_ignores_unknown_escape_sequence)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x1B[Z" "a\r", buf,
                       sizeof(buf)); // [手順] - 未対応の ESC [ Z を入力してから "a" と Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);      // [確認_正常系] - 未対応シーケンスが無視されて "a" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 数値付きエスケープ シーケンスが Home と End として扱われることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_numeric_escape_sequences_move_cursor)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_home = readline("bc\x1B[1~" "a\r", buf,
                            sizeof(buf)); // [手順] - "bc" の後に ESC [ 1 ~ (Home)、"a"、Enter を入力する。
    std::string after_home(buf);
    int actual_ret_end = readline("bc\x1B[1~\x1B[4~" "d\r", buf,
                           sizeof(buf)); // [手順] - "bc" の後に Home、ESC [ 4 ~ (End)、"d"、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_home); // [確認_正常系] - Home を含む呼び出しの戻り値が CPLAT_OK であること。
    EXPECT_EQ("abc", after_home);     // [確認_正常系] - 行頭へ挿入された "abc" が返ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_end);  // [確認_正常系] - End を含む呼び出しの戻り値が CPLAT_OK であること。
    EXPECT_STREQ("bcd", buf);         // [確認_正常系] - 行末へ挿入された "bcd" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 数値付きエスケープ シーケンスが '~' で終わらない場合に無視されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_ignores_incomplete_numeric_escape_sequence)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x1B[3X" "a\r", buf,
                       sizeof(buf)); // [手順] - '~' で終わらない ESC [ 3 X を入力してから "a" と Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);      // [確認_正常系] - 未完のシーケンスが無視されて "a" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// UTF-8 の 1 文字が Backspace でまとめて削除されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_backspace_removes_whole_utf8_character)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("a\xE3\x81\x82\x7F\r", buf,
                       sizeof(buf)); // [手順] - "a" と 3 バイトの日本語 1 文字の後に Backspace と Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);      // [確認_正常系] - 日本語 1 文字が 3 バイトまとめて削除されること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 出力バッファーに収まらない入力が切り詰められることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_truncates_line_to_buffer_size)
{
    // Arrange
    char buf[4]; // [状態] - 終端込みで 3 文字しか収まらない出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("abcdef\r", buf, sizeof(buf)); // [手順] - 6 文字を入力して Enter を押す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - 出力バッファーに収まる 3 文字へ切り詰められること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 入力上限を超える文字が破棄されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_stops_accepting_characters_at_input_limit)
{
    // Arrange
    char buf[32];
    cplat_prompt_options options = {};

    options.input_max_bytes = 4u;

    cplat_prompt *limited = cplat_prompt_create(&options); // [状態] - 入力上限 4 バイトのハンドルを用意する。
    ASSERT_NE((cplat_prompt *)NULL, limited);                // [状態確認] - ハンドルが非 NULL であること。
    limited->is_tty = 1;
    promptFakeSetInput("abcdef\r");

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_at(limited, buf, sizeof(buf), ">> ", "promptTest.cc",
                                          1); // [手順] - 上限を超える 6 文字を入力して Enter を押す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(3u, std::strlen(buf)); // [確認_正常系] - 入力上限に収まる 3 文字までが受け付けられること。

    // Cleanup
    cplat_prompt_dispose(limited);
}
// [サブ手順参照 名前=promptTest.TearDown]

/*
 * 履歴
 */

// 上矢印で直前の入力が呼び出されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, history_up_recalls_previous_line)
{
    // Arrange
    char buf[32];

    ASSERT_EQ(CPLAT_OK, readline("first\r", buf, sizeof(buf))); // [状態] - "first" を履歴へ登録しておく。
                                                                   // [状態確認] - readline の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x1B[A\r", buf, sizeof(buf)); // [手順] - 上矢印を押して Enter を押す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("first", buf);  // [確認_正常系] - 履歴の "first" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 上矢印と下矢印で履歴を往復できることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, history_down_returns_to_newer_entry)
{
    // Arrange
    char buf[32];

    ASSERT_EQ(CPLAT_OK, readline("first\r", buf, sizeof(buf))); // [状態] - "first" を履歴へ登録する。
                                                                   // [状態確認] - "first" を登録する readline の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, readline("second\r", buf, sizeof(buf))); // [状態] - 履歴へ 2 件を登録しておく。
                                                                    // [状態確認] - "second" を登録する readline の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x1B[A\x1B[A\x1B[B\r", buf,
                       sizeof(buf)); // [手順] - 上矢印を 2 回、下矢印を 1 回押して Enter を押す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("second\0", buf); // [確認_正常系] - 新しい側の履歴 "second" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 履歴の末尾で下矢印を押すと編集前の内容へ戻ることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, history_down_restores_saved_line_at_newest)
{
    // Arrange
    char buf[32];

    ASSERT_EQ(CPLAT_OK, readline("first\r", buf, sizeof(buf))); // [状態] - 履歴へ 1 件を登録しておく。
                                                                   // [状態確認] - readline の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = readline("ab\x1B[A\x1B[B\r", buf,
                       sizeof(buf)); // [手順] - "ab" を入力後に上矢印と下矢印を押して Enter を押す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ab", buf);     // [確認_正常系] - 履歴へ入る前の編集内容 "ab" が復元されること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 履歴がない状態で上矢印を押しても何も起きないことの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, history_up_does_nothing_when_empty)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x1B[A" "a\r", buf, sizeof(buf)); // [手順] - 履歴が空の状態で上矢印、"a"、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);      // [確認_正常系] - 上矢印が無視されて "a" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 空行が履歴へ登録されないことの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, history_does_not_record_empty_line)
{
    // Arrange
    char buf[32];

    ASSERT_EQ(CPLAT_OK, readline("\r", buf, sizeof(buf))); // [状態] - 空行を確定しておく。
                                                              // [状態確認] - 空行を確定する readline の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x1B[A" "a\r", buf, sizeof(buf)); // [手順] - 上矢印、"a"、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);      // [確認_正常系] - 履歴が空のままのため上矢印が無視されること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 呼び出し位置ごとに履歴が独立することの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, history_is_independent_per_call_site)
{
    // Arrange
    char buf[32];

    ASSERT_EQ(CPLAT_OK,
              readline("first\r", buf, sizeof(buf), "a.cc", 10)); // [状態] - 呼び出し位置 a.cc:10 で履歴を作る。
                                                                  // [状態確認] - 呼び出し位置 a.cc:10 の readline の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = readline("\x1B[A" "x\r", buf, sizeof(buf), "b.cc",
                       20); // [手順] - 別の呼び出し位置 b.cc:20 で上矢印、"x"、Enter を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("x", buf);      // [確認_正常系] - 別の呼び出し位置の履歴は参照されないこと。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 履歴上限を超えた場合に最古のエントリが破棄されることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, history_discards_oldest_entry_over_limit)
{
    // Arrange
    char buf[32];
    cplat_prompt_options options = {};

    options.history_max = 2u;

    cplat_prompt *limited = cplat_prompt_create(&options); // [状態] - 履歴上限 2 件のハンドルを用意する。
    ASSERT_NE((cplat_prompt *)NULL, limited);                // [状態確認] - ハンドルが非 NULL であること。
    limited->is_tty = 1;

    for (const char *line : {"one\r", "two\r", "three\r"})
    {
        promptFakeSetInput(line);
        ASSERT_EQ(CPLAT_OK, cplat_prompt_readline_at(limited, buf, sizeof(buf), ">> ", "promptTest.cc",
                                                          1)); // [状態] - 3 件を順に確定して上限を超えさせる。
                                                               // [状態確認] - cplat_prompt_readline_at の戻り値が CPLAT_OK であること。
    }

    // Pre-Assert

    // Act
    promptFakeSetInput("\x1B[A\x1B[A\x1B[A\r");
    int actual_ret = cplat_prompt_readline_at(limited, buf, sizeof(buf), ">> ", "promptTest.cc",
                                          1); // [手順] - 上矢印を 3 回押して Enter を押す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("two", buf); // [確認_正常系] - 最古の "one" が破棄され、遡れる最古が "two" であること。

    // Cleanup
    cplat_prompt_dispose(limited);
}
// [サブ手順参照 名前=promptTest.TearDown]

/*
 * cplat_prompt_readline_fmt_at
 */

// 書式付きプロンプトで 1 行読み取れることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_fmt_reads_line)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    promptFakeSetInput("abc\r");

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "promptTest.cc", 1, "[%d] ",
                                              7); // [手順] - 書式引数 7 を指定して 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - 入力した "abc" が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 書式付きプロンプトが不正な引数を拒否することの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_fmt_rejects_invalid_arguments)
{
    // Arrange
    char buf[32]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_prompt = cplat_prompt_readline_fmt_at(NULL, buf, sizeof(buf), "f", 1, ">> ");
    int actual_ret_null_buf = cplat_prompt_readline_fmt_at(prompt_, NULL, sizeof(buf), "f", 1, ">> ");
    int actual_ret_zero_size = cplat_prompt_readline_fmt_at(prompt_, buf, 0u, "f", 1,
                                                        ">> "); // [手順] - prompt、buf、buf_size に不正値を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_prompt); // [確認_異常系] - prompt が NULL のとき CPLAT_ERR_INVALID_ARGUMENT が返ること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_buf); // [確認_異常系] - buf が NULL のとき CPLAT_ERR_INVALID_ARGUMENT が返ること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_zero_size); // [確認_異常系] - buf_size が 0 のとき CPLAT_ERR_INVALID_ARGUMENT が返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

/*
 * TTY でない場合のフォールバック
 */

// TTY でない場合に cplat_fgets へフォールバックすることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_falls_back_to_fgets_when_not_tty)
{
    // Arrange
    char buf[32];

    prompt_->is_tty = 0; // [状態] - TTY でない状態にする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), NULL, "promptTest.cc",
                                          1); // [手順] - 標準入力が空の状態で 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF, actual_ret); // [確認_異常系] - 標準入力が EOF のため CPLAT_ERR_EOF が返ること。
    EXPECT_EQ(0, promptFakeEnterRawCount()); // [確認_異常系] - raw モードへ移行しないこと。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 非 TTY の readline が行の切り詰めをバッファー不足として返すことの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_fallback_reports_buffer_too_small)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char empty[] = "";
    char buf[8] = "stale";

    prompt_->is_tty = 0; // [状態] - TTY でない状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(empty, empty + sizeof(empty)), Return(CPLAT_ERR_BUFFER_TOO_SMALL)));
    // [Pre-Assert確認_異常系] - cplat_fgets が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から CPLAT_ERR_BUFFER_TOO_SMALL と空文字列を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), NULL, "promptTest.cc",
                                          1); // [手順] - 行が収まらない非 TTY readline を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - 切り詰め時の cplat_prompt_readline_at が CPLAT_ERR_BUFFER_TOO_SMALL を返すこと。
    EXPECT_STREQ("", buf); // [確認_異常系] - 切り詰め時に出力先が空文字列であること。
}
// [サブ手順参照 名前=promptTest.TearDown]

/*
 * cplat_prompt_readline_with_initial_at
 */

// 初期値のまま Enter を押すと初期値が返ることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_returns_initial_text_on_enter)
{
    // Arrange
    char buf[32];

    promptFakeSetInput("\r"); // [状態] - Enter だけを入力する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), ">> ", "edit 1 abc",
                                                           "promptTest.cc", 1); // [手順] - 初期値を指定して 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);   // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("edit 1 abc", buf); // [確認_正常系] - 初期値がそのまま返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 初期値の末尾にカーソルがあり、後退削除と入力で編集できることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_places_cursor_at_end)
{
    // Arrange
    char buf[32];

    promptFakeSetInput("\x7FX\r"); // [状態] - Backspace、X、Enter を入力する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), ">> ", "abc", "promptTest.cc",
                                                           1); // [手順] - 初期値 "abc" を指定して 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abX", buf);        // [確認_正常系] - 末尾の c が削除され、X が追加されること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 初期値の先頭へ移動して挿入できることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_allows_insert_at_head)
{
    // Arrange
    char buf[32];

    promptFakeSetInput("\x1B[Ha\r"); // [状態] - Home、a、Enter を入力する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), ">> ", "bc", "promptTest.cc",
                                                           1); // [手順] - 初期値 "bc" を指定して 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);        // [確認_正常系] - 先頭に a が挿入されること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 履歴をさかのぼってから最新の側へ戻ると、初期値へ戻ることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_restores_initial_text_after_history)
{
    // Arrange
    char buf[32];

    ASSERT_EQ(CPLAT_OK, readline("first\r", buf, sizeof(buf))); // [状態] - "first" を履歴へ登録しておく。
                                                                   // [状態確認] - readline の戻り値が CPLAT_OK であること。
    promptFakeSetInput("\x1B[A\x1B[B\r"); // [状態] - 上矢印、下矢印、Enter を入力する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), ">> ", "initial",
                                                           "promptTest.cc", 1); // [手順] - 同じ呼び出し元の履歴で初期値付きの入力を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("initial", buf);    // [確認_正常系] - 退避した初期値へ戻ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// NULL の初期値は、初期値なしの readline と同じであることの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_accepts_null_initial_text)
{
    // Arrange
    char buf[32];

    promptFakeSetInput("xyz\r"); // [状態] - xyz と Enter を入力する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), ">> ", NULL, "promptTest.cc",
                                                           1); // [手順] - 初期値に NULL を指定して 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("xyz", buf);        // [確認_正常系] - 入力した内容だけが返ること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 制御文字を含む初期値を拒否し、入力を読み取らないことの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_rejects_control_character_without_reading)
{
    // Arrange
    char buf[32] = "stale";

    promptFakeSetInput("abc\r"); // [状態] - 後続の読み取りのための入力を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), ">> ", "a\nb", "promptTest.cc",
                                                           1); // [手順] - 改行を含む初期値を指定する。
    int actual_enter_raw_count = promptFakeEnterRawCount();
    int actual_ret_next = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptTest.cc",
                                                   1); // [手順] - 続けて初期値なしで 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - CPLAT_ERR_INVALID_ARGUMENT が返ること。
    EXPECT_EQ(0, actual_enter_raw_count);              // [確認_異常系] - raw モードへ移行しないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_next);              // [確認_異常系] - 後続の読み取りが成功すること。
    EXPECT_STREQ("abc", buf);                          // [確認_異常系] - 入力が消費されずに残っていること。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 入力欄の上限を超える初期値を拒否することの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_rejects_text_over_input_limit)
{
    // Arrange
    cplat_prompt_options options = {};
    char buf[32] = "stale";

    options.input_initial_capacity = 4u;
    options.input_max_bytes = 8u; // [状態] - 入力上限 8 バイトのハンドルを用意する。
    cplat_prompt *handle = cplat_prompt_create(&options);
    ASSERT_NE((cplat_prompt *)NULL, handle); // [状態確認] - ハンドルを生成できること。
    handle->is_tty = 1;

    // Pre-Assert

    // Act
    int actual_ret_fit = cplat_prompt_readline_with_initial_at(handle, buf, sizeof(buf), ">> ", "1234567",
                                                               "promptTest.cc", 1); // [手順] - NUL を含めて 8 バイトの初期値で読み取る (入力は EOF)。
    int actual_ret_over = cplat_prompt_readline_with_initial_at(handle, buf, sizeof(buf), ">> ", "12345678",
                                                                "promptTest.cc", 1); // [手順] - NUL を含めて 9 バイトの初期値を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF, actual_ret_fit);                 // [確認_正常系] - 上限ちょうどの初期値は受け入れられ、入力の EOF が返ること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, actual_ret_over); // [確認_異常系] - 上限を超える初期値が拒否されること。
    EXPECT_STREQ("", buf);                                  // [確認_異常系] - 出力先が空文字列であること。

    // Cleanup
    cplat_prompt_dispose(handle);
}
// [サブ手順参照 名前=promptTest.TearDown]

// TTY でない場合は初期値を使わず、読み取った行を返すことの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_ignores_initial_text_when_not_tty)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char piped[] = "piped";
    char buf[32];

    prompt_->is_tty = 0; // [状態] - TTY でない状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(piped, piped + sizeof(piped)), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_fgets が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets から "piped" を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), NULL, "initial",
                                                           "promptTest.cc", 1); // [手順] - 初期値を指定して非 TTY で読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);         // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("piped", buf);              // [確認_正常系] - 初期値ではなく読み取った行が返ること。
    EXPECT_EQ(0, promptFakeEnterRawCount()); // [確認_正常系] - raw モードへ移行しないこと。
}
// [サブ手順参照 名前=promptTest.TearDown]

// 不正な引数を拒否することの確認
// [サブ手順参照 名前=promptTest.SetUp]
TEST_F(promptTest, readline_with_initial_rejects_invalid_arguments)
{
    // Arrange
    char buf[32];

    // Pre-Assert

    // Act
    int actual_ret_handle = cplat_prompt_readline_with_initial_at(NULL, buf, sizeof(buf), ">> ", "a", "promptTest.cc",
                                                                  1); // [手順] - ハンドルに NULL を指定する。
    int actual_ret_buf = cplat_prompt_readline_with_initial_at(prompt_, NULL, sizeof(buf), ">> ", "a", "promptTest.cc",
                                                               1); // [手順] - 出力先に NULL を指定する。
    int actual_ret_size = cplat_prompt_readline_with_initial_at(prompt_, buf, 0u, ">> ", "a", "promptTest.cc",
                                                                1); // [手順] - 出力先のバイト数に 0 を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_handle); // [確認_異常系] - ハンドル NULL が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_buf);    // [確認_異常系] - 出力先 NULL が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_size);   // [確認_異常系] - バイト数 0 が拒否されること。
}
// [サブ手順参照 名前=promptTest.TearDown]
