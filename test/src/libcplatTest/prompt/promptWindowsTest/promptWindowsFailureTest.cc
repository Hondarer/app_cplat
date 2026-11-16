#include <testfw.h>
#include <mock_cplat.h>
#include "prompt_platform.h"
#if defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
    #include "prompt_windows.inject.h"

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

namespace
{

BOOL WINAPI fake_cancel_access_denied(HANDLE thread_handle)
{
    (void)thread_handle;
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
}

BOOL WINAPI fake_cancel_not_found(HANDLE thread_handle)
{
    (void)thread_handle;
    SetLastError(ERROR_NOT_FOUND);

    return FALSE;
}

BOOL WINAPI fake_cancel_true(HANDLE thread_handle)
{
    (void)thread_handle;

    return TRUE;
}

} // namespace

// CancelSynchronousIo の非 ERROR_NOT_FOUND エラー・DuplicateHandle 失敗・初期化失敗を検証する
TEST(promptWindowsFailureTest, stop_input_and_init_report_sync_io_and_resource_failures)
{
    // Arrange: NULL 引数拒否
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, prompt_platform_stop_input(nullptr));
    EXPECT_EQ(-1, prompt_platform_init_cancelable(nullptr));

    // init_cancelable の condvar / lock 作成失敗時にリークなく失敗すること
    {
        NiceMock<Mock_cplat> mock_cplat;
        prompt_platform_context fail_ctx = {};
        EXPECT_CALL(mock_cplat, cplat_local_lock_create(_)).WillOnce(Return(CPLAT_ERR_UNKNOWN));
        EXPECT_EQ(-1, prompt_platform_init_cancelable(&fail_ctx));

        EXPECT_CALL(mock_cplat, cplat_local_lock_create(_)).WillOnce(Invoke(delegate_real_cplat_local_lock_create));
        EXPECT_CALL(mock_cplat, cplat_condvar_create(_)).WillOnce(Return(CPLAT_ERR_UNKNOWN));
        EXPECT_CALL(mock_cplat, cplat_local_lock_dispose(_)).WillOnce(Invoke(delegate_real_cplat_local_lock_dispose));
        EXPECT_EQ(-1, prompt_platform_init_cancelable(&fail_ctx));
        EXPECT_EQ(nullptr, fail_ctx.input_lock);
    }

    // DuplicateHandle 失敗は PROMPT_PLATFORM_ERROR となり CancelSynchronousIo を呼ばない
    prompt_platform_context ctx = {};
    ASSERT_EQ(0, prompt_platform_init_cancelable(&ctx));
    {
        NiceMock<Mock_windows> mock_windows;
        EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, _, _, _, _, THREAD_TERMINATE, FALSE, 0U))
            .WillOnce(Return(FALSE));
        EXPECT_EQ(PROMPT_PLATFORM_ERROR, test_prompt_windows_begin_sync_input(&ctx));
    }

    // 入力区間在籍中に CancelSynchronousIo が ERROR_NOT_FOUND 以外を返したら CPLAT_ERR_UNKNOWN を返す
    {
        NiceMock<Mock_windows> mock_windows;
        HANDLE dup_handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x5555U));
        EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, _, _, _, _, THREAD_TERMINATE, FALSE, 0U))
            .WillOnce(DoAll(SetArgPointee<6>(dup_handle), Return(TRUE)));
        EXPECT_CALL(mock_windows, CloseHandle(_, _, _, dup_handle)).WillOnce(Return(TRUE));
        ASSERT_EQ(0, test_prompt_windows_begin_sync_input(&ctx));
        test_prompt_windows_set_cancel_sync_io_fn(fake_cancel_access_denied);
        EXPECT_EQ(CPLAT_ERR_UNKNOWN, prompt_platform_stop_input(&ctx));
        test_prompt_windows_reset_cancel_sync_io_fn();
        EXPECT_EQ(1, test_prompt_windows_end_sync_input(&ctx));
    }

    // ERROR_NOT_FOUND / TRUE 後の cplat_condvar_timed_wait 失敗時は待機ループを止め、中止状態を保って UNKNOWN を返す
    {
        NiceMock<Mock_windows> mock_windows;
        NiceMock<Mock_cplat> mock_cplat;
        prompt_platform_context wait_fail_ctx = {};
        ASSERT_EQ(0, prompt_platform_init_cancelable(&wait_fail_ctx));
        HANDLE dup_handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x6666U));
        EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, _, _, _, _, THREAD_TERMINATE, FALSE, 0U))
            .WillOnce(DoAll(SetArgPointee<6>(dup_handle), Return(TRUE)));
        EXPECT_CALL(mock_windows, CloseHandle(_, _, _, dup_handle)).WillOnce(Return(TRUE));
        EXPECT_CALL(mock_cplat, cplat_condvar_timed_wait(_, _, PROMPT_PLATFORM_SHORT_WAIT_MS))
            .WillOnce(Return(CPLAT_ERR_UNKNOWN))
            .WillOnce(Return(CPLAT_ERR_UNKNOWN));

        ASSERT_EQ(0, test_prompt_windows_begin_sync_input(&wait_fail_ctx));
        test_prompt_windows_set_cancel_sync_io_fn(fake_cancel_not_found);
        EXPECT_EQ(CPLAT_ERR_UNKNOWN, prompt_platform_stop_input(&wait_fail_ctx));
        EXPECT_EQ(1, prompt_platform_is_input_canceled(&wait_fail_ctx));

        test_prompt_windows_set_cancel_sync_io_fn(fake_cancel_true);
        EXPECT_EQ(CPLAT_ERR_UNKNOWN, prompt_platform_stop_input(&wait_fail_ctx));
        EXPECT_EQ(1, prompt_platform_is_input_canceled(&wait_fail_ctx));
        test_prompt_windows_reset_cancel_sync_io_fn();
        EXPECT_EQ(1, test_prompt_windows_end_sync_input(&wait_fail_ctx));
        prompt_platform_dispose(&wait_fail_ctx);
    }

    prompt_platform_dispose(&ctx);
    prompt_platform_dispose(nullptr);
}
#endif
