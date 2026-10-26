#include <testfw.h>
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <mock_sched.h>
    #include <mock_unistd.h>
    #include <sys/mock_resource.h>
    #include <mock_cplat.h>
    #include <cplat/runtime/process_priority_linux.h>

    #include <cerrno>
    #include <cstdint>
    #include <cstring>
    #include <sched.h>
    #include <sys/resource.h>
    #include <sys/syscall.h>
    #include <thread>
    #include <unistd.h>

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{

// テスト側で sched_attr の内容を確かめるための、カーネルと同じ配置の構造体。
struct test_sched_attr
{
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
    uint32_t sched_util_min;
    uint32_t sched_util_max;
};

constexpr uint32_t kThreadId = 4321U;
constexpr int kIoprioClassShift = 13;
constexpr int kSchedDeadline = 6;
constexpr int kSchedResetOnFork = 0x40000000;

// 作業スレッドで実機の API を呼び出した結果。
struct worker_results
{
    int lower_nice;
    int get_nice;
    int nice_value;
    int raise_nice;
    int real_time_policy;
    int idle_io;
    int real_time_io;
};

// 作業スレッドで優先度を変更し、テスト プロセスのメイン スレッドの優先度を変えずに確かめる。
void run_on_worker_thread(worker_results *results)
{
    results->nice_value = 0;
    results->lower_nice = cplat_process_priority_linux_set_nice(0U, 10, nullptr);
    results->get_nice = cplat_process_priority_linux_get_nice(0U, &results->nice_value, nullptr);
    results->raise_nice = cplat_process_priority_linux_set_nice(0U, 0, nullptr);
    results->real_time_policy =
        cplat_process_priority_linux_set_policy(0U, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO, 1, 0, nullptr);
    results->idle_io =
        cplat_process_priority_linux_set_io_priority(0U, CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE, 0, nullptr);
    results->real_time_io =
        cplat_process_priority_linux_set_io_priority(0U, CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_REAL_TIME, 0, nullptr);
}

// 特権 (root、nice と リアルタイム優先度の資源上限) を持たない実行環境かを判定する。
bool is_unprivileged()
{
    struct rlimit nice_limit;
    struct rlimit real_time_limit;

    if (geteuid() == 0)
    {
        return false;
    }
    if ((getrlimit(RLIMIT_NICE, &nice_limit) != 0) || (getrlimit(RLIMIT_RTPRIO, &real_time_limit) != 0))
    {
        return false;
    }
    return (nice_limit.rlim_cur == 0) && (real_time_limit.rlim_cur == 0);
}

} // namespace

class processPriorityLinuxTest : public Test
{
};

// 範囲外の引数が OS を呼び出さずに拒否されることの確認
TEST_F(processPriorityLinuxTest, set_functions_reject_invalid_arguments)
{
    // Arrange
    NiceMock<Mock_sched> mock_sched;
    NiceMock<Mock_sys_resource> mock_sys_resource;
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_priority_linux_deadline deadline = {1000000U, 2000000U, 3000000U}; // [状態] - デッド ラインのパラメーターを用意する。
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sched, sched_setscheduler(_, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - sched_setscheduler() が呼び出されないこと。
    EXPECT_CALL(mock_sys_resource, setpriority(_, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - setpriority() が呼び出されないこと。
    EXPECT_CALL(mock_unistd, syscall(_, _, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - syscall() が呼び出されないこと。

    // Act
    int actual_ret_deadline_policy = cplat_process_priority_linux_set_policy(
        0U, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_DEADLINE, 0, 0,
        nullptr); // [手順] - DEADLINE を指定して cplat_process_priority_linux_set_policy() を呼び出す。
    int actual_ret_fifo_zero = cplat_process_priority_linux_set_policy(
        0U, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO, 0, 0,
        nullptr); // [手順] - FIFO と静的優先度 0 を指定して cplat_process_priority_linux_set_policy() を呼び出す。
    int actual_ret_other_one = cplat_process_priority_linux_set_policy(
        0U, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER, 1, 0,
        nullptr); // [手順] - OTHER と静的優先度 1 を指定して cplat_process_priority_linux_set_policy() を呼び出す。
    int actual_ret_reset_flag = cplat_process_priority_linux_set_policy(
        0U, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER, 0, 2,
        nullptr); // [手順] - reset_on_fork に 2 を指定して cplat_process_priority_linux_set_policy() を呼び出す。
    int actual_ret_large_id = cplat_process_priority_linux_set_nice(
        0x80000000U, 0, nullptr); // [手順] - INT32_MAX を超える ID を指定して cplat_process_priority_linux_set_nice() を呼び出す。
    int actual_ret_nice_low = cplat_process_priority_linux_set_nice(
        0U, -21, nullptr); // [手順] - nice 値 -21 を指定して cplat_process_priority_linux_set_nice() を呼び出す。
    int actual_ret_nice_high = cplat_process_priority_linux_set_nice(
        0U, 20, nullptr); // [手順] - nice 値 20 を指定して cplat_process_priority_linux_set_nice() を呼び出す。
    int actual_ret_null_deadline = cplat_process_priority_linux_set_deadline(
        0U, nullptr, 0, nullptr); // [手順] - deadline に nullptr を指定して cplat_process_priority_linux_set_deadline() を呼び出す。
    int actual_ret_deadline_reset = cplat_process_priority_linux_set_deadline(
        0U, &deadline, 2, nullptr); // [手順] - reset_on_fork に 2 を指定して cplat_process_priority_linux_set_deadline() を呼び出す。
    int actual_ret_io_level = cplat_process_priority_linux_set_io_priority(
        0U, CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_BEST_EFFORT, 8,
        nullptr); // [手順] - BEST_EFFORT とレベル 8 を指定して cplat_process_priority_linux_set_io_priority() を呼び出す。
    int actual_ret_io_idle_level = cplat_process_priority_linux_set_io_priority(
        0U, CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE, 1,
        nullptr); // [手順] - IDLE とレベル 1 を指定して cplat_process_priority_linux_set_io_priority() を呼び出す。
    int actual_ret_clamp_max = cplat_process_priority_linux_set_utilization_clamp(
        0U, 0U, 1025U, nullptr); // [手順] - 上限 1025 を指定して cplat_process_priority_linux_set_utilization_clamp() を呼び出す。
    int actual_ret_clamp_order = cplat_process_priority_linux_set_utilization_clamp(
        0U, 600U, 500U,
        &detail); // [手順] - 下限 600 と上限 500 を指定して cplat_process_priority_linux_set_utilization_clamp() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_deadline_policy); // [確認_異常系] - DEADLINE に対する cplat_process_priority_linux_set_policy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_fifo_zero); // [確認_異常系] - FIFO と静的優先度 0 に対する cplat_process_priority_linux_set_policy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_other_one); // [確認_異常系] - OTHER と静的優先度 1 に対する cplat_process_priority_linux_set_policy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_reset_flag); // [確認_異常系] - reset_on_fork 2 に対する cplat_process_priority_linux_set_policy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_large_id); // [確認_異常系] - INT32_MAX を超える ID に対する cplat_process_priority_linux_set_nice() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_nice_low); // [確認_異常系] - nice 値 -21 に対する cplat_process_priority_linux_set_nice() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_nice_high); // [確認_異常系] - nice 値 20 に対する cplat_process_priority_linux_set_nice() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_deadline); // [確認_異常系] - nullptr の deadline に対する cplat_process_priority_linux_set_deadline() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_deadline_reset); // [確認_異常系] - reset_on_fork 2 に対する cplat_process_priority_linux_set_deadline() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_io_level); // [確認_異常系] - BEST_EFFORT とレベル 8 に対する cplat_process_priority_linux_set_io_priority() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_io_idle_level); // [確認_異常系] - IDLE とレベル 1 に対する cplat_process_priority_linux_set_io_priority() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_clamp_max); // [確認_異常系] - 上限 1025 に対する cplat_process_priority_linux_set_utilization_clamp() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_clamp_order); // [確認_異常系] - 下限 600 と上限 500 に対する cplat_process_priority_linux_set_utilization_clamp() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EINVAL であること。
}

// 取得関数が nullptr の出力引数を拒否することの確認
TEST_F(processPriorityLinuxTest, get_functions_reject_null_outputs)
{
    // Arrange
    cplat_process_priority_linux_policy policy;
    int value = 0;
    uint32_t clamp = 0U; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret_policy = cplat_process_priority_linux_get_policy(
        0U, nullptr, &value, &value,
        nullptr); // [手順] - policy_out に nullptr を指定して cplat_process_priority_linux_get_policy() を呼び出す。
    int actual_ret_reset = cplat_process_priority_linux_get_policy(
        0U, &policy, &value, nullptr,
        nullptr); // [手順] - reset_on_fork_out に nullptr を指定して cplat_process_priority_linux_get_policy() を呼び出す。
    int actual_ret_deadline = cplat_process_priority_linux_get_deadline(
        0U, nullptr, nullptr); // [手順] - deadline_out に nullptr を指定して cplat_process_priority_linux_get_deadline() を呼び出す。
    int actual_ret_nice = cplat_process_priority_linux_get_nice(
        0U, nullptr, nullptr); // [手順] - nice_value_out に nullptr を指定して cplat_process_priority_linux_get_nice() を呼び出す。
    int actual_ret_io = cplat_process_priority_linux_get_io_priority(
        0U, nullptr, &value, nullptr); // [手順] - io_class_out に nullptr を指定して cplat_process_priority_linux_get_io_priority() を呼び出す。
    int actual_ret_clamp = cplat_process_priority_linux_get_utilization_clamp(
        0U, &clamp, nullptr,
        nullptr); // [手順] - maximum_out に nullptr を指定して cplat_process_priority_linux_get_utilization_clamp() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_policy); // [確認_異常系] - nullptr の policy_out に対する cplat_process_priority_linux_get_policy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_reset); // [確認_異常系] - nullptr の reset_on_fork_out に対する cplat_process_priority_linux_get_policy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_deadline); // [確認_異常系] - nullptr の deadline_out に対する cplat_process_priority_linux_get_deadline() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_nice); // [確認_異常系] - nullptr の nice_value_out に対する cplat_process_priority_linux_get_nice() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_io); // [確認_異常系] - nullptr の io_class_out に対する cplat_process_priority_linux_get_io_priority() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_clamp); // [確認_異常系] - nullptr の maximum_out に対する cplat_process_priority_linux_get_utilization_clamp() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// ポリシー、静的優先度、fork 時の復帰フラグが 1 回の sched_setscheduler() へ渡されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, set_policy_passes_policy_priority_and_reset_flag)
{
    // Arrange
    NiceMock<Mock_sched> mock_sched;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sched, sched_setscheduler(_, _, _, (pid_t)kThreadId, SCHED_RR | kSchedResetOnFork, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, pid_t, int, const struct sched_param *param)
            {
                EXPECT_EQ(
                    10,
                    param->sched_priority); // [Pre-Assert確認_正常系] - param->sched_priority が 10 であること。
                return 0;
            })); // [Pre-Assert確認_正常系] - sched_setscheduler() が kThreadId、SCHED_RR と SCHED_RESET_ON_FORK の論理和を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - sched_setscheduler() から 0 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_set_policy(
        kThreadId, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_ROUND_ROBIN, 10, 1,
        &detail); // [手順] - kThreadId、ROUND_ROBIN、静的優先度 10、reset_on_fork 1 を指定して cplat_process_priority_linux_set_policy() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_process_priority_linux_set_policy() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - cplat_error_is_set() の戻り値が 0 であること。(成功時に詳細エラーがクリアされたことの確認)
}

// OS の権限不足と対象不在が結果コードと詳細エラーで通知されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-016; uuid=b2d649a0-78a9-4050-a075-f22bac73fb70
TEST_F(processPriorityLinuxTest, set_policy_reports_permission_and_not_found)
{
    // Arrange
    NiceMock<Mock_sched> mock_sched;
    cplat_error detail_permission;
    cplat_error detail_not_found;
    cplat_error last; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sched, sched_setscheduler(_, _, _, _, _, _))
        .WillOnce(DoAll(testing::Assign(&errno, EPERM), Return(-1)))
        .WillOnce(DoAll(testing::Assign(&errno, ESRCH),
                        Return(-1))); // [Pre-Assert確認_異常系] - sched_setscheduler() が 2 回呼び出されること。
                                      // [Pre-Assert手順] - sched_setscheduler() から 1 回目は errno に EPERM、2 回目は errno に ESRCH を設定して -1 を返す。

    // Act
    int actual_ret_permission = cplat_process_priority_linux_set_policy(
        0U, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO, 50, 0,
        &detail_permission); // [手順] - FIFO と静的優先度 50 を指定して cplat_process_priority_linux_set_policy() を呼び出す。
    cplat_error_get_last(&last); // [手順] - cplat_error_get_last() を呼び出す。
    int actual_ret_not_found = cplat_process_priority_linux_set_policy(
        kThreadId, CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER, 0, 0,
        &detail_not_found); // [手順] - kThreadId と OTHER を指定して cplat_process_priority_linux_set_policy() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_ret_permission); // [確認_異常系] - EPERM に対する cplat_process_priority_linux_set_policy() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(
        EPERM,
        cplat_error_get_errno(&detail_permission)); // [確認_異常系] - detail_permission に対する cplat_error_get_errno() の戻り値が EPERM であること。
    EXPECT_EQ(
        EPERM,
        cplat_error_get_errno(&last)); // [確認_異常系] - last に対する cplat_error_get_errno() の戻り値が EPERM であること。(スレッド ローカルの直前値にも記録されたことの確認)
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_not_found); // [確認_異常系] - ESRCH に対する cplat_process_priority_linux_set_policy() の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(
        ESRCH,
        cplat_error_get_errno(&detail_not_found)); // [確認_異常系] - detail_not_found に対する cplat_error_get_errno() の戻り値が ESRCH であること。
}

// 取得したポリシーから fork 時の復帰フラグが分離されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, get_policy_separates_reset_flag)
{
    // Arrange
    NiceMock<Mock_sched> mock_sched;
    cplat_process_priority_linux_policy policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER;
    int static_priority = 0;
    int reset_on_fork = 0; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sched, sched_getscheduler(_, _, _, (pid_t)kThreadId))
        .WillOnce(Return(SCHED_FIFO | kSchedResetOnFork)); // [Pre-Assert確認_正常系] - sched_getscheduler() が kThreadId を指定して 1 回呼び出されること。
                                                           // [Pre-Assert手順] - sched_getscheduler() から SCHED_FIFO と SCHED_RESET_ON_FORK の論理和を返す。
    EXPECT_CALL(mock_sched, sched_getparam(_, _, _, (pid_t)kThreadId, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, pid_t, struct sched_param *param)
            {
                param->sched_priority = 20;
                return 0;
            })); // [Pre-Assert確認_正常系] - sched_getparam() が kThreadId を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - sched_getparam() から、静的優先度に 20 を設定して 0 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_get_policy(
        kThreadId, &policy, &static_priority, &reset_on_fork,
        nullptr); // [手順] - kThreadId を指定して cplat_process_priority_linux_get_policy() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_process_priority_linux_get_policy() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO, policy); // [確認_正常系] - policy が CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO であること。
    EXPECT_EQ(20, static_priority); // [確認_正常系] - static_priority が 20 であること。
    EXPECT_EQ(1, reset_on_fork);    // [確認_正常系] - reset_on_fork が 1 であること。
}

// 列挙型にないポリシーを取得した場合に出力引数を変えず CPLAT_ERR_UNKNOWN を返すことの確認
TEST_F(processPriorityLinuxTest, get_policy_reports_unknown_policy)
{
    // Arrange
    NiceMock<Mock_sched> mock_sched;
    cplat_process_priority_linux_policy policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH;
    int static_priority = -1;
    int reset_on_fork = -1; // [状態] - 出力引数に目印の値を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_sched, sched_getscheduler(_, _, _, _))
        .WillOnce(Return(7)); // [Pre-Assert確認_異常系] - sched_getscheduler() が 1 回呼び出されること。
                              // [Pre-Assert手順] - sched_getscheduler() から 7 を返す。
    EXPECT_CALL(mock_sched, sched_getparam(_, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - sched_getparam() が 1 回呼び出されること。
                              // [Pre-Assert手順] - sched_getparam() から 0 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_get_policy(
        0U, &policy, &static_priority, &reset_on_fork, nullptr); // [手順] - cplat_process_priority_linux_get_policy() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - cplat_process_priority_linux_get_policy() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH, policy); // [確認_異常系] - policy が CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH であること。(出力引数が変更されないことの確認)
    EXPECT_EQ(-1, static_priority); // [確認_異常系] - static_priority が -1 であること。(出力引数が変更されないことの確認)
    EXPECT_EQ(-1, reset_on_fork);   // [確認_異常系] - reset_on_fork が -1 であること。(出力引数が変更されないことの確認)
}

// デッド ラインのパラメーターが最初の版の sched_attr で sched_setattr() へ渡されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, set_deadline_passes_first_version_attr)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_priority_linux_deadline deadline = {1000000U, 2000000U, 3000000U}; // [状態] - 実行時間 1 ms、期限 2 ms、周期 3 ms を用意する。
    test_sched_attr captured;
    std::memset(&captured, 0, sizeof(captured)); // [状態] - 渡された sched_attr の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_sched_setattr, (intptr_t)kThreadId, _, 0, _, _))
        .WillOnce(Invoke(
            [&captured](const char *, const int, const char *, long, intptr_t, intptr_t attr, intptr_t, intptr_t,
                        intptr_t)
            {
                std::memcpy(&captured, reinterpret_cast<const void *>(attr), 48U);
                return 0L;
            })); // [Pre-Assert確認_正常系] - syscall() が SYS_sched_setattr、kThreadId、0 を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - syscall() から、渡された sched_attr の先頭 48 バイトを captured へ複写して 0 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_set_deadline(
        kThreadId, &deadline, 1,
        nullptr); // [手順] - kThreadId、デッド ラインのパラメーター、reset_on_fork 1 を指定して cplat_process_priority_linux_set_deadline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_process_priority_linux_set_deadline() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(48U, captured.size);   // [確認_正常系] - captured.size が 48 であること。(最初の版の sched_attr であることの確認)
    EXPECT_EQ((uint32_t)kSchedDeadline, captured.sched_policy); // [確認_正常系] - captured.sched_policy が kSchedDeadline であること。
    EXPECT_EQ(1U, captured.sched_flags); // [確認_正常系] - captured.sched_flags が 1 であること。(SCHED_FLAG_RESET_ON_FORK が設定されたことの確認)
    EXPECT_EQ(1000000U, captured.sched_runtime);  // [確認_正常系] - captured.sched_runtime が 1000000 であること。
    EXPECT_EQ(2000000U, captured.sched_deadline); // [確認_正常系] - captured.sched_deadline が 2000000 であること。
    EXPECT_EQ(3000000U, captured.sched_period);   // [確認_正常系] - captured.sched_period が 3000000 であること。
}

// デッド ラインの帯域不足が CPLAT_ERR_BUSY で通知されることの確認
TEST_F(processPriorityLinuxTest, set_deadline_reports_busy)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_priority_linux_deadline deadline = {1000000U, 2000000U, 0U}; // [状態] - デッド ラインのパラメーターを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_sched_setattr, _, _, _, _, _))
        .WillOnce(DoAll(testing::Assign(&errno, EBUSY), Return(-1L))); // [Pre-Assert確認_異常系] - syscall() が SYS_sched_setattr を指定して 1 回呼び出されること。
                                                                       // [Pre-Assert手順] - syscall() から、errno に EBUSY を設定して -1 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_set_deadline(
        0U, &deadline, 0, nullptr); // [手順] - cplat_process_priority_linux_set_deadline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUSY, actual_ret); // [確認_異常系] - cplat_process_priority_linux_set_deadline() の戻り値が CPLAT_ERR_BUSY であること。
}

// 取得したデッド ラインのパラメーターが出力されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, get_deadline_outputs_attr_times)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_priority_linux_deadline deadline = {0U, 0U, 0U}; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_sched_getattr, (intptr_t)kThreadId, _, 48, 0, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, long, intptr_t, intptr_t attr, intptr_t, intptr_t, intptr_t)
            {
                test_sched_attr *out = reinterpret_cast<test_sched_attr *>(attr);
                out->size = 48U;
                out->sched_policy = (uint32_t)kSchedDeadline;
                out->sched_runtime = 500000U;
                out->sched_deadline = 1000000U;
                out->sched_period = 4000000U;
                return 0L;
            })); // [Pre-Assert確認_正常系] - syscall() が SYS_sched_getattr、kThreadId、48、0 を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - syscall() から、実行時間 0.5 ms、期限 1 ms、周期 4 ms を設定して 0 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_get_deadline(
        kThreadId, &deadline, nullptr); // [手順] - kThreadId を指定して cplat_process_priority_linux_get_deadline() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_process_priority_linux_get_deadline() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(500000U, deadline.runtime_nanoseconds);   // [確認_正常系] - deadline.runtime_nanoseconds が 500000 であること。
    EXPECT_EQ(1000000U, deadline.deadline_nanoseconds); // [確認_正常系] - deadline.deadline_nanoseconds が 1000000 であること。
    EXPECT_EQ(4000000U, deadline.period_nanoseconds);   // [確認_正常系] - deadline.period_nanoseconds が 4000000 であること。
}

// nice 値の設定と取得が setpriority() と getpriority() へ委譲され、-1 を正当な値として扱うことの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, nice_functions_delegate_and_distinguish_minus_one)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    int nice_value = 0;
    int unchanged_value = 99; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, setpriority(_, _, _, (int)PRIO_PROCESS, (id_t)kThreadId, 5))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - setpriority() が PRIO_PROCESS、kThreadId、5 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - setpriority() から 0 を返す。
    EXPECT_CALL(mock_sys_resource, getpriority(_, _, _, (int)PRIO_PROCESS, (id_t)kThreadId))
        .WillOnce(Return(-1))
        .WillOnce(DoAll(testing::Assign(&errno, ESRCH),
                        Return(-1))); // [Pre-Assert確認_正常系] - getpriority() が PRIO_PROCESS、kThreadId を指定して 2 回呼び出されること。
                                      // [Pre-Assert手順] - getpriority() から 1 回目は -1、2 回目は errno に ESRCH を設定して -1 を返す。

    // Act
    int actual_ret_set = cplat_process_priority_linux_set_nice(
        kThreadId, 5, nullptr); // [手順] - kThreadId と nice 値 5 を指定して cplat_process_priority_linux_set_nice() を呼び出す。
    int actual_ret_get = cplat_process_priority_linux_get_nice(
        kThreadId, &nice_value, nullptr); // [手順] - kThreadId を指定して cplat_process_priority_linux_get_nice() を呼び出す。
    int actual_ret_get_error = cplat_process_priority_linux_get_nice(
        kThreadId, &unchanged_value, nullptr); // [手順] - kThreadId を指定して cplat_process_priority_linux_get_nice() をもう一度呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_set); // [確認_正常系] - cplat_process_priority_linux_set_nice() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_get); // [確認_正常系] - 1 回目の cplat_process_priority_linux_get_nice() の戻り値が CPLAT_OK であること。(errno が変わらない -1 を正当な値として扱うことの確認)
    EXPECT_EQ(-1, nice_value);           // [確認_正常系] - nice_value が -1 であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_get_error); // [確認_異常系] - 2 回目の cplat_process_priority_linux_get_nice() の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(99, unchanged_value); // [確認_異常系] - unchanged_value が 99 であること。(失敗時に出力引数が変更されないことの確認)
}

// I/O 優先度がクラスとレベルの合成値で ioprio_set() へ渡されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, set_io_priority_combines_class_and_level)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_ioprio_set, 1, (intptr_t)kThreadId,
                                     (intptr_t)((2 << kIoprioClassShift) | 4), _, _))
        .WillOnce(Return(0L)); // [Pre-Assert確認_正常系] - syscall() が SYS_ioprio_set、1、kThreadId、BEST_EFFORT とレベル 4 の合成値を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - syscall() から 0 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_set_io_priority(
        kThreadId, CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_BEST_EFFORT, 4,
        nullptr); // [手順] - kThreadId、BEST_EFFORT、レベル 4 を指定して cplat_process_priority_linux_set_io_priority() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_process_priority_linux_set_io_priority() の戻り値が CPLAT_OK であること。
}

// 取得した I/O 優先度からクラスとレベルが分離され、ヒントのビットが除かれることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, get_io_priority_splits_class_and_level)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_process_priority_linux_io_class io_class = CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_NONE;
    int level = -1; // [状態] - 出力引数の格納先を用意する。
    cplat_process_priority_linux_io_class unchanged_class = CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE;
    int unchanged_level = -1; // [状態] - 失敗時の出力引数に目印の値を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_ioprio_get, 1, (intptr_t)kThreadId, _, _, _))
        .WillOnce(Return((long)((2 << kIoprioClassShift) | (1 << 3) | 5)))
        .WillOnce(Return((long)(5 << kIoprioClassShift))); // [Pre-Assert確認_正常系] - syscall() が SYS_ioprio_get、1、kThreadId を指定して 2 回呼び出されること。
                                                           // [Pre-Assert手順] - syscall() から 1 回目は BEST_EFFORT、ヒント、レベル 5 の合成値、2 回目は未知のクラス 5 の値を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_get_io_priority(
        kThreadId, &io_class, &level, nullptr); // [手順] - kThreadId を指定して cplat_process_priority_linux_get_io_priority() を呼び出す。
    int actual_ret_unknown = cplat_process_priority_linux_get_io_priority(
        kThreadId, &unchanged_class, &unchanged_level,
        nullptr); // [手順] - kThreadId を指定して cplat_process_priority_linux_get_io_priority() をもう一度呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 1 回目の cplat_process_priority_linux_get_io_priority() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_BEST_EFFORT, io_class); // [確認_正常系] - io_class が CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_BEST_EFFORT であること。
    EXPECT_EQ(5, level); // [確認_正常系] - level が 5 であること。(ヒントのビットが除かれたことの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_unknown); // [確認_異常系] - 2 回目の cplat_process_priority_linux_get_io_priority() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE, unchanged_class); // [確認_異常系] - unchanged_class が CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE であること。(出力引数が変更されないことの確認)
    EXPECT_EQ(-1, unchanged_level); // [確認_異常系] - unchanged_level が -1 であること。(出力引数が変更されないことの確認)
}

// CPU 使用率クランプが 2 番目の版の sched_attr でポリシーを保ったまま設定されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, set_utilization_clamp_keeps_policy_and_params)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    test_sched_attr captured;
    std::memset(&captured, 0, sizeof(captured)); // [状態] - 渡された sched_attr の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_sched_setattr, (intptr_t)kThreadId, _, 0, _, _))
        .WillOnce(Invoke(
            [&captured](const char *, const int, const char *, long, intptr_t, intptr_t attr, intptr_t, intptr_t,
                        intptr_t)
            {
                std::memcpy(&captured, reinterpret_cast<const void *>(attr), sizeof(captured));
                return 0L;
            })); // [Pre-Assert確認_正常系] - syscall() が SYS_sched_setattr、kThreadId、0 を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - syscall() から、渡された sched_attr を captured へ複写して 0 を返す。

    // Act
    int actual_ret = cplat_process_priority_linux_set_utilization_clamp(
        kThreadId, 128U, 512U,
        nullptr); // [手順] - kThreadId、下限 128、上限 512 を指定して cplat_process_priority_linux_set_utilization_clamp() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_process_priority_linux_set_utilization_clamp() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(56U, captured.size);   // [確認_正常系] - captured.size が 56 であること。(2 番目の版の sched_attr であることの確認)
    EXPECT_EQ(0x78U, captured.sched_flags); // [確認_正常系] - captured.sched_flags が 0x78 であること。(KEEP_POLICY、KEEP_PARAMS、UTIL_CLAMP_MIN、UTIL_CLAMP_MAX の論理和であることの確認)
    EXPECT_EQ(128U, captured.sched_util_min); // [確認_正常系] - captured.sched_util_min が 128 であること。
    EXPECT_EQ(512U, captured.sched_util_max); // [確認_正常系] - captured.sched_util_max が 512 であること。
}

// カーネルが CPU 使用率クランプに対応しない場合の設定が CPLAT_ERR_UNSUPPORTED になることの確認
TEST_F(processPriorityLinuxTest, set_utilization_clamp_reports_unsupported)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_error detail_old_kernel;
    cplat_error detail_disabled; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_sched_setattr, _, _, _, _, _))
        .WillOnce(DoAll(testing::Assign(&errno, E2BIG), Return(-1L)))
        .WillOnce(DoAll(testing::Assign(&errno, EOPNOTSUPP),
                        Return(-1L))); // [Pre-Assert確認_異常系] - syscall() が SYS_sched_setattr を指定して 2 回呼び出されること。
                                       // [Pre-Assert手順] - syscall() から 1 回目は errno に E2BIG、2 回目は errno に EOPNOTSUPP を設定して -1 を返す。

    // Act
    int actual_ret_old_kernel = cplat_process_priority_linux_set_utilization_clamp(
        0U, 0U, 1024U, &detail_old_kernel); // [手順] - cplat_process_priority_linux_set_utilization_clamp() を呼び出す。
    int actual_ret_disabled = cplat_process_priority_linux_set_utilization_clamp(
        0U, 0U, 1024U, &detail_disabled); // [手順] - cplat_process_priority_linux_set_utilization_clamp() をもう一度呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        actual_ret_old_kernel); // [確認_異常系] - E2BIG に対する cplat_process_priority_linux_set_utilization_clamp() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
    EXPECT_EQ(
        E2BIG,
        cplat_error_get_errno(&detail_old_kernel)); // [確認_異常系] - detail_old_kernel に対する cplat_error_get_errno() の戻り値が E2BIG であること。
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        actual_ret_disabled); // [確認_異常系] - EOPNOTSUPP に対する cplat_process_priority_linux_set_utilization_clamp() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
}

// CPU 使用率クランプの取得が、sysctl の有無と sched_attr の版で対応を判定することの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
TEST_F(processPriorityLinuxTest, get_utilization_clamp_checks_kernel_support)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    uint32_t minimum = 0U;
    uint32_t maximum = 0U; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, access(_, _, _, testing::StrEq("/proc/sys/kernel/sched_util_clamp_min"), F_OK))
        .WillOnce(Return(-1))
        .WillRepeatedly(Return(0)); // [Pre-Assert確認_正常系] - access() が "/proc/sys/kernel/sched_util_clamp_min" と等しい文字列、F_OK を指定して 1 回以上呼び出されること。
                                    // [Pre-Assert手順] - access() から 1 回目は -1、以降は 0 を返す。
    EXPECT_CALL(mock_unistd, syscall(_, _, _, (long)SYS_sched_getattr, (intptr_t)kThreadId, _, 56, 0, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, long, intptr_t, intptr_t attr, intptr_t, intptr_t, intptr_t)
            {
                reinterpret_cast<test_sched_attr *>(attr)->size = 48U;
                return 0L;
            }))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, long, intptr_t, intptr_t attr, intptr_t, intptr_t, intptr_t)
            {
                test_sched_attr *out = reinterpret_cast<test_sched_attr *>(attr);
                out->size = 56U;
                out->sched_util_min = 64U;
                out->sched_util_max = 900U;
                return 0L;
            })); // [Pre-Assert確認_正常系] - syscall() が SYS_sched_getattr、kThreadId、56、0 を指定して 2 回呼び出されること。
                 // [Pre-Assert手順] - syscall() から 1 回目は大きさ 48、2 回目は大きさ 56、下限 64、上限 900 を設定して 0 を返す。

    // Act
    int actual_ret_no_sysctl = cplat_process_priority_linux_get_utilization_clamp(
        kThreadId, &minimum, &maximum, nullptr); // [手順] - sysctl がない状態で cplat_process_priority_linux_get_utilization_clamp() を呼び出す。
    int actual_ret_old_kernel = cplat_process_priority_linux_get_utilization_clamp(
        kThreadId, &minimum, &maximum,
        nullptr); // [手順] - カーネルが最初の版の sched_attr を返す状態で cplat_process_priority_linux_get_utilization_clamp() を呼び出す。
    int actual_ret = cplat_process_priority_linux_get_utilization_clamp(
        kThreadId, &minimum, &maximum,
        nullptr); // [手順] - カーネルが 2 番目の版の sched_attr を返す状態で cplat_process_priority_linux_get_utilization_clamp() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        actual_ret_no_sysctl); // [確認_異常系] - sysctl がない状態の cplat_process_priority_linux_get_utilization_clamp() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        actual_ret_old_kernel); // [確認_異常系] - 最初の版の sched_attr に対する cplat_process_priority_linux_get_utilization_clamp() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 2 番目の版の sched_attr に対する cplat_process_priority_linux_get_utilization_clamp() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(64U, minimum);  // [確認_正常系] - minimum が 64 であること。
    EXPECT_EQ(900U, maximum); // [確認_正常系] - maximum が 900 であること。
}

// 特権のない作業スレッドで、優先度を下げる操作が成功し、上げる操作が権限不足になることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-014; uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4
// cplat-req: id=CPLAT-RUNTIME-FUNC-016; uuid=b2d649a0-78a9-4050-a075-f22bac73fb70
TEST_F(processPriorityLinuxTest, worker_thread_lowers_priority_and_cannot_raise_it)
{
    // Arrange
    worker_results results = {}; // [状態] - 作業スレッドの結果格納先を用意する。
    if (!is_unprivileged())
    {
        GTEST_SKIP() << "the test process has privileges to raise priority";
    }

    // Pre-Assert

    // Act
    std::thread worker(run_on_worker_thread,
                       &results); // [手順] - 作業スレッドで run_on_worker_thread() を呼び出す。
    worker.join();

    // Assert
    EXPECT_EQ(CPLAT_OK, results.lower_nice); // [確認_正常系] - results.lower_nice が CPLAT_OK であること。(nice 値 10 へ下げる操作が成功したことの確認)
    EXPECT_EQ(CPLAT_OK, results.get_nice);   // [確認_正常系] - results.get_nice が CPLAT_OK であること。
    EXPECT_EQ(10, results.nice_value);       // [確認_正常系] - results.nice_value が 10 であること。
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        results.raise_nice); // [確認_異常系] - results.raise_nice が CPLAT_ERR_PERMISSION_DENIED であること。(nice 値 0 へ戻す操作が権限不足になったことの確認)
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        results.real_time_policy); // [確認_異常系] - results.real_time_policy が CPLAT_ERR_PERMISSION_DENIED であること。(FIFO への変更が権限不足になったことの確認)
    EXPECT_EQ(CPLAT_OK, results.idle_io); // [確認_正常系] - results.idle_io が CPLAT_OK であること。(I/O 優先度を IDLE へ下げる操作が成功したことの確認)
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        results.real_time_io); // [確認_異常系] - results.real_time_io が CPLAT_ERR_PERMISSION_DENIED であること。(I/O 優先度の REAL_TIME が権限不足になったことの確認)
}

// 存在しないスレッド ID が CPLAT_ERR_NOT_FOUND になることの確認
TEST_F(processPriorityLinuxTest, nonexistent_thread_reports_not_found)
{
    // Arrange
    int nice_value = 0; // [状態] - 出力引数の格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_process_priority_linux_get_nice(
        0x7FFFFFFEU, &nice_value,
        nullptr); // [手順] - pid_max を超えるスレッド ID 0x7FFFFFFE を指定して cplat_process_priority_linux_get_nice() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, actual_ret); // [確認_異常系] - cplat_process_priority_linux_get_nice() の戻り値が CPLAT_ERR_NOT_FOUND であること。
}

// 実行環境のカーネルが対応しない場合に、CPU 使用率クランプの設定と取得が CPLAT_ERR_UNSUPPORTED になることの確認
TEST_F(processPriorityLinuxTest, utilization_clamp_reports_unsupported_on_kernel_without_clamp)
{
    // Arrange
    uint32_t minimum = 0U;
    uint32_t maximum = 0U; // [状態] - 出力引数の格納先を用意する。
    if (access("/proc/sys/kernel/sched_util_clamp_min", F_OK) == 0)
    {
        GTEST_SKIP() << "the kernel supports utilization clamping";
    }

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_process_priority_linux_set_utilization_clamp(
        0U, 0U, 1024U, nullptr); // [手順] - 下限 0、上限 1024 を指定して cplat_process_priority_linux_set_utilization_clamp() を呼び出す。
    int actual_ret_get = cplat_process_priority_linux_get_utilization_clamp(
        0U, &minimum, &maximum, nullptr); // [手順] - cplat_process_priority_linux_get_utilization_clamp() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        actual_ret_set); // [確認_異常系] - cplat_process_priority_linux_set_utilization_clamp() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        actual_ret_get); // [確認_異常系] - cplat_process_priority_linux_get_utilization_clamp() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
}

#endif /* PLATFORM_LINUX */
