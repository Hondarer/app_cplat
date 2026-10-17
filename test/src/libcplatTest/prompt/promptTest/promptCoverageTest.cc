#include <testfw.h>
#include <mock_stdio.h>
#include <mock_cplat.h>
#include <cplat/base/result.h>
#include <cplat/crt/string.h>
#include <cplat/prompt/prompt.h>
#include <cplat/prompt/prompt_internal.h>

#include <cstring>
#include <string>
#include <vector>

#include "prompt.inject.h"
#include "promptPlatformFake.h"

class promptCoverageTest : public Test
{
  protected:
    cplat_prompt *prompt_ = nullptr;

    // [サブ手順 名前=promptCoverageTest.SetUp]
    void SetUp() override
    {
        promptFakeReset();
        prompt_ = cplat_prompt_create(nullptr);
        ASSERT_NE(nullptr, prompt_);
        // [状態確認] - cplat_prompt_create() の戻り値が nullptr 以外であること。
        prompt_->is_tty = 1;
    }
    // [サブ手順終了]

    // [サブ手順 名前=promptCoverageTest.TearDown]
    void TearDown() override
    {
        cplat_prompt_dispose(prompt_);
        prompt_ = nullptr;
    }
    // [サブ手順終了]

    int readline(const std::string &input, char *buf, size_t buf_size, const char *prompt_string = "> ")
    {
        promptFakeSetInput(input);
        return cplat_prompt_readline_at(prompt_, buf, buf_size, prompt_string, "promptCoverageTest.cc", 1);
    }
};

// 改行と Backspace の代替表現を処理することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, readline_accepts_lf_and_ctrl_h)
{
    // Arrange
    char buf[16]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline("ab\x08\n", buf,
                              sizeof(buf)); // [手順] - "ab"、Ctrl+H、LF の順の入力を指定して readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - readline() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);          // [確認_正常系] - buf が "a" であること。(Ctrl+H で末尾を削除したことの確認)
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 未対応の制御入力と数値付きエスケープ シーケンスを無視することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, readline_ignores_remaining_unknown_sequences)
{
    // Arrange
    char buf[16]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = readline(
        "\x01\x1Bx\x1B[1X\x1B[4Xq\n", buf,
        sizeof(
            buf)); // [手順] - 未対応制御文字、未知 ESC、終端が不正な Home と End、"q"、LF の順の入力を指定して readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - readline() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("q", buf);          // [確認_正常系] - buf が "q" であること。(未対応入力が無視されたことの確認)
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 行端で編集キーを押した場合に入力内容を変更しないことの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, readline_ignores_editing_keys_at_line_edges)
{
    // Arrange
    char buf[16]; // [状態] - 空行へ行端用の編集キーを入力する。

    // Pre-Assert

    // Act
    int actual_ret = readline(
        "\x1B[3~\x1B[D\x1B[C\x1B[H\x1B[F\n", buf,
        sizeof(
            buf)); // [手順] - 空行に対する Delete、Left、Right、Home、End、LF の順の入力を指定して readline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - readline() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", buf);           // [確認_正常系] - buf が "" であること。(行端用の編集キーが無視されたことの確認)
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// リサイズ通知後に NULL のプロンプトで入力を継続することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, readline_redisplays_after_resize_with_null_prompt)
{
    // Arrange
    char buf[16];
    const std::vector<int> results = {-2, 'z', '\n'}; // [状態] - リサイズ通知、"z"、LF の入力結果列を用意する。
    promptFakeSetResults(results);

    // Pre-Assert

    // Act
    int actual_ret = cplat_prompt_readline_at(
        prompt_, buf, sizeof(buf), nullptr, "promptCoverageTest.cc",
        2); // [手順] - プロンプトに nullptr を指定し、リサイズ通知後の入力を読み取る状態で cplat_prompt_readline_at() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_prompt_readline_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("z", buf);          // [確認_正常系] - buf が "z" であること。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 非 TTY とコンテキスト確保失敗時の fallback が入力を返すことの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, readline_fallback_paths_return_input)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char first_input[] = "first";
    char second_input[] = "second";
    char first_output[16] = {};
    char second_output[16] = {}; // [状態] - fallback から返す 2 行を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fgets(_, _, _, _))
        .WillOnce(DoAll(SetArrayArgument<0>(first_input, first_input + sizeof(first_input)), Return(CPLAT_OK)))
        .WillOnce(DoAll(SetArrayArgument<0>(second_input, second_input + sizeof(second_input)), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_fgets() が 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_fgets() から、1 回目は "first"、2 回目は "second" を出力引数に設定して CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_realloc() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_realloc() から nullptr を返す。

    // Act
    prompt_->is_tty = 0;
    int actual_ret_non_tty =
        cplat_prompt_readline_at(prompt_, first_output, sizeof(first_output), "> ", "fallback.c", 1);
    prompt_->is_tty = 1;
    int actual_ret_allocation = cplat_prompt_readline_at(
        prompt_, second_output, sizeof(second_output), nullptr, "fallback.c",
        2); // [手順] - 非 TTY と確保失敗の fallback の状態で cplat_prompt_readline_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_non_tty); // [確認_正常系] - 非 TTY の cplat_prompt_readline_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("first", first_output); // [確認_正常系] - first_output が "first" であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_allocation); // [確認_正常系] - コンテキスト確保失敗時の cplat_prompt_readline_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("second", second_output); // [確認_正常系] - second_output が "second" であること。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 呼び出し位置コンテキストの検索と配列拡張を処理することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, contexts_distinguish_file_and_line_and_expand_twice)
{
    // Arrange
    static const char first_file[] = "first.c";
    static const char second_file[] = "second.c";
    cplat_internal_prompt_ctx *contexts[6] = {}; // [状態] - 異なるファイル名と行番号で 6 個のコンテキストを作成する。

    // Pre-Assert

    // Act
    contexts[0] = test_prompt_find_or_create_context(prompt_, first_file, 10);
    contexts[1] = test_prompt_find_or_create_context(prompt_, second_file, 10);
    contexts[2] = test_prompt_find_or_create_context(prompt_, first_file, 11);
    contexts[3] = test_prompt_find_or_create_context(prompt_, first_file, 12);
    contexts[4] = test_prompt_find_or_create_context(prompt_, first_file, 13);
    contexts[5] = test_prompt_find_or_create_context(
        prompt_, first_file,
        10); // [手順] - 最初と同じ呼び出し位置を指定して test_prompt_find_or_create_context() を呼び出す。

    // Assert
    for (size_t i = 0u; i < 5u; i++)
    {
        ASSERT_NE(
            nullptr,
            contexts
                [i]); // [確認_正常系 回数=5] - 新規作成時の test_prompt_find_or_create_context() の戻り値が nullptr 以外であること。
    }
    EXPECT_EQ(
        &prompt_->contexts[0],
        contexts
            [5]); // [確認_正常系] - 再検索時の test_prompt_find_or_create_context() の戻り値が &prompt_->contexts[0] と一致すること。
    EXPECT_EQ(5u, prompt_->ctx_count); // [確認_正常系] - prompt_->ctx_count が 5 であること。
    EXPECT_EQ(8u, prompt_->ctx_cap);   // [確認_正常系] - prompt_->ctx_cap が 8 であること。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 履歴の重複、NULL エントリ、前後端、容量制限を処理することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, history_helpers_cover_remaining_boundaries)
{
    // Arrange
    cplat_internal_prompt_ctx *context =
        test_prompt_find_or_create_context(prompt_, "history.c", 1); // [状態] - 空の履歴コンテキストを用意する。
    cplat_internal_prompt_ctx *null_entry_context = test_prompt_find_or_create_context(
        prompt_, "null-entry.c", 1); // [状態] - nullptr エントリ試験用の履歴コンテキストを用意する。
    char *first_entry = nullptr;
    ASSERT_NE(nullptr,
              context); // [状態確認] - test_prompt_find_or_create_context() の戻り値が nullptr 以外であること。
    ASSERT_NE(
        nullptr,
        null_entry_context); // [状態確認] - nullptr エントリ試験用の test_prompt_find_or_create_context() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    test_prompt_history_next(prompt_, context, nullptr);
    test_prompt_history_add(prompt_, context, "");
    test_prompt_history_add(prompt_, context, "first");
    test_prompt_history_add(prompt_, context, "first");
    null_entry_context->count = 1u;
    null_entry_context->entries[0] = nullptr;
    test_prompt_history_add(prompt_, null_entry_context, "second");
    first_entry = context->entries[0];
    context->entries[0] = nullptr;
    test_prompt_history_prev(prompt_, context, nullptr);
    context->entries[0] = first_entry;
    prompt_->edit_cap = 4u;
    prompt_->input_max_bytes = 4u;
    context->browse_idx = -1;
    test_prompt_history_prev(prompt_, context, nullptr);
    (void)cplat_strcpy(context->saved_line, CPLAT_PROMPT_INPUT_BYTES_DEFAULT, "saved");
    context->browse_idx = 0;
    test_prompt_history_next(prompt_, context, nullptr);
    context->count = 2u;
    context->browse_idx = 0;
    context->entries[1] = nullptr;
    test_prompt_history_next(
        prompt_, context,
        nullptr); // [手順] - 履歴の重複、nullptr エントリ、容量制限、前後端を扱う状態で test_prompt_history_next() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        context
            ->browse_idx); // [確認_正常系] - context->browse_idx が 1 であること。(nullptr の新しい履歴位置まで進んだことの確認)
    EXPECT_STREQ(
        "sav",
        prompt_
            ->edit_buf); // [確認_正常系] - prompt_->edit_buf が "sav" であること。(容量制限により退避文字列が切り詰められたことの確認)
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 履歴上限が 0 の場合に、剰余を取らずに履歴を追加しないことの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, history_add_ignores_zero_history_max)
{
    // Arrange
    cplat_internal_prompt_ctx *context =
        test_prompt_find_or_create_context(prompt_, "zero-history.c", 1); // [状態] - 空の履歴コンテキストを用意する。
    ASSERT_NE(nullptr,
              context); // [状態確認] - test_prompt_find_or_create_context() の戻り値が nullptr 以外であること。
    size_t saved_history_max = prompt_->history_max;
    prompt_->history_max = 0u; // [状態] - 履歴上限を 0 にする。

    // Pre-Assert

    // Act
    test_prompt_history_add(prompt_, context,
                            "line"); // [手順] - 履歴上限が 0 の状態で test_prompt_history_add() を呼び出す。

    // Assert
    EXPECT_EQ(0u, context->count); // [確認_正常系] - context->count が 0 であること。(履歴の件数が変わらないことの確認)
    EXPECT_EQ(0u,
              context->head); // [確認_正常系] - context->head が 0 であること。(最古の履歴位置が変わらないことの確認)

    prompt_->history_max = saved_history_max; // 後始末で履歴エントリ配列を走査できるよう、履歴上限を戻す。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 履歴エントリ配列の確保失敗をコンテキスト作成失敗として扱うことの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, context_creation_fails_when_entries_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat; // [状態] - calloc の失敗を注入する mock を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - cplat_calloc() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    cplat_internal_prompt_ctx *context =
        test_prompt_find_or_create_context(prompt_, "failure.c",
                                           1); // [手順] - test_prompt_find_or_create_context() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              context); // [確認_異常系] - test_prompt_find_or_create_context() の戻り値が nullptr であること。
    EXPECT_EQ(
        0u,
        prompt_
            ->ctx_count); // [確認_異常系] - prompt_->ctx_count が 0 であること。(失敗したコンテキストが件数へ加算されないことの確認)
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 書式生成の NULL、エンコード エラー、バッファー拡張を処理することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, readline_fmt_handles_null_error_and_growth)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char buf[16];
    std::string long_prompt(300u, 'p'); // [状態] - nullptr 書式、書式エラー、初期容量を超える書式を試験する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vsnprintf(_, _, _, _, _, _)).WillOnce(Return(-1)).WillRepeatedly(DoDefault());
    // [Pre-Assert確認_異常系] - vsnprintf() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - vsnprintf() から 1 回目は -1 を返し、以降は libc の処理へ委譲する。

    // Act
    promptFakeSetInput("a\n");
    int actual_ret_error = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "format.c", 1, "%s", "x");
    promptFakeSetInput("b\n");
    int actual_ret_null = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "format.c", 2, nullptr);
    promptFakeSetInput("c\n");
    int actual_ret_growth = cplat_prompt_readline_fmt_at(
        prompt_, buf, sizeof(buf), "format.c", 3, "%s",
        long_prompt.c_str()); // [手順] - 長い書式を指定して cplat_prompt_readline_fmt_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_error); // [確認_正常系] - 書式エラー時の cplat_prompt_readline_fmt_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_null); // [確認_正常系] - nullptr 書式の cplat_prompt_readline_fmt_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_growth); // [確認_正常系] - 長い書式の cplat_prompt_readline_fmt_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("c", buf); // [確認_正常系] - buf が "c" であること。
    EXPECT_EQ(
        301u,
        prompt_
            ->prompt_fmt_cap); // [確認_正常系] - prompt_->prompt_fmt_cap が 301 であること。(終端を含む書式バッファーの容量であることの確認)
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]
