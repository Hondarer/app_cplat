#include <testfw.h>
#include "../../../../../prod/libsrc/cplat/prompt/prompt_state.h"
#include <mock_termios.h>
#include <mock_unistd.h>
#include <mock_poll.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <mock_ioctl.h>
#include <mock_signal.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

    #include <signal.h>
    #include <unistd.h>

    #include "prompt_linux.inject.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

static size_t platform_columns(void)
{
    int cols;
    int rows;
    prompt_platform_get_size(&cols, &rows);
    return (size_t)cols;
}

class promptLinuxTest : public Test
{
  protected:
    prompt_platform_context handle_ = {};

    // [サブ手順 名前=promptLinuxTest.SetUp]
    void SetUp() override
    {
        test_prompt_set_resize_pending(0);
    }
    // [サブ手順終了]
};

/*
 * prompt_platform_os_enter_raw / prompt_platform_os_leave_raw
 */

// 端末でない標準入力では raw モードへ移行しないことの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_raw_does_nothing_for_non_terminal)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(
            Return(-1)); // [Pre-Assert確認_異常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - tcgetattr() から -1 を返す。

    // Act
    prompt_platform_os_enter_raw(&handle_); // [手順] - prompt_platform_os_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_異常系] - handle_.raw_active が 0 であること。(tcgetattr() の失敗で raw モードへ移行しないことの確認)
}

// 端末に対して raw モードへ移行し復帰できることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_and_leave_raw_on_terminal)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;
    struct termios original = {};

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr() が STDIN_FILENO を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。

    // Act
    prompt_platform_os_enter_raw(&handle_); // [手順] - prompt_platform_os_enter_raw() を呼び出す。
    int raw_after_enter = handle_.raw_active;
    int sigwinch_after_enter = test_prompt_sigwinch_installed();
    prompt_platform_os_leave_raw(&handle_); // [手順] - prompt_platform_os_leave_raw() を呼び出す。

    // Assert
    EXPECT_EQ(1,
              raw_after_enter); // [確認_正常系] - raw_after_enter が 1 であること。(raw モードが有効になったことの確認)
    EXPECT_EQ(
        1,
        sigwinch_after_enter); // [確認_正常系] - 進入後の test_prompt_sigwinch_installed() の戻り値が 1 であること。(SIGWINCH ハンドラーが登録されたことの確認)
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_正常系] - 復帰後の handle_.raw_active が 0 であること。(raw モードが無効になったことの確認)
    EXPECT_EQ(
        0,
        test_prompt_sigwinch_installed()); // [確認_正常系] - 復帰後の test_prompt_sigwinch_installed() の戻り値が 0 であること。(SIGWINCH ハンドラーが復元されたことの確認)
}

// 端末設定の適用に失敗した場合に raw モードへ移行しないことの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_raw_does_nothing_when_tcsetattr_fails)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;
    struct termios original = {};

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr() から、元の端末設定を設定して 0 を返す。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(
            Return(-1)); // [Pre-Assert確認_異常系] - tcsetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - tcsetattr() から -1 を返す。

    // Act
    prompt_platform_os_enter_raw(&handle_); // [手順] - prompt_platform_os_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_異常系] - handle_.raw_active が 0 であること。(端末設定を適用できず raw モードへ移行しないことの確認)
    EXPECT_EQ(
        0,
        test_prompt_sigwinch_installed()); // [確認_異常系] - test_prompt_sigwinch_installed() の戻り値が 0 であること。(SIGWINCH ハンドラーが登録されないことの確認)
}

// raw モード中の再入で二重に移行しないことの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_raw_is_ignored_while_already_raw)
{
    // Arrange
    handle_.raw_active = 1; // [状態] - すでに raw モードとして扱う。

    // Pre-Assert

    // Act
    prompt_platform_os_enter_raw(&handle_); // [手順] - raw モード中に prompt_platform_os_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        handle_
            .raw_active); // [確認_正常系] - handle_.raw_active が 1 であること。(raw モードの状態が変化しないことの確認)

    // Cleanup
    handle_.raw_active = 0;
}

// SIGWINCH ハンドラーの登録済み状態を保持して raw モードへ移行することの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_raw_preserves_unrestored_signal_owner)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;
    test_prompt_set_sigwinch_installed(1);
    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 復元未完の前ハンドラーがある場合は設定を保存し直さないこと。
    // Act
    int result = prompt_platform_os_enter_raw(&handle_); // [手順] - 別の復元義務が残る状態で取得する。
    // Assert
    EXPECT_EQ(-1, result);            // [確認_異常系] - 未復元の状態を上書きしないこと。
    EXPECT_EQ(0, handle_.raw_active); // [確認_異常系] - raw モードを開始しないこと。
    // Cleanup
    test_prompt_set_sigwinch_installed(0);
}

// raw モードでないときの復帰が何もしないことの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, leave_raw_is_ignored_when_not_raw)
{
    // Arrange
    handle_.raw_active = 0; // [状態] - raw モードでない状態とする。

    // Pre-Assert

    // Act
    prompt_platform_os_leave_raw(&handle_); // [手順] - prompt_platform_os_leave_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_正常系] - handle_.raw_active が 0 であること。(raw モードの状態が変化しないことの確認)
}

// SIGWINCH ハンドラーが未登録でも raw モードを解除できることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, leave_raw_handles_uninstalled_sigwinch_handler)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;

    handle_.raw_active = 1;
    test_prompt_set_sigwinch_installed(0); // [状態] - raw モード中で SIGWINCH ハンドラーが未登録の状態とする。

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr() が STDIN_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr() から 0 を返す。

    // Act
    prompt_platform_os_leave_raw(&handle_); // [手順] - 未登録状態で prompt_platform_os_leave_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_.raw_active); // [確認_正常系] - handle_.raw_active が 0 であること。(raw モードが無効になったことの確認)
    EXPECT_EQ(
        0,
        test_prompt_sigwinch_installed()); // [確認_正常系] - test_prompt_sigwinch_installed() の戻り値が 0 であること。(SIGWINCH ハンドラーの未登録状態が維持されたことの確認)
}

// SIGWINCH の受信がリサイズ待ちとして記録されることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, sigwinch_handler_records_pending_resize)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;
    struct termios original = {};

    ON_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillByDefault(
            DoAll(SetArgPointee<4>(original),
                  Return(0))); // [状態] - tcgetattr() から、元の端末設定を設定して 0 を返すように既定動作を設定する。
    ON_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _))
        .WillByDefault(Return(0)); // [状態] - tcsetattr() から 0 を返すように既定動作を設定する。
    prompt_platform_os_enter_raw(&handle_);
    ASSERT_EQ(
        1, test_prompt_sigwinch_installed()); // [状態確認] - test_prompt_sigwinch_installed() の戻り値が 1 であること。

    // Pre-Assert

    // Act
    raise(SIGWINCH); // [手順] - SIGWINCH を指定して raise() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        test_prompt_resize_pending()); // [確認_正常系] - test_prompt_resize_pending() の戻り値が 1 であること。(リサイズ待ちとして記録されたことの確認)

    // Cleanup
    test_prompt_set_resize_pending(0);
    prompt_platform_os_leave_raw(&handle_);
}

/*
 * prompt_platform_read_char
 */

// 標準入力から 1 バイトが読み取れることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_returns_next_byte)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_poll, poll(_, _, _, _, 1U, -1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }));
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(
            [](const char *, int, const char *, int, void *buf, size_t) -> ssize_t
            {
                *static_cast<unsigned char *>(buf) = static_cast<unsigned char>('A');
                return 1;
            }); // [Pre-Assert確認_正常系] - poll() 後に read() が STDIN_FILENO、1 を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - read() から、'A' を書き込んで 1 を返す。

    // Act
    int actual_ret = prompt_platform_read_char(&handle_); // [手順] - prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ('A',
              actual_ret); // [確認_正常系] - prompt_platform_read_char() の戻り値が 'A' であること。
}

// 標準入力が閉じられた場合に EOF が返ることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_returns_minus1_at_eof)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_poll, poll(_, _, _, _, 1U, -1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }));
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - read() が STDIN_FILENO、1 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - read() から、EOF を示す 0 を返す。

    // Act
    int actual_ret = prompt_platform_read_char(&handle_); // [手順] - prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret); // [確認_異常系] - prompt_platform_read_char() の戻り値が -1 であること。(EOF であることの確認)
}

// 端末サイズ変更の通知がリサイズ結果として返ることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_reports_resize_on_interrupted_read)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;
    NiceMock<Mock_unistd> mock_unistd;

    test_prompt_set_resize_pending(0); // [状態] - SIGWINCH 受信済みの状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_poll, poll(_, _, _, _, 1U, -1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }));
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(Invoke(
            [](const char *, int, const char *, int, void *, size_t)
            {
                test_prompt_set_resize_pending(1);
                errno = EINTR;
                return (ssize_t)-1;
            })); // [Pre-Assert確認_異常系] - read() が STDIN_FILENO、1 を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - read() から、errno に EINTR を設定して -1 を返す。

    // Act
    int actual_ret = prompt_platform_read_char(&handle_); // [手順] - prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(
        -2,
        actual_ret); // [確認_正常系] - prompt_platform_read_char() の戻り値が -2 であること。(リサイズ通知であることの確認)
    EXPECT_EQ(
        0,
        test_prompt_sigwinch_installed()); // [確認_正常系] - test_prompt_sigwinch_installed() の戻り値が 0 であること。(ハンドラー登録状態が変化しないことの確認)
}

// 割り込みでリサイズ通知がない場合に読み取りが継続されることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_retries_after_interrupt_without_resize)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;
    NiceMock<Mock_unistd> mock_unistd;

    test_prompt_set_resize_pending(0); // [状態] - SIGWINCH を受信していない状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_poll, poll(_, _, _, _, 1U, -1))
        .Times(2)
        .WillRepeatedly(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }));
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(
            DoAll(Assign(&errno, EIO),
                  Return(-1))); // [Pre-Assert確認_異常系] - read() が STDIN_FILENO、1 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - read() から、1 回目は errno に EINTR、2 回目は errno に EIO を設定して -1 を返す。

    // Act
    int actual_ret = prompt_platform_read_char(&handle_); // [手順] - prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(PROMPT_PLATFORM_ERROR,
              actual_ret); // [確認_異常系] - prompt_platform_read_char() の戻り値が PROMPT_PLATFORM_ERROR であること。
}

/*
 * prompt_platform_read_char_nb
 */

// 入力がある場合に 1 バイトが読み取れることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_nb_returns_next_byte_when_available)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_poll, poll(_, _, _, _, 1U, PROMPT_PLATFORM_SHORT_WAIT_MS))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            })); // [Pre-Assert確認_正常系] - poll() が STDIN_FILENO、1 件、50ms を指定して 1 回呼び出されること。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(
            [](const char *, int, const char *, int, void *buf, size_t) -> ssize_t
            {
                *static_cast<unsigned char *>(buf) = static_cast<unsigned char>('B');
                return 1;
            }); // [Pre-Assert確認_正常系] - read() が STDIN_FILENO、1 を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - read() から、'B' を書き込んで 1 を返す。

    // Act
    int actual_ret = prompt_platform_read_char_nb(&handle_); // [手順] - prompt_platform_read_char_nb() を呼び出す。

    // Assert
    EXPECT_EQ('B',
              actual_ret); // [確認_正常系] - prompt_platform_read_char_nb() の戻り値が 'B' であること。
}

// 入力がない場合にタイムアウトすることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_nb_reports_timeout)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;

    // Pre-Assert
    EXPECT_CALL(mock_poll, poll(_, _, _, _, 1U, PROMPT_PLATFORM_SHORT_WAIT_MS))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - poll() がタイムアウトを示す 0 を返す。

    // Act
    int actual_ret = prompt_platform_read_char_nb(&handle_); // [手順] - prompt_platform_read_char_nb() を呼び出す。

    // Assert
    EXPECT_EQ(
        PROMPT_PLATFORM_TIMEOUT,
        actual_ret); // [確認_異常系] - prompt_platform_read_char_nb() の戻り値が PROMPT_PLATFORM_TIMEOUT であること。(入力がないことの確認)
}

// 出力端末の幅を取得し、失敗または幅 0 の場合は 80 桁を返すことの確認
TEST_F(promptLinuxTest, columns_use_winsize_and_fallback)
{
    // Arrange
    NiceMock<Mock_ioctl> mock_ioctl;
    struct winsize size = {};
    size.ws_col = 100;
    // Pre-Assert
    EXPECT_CALL(mock_ioctl, ioctl(_, _, _, STDOUT_FILENO, TIOCGWINSZ, _))
        .WillOnce(DoAll(testing::Invoke([size](const char *, int, const char *, int, unsigned long, void *out)
                                        { *static_cast<struct winsize *>(out) = size; }),
                        Return(0)))
        .WillOnce(Return(-1))
        .WillOnce(DoAll(testing::Invoke([](const char *, int, const char *, int, unsigned long, void *out)
                                        { static_cast<struct winsize *>(out)->ws_col = 0; }),
                        Return(0)));
    // [Pre-Assert確認_正常系] - 出力端末のサイズを 3 回取得すること。
    // [Pre-Assert手順] - 正常値、失敗、幅 0 を返す。
    // Act
    size_t columns = platform_columns();  // [手順] - 正常な端末幅を取得する。
    size_t fallback = platform_columns(); // [手順] - 失敗時の端末幅を取得する。
    size_t zero = platform_columns();     // [手順] - 幅 0 の端末幅を取得する。
    // Assert
    EXPECT_EQ(100U, columns); // [確認_正常系] - 100 桁を取得すること。
    EXPECT_EQ(80U, fallback); // [確認_異常系] - 失敗時は 80 桁であること。
    EXPECT_EQ(80U, zero);     // [確認_異常系] - 幅 0 では 80 桁であること。
}

namespace
{

int g_linux_test_pipe2_flags = 0;

int fake_linux_test_pipe2(int fds[2], const int flags)
{
    g_linux_test_pipe2_flags = flags;
    fds[0] = 40;
    fds[1] = 41;
    return 0;
}

} // namespace

// 中止用パイプの原子的生成・書き込み・解放で stdin を閉じないことの確認
TEST_F(promptLinuxTest, cancel_pipe_init_stop_and_dispose_never_closes_stdin)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    g_linux_test_pipe2_flags = 0;
    test_prompt_set_pipe2_fn(fake_linux_test_pipe2);

    // Pre-Assert: pipe2(O_NONBLOCK|O_CLOEXEC)、stop で 1 回 write、dispose で 2 FD のみ close
    EXPECT_CALL(mock_unistd, write(_, _, _, 41, _, 1U)).WillOnce(Return(1));
    EXPECT_CALL(mock_unistd, close(_, _, _, STDIN_FILENO)).Times(0);
    EXPECT_CALL(mock_unistd, close(_, _, _, 40)).WillOnce(Return(0));
    EXPECT_CALL(mock_unistd, close(_, _, _, 41)).WillOnce(Return(0));

    // Act
    int init_ret = prompt_platform_init_cancelable(&handle_);
    int before_stop = prompt_platform_is_input_canceled(&handle_);
    int stop_ret = prompt_platform_stop_input(&handle_);
    int after_stop = prompt_platform_is_input_canceled(&handle_);
    prompt_platform_dispose(&handle_);
    prompt_platform_dispose(&handle_);
    prompt_platform_dispose(NULL);
    test_prompt_set_pipe2_fn(NULL);

    // Assert
    EXPECT_EQ(0, init_ret);
    EXPECT_EQ(O_NONBLOCK | O_CLOEXEC, g_linux_test_pipe2_flags);
    EXPECT_EQ(0, before_stop);
    EXPECT_EQ(CPLAT_OK, stop_ret);
    EXPECT_EQ(1, after_stop);
    EXPECT_EQ(0, handle_.cancel_pipe_active);
}

// POLLHUP / POLLERR / POLLNVAL / cancel パイプ / 中止要求の poll 境界を確認する
TEST_F(promptLinuxTest, poll_distinguishes_cancel_pipe_hup_and_errors)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;
    NiceMock<Mock_unistd> mock_unistd;
    handle_.cancel_pipe[0] = 50;
    handle_.cancel_pipe[1] = 51;
    handle_.cancel_pipe_active = 1;

    // Pre-Assert: POLLIN|POLLHUP -> 残存バイト読取後、POLLHUP 単体で EOF、その後パイプ通知で CANCELED
    EXPECT_CALL(mock_poll, poll(_, _, _, _, 2U, -1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN | POLLHUP;
                return 1;
            }))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLHUP;
                return 1;
            }))
        .WillOnce(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[1].revents = POLLIN;
                return 1;
            }));
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1U))
        .WillOnce(
            [](const char *, int, const char *, int, void *buf, size_t) -> ssize_t
            {
                *static_cast<unsigned char *>(buf) = 'Z';
                return 1;
            });

    // Act / Assert
    EXPECT_EQ('Z', prompt_platform_read_char(&handle_));
    EXPECT_EQ(PROMPT_PLATFORM_EOF, prompt_platform_read_char(&handle_));
    EXPECT_EQ(PROMPT_PLATFORM_CANCELED, prompt_platform_read_char(&handle_));
}

// 非 TTY の共通行入力が CRLF 除去、空行、EOF 前部分行、超過後の残余読取を処理できることの確認
TEST_F(promptLinuxTest, read_line_matches_crlf_empty_eof_and_truncation_remainder)
{
    // Arrange
    NiceMock<Mock_poll> mock_poll;
    NiceMock<Mock_unistd> mock_unistd;
    handle_.cancel_pipe[0] = 60;
    handle_.cancel_pipe[1] = 61;
    handle_.cancel_pipe_active = 1;

    static const char kStream[] = "ab\r\n\ncdefg\nxy";
    size_t pos = 0U;

    ON_CALL(mock_poll, poll(_, _, _, _, 2U, -1))
        .WillByDefault(Invoke(
            [](const char *, int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                return 1;
            }));
    ON_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1U))
        .WillByDefault(Invoke(
            [&pos](const char *, int, const char *, int, void *out, size_t) -> ssize_t
            {
                if (pos >= sizeof(kStream) - 1U)
                {
                    return 0;
                }
                *static_cast<unsigned char *>(out) = static_cast<unsigned char>(kStream[pos++]);
                return 1;
            }));

    char buf[5] = {};

    // Act / Assert
    EXPECT_EQ(CPLAT_OK, prompt_platform_read_line(&handle_, buf, sizeof(buf)));
    EXPECT_STREQ("ab", buf);

    EXPECT_EQ(CPLAT_OK, prompt_platform_read_line(&handle_, buf, sizeof(buf)));
    EXPECT_STREQ("", buf);

    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, prompt_platform_read_line(&handle_, buf, sizeof(buf)));
    EXPECT_STREQ("", buf);

    EXPECT_EQ(CPLAT_OK, prompt_platform_read_line(&handle_, buf, sizeof(buf)));
    EXPECT_STREQ("g", buf);

    EXPECT_EQ(CPLAT_OK, prompt_platform_read_line(&handle_, buf, sizeof(buf)));
    EXPECT_STREQ("xy", buf);

    EXPECT_EQ(CPLAT_ERR_EOF, prompt_platform_read_line(&handle_, buf, sizeof(buf)));
}

#endif /* PLATFORM_LINUX */
