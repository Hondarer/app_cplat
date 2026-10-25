#include <testfw.h>
#include <string>
#include <mock_cplat.h>
#include "prompt_state.h"
#include "promptPlatformFake.h"
#include <vector>

// 端末入力と復元の失敗が EOF や成功と区別されることの確認
TEST(promptFailureTest, input_errors_and_restore_failures_are_reported)
{
    // Arrange
    const std::vector<int> inputs[] = {{PROMPT_PLATFORM_ERROR},
                                       {27, PROMPT_PLATFORM_ERROR},
                                       {27, '[', PROMPT_PLATFORM_ERROR},
                                       {27, '[', '1', PROMPT_PLATFORM_ERROR},
                                       {27, '[', '4', PROMPT_PLATFORM_ERROR},
                                       {27, '[', '3', PROMPT_PLATFORM_ERROR},
                                       {'\n'},
                                       {PROMPT_PLATFORM_EOF},
                                       {3}};
    // Pre-Assert
    // Act / Assert
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
    {
        promptFakeReset();
        cplat_prompt *prompt = cplat_prompt_create(nullptr);
        ASSERT_NE(nullptr, prompt); // [状態確認] - ハンドルを確保できたこと。9 件。
        prompt->is_tty = 1;
        promptFakeSetResults(inputs[i]);
        if (i >= 6U)
        {
            promptFakeSetFailures(0, -1);
        }
        char output[16] = {};
        int result = cplat_prompt_readline(prompt, output, sizeof(output),
                                           nullptr); // [手順] - 入力または復元を失敗させる。9 回実施。
        EXPECT_EQ(CPLAT_ERR_UNKNOWN, result);        // [確認_異常系] - 全経路で端末処理の失敗が通知されること。9 件。
        cplat_prompt_dispose(prompt);
    }
    promptFakeReset();
}

// 移行失敗を行入力へ切り替え、復元未完なら切り替えないことの確認
TEST(promptFailureTest, acquisition_failure_only_falls_back_after_restoration)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    promptFakeReset();
    cplat_prompt *prompt = cplat_prompt_create(nullptr);
    ASSERT_NE(nullptr, prompt); // [状態確認] - ハンドルを確保できたこと。
    prompt->is_tty = 1;
    promptFakeSetFailures(1, 0);
    char output[16] = {};
    // Pre-Assert
    EXPECT_CALL(mock, cplat_fgets(_, _, _, _)).WillOnce(Return(CPLAT_ERR_EOF));
    // [Pre-Assert確認_正常系] - 復元済みの移行失敗だけ行入力を 1 回呼ぶこと。
    // Act
    int fallback =
        cplat_prompt_readline(prompt, output, sizeof(output), nullptr); // [手順] - 切替可能な移行失敗を注入する。
    promptFakeSetFailures(-1, -1);
    int unrestored = cplat_prompt_readline(prompt, output, sizeof(output), nullptr); // [手順] - 復元未完を注入する。
    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF, fallback);       // [確認_正常系] - 行入力の結果が返ること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unrestored); // [確認_異常系] - 復元未完は行入力へ切り替えないこと。
    // Cleanup
    cplat_prompt_dispose(prompt);
    promptFakeReset();
}

// 行入力への切替前にも復元未完を通知することの確認
TEST(promptFailureTest, fallback_does_not_ignore_unrestored_state)
{
    // Arrange
    promptFakeReset();
    cplat_prompt *prompt = cplat_prompt_create(nullptr);
    ASSERT_NE(nullptr, prompt); // [状態確認] - ハンドルを確保できたこと。
    NiceMock<Mock_cplat> mock;
    promptFakeSetFailures(0, -1);
    char output[16] = {};
    // Pre-Assert
    EXPECT_CALL(mock, cplat_fgets(_, _, _, _)).Times(0);
    EXPECT_CALL(mock, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - 履歴配列の確保を 1 回失敗させ、復元未完では行入力を呼ばないこと。
    // Act
    prompt->is_tty = 0;
    int redirected = cplat_prompt_readline(prompt, output, sizeof(output),
                                           nullptr); // [手順] - 非端末入力への切替前の復元を失敗させる。
    prompt->is_tty = 1;
    int allocation =
        cplat_prompt_readline(prompt, output, sizeof(output), nullptr); // [手順] - 履歴確保失敗後の復元も失敗させる。
    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, redirected); // [確認_異常系] - 非端末でも復元未完を通知すること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, allocation); // [確認_異常系] - 履歴確保失敗でも復元未完を通知すること。
    // Cleanup
    cplat_prompt_dispose(prompt);
    promptFakeReset();
}

// 追加入力待ちのタイムアウトを入力エラーと扱わないことの確認
TEST(promptFailureTest, escape_timeout_clears_line_without_error)
{
    // Arrange
    promptFakeReset();
    cplat_prompt *prompt = cplat_prompt_create(nullptr);
    ASSERT_NE(nullptr, prompt); // [状態確認] - ハンドルを確保できたこと。
    prompt->is_tty = 1;
    promptFakeSetResults({'a', 27, PROMPT_PLATFORM_TIMEOUT, '\n'});
    char output[16] = {};
    // Pre-Assert
    // Act
    int result =
        cplat_prompt_readline(prompt, output, sizeof(output), nullptr); // [手順] - 単独 ESC の待機終了を注入する。
    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - 入力を継続して確定できること。
    EXPECT_STREQ("", output);    // [確認_正常系] - 単独 ESC で行を消去すること。
    // Cleanup
    cplat_prompt_dispose(prompt);
    promptFakeReset();
}

// メモリ不足よりも端末復元失敗を優先することの確認
TEST(promptFailureTest, restoration_failure_overrides_initial_buffer_allocation_failure)
{
    // Arrange
    promptFakeReset();
    cplat_prompt *prompt = cplat_prompt_create(nullptr);
    ASSERT_NE(nullptr, prompt); // [状態確認] - ハンドルを確保できたこと。
    prompt->is_tty = 1;
    char output[16] = {};
    promptFakeSetInput("\n");
    ASSERT_EQ(CPLAT_OK, cplat_prompt_readline_at(prompt, output, sizeof(output), nullptr, "restore-oom", 1));
    // [状態確認] - 同じ入力位置の履歴を確保したこと。
    std::string initial(300U, 'x');
    NiceMock<Mock_cplat> mock;
    promptFakeSetFailures(0, -1);
    // Pre-Assert
    EXPECT_CALL(mock, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - 初期値用の編集バッファー拡張を 1 回失敗させること。
    // Act
    int result = cplat_prompt_readline_with_initial_at(prompt, output, sizeof(output), nullptr, initial.c_str(),
                                                       "restore-oom", 1);
    // [手順] - 編集バッファーの確保と端末復元を失敗させる。
    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - 復元失敗がメモリ不足より優先されること。
    // Cleanup
    cplat_prompt_dispose(prompt);
    promptFakeReset();
}
