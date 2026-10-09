#include <testfw.h>
#include <cplat/prompt/prompt_internal.h>
#include <mock_termios.h>
#include <mock_unistd.h>
#include <sys/mock_select.h>

#include <errno.h>

#if defined(PLATFORM_LINUX)

    #include <signal.h>
    #include <unistd.h>

    #include "prompt_linux.inject.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

class promptLinuxTest : public Test
{
  protected:
    cplat_prompt handle_ = {};

    // [サブ手順 名前=promptLinuxTest.SetUp]
    void SetUp() override
    {
        test_prompt_set_resize_pending(0);
    }
    // [サブ手順終了]
};

/*
 * cplat_internal_prompt_platform_enter_raw / cplat_internal_prompt_platform_leave_raw
 */

// 端末でない標準入力では raw モードへ移行しないことの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_raw_does_nothing_for_non_terminal)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - tcgetattr から -1 を返却する。

    // Act
    cplat_internal_prompt_platform_enter_raw(&handle_); // [手順] - cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active); // [確認_異常系] - tcgetattr が失敗するため raw モードにならないこと。
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
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr が元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).Times(2).WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が進入と復帰で 2 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr から 0 を返却する。

    // Act
    cplat_internal_prompt_platform_enter_raw(&handle_); // [手順] - cplat_internal_prompt_platform_enter_raw を呼び出す。
    int raw_after_enter = handle_.raw_active;
    int sigwinch_after_enter = test_prompt_sigwinch_installed();
    cplat_internal_prompt_platform_leave_raw(&handle_); // [手順] - cplat_internal_prompt_platform_leave_raw を呼び出す。

    // Assert
    EXPECT_EQ(1, raw_after_enter);                  // [確認_正常系] - raw モードが有効になること。
    EXPECT_EQ(1, sigwinch_after_enter);             // [確認_正常系] - SIGWINCH ハンドラーが登録されること。
    EXPECT_EQ(0, handle_.raw_active);               // [確認_正常系] - 復帰後に raw モードが無効になること。
    EXPECT_EQ(0, test_prompt_sigwinch_installed()); // [確認_正常系] - SIGWINCH ハンドラーが復元されること。
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
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr が元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - tcsetattr が標準入力を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - tcsetattr から -1 を返却する。

    // Act
    cplat_internal_prompt_platform_enter_raw(&handle_); // [手順] - cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active); // [確認_異常系] - 端末設定を適用できないため raw モードにならないこと。
    EXPECT_EQ(0, test_prompt_sigwinch_installed()); // [確認_異常系] - SIGWINCH ハンドラーが登録されないこと。
}

// raw モード中の再入で二重に移行しないことの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_raw_is_ignored_while_already_raw)
{
    // Arrange
    handle_.raw_active = 1; // [状態] - すでに raw モードとして扱う。

    // Pre-Assert

    // Act
    cplat_internal_prompt_platform_enter_raw(&handle_); // [手順] - raw モード中に cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(1, handle_.raw_active); // [確認_正常系] - raw モードの状態が変化しないこと。

    // Cleanup
    handle_.raw_active = 0;
}

// SIGWINCH ハンドラーの登録済み状態を保持して raw モードへ移行することの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, enter_raw_does_not_reinstall_sigwinch_handler)
{
    // Arrange
    NiceMock<Mock_termios> mock_termios;
    struct termios original = {};

    test_prompt_set_sigwinch_installed(1); // [状態] - SIGWINCH ハンドラーを登録済みの状態とする。

    // Pre-Assert
    EXPECT_CALL(mock_termios, tcgetattr(_, _, _, STDIN_FILENO, _))
        .WillOnce(DoAll(SetArgPointee<4>(original), Return(0)));
    // [Pre-Assert確認_正常系] - tcgetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcgetattr が元の端末設定を返却する。
    EXPECT_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr が 0 を返却する。

    // Act
    cplat_internal_prompt_platform_enter_raw(&handle_); // [手順] - 登録済み状態で cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(1, handle_.raw_active); // [確認_正常系] - raw モードが有効になること。
    EXPECT_EQ(1,
              test_prompt_sigwinch_installed()); // [確認_正常系] - SIGWINCH ハンドラーの登録済み状態が維持されること。

    // Cleanup
    handle_.raw_active = 0;
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
    cplat_internal_prompt_platform_leave_raw(&handle_); // [手順] - cplat_internal_prompt_platform_leave_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active); // [確認_正常系] - raw モードの状態が変化しないこと。
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
    // [Pre-Assert確認_正常系] - tcsetattr が標準入力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - tcsetattr が 0 を返却する。

    // Act
    cplat_internal_prompt_platform_leave_raw(&handle_); // [手順] - 未登録状態で cplat_internal_prompt_platform_leave_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active);               // [確認_正常系] - raw モードが無効になること。
    EXPECT_EQ(0, test_prompt_sigwinch_installed()); // [確認_正常系] - SIGWINCH ハンドラーの未登録状態が維持されること。
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
            DoAll(SetArgPointee<4>(original), Return(0))); // [状態] - tcgetattr が元の端末設定を返すようにする。
    ON_CALL(mock_termios, tcsetattr(_, _, _, STDIN_FILENO, _, _))
        .WillByDefault(Return(0)); // [状態] - tcsetattr が成功するようにする。
    cplat_internal_prompt_platform_enter_raw(&handle_);
    ASSERT_EQ(1, test_prompt_sigwinch_installed()); // [状態確認] - SIGWINCH ハンドラーが登録済みであること。

    // Pre-Assert

    // Act
    raise(SIGWINCH); // [手順] - 自プロセスへ SIGWINCH を送出する。

    // Assert
    EXPECT_EQ(1, test_prompt_resize_pending()); // [確認_正常系] - リサイズ待ちとして記録されること。

    // Cleanup
    test_prompt_set_resize_pending(0);
    cplat_internal_prompt_platform_leave_raw(&handle_);
}

/*
 * cplat_internal_prompt_platform_read_char
 */

// 標準入力から 1 バイトが読み取れることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_returns_next_byte)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(
            [](const char *, int, const char *, int, void *buf, size_t) -> ssize_t
            {
                *static_cast<unsigned char *>(buf) = static_cast<unsigned char>('A');
                return 1;
            }); // [Pre-Assert確認_正常系] - read が標準入力に対し 1 バイトを指定して 1 回呼び出されること。
                // [Pre-Assert手順] - 'A' を書き込み、1 を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(&handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ('A', actual_ret); // [確認_正常系] - 読み取ったバイト値 'A' が返ること。
}

// 標準入力が閉じられた場合に EOF が返ることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_returns_minus1_at_eof)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - read が標準入力に対し 1 バイトを指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - EOF を示す 0 を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(&handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - EOF を示す -1 が返ること。
}

// 端末サイズ変更の通知がリサイズ結果として返ることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_reports_resize_on_interrupted_read)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    test_prompt_set_resize_pending(1); // [状態] - SIGWINCH 受信済みの状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(DoAll(
            Assign(&errno, EINTR),
            Return(-1))); // [Pre-Assert確認_異常系] - read が標準入力に対し 1 バイトを指定して 1 回呼び出されること。
                          // [Pre-Assert手順] - errno に EINTR を設定し、read から -1 を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(&handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ(-2, actual_ret);                             // [確認_正常系] - リサイズ通知を示す -2 が返ること。
    EXPECT_EQ(0, test_prompt_sigwinch_installed()); // [確認_正常系] - ハンドラー登録状態は変化しないこと。
}

// 割り込みでリサイズ通知がない場合に読み取りが継続されることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_retries_after_interrupt_without_resize)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    test_prompt_set_resize_pending(0); // [状態] - SIGWINCH を受信していない状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(DoAll(Assign(&errno, EIO),
                        Return(-1))); // [Pre-Assert確認_異常系] - read が標準入力に対し 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は errno に EINTR、2 回目は errno に EIO を設定して -1 を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(&handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - 再試行の結果として -1 が返ること。
}

/*
 * cplat_internal_prompt_platform_read_char_nb
 */

// 入力がある場合に 1 バイトが読み取れることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_nb_returns_next_byte_when_available)
{
    // Arrange
    NiceMock<Mock_sys_select> mock_select;
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_select, select(_, _, _, STDIN_FILENO + 1, _, _, _, _))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - select が 1 回呼び出されること。
                              // [Pre-Assert手順] - 入力ありを示す 1 を返却する。
    EXPECT_CALL(mock_unistd, read(_, _, _, STDIN_FILENO, _, 1u))
        .WillOnce(
            [](const char *, int, const char *, int, void *buf, size_t) -> ssize_t
            {
                *static_cast<unsigned char *>(buf) = static_cast<unsigned char>('B');
                return 1;
            }); // [Pre-Assert確認_正常系] - read が標準入力に対し 1 バイトを指定して 1 回呼び出されること。
                // [Pre-Assert手順] - 'B' を書き込み、1 を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char_nb(&handle_); // [手順] - cplat_internal_prompt_platform_read_char_nb を呼び出す。

    // Assert
    EXPECT_EQ('B', actual_ret); // [確認_正常系] - 読み取ったバイト値 'B' が返ること。
}

// 入力がない場合にタイムアウトすることの確認
// [サブ手順参照 名前=promptLinuxTest.SetUp]
TEST_F(promptLinuxTest, read_char_nb_returns_minus1_on_timeout)
{
    // Arrange
    NiceMock<Mock_sys_select> mock_select;

    // Pre-Assert
    EXPECT_CALL(mock_select, select(_, _, _, STDIN_FILENO + 1, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - select が 1 回呼び出されること。
                              // [Pre-Assert手順] - タイムアウトを示す 0 を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char_nb(&handle_); // [手順] - cplat_internal_prompt_platform_read_char_nb を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - 入力がないため -1 が返ること。
}

#endif /* PLATFORM_LINUX */
