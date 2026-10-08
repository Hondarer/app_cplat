#include <testfw.h>
#include <cplat/prompt/prompt_internal.h>

#if defined(PLATFORM_WINDOWS)

    #include <mock_windows.h>

class promptWindowsTest : public Test
{
  protected:
    cplat_prompt handle_ = {};
};

/*
 * cplat_internal_prompt_platform_enter_raw / cplat_internal_prompt_platform_leave_raw
 */

// 端末でない標準入力では raw モードへ移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_does_nothing_for_non_terminal)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            dummy_handle)); // [Pre-Assert確認_正常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - ダミー ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - GetConsoleMode が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active); // [確認_異常系] - GetConsoleMode が失敗するため raw モードにならないこと。
}

// 標準入力ハンドルが無効な場合に raw モードへ移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_does_nothing_when_stdin_handle_invalid)
{
    // Arrange
    Mock_windows mock_windows;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                                    // [Pre-Assert手順] - INVALID_HANDLE_VALUE を返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - GetConsoleMode が呼び出されないこと。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_異常系] - GetStdHandle が INVALID_HANDLE_VALUE を返すため raw モードにならないこと。
}

// 端末に対して raw モードへ移行できることと渡されるモード値が正しいことの確認
TEST_F(promptWindowsTest, enter_raw_succeeds_and_changes_mode)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    DWORD orig_mode = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
    DWORD captured_mode = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            dummy_handle)); // [Pre-Assert確認_正常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - ダミー ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(orig_mode),
                        Return(TRUE))); // [Pre-Assert確認_正常系] - GetConsoleMode が 1 回呼び出されること。
                                        // [Pre-Assert手順] - 元のコンソール モードを返却する。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(
            DoAll(SaveArg<4>(&captured_mode),
                  Return(TRUE))); // [Pre-Assert確認_正常系] - SetConsoleMode が新しいモードで 1 回呼び出されること。
                                  // [Pre-Assert手順] - TRUE を返却する。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(1, handle_.raw_active); // [確認_正常系] - raw モードが有効になること。
    EXPECT_NE(0U,
              captured_mode & (DWORD)ENABLE_VIRTUAL_TERMINAL_INPUT); // [確認_正常系] - 仮想端末入力が有効化されること。
    EXPECT_EQ(0U, captured_mode & (DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                                          ENABLE_PROCESSED_INPUT)); // [確認_正常系] - エコー・行入力・Ctrl+C
                                                                    // シグナル化が無効化されること。
}

// SetConsoleMode が失敗した場合に raw モードへ移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_does_nothing_when_set_console_mode_fails)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    DWORD orig_mode = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            dummy_handle)); // [Pre-Assert確認_正常系] - GetStdHandle が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - ダミー ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(DoAll(SetArgPointee<4>(orig_mode),
                        Return(TRUE))); // [Pre-Assert確認_正常系] - GetConsoleMode が 1 回呼び出されること。
                                        // [Pre-Assert手順] - 元のコンソール モードを返却する。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetConsoleMode が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active); // [確認_異常系] - SetConsoleMode が失敗するため raw モードが有効にならないこと。
}

// raw モード中の再入で二重に移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_is_ignored_while_already_raw)
{
    // Arrange
    handle_.raw_active = 1; // [状態] - すでに raw モードとして扱う。

    // Pre-Assert

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - raw モード中に cplat_internal_prompt_platform_enter_raw を呼び出す。

    // Assert
    EXPECT_EQ(1, handle_.raw_active); // [確認_正常系] - raw モードの状態が変化しないこと。

    // Cleanup
    handle_.raw_active = 0;
}

// raw モードでないときの復帰が何もしないことの確認
TEST_F(promptWindowsTest, leave_raw_is_ignored_when_not_raw)
{
    // Arrange
    handle_.raw_active = 0; // [状態] - raw モードでない状態とする。

    // Pre-Assert

    // Act
    cplat_internal_prompt_platform_leave_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_leave_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active); // [確認_正常系] - raw モードの状態が変化しないこと。
}

// raw モードからの復帰時に保存済みのコンソール モードで復元されることの確認
TEST_F(promptWindowsTest, leave_raw_restores_mode)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    DWORD captured_mode = 0;
    handle_.raw_active = 1;
    handle_.stdin_handle = dummy_handle;
    handle_.orig_in_mode =
        ENABLE_PROCESSED_INPUT; // [状態] - raw モード中で、復元対象のモードを保持している状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(DoAll(SaveArg<4>(&captured_mode),
                        Return(TRUE))); // [Pre-Assert確認_正常系] - SetConsoleMode が 1 回呼び出されること。
                                        // [Pre-Assert手順] - TRUE を返却する。

    // Act
    cplat_internal_prompt_platform_leave_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_leave_raw を呼び出す。

    // Assert
    EXPECT_EQ(0, handle_.raw_active);               // [確認_正常系] - raw モードが無効になること。
    EXPECT_EQ(handle_.orig_in_mode, captured_mode); // [確認_正常系] - 保存済みのモードで復元されること。
}

/*
 * cplat_internal_prompt_platform_read_char
 */

// 標準入力から 1 バイトが読み取れることの確認
TEST_F(promptWindowsTest, read_char_returns_next_byte)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    char byte_value = 'A';
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject が 100ms を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WAIT_OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, dummy_handle, _, 1U, _, _))
        .WillOnce(
            [byte_value](const char *, int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read,
                         LPOVERLAPPED)
            {
                *static_cast<char *>(buffer) = byte_value;
                *bytes_read = 1UL;
                return TRUE;
            }); // [Pre-Assert確認_正常系] - ReadFile が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力バッファーへ 'A' を書き込み、読み取りバイト数 1 と TRUE を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ('A', actual_ret); // [確認_正常系] - 読み取ったバイト値 'A' が返ること。
}

// 標準入力が閉じられた場合に EOF が返ることの確認
TEST_F(promptWindowsTest, read_char_returns_minus1_at_eof)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject が 100ms を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WAIT_OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, dummy_handle, _, 1U, _, _))
        .WillOnce(DoAll(SetArgPointee<6>(0UL),
                        Return(TRUE))); // [Pre-Assert確認_異常系] - ReadFile が 1 回呼び出されること。
                                        // [Pre-Assert手順] - 読み取りバイト数 0 と TRUE を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - EOF を示す -1 が返ること。
}

// 入力がない場合に待機がタイムアウトしリサイズ チェック通知が返ることの確認
TEST_F(promptWindowsTest, read_char_returns_minus2_on_timeout)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(WAIT_TIMEOUT)); // [Pre-Assert確認_正常系] - WaitForSingleObject が 1 回呼び出されること。
                                         // [Pre-Assert手順] - WAIT_TIMEOUT を返却する。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - ReadFile が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ(-2, actual_ret); // [確認_正常系] - リサイズ チェック用タイムアウトを示す -2 が返ること。
}

// 待機に失敗した場合に読み取り失敗が返ることの確認
TEST_F(promptWindowsTest, read_char_returns_minus1_when_wait_fails)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(WAIT_FAILED)); // [Pre-Assert確認_異常系] - WaitForSingleObject が 1 回呼び出されること。
    // [Pre-Assert手順] - WAIT_TIMEOUT でも WAIT_OBJECT_0 でもない WAIT_FAILED を返却する。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ReadFile が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - 読み取り失敗を示す -1 が返ること。
}

/*
 * cplat_internal_prompt_platform_read_char_nb
 */

// 入力がある場合に 1 バイトが読み取れることの確認
TEST_F(promptWindowsTest, read_char_nb_returns_next_byte_when_available)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    char byte_value = 'B';
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 50U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject が 50ms を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WAIT_OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - cplat_internal_prompt_platform_read_char への委譲で 100ms を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WAIT_OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, dummy_handle, _, 1U, _, _))
        .WillOnce(
            [byte_value](const char *, int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read,
                         LPOVERLAPPED)
            {
                *static_cast<char *>(buffer) = byte_value;
                *bytes_read = 1UL;
                return TRUE;
            }); // [Pre-Assert確認_正常系] - ReadFile が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力バッファーへ 'B' を書き込み、読み取りバイト数 1 と TRUE を返却する。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char_nb(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char_nb を呼び出す。

    // Assert
    EXPECT_EQ('B', actual_ret); // [確認_正常系] - 読み取ったバイト値 'B' が返ること。
}

// 入力がない場合にタイムアウトすることの確認
TEST_F(promptWindowsTest, read_char_nb_returns_minus1_on_timeout)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 50U))
        .WillOnce(Return(
            WAIT_TIMEOUT)); // [Pre-Assert確認_異常系] - WaitForSingleObject が 50ms を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - WAIT_TIMEOUT を返却する。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .Times(
            0); // [Pre-Assert確認_異常系] - cplat_internal_prompt_platform_read_char への委譲 (100ms 待機) が発生しないこと。
    EXPECT_CALL(mock_windows, ReadFile(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ReadFile が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char_nb(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char_nb を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - 50 ミリ秒待っても入力がないため -1 が返ること。
}

#endif /* PLATFORM_WINDOWS */
