#include <testfw.h>
#include <mock_cplat.h>
#include "prompt_platform.h"
#if defined(PLATFORM_LINUX)
    #include <mock_termios.h>
    #include <mock_signal.h>
    #include <mock_unistd.h>
    #include <mock_poll.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <poll.h>
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
TEST(promptLinuxFailureTest, read_and_poll_errors_are_distinct_from_timeout)
{
    // Arrange
    NiceMock<Mock_unistd> unistd_mock;
    NiceMock<Mock_poll> poll_mock;
    prompt_platform_context context = {};
    test_prompt_set_resize_pending(0);
    // Pre-Assert
    EXPECT_CALL(poll_mock, poll(_, _, _, _, 1U, -1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }));
    EXPECT_CALL(unistd_mock, read(_, _, _, _, _, _)).WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));
    EXPECT_CALL(poll_mock, poll(_, _, _, _, 1U, PROMPT_PLATFORM_SHORT_WAIT_MS))
        .WillOnce(DoAll(Assign(&errno, EIO), Return(-1)))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - read 1 回と poll の入力あり・失敗・中断・タイムアウトを確認する。
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
    NiceMock<Mock_poll> poll_mock;
    prompt_platform_context context = {};
    // Pre-Assert
    EXPECT_CALL(unistd_mock, read(_, _, _, _, _, _)).Times(0);
    EXPECT_CALL(poll_mock, poll(_, _, _, _, _, _)).Times(0);
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

namespace
{

int g_pipe2_flags = 0;
int g_pipe2_return = 0;

int fake_pipe2_capture_or_fail(int fds[2], int flags)
{
    g_pipe2_flags = flags;
    if (g_pipe2_return != 0)
    {
        return -1;
    }

    fds[0] = 10;
    fds[1] = 11;
    return 0;
}

} // namespace

// 中止用パイプの原子的生成失敗時に状態を汚染しないことの確認
TEST(promptLinuxFailureTest, init_cancelable_handles_pipe_and_fcntl_failures)
{
    // Arrange
    prompt_platform_context context = {};
    g_pipe2_flags = 0;
    g_pipe2_return = -1;
    test_prompt_set_pipe2_fn(fake_pipe2_capture_or_fail);

    // Act / Assert: pipe2 失敗
    EXPECT_EQ(-1, prompt_platform_init_cancelable(&context));
    EXPECT_EQ(O_NONBLOCK | O_CLOEXEC, g_pipe2_flags);
    EXPECT_EQ(0, context.cancel_pipe_active);
    EXPECT_EQ(-1, prompt_platform_init_cancelable(NULL));

    // Cleanup
    test_prompt_set_pipe2_fn(NULL);
}

// stop_input の引数異常・未初期化・書き込み失敗・EAGAIN 許容を確認する
TEST(promptLinuxFailureTest, stop_input_handles_errors_and_eagain)
{
    // Arrange
    NiceMock<Mock_unistd> unistd_mock;
    prompt_platform_context uninit = {};
    prompt_platform_context ctx = {};
    ctx.cancel_pipe[0] = 20;
    ctx.cancel_pipe[1] = 21;
    ctx.cancel_pipe_active = 1;

    // Pre-Assert
    EXPECT_CALL(unistd_mock, write(_, _, _, 21, _, 1U))
        .WillOnce(DoAll(Assign(&errno, EAGAIN), Return(-1)))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));

    // Act / Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, prompt_platform_stop_input(NULL));
    EXPECT_EQ(0, prompt_platform_is_input_canceled(NULL));
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, prompt_platform_stop_input(&uninit));
    EXPECT_EQ(1, prompt_platform_is_input_canceled(&uninit));
    EXPECT_EQ(CPLAT_OK, prompt_platform_stop_input(&ctx));
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, prompt_platform_stop_input(&ctx));
}

// 非 TTY の行入力におけるエラー・中止・バッファー不足の境界を確認する
TEST(promptLinuxFailureTest, read_line_handles_cancel_errors_and_small_buffer)
{
    // Arrange
    NiceMock<Mock_poll> poll_mock;
    NiceMock<Mock_unistd> unistd_mock;
    prompt_platform_context ctx = {};
    ctx.cancel_pipe[0] = 30;
    ctx.cancel_pipe[1] = 31;
    ctx.cancel_pipe_active = 1;
    char buf[4] = "old";

    // Act / Assert: 引数異常と buf_size == 1
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, prompt_platform_read_line(NULL, buf, sizeof(buf)));
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, prompt_platform_read_line(&ctx, NULL, sizeof(buf)));
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, prompt_platform_read_line(&ctx, buf, 0U));
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, prompt_platform_read_line(&ctx, buf, 1U));

    // Pre-Assert: 2 バイト読んで部分行のままパイプ中止される
    EXPECT_CALL(poll_mock, poll(_, _, _, _, 2U, -1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[1].revents = POLLIN;
                return 1;
            }));
    EXPECT_CALL(unistd_mock, read(_, _, _, STDIN_FILENO, _, 1U))
        .WillOnce(Invoke(
            [](const char *, int, const char *, int, void *out, size_t) -> ssize_t
            {
                *static_cast<unsigned char *>(out) = 'a';
                return 1;
            }))
        .WillOnce(Invoke(
            [](const char *, int, const char *, int, void *out, size_t) -> ssize_t
            {
                *static_cast<unsigned char *>(out) = 'b';
                return 1;
            }));

    // Act / Assert: 部分行でも中止なら空文字と CPLAT_ERR_CANCELED
    EXPECT_EQ(CPLAT_ERR_CANCELED, prompt_platform_read_line(&ctx, buf, sizeof(buf)));
    EXPECT_STREQ("", buf);
}
#endif
