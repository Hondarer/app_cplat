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
    cplat_prompt *prompt_ = NULL;

    // [サブ手順 名前=promptCoverageTest.SetUp]
    void SetUp() override
    {
        promptFakeReset();
        prompt_ = cplat_prompt_create(NULL);
        ASSERT_NE((cplat_prompt *)NULL, prompt_);
        // [状態確認] - `(cplat_prompt *)NULL` と `prompt_` が異なること。
        prompt_->is_tty = 1;
    }
    // [サブ手順終了]

    // [サブ手順 名前=promptCoverageTest.TearDown]
    void TearDown() override
    {
        cplat_prompt_dispose(prompt_);
        prompt_ = NULL;
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
                       sizeof(buf)); // [手順] - "ab"、Ctrl+H、LF の順に入力して 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_prompt_readline_at の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", buf);     // [確認_正常系] - Ctrl+H で末尾を削除した "a" が返ること。
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
    int actual_ret = readline("\x01\x1Bx\x1B[1X\x1B[4Xq\n", buf,
                       sizeof(buf)); // [手順] - 未対応制御文字、未知 ESC、終端が不正な Home と End、"q"、LF を入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_prompt_readline_at の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("q", buf);     // [確認_正常系] - 未対応入力が無視されて "q" が返ること。
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
    int actual_ret = readline("\x1B[3~\x1B[D\x1B[C\x1B[H\x1B[F\n", buf,
                       sizeof(buf)); // [手順] - 空行で Delete、Left、Right、Home、End、LF の順に入力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_prompt_readline_at の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", buf);      // [確認_正常系] - 行端用の編集キーが無視されて空行が返ること。
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
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), NULL, "promptCoverageTest.cc",
                                          2); // [手順] - NULL のプロンプトでリサイズ通知後の入力を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_prompt_readline_at の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("z", buf);     // [確認_正常系] - リサイズ通知後に入力した "z" が返ること。
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
    // [Pre-Assert確認_正常系] - cplat_fgets が非 TTY とコンテキスト確保失敗の各経路で呼び出されること。
    // [Pre-Assert手順] - cplat_fgets が 1 回目に "first"、2 回目に "second" を返却する。
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - コンテキスト配列用の realloc が 1 回呼び出されること。
    // [Pre-Assert手順] - realloc から NULL を返却する。

    // Act
    prompt_->is_tty = 0;
    int actual_ret_non_tty = cplat_prompt_readline_at(prompt_, first_output, sizeof(first_output), "> ", "fallback.c", 1);
    prompt_->is_tty = 1;
    int actual_ret_allocation = cplat_prompt_readline_at(prompt_, second_output, sizeof(second_output), NULL,
                                                     "fallback.c", 2); // [手順] - 非 TTY と確保失敗の fallback で入力を読む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_non_tty); // [確認_正常系] - 非 TTY の cplat_prompt_readline_at が CPLAT_OK を返すこと。
    EXPECT_STREQ("first", first_output); // [確認_正常系] - 非 TTY の入力結果が "first" であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_allocation); // [確認_正常系] - コンテキスト確保失敗時の cplat_prompt_readline_at が CPLAT_OK を返すこと。
    EXPECT_STREQ("second", second_output); // [確認_正常系] - コンテキスト確保失敗時の入力結果が "second" であること。
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
    contexts[5] = test_prompt_find_or_create_context(prompt_, first_file,
                                                     10); // [手順] - 5 個を新規作成し、最初の呼び出し位置を再検索する。

    // Assert
    for (size_t i = 0u; i < 5u; i++)
    {
        ASSERT_NE((cplat_internal_prompt_ctx *)NULL,
                  contexts[i]); // [確認_正常系 回数=5] - 5 個の新規コンテキストが取得できること。
    }
    EXPECT_EQ(&prompt_->contexts[0], contexts[5]); // [確認_正常系] - 同じファイルと行番号から既存コンテキストが返ること。
    EXPECT_EQ(5u, prompt_->ctx_count);   // [確認_正常系] - コンテキスト数が 5 であること。
    EXPECT_EQ(8u, prompt_->ctx_cap);     // [確認_正常系] - コンテキスト配列の容量が 8 へ拡張されること。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 履歴の重複、NULL エントリ、前後端、容量制限を処理することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, history_helpers_cover_remaining_boundaries)
{
    // Arrange
    cplat_internal_prompt_ctx *context =
        test_prompt_find_or_create_context(prompt_, "history.c", 1); // [状態] - 空の履歴コンテキストを用意する。
    cplat_internal_prompt_ctx *null_entry_context =
        test_prompt_find_or_create_context(prompt_, "null-entry.c", 1); // [状態] - NULL エントリ試験用の履歴コンテキストを用意する。
    char *first_entry = NULL;
    ASSERT_NE((cplat_internal_prompt_ctx *)NULL, context);             // [状態確認] - コンテキストが非 NULL であること。
    ASSERT_NE((cplat_internal_prompt_ctx *)NULL, null_entry_context); // [状態確認] - NULL エントリ試験用コンテキストが非 NULL であること。

    // Pre-Assert

    // Act
    test_prompt_history_next(prompt_, context, NULL);
    test_prompt_history_add(prompt_, context, "");
    test_prompt_history_add(prompt_, context, "first");
    test_prompt_history_add(prompt_, context, "first");
    null_entry_context->count = 1u;
    null_entry_context->entries[0] = NULL;
    test_prompt_history_add(prompt_, null_entry_context, "second");
    first_entry = context->entries[0];
    context->entries[0] = NULL;
    test_prompt_history_prev(prompt_, context, NULL);
    context->entries[0] = first_entry;
    prompt_->edit_cap = 4u;
    prompt_->input_max_bytes = 4u;
    context->browse_idx = -1;
    test_prompt_history_prev(prompt_, context, NULL);
    (void)cplat_strcpy(context->saved_line, CPLAT_PROMPT_INPUT_BYTES_DEFAULT, "saved");
    context->browse_idx = 0;
    test_prompt_history_next(prompt_, context, NULL);
    context->count = 2u;
    context->browse_idx = 0;
    context->entries[1] = NULL;
    test_prompt_history_next(prompt_, context,
                             NULL); // [手順] - 重複、NULL エントリ、容量制限、前後端の履歴操作を行う。

    // Assert
    EXPECT_EQ(1, context->browse_idx); // [確認_正常系] - NULL の新しい履歴位置まで browse index が進むこと。
    EXPECT_STREQ("sav", prompt_->edit_buf); // [確認_正常系] - 容量制限により退避文字列が "sav" へ切り詰められること。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 履歴上限が 0 の場合に、剰余を取らずに履歴を追加しないことの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, history_add_ignores_zero_history_max)
{
    // Arrange
    cplat_internal_prompt_ctx *context =
        test_prompt_find_or_create_context(prompt_, "zero-history.c", 1); // [状態] - 空の履歴コンテキストを用意する。
    ASSERT_NE((cplat_internal_prompt_ctx *)NULL, context);                         // [状態確認] - コンテキストが非 NULL であること。
    size_t saved_history_max = prompt_->history_max;
    prompt_->history_max = 0u; // [状態] - 履歴上限を 0 にする。

    // Pre-Assert

    // Act
    test_prompt_history_add(prompt_, context, "line"); // [手順] - 履歴上限 0 で履歴を追加する。

    // Assert
    EXPECT_EQ(0u, context->count); // [確認_正常系] - 履歴の件数が 0 のままであること。
    EXPECT_EQ(0u, context->head);  // [確認_正常系] - 最古の履歴位置が 0 のままであること。

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
    // [Pre-Assert確認_異常系] - 履歴エントリ配列用の cplat_calloc が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_calloc から NULL を返却する。

    // Act
    cplat_internal_prompt_ctx *context = test_prompt_find_or_create_context(prompt_, "failure.c",
                                                                      1); // [手順] - 履歴コンテキストを作成する。

    // Assert
    EXPECT_EQ((cplat_internal_prompt_ctx *)NULL,
              context); // [確認_異常系] - test_prompt_find_or_create_context の戻り値が NULL であること。
    EXPECT_EQ(0u, prompt_->ctx_count); // [確認_異常系] - 失敗したコンテキストが件数へ加算されないこと。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]

// 書式生成の NULL、エンコード エラー、バッファー拡張を処理することの確認
// [サブ手順参照 名前=promptCoverageTest.SetUp]
TEST_F(promptCoverageTest, readline_fmt_handles_null_error_and_growth)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char buf[16];
    std::string long_prompt(300u, 'p'); // [状態] - NULL 書式、書式エラー、初期容量を超える書式を試験する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vsnprintf(_, _, _, _, _, _))
        .WillOnce(Return(-1))
        .WillRepeatedly(DoDefault());
    // [Pre-Assert確認_異常系] - 1 回目の vsnprintf がエンコード エラーを返すこと。
    // [Pre-Assert手順] - 1 回目は -1 を返却し、以降は既定動作を行う。

    // Act
    promptFakeSetInput("a\n");
    int actual_ret_error = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "format.c", 1, "%s", "x");
    promptFakeSetInput("b\n");
    int actual_ret_null = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "format.c", 2, NULL);
    promptFakeSetInput("c\n");
    int actual_ret_growth = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "format.c", 3, "%s",
                                                     long_prompt.c_str()); // [手順] - 3 種類の書式で 1 行ずつ読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_error); // [確認_正常系] - 書式エラー時の cplat_prompt_readline_fmt_at が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_null); // [確認_正常系] - NULL 書式の cplat_prompt_readline_fmt_at が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_growth); // [確認_正常系] - 長い書式の cplat_prompt_readline_fmt_at が CPLAT_OK を返すこと。
    EXPECT_STREQ("c", buf);              // [確認_正常系] - 最後に入力した "c" が返ること。
    EXPECT_EQ(301u, prompt_->prompt_fmt_cap); // [確認_正常系] - 書式バッファーの容量が終端を含む 301 であること。
}
// [サブ手順参照 名前=promptCoverageTest.TearDown]
