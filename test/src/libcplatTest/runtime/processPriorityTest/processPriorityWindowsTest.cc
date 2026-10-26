#include <testfw.h>
#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <mock_windows.h>
    #include <mock_cplat.h>
    #include <cplat/runtime/process_priority_windows.h>

    #include <thread>

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace
{

// 存在しないプロセス ID とスレッド ID の代わりに、mock で開けない対象として使う ID。
constexpr uint32_t kTargetId = 4321U;
// mock の OpenProcess() / OpenThread() が返すセンチネル ハンドル。
HANDLE const kSentinelHandle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(0x1234));

// 作業スレッドで実機の API を呼び出した結果。
struct worker_results
{
    int set_lowest;
    int get_priority;
    cplat_process_priority_windows_thread_priority priority;
};

// 作業スレッドでスレッド優先度を変更し、テスト プロセスのメイン スレッドの優先度を変えずに確かめる。
void run_on_worker_thread(worker_results *results)
{
    results->priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_NORMAL;
    results->set_lowest = cplat_process_priority_windows_set_thread_priority(
        0U, CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_LOWEST, nullptr);
    results->get_priority = cplat_process_priority_windows_get_thread_priority(0U, &results->priority, nullptr);
}

} // namespace

class processPriorityWindowsTest : public Test
{
};

// 範囲外の引数と nullptr の出力引数が拒否されることの確認
TEST_F(processPriorityWindowsTest, functions_reject_invalid_arguments)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_class = cplat_process_priority_windows_set_class(
        0U, static_cast<cplat_process_priority_windows_class>(99),
        nullptr); // [手順] - 列挙型にない値 99 を指定して cplat_process_priority_windows_set_class() を呼び出す。
    int actual_ret_memory = cplat_process_priority_windows_set_memory_priority(
        0U, static_cast<cplat_process_priority_windows_memory_priority>(0),
        nullptr); // [手順] - 列挙型にない値 0 を指定して cplat_process_priority_windows_set_memory_priority() を呼び出す。
    int actual_ret_boost = cplat_process_priority_windows_set_priority_boost(
        0U, 2, nullptr); // [手順] - enabled に 2 を指定して cplat_process_priority_windows_set_priority_boost() を呼び出す。
    int actual_ret_get_class = cplat_process_priority_windows_get_class(
        0U, nullptr, nullptr); // [手順] - priority_class_out に nullptr を指定して cplat_process_priority_windows_get_class() を呼び出す。
    int actual_ret_get_thread = cplat_process_priority_windows_get_thread_priority(
        0U, nullptr,
        nullptr); // [手順] - thread_priority_out に nullptr を指定して cplat_process_priority_windows_get_thread_priority() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_class); // [確認_異常系] - 値 99 に対する cplat_process_priority_windows_set_class() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_memory); // [確認_異常系] - 値 0 に対する cplat_process_priority_windows_set_memory_priority() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_boost); // [確認_異常系] - enabled 2 に対する cplat_process_priority_windows_set_priority_boost() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_class); // [確認_異常系] - nullptr の priority_class_out に対する cplat_process_priority_windows_get_class() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_thread); // [確認_異常系] - nullptr の thread_priority_out に対する cplat_process_priority_windows_get_thread_priority() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// REAL_TIME が HIGH へ格下げされた場合に元のクラスへ戻し、権限不足を返すことの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-016; uuid=b2d649a0-78a9-4050-a075-f22bac73fb70
TEST_F(processPriorityWindowsTest, set_class_restores_class_when_real_time_is_downgraded)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetPriorityClass(_, _, _, _))
        .WillOnce(Return((DWORD)NORMAL_PRIORITY_CLASS))
        .WillOnce(Return((DWORD)HIGH_PRIORITY_CLASS)); // [Pre-Assert確認_異常系] - GetPriorityClass() が 2 回呼び出されること。
                                                       // [Pre-Assert手順] - GetPriorityClass() から 1 回目は NORMAL_PRIORITY_CLASS、2 回目は HIGH_PRIORITY_CLASS を返す。
    EXPECT_CALL(mock_windows, SetPriorityClass(_, _, _, _, (DWORD)REALTIME_PRIORITY_CLASS))
        .WillOnce(Return(TRUE)); // [Pre-Assert確認_異常系] - SetPriorityClass() が REALTIME_PRIORITY_CLASS を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - SetPriorityClass() から TRUE を返す。
    EXPECT_CALL(mock_windows, SetPriorityClass(_, _, _, _, (DWORD)NORMAL_PRIORITY_CLASS))
        .WillOnce(Return(TRUE)); // [Pre-Assert確認_異常系] - SetPriorityClass() が NORMAL_PRIORITY_CLASS を指定して 1 回呼び出されること。(元のクラスへ戻すことの確認)
                                 // [Pre-Assert手順] - SetPriorityClass() から TRUE を返す。

    // Act
    int actual_ret = cplat_process_priority_windows_set_class(
        0U, CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_REAL_TIME,
        &detail); // [手順] - REAL_TIME を指定して cplat_process_priority_windows_set_class() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_ret); // [確認_異常系] - cplat_process_priority_windows_set_class() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(
        (unsigned long)ERROR_PRIVILEGE_NOT_HELD,
        cplat_error_get_windows_error(&detail)); // [確認_異常系] - cplat_error_get_windows_error() の戻り値が ERROR_PRIVILEGE_NOT_HELD であること。
}

// 対象を開けない場合に、存在しない ID と権限不足が区別されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-016; uuid=b2d649a0-78a9-4050-a075-f22bac73fb70
TEST_F(processPriorityWindowsTest, open_failures_map_to_not_found_and_permission_denied)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_process_priority_windows_class priority_class = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_NORMAL;
    cplat_process_priority_windows_thread_priority thread_priority =
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_NORMAL; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, OpenProcess(_, _, _, _, _, (DWORD)kTargetId))
        .WillOnce(Return((HANDLE) nullptr))
        .WillOnce(Return((HANDLE) nullptr)); // [Pre-Assert確認_異常系] - OpenProcess() が kTargetId を指定して 2 回呼び出されること。
                                             // [Pre-Assert手順] - OpenProcess() から nullptr を返す。
    EXPECT_CALL(mock_windows, OpenThread(_, _, _, _, _, (DWORD)kTargetId))
        .WillOnce(Return((HANDLE) nullptr)); // [Pre-Assert確認_異常系] - OpenThread() が kTargetId を指定して 1 回呼び出されること。
                                             // [Pre-Assert手順] - OpenThread() から nullptr を返す。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return((DWORD)ERROR_INVALID_PARAMETER))
        .WillOnce(Return((DWORD)ERROR_ACCESS_DENIED))
        .WillOnce(Return((DWORD)ERROR_INVALID_PARAMETER)); // [Pre-Assert確認_異常系] - GetLastError() が 3 回呼び出されること。
                                                           // [Pre-Assert手順] - GetLastError() から 1 回目は ERROR_INVALID_PARAMETER、2 回目は ERROR_ACCESS_DENIED、3 回目は ERROR_INVALID_PARAMETER を返す。

    // Act
    int actual_ret_not_found = cplat_process_priority_windows_get_class(
        kTargetId, &priority_class, nullptr); // [手順] - kTargetId を指定して cplat_process_priority_windows_get_class() を呼び出す。
    int actual_ret_denied = cplat_process_priority_windows_set_class(
        kTargetId, CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_IDLE,
        nullptr); // [手順] - kTargetId と IDLE を指定して cplat_process_priority_windows_set_class() を呼び出す。
    int actual_ret_thread = cplat_process_priority_windows_get_thread_priority(
        kTargetId, &thread_priority, nullptr); // [手順] - kTargetId を指定して cplat_process_priority_windows_get_thread_priority() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_not_found); // [確認_異常系] - ERROR_INVALID_PARAMETER に対する cplat_process_priority_windows_get_class() の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_ret_denied); // [確認_異常系] - ERROR_ACCESS_DENIED に対する cplat_process_priority_windows_set_class() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_thread); // [確認_異常系] - ERROR_INVALID_PARAMETER に対する cplat_process_priority_windows_get_thread_priority() の戻り値が CPLAT_ERR_NOT_FOUND であること。
}

// 開いた対象のハンドルが関数を抜ける前に閉じられることの確認
TEST_F(processPriorityWindowsTest, opened_handle_is_closed)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    int enabled = -1; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, OpenProcess(_, _, _, (DWORD)PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)kTargetId))
        .WillOnce(Return(kSentinelHandle)); // [Pre-Assert確認_正常系] - OpenProcess() が PROCESS_QUERY_LIMITED_INFORMATION、FALSE、kTargetId を指定して 1 回呼び出されること。
                                            // [Pre-Assert手順] - OpenProcess() から kSentinelHandle を返す。
    EXPECT_CALL(mock_windows, GetProcessPriorityBoost(_, _, _, kSentinelHandle, _))
        .WillOnce(testing::DoAll(testing::SetArgPointee<4>(TRUE),
                                 Return(TRUE))); // [Pre-Assert確認_正常系] - GetProcessPriorityBoost() が kSentinelHandle を指定して 1 回呼び出されること。
                                                 // [Pre-Assert手順] - GetProcessPriorityBoost() から、無効を表す TRUE を設定して TRUE を返す。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, kSentinelHandle))
        .WillOnce(Return(TRUE)); // [Pre-Assert確認_正常系] - CloseHandle() が kSentinelHandle を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - CloseHandle() から TRUE を返す。

    // Act
    int actual_ret = cplat_process_priority_windows_get_priority_boost(
        kTargetId, &enabled, nullptr); // [手順] - kTargetId を指定して cplat_process_priority_windows_get_priority_boost() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_process_priority_windows_get_priority_boost() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, enabled); // [確認_正常系] - enabled が 0 であること。(OS の無効を表す値が反転されたことの確認)
}

// バックグラウンド処理モードの二重開始と、モード外での終了が CPLAT_SKIPPED になることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-015; uuid=7bae4d4b-7ce1-4d6f-9560-c4618c7313e9
TEST_F(processPriorityWindowsTest, background_mode_reports_skipped_for_redundant_calls)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_error detail_begin;
    cplat_error detail_end; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetPriorityClass(_, _, _, _, (DWORD)PROCESS_MODE_BACKGROUND_BEGIN))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetPriorityClass() が PROCESS_MODE_BACKGROUND_BEGIN を指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - SetPriorityClass() から FALSE を返す。
    EXPECT_CALL(mock_windows, SetPriorityClass(_, _, _, _, (DWORD)PROCESS_MODE_BACKGROUND_END))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetPriorityClass() が PROCESS_MODE_BACKGROUND_END を指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - SetPriorityClass() から FALSE を返す。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return((DWORD)ERROR_PROCESS_MODE_ALREADY_BACKGROUND))
        .WillOnce(Return((DWORD)ERROR_PROCESS_MODE_NOT_BACKGROUND)); // [Pre-Assert確認_異常系] - GetLastError() が 2 回呼び出されること。
                                                                     // [Pre-Assert手順] - GetLastError() から 1 回目は ERROR_PROCESS_MODE_ALREADY_BACKGROUND、2 回目は ERROR_PROCESS_MODE_NOT_BACKGROUND を返す。

    // Act
    int actual_ret_begin = cplat_process_priority_windows_begin_background_mode(
        &detail_begin); // [手順] - cplat_process_priority_windows_begin_background_mode() を呼び出す。
    int actual_ret_end = cplat_process_priority_windows_end_background_mode(
        &detail_end); // [手順] - cplat_process_priority_windows_end_background_mode() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_begin); // [確認_正常系] - cplat_process_priority_windows_begin_background_mode() の戻り値が CPLAT_SKIPPED であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail_begin)); // [確認_正常系] - detail_begin に対する cplat_error_is_set() の戻り値が 0 であること。(省略時に詳細エラーがクリアされたことの確認)
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_end); // [確認_正常系] - cplat_process_priority_windows_end_background_mode() の戻り値が CPLAT_SKIPPED であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail_end)); // [確認_正常系] - detail_end に対する cplat_error_is_set() の戻り値が 0 であること。(省略時に詳細エラーがクリアされたことの確認)
}

// 列挙型にないスレッド優先度を取得した場合に CPLAT_ERR_UNKNOWN を返すことの確認
TEST_F(processPriorityWindowsTest, get_thread_priority_reports_unknown_value)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_process_priority_windows_thread_priority thread_priority =
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_IDLE; // [状態] - 出力引数に目印の値を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetThreadPriority(_, _, _, _))
        .WillOnce(Return(3)); // [Pre-Assert確認_異常系] - GetThreadPriority() が 1 回呼び出されること。
                              // [Pre-Assert手順] - GetThreadPriority() から REALTIME クラス専用の値 3 を返す。

    // Act
    int actual_ret = cplat_process_priority_windows_get_thread_priority(
        0U, &thread_priority, nullptr); // [手順] - cplat_process_priority_windows_get_thread_priority() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - cplat_process_priority_windows_get_thread_priority() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_IDLE,
        thread_priority); // [確認_異常系] - thread_priority が CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_IDLE であること。(出力引数が変更されないことの確認)
}

// 自プロセスの優先度クラスを下げて取得し、元へ戻せることの確認 (マルチ フェーズ テスト)
// cplat-req: id=CPLAT-RUNTIME-FUNC-015; uuid=7bae4d4b-7ce1-4d6f-9560-c4618c7313e9
TEST_F(processPriorityWindowsTest, own_process_class_round_trip)
{
    // Arrange
    cplat_process_priority_windows_class original = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_NORMAL;
    cplat_process_priority_windows_class lowered = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_NORMAL; // [状態] - 出力引数の格納先を用意する。
    ASSERT_EQ(CPLAT_OK, cplat_process_priority_windows_get_class(0U, &original, nullptr)); // [状態確認] - 変更前の cplat_process_priority_windows_get_class() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_process_priority_windows_set_class(
        0U, CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_BELOW_NORMAL,
        nullptr); // [手順] - 自プロセスと BELOW_NORMAL を指定して cplat_process_priority_windows_set_class() を呼び出す。
    int actual_ret_get = cplat_process_priority_windows_get_class(
        0U, &lowered, nullptr); // [手順] - 自プロセスを指定して cplat_process_priority_windows_get_class() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_set); // [確認_正常系] - cplat_process_priority_windows_set_class() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_get); // [確認_正常系] - cplat_process_priority_windows_get_class() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_BELOW_NORMAL, lowered); // [確認_正常系] - lowered が CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_BELOW_NORMAL であること。

    // Act_2
    int actual_ret_restore = cplat_process_priority_windows_set_class(
        0U, original, nullptr); // [手順] - 変更前のクラスを指定して cplat_process_priority_windows_set_class() を呼び出す。

    // Assert_2
    EXPECT_EQ(CPLAT_OK, actual_ret_restore); // [確認_正常系] - 復元の cplat_process_priority_windows_set_class() の戻り値が CPLAT_OK であること。
}

// 実機のバックグラウンド処理モードで、開始と終了の二重呼び出しが CPLAT_SKIPPED になることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-015; uuid=7bae4d4b-7ce1-4d6f-9560-c4618c7313e9
TEST_F(processPriorityWindowsTest, own_process_background_mode_round_trip)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_begin = cplat_process_priority_windows_begin_background_mode(
        nullptr); // [手順] - cplat_process_priority_windows_begin_background_mode() を呼び出す。
    int actual_ret_begin_again = cplat_process_priority_windows_begin_background_mode(
        nullptr); // [手順] - cplat_process_priority_windows_begin_background_mode() をもう一度呼び出す。
    int actual_ret_end = cplat_process_priority_windows_end_background_mode(
        nullptr); // [手順] - cplat_process_priority_windows_end_background_mode() を呼び出す。
    int actual_ret_end_again = cplat_process_priority_windows_end_background_mode(
        nullptr); // [手順] - cplat_process_priority_windows_end_background_mode() をもう一度呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_begin); // [確認_正常系] - 1 回目の cplat_process_priority_windows_begin_background_mode() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_begin_again); // [確認_正常系] - 2 回目の cplat_process_priority_windows_begin_background_mode() の戻り値が CPLAT_SKIPPED であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_end); // [確認_正常系] - 1 回目の cplat_process_priority_windows_end_background_mode() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_end_again); // [確認_正常系] - 2 回目の cplat_process_priority_windows_end_background_mode() の戻り値が CPLAT_SKIPPED であること。
}

// 作業スレッドのスレッド優先度を設定して取得できることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-015; uuid=7bae4d4b-7ce1-4d6f-9560-c4618c7313e9
TEST_F(processPriorityWindowsTest, worker_thread_priority_round_trip)
{
    // Arrange
    worker_results results = {}; // [状態] - 作業スレッドの結果格納先を用意する。

    // Pre-Assert

    // Act
    std::thread worker(run_on_worker_thread,
                       &results); // [手順] - 作業スレッドで run_on_worker_thread() を呼び出す。
    worker.join();

    // Assert
    EXPECT_EQ(CPLAT_OK, results.set_lowest);   // [確認_正常系] - results.set_lowest が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, results.get_priority); // [確認_正常系] - results.get_priority が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_LOWEST, results.priority); // [確認_正常系] - results.priority が CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_LOWEST であること。
}

#endif /* PLATFORM_WINDOWS */
