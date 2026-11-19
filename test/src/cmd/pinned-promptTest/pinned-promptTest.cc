#include <testfw.h>
#include <mock_stdio.h>
#include <mock_cplat.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include "pinned-prompt.inject.h"

class pinned_promptTest : public Test
{
  protected:
    pinned_promptTest()
    {
        // pinned-prompt.c の単体テストでは shutdown.c を対象外とし、登録 API をフェイクする。
        ON_CALL(mock_cplat_, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
    }

    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;
    cplat_pinned_prompt *screen_ = reinterpret_cast<cplat_pinned_prompt *>(static_cast<uintptr_t>(0x1234));
};
// --help の使い方の表示の関数が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(pinned_promptTest, main_returns_failure_when_help_usage_print_fails)
{
    // Arrange
    const char *argv[] = {"pinned-prompt", "--help"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_argparser_print_usage(stdout))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_argparser_print_usage() が stdout を指定して 1 回呼び出されること。
                                              // [Pre-Assert手順] - cplat_argparser_print_usage() から CPLAT_ERR_UNKNOWN を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - 使い方の表示が失敗する状態で --help を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// --help の後の fflush(stdout) が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(pinned_promptTest, main_returns_failure_when_help_flush_fails)
{
    // Arrange
    const char *argv[] = {"pinned-prompt", "--help"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, fflush(_, _, _, stdout))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stdout を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - フラッシュが失敗する状態で --help を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// --help の後の ferror(stdout) がエラーを示した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(pinned_promptTest, main_returns_failure_when_help_stream_has_error)
{
    // Arrange
    const char *argv[] = {"pinned-prompt", "--help"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, ferror(_, _, _, stdout))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - ferror() が stdout を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror() から 1 を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - ストリームのエラーがある状態で --help を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// 待機が成功とタイムアウト以外の結果を返した場合に、周期の処理を止めて標準エラー出力のチャネルへ伝えることの確認
TEST_F(pinned_promptTest, worker_stops_when_timed_wait_fails)
{
    // Arrange
    int thread_running = -1;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_condvar_timed_wait(_, _, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_condvar_timed_wait() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - cplat_condvar_timed_wait() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat_, cplat_pinned_prompt_printf(screen_, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                                        HasSubstr("stdout worker stopped: wait failed")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - cplat_pinned_prompt_printf() が STDERR のチャネルとワーカー名を含む文字列を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_pinned_prompt_printf() から 0 を返す。
    EXPECT_CALL(mock_cplat_, cplat_pinned_prompt_printf(_, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, _))
        .Times(0); // [Pre-Assert確認_異常系] - tick が出力されないこと。

    // Act
    int result = test_pinned_prompt_cli_run_worker(
        screen_, &thread_running); // [手順] - 待機が失敗する状態で worker_thread_proc() を呼び出す。

    // Assert
    EXPECT_EQ(0, result);         // [確認_異常系] - worker_thread_proc() が戻ること。
    EXPECT_EQ(0, thread_running); // [確認_異常系] - thread_running が 0 であること。
}

// ステータスを設定できない場合に、標準エラー出力のチャネルへ伝えて対話を続けることの確認
TEST_F(pinned_promptTest, process_status_reports_status_set_failure)
{
    // Arrange
    const char *subcommands[] = {"set-top-left text", "set-top-right text", "set-bottom-left text",
                                 "set-bottom-right text"};

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_pinned_prompt_status_set(screen_, _, _, testing::StrEq("text")))
        .Times(4)
        .WillRepeatedly(Return(CPLAT_ERR_OUT_OF_MEMORY)); // [Pre-Assert確認_異常系] - cplat_pinned_prompt_status_set() が "text" を指定して 4 回呼び出されること。
                                                          // [Pre-Assert手順] - cplat_pinned_prompt_status_set() から CPLAT_ERR_OUT_OF_MEMORY を返す。
    EXPECT_CALL(mock_cplat_, cplat_pinned_prompt_printf(screen_, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                                        HasSubstr("failed: result=")))
        .Times(4)
        .WillRepeatedly(Return(0)); // [Pre-Assert確認_異常系] - cplat_pinned_prompt_printf() が STDERR のチャネルと結果コードを含む文字列を指定して 4 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_pinned_prompt_printf() から 0 を返す。

    // Act
    for (const char *subcommand : subcommands)
    {
        test_pinned_prompt_cli_process_status(
            screen_, subcommand); // [手順] - 設定が失敗する状態で 4 つの副命令を指定して process_status() を呼び出す。4 回実施。
    }

    // Assert
    SUCCEED(); // [確認_異常系] - SUCCEED() に到達すること。(設定の失敗の後も process_status() が戻り、対話を続けられることの確認)
}
