#include <testfw.h>
#include <mock_cplat.h>
#include "prompt_platform.h"
#if defined(PLATFORM_LINUX)
    #include <mock_termios.h>
    #include <mock_signal.h>
    #include <mock_unistd.h>
    #include <sys/mock_select.h>
    #include <errno.h>
    #include "prompt_linux.inject.h"

// シグナル登録失敗とロールバック失敗を区別することの確認
TEST(promptLinuxFailureTest, registration_failure_retains_failed_restoration)
{
    // Arrange
    NiceMock<Mock_termios> termios_mock;
    NiceMock<Mock_signal> signal_mock;
    prompt_platform_context context = {};
    test_prompt_set_sigwinch_installed(0);
    test_prompt_set_resize_pending(0);
    // Pre-Assert
    EXPECT_CALL(termios_mock, tcgetattr(_, _, _, _, _)).WillOnce(Return(0));
    EXPECT_CALL(termios_mock, tcsetattr(_, _, _, _, _, _)).WillOnce(Return(0)).WillOnce(Return(-1)).WillOnce(Return(0));
    EXPECT_CALL(signal_mock, sigemptyset(_, _, _, _)).WillOnce(Return(0));
    EXPECT_CALL(signal_mock, sigaction(_, _, _, _, _, _)).WillOnce(Return(-1));
    // [Pre-Assert確認_異常系] - 保存 1 回、設定・失敗した復元・再復元の 3 回、通知登録の失敗 1 回を確認する。
    // Act
    int enter = prompt_platform_enter_raw(&context); // [手順] - 通知登録と直後の復元を失敗させる。
    int pending = context.raw_active;
    int retry = prompt_platform_leave_raw(&context); // [手順] - 復元を再試行する。
    // Assert
    EXPECT_EQ(-1, enter);             // [確認_異常系] - 行入力への切り替え不可を返すこと。
    EXPECT_EQ(1, pending);            // [確認_異常系] - 復元義務を保持すること。
    EXPECT_EQ(0, retry);              // [確認_正常系] - 再復元に成功すること。
    EXPECT_EQ(0, context.raw_active); // [確認_正常系] - 成功後にだけ義務を解除すること。
}

// 片方の失敗で他の復元を省略せず、保存値を上書きしないことの確認
TEST(promptLinuxFailureTest, both_restore_items_are_retried_without_overwriting_saved_state)
{
    // Arrange
    NiceMock<Mock_termios> termios_mock;
    NiceMock<Mock_signal> signal_mock;
    prompt_platform_context context = {};
    context.raw_active = 1;
    context.signal_active = 1;
    context.orig_term.c_lflag = 123;
    test_prompt_set_sigwinch_installed(1);
    // Pre-Assert
    EXPECT_CALL(termios_mock, tcgetattr(_, _, _, _, _)).Times(0);
    EXPECT_CALL(termios_mock, tcsetattr(_, _, _, _, _, _)).WillOnce(Return(-1)).WillOnce(Return(0));
    EXPECT_CALL(signal_mock, sigaction(_, _, _, _, _, _)).WillOnce(Return(-1)).WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - 両項目を各 2 回復元し、元の設定を再取得しないこと。
    // Act
    int enter = prompt_platform_enter_raw(&context); // [手順] - 復元未完のまま入力を再開する。
    int raw_pending = context.raw_active;
    int signal_pending = context.signal_active;
    int retry = prompt_platform_leave_raw(&context); // [手順] - 両項目の復元を再試行する。
    // Assert
    EXPECT_EQ(-1, enter);                           // [確認_異常系] - 復元できなければ取得を開始しないこと。
    EXPECT_EQ(1, raw_pending);                      // [確認_異常系] - 端末設定の復元義務が残ること。
    EXPECT_EQ(1, signal_pending);                   // [確認_異常系] - 通知の復元義務が残ること。
    EXPECT_EQ(123U, context.orig_term.c_lflag);     // [確認_正常系] - 元の設定を維持すること。
    EXPECT_EQ(0, retry);                            // [確認_正常系] - 復元を再試行できること。
    EXPECT_EQ(0, test_prompt_sigwinch_installed()); // [確認_正常系] - 通知登録を解除すること。
}

// OS 入力失敗と短時間待ちの中断・タイムアウトを区別することの確認
TEST(promptLinuxFailureTest, read_and_select_errors_are_distinct_from_timeout)
{
    // Arrange
    NiceMock<Mock_unistd> unistd_mock;
    NiceMock<Mock_sys_select> select_mock;
    prompt_platform_context context = {};
    test_prompt_set_resize_pending(0);
    // Pre-Assert
    EXPECT_CALL(unistd_mock, read(_, _, _, _, _, _)).WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));
    EXPECT_CALL(select_mock, select(_, _, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EIO), Return(-1)))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - read 1 回と select の失敗・中断・タイムアウトの 3 回を確認する。
    // Act
    int read_error = prompt_platform_read_char(&context);    // [手順] - 読み取り失敗を取得する。
    int wait_error = prompt_platform_read_char_nb(&context); // [手順] - 待機失敗を取得する。
    int timeout = prompt_platform_read_char_nb(&context);    // [手順] - 中断後のタイムアウトを取得する。
    // Assert
    EXPECT_EQ(PROMPT_PLATFORM_ERROR, read_error); // [確認_異常系] - 読み取り失敗を EOF と区別すること。
    EXPECT_EQ(PROMPT_PLATFORM_ERROR, wait_error); // [確認_異常系] - 待機失敗をタイムアウトと区別すること。
    EXPECT_EQ(PROMPT_PLATFORM_TIMEOUT, timeout);  // [確認_正常系] - 中断を再待機してタイムアウトを返すこと。
}
// シグナルマスク初期化失敗時に端末を復元して行入力へ切り替えられることの確認
TEST(promptLinuxFailureTest, mask_failure_rolls_back_before_fallback)
{
    // Arrange
    NiceMock<Mock_termios> termios_mock;
    NiceMock<Mock_signal> signal_mock;
    prompt_platform_context context = {};
    test_prompt_set_sigwinch_installed(0);
    // Pre-Assert
    EXPECT_CALL(termios_mock, tcgetattr(_, _, _, _, _)).WillOnce(Return(0));
    EXPECT_CALL(termios_mock, tcsetattr(_, _, _, _, _, _)).Times(2).WillRepeatedly(Return(0));
    EXPECT_CALL(signal_mock, sigemptyset(_, _, _, _)).WillOnce(Return(-1));
    EXPECT_CALL(signal_mock, sigaction(_, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 端末設定の保存 1 回、変更と復元の 2 回を行い、登録を開始しないこと。
    // Act
    int result = prompt_platform_enter_raw(&context); // [手順] - シグナルマスク初期化を失敗させる。
    // Assert
    EXPECT_EQ(1, result);             // [確認_異常系] - 復元済みの移行失敗を返すこと。
    EXPECT_EQ(0, context.raw_active); // [確認_正常系] - 端末設定を復元すること。
}

// 読み取り直前のリサイズ通知を入力待ちより先に消費することの確認
TEST(promptLinuxFailureTest, pending_resize_does_not_block_on_input)
{
    // Arrange
    NiceMock<Mock_unistd> unistd_mock;
    NiceMock<Mock_sys_select> select_mock;
    prompt_platform_context context = {};
    // Pre-Assert
    EXPECT_CALL(unistd_mock, read(_, _, _, _, _, _)).Times(0);
    EXPECT_CALL(select_mock, select(_, _, _, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_正常系] - 保留通知がある場合は入力待ちを呼ばないこと。
    // Act
    test_prompt_set_resize_pending(1);
    int blocking = prompt_platform_read_char(&context); // [手順] - 待機入力前の通知を消費する。
    test_prompt_set_resize_pending(1);
    int short_wait = prompt_platform_read_char_nb(&context); // [手順] - 短時間待機前の通知を消費する。
    // Assert
    EXPECT_EQ(PROMPT_PLATFORM_RESIZE, blocking);   // [確認_正常系] - 待機入力も通知を先に返すこと。
    EXPECT_EQ(PROMPT_PLATFORM_RESIZE, short_wait); // [確認_正常系] - 短時間待機も通知を先に返すこと。
    EXPECT_EQ(0, test_prompt_resize_pending());    // [確認_正常系] - 消費済み通知を残さないこと。
}
#endif
