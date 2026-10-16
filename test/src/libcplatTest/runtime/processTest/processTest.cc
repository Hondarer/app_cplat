#include <testfw.h>
#include <mock_cplat.h>
#include <mock_fcntl.h>
#include <mock_time.h>
#include <mock_unistd.h>
#include <sys/mock_wait.h>

#include <cplat/base/platform.h>
#include <cplat/base/result_internal.h>
#include <cplat/crt/path.h>
#include <cplat/runtime/process.h>
#include <cplat/runtime/process_internal.h>

#include "process.inject.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <thread>

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;

#if defined(PLATFORM_LINUX)
    #include <fcntl.h>
    #include <signal.h>
    #include <unistd.h>
static void set_invalid_stdio_mode(cplat_process_stdio *spec)
{
    int invalid_mode = 99;

    memcpy(&spec->mode, &invalid_mode, sizeof(spec->mode));
}
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif /* PLATFORM_ */

static void get_tid_in_thread(uint32_t *tid_out)
{
    *tid_out = cplat_process_get_tid();
}

// errno が共通結果コードへ分類されることの確認
TEST(processTest, MapsErrnoToCommonResults)
{
    // Arrange

    // Pre-Assert

    // Act
    int invalid_result = cplat_internal_result_from_errno(EINVAL);    // [手順] - EINVAL を共通結果コードへ変換する。
    int permission_result = cplat_internal_result_from_errno(EACCES); // [手順] - EACCES を共通結果コードへ変換する。
    int timeout_result = cplat_internal_result_from_errno(ETIMEDOUT); // [手順] - ETIMEDOUT を共通結果コードへ変換する。
    int busy_result = cplat_internal_result_from_errno(EBUSY);        // [手順] - EBUSY を共通結果コードへ変換する。
    int memory_result = cplat_internal_result_from_errno(ENOMEM);     // [手順] - ENOMEM を共通結果コードへ変換する。
    int not_found_result = cplat_internal_result_from_errno(ENOENT);  // [手順] - ENOENT を共通結果コードへ変換する。
    int other_result = cplat_internal_result_from_errno(EDOM);        // [手順] - EDOM を共通結果コードへ変換する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_result); // [確認_正常系] - EINVAL に対する cplat_internal_result_from_errno() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        permission_result); // [確認_正常系] - EACCES に対する cplat_internal_result_from_errno() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        timeout_result); // [確認_正常系] - ETIMEDOUT に対する cplat_internal_result_from_errno() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(
        CPLAT_ERR_BUSY,
        busy_result); // [確認_正常系] - EBUSY に対する cplat_internal_result_from_errno() の戻り値が CPLAT_ERR_BUSY であること。
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        memory_result); // [確認_正常系] - ENOMEM に対する cplat_internal_result_from_errno() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        not_found_result); // [確認_正常系] - ENOENT に対する cplat_internal_result_from_errno() の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        other_result); // [確認_正常系] - EDOM に対する cplat_internal_result_from_errno() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

#if defined(PLATFORM_WINDOWS)
// Windows のバッファー不足が共通結果コードへ分類されることの確認
TEST(processTest, MapsWindowsInsufficientBuffer)
{
    // Arrange

    // Pre-Assert

    // Act
    int result = cplat_internal_result_from_windows_error(
        ERROR_INSUFFICIENT_BUFFER); // [手順] - ERROR_INSUFFICIENT_BUFFER を共通結果コードへ変換する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        result); // [確認_正常系] - ERROR_INSUFFICIENT_BUFFER に対する cplat_internal_result_from_windows_error() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}
#endif

#if defined(PLATFORM_LINUX)
// 同期実行が子プロセスの終了コードを返すことの確認
TEST(processTest, RunSyncReturnsChildExitCode)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_wait> mock_sys_wait;
    cplat_process_options options = {};
    char arg0[] = "/bin/sh";
    char arg1[] = "-c";
    char arg2[] = "exit 7";
    char *argv[] = {arg0, arg1, arg2, nullptr};
    int exit_code = 0;
    int status = 7 << 8;

    options.argv = argv; // [状態] - 終了コード 7 を返す argv とする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, fork(_, _, _))
        .WillOnce(Return(static_cast<pid_t>(4242))); // [Pre-Assert確認_正常系] - fork() が 1 回呼び出されること。
                                                     // [Pre-Assert手順] - 子プロセス pid 4242 を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 4242, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(status), Return(static_cast<pid_t>(4242))));
    // [Pre-Assert確認_正常系] - waitpid() が 4242 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 終了ステータス 7 を設定して 4242 を返却する。

    // Act
    int result = cplat_process_run_sync(&options, CPLAT_PROCESS_WAIT_FOREVER,
                                        &exit_code); // [手順] - cplat_process_run_sync() を無期限待機で呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);       // [確認_正常系] - cplat_process_run_sync() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(7, exit_code); // [確認_正常系] - exit_code が 7 であること。(子プロセスの終了コードが 7 であることの確認)
}

// 環境変数の上書きを付けた同期実行が成功することの確認
TEST(processTest, EnvironmentOverridesAreAcceptedByRunSync)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_wait> mock_sys_wait;
    cplat_process_options options = {};
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    char env_value[] = "CPLAT_PROCESS_TEST_VALUE=override-value";
    char *env_overrides[] = {env_value, nullptr};
    int exit_code = 0;
    int status = 0;

    options.argv = argv;
    options.env_overrides =
        env_overrides; // [状態] - 環境変数 CPLAT_PROCESS_TEST_VALUE を "override-value" に上書きする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, fork(_, _, _))
        .WillOnce(Return(static_cast<pid_t>(4243))); // [Pre-Assert確認_正常系] - fork() が 1 回呼び出されること。
                                                     // [Pre-Assert手順] - 子プロセス pid 4243 を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 4243, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(status), Return(static_cast<pid_t>(4243))));
    // [Pre-Assert確認_正常系] - waitpid() が 4243 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 終了ステータス 0 を設定して 4243 を返却する。

    // Act
    int result = cplat_process_run_sync(&options, CPLAT_PROCESS_WAIT_FOREVER,
                                        &exit_code); // [手順] - 環境変数上書き付きで同期実行する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_process_run_sync() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code); // [確認_正常系] - exit_code が 0 であること。(子プロセスの終了コードが 0 であることの確認)
}

// 実行中プロセスへの NO_WAIT 待機が TIMEOUT を報告することの確認
TEST(processTest, WaitNoWaitReportsTimeoutForRunningProcess)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_wait> mock_sys_wait;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    int exit_code = 0;
    int status = 0;

    options.argv = argv; // [状態] - 起動後に未終了として扱う argv とする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, fork(_, _, _))
        .WillOnce(Return(static_cast<pid_t>(4244))); // [Pre-Assert確認_正常系] - fork() が 1 回呼び出されること。
                                                     // [Pre-Assert手順] - 子プロセス pid 4244 を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 4244, _, WNOHANG))
        .WillOnce(Return(static_cast<pid_t>(
            0))); // [Pre-Assert確認_正常系] - waitpid() が 4244、WNOHANG を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - 未終了を示す 0 を返却する。
    EXPECT_CALL(mock_unistd, kill(_, _, _, 4244, SIGTERM))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - kill() が 4244、SIGTERM を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - kill から 0 を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 4244, _, 0))
        .WillOnce(DoAll(SetArgPointee<4>(status), Return(static_cast<pid_t>(4244))));
    // [Pre-Assert確認_正常系] - waitpid() が 4244、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 終了ステータス 0 を設定して 4244 を返却する。

    // Act
    int start_result =
        cplat_process_start(&options, &process); // [手順] - cplat_process_start() で子プロセスを起動する。
    ASSERT_EQ(CPLAT_OK, start_result);
    // [確認_正常系] - cplat_process_start() の戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, process);
    // [確認_正常系] - process が nullptr 以外であること。

    int wait_result = cplat_process_wait(process, CPLAT_PROCESS_NO_WAIT);     // [手順] - NO_WAIT で待機する。
    int terminate_result = cplat_process_terminate(process);                  // [手順] - 子プロセスを terminate する。
    int final_wait = cplat_process_wait(process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - 無期限待機で終了を待つ。
    int exit_result = cplat_process_get_exit_code(process, &exit_code);       // [手順] - 終了コードを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        wait_result); // [確認_正常系] - CPLAT_PROCESS_NO_WAIT の cplat_process_wait() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(CPLAT_OK, terminate_result); // [確認_正常系] - cplat_process_terminate() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              final_wait); // [確認_正常系] - 無期限待機の cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              exit_result); // [確認_正常系] - cplat_process_get_exit_code() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_process_dispose(process);
}
#elif defined(PLATFORM_WINDOWS)
// Windows の待機が子プロセスの終了コードを返すことの確認
TEST(processTest, WaitReturnsChildExitCode)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    HANDLE fake_process = reinterpret_cast<HANDLE>(0x70);
    cplat_process *process = cplat_internal_process_adopt_native(reinterpret_cast<intptr_t>(fake_process));
    int exit_code = 0;

    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, fake_process, INFINITE))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が fake_process、INFINITE を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WAIT_OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, GetExitCodeProcess(_, _, _, fake_process, _))
        .WillOnce(DoAll(SetArgPointee<4>(7), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetExitCodeProcess() が fake_process を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 終了コード 7 を設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, fake_process)).WillOnce(Return(TRUE)); // Cleanup の destroy 用
    // [Pre-Assert確認_正常系] - CloseHandle() が fake_process を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    int wait_result = cplat_process_wait(process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - 無期限待機で終了を待つ。
    int exit_result = cplat_process_get_exit_code(process, &exit_code);        // [手順] - 終了コードを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, wait_result); // [確認_正常系] - cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              exit_result);  // [確認_正常系] - cplat_process_get_exit_code() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(7, exit_code); // [確認_正常系] - exit_code が 7 であること。(子プロセスの終了コードが 7 であることの確認)

    // Cleanup
    cplat_process_dispose(process);
}

// 実行中プロセスへの NO_WAIT 待機が TIMEOUT を報告することの確認
TEST(processTest, WaitNoWaitReportsTimeoutForAdoptedProcess)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    HANDLE fake_process = reinterpret_cast<HANDLE>(0x71);
    cplat_process *process = cplat_internal_process_adopt_native(reinterpret_cast<intptr_t>(fake_process));
    int exit_code = 0;

    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, fake_process, 0U))
        .WillOnce(Return(
            WAIT_TIMEOUT)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が fake_process、0U を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - WAIT_TIMEOUT を返却する。
    EXPECT_CALL(mock_windows, TerminateProcess(_, _, _, fake_process, EXIT_FAILURE))
        .WillOnce(Return(
            TRUE)); // [Pre-Assert確認_正常系] - TerminateProcess() が fake_process、EXIT_FAILURE を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, fake_process, INFINITE))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が fake_process、INFINITE を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WAIT_OBJECT_0 を返却する。
    EXPECT_CALL(mock_windows, GetExitCodeProcess(_, _, _, fake_process, _))
        .WillOnce(DoAll(SetArgPointee<4>(1), Return(TRUE)));
    // [Pre-Assert確認_正常系] - GetExitCodeProcess() が fake_process を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 終了コード 1 を設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, fake_process)).WillOnce(Return(TRUE)); // Cleanup の destroy 用
    // [Pre-Assert確認_正常系] - CloseHandle() が fake_process を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    int wait_result = cplat_process_wait(process, CPLAT_PROCESS_NO_WAIT);     // [手順] - NO_WAIT で待機する。
    int terminate_result = cplat_process_terminate(process);                  // [手順] - 子プロセスを terminate する。
    int final_wait = cplat_process_wait(process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - 無期限待機で終了を待つ。
    int exit_result = cplat_process_get_exit_code(process, &exit_code);       // [手順] - 終了コードを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        wait_result); // [確認_正常系] - CPLAT_PROCESS_NO_WAIT の cplat_process_wait() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(CPLAT_OK, terminate_result); // [確認_正常系] - cplat_process_terminate() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              final_wait); // [確認_正常系] - 無期限待機の cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              exit_result); // [確認_正常系] - cplat_process_get_exit_code() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_process_dispose(process);
}

// Windows の process_start が CreateProcessW 成功でハンドルを返すことの確認
TEST(processTest, StartCreatesProcessWithCreateProcessW)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_cplat> mock_cplat;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "C:\\Windows\\System32\\cmd.exe";
    char *argv[] = {arg0, nullptr};
    HANDLE process_handle = reinterpret_cast<HANDLE>(0x80);
    HANDLE thread_handle = reinterpret_cast<HANDLE>(0x81);
    HANDLE stdio_handle = reinterpret_cast<HANDLE>(0x90);

    options.argv = argv; // [状態] - cmd.exe を起動する argv とする。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;
    options.stderr_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, CreateFileU(_, _, _, _, _, _, _)).Times(3).WillRepeatedly(Return(stdio_handle));
    // [Pre-Assert確認_正常系] - CreateFileU() が 3 回呼び出されること。
    // [Pre-Assert手順] - ダミーの標準ハンドルを返却する。
    EXPECT_CALL(mock_windows, InitializeProcThreadAttributeList(_, _, _, _, 1U, 0U, _))
        .WillOnce(
            [](const char *, const int, const char *, LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T size)
            {
                *size = 64U;
                return FALSE;
            })
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - InitializeProcThreadAttributeList() が 1U、0U を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は必要サイズ 64 を設定し、2 回目は TRUE を返却する。
    EXPECT_CALL(mock_windows, UpdateProcThreadAttribute(_, _, _, _, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, _, _, _, _))
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - UpdateProcThreadAttribute() が 0U、PROC_THREAD_ATTRIBUTE_HANDLE_LIST を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, CreateProcessW(_, _, _, _, _, _, _, _, _, _, _, _, _))
        .WillOnce(
            [process_handle, thread_handle](const char *, const int, const char *, LPCWSTR, LPWSTR,
                                            LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR,
                                            LPSTARTUPINFOW, LPPROCESS_INFORMATION info)
            {
                info->hProcess = process_handle;
                info->hThread = thread_handle;
                info->dwProcessId = 1000U;
                info->dwThreadId = 1001U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - CreateProcessW() が 1 回呼び出されること。
    // [Pre-Assert手順] - プロセス ハンドルとスレッド ハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, DeleteProcThreadAttributeList(_, _, _, _)).Times(1);
    // [Pre-Assert確認_正常系] - DeleteProcThreadAttributeList() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, _)).WillRepeatedly(Return(TRUE));
    // [Pre-Assert確認_正常系] - CloseHandle() が任意の回数呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    int result = cplat_process_start(&options, &process); // [手順] - CreateProcessW 成功状態でプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_process_start() の戻り値が CPLAT_OK であること。
    EXPECT_NE(nullptr, process); // [確認_正常系] - process が nullptr 以外であること。

    // Cleanup
    cplat_process_dispose(process);
}

// Windows の process_start が CreateProcessW 失敗を返すことの確認
TEST(processTest, StartReportsCreateProcessWFailure)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_cplat> mock_cplat;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "C:\\Windows\\System32\\cmd.exe";
    char *argv[] = {arg0, nullptr};
    HANDLE stdio_handle = reinterpret_cast<HANDLE>(0x91);

    options.argv = argv; // [状態] - cmd.exe を起動する argv とする。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;
    options.stderr_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, CreateFileU(_, _, _, _, _, _, _)).Times(3).WillRepeatedly(Return(stdio_handle));
    // [Pre-Assert確認_異常系] - CreateFileU() が 3 回呼び出されること。
    // [Pre-Assert手順] - ダミーの標準ハンドルを返却する。
    EXPECT_CALL(mock_windows, InitializeProcThreadAttributeList(_, _, _, _, 1U, 0U, _))
        .WillOnce(
            [](const char *, const int, const char *, LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T size)
            {
                *size = 64U;
                return FALSE;
            })
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_異常系] - InitializeProcThreadAttributeList() が 1U、0U を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は必要サイズ 64 を設定し、2 回目は TRUE を返却する。
    EXPECT_CALL(mock_windows, UpdateProcThreadAttribute(_, _, _, _, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, _, _, _, _))
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_異常系] - UpdateProcThreadAttribute() が 0U、PROC_THREAD_ATTRIBUTE_HANDLE_LIST を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, CreateProcessW(_, _, _, _, _, _, _, _, _, _, _, _, _)).WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - CreateProcessW() が 1 回呼び出されること。
    // [Pre-Assert手順] - FALSE を返却する。
    EXPECT_CALL(mock_windows, DeleteProcThreadAttributeList(_, _, _, _)).Times(1);
    // [Pre-Assert確認_異常系] - DeleteProcThreadAttributeList() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, _)).WillRepeatedly(Return(TRUE));
    // [Pre-Assert確認_異常系] - CloseHandle() が任意の回数呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    int result = cplat_process_start(&options, &process); // [手順] - CreateProcessW 失敗状態でプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_process_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        nullptr,
        process); // [確認_異常系] - process が nullptr であること。(CreateProcessW 失敗時に process が NULL のままであることの確認)
}

// Windows の process_start が親の標準ハンドルを複製して起動することの確認
TEST(processTest, StartCreatesProcessWithInheritedStdio)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "C:\\Windows\\System32\\cmd.exe";
    char *argv[] = {arg0, nullptr};
    HANDLE current_process = reinterpret_cast<HANDLE>(0x50);
    HANDLE stdin_source = reinterpret_cast<HANDLE>(0x10);
    HANDLE stdout_source = reinterpret_cast<HANDLE>(0x11);
    HANDLE stderr_source = reinterpret_cast<HANDLE>(0x12);
    HANDLE stdin_dup = reinterpret_cast<HANDLE>(0x20);
    HANDLE stdout_dup = reinterpret_cast<HANDLE>(0x21);
    HANDLE stderr_dup = reinterpret_cast<HANDLE>(0x22);
    HANDLE process_handle = reinterpret_cast<HANDLE>(0x80);
    HANDLE thread_handle = reinterpret_cast<HANDLE>(0x81);

    options.argv = argv;                                    // [状態] - cmd.exe を起動する argv とする。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;  // [状態] - stdin を親ハンドル継承とする。
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_INHERIT; // [状態] - stdout を親ハンドル継承とする。
    options.stderr_spec.mode = CPLAT_PROCESS_STDIO_INHERIT; // [状態] - stderr を親ハンドル継承とする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(stdin_source));
    // [Pre-Assert確認_正常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 親の標準入力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).WillOnce(Return(stdout_source));
    // [Pre-Assert確認_正常系] - GetStdHandle() が STD_OUTPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 親の標準出力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_ERROR_HANDLE)).WillOnce(Return(stderr_source));
    // [Pre-Assert確認_正常系] - GetStdHandle() が STD_ERROR_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 親の標準エラー ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetCurrentProcess(_, _, _)).Times(3).WillRepeatedly(Return(current_process));
    // [Pre-Assert確認_正常系] - GetCurrentProcess() が 3 回呼び出されること。
    // [Pre-Assert手順] - 現在プロセスのダミー ハンドルを返却する。
    EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, current_process, stdin_source, current_process, _, 0U, TRUE,
                                              DUPLICATE_SAME_ACCESS))
        .WillOnce(
            [stdin_dup](const char *, const int, const char *, HANDLE, HANDLE, HANDLE, LPHANDLE target, DWORD, BOOL,
                        DWORD)
            {
                *target = stdin_dup;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - DuplicateHandle() が current_process、stdin_source、current_process、0U、TRUE、DUPLICATE_SAME_ACCESS を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 複製後の標準入力ハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, current_process, stdout_source, current_process, _, 0U, TRUE,
                                              DUPLICATE_SAME_ACCESS))
        .WillOnce(
            [stdout_dup](const char *, const int, const char *, HANDLE, HANDLE, HANDLE, LPHANDLE target, DWORD, BOOL,
                         DWORD)
            {
                *target = stdout_dup;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - DuplicateHandle() が current_process、stdout_source、current_process、0U、TRUE、DUPLICATE_SAME_ACCESS を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 複製後の標準出力ハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, current_process, stderr_source, current_process, _, 0U, TRUE,
                                              DUPLICATE_SAME_ACCESS))
        .WillOnce(
            [stderr_dup](const char *, const int, const char *, HANDLE, HANDLE, HANDLE, LPHANDLE target, DWORD, BOOL,
                         DWORD)
            {
                *target = stderr_dup;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - DuplicateHandle() が current_process、stderr_source、current_process、0U、TRUE、DUPLICATE_SAME_ACCESS を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 複製後の標準エラー ハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, InitializeProcThreadAttributeList(_, _, _, _, 1U, 0U, _))
        .WillOnce(
            [](const char *, const int, const char *, LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T size)
            {
                *size = 64U;
                return FALSE;
            })
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - InitializeProcThreadAttributeList() が 1U、0U を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は必要サイズ 64 を設定し、2 回目は TRUE を返却する。
    EXPECT_CALL(mock_windows, UpdateProcThreadAttribute(_, _, _, _, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, _, _, _, _))
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - UpdateProcThreadAttribute() が 0U、PROC_THREAD_ATTRIBUTE_HANDLE_LIST を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, CreateProcessW(_, _, _, _, _, _, _, _, _, _, _, _, _))
        .WillOnce(
            [process_handle, thread_handle](const char *, const int, const char *, LPCWSTR, LPWSTR,
                                            LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR,
                                            LPSTARTUPINFOW, LPPROCESS_INFORMATION info)
            {
                info->hProcess = process_handle;
                info->hThread = thread_handle;
                info->dwProcessId = 1000U;
                info->dwThreadId = 1001U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - CreateProcessW() が 1 回呼び出されること。
    // [Pre-Assert手順] - プロセス ハンドルとスレッド ハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, DeleteProcThreadAttributeList(_, _, _, _)).Times(1);
    // [Pre-Assert確認_正常系] - DeleteProcThreadAttributeList() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, _)).WillRepeatedly(Return(TRUE));
    // [Pre-Assert確認_正常系] - CloseHandle() が任意の回数呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    int result =
        cplat_process_start(&options, &process); // [手順] - 親の標準ハンドルを継承する設定でプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_process_start() の戻り値が CPLAT_OK であること。
    EXPECT_NE(nullptr, process); // [確認_正常系] - process が nullptr 以外であること。

    // Cleanup
    cplat_process_dispose(process);
}

// Windows の process_start が DuplicateHandle 失敗を返すことの確認
TEST(processTest, StartReportsDuplicateHandleFailure)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "C:\\Windows\\System32\\cmd.exe";
    char *argv[] = {arg0, nullptr};
    HANDLE current_process = reinterpret_cast<HANDLE>(0x50);
    HANDLE stdin_source = reinterpret_cast<HANDLE>(0x10);

    options.argv = argv;                                    // [状態] - cmd.exe を起動する argv とする。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;  // [状態] - stdin を親ハンドル継承とする。
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_INHERIT; // [状態] - stdout を親ハンドル継承とする。
    options.stderr_spec.mode = CPLAT_PROCESS_STDIO_INHERIT; // [状態] - stderr を親ハンドル継承とする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE)).WillOnce(Return(stdin_source));
    // [Pre-Assert確認_異常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 親の標準入力ハンドルを返却する。
    EXPECT_CALL(mock_windows, GetCurrentProcess(_, _, _)).WillOnce(Return(current_process));
    // [Pre-Assert確認_異常系] - GetCurrentProcess() が 1 回呼び出されること。
    // [Pre-Assert手順] - 現在プロセスのダミー ハンドルを返却する。
    EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, current_process, stdin_source, current_process, _, 0U, TRUE,
                                              DUPLICATE_SAME_ACCESS))
        .WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - DuplicateHandle() が current_process、stdin_source、current_process、0U、TRUE、DUPLICATE_SAME_ACCESS を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - FALSE を返却する。

    // Act
    int result = cplat_process_start(&options, &process); // [手順] - DuplicateHandle 失敗状態でプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_process_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        nullptr,
        process); // [確認_異常系] - process が nullptr であること。(DuplicateHandle 失敗時に process が NULL のままであることの確認)
}

// Windows の process_start が無効な標準ハンドルを null device へフォールバックすることの確認
TEST(processTest, StartFallsBackToNullDeviceWhenStdHandleInvalid)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_cplat> mock_cplat;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "C:\\Windows\\System32\\cmd.exe";
    char *argv[] = {arg0, nullptr};
    HANDLE null_handle = reinterpret_cast<HANDLE>(0x90);
    HANDLE process_handle = reinterpret_cast<HANDLE>(0x80);
    HANDLE thread_handle = reinterpret_cast<HANDLE>(0x81);

    options.argv = argv;                                    // [状態] - cmd.exe を起動する argv とする。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;  // [状態] - stdin を親ハンドル継承とする。
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_INHERIT; // [状態] - stdout を親ハンドル継承とする。
    options.stderr_spec.mode = CPLAT_PROCESS_STDIO_INHERIT; // [状態] - stderr を親ハンドル継承とする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, _)).Times(3).WillRepeatedly(Return(INVALID_HANDLE_VALUE));
    // [Pre-Assert確認_異常系] - GetStdHandle() が 3 回呼び出されること。
    // [Pre-Assert手順] - INVALID_HANDLE_VALUE を返却する。
    EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, _, _, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - DuplicateHandle() が呼び出されないこと。
    EXPECT_CALL(mock_cplat, CreateFileU(_, _, _, _, _, _, _)).Times(3).WillRepeatedly(Return(null_handle));
    // [Pre-Assert確認_正常系] - CreateFileU() が 3 回呼び出されること。
    // [Pre-Assert手順] - ダミーの null device ハンドルを返却する。
    EXPECT_CALL(mock_windows, InitializeProcThreadAttributeList(_, _, _, _, 1U, 0U, _))
        .WillOnce(
            [](const char *, const int, const char *, LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T size)
            {
                *size = 64U;
                return FALSE;
            })
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - InitializeProcThreadAttributeList() が 1U、0U を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は必要サイズ 64 を設定し、2 回目は TRUE を返却する。
    EXPECT_CALL(mock_windows, UpdateProcThreadAttribute(_, _, _, _, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, _, _, _, _))
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - UpdateProcThreadAttribute() が 0U、PROC_THREAD_ATTRIBUTE_HANDLE_LIST を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, CreateProcessW(_, _, _, _, _, _, _, _, _, _, _, _, _))
        .WillOnce(
            [process_handle, thread_handle](const char *, const int, const char *, LPCWSTR, LPWSTR,
                                            LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR,
                                            LPSTARTUPINFOW, LPPROCESS_INFORMATION info)
            {
                info->hProcess = process_handle;
                info->hThread = thread_handle;
                info->dwProcessId = 1000U;
                info->dwThreadId = 1001U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - CreateProcessW() が 1 回呼び出されること。
    // [Pre-Assert手順] - プロセス ハンドルとスレッド ハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, DeleteProcThreadAttributeList(_, _, _, _)).Times(1);
    // [Pre-Assert確認_正常系] - DeleteProcThreadAttributeList() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, _)).WillRepeatedly(Return(TRUE));
    // [Pre-Assert確認_正常系] - CloseHandle() が任意の回数呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    int result =
        cplat_process_start(&options, &process); // [手順] - 無効な標準ハンドルを継承する設定でプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_process_start() の戻り値が CPLAT_OK であること。
    EXPECT_NE(nullptr, process); // [確認_正常系] - process が nullptr 以外であること。

    // Cleanup
    cplat_process_dispose(process);
}

// Windows の process_start が指定ハンドルを複製して起動することの確認
TEST(processTest, StartCreatesProcessWithNativeStdioHandle)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "C:\\Windows\\System32\\cmd.exe";
    char *argv[] = {arg0, nullptr};
    HANDLE current_process = reinterpret_cast<HANDLE>(0x50);
    HANDLE native_handle = reinterpret_cast<HANDLE>(0x30);
    HANDLE duplicated = reinterpret_cast<HANDLE>(0x31);
    HANDLE process_handle = reinterpret_cast<HANDLE>(0x80);
    HANDLE thread_handle = reinterpret_cast<HANDLE>(0x81);

    options.argv = argv;                                         // [状態] - cmd.exe を起動する argv とする。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_NATIVE_HANDLE; // [状態] - stdin を指定ハンドルとする。
    options.stdin_spec.native_handle = reinterpret_cast<intptr_t>(native_handle);
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_NATIVE_HANDLE; // [状態] - stdout を指定ハンドルとする。
    options.stdout_spec.native_handle = reinterpret_cast<intptr_t>(native_handle);
    options.stderr_spec.mode = CPLAT_PROCESS_STDIO_NATIVE_HANDLE; // [状態] - stderr を指定ハンドルとする。
    options.stderr_spec.native_handle = reinterpret_cast<intptr_t>(native_handle);

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetCurrentProcess(_, _, _)).Times(3).WillRepeatedly(Return(current_process));
    // [Pre-Assert確認_正常系] - GetCurrentProcess() が 3 回呼び出されること。
    // [Pre-Assert手順] - 現在プロセスのダミー ハンドルを返却する。
    EXPECT_CALL(mock_windows, DuplicateHandle(_, _, _, current_process, native_handle, current_process, _, 0U, TRUE,
                                              DUPLICATE_SAME_ACCESS))
        .Times(3)
        .WillRepeatedly(
            [duplicated](const char *, const int, const char *, HANDLE, HANDLE, HANDLE, LPHANDLE target, DWORD, BOOL,
                         DWORD)
            {
                *target = duplicated;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - DuplicateHandle() が current_process、native_handle、current_process、0U、TRUE、DUPLICATE_SAME_ACCESS を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - 複製後のハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, InitializeProcThreadAttributeList(_, _, _, _, 1U, 0U, _))
        .WillOnce(
            [](const char *, const int, const char *, LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T size)
            {
                *size = 64U;
                return FALSE;
            })
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - InitializeProcThreadAttributeList() が 1U、0U を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は必要サイズ 64 を設定し、2 回目は TRUE を返却する。
    EXPECT_CALL(mock_windows, UpdateProcThreadAttribute(_, _, _, _, 0U, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, _, _, _, _))
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - UpdateProcThreadAttribute() が 0U、PROC_THREAD_ATTRIBUTE_HANDLE_LIST を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。
    EXPECT_CALL(mock_windows, CreateProcessW(_, _, _, _, _, _, _, _, _, _, _, _, _))
        .WillOnce(
            [process_handle, thread_handle](const char *, const int, const char *, LPCWSTR, LPWSTR,
                                            LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR,
                                            LPSTARTUPINFOW, LPPROCESS_INFORMATION info)
            {
                info->hProcess = process_handle;
                info->hThread = thread_handle;
                info->dwProcessId = 1000U;
                info->dwThreadId = 1001U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - CreateProcessW() が 1 回呼び出されること。
    // [Pre-Assert手順] - プロセス ハンドルとスレッド ハンドルを設定して TRUE を返却する。
    EXPECT_CALL(mock_windows, DeleteProcThreadAttributeList(_, _, _, _)).Times(1);
    // [Pre-Assert確認_正常系] - DeleteProcThreadAttributeList() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, _)).WillRepeatedly(Return(TRUE));
    // [Pre-Assert確認_正常系] - CloseHandle() が任意の回数呼び出されること。
    // [Pre-Assert手順] - TRUE を返却する。

    // Act
    int result = cplat_process_start(&options, &process); // [手順] - native_handle 指定でプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_process_start() の戻り値が CPLAT_OK であること。
    EXPECT_NE(nullptr, process); // [確認_正常系] - process が nullptr 以外であること。

    // Cleanup
    cplat_process_dispose(process);
}
#endif /* PLATFORM_ */

// start が不正引数を検出することの確認
TEST(processTest, RejectsInvalidArguments)
{
    // Arrange
    cplat_process *process = nullptr; // [状態] - プロセス ハンドルの受け取り先を NULL で初期化する。

    // Pre-Assert

    // Act
    int result =
        cplat_process_start(nullptr, &process); // [手順] - options に NULL を渡して cplat_process_start() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_process_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(nullptr,
              process); // [確認_異常系] - process が nullptr であること。(ハンドルが NULL のままであることの確認)
}

// 実行ファイルのパスを取得できることの確認
TEST(processTest, GetsExecutablePath)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {};
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    const char kResolvedPath[] = "/opt/cplat/processTest";
#elif defined(PLATFORM_WINDOWS)
    NiceMock<Mock_windows> mock_windows;
#endif /* PLATFORM_ */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd, readlink(_, _, _, StrEq("/proc/self/exe"), _, _))
        .WillOnce(
            [](const char *, int, const char *, const char *, char *buf, size_t size) -> ssize_t
            {
                const char resolved[] = "/opt/cplat/processTest";
                size_t len = sizeof(resolved) - 1U;
                (void)size;
                memcpy(buf, resolved, len);
                return static_cast<ssize_t>(len);
            }); // [Pre-Assert確認_正常系] - readlink() が "/proc/self/exe" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - 解決済みパスを書き込み、その長さを返却する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, static_cast<HMODULE>(nullptr), _, _))
        .WillOnce(
            [](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD size) -> DWORD
            {
                const wchar_t resolved[] = L"C:\\opt\\cplat\\processTest.exe";
                size_t len = wcslen(resolved);
                (void)size;
                memcpy(filename, resolved, (len + 1U) * sizeof(wchar_t));
                return static_cast<DWORD>(len);
            }); // [Pre-Assert確認_正常系] - GetModuleFileNameW() が nullptr を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - モジュール パスを書き込み、その長さを返却する。
#endif /* PLATFORM_ */

    // Act
    int result = cplat_process_get_executable_path(
        path, sizeof(path)); // [手順] - 十分な容量のバッファーを渡して実行ファイルのパスを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_process_get_executable_path() の戻り値が CPLAT_OK であること。
#if defined(PLATFORM_LINUX)
    EXPECT_STREQ(kResolvedPath, path); // [確認_正常系] - path が "/opt/cplat/processTest" であること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_NE(
        '\0',
        path[0]); // [確認_正常系] - path[0] が '\0' 以外であること。(取得した実行ファイルのパスが空文字列でないことの確認)
    EXPECT_NE(
        nullptr,
        strstr(
            path,
            "processTest")); // [確認_正常系] - "processTest" を探した strstr() の戻り値が nullptr 以外であること。(パスに "processTest" が含まれることの確認)
#endif /* PLATFORM_ */
}

// 実行ファイルのパス取得が不正な出力引数を拒否することの確認
TEST(processTest, ExecutablePathRejectsInvalidOutputArguments)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert

    // Act
    int null_result = cplat_process_get_executable_path(
        nullptr, sizeof(path)); // [手順] - 出力先に NULL を渡して実行ファイルのパスを取得する。
    int zero_size_result =
        cplat_process_get_executable_path(path, 0); // [手順] - 出力先サイズに 0 を渡して実行ファイルのパスを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_result); // [確認_異常系] - 出力先が NULL の cplat_process_get_executable_path() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_result); // [確認_異常系] - 出力先サイズが 0 の cplat_process_get_executable_path() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 実行ファイルのパス取得がバッファー不足を報告することの確認
TEST(processTest, ExecutablePathReportsSmallBuffer)
{
    // Arrange
    char path[1] = {'x'};

    // Pre-Assert

    // Act
    int result = cplat_process_get_executable_path(
        path, sizeof(path)); // [手順] - 1 バイトの出力先へ実行ファイルのパスを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        result); // [確認_異常系] - cplat_process_get_executable_path() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        '\0',
        path[0]); // [確認_異常系] - path[0] が '\0' であること。(バッファー不足時に出力先が空文字列であることの確認)
}

#if defined(PLATFORM_LINUX)
// mock 化した getpid() の戻り値が cplat_process_get_pid へそのまま伝播することの確認
TEST(processTest, GetsPidPropagatesMockedGetpid)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, getpid(_, _, _))
        .WillOnce(Return(static_cast<pid_t>(4321))); // [Pre-Assert確認_正常系] - getpid() が 1 回呼び出されること。
                                                     // [Pre-Assert手順] - pid 4321 を返却する。

    // Act
    uint32_t result = cplat_process_get_pid(); // [手順] - cplat_process_get_pid() を呼び出す。

    // Assert
    EXPECT_EQ(
        4321U,
        result); // [確認_正常系] - cplat_process_get_pid() の戻り値が 4321U であること。(mock 化した getpid() の戻り値がそのまま返ることの確認)
}
#elif defined(PLATFORM_WINDOWS)
// mock 化した GetCurrentProcessId() の戻り値が cplat_process_get_pid へそのまま伝播することの確認
TEST(processTest, GetsPidPropagatesMockedGetCurrentProcessId)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetCurrentProcessId(_, _, _))
        .WillOnce(Return(
            static_cast<DWORD>(4321))); // [Pre-Assert確認_正常系] - GetCurrentProcessId() が 1 回呼び出されること。
                                        // [Pre-Assert手順] - pid 4321 を返却する。

    // Act
    uint32_t result = cplat_process_get_pid(); // [手順] - cplat_process_get_pid() を呼び出す。

    // Assert
    EXPECT_EQ(
        4321U,
        result); // [確認_正常系] - cplat_process_get_pid() の戻り値が 4321U であること。(mock 化した GetCurrentProcessId() の戻り値がそのまま返ることの確認)
}
#endif /* PLATFORM_ */

// 同じスレッドから繰り返し取得した TID が一致することの確認
TEST(processTest, GetsTidReturnsSameValueInSameThread)
{
    // Arrange

    // Pre-Assert

    // Act
    uint32_t first_tid = cplat_process_get_tid();  // [手順] - cplat_process_get_tid() を呼び出す。
    uint32_t second_tid = cplat_process_get_tid(); // [手順] - 同じスレッドから cplat_process_get_tid() を再度呼び出す。

    // Assert
    EXPECT_NE(
        0U,
        first_tid); // [確認_正常系] - 1 回目の cplat_process_get_tid() の戻り値が 0U 以外であること。(取得した TID が 0 でないことの確認)
    EXPECT_EQ(
        first_tid,
        second_tid); // [確認_正常系] - 2 回目の cplat_process_get_tid() の戻り値が 1 回目の cplat_process_get_tid() の戻り値と一致すること。(同じスレッドからの 2 回の取得結果が一致することの確認)
}

// 別のスレッドから取得した TID が、呼び出し元のスレッドの TID と異なることの確認
TEST(processTest, GetsTidDiffersBetweenThreads)
{
    // Arrange
    uint32_t worker_tid = 0U; // [状態] - 別スレッドで取得する TID の格納先を 0 にする。

    // Pre-Assert

    // Act
    std::thread worker(get_tid_in_thread,
                       &worker_tid); // [手順] - 別のスレッドから cplat_process_get_tid() を呼び出す。
    worker.join();
    uint32_t current_tid =
        cplat_process_get_tid(); // [手順] - 呼び出し元のスレッドから cplat_process_get_tid() を呼び出す。

    // Assert
    EXPECT_NE(
        0U,
        worker_tid); // [確認_正常系] - worker_tid が 0U 以外であること。(別スレッドで取得した TID が 0 でないことの確認)
    EXPECT_NE(
        current_tid,
        worker_tid); // [確認_正常系] - worker_tid が current_tid 以外であること。(別スレッドの TID が呼び出し元のスレッドの TID と異なることの確認)
}

#if defined(PLATFORM_LINUX)
// 最初のスレッドの TID が PID と一致することの確認
TEST(processTest, GetsTidMatchesPidOnInitialThread)
{
    // Arrange

    // Pre-Assert

    // Act
    uint32_t tid = cplat_process_get_tid(); // [手順] - 最初のスレッドから cplat_process_get_tid() を呼び出す。
    uint32_t pid = cplat_process_get_pid(); // [手順] - cplat_process_get_pid() を呼び出す。

    // Assert
    EXPECT_EQ(
        pid,
        tid); // [確認_正常系] - cplat_process_get_tid() の戻り値が cplat_process_get_pid() の戻り値と一致すること。(Linux の最初のスレッドで TID が PID と一致することの確認)
}
#endif /* PLATFORM_ */

#if defined(PLATFORM_LINUX)
// 実行ファイルのパス取得が readlink の OS エラーを共通結果へ変換することの確認
TEST(processTest, ExecutablePathReportsReadlinkFailure)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    char path[PLATFORM_PATH_MAX] = {'x'};
    errno = EACCES;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, readlink(_, _, _, StrEq("/proc/self/exe"), _, _))
        .WillOnce(Return(static_cast<ssize_t>(
            -1))); // [Pre-Assert確認_異常系] - readlink() が "/proc/self/exe" と等しい文字列を指定して 1 回呼び出されること。
                   // [Pre-Assert手順] - readlink から -1 を返却する。

    // Act
    int result = cplat_process_get_executable_path(
        path, sizeof(path)); // [手順] - readlink が EACCES で失敗する状態で実行ファイルのパスを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        result); // [確認_異常系] - cplat_process_get_executable_path() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(
        '\0',
        path[0]); // [確認_異常系] - path[0] が '\0' であること。(readlink 失敗時に出力先が空文字列であることの確認)
}

// readlink が出力バッファーを超える長さを返した場合に不足を報告することの確認
TEST(processTest, ExecutablePathReportsReadlinkLengthOverflow)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    char path[8] = {'x'};

    // Pre-Assert
    EXPECT_CALL(mock_unistd, readlink(_, _, _, StrEq("/proc/self/exe"), _, _))
        .WillOnce(Return(static_cast<ssize_t>(
            8))); // [Pre-Assert確認_異常系] - readlink() が "/proc/self/exe" と等しい文字列を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - 出力先容量と同じ 8 を返却する。

    // Act
    int result = cplat_process_get_executable_path(
        path, sizeof(path)); // [手順] - readlink が出力先容量以上の長さを返す状態でパスを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        result); // [確認_異常系] - cplat_process_get_executable_path() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ('\0',
              path[0]); // [確認_異常系] - path[0] が '\0' であること。(長さ超過時に出力先が空文字列であることの確認)
}

// argv の NULL、空文字列を process_start が拒否することの確認
TEST(processTest, RejectsInvalidArgumentVectors)
{
    // Arrange
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char empty_arg0[] = "";
    char *null_first_argv[] = {nullptr};
    char *empty_argv[] = {empty_arg0, nullptr};

    // Pre-Assert

    // Act
    options.argv = nullptr;
    int null_argv_result =
        cplat_process_start(&options, &process); // [手順] - argv 自体が NULL の options でプロセスを開始する。
    options.argv = null_first_argv;
    int null_first_result =
        cplat_process_start(&options, &process); // [手順] - argv[0] が NULL の options でプロセスを開始する。
    options.argv = empty_argv;
    int empty_first_result =
        cplat_process_start(&options, &process); // [手順] - argv[0] が空文字列の options でプロセスを開始する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_argv_result); // [確認_異常系] - argv が NULL の cplat_process_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_first_result); // [確認_異常系] - argv[0] が NULL の cplat_process_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        empty_first_result); // [確認_異常系] - argv[0] が空文字列の cplat_process_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        process); // [確認_異常系] - process が nullptr であること。(不正な argv で process が NULL のままであることの確認)
}

// 環境変数上書きの形式が不正な場合に process_start が拒否することの確認
TEST(processTest, RejectsInvalidEnvironmentOverride)
{
    // Arrange
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char invalid_override[] = "INVALID_ENVIRONMENT_ENTRY";
    char *overrides[] = {invalid_override, nullptr};
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    options.argv = argv;
    options.env_overrides = overrides; // [状態] - '=' を含まない不正な環境変数上書きを指定する。

    // Pre-Assert

    // Act
    int result = cplat_process_start(&options, &process); // [手順] - 不正な環境変数上書きでプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_process_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        process); // [確認_異常系] - process が nullptr であること。(不正な環境変数上書きで process が NULL のままであることの確認)
}

// プロセス ハンドル確保に失敗した場合に process_start が失敗することの確認
TEST(processTest, StartReportsProcessAllocationFailure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    options.argv = argv;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc() が 2 回呼び出されること。
                                    // [Pre-Assert手順] - 1 回目は本物へ委譲し、2 回目は NULL を返却する。

    // Act
    int result = cplat_process_start(&options, &process); // [手順] - process ハンドル確保失敗を注入して開始する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_process_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        nullptr,
        process); // [確認_異常系] - process が nullptr であること。(確保失敗時に process が NULL のままであることの確認)
}

// fork が失敗した場合に process_start が失敗することの確認
TEST(processTest, StartReportsForkFailure)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    options.argv = argv;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, fork(_, _, _))
        .WillOnce(Return(static_cast<pid_t>(-1))); // [Pre-Assert確認_異常系] - fork() が 1 回呼び出されること。
                                                   // [Pre-Assert手順] - fork から -1 を返却する。

    // Act
    int result = cplat_process_start(&options, &process); // [手順] - fork 失敗を注入してプロセスを開始する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result);           // [確認_異常系] - cplat_process_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr, process); // [確認_異常系] - fork 失敗時の process が nullptr であること。
}

// 待機が終了コードとシグナル終了を分類し、割り込みを再試行することの確認
TEST(processTest, WaitMapsExitStatesAndRetriesEintr)
{
    // Arrange
    NiceMock<Mock_sys_wait> mock_sys_wait;
    cplat_process *normal_process =
        cplat_internal_process_adopt_native(123); // [状態] - pid 123 の process を用意する。
    cplat_process *signaled_process =
        cplat_internal_process_adopt_native(124); // [状態] - pid 124 の process を用意する。
    int normal_status = 7 << 8;
    int signaled_status = SIGTERM;
    int normal_exit_code = 0;
    int signaled_exit_code = 0;
    ASSERT_NE(
        nullptr,
        normal_process); // [状態確認] - pid 123 の cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。
    ASSERT_NE(
        nullptr,
        signaled_process); // [状態確認] - pid 124 の cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。
    errno = EINTR;               // [状態] - 1 回目の waitpid が EINTR を返す状態とする。

    // Pre-Assert
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 123, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(normal_status), Return(static_cast<pid_t>(-1))))
        .WillOnce(DoAll(SetArgPointee<4>(normal_status),
                        Return(static_cast<pid_t>(
                            123)))); // [Pre-Assert確認_正常系] - waitpid() が 123 を指定して 2 回呼び出されること。
                                     // [Pre-Assert手順] - 1 回目は -1、2 回目は終了ステータス 7 を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 124, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(signaled_status),
                        Return(static_cast<pid_t>(
                            124)))); // [Pre-Assert確認_正常系] - waitpid() が 124 を指定して 1 回呼び出されること。
                                     // [Pre-Assert手順] - SIGTERM 終了ステータスを設定して 124 を返却する。

    // Act
    int normal_wait = cplat_process_wait(
        normal_process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - EINTR 後に正常終了する process を待機する。
    int normal_get = cplat_process_get_exit_code(
        normal_process, &normal_exit_code); // [手順] - 正常終了 process の終了コードを取得する。
    int signaled_wait =
        cplat_process_wait(signaled_process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - シグナル終了 process を待機する。
    int signaled_get = cplat_process_get_exit_code(
        signaled_process, &signaled_exit_code); // [手順] - シグナル終了 process の終了コードを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              normal_wait); // [確認_正常系] - 1 回目の EINTR 後の cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        normal_get); // [確認_正常系] - 正常終了 process の cplat_process_get_exit_code() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        7,
        normal_exit_code); // [確認_正常系] - normal_exit_code が 7 であること。(正常終了 process の終了コードが 7 であることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        signaled_wait); // [確認_正常系] - シグナル終了 process の cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        signaled_get); // [確認_正常系] - シグナル終了 process の cplat_process_get_exit_code() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        -1,
        signaled_exit_code); // [確認_正常系] - signaled_exit_code が -1 であること。(シグナル終了 process の終了コードが -1 であることの確認)

    // Cleanup
    cplat_process_dispose(normal_process);
    cplat_process_dispose(signaled_process);
}

// waitpid の OS エラーを未知エラーへ変換することの確認
TEST(processTest, WaitReportsWaitpidFailure)
{
    // Arrange
    NiceMock<Mock_sys_wait> mock_sys_wait;
    cplat_process *process = cplat_internal_process_adopt_native(125); // [状態] - pid 125 の process を用意する。
    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。
    errno = ECHILD;              // [状態] - waitpid が子プロセスなしで失敗する状態とする。

    // Pre-Assert
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 125, _, _))
        .WillOnce(Return(
            static_cast<pid_t>(-1))); // [Pre-Assert確認_異常系] - waitpid() が 125 を指定して 1 回呼び出されること。
                                      // [Pre-Assert手順] - waitpid から -1 を返却する。

    // Act
    int result = cplat_process_wait(process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - waitpid 失敗を注入して待機する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_process_wait() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_process_dispose(process);
}

// terminate の kill 失敗を未知エラーへ変換することの確認
TEST(processTest, TerminateReportsKillFailure)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process *process = cplat_internal_process_adopt_native(126); // [状態] - pid 126 の process を用意する。
    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。
    errno = ESRCH;               // [状態] - terminate 対象が存在せず kill が失敗する状態とする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, kill(_, _, _, 126, SIGTERM))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - kill() が 126、SIGTERM を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - kill から -1 を返却する。

    // Act
    int result = cplat_process_terminate(process); // [手順] - kill 失敗を注入して process を terminate する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_process_terminate() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_process_dispose(process);
}

// process API が NULL、負値、終了前の状態を拒否することの確認
TEST(processTest, RejectsInvalidWaitAndExitArguments)
{
    // Arrange
    cplat_process *process = cplat_internal_process_adopt_native(127); // [状態] - pid 127 の process を用意する。
    int exit_code = 0;
    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    int null_wait = cplat_process_wait(nullptr, CPLAT_PROCESS_NO_WAIT); // [手順] - NULL process で待機する。
    int negative_wait = cplat_process_wait(process, -1);             // [手順] - 負の timeout で待機する。
    int null_exit_process =
        cplat_process_get_exit_code(nullptr, &exit_code); // [手順] - NULL process から終了コードを取得する。
    int null_exit_output =
        cplat_process_get_exit_code(process, nullptr); // [手順] - NULL 出力先へ終了コードを取得する。
    int running_exit =
        cplat_process_get_exit_code(process, &exit_code); // [手順] - 待機前 process から終了コードを取得する。
    int null_terminate = cplat_process_terminate(nullptr); // [手順] - NULL process を terminate する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_wait); // [確認_異常系] - NULL process の cplat_process_wait() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        negative_wait); // [確認_異常系] - 負の timeout の cplat_process_wait() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_exit_process); // [確認_異常系] - NULL process の cplat_process_get_exit_code() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_exit_output); // [確認_異常系] - NULL 出力先の cplat_process_get_exit_code() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        running_exit); // [確認_異常系] - 実行中 process の cplat_process_get_exit_code() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_terminate); // [確認_異常系] - NULL process の cplat_process_terminate() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_process_dispose(process);
    cplat_process_dispose(nullptr);
}

    #if defined(PLATFORM_LINUX)

// Linux の環境変数補助関数がキー境界、上書き、容量不足を処理することの確認
TEST(processTest, environment_helpers_handle_keys_and_capacity)
{
    // Arrange
    char *old_entry = strdup("KEY=old");
    char *envp[3] = {old_entry, nullptr, nullptr};
    char replace_entry[] = "KEY=new";
    char add_entry[] = "OTHER=value";
    char overflow_entry[] = "THIRD=value";
    char override_one[] = "PATH=/custom/bin";
    char override_two[] = "CPLAT_PROCESS_TEST_HELPER=helper";
    char *overrides[] = {override_one, override_two, nullptr};
    char invalid_override[] = "INVALID_HELPER_ENTRY";
    char *invalid_overrides[] = {invalid_override, nullptr};

    // Pre-Assert

    // Act
    size_t key_len = test_process_env_key_len("KEY=value"); // [手順] - 環境変数エントリのキー長を取得する。
    size_t no_key_len = test_process_env_key_len("KEY");    // [手順] - 区切りを持たないエントリのキー長を取得する。
    int matching_key = test_process_env_key_matches("KEY=value", "KEY", 3U);    // [手順] - 一致するキーを判定する。
    int prefix_key = test_process_env_key_matches("KEY=value", "KE", 2U);       // [手順] - 接頭辞だけのキーを判定する。
    int different_key = test_process_env_key_matches("KEY=value", "OTHER", 5U); // [手順] - 異なるキーを判定する。
    int replace_result = test_process_set_env_entry(envp, 3U, replace_entry);   // [手順] - 既存キーを上書きする。
    int add_result = test_process_set_env_entry(envp, 3U, add_entry);           // [手順] - 新しいキーを追加する。
    int overflow_result = test_process_set_env_entry(envp, 3U, overflow_entry); // [手順] - 容量超過のキーを追加する。
    char **built_env = test_process_build_environment(overrides);             // [手順] - 現在の環境へ上書きを適用する。
    char **invalid_env = test_process_build_environment(invalid_overrides);   // [手順] - 不正な上書きを適用する。
    const char *updated_value = test_process_find_env_value(envp, "KEY");     // [手順] - 上書き後の値を検索する。
    const char *missing_value = test_process_find_env_value(envp, "MISSING"); // [手順] - 存在しない値を検索する。

    // Assert
    EXPECT_EQ(
        3U,
        key_len); // [確認_正常系] - "KEY=value" に対する test_process_env_key_len() の戻り値が 3U であること。(KEY の長さが 3 であることの確認)
    EXPECT_EQ(
        0U, no_key_len); // [確認_異常系] - no_key_len が 0U であること。(区切りなしエントリの長さが 0 であることの確認)
    EXPECT_EQ(
        1,
        matching_key); // [確認_正常系] - 一致するキーに対する test_process_env_key_matches() の戻り値が 1 であること。(一致するキーと判定されることの確認)
    EXPECT_EQ(0, prefix_key); // [確認_異常系] - prefix_key が 0 であること。(接頭辞だけのキーが不一致であることの確認)
    EXPECT_EQ(0, different_key); // [確認_異常系] - different_key が 0 であること。(異なるキーが不一致であることの確認)
    EXPECT_EQ(0,
              replace_result); // [確認_正常系] - replace_result が 0 であること。(既存キーの上書きが成功することの確認)
    EXPECT_EQ(0, add_result);  // [確認_正常系] - add_result が 0 であること。(新しいキーの追加が成功することの確認)
    EXPECT_EQ(
        -1, overflow_result); // [確認_異常系] - overflow_result が -1 であること。(容量超過の追加が失敗することの確認)
    EXPECT_STREQ(
        "new",
        updated_value); // [確認_正常系] - "KEY" を探した test_process_find_env_value() の戻り値が "new" であること。(上書き後の KEY が "new" であることの確認)
    EXPECT_EQ(
        static_cast<const char *>(nullptr),
        missing_value); // [確認_異常系] - "MISSING" を探した test_process_find_env_value() の戻り値が nullptr と一致すること。(未登録キーが nullptr であることの確認)
    ASSERT_NE(
        static_cast<char **>(nullptr),
        built_env); // [確認_正常系] - 上書きを適用した test_process_build_environment() の戻り値が nullptr 以外であること。(環境配列が生成されることの確認)
    EXPECT_STREQ(
        "/custom/bin",
        test_process_find_env_value(
            built_env,
            "PATH")); // [確認_正常系] - "PATH" を探した test_process_find_env_value() の戻り値が "/custom/bin" であること。(PATH が上書きされることの確認)
    EXPECT_STREQ(
        "helper",
        test_process_find_env_value(
            built_env,
            "CPLAT_PROCESS_TEST_HELPER")); // [確認_正常系] - "CPLAT_PROCESS_TEST_HELPER" を探した test_process_find_env_value() の戻り値が "helper" であること。(追加した環境変数が検索できることの確認)
    EXPECT_EQ(
        static_cast<char **>(nullptr),
        invalid_env); // [確認_異常系] - 不正な上書きを指定した test_process_build_environment() の戻り値が nullptr と一致すること。(不正な上書きで nullptr が返ることの確認)

    // Cleanup
    test_process_free_envp(built_env);
}

// Linux の子プロセス標準入出力設定が各モードと OS エラーを分類することの確認
TEST(processTest, child_stdio_helpers_handle_modes_and_errors)
{
    // Arrange
    NiceMock<Mock_fcntl> mock_fcntl;
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_stdio spec = {};
    cplat_process_options options = {};

    // Pre-Assert
    EXPECT_CALL(mock_fcntl, open(_, _, _, _, _, _))
        .WillOnce(Return(10))
        .WillOnce(Return(13))
        .WillOnce(Return(-1)); // [Pre-Assert確認_正常系] - open() が 3 回呼び出されること。
                               // [Pre-Assert手順] - 1 回目は 10、2 回目は 13、3 回目は -1 を返却する。
    EXPECT_CALL(mock_unistd, dup2(_, _, _, 10, STDIN_FILENO))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - dup2() が 10、STDIN_FILENO を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - STDIN 向け dup2 から 0 を返却する。
    EXPECT_CALL(mock_unistd, close(_, _, _, 10))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が 10 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - fd 10 の close から 0 を返却する。
    EXPECT_CALL(mock_unistd, dup2(_, _, _, 13, STDOUT_FILENO))
        .WillOnce(
            Return(-1)); // [Pre-Assert確認_異常系] - dup2() が 13、STDOUT_FILENO を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - NULL デバイス向け dup2 から -1 を返却する。
    EXPECT_CALL(mock_unistd, close(_, _, _, 13))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が 13 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - fd 13 の close から 0 を返却する。
    EXPECT_CALL(mock_unistd, dup2(_, _, _, 11, STDOUT_FILENO))
        .WillOnce(
            Return(-1)); // [Pre-Assert確認_異常系] - dup2() が 11、STDOUT_FILENO を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - ネイティブハンドル 11 の dup2 から -1 を返却する。
    EXPECT_CALL(mock_unistd, dup2(_, _, _, 12, STDERR_FILENO))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - dup2() が 12、STDERR_FILENO を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - STDERR 向け dup2 から 0 を返却する。

    // Act
    int inherit_result =
        test_process_setup_child_stdio_one(&spec, STDIN_FILENO, O_RDONLY); // [手順] - 継承モードを設定する。
    spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;
    int null_result =
        test_process_setup_child_stdio_one(&spec, STDIN_FILENO, O_RDONLY); // [手順] - NULL デバイスへ接続する。
    int null_dup_failure_result = test_process_setup_child_stdio_one(
        &spec, STDOUT_FILENO, O_WRONLY); // [手順] - NULL デバイスの dup2 失敗を処理する。
    int null_open_result = test_process_setup_child_stdio_one(
        &spec, STDOUT_FILENO, O_WRONLY); // [手順] - NULL デバイスの open 失敗を処理する。
    spec.mode = CPLAT_PROCESS_STDIO_NATIVE_HANDLE;
    spec.native_handle = -1;
    int invalid_handle_result = test_process_setup_child_stdio_one(
        &spec, STDOUT_FILENO, O_WRONLY); // [手順] - 負のネイティブハンドルを設定する。
    spec.native_handle = 11;
    int dup_failure_result =
        test_process_setup_child_stdio_one(&spec, STDOUT_FILENO, O_WRONLY); // [手順] - dup2 の失敗を処理する。
    spec.native_handle = 12;
    int native_result =
        test_process_setup_child_stdio_one(&spec, STDERR_FILENO, O_WRONLY); // [手順] - ネイティブハンドルを接続する。
    int invalid_mode_value = 99;
    spec.mode = static_cast<cplat_process_stdio_mode>(invalid_mode_value);
    int invalid_mode_result = test_process_setup_child_stdio_one(
        &spec, STDERR_FILENO, O_WRONLY); // [手順] - 不正な標準入出力モードを設定する。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;
    options.stderr_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;
    int all_inherit_result = test_process_setup_child_stdio(&options); // [手順] - 3 標準ストリームを継承する。

    // Assert
    EXPECT_EQ(0, inherit_result); // [確認_正常系] - inherit_result が 0 であること。(継承モードが成功することの確認)
    EXPECT_EQ(0, null_result); // [確認_正常系] - null_result が 0 であること。(NULL デバイス接続が成功することの確認)
    EXPECT_EQ(
        -1,
        null_dup_failure_result); // [確認_異常系] - null_dup_failure_result が -1 であること。(NULL デバイスの dup2 が失敗することの確認)
    EXPECT_EQ(
        -1,
        null_open_result); // [確認_異常系] - null_open_result が -1 であること。(NULL デバイスの open が失敗することの確認)
    EXPECT_EQ(
        -1,
        invalid_handle_result); // [確認_異常系] - invalid_handle_result が -1 であること。(負のハンドルが拒否されることの確認)
    EXPECT_EQ(-1,
              dup_failure_result); // [確認_異常系] - dup_failure_result が -1 であること。(dup2 が失敗することの確認)
    EXPECT_EQ(
        0,
        native_result); // [確認_正常系] - native_result が 0 であること。(ネイティブハンドル接続が成功することの確認)
    EXPECT_EQ(
        -1,
        invalid_mode_result); // [確認_異常系] - invalid_mode_result が -1 であること。(不正モードが拒否されることの確認)
    EXPECT_EQ(
        0,
        all_inherit_result); // [確認_正常系] - all_inherit_result が 0 であること。(全ストリーム継承が成功することの確認)
}

// Linux の PATH 探索が絶対パス、空要素、既定値、候補長を処理することの確認
TEST(processTest, exec_path_helper_searches_path_segments)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    char absolute_arg[] = "/bin/tool";
    char relative_arg[] = "tool";
    char long_arg[PLATFORM_PATH_MAX] = {};
    char path_value[] = "PATH=/one::/two";
    char *absolute_argv[] = {absolute_arg, nullptr};
    char *relative_argv[] = {relative_arg, nullptr};
    char *long_argv[] = {long_arg, nullptr};
    char *envp[] = {path_value, nullptr};
    memset(long_arg, 'x', sizeof(long_arg) - 1U);
    long_arg[sizeof(long_arg) - 1U] = '\0';

    // Pre-Assert
    EXPECT_CALL(mock_unistd, execve(_, _, _, _, _, _))
        .WillRepeatedly(Return(-1)); // [Pre-Assert確認_正常系] - execve() が任意の回数呼び出されること。
                                     // [Pre-Assert手順] - PATH 探索中の execve から -1 を返却する。

    // Act
    test_process_exec_with_path(absolute_argv, envp); // [手順] - 絶対パスを exec する。
    test_process_exec_with_path(relative_argv, envp); // [手順] - PATH の各要素から相対パスを探索する。
    char *empty_path_env[] = {nullptr};
    test_process_exec_with_path(relative_argv, empty_path_env); // [手順] - PATH 不在時の既定値から探索する。
    test_process_exec_with_path(long_argv, envp);               // [手順] - 長すぎる候補を exec しない。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(exec 失敗後も PATH 探索ヘルパーが戻ることの確認)
}

// Linux の単調時間取得が timespec をミリ秒へ変換することの確認
TEST(processTest, monotonic_time_converts_timespec_to_milliseconds)
{
    // Arrange
    NiceMock<Mock_time> mock_time;
    struct timespec value = {};
    value.tv_sec = 12;
    value.tv_nsec = 345000000L;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(DoAll(
            SetArgPointee<4>(value),
            Return(
                0))); // [Pre-Assert確認_正常系] - clock_gettime() が CLOCK_MONOTONIC を指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - 12 秒 345 ミリ秒の timespec を設定し、0 を返却する。

    // Act
    uint64_t result = test_process_monotonic_ms(); // [手順] - 単調時計をミリ秒へ変換する。

    // Assert
    EXPECT_EQ(
        12345U,
        result); // [確認_正常系] - test_process_monotonic_ms() の戻り値が 12345U であること。(12 秒 345 ミリ秒が 12345 ミリ秒であることの確認)
}

// Linux の有限待機が deadline 到達時に timeout を返すことの確認
TEST(processTest, wait_reports_timeout_at_finite_deadline)
{
    // Arrange
    NiceMock<Mock_sys_wait> mock_sys_wait;
    NiceMock<Mock_time> mock_time;
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process *process = cplat_internal_process_adopt_native(128); // [状態] - pid 128 の process を用意する。
    struct timespec first = {};
    struct timespec second = {};
    int clock_count = 0;
    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。
    first.tv_sec = 1;
    second.tv_sec = 2;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [&clock_count, first, second](const char *, const int, const char *, const clockid_t, struct timespec *arg)
            {
                *arg = (clock_count++ == 0) ? first : second;
                return 0;
            })); // [Pre-Assert確認_正常系] - clock_gettime() が CLOCK_MONOTONIC を指定して 2 回呼び出されること。
                 // [Pre-Assert手順] - 1 回目は 1 秒、2 回目は 2 秒の timespec を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 128, _, WNOHANG))
        .WillOnce(Return(static_cast<pid_t>(
            0))); // [Pre-Assert確認_正常系] - waitpid() が 128、WNOHANG を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - 未終了を示す 0 を返却する。

    // Act
    int result = cplat_process_wait(process, 500); // [手順] - 終了しないプロセスを有限時間待機する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              result); // [確認_正常系] - cplat_process_wait() の戻り値が CPLAT_ERR_TIMEOUT であること。

    // Cleanup
    cplat_process_dispose(process);
}

// Linux の有限待機が期限前にスリープしてから終了を検出することの確認
TEST(processTest, wait_sleeps_before_finite_deadline_and_detects_exit)
{
    // Arrange
    NiceMock<Mock_sys_wait> mock_sys_wait;
    NiceMock<Mock_time> mock_time;
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process *process = cplat_internal_process_adopt_native(131); // [状態] - pid 131 の process を用意する。
    struct timespec now = {};
    int status = 4 << 8;
    int exit_code = 0;
    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。
    now.tv_sec = 1;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(DoAll(
            SetArgPointee<4>(now),
            Return(
                0))); // [Pre-Assert確認_正常系] - clock_gettime() が CLOCK_MONOTONIC を指定して 2 回呼び出されること。
                      // [Pre-Assert手順] - 1 秒の timespec を設定し、0 を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 131, _, WNOHANG))
        .WillOnce(Return(static_cast<pid_t>(0)))
        .WillOnce(
            DoAll(SetArgPointee<4>(status),
                  Return(static_cast<pid_t>(
                      131)))); // [Pre-Assert確認_正常系] - waitpid() が 131、WNOHANG を指定して 2 回呼び出されること。
                               // [Pre-Assert手順] - 1 回目は未終了 0、2 回目は終了ステータス 4 を返却する。
    EXPECT_CALL(mock_unistd, usleep(_, _, _, 1000U))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - usleep() が 1000U を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - usleep から 0 を返却する。

    // Act
    int wait_result = cplat_process_wait(process, 500);                 // [手順] - 期限前のプロセスを有限時間待機する。
    int exit_result = cplat_process_get_exit_code(process, &exit_code); // [手順] - 終了コードを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, wait_result); // [確認_正常系] - cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, exit_result); // [確認_正常系] - cplat_process_get_exit_code() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(4, exit_code); // [確認_正常系] - exit_code が 4 であること。(子プロセスの終了コードが 4 であることの確認)

    // Cleanup
    cplat_process_dispose(process);
}

// Linux の環境変数補助関数が不正エントリとメモリ確保失敗を処理することの確認
TEST(processTest, environment_helpers_report_invalid_and_allocation_failures)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char *envp[] = {nullptr, nullptr};
    char invalid_entry[] = "INVALID";
    char valid_entry[] = "KEY=value";
    char *invalid_overrides[] = {invalid_entry, nullptr};
    int set_result;
    char **invalid_result;
    char **calloc_result;
    char **duplicate_result;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 2 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から NULL を返却する。
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr))
        .WillOnce(DoDefault()); // [Pre-Assert確認_異常系] - cplat_calloc() が 2 回呼び出されること。
                                // [Pre-Assert手順] - 1 回目は NULL を返却し、2 回目は本物へ委譲する。

    // Act
    set_result = test_process_set_env_entry(envp, 2U, valid_entry); // [手順] - 環境変数エントリの確保失敗を処理する。
    test_process_free_envp(nullptr);                                // [手順] - NULL の環境配列を解放する。
    invalid_result = test_process_build_environment(invalid_overrides); // [手順] - 不正な上書き形式を処理する。
    calloc_result = test_process_build_environment(nullptr);            // [手順] - 環境配列の確保失敗を処理する。
    duplicate_result = test_process_build_environment(nullptr);         // [手順] - 環境エントリの複製失敗を処理する。

    // Assert
    EXPECT_EQ(-1, set_result); // [確認_異常系] - set_result が -1 であること。(エントリ確保が失敗することの確認)
    EXPECT_EQ(
        static_cast<char **>(nullptr),
        invalid_result); // [確認_異常系] - 不正な上書きを指定した test_process_build_environment() の戻り値が nullptr と一致すること。(不正な上書きで nullptr が返ることの確認)
    EXPECT_EQ(
        static_cast<char **>(nullptr),
        calloc_result); // [確認_異常系] - 環境配列の確保に失敗した test_process_build_environment() の戻り値が nullptr と一致すること。(環境配列の確保失敗で nullptr が返ることの確認)
    EXPECT_EQ(
        static_cast<char **>(nullptr),
        duplicate_result); // [確認_異常系] - 環境エントリの複製に失敗した test_process_build_environment() の戻り値が nullptr と一致すること。(エントリ複製失敗で nullptr が返ることの確認)
}

// Linux の終了済みプロセスに対する待機と終了要求が冪等であることの確認
TEST(processTest, completed_process_wait_and_terminate_are_idempotent)
{
    // Arrange
    NiceMock<Mock_sys_wait> mock_sys_wait;
    cplat_process *process = cplat_internal_process_adopt_native(129); // [状態] - pid 129 の process を用意する。
    int status = 3 << 8;
    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 129, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(status),
                        Return(static_cast<pid_t>(
                            129)))); // [Pre-Assert確認_正常系] - waitpid() が 129 を指定して 1 回呼び出されること。
                                     // [Pre-Assert手順] - 終了ステータス 3 を設定して 129 を返却する。

    // Act
    int first_wait = cplat_process_wait(process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - プロセスの終了を待機する。
    int second_wait =
        cplat_process_wait(process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - 終了済みプロセスを再度待機する。
    int terminate_result = cplat_process_terminate(process);     // [手順] - 終了済みプロセスを terminate する。

    // Assert
    EXPECT_EQ(CPLAT_OK, first_wait); // [確認_正常系] - 1 回目の cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        second_wait); // [確認_正常系] - 終了済みプロセスに対する 2 回目の cplat_process_wait() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        terminate_result); // [確認_正常系] - 終了済みプロセスの cplat_process_terminate() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_process_dispose(process);
}

// Linux の adopt_native がプロセス構造体の確保失敗を返すことの確認
TEST(processTest, adopt_native_reports_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_calloc() から NULL を返却する。

    // Act
    cplat_process *process = cplat_internal_process_adopt_native(130); // [手順] - プロセス構造体の確保失敗を注入する。

    // Assert
    EXPECT_EQ(
        nullptr,
        process); // [確認_異常系] - cplat_internal_process_adopt_native() の戻り値が nullptr であること。(確保失敗時に NULL が返ることの確認)
}

// Linux の run_sync が出力引数と start 失敗を検出することの確認
TEST(processTest, run_sync_rejects_invalid_output_and_start_failure)
{
    // Arrange
    cplat_process_options options = {};
    cplat_process *process = nullptr;
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    int exit_code = 0;
    options.argv = argv;

    // Pre-Assert

    // Act
    int null_output = cplat_process_run_sync(&options, CPLAT_PROCESS_WAIT_FOREVER,
                                             nullptr); // [手順] - 終了コード出力先に NULL を渡す。
    options.argv = nullptr;
    int start_failure = cplat_process_run_sync(&options, CPLAT_PROCESS_WAIT_FOREVER,
                                               &exit_code); // [手順] - 不正な options で同期実行する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_output); // [確認_異常系] - 終了コード出力先が NULL の cplat_process_run_sync() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        start_failure); // [確認_異常系] - 不正な options の cplat_process_run_sync() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        process); // [確認_異常系] - process が nullptr であること。(使用していない process が NULL のままであることの確認)
}

// Linux の環境変数補助関数が NULL、空配列、上書き失敗を処理することの確認
TEST(processTest, environment_helpers_cover_empty_and_override_failure_paths)
{
    // Arrange
    extern char **environ;
    NiceMock<Mock_cplat> mock_cplat;
    char **saved_environ = environ;
    char *empty_environment[] = {nullptr};
    char valid_override[] = "KEY=value";
    char invalid_entry[] = "INVALID";
    char *overrides[] = {valid_override, nullptr};
    char *empty_envp[] = {nullptr};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から NULL を返却する。

    // Act
    char *null_copy = test_process_string_duplicate(nullptr); // [手順] - NULL 文字列を複製する。
    int invalid_set =
        test_process_set_env_entry(empty_envp, 1U, invalid_entry);     // [手順] - '=' がない環境変数を設定する。
    const char *null_find =
        test_process_find_env_value(nullptr, "PATH"); // [手順] - NULL 環境配列から PATH を検索する。
    const char *empty_find =
        test_process_find_env_value(empty_envp, "PATH"); // [手順] - 空の環境配列から PATH を検索する。
    environ = empty_environment;
    char **override_result =
        test_process_build_environment(overrides); // [手順] - 環境変数上書きの文字列確保失敗を発生させる。

    // Assert
    EXPECT_EQ(
        nullptr,
        null_copy); // [確認_異常系] - NULL 文字列に対する test_process_string_duplicate() の戻り値が nullptr であること。
    EXPECT_EQ(-1,
              invalid_set); // [確認_異常系] - invalid_set が -1 であること。(不正形式の環境変数を拒否することの確認)
    EXPECT_EQ(
        nullptr,
        null_find); // [確認_正常系] - NULL 環境配列で "PATH" を探した test_process_find_env_value() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        empty_find); // [確認_正常系] - 空の環境配列で "PATH" を探した test_process_find_env_value() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        override_result); // [確認_異常系] - 上書きの文字列確保に失敗した test_process_build_environment() の戻り値が nullptr であること。

    // Cleanup
    environ = saved_environ;
}

// Linux の標準入出力設定が各ストリームの失敗位置を通知することの確認
TEST(processTest, child_stdio_setup_reports_each_stream_failure)
{
    // Arrange
    cplat_process_options options = {};
    set_invalid_stdio_mode(&options.stdin_spec);

    // Pre-Assert

    // Act
    int stdin_result = test_process_setup_child_stdio(&options); // [手順] - stdin 設定を不正モードで実行する。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;
    set_invalid_stdio_mode(&options.stdout_spec);
    int stdout_result = test_process_setup_child_stdio(&options); // [手順] - stdout 設定を不正モードで実行する。
    options.stdout_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;
    set_invalid_stdio_mode(&options.stderr_spec);
    int stderr_result = test_process_setup_child_stdio(&options); // [手順] - stderr 設定を不正モードで実行する。

    // Assert
    EXPECT_EQ(-1, stdin_result);  // [確認_異常系] - stdin_result が -1 であること。(stdin の設定が失敗することの確認)
    EXPECT_EQ(-1, stdout_result); // [確認_異常系] - stdout_result が -1 であること。(stdout の設定が失敗することの確認)
    EXPECT_EQ(-1, stderr_result); // [確認_異常系] - stderr_result が -1 であること。(stderr の設定が失敗することの確認)
}

// Linux の子プロセス準備が作業ディレクトリ、stdio、exec の失敗を 127 へ変換することの確認
TEST(processTest, child_runner_classifies_each_preparation_failure)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_options options = {};
    char arg0[] = "missing-tool";
    char path_entry[] = "PATH=";
    char current_directory[PLATFORM_PATH_MAX] = {};
    char *argv[] = {arg0, nullptr};
    char *envp[] = {path_entry, nullptr};
    options.argv = argv;
    ASSERT_NE(nullptr, getcwd(current_directory,
                              sizeof(current_directory))); // [状態] - 現在の作業ディレクトリを取得する。
                                                           // [状態確認] - getcwd() の戻り値が nullptr 以外であること。
    options.working_directory = "/cplat/process/directory/does/not/exist";

    // Pre-Assert
    EXPECT_CALL(mock_unistd, execve(_, _, _, _, _, _))
        .WillRepeatedly(Return(-1)); // [Pre-Assert確認_異常系] - execve() が任意の回数呼び出されること。
                                     // [Pre-Assert手順] - 子プロセス準備中の execve から -1 を返却する。

    // Act
    int chdir_result =
        test_process_run_child(&options, envp); // [手順] - 存在しない作業ディレクトリで子プロセス準備を実行する。
    options.working_directory = nullptr;
    set_invalid_stdio_mode(&options.stdin_spec);
    int stdio_result = test_process_run_child(&options, envp); // [手順] - 不正な stdin 設定で子プロセス準備を実行する。
    options.stdin_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;
    int exec_result = test_process_run_child(&options, envp); // [手順] - 空の PATH で実在しないコマンドを実行する。
    options.working_directory = current_directory;
    int chdir_success_result =
        test_process_run_child(&options, envp); // [手順] - 存在する作業ディレクトリで exec 失敗まで実行する。

    // Assert
    EXPECT_EQ(
        127,
        chdir_result); // [確認_異常系] - 存在しない作業ディレクトリに対する test_process_run_child() の戻り値が 127 であること。(chdir 失敗を 127 へ変換することの確認)
    EXPECT_EQ(
        127,
        stdio_result); // [確認_異常系] - 不正な stdin 設定に対する test_process_run_child() の戻り値が 127 であること。(stdio 失敗を 127 へ変換することの確認)
    EXPECT_EQ(
        127,
        exec_result); // [確認_異常系] - 空の PATH に対する test_process_run_child() の戻り値が 127 であること。(exec 失敗を 127 へ変換することの確認)
    EXPECT_EQ(
        127,
        chdir_success_result); // [確認_異常系] - 存在する作業ディレクトリに対する test_process_run_child() の戻り値が 127 であること。(chdir 成功後の exec 失敗を 127 へ変換することの確認)
}

// Linux の process start が NULL の出力先を拒否することの確認
TEST(processTest, start_rejects_null_process_output)
{
    // Arrange
    cplat_process_options options = {};
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    options.argv = argv;

    // Pre-Assert

    // Act
    int result = cplat_process_start(&options, nullptr); // [手順] - process の出力先を NULL として起動する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result); // [確認_異常系] - process の出力先が NULL の cplat_process_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// Linux の無期限待機が未終了状態から終了状態へ遷移することの確認
TEST(processTest, wait_forever_retries_unexpected_nonblocking_result)
{
    // Arrange
    NiceMock<Mock_sys_wait> mock_sys_wait;
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process *process = cplat_internal_process_adopt_native(132); // [状態] - pid 132 の process を用意する。
    int status = 0;
    ASSERT_NE(nullptr,
              process); // [状態確認] - cplat_internal_process_adopt_native() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 132, _, 0))
        .WillOnce(Return(static_cast<pid_t>(0)))
        .WillOnce(DoAll(SetArgPointee<4>(status),
                        Return(static_cast<pid_t>(
                            132)))); // [Pre-Assert確認_正常系] - waitpid() が 132、0 を指定して 2 回呼び出されること。
                                     // [Pre-Assert手順] - 1 回目は未終了 0、2 回目は終了ステータス 0 を返却する。
    EXPECT_CALL(mock_unistd, usleep(_, _, _, 1000U))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - usleep() が 1000U を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 再試行前の usleep から 0 を返却する。

    // Act
    int result =
        cplat_process_wait(process, CPLAT_PROCESS_WAIT_FOREVER); // [手順] - 未終了応答後に無期限待機を継続する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - 再試行後の cplat_process_wait() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_process_dispose(process);
}

// Linux の同期実行が wait timeout を呼び出し元へ返すことの確認
TEST(processTest, run_sync_returns_wait_failure)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_sys_wait> mock_sys_wait;
    cplat_process_options options = {};
    char arg0[] = "/bin/true";
    char *argv[] = {arg0, nullptr};
    int exit_code = 0;
    options.argv = argv;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, fork(_, _, _))
        .WillOnce(Return(static_cast<pid_t>(133))); // [Pre-Assert確認_正常系] - fork() が 1 回呼び出されること。
                                                    // [Pre-Assert手順] - 子プロセス pid 133 を返却する。
    EXPECT_CALL(mock_sys_wait, waitpid(_, _, _, 133, _, WNOHANG))
        .WillOnce(Return(static_cast<pid_t>(
            0))); // [Pre-Assert確認_正常系] - waitpid() が 133、WNOHANG を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - 未終了を示す 0 を返却する。

    // Act
    int result = cplat_process_run_sync(&options, CPLAT_PROCESS_NO_WAIT,
                                        &exit_code); // [手順] - 未終了プロセスを即時 timeout で同期実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              result); // [確認_正常系] - cplat_process_run_sync() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(
        EXIT_FAILURE,
        exit_code); // [確認_正常系] - exit_code が EXIT_FAILURE であること。(wait timeout 時の終了コードが EXIT_FAILURE のままであることの確認)
}

    #endif /* PLATFORM_LINUX */
#endif     /* PLATFORM_LINUX */
