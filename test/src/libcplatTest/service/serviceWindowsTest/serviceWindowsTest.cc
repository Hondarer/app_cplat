#define _HAS_STD_BYTE 0
#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <testfw.h>
    #include <mock_cplat.h>
    #include <mock_windows.h>

    #include <cstdint>
    #include <cstdlib>
    #include <cstring>
    #include <vector>

    #include <cplat/service/service.h>
    #include <cplat/service/service_internal.h>
    #include "service_windows.inject.h"

using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::HasSubstr;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

namespace
{

/** ステータス ロックの保持状態と再読込コールバックの呼び出し回数。 */
bool g_status_lock_held = false;
int g_reload_callback_count = 0;

/** SetServiceStatus() に渡された状態、checkpoint、dwWaitHint、終了コードの記録。 */
struct ServiceStatusRecord
{
    DWORD state;
    DWORD checkpoint;
    DWORD wait_hint;
    DWORD win32_exit_code;
    DWORD service_specific_exit_code;
};

/** SetServiceStatus() の呼び出しを順に記録する。 */
std::vector<ServiceStatusRecord> g_status_records;

/** RegisterServiceCtrlHandlerExU() に登録された制御ハンドラー。 */
LPHANDLER_FUNCTION_EX g_ctrl_handler = nullptr;

extern "C"
{
    static int dummy_on_run(void *user_data)
    {
        (void)user_data;
        return 0;
    }

    static void windows_test_on_reload(void *user_data)
    {
        (void)user_data;
        g_reload_callback_count++;
    }

    static int on_run_with_stop_request(void *user_data)
    {
        (void)user_data;
        (void)g_ctrl_handler(SERVICE_CONTROL_STOP, 0, nullptr, nullptr);
        return 0;
    }

    static int on_start_fails(void *user_data)
    {
        (void)user_data;
        return 7;
    }

    static int on_run_fails(void *user_data)
    {
        (void)user_data;
        return 4;
    }

    static int on_stop_fails(void *user_data)
    {
        (void)user_data;
        return 5;
    }
}

cplat_service_definition make_def()
{
    cplat_service_definition def = {};
    def.name = "test-win-svc";
    def.display_name = "Test Windows Service";
    def.description = "Test Windows Description";
    def.on_run = dummy_on_run;
    return def;
}

void arrange_service_dispatch(NiceMock<Mock_cplat> &mock_cplat, NiceMock<Mock_windows> &mock_windows,
                              SERVICE_STATUS_HANDLE status_handle)
{
    g_status_records.clear();
    g_ctrl_handler = nullptr;
    ON_CALL(mock_cplat, cplat_local_lock_create(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_create));
    ON_CALL(mock_cplat, cplat_local_lock_lock(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_lock));
    ON_CALL(mock_cplat, cplat_local_lock_unlock(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_unlock));
    ON_CALL(mock_cplat, cplat_local_lock_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_dispose));
    ON_CALL(mock_cplat, cplat_tracer_write_with_source(_, _, _, _, _, _)).WillByDefault(Return(CPLAT_OK));
    ON_CALL(mock_cplat, cplat_tracer_writef_at(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
    ON_CALL(mock_cplat, RegisterServiceCtrlHandlerExU(_, _, _))
        .WillByDefault(Invoke(
            [status_handle](const char *, LPHANDLER_FUNCTION_EX handler, LPVOID) -> SERVICE_STATUS_HANDLE
            {
                g_ctrl_handler = handler;
                return status_handle;
            }));
    ON_CALL(mock_cplat, StartServiceCtrlDispatcherU(_))
        .WillByDefault(Invoke(
            [](const SERVICE_TABLE_ENTRYU *table) -> BOOL
            {
                table[0].lpServiceProc(0, nullptr);
                return TRUE;
            }));
    ON_CALL(mock_windows, SetServiceStatus(_, _, _, _, _))
        .WillByDefault(Invoke(
            [](const char *, const int, const char *, SERVICE_STATUS_HANDLE, LPSERVICE_STATUS status) -> BOOL
            {
                g_status_records.push_back({status->dwCurrentState, status->dwCheckPoint, status->dwWaitHint,
                                            status->dwWin32ExitCode, status->dwServiceSpecificExitCode});
                return TRUE;
            }));
}

} // namespace

// Windows で cplat_service_prepare_run が CPLAT_OK を返し SCM へ状態を報告しないことの確認
TEST(serviceWindowsTest, prepare_run_returns_ok)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetServiceStatus(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - SetServiceStatus() が呼び出されないこと。

    // Act
    int actual_ret = cplat_service_prepare_run(); // [手順] - cplat_service_prepare_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_prepare_run() の戻り値が CPLAT_OK であること。
}

// 非管理者時の install / uninstall が自動昇格せず CPLAT_ERR_PERMISSION_DENIED を返すことの確認
TEST(serviceWindowsTest, install_and_uninstall_require_admin_without_elevation)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillRepeatedly(DoAll(
            SetArgPointee<0>(0),
            Return(CPLAT_OK))); // [Pre-Assert確認_異常系] - cplat_elevated_process_is_elevated() が呼び出されること。
    // [Pre-Assert手順] - cplat_elevated_process_is_elevated() から非管理者 (0) を設定して CPLAT_OK を返す。

    // Act
    int actual_install =
        cplat_service_install(&def, nullptr);             // [手順] - 非管理者で cplat_service_install() を呼び出す。
    int actual_uninstall = cplat_service_uninstall(&def); // [手順] - 非管理者で cplat_service_uninstall() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_install); // [確認_異常系] - cplat_service_install() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_uninstall); // [確認_異常系] - cplat_service_uninstall() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
}

// display_name 欠如と restart_delay_sec オーバーフローの拒否確認
TEST(serviceWindowsTest, install_validates_display_name_and_delay_overflow)
{
    // Arrange
    cplat_service_definition def = make_def();
    def.display_name = nullptr;
    cplat_service_install_options overflow = {};
    overflow.restart_delay_sec = (UINT32_MAX / 1000U) + 1U;

    // Pre-Assert

    // Act
    int actual_no_display = cplat_service_install(
        &def, nullptr); // [手順] - display_name に nullptr を指定して cplat_service_install() を呼び出す。
    def.display_name = "Test Windows Service";
    int actual_overflow = cplat_service_install(
        &def, &overflow); // [手順] - 遅延がオーバーフローする設定で cplat_service_install() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_no_display); // [確認_異常系] - display_name 欠如時の cplat_service_install() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_overflow); // [確認_異常系] - 遅延オーバーフロー時の cplat_service_install() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// サービスの説明を設定できない場合に WARNING トレースを出して登録を続けることの確認
TEST(serviceWindowsTest, install_continues_when_description_setting_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    /*
     * SC_HANDLE はクライアント側の構造体を指すため、偽の値を CloseServiceHandle() などへ渡すと不正なアクセスになる。
     * そのため、実在するサービスを状態の照会だけの権限で開いたハンドルを、登録したサービスの代わりに使う。
     * 変更の権限がないため、ChangeServiceConfig2W() はサービスの設定を変えずに失敗する。
     * 2 つのハンドルは cplat_service_install() が閉じる。
     */
    SC_HANDLE scm_handle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    ASSERT_NE(nullptr, scm_handle); // [状態確認] - OpenSCManagerW() の戻り値が nullptr 以外であること。
    SC_HANDLE service_handle = OpenServiceW(scm_handle, L"EventLog", SERVICE_QUERY_STATUS);
    if (service_handle == nullptr)
    {
        (void)CloseServiceHandle(scm_handle);
    }
    ASSERT_NE(nullptr, service_handle); // [状態確認] - OpenServiceW() の戻り値が nullptr 以外であること。
    ON_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillByDefault(DoAll(SetArgPointee<0>(1), Return(CPLAT_OK))); // [状態] - 管理者権限ありの状態とする。
    ON_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillByDefault(Invoke(
            [](char *path, size_t size) -> int
            {
                (void)strncpy_s(path, size, "C:\\service-sample.exe", _TRUNCATE);
                return CPLAT_OK;
            })); // [状態] - 実行ファイル パスを返すように既定動作を設定する。
    ON_CALL(mock_cplat, OpenSCManagerU(_, _, _))
        .WillByDefault(
            Return(scm_handle)); // [状態] - OpenSCManagerU() から scm_handle を返すように既定動作を設定する。
    ON_CALL(mock_cplat, CreateServiceU(_, _, _, _, _, _, _, _, _, _, _, _, _))
        .WillByDefault(
            Return(service_handle)); // [状態] - CreateServiceU() から service_handle を返すように既定動作を設定する。
    ON_CALL(mock_cplat, cplat_tracer_write_with_source(_, _, _, _, _, _)).WillByDefault(Return(CPLAT_OK));

    // Pre-Assert
    EXPECT_CALL(mock_cplat, ChangeServiceConfig2U(service_handle, SERVICE_CONFIG_DESCRIPTION, _))
        .WillOnce(Return(
            FALSE)); // [Pre-Assert確認_異常系] - ChangeServiceConfig2U() が service_handle と SERVICE_CONFIG_DESCRIPTION を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - ChangeServiceConfig2U() から FALSE を返す。
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(_, _, _, _)).Times(AnyNumber()).WillRepeatedly(Return(CPLAT_OK));
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(nullptr, CPLAT_TRACE_LEVEL_WARNING, _,
                                                   HasSubstr("サービスの説明の設定に失敗しました")))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - 説明設定失敗の WARNING トレースが 1 回出力されること。
                                     // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(nullptr, CPLAT_TRACE_LEVEL_INFO, _, HasSubstr("を登録しました")))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - 登録完了の INFO トレースが 1 回出力されること。
                                     // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_service_install(&def, nullptr); // [手順] - cplat_service_install() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_service_install() の戻り値が CPLAT_OK であること。
}

// StartServiceCtrlDispatcherU 失敗時に CPLAT_ERR_UNKNOWN を返すことの確認
TEST(serviceWindowsTest, run_returns_unknown_when_dispatcher_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, StartServiceCtrlDispatcherU(_))
        .WillOnce(Invoke(
            [](const SERVICE_TABLE_ENTRYU *)
            {
                SetLastError(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT);
                return FALSE;
            })); // [Pre-Assert確認_異常系] - StartServiceCtrlDispatcherU() が 1 回呼び出されること。
    // [Pre-Assert手順] - StartServiceCtrlDispatcherU() から ERROR_FAILED_SERVICE_CONTROLLER_CONNECT を設定して FALSE を返す。

    // Act
    int actual_ret = cplat_service_run(&def, &exit_code); // [手順] - cplat_service_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_service_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// SetServiceStatus が失敗した場合に ERROR トレースを出して処理を続けることの確認
TEST(serviceWindowsTest, notify_continues_when_set_service_status_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x30));
    test_service_windows_set_status_handle(fake_status_handle); // [状態] - SCM に接続済みの状態とする。
    ON_CALL(mock_cplat, cplat_tracer_writef_at(_, _, _, _)).WillByDefault(Return(CPLAT_OK));

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetServiceStatus(_, _, _, fake_status_handle, _))
        .Times(2)
        .WillRepeatedly(Return(
            FALSE)); // [Pre-Assert確認_異常系] - SetServiceStatus() が fake_status_handle を指定して 2 回呼び出されること。
                     // [Pre-Assert手順] - SetServiceStatus() から FALSE を返す。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .Times(2)
        .WillRepeatedly(
            Return(ERROR_ACCESS_DENIED)); // [Pre-Assert確認_異常系] - GetLastError() が 2 回呼び出されること。
                                          // [Pre-Assert手順] - GetLastError() から ERROR_ACCESS_DENIED を返す。
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(nullptr, CPLAT_TRACE_LEVEL_ERROR, _,
                                                   HasSubstr("SetServiceStatus が失敗しました")))
        .Times(2)
        .WillRepeatedly(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - 通知失敗の ERROR トレースが 2 回出力されること。
                                           // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。

    // Act
    cplat_internal_service_os_notify_ready();    // [手順] - cplat_internal_service_os_notify_ready() を呼び出す。
    cplat_internal_service_os_notify_stopping(); // [手順] - cplat_internal_service_os_notify_stopping() を呼び出す。

    // Assert
    SUCCEED(); // [確認_異常系] - SUCCEED() に到達すること。(失敗後もクラッシュせず継続することの確認)

    // Cleanup
    test_service_windows_set_status_handle(nullptr);
}

// 停止中へ遷移した後に cplat_internal_service_os_notify_ready() を呼んでも SERVICE_RUNNING を報告しないことの確認
TEST(serviceWindowsTest, ready_after_stop_pending_is_not_reported)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x31));
    std::vector<DWORD> states;
    test_service_windows_set_status_lock(nullptr);              // [状態] - 通知ロックを未設定にする。
    test_service_windows_set_status_handle(fake_status_handle); // [状態] - SCM に接続済みの状態とする。
    ON_CALL(mock_cplat, cplat_tracer_write_with_source(_, _, _, _, _, _)).WillByDefault(Return(CPLAT_OK));

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetServiceStatus(_, _, _, fake_status_handle, _))
        .WillOnce(Invoke(
            [&states](const char *, const int, const char *, SERVICE_STATUS_HANDLE, LPSERVICE_STATUS status)
            {
                states.push_back(status->dwCurrentState);
                return TRUE;
            })); // [Pre-Assert確認_正常系] - SetServiceStatus() が fake_status_handle を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - SetServiceStatus() から状態を記録して TRUE を返す。

    // Act
    cplat_internal_service_os_notify_stopping(); // [手順] - cplat_internal_service_os_notify_stopping() を呼び出す。
    cplat_internal_service_os_notify_ready(); // [手順] - 停止中に cplat_internal_service_os_notify_ready() を呼び出す。

    // Assert
    ASSERT_EQ(1U, states.size()); // [確認_正常系] - states.size() の戻り値が 1U であること。
    EXPECT_EQ(
        static_cast<DWORD>(SERVICE_STOP_PENDING),
        states
            [0]); // [確認_正常系] - states[0] が SERVICE_STOP_PENDING であること。(SERVICE_RUNNING が報告されないことの確認)

    // Cleanup
    test_service_windows_set_status_handle(nullptr);
}

// 動作中の再読込完了で SERVICE_RUNNING を再報告することの確認
TEST(serviceWindowsTest, ready_after_reload_in_running_is_reported)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x32));
    std::vector<DWORD> states;
    cplat_service_definition def = make_def();
    def.on_reload = windows_test_on_reload;
    g_reload_callback_count = 0;
    test_service_windows_set_status_lock(nullptr);              // [状態] - 通知ロックを未設定にする。
    test_service_windows_set_status_handle(fake_status_handle); // [状態] - SCM に接続済みの状態とする。
    ON_CALL(mock_cplat, cplat_tracer_write_with_source(_, _, _, _, _, _)).WillByDefault(Return(CPLAT_OK));

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetServiceStatus(_, _, _, fake_status_handle, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [&states](const char *, const int, const char *, SERVICE_STATUS_HANDLE, LPSERVICE_STATUS status)
            {
                states.push_back(status->dwCurrentState);
                return TRUE;
            })); // [Pre-Assert確認_正常系] - SetServiceStatus() が fake_status_handle を指定して 2 回呼び出されること。
                 // [Pre-Assert手順] - SetServiceStatus() から状態を記録して TRUE を返す。

    // Act
    cplat_internal_service_os_notify_ready();     // [手順] - cplat_internal_service_os_notify_ready() を呼び出す。
    cplat_internal_service_dispatch_reload(&def); // [手順] - cplat_internal_service_dispatch_reload() を呼び出す。

    // Assert
    ASSERT_EQ(2U, states.size()); // [確認_正常系] - states.size() の戻り値が 2U であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_RUNNING),
              states[0]); // [確認_正常系] - states[0] が SERVICE_RUNNING であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_RUNNING),
              states[1]);                  // [確認_正常系] - states[1] が SERVICE_RUNNING であること。
    EXPECT_EQ(1, g_reload_callback_count); // [確認_正常系] - g_reload_callback_count が 1 であること。

    // Cleanup
    test_service_windows_set_status_handle(nullptr);
}

// サービス ステータス ロックの生成に失敗した場合は SCM dispatcher を開始しないことの確認
TEST(serviceWindowsTest, status_lock_creation_failure_fails_run_service)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    cplat_service_definition def = make_def();
    int exit_code = 0;
    ON_CALL(mock_cplat, cplat_tracer_write_with_source(_, _, _, _, _, _)).WillByDefault(Return(CPLAT_OK));

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_create(_))
        .WillOnce(Return(
            CPLAT_ERR_OUT_OF_MEMORY)); // [Pre-Assert確認_異常系] - cplat_local_lock_create() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_create() から CPLAT_ERR_OUT_OF_MEMORY を返す。
    EXPECT_CALL(mock_cplat, StartServiceCtrlDispatcherU(_))
        .Times(0); // [Pre-Assert確認_異常系] - StartServiceCtrlDispatcherU() が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_service_os_run(
        &def, &exit_code); // [手順] - ステータス ロック生成失敗の状態で cplat_internal_service_os_run() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}

// SetServiceStatus() の失敗トレースをステータス ロックの解放後に出すことの確認
TEST(serviceWindowsTest, status_failure_trace_is_written_after_unlock)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x33));
    cplat_local_lock *status_lock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              delegate_real_cplat_local_lock_create(&status_lock)); // [状態確認] - テスト用ロックを生成できること。
    g_status_lock_held = false;
    test_service_windows_set_status_lock(status_lock);          // [状態] - テスト用ステータス ロックを設定する。
    test_service_windows_set_status_handle(fake_status_handle); // [状態] - SCM に接続済みの状態とする。

    // Pre-Assert
    {
        InSequence sequence;
        EXPECT_CALL(mock_cplat, cplat_local_lock_lock(status_lock))
            .WillOnce(Invoke(
                [](cplat_local_lock *lock)
                {
                    delegate_real_cplat_local_lock_lock(lock);
                    g_status_lock_held = true;
                })); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_local_lock_lock() から製品 (prod) の処理へ委譲しロック保持を記録する。
        EXPECT_CALL(mock_windows, SetServiceStatus(_, _, _, fake_status_handle, _))
            .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetServiceStatus() が 1 回呼び出されること。
                                      // [Pre-Assert手順] - SetServiceStatus() から FALSE を返す。
        EXPECT_CALL(mock_windows, GetLastError(_, _, _))
            .WillOnce(
                Return(ERROR_ACCESS_DENIED)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - GetLastError() から ERROR_ACCESS_DENIED を返す。
        EXPECT_CALL(mock_cplat, cplat_local_lock_unlock(status_lock))
            .WillOnce(Invoke(
                [](cplat_local_lock *lock)
                {
                    delegate_real_cplat_local_lock_unlock(lock);
                    g_status_lock_held = false;
                })); // [Pre-Assert確認_異常系] - cplat_local_lock_unlock() が 1 回呼び出されること。
        // [Pre-Assert手順] - cplat_local_lock_unlock() から製品 (prod) の処理へ委譲しロック解放を記録する。
        EXPECT_CALL(mock_cplat,
                    cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_ERROR, _, HasSubstr("SetServiceStatus が失敗しました")))
            .WillOnce(Invoke(
                [](auto &&...)
                {
                    EXPECT_FALSE(g_status_lock_held);
                    return CPLAT_OK;
                })); // [Pre-Assert確認_異常系] - ロック解放後に SetServiceStatus 失敗の ERROR トレースが 1 回出力されること。
                     // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。
    }

    // Act
    cplat_internal_service_os_notify_stopping(); // [手順] - cplat_internal_service_os_notify_stopping() を呼び出す。

    // Assert
    EXPECT_FALSE(g_status_lock_held); // [確認_正常系] - g_status_lock_held が false であること。

    // Cleanup
    test_service_windows_set_status_lock(nullptr);
    test_service_windows_set_status_handle(nullptr);
    delegate_real_cplat_local_lock_dispose(status_lock);
}

// on_run() の中で停止要求を受けた場合に on_run() 復帰後の SERVICE_STOP_PENDING の checkpoint が 2 になることの確認
TEST(serviceWindowsTest, stop_path_increments_checkpoint_on_second_stop_pending)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x40));
    cplat_service_definition def = make_def();
    def.on_run = on_run_with_stop_request;
    int exit_code = -1;
    arrange_service_dispatch(mock_cplat, mock_windows,
                             fake_status_handle); // [状態] - ServiceMain を駆動し SetServiceStatus() の引数を記録する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_service_run(&def, &exit_code); // [手順] - cplat_service_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);        // [確認_正常系] - cplat_service_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code);                // [確認_正常系] - exit_code が 0 であること。
    ASSERT_EQ(5U, g_status_records.size()); // [確認_正常系] - g_status_records.size() の戻り値が 5U であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_START_PENDING),
              g_status_records[0].state);          // [確認_正常系] - 1 回目の状態が SERVICE_START_PENDING であること。
    EXPECT_EQ(1U, g_status_records[0].checkpoint); // [確認_正常系] - 1 回目の checkpoint が 1 であること。
    EXPECT_EQ(3000U, g_status_records[0].wait_hint); // [確認_正常系] - 1 回目の dwWaitHint が 3000 であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_RUNNING),
              g_status_records[1].state);          // [確認_正常系] - 2 回目の状態が SERVICE_RUNNING であること。
    EXPECT_EQ(0U, g_status_records[1].checkpoint); // [確認_正常系] - 2 回目の checkpoint が 0 であること。
    EXPECT_EQ(0U, g_status_records[1].wait_hint);  // [確認_正常系] - 2 回目の dwWaitHint が 0 であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_STOP_PENDING),
              g_status_records[2].state);            // [確認_正常系] - 3 回目の状態が SERVICE_STOP_PENDING であること。
    EXPECT_EQ(1U, g_status_records[2].checkpoint);   // [確認_正常系] - 3 回目の checkpoint が 1 であること。
    EXPECT_EQ(5000U, g_status_records[2].wait_hint); // [確認_正常系] - 3 回目の dwWaitHint が 5000 であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_STOP_PENDING),
              g_status_records[3].state); // [確認_正常系] - 4 回目の状態が SERVICE_STOP_PENDING であること。
    EXPECT_EQ(
        2U,
        g_status_records[3]
            .checkpoint); // [確認_正常系] - 4 回目の checkpoint が 2 であること。(2 回目の STOP_PENDING で増加することの確認)
    EXPECT_EQ(3000U, g_status_records[3].wait_hint); // [確認_正常系] - 4 回目の dwWaitHint が 3000 であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_STOPPED),
              g_status_records[4].state);          // [確認_正常系] - 5 回目の状態が SERVICE_STOPPED であること。
    EXPECT_EQ(0U, g_status_records[4].checkpoint); // [確認_正常系] - 5 回目の checkpoint が 0 であること。
    EXPECT_EQ(0U, g_status_records[4].wait_hint);  // [確認_正常系] - 5 回目の dwWaitHint が 0 であること。
}

// on_run() が停止要求なしに戻った場合に最初の SERVICE_STOP_PENDING の checkpoint が 2 になることの確認
TEST(serviceWindowsTest, stop_pending_uses_checkpoint_2_when_run_returns_without_stop_request)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x50));
    cplat_service_definition def = make_def();
    int exit_code = -1;
    arrange_service_dispatch(mock_cplat, mock_windows,
                             fake_status_handle); // [状態] - ServiceMain を駆動し SetServiceStatus() の引数を記録する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_service_run(&def, &exit_code); // [手順] - cplat_service_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);        // [確認_正常系] - cplat_service_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code);                // [確認_正常系] - exit_code が 0 であること。
    ASSERT_EQ(4U, g_status_records.size()); // [確認_正常系] - g_status_records.size() の戻り値が 4U であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_STOP_PENDING),
              g_status_records[2].state);            // [確認_正常系] - 3 回目の状態が SERVICE_STOP_PENDING であること。
    EXPECT_EQ(2U, g_status_records[2].checkpoint);   // [確認_正常系] - 3 回目の checkpoint が 2 であること。
    EXPECT_EQ(3000U, g_status_records[2].wait_hint); // [確認_正常系] - 3 回目の dwWaitHint が 3000 であること。
}

// on_start() が失敗したとき SERVICE_RUNNING を出さずに SERVICE_STOPPED を失敗値で 1 回報告することの確認
TEST(serviceWindowsTest, start_failure_reports_stopped_once_without_running)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x60));
    cplat_service_definition def = make_def();
    def.on_start = on_start_fails;
    def.on_stop = on_stop_fails;
    int exit_code = -1;
    arrange_service_dispatch(mock_cplat, mock_windows,
                             fake_status_handle); // [状態] - ServiceMain を駆動し SetServiceStatus() の引数を記録する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(nullptr, CPLAT_TRACE_LEVEL_ERROR, _,
                                                   HasSubstr("on_start が失敗しました (戻り値: 7)。")))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - on_start 失敗の ERROR トレースが 1 回出力されること。
                                     // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。

    // Act
    int actual_ret =
        cplat_service_run(&def, &exit_code); // [手順] - on_start() が失敗する定義で cplat_service_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);        // [確認_異常系] - cplat_service_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(7, exit_code);                // [確認_異常系] - exit_code が 7 であること。
    ASSERT_EQ(2U, g_status_records.size()); // [確認_異常系] - g_status_records.size() の戻り値が 2U であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_START_PENDING),
              g_status_records[0].state); // [確認_異常系] - 1 回目の状態が SERVICE_START_PENDING であること。
    EXPECT_EQ(
        static_cast<DWORD>(SERVICE_STOPPED),
        g_status_records[1]
            .state); // [確認_異常系] - 2 回目の状態が SERVICE_STOPPED であること。(SERVICE_RUNNING が報告されないことの確認)
    EXPECT_EQ(
        static_cast<DWORD>(ERROR_SERVICE_SPECIFIC_ERROR),
        g_status_records[1]
            .win32_exit_code); // [確認_異常系] - 2 回目の dwWin32ExitCode が ERROR_SERVICE_SPECIFIC_ERROR であること。
    EXPECT_EQ(7U,
              g_status_records[1]
                  .service_specific_exit_code); // [確認_異常系] - 2 回目の dwServiceSpecificExitCode が 7 であること。
}

// on_run() と on_stop() の両方が失敗したとき dwServiceSpecificExitCode と exit_code が on_run() の値であることの確認
TEST(serviceWindowsTest, run_and_stop_failures_report_on_run_exit_code)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    SERVICE_STATUS_HANDLE fake_status_handle = reinterpret_cast<SERVICE_STATUS_HANDLE>(static_cast<uintptr_t>(0x70));
    cplat_service_definition def = make_def();
    def.on_run = on_run_fails;
    def.on_stop = on_stop_fails;
    int exit_code = -1;
    arrange_service_dispatch(mock_cplat, mock_windows,
                             fake_status_handle); // [状態] - ServiceMain を駆動し SetServiceStatus() の引数を記録する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(nullptr, CPLAT_TRACE_LEVEL_ERROR, _,
                                                   HasSubstr("on_run が失敗しました (戻り値: 4)。")))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - on_run 失敗の ERROR トレースが 1 回出力されること。
                                     // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(nullptr, CPLAT_TRACE_LEVEL_ERROR, _,
                                                   HasSubstr("on_stop が失敗しました (戻り値: 5)。")))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - on_stop 失敗の ERROR トレースが 1 回出力されること。
                                     // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_service_run(
        &def, &exit_code); // [手順] - on_run() と on_stop() が失敗する定義で cplat_service_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);        // [確認_異常系] - cplat_service_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(4, exit_code);                // [確認_異常系] - exit_code が 4 であること。
    ASSERT_EQ(4U, g_status_records.size()); // [確認_異常系] - g_status_records.size() の戻り値が 4U であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_START_PENDING),
              g_status_records[0].state); // [確認_異常系] - 1 回目の状態が SERVICE_START_PENDING であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_RUNNING),
              g_status_records[1].state); // [確認_異常系] - 2 回目の状態が SERVICE_RUNNING であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_STOP_PENDING),
              g_status_records[2].state);          // [確認_異常系] - 3 回目の状態が SERVICE_STOP_PENDING であること。
    EXPECT_EQ(2U, g_status_records[2].checkpoint); // [確認_異常系] - 3 回目の checkpoint が 2 であること。
    EXPECT_EQ(static_cast<DWORD>(SERVICE_STOPPED),
              g_status_records[3].state); // [確認_異常系] - 4 回目の状態が SERVICE_STOPPED であること。
    EXPECT_EQ(
        static_cast<DWORD>(ERROR_SERVICE_SPECIFIC_ERROR),
        g_status_records[3]
            .win32_exit_code); // [確認_異常系] - 4 回目の dwWin32ExitCode が ERROR_SERVICE_SPECIFIC_ERROR であること。
    EXPECT_EQ(4U,
              g_status_records[3]
                  .service_specific_exit_code); // [確認_異常系] - 4 回目の dwServiceSpecificExitCode が 4 であること。
}

#endif /* PLATFORM_WINDOWS */
