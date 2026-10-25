#include <testfw.h>
#include <mock_cplat.h>
#include "prompt_platform.h"
#if defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>

// 復元失敗時は保存値を維持し、成功した再試行で義務を解除することの確認
TEST(promptWindowsFailureTest, restoration_failure_keeps_saved_mode)
{
    // Arrange
    NiceMock<Mock_windows> mock;
    prompt_platform_context context = {};
    context.stdin_handle = (HANDLE)0x1234;
    context.orig_in_mode = 7U;
    context.raw_active = 1;
    // Pre-Assert
    EXPECT_CALL(mock, SetConsoleMode(_, _, _, context.stdin_handle, 7U)).WillOnce(Return(FALSE)).WillOnce(Return(TRUE));
    EXPECT_CALL(mock, GetConsoleMode(_, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 保存値の再取得をせず、復元を 2 回試みること。
    // Act
    int failed = prompt_platform_enter_raw(&context); // [手順] - 復元未完の再入力を試みる。
    int pending = context.raw_active;
    int retry = prompt_platform_leave_raw(&context); // [手順] - 保存値で再復元する。
    // Assert
    EXPECT_EQ(-1, failed);            // [確認_異常系] - 復元未完を返すこと。
    EXPECT_EQ(1, pending);            // [確認_異常系] - 復元義務を残すこと。
    EXPECT_EQ(0, retry);              // [確認_正常系] - 再復元できること。
    EXPECT_EQ(0, context.raw_active); // [確認_正常系] - 成功したら義務を解除すること。
}

// OS の読取り・待機失敗を EOF やタイムアウトと区別することの確認
TEST(promptWindowsFailureTest, console_read_and_short_wait_report_os_errors)
{
    // Arrange
    NiceMock<Mock_windows> mock;
    prompt_platform_context context = {};
    // Pre-Assert
    EXPECT_CALL(mock, WaitForSingleObject(_, _, _, _, 100U)).WillOnce(Return(WAIT_OBJECT_0));
    EXPECT_CALL(mock, ReadConsoleW(_, _, _, _, _, _, _, _)).WillOnce(Return(FALSE));
    EXPECT_CALL(mock, WaitForSingleObject(_, _, _, _, 50U)).WillOnce(Return(WAIT_FAILED));
    // [Pre-Assert確認_異常系] - 読取り失敗と短時間待機失敗を各 1 回返すこと。
    // Act
    int read_error = prompt_platform_read_char(&context);    // [手順] - 読取り失敗を取得する。
    int wait_error = prompt_platform_read_char_nb(&context); // [手順] - 待機失敗を取得する。
    // Assert
    EXPECT_EQ(PROMPT_PLATFORM_ERROR, read_error); // [確認_異常系] - 読取り失敗を通知すること。
    EXPECT_EQ(PROMPT_PLATFORM_ERROR, wait_error); // [確認_異常系] - 待機失敗を通知すること。
}
#endif
