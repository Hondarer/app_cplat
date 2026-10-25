#include <testfw.h>
#include <string>
#include <vector>
#include "prompt_platform.h"
#include <mock_cplat.h>
#include <cplat/prompt/pinned_prompt.h>
#include "pinned_prompt.inject.h"

// キー解析の各段階の OS エラーが公開入力関数まで伝わることの確認
TEST(pinnedFailureTest, escape_sequence_errors_reach_caller)
{
    // Arrange
    // Pre-Assert
    // Act / Assert
    const std::vector<int> inputs[] = {
        {-3}, {27, -3}, {27, '[', -3}, {27, '[', '1', -3}, {27, '[', '3', -3}, {27, '[', '4', -3}};
    for (const auto &input : inputs)
    {
        cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
        ASSERT_NE(nullptr, screen); // [状態確認] - ハンドルを確保できたこと。6 件。
        test_pinned_prompt_set_tty(screen, 1);
        test_pinned_prompt_failures(2, 0);
        test_pinned_prompt_input_results(input.data(), input.size());
        char output[16] = {};
        int result = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                                  nullptr); // [手順] - キーの各段階を失敗させる。6 回実施。
        EXPECT_EQ(CPLAT_ERR_UNKNOWN, result);               // [確認_異常系] - 入力エラーを EOF と区別すること。6 件。
        cplat_pinned_prompt_dispose(screen);
    }
    test_pinned_prompt_failures(0, 0);
    test_pinned_prompt_input_results(nullptr, 0);
}

// 移行失敗と復元未完を分類することの確認
TEST(pinnedFailureTest, acquisition_failure_requires_restoration_before_fallback)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
    ASSERT_NE(nullptr, screen); // [状態確認] - ハンドルを確保できたこと。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_failures(1, 0);
    char output[16] = {};
    // Pre-Assert
    EXPECT_CALL(mock, cplat_fgets(_, _, _, _)).WillOnce(Return(CPLAT_ERR_EOF));
    // [Pre-Assert確認_正常系] - 復元済みの移行失敗だけ行入力を 1 回呼ぶこと。
    // Act
    int fallback = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                                nullptr); // [手順] - 切替可能な移行失敗を注入する。
    test_pinned_prompt_failures(-1, -1);
    int unrestored =
        cplat_pinned_prompt_readline(screen, output, sizeof(output), nullptr); // [手順] - 復元未完を注入する。
    // Assert
    EXPECT_EQ(CPLAT_ERR_EOF, fallback);       // [確認_正常系] - 行入力の結果が返ること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unrestored); // [確認_異常系] - 復元失敗は行入力へ切り替えないこと。
    // Cleanup
    cplat_pinned_prompt_dispose(screen);
    test_pinned_prompt_failures(0, 0);
}

// 入力成功、EOF、キャンセルより復元失敗を優先することの確認
TEST(pinnedFailureTest, restoration_failure_overrides_input_results)
{
    // Arrange
    const int inputs[] = {'\n', -1, 3};
    // Pre-Assert
    // Act / Assert
    for (int input : inputs)
    {
        cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
        ASSERT_NE(nullptr, screen); // [状態確認] - ハンドルを確保できたこと。3 件。
        test_pinned_prompt_set_tty(screen, 1);
        test_pinned_prompt_failures(2, -1);
        test_pinned_prompt_input_results(&input, 1);
        char output[16] = {};
        int result = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                                  nullptr); // [手順] - 各入力終了後の復元を失敗させる。3 回実施。
        EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - 復元失敗が入力結果より優先されること。3 件。
        cplat_pinned_prompt_dispose(screen);
    }
    test_pinned_prompt_failures(0, 0);
    test_pinned_prompt_input_results(nullptr, 0);
}

// メモリ不足や行入力への切替よりも端末復元失敗を優先することの確認
TEST(pinnedFailureTest, restoration_failure_overrides_allocation_and_fallback)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
    ASSERT_NE(nullptr, screen); // [状態確認] - ハンドルを確保できたこと。
    char output[16] = {};
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_failures(2, 0);
    const int newline = '\n';
    test_pinned_prompt_input_results(&newline, 1);
    ASSERT_EQ(CPLAT_OK, cplat_pinned_prompt_readline_at(screen, output, sizeof(output), nullptr, "restore-oom", 1));
    // [状態確認] - 同じ入力位置の履歴とプロンプトを確保したこと。
    std::string initial(300U, 'x');
    NiceMock<Mock_cplat> mock;
    test_pinned_prompt_failures(2, -1);
    // Pre-Assert
    EXPECT_CALL(mock, cplat_realloc(_, _, _)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock, cplat_fgets(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 初期値の拡張を 1 回失敗させ、復元未完では行入力を呼ばないこと。
    // Act
    int allocation = cplat_pinned_prompt_readline_with_initial_at(screen, output, sizeof(output), nullptr,
                                                                  initial.c_str(), "restore-oom", 1);
    // [手順] - 編集バッファーの確保と復元を失敗させる。
    test_pinned_prompt_set_tty(screen, 0);
    int fallback = cplat_pinned_prompt_readline(screen, output, sizeof(output),
                                                nullptr); // [手順] - 非端末入力への切替前の復元を失敗させる。
    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, allocation); // [確認_異常系] - 復元失敗がメモリ不足より優先されること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, fallback);   // [確認_異常系] - 復元未完では行入力へ切り替えないこと。
    // Cleanup
    cplat_pinned_prompt_dispose(screen);
    test_pinned_prompt_failures(0, 0);
    test_pinned_prompt_input_results(nullptr, 0);
}

// ESC 直後の EOF と入力中のリサイズ通知を入力エラーと区別することの確認
TEST(pinnedFailureTest, eof_after_escape_and_resize_are_not_input_errors)
{
    // Arrange
    const std::vector<int> inputs[] = {{27, PROMPT_PLATFORM_EOF}, {PROMPT_PLATFORM_RESIZE, '\n'}};
    const int expected[] = {CPLAT_ERR_EOF, CPLAT_OK};
    // Pre-Assert
    // Act / Assert
    for (size_t i = 0; i < 2; ++i)
    {
        cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
        ASSERT_NE(nullptr, screen); // [状態確認] - ハンドルを確保できたこと。2 件。
        test_pinned_prompt_set_tty(screen, 1);
        test_pinned_prompt_failures(2, 0);
        test_pinned_prompt_input_results(inputs[i].data(), inputs[i].size());
        char output[16] = {};
        int result = cplat_pinned_prompt_readline(screen, output, sizeof(output), nullptr);
        // [手順] - ESC 途中の EOF と、入力中のリサイズ通知を与える。2 回実施。
        EXPECT_EQ(expected[i], result); // [確認_正常系] - EOF は入力終端、リサイズ後は入力成功を返すこと。2 件。
        cplat_pinned_prompt_dispose(screen);
    }
    test_pinned_prompt_failures(0, 0);
    test_pinned_prompt_input_results(nullptr, 0);
}
