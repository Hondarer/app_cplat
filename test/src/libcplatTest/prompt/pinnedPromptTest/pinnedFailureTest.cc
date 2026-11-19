#include <testfw.h>
#include <string>
#include <vector>
#include "prompt_platform.h"
#if defined(PLATFORM_LINUX)
    #include <mock_poll.h>
    #include <mock_unistd.h>
    #include <poll.h>
    #include <unistd.h>
#endif
#include <mock_cplat.h>
#include <mock_stdio.h>
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
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
    ASSERT_NE(nullptr, screen); // [状態確認] - ハンドルを確保できたこと。
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_failures(1, 0);
    char output[16] = {};
    // Pre-Assert
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_poll> poll_mock;
    NiceMock<Mock_unistd> unistd_mock;
    EXPECT_CALL(poll_mock, poll(_, _, _, _, _, -1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLHUP;
                return 1;
            }));
    EXPECT_CALL(unistd_mock, read(_, _, _, STDIN_FILENO, _, _)).Times(0);
#else
    NiceMock<Mock_cplat> mock;
    EXPECT_CALL(mock, cplat_fgets(_, _, _, _)).WillOnce(Return(CPLAT_ERR_EOF));
#endif
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
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_poll> poll_mock;
    EXPECT_CALL(poll_mock, poll(_, _, _, _, _, _)).Times(0);
#else
    EXPECT_CALL(mock, cplat_fgets(_, _, _, _)).Times(0);
#endif
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

// 入力中止要求後も端末復元失敗を最優先し、正常時は永続中止と空出力を保つことの確認
TEST(pinnedFailureTest, stop_input_prioritizes_restoration_failure_and_persists_canceled_state)
{
    // Arrange
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_pinned_prompt_stop_input(nullptr));

    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
    ASSERT_NE(nullptr, screen);
    test_pinned_prompt_set_tty(screen, 1);
    test_pinned_prompt_failures(2, -1);
    const int inputs[] = {'a', PROMPT_PLATFORM_CANCELED};
    test_pinned_prompt_input_results(inputs, 2);
    char output[16] = "stale";

    // Act / Assert: 中止要求が成立していても、raw 復元失敗時は CPLAT_ERR_UNKNOWN を優先する
    EXPECT_EQ(CPLAT_OK, cplat_pinned_prompt_stop_input(screen));
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_pinned_prompt_readline(screen, output, sizeof(output), nullptr));

    // Act / Assert: 復元が成功しても永続中止状態が残り、以降は入力待ちせず CPLAT_ERR_CANCELED と空文字を返す
    test_pinned_prompt_failures(2, 0);
    EXPECT_EQ(CPLAT_ERR_CANCELED, cplat_pinned_prompt_readline(screen, output, sizeof(output), nullptr));
    EXPECT_STREQ("", output);

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
    test_pinned_prompt_failures(0, 0);
    test_pinned_prompt_input_results(nullptr, 0);
}

// 端末でない経路で、出力のフラッシュに失敗した場合に CPLAT_ERR_UNKNOWN を返すことの確認
TEST(pinnedFailureTest, write_reports_flush_failure_without_tty)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
    ASSERT_NE(nullptr, screen);             // [状態確認] - ハンドルを確保できたこと。
    test_pinned_prompt_set_tty(screen, 0); // [状態] - 端末でない経路を選択する。
    NiceMock<Mock_stdio> mock_stdio;
    const char data[] = "abc";
    size_t written = 0U;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, 1U, 3U, stdout))
        .WillOnce(Return(3U)); // [Pre-Assert確認_異常系] - fwrite() が stdout へ 3 バイトを指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - fwrite() から 3 を返す。
    EXPECT_CALL(mock_stdio, fflush(_, _, _, stdout))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stdout を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    int result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
                                           &written); // [手順] - フラッシュが失敗する状態で cplat_pinned_prompt_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_pinned_prompt_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(3U, written); // [確認_異常系] - written が fwrite() の戻り値 3 と等しいこと。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}

// 端末の経路で、出力のフラッシュに失敗した場合に CPLAT_ERR_UNKNOWN を返し、ロックを解除することの確認
TEST(pinnedFailureTest, write_reports_flush_failure_with_tty)
{
    // Arrange
    cplat_pinned_prompt *screen = cplat_pinned_prompt_create(nullptr);
    ASSERT_NE(nullptr, screen);             // [状態確認] - ハンドルを確保できたこと。
    test_pinned_prompt_set_tty(screen, 1); // [状態] - 端末の経路を選択する。
    NiceMock<Mock_stdio> mock_stdio;
    ON_CALL(mock_stdio, printf(_, _, _, _)).WillByDefault(Return(0)); // [状態] - 端末制御の printf() から 0 を返す。
    ON_CALL(mock_stdio, fwrite(_, _, _, _, _, _, _)).WillByDefault(ReturnArg<5>()); // [状態] - fwrite() から要素数を返す。
    const char data[] = "abc";
    size_t written = 0U;
    size_t second_written = 0U;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, static_cast<const void *>(data), 1U, 3U, stdout))
        .WillOnce(Return(3U)); // [Pre-Assert確認_異常系] - fwrite() が stdout へ 3 バイトを指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - fwrite() から 3 を返す。
    EXPECT_CALL(mock_stdio, fflush(_, _, _, _))
        .WillRepeatedly(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が呼び出されること。
                                      // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    int result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, data, 3U,
                                           &written); // [手順] - フラッシュが失敗する状態で cplat_pinned_prompt_write() を呼び出す。
    int second_result = cplat_pinned_prompt_write(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, nullptr, 0U,
                                                  &second_written); // [手順] - 続けて 0 バイトの cplat_pinned_prompt_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - 1 回目の cplat_pinned_prompt_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(3U, written); // [確認_異常系] - written が fwrite() の戻り値 3 と等しいこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, second_result); // [確認_異常系] - 2 回目の cplat_pinned_prompt_write() が戻り、戻り値が CPLAT_ERR_UNKNOWN であること。(ロックが解除されていることの確認)
    EXPECT_EQ(0U, second_written); // [確認_異常系] - second_written が 0 であること。

    // Cleanup
    cplat_pinned_prompt_dispose(screen);
}