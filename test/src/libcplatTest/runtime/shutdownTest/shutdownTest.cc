#include <testfw.h>
#include <mock_cplat.h>
#include <mock_stdlib.h>
#include <cplat/runtime/shutdown.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>

#if defined(PLATFORM_LINUX)
    #include <mock_signal.h>
    #include <signal.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif

#include "shutdown.inject.h"

namespace
{

int g_call_count = 0;
int g_order[8] = {};
cplat_shutdown_event g_last_event = {};

void reset_records()
{
    g_call_count = 0;
    memset(g_order, 0, sizeof(g_order));
    g_last_event.reason = CPLAT_SHUTDOWN_REASON_NORMAL_EXIT;
    g_last_event.code_kind = CPLAT_SHUTDOWN_CODE_KIND_NONE;
    g_last_event.code = 0;
}

cplat_shutdown_event make_event(cplat_shutdown_reason reason, cplat_shutdown_code_kind code_kind, int code)
{
    cplat_shutdown_event event;
    event.reason = reason;
    event.code_kind = code_kind;
    event.code = code;
    return event;
}

void record_callback(const cplat_shutdown_event *event, void *context)
{
    int id = *static_cast<int *>(context);
    g_order[g_call_count++] = id;
    g_last_event = *event;
}

void print_callback(const cplat_shutdown_event *event, void *)
{
    fprintf(stderr, "reason=%d kind=%d code=%d\n", (int)event->reason, (int)event->code_kind, event->code);
}

void print_count_callback(const cplat_shutdown_event *, void *)
{
    static int count = 0;
    count++;
    fprintf(stderr, "count=%d\n", count);
}

} // namespace

class shutdownTest : public Test
{
  protected:
    NiceMock<Mock_stdlib> mock_stdlib_;

    void SetUp() override
    {
        cplat_shutdown_reset_for_test();
        reset_records();
        // atexit は成功を返す。
        ON_CALL(mock_stdlib_, atexit(_, _, _, _)).WillByDefault(Return(0));
    }

    void TearDown() override
    {
        cplat_shutdown_reset_for_test();
    }
};

// shutdown callback が登録の逆順 (LIFO) で実行されることの確認
TEST_F(shutdownTest, test_callbacks_are_invoked_in_lifo_order)
{
    // Arrange
    int first = 1;
    int second = 2;
    int third = 3;
    cplat_shutdown_event event =
        make_event(CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_EXIT_CODE,
                   7); // [状態] - 終了コード 7 を持つ通常終了イベントを用意する。

    ASSERT_EQ(CPLAT_OK, cplat_shutdown_register(record_callback,
                                                      &first)); // [状態] - 記録用 callback を id 1 で登録する。
                                                                // [状態確認] - 1 件目の cplat_shutdown_register の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_shutdown_register(record_callback,
                                                      &second)); // [状態] - 記録用 callback を id 2 で登録する。
                                                                 // [状態確認] - 2 件目の cplat_shutdown_register の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_register(record_callback,
                                         &third)); // [状態] - 記録用 callback を id 3 で登録する。
                                                   // [状態確認] - 3 件目の cplat_shutdown_register の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int invoked = 0;
    int result = cplat_shutdown_invoke_for_test(
        &event, &invoked); // [手順] - 3 件の callback を shutdown イベントで実行する。

    // Assert
    ASSERT_EQ(CPLAT_OK,
              result);     // [確認_正常系] - cplat_shutdown_invoke_for_test の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, invoked); // [確認_正常系] - invoked_out が 1 (実行した) であること。
    ASSERT_EQ(3, g_call_count); // [確認_正常系] - callback が 3 回呼ばれること。
    EXPECT_EQ(3, g_order[0]);   // [確認_正常系] - 最後に登録した callback から実行されること。
    EXPECT_EQ(2, g_order[1]);   // [確認_正常系] - 2 番目に登録した callback が続くこと。
    EXPECT_EQ(1, g_order[2]);   // [確認_正常系] - 最初に登録した callback が最後に実行されること。
    EXPECT_EQ(CPLAT_SHUTDOWN_REASON_NORMAL_EXIT,
              g_last_event.reason); // [確認_正常系] - 終了理由が callback に渡ること。
    EXPECT_EQ(CPLAT_SHUTDOWN_CODE_KIND_EXIT_CODE,
              g_last_event.code_kind); // [確認_正常系] - code_kind が callback に渡ること。
    EXPECT_EQ(7, g_last_event.code);   // [確認_正常系] - 終了コード 7 が callback に渡ること。
}

// 複数回の shutdown 実行でも callback が 1 回だけ実行されることの確認
TEST_F(shutdownTest, test_multiple_invoke_runs_callbacks_only_once)
{
    // Arrange
    int id = 1;
    cplat_shutdown_event event =
        make_event(CPLAT_SHUTDOWN_REASON_PROCESS_TERMINATING, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                   0); // [状態] - プロセス終了中イベントを用意する。

    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_register(record_callback, &id)); // [状態] - 記録用 callback を 1 件登録する。
                                                                // [状態確認] - cplat_shutdown_register の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int first_invoked = 0;
    int second_invoked = 0;
    int first_result =
        cplat_shutdown_invoke_for_test(&event, &first_invoked); // [手順] - 1 回目の shutdown を実行する。
    int second_result =
        cplat_shutdown_invoke_for_test(&event, &second_invoked); // [手順] - 2 回目の shutdown を実行する。
    int register_after_shutdown =
        cplat_shutdown_register(record_callback, &id); // [手順] - shutdown 後に新規登録を試みる。

    // Assert
    ASSERT_EQ(CPLAT_OK, first_result);  // [確認_正常系] - 1 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, first_invoked);           // [確認_正常系] - 1 回目は実行されること。
    ASSERT_EQ(CPLAT_OK, second_result); // [確認_正常系] - 2 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, second_invoked);          // [確認_正常系] - 2 回目は再実行されないこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, register_after_shutdown); // [確認_正常系] - shutdown 開始後の登録が拒否されること。
    EXPECT_EQ(1, g_call_count);                               // [確認_正常系] - callback は 1 回だけ実行されること。
}

// 終了要求 callback の実行が最終 shutdown を消費しないことの確認
TEST_F(shutdownTest, test_request_callbacks_do_not_consume_final_shutdown)
{
    // Arrange
    int request_first = 1;
    int request_second = 2;
    int final_id = 3;
    cplat_shutdown_event request_event =
        make_event(CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT, CPLAT_SHUTDOWN_CODE_KIND_SIGNAL_NUMBER,
                   2); // [状態] - シグナル番号 2 の終了要求イベントを用意する。
    cplat_shutdown_event final_event =
        make_event(CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                   0); // [状態] - 通常終了の最終 shutdown イベントを用意する。

    ASSERT_EQ(CPLAT_OK, cplat_shutdown_request_register(
                              record_callback, &request_first)); // [状態] - 終了要求 callback を id 1 で登録する。
                                                                 // [状態確認] - 1 件目の cplat_shutdown_request_register の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_request_register(
                  record_callback, &request_second)); // [状態] - 終了要求 callback を id 2 で登録する。
                                                      // [状態確認] - 2 件目の cplat_shutdown_request_register の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_register(record_callback,
                                         &final_id)); // [状態] - 最終 shutdown callback を id 3 で 1 件登録する。
                                                      // [状態確認] - cplat_shutdown_register の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int request_invoked = 0;
    int request_result = cplat_shutdown_request_invoke_for_test(
        &request_event, &request_invoked); // [手順] - 終了要求 callback を実行する。

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        request_result); // [確認_正常系] - cplat_shutdown_request_invoke_for_test の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, request_invoked); // [確認_正常系] - invoked_out が 1 (実行した) であること。
    ASSERT_EQ(2, g_call_count);    // [確認_正常系] - request callback が 2 件だけ実行されること。
    EXPECT_EQ(2, g_order[0]);      // [確認_正常系] - request callback も LIFO 順で実行されること。
    EXPECT_EQ(1, g_order[1]);      // [確認_正常系] - 最初の request callback が最後に実行されること。
    EXPECT_EQ(CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT,
              g_last_event.reason); // [確認_正常系] - request イベントが渡ること。

    reset_records();

    int final_invoked = 0;
    int final_result =
        cplat_shutdown_invoke_for_test(&final_event, &final_invoked); // [手順] - 続けて最終 shutdown を実行する。

    ASSERT_EQ(CPLAT_OK, final_result); // [確認_正常系] - final shutdown の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, final_invoked);          // [確認_正常系] - final shutdown が別途実行できること。
    ASSERT_EQ(1, g_call_count);           // [確認_正常系] - final callback は独立して 1 件実行されること。
    EXPECT_EQ(3, g_order[0]);             // [確認_正常系] - final callback が実行されること。
    EXPECT_EQ(CPLAT_SHUTDOWN_REASON_NORMAL_EXIT,
              g_last_event.reason); // [確認_正常系] - final shutdown では通常終了イベントが渡ること。
}

// 終了要求 callback が 1 回だけ実行されることの確認
TEST_F(shutdownTest, test_request_callback_runs_only_once)
{
    // Arrange
    int id = 1;
    cplat_shutdown_event event =
        make_event(CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT, CPLAT_SHUTDOWN_CODE_KIND_SIGNAL_NUMBER,
                   2); // [状態] - シグナル番号 2 の終了要求イベントを用意する。

    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_request_register(record_callback, &id)); // [状態] - 終了要求 callback を 1 件登録する。
                                                                        // [状態確認] - cplat_shutdown_request_register の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int first_invoked = 0;
    int second_invoked = 0;
    int first_result =
        cplat_shutdown_request_invoke_for_test(&event, &first_invoked); // [手順] - 1 回目の終了要求を実行する。
    int second_result =
        cplat_shutdown_request_invoke_for_test(&event, &second_invoked); // [手順] - 2 回目の終了要求を実行する。
    int register_after_request =
        cplat_shutdown_request_register(record_callback, &id); // [手順] - 通知後に新規登録を試みる。

    // Assert
    ASSERT_EQ(CPLAT_OK, first_result);  // [確認_正常系] - 1 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, first_invoked);           // [確認_正常系] - 1 回目は実行されること。
    ASSERT_EQ(CPLAT_OK, second_result); // [確認_正常系] - 2 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, second_invoked);          // [確認_正常系] - 2 回目は再実行されないこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, register_after_request); // [確認_正常系] - 通知後の登録は拒否されること。
    EXPECT_EQ(1, g_call_count); // [確認_正常系] - request callback は 1 回だけ実行されること。
}

// cplat_exit が終了コードを保持したまま callback を実行することの確認
TEST_F(shutdownTest, test_cplat_exit_preserves_exit_code)
{
    // Arrange
    // EXPECT_EXIT の子プロセスは exit で fixture を破棄しないため、子プロセス側の mock 登録を解放対象外にする。
    testing::Mock::AllowLeak(&mock_stdlib_);
    ON_CALL(mock_stdlib_, atexit(_, _, _, _))
        .WillByDefault(
            Invoke(delegate_real_atexit)); // [状態] - atexit が呼び出された際に本物へ委譲するようにモックを設定する。

    // Pre-Assert

    // Act
    // [手順] - 子プロセス側でイベント内容を出力する callback を登録し、cplat_exit(7) を呼び出す。
    // Assert
    // 終了コード 7 で終了し、callback に reason=0 kind=1 code=7 のイベントが渡ること。
    EXPECT_EXIT(
        {
            cplat_shutdown_reset_for_test();
            cplat_shutdown_register(print_callback, NULL);
            cplat_exit(7);
        },
        ::testing::ExitedWithCode(7), "reason=0 kind=1 code=7");
    // [確認_正常系] - EXPECT_EXIT({ cplat_shutdown_reset_for_test(); cplat_shutdown_register(print_callback, NULL); cplat_exit(7); }, ::testing::ExitedWithCode(7), "reason=0 kind=1 code=7") の期待が成立すること。
}

// cplat_exit が範囲外 (CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE 以上) の終了コードを
// CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE へ差し替えることの確認
TEST_F(shutdownTest, test_cplat_exit_clamps_code_above_range)
{
    // Arrange
    // EXPECT_EXIT の子プロセスは exit で fixture を破棄しないため、子プロセス側の mock 登録を解放対象外にする。
    testing::Mock::AllowLeak(&mock_stdlib_);
    ON_CALL(mock_stdlib_, atexit(_, _, _, _))
        .WillByDefault(
            Invoke(delegate_real_atexit)); // [状態] - atexit が呼び出された際に本物へ委譲するようにモックを設定する。

    // Pre-Assert

    // Act
    // [手順] - 子プロセス側でイベント内容を出力する callback を登録し、下位 8 bit 切り捨てで 0 になる
    //          cplat_exit(256) を呼び出す。
    // Assert
    // 終了コードが CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE (125) へ差し替わり、
    //                callback にも code=125 のイベントが渡ること。
    EXPECT_EXIT(
        {
            cplat_shutdown_reset_for_test();
            cplat_shutdown_register(print_callback, NULL);
            cplat_exit(256);
        },
        ::testing::ExitedWithCode(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE), "reason=0 kind=1 code=125");
    // [確認_正常系] - EXPECT_EXIT({ cplat_shutdown_reset_for_test(); cplat_shutdown_register(print_callback, NULL); cplat_exit(256); }, ::testing::ExitedWithCode(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE), "reason=0 kind=1 code=125") の期待が成立すること。
}

// cplat_exit が負の終了コードも CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE へ差し替えることの確認
TEST_F(shutdownTest, test_cplat_exit_clamps_negative_code)
{
    // Arrange
    // EXPECT_EXIT の子プロセスは exit で fixture を破棄しないため、子プロセス側の mock 登録を解放対象外にする。
    testing::Mock::AllowLeak(&mock_stdlib_);
    ON_CALL(mock_stdlib_, atexit(_, _, _, _))
        .WillByDefault(
            Invoke(delegate_real_atexit)); // [状態] - atexit が呼び出された際に本物へ委譲するようにモックを設定する。

    // Pre-Assert

    // Act
    // [手順] - 子プロセス側でイベント内容を出力する callback を登録し、cplat_exit(-1) を呼び出す。
    // Assert
    // 終了コードが CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE (125) へ差し替わり、
    //                callback にも code=125 のイベントが渡ること。
    EXPECT_EXIT(
        {
            cplat_shutdown_reset_for_test();
            cplat_shutdown_register(print_callback, NULL);
            cplat_exit(-1);
        },
        ::testing::ExitedWithCode(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE), "reason=0 kind=1 code=125");
    // [確認_正常系] - EXPECT_EXIT({ cplat_shutdown_reset_for_test(); cplat_shutdown_register(print_callback, NULL); cplat_exit(-1); }, ::testing::ExitedWithCode(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE), "reason=0 kind=1 code=125") の期待が成立すること。
}

// cplat_exit が範囲上限 (CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1) の終了コードを
// 差し替えずにそのまま使うことの確認
TEST_F(shutdownTest, test_cplat_exit_preserves_upper_bound_code)
{
    // Arrange
    // EXPECT_EXIT の子プロセスは exit で fixture を破棄しないため、子プロセス側の mock 登録を解放対象外にする。
    testing::Mock::AllowLeak(&mock_stdlib_);
    ON_CALL(mock_stdlib_, atexit(_, _, _, _))
        .WillByDefault(
            Invoke(delegate_real_atexit)); // [状態] - atexit が呼び出された際に本物へ委譲するようにモックを設定する。

    // Pre-Assert

    // Act
    // [手順] - 子プロセス側でイベント内容を出力する callback を登録し、
    //          範囲上限の cplat_exit(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1) を呼び出す。
    // Assert
    // 終了コード 124 のまま差し替わらずに終了し、callback にも code=124 のイベントが渡ること。
    EXPECT_EXIT(
        {
            cplat_shutdown_reset_for_test();
            cplat_shutdown_register(print_callback, NULL);
            cplat_exit(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1);
        },
        ::testing::ExitedWithCode(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1), "reason=0 kind=1 code=124");
    // [確認_正常系] - EXPECT_EXIT({ cplat_shutdown_reset_for_test(); cplat_shutdown_register(print_callback, NULL); cplat_exit(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1); }, ::testing::ExitedWithCode(CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1), "reason=0 kind=1 code=124") の期待が成立すること。
}

// 明示的な shutdown 実行後に atexit で二重実行されないことの確認
TEST_F(shutdownTest, test_explicit_invoke_prevents_atexit_double_execution)
{
    // Arrange
    // EXPECT_EXIT の子プロセスは exit で fixture を破棄しないため、子プロセス側の mock 登録を解放対象外にする。
    testing::Mock::AllowLeak(&mock_stdlib_);
    ON_CALL(mock_stdlib_, atexit(_, _, _, _))
        .WillByDefault(
            Invoke(delegate_real_atexit)); // [状態] - atexit が呼び出された際に本物へ委譲するようにモックを設定する。

    // Pre-Assert

    // Act
    // [手順] - 子プロセス側で実行回数を出力する callback を登録し、明示的な shutdown 実行後に exit(0) を呼び出す。
    // Assert
    // 終了コード 0 で終了し、callback の実行回数が count=1 のまま二重実行されないこと。
    EXPECT_EXIT(
        {
            cplat_shutdown_event event =
                make_event(CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE, 0);
            cplat_shutdown_reset_for_test();
            cplat_shutdown_register(print_count_callback, NULL);
            cplat_shutdown_invoke_for_test(&event, NULL);
            exit(0);
        },
        ::testing::ExitedWithCode(0), "count=1");
    // [確認_正常系] - EXPECT_EXIT({ cplat_shutdown_event event = make_event(CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE, 0); cplat_shutdown_reset_for_test(); cplat_shutdown_register(print_count_callback, NULL); cplat_shutdown_invoke_for_test(&event, NULL); exit(0); }, ::testing::ExitedWithCode(0), "count=1") の期待が成立すること。
}

#if defined(PLATFORM_LINUX)
// SIGINT が終了要求 callback へ報告され、処理が継続することの確認
TEST_F(shutdownTest, test_sigint_is_reported_to_callback)
{
    // Arrange
    // EXPECT_EXIT の子プロセスは exit で fixture を破棄しないため、子プロセス側の mock 登録を解放対象外にする。
    testing::Mock::AllowLeak(&mock_stdlib_);

    // Pre-Assert

    // Act
    // [手順] - 子プロセス側でイベント内容を出力する終了要求 callback を登録し、raise(SIGINT) を発生させる。
    // Assert
    // callback に reason=2 kind=2 code=2 のイベントが渡り、SIGINT 後も処理が継続して終了コード 0 で終了すること。
    EXPECT_EXIT(
        {
            cplat_shutdown_reset_for_test();
            cplat_shutdown_request_register(print_callback, NULL);
            raise(SIGINT);
            fprintf(stderr, "after-sigint\n");
            exit(0);
        },
        ::testing::ExitedWithCode(0), "reason=2 kind=2 code=2.*after-sigint");
    // [確認_正常系] - EXPECT_EXIT({ cplat_shutdown_reset_for_test(); cplat_shutdown_request_register(print_callback, NULL); raise(SIGINT); fprintf(stderr, "after-sigint\n"); exit(0); }, ::testing::ExitedWithCode(0), "reason=2 kind=2 code=2.*after-sigint") の期待が成立すること。
}

// callback がないシグナルを最終 shutdown 後に既定処理へ戻すことの確認
TEST_F(shutdownTest, test_unhandled_signal_invokes_final_callbacks_and_reraises)
{
    // Arrange
    NiceMock<Mock_signal> mock_signal;
    int id = 1;
    ASSERT_EQ(CPLAT_OK, cplat_shutdown_register(record_callback, &id)); // [状態] - 最終 shutdown callback を 1 件登録する。
                                                                            // [状態確認] - cplat_shutdown_register の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_signal, signal(_, _, _, _, SIG_DFL))
        .WillOnce(Return(SIG_DFL)); // [Pre-Assert確認_正常系] - signal が SIG_DFL を指定して 1 回呼び出されること。
                                    // [Pre-Assert手順] - signal から SIG_DFL を返却する。
    EXPECT_CALL(mock_signal, raise(_, _, _, SIGTERM))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - raise が SIGTERM を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - raise から 0 を返却する。

    // Act
    test_shutdown_signal_handler(SIGTERM); // [手順] - callback がない SIGTERM のシグナル ハンドラーを実行する。

    // Assert
    EXPECT_EQ(1, g_call_count);            // [確認_正常系] - 最終 shutdown callback が 1 回実行されること。
    EXPECT_EQ(SIGTERM, g_last_event.code); // [確認_正常系] - callback へ SIGTERM の番号が渡ること。
}

// 処理済みの終了要求ではシグナルの既定処理を再実行しないことの確認
TEST_F(shutdownTest, test_repeated_signal_request_returns_without_reraise)
{
    // Arrange
    cplat_shutdown_event event =
        make_event(CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT, CPLAT_SHUTDOWN_CODE_KIND_SIGNAL_NUMBER, SIGINT);
    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_request_invoke_for_test(&event, NULL)); // [状態] - 終了要求を処理済みの状態にする。
                                                                        // [状態確認] - cplat_shutdown_request_invoke_for_test の戻り値が CPLAT_OK であること。
    NiceMock<Mock_signal> mock_signal;

    // Pre-Assert
    EXPECT_CALL(mock_signal, signal(_, _, _, _, _)).Times(0); // [Pre-Assert確認_正常系] - signal が呼び出されないこと。
    EXPECT_CALL(mock_signal, raise(_, _, _, _)).Times(0);     // [Pre-Assert確認_正常系] - raise が呼び出されないこと。

    // Act
    test_shutdown_signal_handler(SIGINT); // [手順] - 終了要求処理済みの状態で SIGINT のハンドラーを実行する。

    // Assert
    EXPECT_EQ(0, g_call_count); // [確認_正常系] - callback が追加実行されないこと。
}

// shutdown API が不正引数とメモリ確保失敗を通知することの確認
TEST_F(shutdownTest, test_registration_and_invoke_reject_invalid_inputs)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が 2 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    int register_null_result =
        cplat_shutdown_register(NULL, NULL); // [手順] - NULL callback を最終 shutdown へ登録する。
    int request_register_null_result =
        cplat_shutdown_request_register(NULL, NULL); // [手順] - NULL callback を終了要求へ登録する。
    int register_oom_result =
        cplat_shutdown_register(record_callback, NULL); // [手順] - malloc 失敗時に最終 callback を登録する。
    int request_register_oom_result = cplat_shutdown_request_register(
        record_callback, NULL); // [手順] - malloc 失敗時に終了要求 callback を登録する。
    int invoke_null_result =
        cplat_shutdown_invoke_for_test(NULL, NULL); // [手順] - NULL イベントで最終 shutdown を実行する。
    int request_invoke_null_result =
        cplat_shutdown_request_invoke_for_test(NULL, NULL); // [手順] - NULL イベントで終了要求を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              register_null_result); // [確認_異常系] - cplat_shutdown_register が NULL callback を拒否すること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        request_register_null_result); // [確認_異常系] - cplat_shutdown_request_register が NULL callback を拒否すること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              register_oom_result); // [確認_異常系] - cplat_shutdown_register がメモリ不足を通知すること。
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        request_register_oom_result); // [確認_異常系] - cplat_shutdown_request_register がメモリ不足を通知すること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invoke_null_result); // [確認_異常系] - cplat_shutdown_invoke_for_test が NULL イベントを拒否すること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        request_invoke_null_result); // [確認_異常系] - cplat_shutdown_request_invoke_for_test が NULL イベントを拒否すること。
}

// 最終 shutdown 開始後の終了要求 callback 登録を拒否することの確認
TEST_F(shutdownTest, test_request_registration_after_final_shutdown_is_rejected)
{
    // Arrange
    cplat_shutdown_event event =
        make_event(CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE, 0);
    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_invoke_for_test(&event, NULL)); // [状態] - 最終 shutdown を実行済みの状態にする。
                                                                // [状態確認] - cplat_shutdown_invoke_for_test の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_shutdown_request_register(
        record_callback, NULL); // [手順] - 最終 shutdown 開始後に終了要求 callback を登録する。
    int request_invoked = 1;
    int invoke_result = cplat_shutdown_request_invoke_for_test(
        &event, &request_invoked); // [手順] - 最終 shutdown 開始後に終了要求 callback の実行を試みる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_shutdown_request_register が CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(
        CPLAT_OK,
        invoke_result); // [確認_正常系] - cplat_shutdown_request_invoke_for_test の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, request_invoked); // [確認_正常系] - 最終 shutdown 開始後は終了要求 callback が実行されないこと。
}

// 未実行の callback をテスト状態のリセット時に解放することの確認
TEST_F(shutdownTest, test_reset_discards_pending_callbacks)
{
    // Arrange
    int id = 1;
    ASSERT_EQ(CPLAT_OK, cplat_shutdown_register(record_callback, &id)); // [状態] - 最終 shutdown callback を 1 件登録する。
                                                                            // [状態確認] - cplat_shutdown_register の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_request_register(record_callback, &id)); // [状態] - 終了要求 callback を 1 件登録する。
                                                                        // [状態確認] - cplat_shutdown_request_register の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    cplat_shutdown_reset_for_test(); // [手順] - 未実行の 2 種類の callback を登録した状態をリセットする。

    // Assert
    EXPECT_EQ(0, g_call_count); // [確認_正常系] - リセット時に callback が実行されないこと。
}
#elif defined(PLATFORM_WINDOWS)
// コンソール イベントが終了要求 callback へ報告されることの確認
TEST_F(shutdownTest, test_console_event_is_reported_to_callback)
{
    // Arrange
    int id = 1;
    cplat_shutdown_event event =
        make_event(CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT, CPLAT_SHUTDOWN_CODE_KIND_CONSOLE_CTRL_TYPE,
                   CTRL_C_EVENT); // [状態] - CTRL_C_EVENT を模擬したイベントを用意する。

    ASSERT_EQ(CPLAT_OK,
              cplat_shutdown_request_register(record_callback,
                                                 &id)); // [状態] - 記録用の終了要求 callback を 1 件登録する。
                                                        // [状態確認] - cplat_shutdown_request_register の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int invoked = 0;
    int result = cplat_shutdown_request_invoke_for_test(
        &event, &invoked); // [手順] - CTRL_C_EVENT 相当の request callback を実行する。

    // Assert
    ASSERT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_shutdown_request_invoke_for_test の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, invoked);      // [確認_正常系] - invoked_out が 1 (実行した) であること。
    EXPECT_EQ(1, g_call_count); // [確認_正常系] - callback が 1 回実行されること。
    EXPECT_EQ(CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT,
              g_last_event.reason); // [確認_正常系] - コンソール イベントの理由が渡ること。
    EXPECT_EQ(CPLAT_SHUTDOWN_CODE_KIND_CONSOLE_CTRL_TYPE,
              g_last_event.code_kind);               // [確認_正常系] - CTRL 種別として渡ること。
    EXPECT_EQ((int)CTRL_C_EVENT, g_last_event.code); // [確認_正常系] - CTRL_C_EVENT の値が渡ること。
}
#endif
