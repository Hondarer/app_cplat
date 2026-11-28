#include <testfw.h>
#include <mock_cplat.h>
#include <mock_fcntl.h>
#include <mock_signal.h>
#include <mock_systemd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <cplat/service/service.h>
#include <cplat/service/service_internal.h>
#include <cplat/service/service_linux_events.h>
#include "service_linux_events.inject.h"

using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::HasSubstr;
using testing::InSequence;
using testing::Invoke;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;

namespace
{

int g_on_start_called = 0;
int g_on_run_called = 0;

/** コールバックの呼び出しの記録 (on_run を実行するスレッドだけが追加する)。 */
std::vector<std::string> g_run_calls;

/** on_reload の呼び出し回数 (イベント監視スレッドが加算する)。 */
std::atomic<int> g_reload_count(0);

/** on_reload の開始、解放、完了を同期する状態。 */
std::mutex g_reload_mutex;
std::condition_variable g_reload_cv;
bool g_reload_entered = false;
bool g_release_reload = false;
bool g_reload_finished = false;

/** on_event に配送されたイベントの種別。 */
std::vector<cplat_service_event_type> g_dispatched_events;

/** 各コールバックの中で実行する処理。 */
std::function<void()> g_on_start_action;
std::function<void()> g_on_run_action;
std::function<void()> g_on_stop_action;
std::function<void()> g_on_reload_action;

/** 通知ロックの待機順と、ロック外トレースの確認に使う状態。 */
std::atomic<int> g_watchdog_lock_attempted(0);
std::atomic<int> g_watchdog_finished(0);
std::atomic<int> g_track_watchdog_lock(0);
int g_watchdog_waited_for_lock = 0;
int g_stopping_received_in_on_stop = 0;
int g_reload_finished_before_on_stop_return = 0;
int g_reload_started_before_stop = 0;
int g_reload_timed_out = 0;
int g_phase_after_reload = -1;
thread_local cplat_local_lock *g_notification_lock_held = nullptr;

/** cplat_thread_detach() と cplat_close() の呼び出し回数 (異常終了のテストで使う)。 */
std::atomic<int> g_detach_calls(0);
std::atomic<int> g_close_calls(0);

extern "C"
{
    static int dummy_on_start(void *user_data)
    {
        (void)user_data;
        g_on_start_called++;
        return 0;
    }

    static int dummy_on_run(void *user_data)
    {
        (void)user_data;
        g_on_run_called++;
        return 0;
    }

    static void dummy_on_reload(void *user_data)
    {
        (void)user_data;
    }

    static int run_test_on_start(void *user_data)
    {
        (void)user_data;
        g_run_calls.push_back("on_start");
        if (g_on_start_action)
        {
            g_on_start_action();
        }
        return 0;
    }

    static int run_test_on_run(void *user_data)
    {
        (void)user_data;
        g_run_calls.push_back("on_run");
        if (g_on_run_action)
        {
            g_on_run_action();
        }
        return 0;
    }

    static int run_test_on_stop(void *user_data)
    {
        (void)user_data;
        g_run_calls.push_back("on_stop");
        if (g_on_stop_action)
        {
            g_on_stop_action();
        }
        return 0;
    }

    static void run_test_on_reload(void *user_data)
    {
        (void)user_data;
        g_reload_count++;
        if (g_on_reload_action)
        {
            g_on_reload_action();
        }
    }

    static void run_test_on_event(const cplat_service_event_info *info, void *user_data)
    {
        (void)user_data;
        g_dispatched_events.push_back(info->type);
    }
}

cplat_service_definition make_def(bool with_reload = true)
{
    cplat_service_definition def = {};
    def.name = "my-service";
    def.display_name = "My Service";
    def.description = "My Service Description";
    def.on_start = dummy_on_start;
    def.on_run = dummy_on_run;
    def.on_stop = nullptr;
    def.on_event = nullptr;
    def.on_reload = with_reload ? dummy_on_reload : nullptr;
    def.tracer = nullptr;
    return def;
}

cplat_service_definition make_run_def(bool with_reload = false, bool with_event = false)
{
    cplat_service_definition def = {};
    def.name = "service-linux-test";
    def.display_name = "Service Linux Test";
    def.description = "service_linux.c の単体テスト用定義です。";
    def.on_start = run_test_on_start;
    def.on_run = run_test_on_run;
    def.on_stop = run_test_on_stop;
    def.on_event = with_event ? run_test_on_event : nullptr;
    def.on_reload = with_reload ? run_test_on_reload : nullptr;
    def.tracer = nullptr;
    return def;
}

void sleep_ms(const int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void wait_repeatedly(const int timeout_ms, const int count)
{
    for (int i = 0; i < count; i++)
    {
        if (cplat_service_wait_for_stop(timeout_ms) != CPLAT_ERR_TIMEOUT)
        {
            break;
        }
    }
}

class NotifySocket
{
  public:
    NotifySocket()
    {
        static std::atomic<int> sequence(0);
        path_ = "/tmp/cplat-serviceLinuxTest-notify-" + std::to_string(::getpid()) + "-" + std::to_string(sequence++);
        (void)::unlink(path_.c_str());
        fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (fd_ >= 0)
        {
            struct sockaddr_un address;
            std::memset(&address, 0, sizeof(address));
            address.sun_family = AF_UNIX;
            (void)std::snprintf(address.sun_path, sizeof(address.sun_path), "%s", path_.c_str());
            if (::bind(fd_, reinterpret_cast<const struct sockaddr *>(&address), sizeof(address)) != 0)
            {
                (void)::close(fd_);
                fd_ = -1;
            }
        }
        (void)::setenv("NOTIFY_SOCKET", path_.c_str(), 1);
    }

    ~NotifySocket()
    {
        (void)::unsetenv("NOTIFY_SOCKET");
        if (fd_ >= 0)
        {
            (void)::close(fd_);
        }
        (void)::unlink(path_.c_str());
    }

    NotifySocket(const NotifySocket &) = delete;
    NotifySocket &operator=(const NotifySocket &) = delete;

    bool is_ready() const
    {
        return fd_ >= 0;
    }

    void remove_path()
    {
        (void)::unlink(path_.c_str());
    }

    bool wait_for_message(const std::string &message, const int timeout_ms)
    {
        for (int elapsed_ms = 0; elapsed_ms < timeout_ms; elapsed_ms += 5)
        {
            std::vector<std::string> messages = receive_all();
            if (std::find(messages.begin(), messages.end(), message) != messages.end())
            {
                return true;
            }
            sleep_ms(5);
        }

        std::vector<std::string> messages = receive_all();
        return std::find(messages.begin(), messages.end(), message) != messages.end();
    }

    std::vector<std::string> receive_all()
    {
        char buffer[4096];

        for (;;)
        {
            const ssize_t received = ::recv(fd_, buffer, sizeof(buffer), MSG_DONTWAIT);
            if (received < 0)
            {
                break;
            }
            messages_.push_back(std::string(buffer, static_cast<size_t>(received)));
        }

        return messages_;
    }

  private:
    std::string path_;
    int fd_ = -1;
    int padding_ = 0;
    std::vector<std::string> messages_;
};

int index_of(const std::vector<std::string> &messages, const std::string &message, const size_t from = 0U)
{
    for (size_t i = from; i < messages.size(); i++)
    {
        if (messages[i] == message)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int count_of(const std::vector<std::string> &messages, const std::string &message)
{
    return static_cast<int>(std::count(messages.begin(), messages.end(), message));
}

sd_bus *const k_dummy_bus = reinterpret_cast<sd_bus *>(static_cast<uintptr_t>(0x10));
sd_bus_message *const k_dummy_message = reinterpret_cast<sd_bus_message *>(static_cast<uintptr_t>(0x20));
sd_bus_message *const k_dummy_reply = reinterpret_cast<sd_bus_message *>(static_cast<uintptr_t>(0x30));
sd_bus_slot *const k_dummy_slot = reinterpret_cast<sd_bus_slot *>(static_cast<uintptr_t>(0x40));
constexpr uint64_t k_inhibit_timeout_usec = 10000ULL * 1000ULL;

void prevent_real_dbus(Mock_systemd &mock)
{
    ON_CALL(mock, sd_bus_message_new_method_call(_, _, _, _, _, _, _, _, _)).WillByDefault(Return(-EIO));
    ON_CALL(mock, sd_bus_message_append_basic(_, _, _, _, _, _)).WillByDefault(Return(-EIO));
    ON_CALL(mock, sd_bus_message_read_basic(_, _, _, _, _, _)).WillByDefault(Return(-EIO));
    ON_CALL(mock, sd_bus_message_unref(_, _, _, _)).WillByDefault(Return(nullptr));
    ON_CALL(mock, sd_bus_call(_, _, _, _, _, _, _, _)).WillByDefault(Return(-EIO));
    ON_CALL(mock, sd_bus_call_async(_, _, _, _, _, _, _, _, _)).WillByDefault(Return(-EIO));
    ON_CALL(mock, sd_bus_slot_unref(_, _, _, _)).WillByDefault(Return(nullptr));
    ON_CALL(mock, sd_bus_error_free(_, _, _, _)).WillByDefault(Return());
    ON_CALL(mock, sd_bus_message_is_method_error(_, _, _, _, _)).WillByDefault(Return(0));
    ON_CALL(mock, sd_bus_message_get_error(_, _, _, _)).WillByDefault(Return(nullptr));
}

void expect_inhibit_message_built(Mock_systemd &mock)
{
    EXPECT_CALL(mock, sd_bus_message_new_method_call(_, _, _, k_dummy_bus, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(k_dummy_message), Return(0)));
    EXPECT_CALL(mock, sd_bus_message_append_basic(_, _, _, k_dummy_message, 's', _)).Times(4).WillRepeatedly(Return(0));
}

int read_basic_writes_fd(sd_bus_message *, char type, void *out, int fd_value, int result)
{
    EXPECT_EQ('h', type);
    if (result > 0)
    {
        *static_cast<int *>(out) = fd_value;
    }
    return result;
}

class serviceLinuxTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        g_on_start_called = 0;
        g_on_run_called = 0;
        test_svc_linux_events_reset_inhibit();
        test_svc_linux_events_set_definition(nullptr);
    }

    void TearDown() override
    {
        cplat_internal_service_stop_dispose();
        test_svc_linux_events_reset_inhibit();
        test_svc_linux_events_set_definition(nullptr);
    }
};

class serviceLinuxRunTest : public ::testing::Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
    NiceMock<Mock_systemd> mock_systemd_;
    sigset_t saved_signal_mask_{};

    void install_real_sync_defaults()
    {
        ON_CALL(mock_cplat_, cplat_local_lock_create(_))
            .WillByDefault(Invoke([](cplat_local_lock **lock) { return delegate_real_cplat_local_lock_create(lock); }));
        ON_CALL(mock_cplat_, cplat_local_lock_lock(_))
            .WillByDefault(Invoke(
                [](cplat_local_lock *lock)
                {
                    if ((g_track_watchdog_lock.load() != 0) && (lock == test_svc_linux_events_notify_lock()))
                    {
                        g_watchdog_lock_attempted = 1;
                    }
                    delegate_real_cplat_local_lock_lock(lock);
                    if (lock == test_svc_linux_events_notify_lock())
                    {
                        g_notification_lock_held = lock;
                    }
                }));
        ON_CALL(mock_cplat_, cplat_local_lock_timed_lock(_, _))
            .WillByDefault(Invoke([](cplat_local_lock *lock, int timeout_ms)
                                  { return delegate_real_cplat_local_lock_timed_lock(lock, timeout_ms); }));
        ON_CALL(mock_cplat_, cplat_local_lock_try_lock(_))
            .WillByDefault(
                Invoke([](cplat_local_lock *lock) { return delegate_real_cplat_local_lock_try_lock(lock); }));
        ON_CALL(mock_cplat_, cplat_local_lock_unlock(_))
            .WillByDefault(Invoke(
                [](cplat_local_lock *lock)
                {
                    delegate_real_cplat_local_lock_unlock(lock);
                    if (lock == test_svc_linux_events_notify_lock())
                    {
                        g_notification_lock_held = nullptr;
                    }
                }));
        ON_CALL(mock_cplat_, cplat_local_lock_dispose(_))
            .WillByDefault(Invoke([](cplat_local_lock *lock) { delegate_real_cplat_local_lock_dispose(lock); }));
        ON_CALL(mock_cplat_, cplat_condvar_create(_))
            .WillByDefault(Invoke([](cplat_condvar **cv) { return delegate_real_cplat_condvar_create(cv); }));
        ON_CALL(mock_cplat_, cplat_condvar_wait(_, _))
            .WillByDefault(
                Invoke([](cplat_condvar *cv, cplat_local_lock *lock) { delegate_real_cplat_condvar_wait(cv, lock); }));
        ON_CALL(mock_cplat_, cplat_condvar_timed_wait(_, _, _))
            .WillByDefault(Invoke([](cplat_condvar *cv, cplat_local_lock *lock, int timeout_ms)
                                  { return delegate_real_cplat_condvar_timed_wait(cv, lock, timeout_ms); }));
        ON_CALL(mock_cplat_, cplat_condvar_signal(_))
            .WillByDefault(Invoke([](cplat_condvar *cv) { delegate_real_cplat_condvar_signal(cv); }));
        ON_CALL(mock_cplat_, cplat_condvar_broadcast(_))
            .WillByDefault(Invoke([](cplat_condvar *cv) { delegate_real_cplat_condvar_broadcast(cv); }));
        ON_CALL(mock_cplat_, cplat_condvar_dispose(_))
            .WillByDefault(Invoke([](cplat_condvar *cv) { delegate_real_cplat_condvar_dispose(cv); }));
    }

    // [サブ手順 名前=serviceLinuxRunTest.SetUp]
    void SetUp() override
    {
        g_run_calls.clear();
        g_reload_count = 0;
        {
            std::lock_guard<std::mutex> lock(g_reload_mutex);
            g_reload_entered = false;
            g_release_reload = false;
            g_reload_finished = false;
        }
        g_dispatched_events.clear();
        g_on_start_action = nullptr;
        g_on_run_action = nullptr;
        g_on_stop_action = nullptr;
        g_on_reload_action = nullptr;
        g_watchdog_lock_attempted = 0;
        g_watchdog_finished = 0;
        g_track_watchdog_lock = 0;
        g_watchdog_waited_for_lock = 0;
        g_stopping_received_in_on_stop = 0;
        g_reload_finished_before_on_stop_return = 0;
        g_reload_started_before_stop = 0;
        g_reload_timed_out = 0;
        g_phase_after_reload = -1;
        g_notification_lock_held = nullptr;
        g_detach_calls = 0;
        g_close_calls = 0;
        (void)::unsetenv("WATCHDOG_USEC"); // [状態] - watchdog を要求しない状態とする。
        (void)::unsetenv("WATCHDOG_PID");  // [状態] - watchdog の対象のプロセスを限定しない状態とする。
        install_real_sync_defaults();
        ASSERT_EQ(CPLAT_OK, cplat_internal_service_stop_init()); // [状態確認] - 停止抽象を初期化できること。
        ASSERT_EQ(
            0, ::pthread_sigmask(SIG_SETMASK, nullptr,
                                 &saved_signal_mask_)); // [状態確認] - テスト開始前のシグナル マスクを保存できること。
        ASSERT_EQ(CPLAT_OK, cplat_service_prepare_run()); // [状態確認] - SIGHUP をブロックできること。
    }
    // [サブ手順終了]

    void TearDown() override
    {
        sigset_t drain_set;
        struct timespec zero_timeout = {};

        (void)::sigemptyset(&drain_set);
        (void)::sigaddset(&drain_set, SIGHUP);
        (void)::sigtimedwait(&drain_set, nullptr, &zero_timeout);
        EXPECT_EQ(0, ::pthread_sigmask(SIG_SETMASK, &saved_signal_mask_, nullptr));
        cplat_internal_service_stop_dispose();
        test_svc_linux_events_reset_inhibit();
        test_svc_linux_events_set_definition(nullptr);
        (void)::unsetenv("WATCHDOG_USEC");
        g_on_start_action = nullptr;
        g_on_run_action = nullptr;
        g_on_stop_action = nullptr;
        g_on_reload_action = nullptr;
    }

    void allow_any_trace()
    {
        EXPECT_CALL(mock_cplat_, cplat_tracer_write_with_source(_, _, _, _, _, _)).Times(AnyNumber());
        EXPECT_CALL(mock_cplat_, cplat_tracer_writef_at(_, _, _, _)).Times(AnyNumber());
    }

    void expect_start_failure_trace()
    {
        EXPECT_CALL(mock_cplat_, cplat_tracer_write_with_source(_, CPLAT_TRACE_LEVEL_ERROR, _, _, _,
                                                                HasSubstr("サービスを開始しません")))
            .Times(1);
    }
};

} // namespace

// cplat_service_prepare_run の成功・失敗確認
TEST_F(serviceLinuxTest, prepare_run_blocks_sighup)
{
    // Arrange
    NiceMock<Mock_signal> mock_signal;

    // Pre-Assert
    EXPECT_CALL(mock_signal, pthread_sigmask(_, _, _, SIG_BLOCK, _, nullptr))
        .WillOnce(Return(0))
        .WillOnce(Return(
            EINVAL)); // [Pre-Assert確認_正常系] - pthread_sigmask() が SIG_BLOCK を指定して 2 回呼び出されること。
                      // [Pre-Assert手順] - pthread_sigmask() から 1 回目は 0、2 回目は EINVAL を返す。

    // Act
    int actual_ok = cplat_service_prepare_run();  // [手順] - 1 回目の cplat_service_prepare_run() を呼び出す。
    int actual_err = cplat_service_prepare_run(); // [手順] - 2 回目の cplat_service_prepare_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ok); // [確認_正常系] - 1 回目の cplat_service_prepare_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_err); // [確認_異常系] - 2 回目の cplat_service_prepare_run() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 非 root 時の install / uninstall が自動昇格せず CPLAT_ERR_PERMISSION_DENIED を返すことの確認
TEST_F(serviceLinuxTest, install_and_uninstall_require_root_without_elevation)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillRepeatedly(DoAll(
            SetArgPointee<0>(0),
            Return(CPLAT_OK))); // [Pre-Assert確認_異常系] - cplat_elevated_process_is_elevated() が呼び出されること。
    // [Pre-Assert手順] - cplat_elevated_process_is_elevated() から非 root (0) を設定して CPLAT_OK を返す。

    // Act
    int actual_install =
        cplat_service_install(&def, nullptr);             // [手順] - 非 root で cplat_service_install() を呼び出す。
    int actual_uninstall = cplat_service_uninstall(&def); // [手順] - 非 root で cplat_service_uninstall() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_install); // [確認_異常系] - cplat_service_install() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_uninstall); // [確認_異常系] - cplat_service_uninstall() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
}

// StartLimit 到達可能性検査 (到達不能およびオーバーフロー) で install が拒否されることの確認
TEST_F(serviceLinuxTest, install_rejects_unreachable_start_limit)
{
    // Arrange
    cplat_service_definition def = make_def();
    cplat_service_install_options unreachable = {};
    unreachable.restart_delay_sec = 5U;
    unreachable.start_limit_burst = 5U;
    unreachable.start_limit_interval_sec = 20U; // (5-1)*5 == 20 >= 20 -> 到達不能

    cplat_service_install_options overflow = {};
    overflow.restart_delay_sec = 0xFFFFFFFFU;
    overflow.start_limit_burst = 0xFFFFFFFFU;
    overflow.start_limit_interval_sec = 100U;

    // Pre-Assert

    // Act
    int actual_unreachable =
        cplat_service_install(&def, &unreachable); // [手順] - 到達不能な設定で cplat_service_install() を呼び出す。
    int actual_overflow =
        cplat_service_install(&def, &overflow); // [手順] - オーバーフロー設定で cplat_service_install() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_unreachable); // [確認_異常系] - 到達不能時の cplat_service_install() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_overflow); // [確認_異常系] - オーバーフロー時の cplat_service_install() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 既定オプションおよび systemd 248 での unit 生成内容の確認
TEST_F(serviceLinuxTest, install_generates_default_unit_with_systemd_248)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def(true);
    std::string written_unit;
    FILE *fake_unit_fp = tmpfile();
    ASSERT_NE(nullptr, fake_unit_fp); // [状態確認] - 一時ファイルを開けること。

    char tmp_dir[] = "/tmp/cplat_svc_test_XXXXXX";
    ASSERT_NE(nullptr, mkdtemp(tmp_dir)); // [状態確認] - 一時ディレクトリを作成できること。
    std::string fake_systemctl = std::string(tmp_dir) + "/systemctl";
    {
        FILE *sfp = std::fopen(fake_systemctl.c_str(), "w");
        ASSERT_NE(nullptr, sfp);
        std::fputs("#!/bin/sh\necho 'systemd 248 (248-1)'\n", sfp);
        std::fclose(sfp);
        ASSERT_EQ(0, chmod(fake_systemctl.c_str(), 0755));
    }
    const char *orig_path = std::getenv("PATH");
    std::string saved_path = (orig_path != nullptr) ? orig_path : "";
    std::string new_path = std::string(tmp_dir) + ":" + saved_path;
    ASSERT_EQ(0, setenv("PATH", new_path.c_str(), 1));

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillOnce(DoAll(SetArgPointee<0>(1), Return(CPLAT_OK)));
    EXPECT_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillOnce(Invoke(
            [](char *buf, size_t size)
            {
                std::snprintf(buf, size, "/usr/local/bin/my-service");
                return CPLAT_OK;
            }));
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("/etc/systemd/system/my-service.service"), StrEq("w"), _))
        .WillOnce(Return(fake_unit_fp));
    EXPECT_CALL(mock_cplat, cplat_fclose(fake_unit_fp, _))
        .WillOnce(Invoke(
            [&written_unit](FILE *fp, cplat_error *)
            {
                std::fflush(fp);
                std::rewind(fp);
                char buf[4096];
                size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
                buf[n] = '\0';
                written_unit = buf;
                std::fclose(fp);
                return 0;
            }));
    EXPECT_CALL(mock_cplat, cplat_process_run_sync(_, _))
        .Times(2)
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_process_run_sync() が 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_process_run_sync() から終了コード 0 を設定して CPLAT_OK を返す。

    // Act
    int actual_ret =
        cplat_service_install(&def, nullptr); // [手順] - 既定オプションで cplat_service_install() を呼び出す。
    setenv("PATH", saved_path.c_str(), 1);
    std::remove(fake_systemctl.c_str());
    rmdir(tmp_dir);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_install() の戻り値が CPLAT_OK であること。
    EXPECT_THAT(
        written_unit,
        HasSubstr(
            "ExecStart=/usr/local/bin/my-service run\n")); // [確認_正常系] - written_unit に既定の ExecStart 行が含まれること。
    EXPECT_THAT(
        written_unit,
        HasSubstr(
            "StartLimitIntervalSec=60\n")); // [確認_正常系] - written_unit に StartLimitIntervalSec=60 が含まれること。
    EXPECT_THAT(written_unit,
                HasSubstr("StartLimitBurst=5\n")); // [確認_正常系] - written_unit に StartLimitBurst=5 が含まれること。
    EXPECT_THAT(
        written_unit,
        HasSubstr("Restart=on-failure\n")); // [確認_正常系] - written_unit に Restart=on-failure が含まれること。
    EXPECT_THAT(written_unit,
                HasSubstr("RestartSec=5\n")); // [確認_正常系] - written_unit に RestartSec=5 が含まれること。
    EXPECT_THAT(written_unit,
                HasSubstr("WatchdogSec=30\n")); // [確認_正常系] - written_unit に WatchdogSec=30 が含まれること。
    EXPECT_THAT(
        written_unit,
        HasSubstr("OOMScoreAdjust=-1000\n")); // [確認_正常系] - written_unit に OOMScoreAdjust=-1000 が含まれること。
    EXPECT_THAT(
        written_unit,
        HasSubstr(
            "ManagedOOMPreference=omit\n")); // [確認_正常系] - written_unit に ManagedOOMPreference=omit が含まれること。
    EXPECT_THAT(
        written_unit,
        HasSubstr(
            "ExecReload=/bin/kill -HUP $MAINPID\n")); // [確認_正常系] - written_unit に ExecReload 行が含まれること。
}

// disable_* オプションおよび空 run_args での unit 生成内容の確認
TEST_F(serviceLinuxTest, install_honors_disable_options_and_empty_run_args)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def(false);
    cplat_service_install_options opts = {};
    opts.run_args = "";
    opts.disable_auto_restart = 1;
    opts.disable_watchdog = 1;
    opts.disable_oom_protection = 1;

    std::string written_unit;
    FILE *fake_unit_fp = tmpfile();
    ASSERT_NE(nullptr, fake_unit_fp); // [状態確認] - 一時ファイルを開けること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillOnce(DoAll(SetArgPointee<0>(1), Return(CPLAT_OK)));
    EXPECT_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillOnce(Invoke(
            [](char *buf, size_t size)
            {
                std::snprintf(buf, size, "/usr/local/bin/my-service");
                return CPLAT_OK;
            }));
    EXPECT_CALL(mock_cplat, cplat_fopen(_, _, _)).WillOnce(Return(fake_unit_fp));
    EXPECT_CALL(mock_cplat, cplat_fclose(fake_unit_fp, _))
        .WillOnce(Invoke(
            [&written_unit](FILE *fp, cplat_error *)
            {
                std::fflush(fp);
                std::rewind(fp);
                char buf[4096];
                size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
                buf[n] = '\0';
                written_unit = buf;
                std::fclose(fp);
                return 0;
            }));
    EXPECT_CALL(mock_cplat, cplat_process_run_sync(_, _))
        .Times(2)
        .WillRepeatedly(DoAll(SetArgPointee<1>(0), Return(CPLAT_OK)));

    // Act
    int actual_ret = cplat_service_install(
        &def, &opts); // [手順] - 全 disable オプションと空 run_args で cplat_service_install() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_install() の戻り値が CPLAT_OK であること。
    EXPECT_THAT(
        written_unit,
        HasSubstr("ExecStart=/usr/local/bin/my-service\n"));   // [確認_正常系] - 引数なし ExecStart が出力されること。
    EXPECT_THAT(written_unit, Not(HasSubstr("Restart=")));     // [確認_正常系] - Restart= が出力されないこと。
    EXPECT_THAT(written_unit, Not(HasSubstr("StartLimit")));   // [確認_正常系] - StartLimit が出力されないこと。
    EXPECT_THAT(written_unit, Not(HasSubstr("WatchdogSec="))); // [確認_正常系] - WatchdogSec= が出力されないこと。
    EXPECT_THAT(written_unit,
                Not(HasSubstr("OOMScoreAdjust=")));           // [確認_正常系] - OOMScoreAdjust= が出力されないこと。
    EXPECT_THAT(written_unit, Not(HasSubstr("ExecReload="))); // [確認_正常系] - ExecReload= が出力されないこと。
}

// ユニット ファイルを閉じる処理が失敗した場合に ERROR トレースを出して失敗し systemctl を実行しないことの確認
TEST_F(serviceLinuxTest, install_fails_when_unit_file_close_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def(false);
    FILE *fake_unit_fp = tmpfile();
    ASSERT_NE(nullptr, fake_unit_fp); // [状態確認] - 一時ファイルを開けること。
    ON_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillByDefault(DoAll(SetArgPointee<0>(1), Return(CPLAT_OK)));
    ON_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillByDefault(Invoke(
            [](char *buf, size_t size)
            {
                std::snprintf(buf, size, "/usr/local/bin/my-service");
                return CPLAT_OK;
            }));

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("/etc/systemd/system/my-service.service"), StrEq("w"), _))
        .WillOnce(Return(
            fake_unit_fp)); // [Pre-Assert確認_異常系] - cplat_fopen() がユニット ファイル パスと "w" を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - cplat_fopen() から一時ファイルを返す。
    EXPECT_CALL(mock_cplat, cplat_fclose(fake_unit_fp, _))
        .WillOnce(Invoke(
            [](FILE *fp, cplat_error *err) -> int
            {
                (void)std::fclose(fp);
                cplat_error_capture_errno(err, EIO);
                return EOF;
            })); // [Pre-Assert確認_異常系] - cplat_fclose() が一時ファイルを指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 一時ファイルを閉じ EIO を記録して EOF を返す。
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(_, _, _, _)).Times(AnyNumber()).WillRepeatedly(Return(CPLAT_OK));
    EXPECT_CALL(mock_cplat,
                cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_ERROR, _, HasSubstr("への書き込みに失敗しました")))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - 書き込み失敗の ERROR トレースが 1 回出力されること。
                                     // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_process_run_sync(_, _))
        .Times(
            0); // [Pre-Assert確認_異常系] - cplat_process_run_sync() が呼び出されないこと。(systemctl daemon-reload を実行しないことの確認)

    // Act
    int actual_ret = cplat_service_install(&def, nullptr); // [手順] - cplat_service_install() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_service_install() の戻り値が CPLAT_OK 以外であること。
}

// [Unit] に起動回数の制限を書き [Service] には書かないことの確認
TEST_F(serviceLinuxTest, install_writes_start_limit_in_unit_section)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def(false);
    std::string written_unit;
    FILE *fake_unit_fp = tmpfile();
    ASSERT_NE(nullptr, fake_unit_fp); // [状態確認] - 一時ファイルを開けること。
    ON_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillByDefault(DoAll(SetArgPointee<0>(1), Return(CPLAT_OK)));
    ON_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillByDefault(Invoke(
            [](char *buf, size_t size)
            {
                std::snprintf(buf, size, "/usr/local/bin/my-service");
                return CPLAT_OK;
            }));
    ON_CALL(mock_cplat, cplat_fopen(_, _, _)).WillByDefault(Return(fake_unit_fp));
    ON_CALL(mock_cplat, cplat_fclose(fake_unit_fp, _))
        .WillByDefault(Invoke(
            [&written_unit](FILE *fp, cplat_error *)
            {
                std::fflush(fp);
                std::rewind(fp);
                char buf[4096];
                size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
                buf[n] = '\0';
                written_unit = buf;
                std::fclose(fp);
                return 0;
            }));
    ON_CALL(mock_cplat, cplat_process_run_sync(_, _)).WillByDefault(DoAll(SetArgPointee<1>(0), Return(CPLAT_OK)));

    // Pre-Assert

    // Act
    int actual_ret = cplat_service_install(
        &def, nullptr); // [手順] - on_reload が nullptr の定義で cplat_service_install() を呼び出す。

    // Assert
    const size_t unit_heading = written_unit.find("[Unit]\n");
    const size_t service_heading = written_unit.find("[Service]\n");
    const size_t interval_pos = written_unit.find("StartLimitIntervalSec=60\n");
    const size_t burst_pos = written_unit.find("StartLimitBurst=5\n");
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_install() の戻り値が CPLAT_OK であること。
    ASSERT_NE(std::string::npos, unit_heading);    // [確認_正常系] - written_unit に [Unit] があること。
    ASSERT_NE(std::string::npos, service_heading); // [確認_正常系] - written_unit に [Service] があること。
    EXPECT_LT(unit_heading, service_heading);      // [確認_正常系] - [Unit] が [Service] より前にあること。
    EXPECT_GT(interval_pos, unit_heading); // [確認_正常系] - StartLimitIntervalSec=60 が [Unit] より後にあること。
    EXPECT_LT(interval_pos,
              service_heading);            // [確認_正常系] - StartLimitIntervalSec=60 が [Service] より前にあること。
    EXPECT_GT(burst_pos, unit_heading);    // [確認_正常系] - StartLimitBurst=5 が [Unit] より後にあること。
    EXPECT_LT(burst_pos, service_heading); // [確認_正常系] - StartLimitBurst=5 が [Service] より前にあること。
    const std::string service_section = written_unit.substr(service_heading);
    EXPECT_EQ(std::string::npos,
              service_section.find(
                  "StartLimitIntervalSec=")); // [確認_正常系] - [Service] に StartLimitIntervalSec= がないこと。
    EXPECT_EQ(std::string::npos,
              service_section.find("StartLimitBurst=")); // [確認_正常系] - [Service] に StartLimitBurst= がないこと。
    EXPECT_EQ(std::string::npos,
              written_unit.find("ExecReload=")); // [確認_正常系] - on_reload が nullptr のため ExecReload= がないこと。
}

// root 権限での uninstall 成功 (unit ファイル ENOENT 許容) の確認
TEST_F(serviceLinuxTest, uninstall_succeeds_even_if_unit_file_missing)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_elevated_process_is_elevated(_))
        .WillOnce(DoAll(SetArgPointee<0>(1), Return(CPLAT_OK)));
    EXPECT_CALL(mock_cplat, cplat_process_run_sync(_, _))
        .Times(3)
        .WillRepeatedly(DoAll(
            SetArgPointee<1>(0),
            Return(
                CPLAT_OK))); // [Pre-Assert確認_正常系] - stop、disable、daemon-reload の 3 回 cplat_process_run_sync() が呼び出されること。
    // [Pre-Assert手順] - cplat_process_run_sync() から終了コード 0 を設定して CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("/etc/systemd/system/my-service.service"), _))
        .WillOnce(Invoke(
            [](const char *, cplat_error *err)
            {
                cplat_error_capture_errno(err, ENOENT);
                return -1;
            }));

    // Act
    int actual_ret =
        cplat_service_uninstall(&def); // [手順] - unit が存在しない状態で cplat_service_uninstall() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_uninstall() の戻り値が CPLAT_OK であること。
}

// イベント監視スレッドの必須初期化失敗時に on_start を呼ばず CPLAT_ERR_UNKNOWN を返すことの確認
TEST_F(serviceLinuxTest, run_fails_without_calling_on_start_when_sd_event_new_fails)
{
    // Arrange
    NiceMock<Mock_systemd> mock_systemd;
    cplat_service_definition def = make_def();
    int exit_code = 99;

    // Pre-Assert
    EXPECT_CALL(mock_systemd, sd_event_new(_, _, _, _))
        .WillOnce(Return(-ENOMEM)); // [Pre-Assert確認_異常系] - sd_event_new() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - sd_event_new() から -ENOMEM を返す。

    // Act
    int actual_ret =
        cplat_service_run(&def, &exit_code); // [手順] - sd_event_new が失敗する状態で cplat_service_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret);           // [確認_異常系] - cplat_service_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, g_on_start_called); // [確認_異常系] - g_on_start_called が 0 であること。
    EXPECT_EQ(0, g_on_run_called);   // [確認_異常系] - g_on_run_called が 0 であること。
}

// WATCHDOG_USEC < 30 [s] のときイベント監視スレッド終了待ち上限が動的調整されることの確認
TEST_F(serviceLinuxTest, watchdog_adjusts_join_timeout_dynamically)
{
    // Arrange
    NiceMock<Mock_systemd> mock_systemd;
    cplat_service_definition def = make_def();
    ASSERT_EQ(CPLAT_OK, cplat_service_prepare_run());
    ASSERT_EQ(CPLAT_OK, cplat_internal_service_stop_init());

    // Pre-Assert
    EXPECT_CALL(mock_systemd, sd_watchdog_enabled(_, _, _, 0, _))
        .WillOnce(DoAll(SetArgPointee<4>(10000000ULL), Return(1)))
        .WillOnce(DoAll(SetArgPointee<4>(500000ULL),
                        Return(1))); // [Pre-Assert確認_正常系] - sd_watchdog_enabled() が 2 回呼び出されること。
                                     // [Pre-Assert手順] - 1 回目は 10 [s]、2 回目は 0.5 [s] を設定して 1 を返す。

    // Act
    int actual_start1 = cplat_internal_service_linux_events_start(&def); // [手順] - 10 [s] watchdog で起動する。
    int actual_timeout1 = test_svc_linux_events_get_join_timeout_ms();
    int actual_stop1 = cplat_internal_service_linux_events_stop(); // [手順] - 停止する。

    int actual_start2 = cplat_internal_service_linux_events_start(&def); // [手順] - 0.5 [s] watchdog で起動する。
    int actual_timeout2 = test_svc_linux_events_get_join_timeout_ms();
    int actual_stop2 = cplat_internal_service_linux_events_stop(); // [手順] - 停止する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_start1); // [確認_正常系] - 1 回目の起動が CPLAT_OK であること。
    EXPECT_EQ(10000, actual_timeout1);  // [確認_正常系] - actual_timeout1 が 10000 であること。
    EXPECT_EQ(CPLAT_OK, actual_stop1);  // [確認_正常系] - 1 回目の停止が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_start2); // [確認_正常系] - 2 回目の起動が CPLAT_OK であること。
    EXPECT_EQ(1000, actual_timeout2);   // [確認_正常系] - actual_timeout2 が 1000 であること。
    EXPECT_EQ(CPLAT_OK, actual_stop2);  // [確認_正常系] - 2 回目の停止が CPLAT_OK であること。
}

// 停止中遷移後の READY=1 / RELOADING=1 抑止と STATUS= 送信の確認
TEST_F(serviceLinuxTest, notify_suppresses_after_stopping_and_sends_status)
{
    // Arrange
    NiceMock<Mock_systemd> mock_systemd;
    test_svc_linux_events_set_phase(0);

    // Pre-Assert
    EXPECT_CALL(mock_systemd, sd_notify(_, _, _, 0, StrEq("READY=1"))).Times(1).WillOnce(Return(1));
    EXPECT_CALL(mock_systemd, sd_notify(_, _, _, 0, StrEq("STOPPING=1"))).Times(1).WillOnce(Return(1));
    EXPECT_CALL(mock_systemd, sd_notify(_, _, _, 0, StrEq("RELOADING=1"))).Times(0);
    EXPECT_CALL(mock_systemd, sd_notify(_, _, _, 0, StrEq("STATUS=ok"))).Times(1).WillOnce(Return(1));

    // Act
    cplat_internal_service_os_notify_ready();     // [手順] - 起動中から動作中へ遷移する。
    cplat_internal_service_os_notify_stopping();  // [手順] - 動作中から停止中へ遷移する。
    cplat_internal_service_os_notify_ready();     // [手順] - 停止中に READY=1 を試みる。
    cplat_internal_service_os_notify_reloading(); // [手順] - 停止中に RELOADING=1 を試みる。
    cplat_service_set_status_text("ok");          // [手順] - STATUS=ok を送信する。

    // Assert
    EXPECT_EQ(2, test_svc_linux_events_get_phase()); // [確認_正常系] - 段階が停止中 (2) のままであること。
}

/* ============================================================
 *  cplat_internal_service_os_run とイベント監視スレッドのテスト
 * ============================================================ */

// イベント監視スレッドを生成できない場合に on_start を呼ばず CPLAT_ERR_UNKNOWN を返すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_fails_when_events_thread_cannot_start)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。

    // Pre-Assert
    allow_any_trace();
    expect_start_failure_trace();
    EXPECT_CALL(mock_cplat_, cplat_thread_create(_, _, _))
        .WillOnce(
            Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_thread_create() が 1 回呼び出されること。
                                        // [Pre-Assert手順] - cplat_thread_create() から CPLAT_ERR_UNKNOWN を返す。

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_TRUE(g_run_calls.empty());             // [確認_異常系] - g_run_calls.empty() の戻り値が true であること。
    EXPECT_EQ(-1, index_of(messages, "READY=1")); // [確認_異常系] - READY=1 が届かないこと。
}

// WATCHDOG_USEC が不正な場合に on_start を呼ばず CPLAT_ERR_UNKNOWN を返すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_fails_when_watchdog_setting_is_invalid)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    ASSERT_TRUE(notify.is_ready());            // [状態確認] - 通知を受け取るソケットを準備できること。
    (void)::setenv("WATCHDOG_USEC", "abc", 1); // [状態] - 数値として解釈できない WATCHDOG_USEC を設定する。

    // Pre-Assert
    allow_any_trace();
    expect_start_failure_trace();

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_TRUE(g_run_calls.empty());             // [確認_異常系] - g_run_calls.empty() の戻り値が true であること。
    EXPECT_EQ(-1, index_of(messages, "READY=1")); // [確認_異常系] - READY=1 が届かないこと。
}

// 停止監視を登録できない場合に on_start を呼ばず CPLAT_ERR_UNKNOWN を返すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_fails_when_stop_watch_cannot_be_registered)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。

    // Pre-Assert
    allow_any_trace();
    expect_start_failure_trace();
    EXPECT_CALL(mock_systemd_, sd_event_add_io(_, _, _, _, _, _, _, _, _))
        .WillOnce(Return(-ENOMEM)); // [Pre-Assert確認_異常系] - sd_event_add_io() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - sd_event_add_io() から -ENOMEM を返す。

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_TRUE(g_run_calls.empty());             // [確認_異常系] - g_run_calls.empty() の戻り値が true であること。
    EXPECT_EQ(-1, index_of(messages, "READY=1")); // [確認_異常系] - READY=1 が届かないこと。
}

// watchdog のタイマーを登録できない場合に on_start を呼ばず CPLAT_ERR_UNKNOWN を返すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_fails_when_watchdog_timer_cannot_be_registered)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    ASSERT_TRUE(notify.is_ready());                // [状態確認] - 通知を受け取るソケットを準備できること。
    (void)::setenv("WATCHDOG_USEC", "2000000", 1); // [状態] - watchdog を要求する状態とする。

    // Pre-Assert
    allow_any_trace();
    expect_start_failure_trace();
    EXPECT_CALL(mock_systemd_, sd_event_add_time(_, _, _, _, _, _, _, _, _, _))
        .WillOnce(Return(-ENOMEM)); // [Pre-Assert確認_異常系] - sd_event_add_time() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - sd_event_add_time() から -ENOMEM を返す。

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_TRUE(g_run_calls.empty());             // [確認_異常系] - g_run_calls.empty() の戻り値が true であること。
    EXPECT_EQ(-1, index_of(messages, "READY=1")); // [確認_異常系] - READY=1 が届かないこと。
}

// 実行中にイベント ループが異常終了した場合に停止を要求し on_stop の後に CPLAT_ERR_UNKNOWN を返すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_fails_when_event_loop_ends_abnormally)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    ASSERT_TRUE(notify.is_ready());                        // [状態確認] - 通知を受け取るソケットを準備できること。
    g_on_run_action = []() { wait_repeatedly(100, 100); }; // [状態] - on_run は停止要求が届くまで待つ。

    // Pre-Assert
    allow_any_trace();
    EXPECT_CALL(mock_cplat_, cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_ERROR, _,
                                                    HasSubstr("イベント ループがエラーで終了したため")))
        .Times(1); // [Pre-Assert確認_異常系] - イベント ループの異常終了を示す ERROR トレースが 1 回出力されること。
    EXPECT_CALL(mock_systemd_, sd_event_loop(_, _, _, _))
        .WillOnce(Return(-EIO)); // [Pre-Assert確認_異常系] - sd_event_loop() が 1 回呼び出されること。
                                 // [Pre-Assert手順] - sd_event_loop() から -EIO を返す。

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    ASSERT_EQ(3U, g_run_calls.size());    // [確認_異常系] - g_run_calls.size() の戻り値が 3U であること。
    EXPECT_EQ("on_run", g_run_calls[1]);  // [確認_異常系] - g_run_calls[1] が "on_run" であること。
    EXPECT_EQ("on_stop", g_run_calls[2]); // [確認_異常系] - g_run_calls[2] が "on_stop" であること。
}

// watchdog を要求しない場合に READY=1 と STOPPING=1 を順に送り WATCHDOG=1 を送らないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_notifies_ready_and_stopping_in_order)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_close(_, _))
        .Times(1); // [Pre-Assert確認_正常系] - cplat_close() が eventfd の 1 回だけ呼び出されること。

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code);         // [確認_正常系] - exit_code が 0 であること。
    ASSERT_EQ(3U, g_run_calls.size());     // [確認_正常系] - g_run_calls.size() の戻り値が 3U であること。
    EXPECT_EQ("on_start", g_run_calls[0]); // [確認_正常系] - g_run_calls[0] が "on_start" であること。
    EXPECT_EQ("on_run", g_run_calls[1]);   // [確認_正常系] - g_run_calls[1] が "on_run" であること。
    EXPECT_EQ("on_stop", g_run_calls[2]);  // [確認_正常系] - g_run_calls[2] が "on_stop" であること。
    int ready_index = index_of(messages, "READY=1");
    int stopping_index = index_of(messages, "STOPPING=1");
    EXPECT_NE(-1, ready_index);                     // [確認_正常系] - READY=1 が届くこと。
    EXPECT_LT(ready_index, stopping_index);         // [確認_正常系] - STOPPING=1 が READY=1 の後に届くこと。
    EXPECT_EQ(0, count_of(messages, "WATCHDOG=1")); // [確認_正常系] - WATCHDOG=1 が届かないこと。
}

// on_run が cplat_service_wait_for_stop() を繰り返している間は WATCHDOG=1 を送ることの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_sends_watchdog_while_on_run_waits)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready());                // [状態確認] - 通知を受け取るソケットを準備できること。
    (void)::setenv("WATCHDOG_USEC", "2000000", 1); // [状態] - watchdog の期限を 2 [s] (確認周期 500 [ms]) とする。
    g_on_run_action = []()
    { wait_repeatedly(100, 20); }; // [状態] - on_run は cplat_service_wait_for_stop(100) を 20 回繰り返す。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    int ready_index = index_of(messages, "READY=1");
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    ASSERT_NE(-1, ready_index);      // [確認_正常系] - READY=1 が届くこと。
    EXPECT_NE(-1, index_of(messages, "WATCHDOG=1",
                           static_cast<size_t>(ready_index))); // [確認_正常系] - READY=1 の後に WATCHDOG=1 が届くこと。
}

// on_run が cplat_service_wait_for_stop() を呼ばずに止まっている間は WATCHDOG=1 を送らず WARNING を 1 回出すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_does_not_send_watchdog_while_on_run_is_blocked)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready());                // [状態確認] - 通知を受け取るソケットを準備できること。
    (void)::setenv("WATCHDOG_USEC", "2000000", 1); // [状態] - watchdog の期限を 2 [s] とする。
    g_on_run_action = []() { sleep_ms(1500); };    // [状態] - on_run は待機 API を呼ばずに 1500 [ms] 止まる。

    // Pre-Assert
    allow_any_trace();
    EXPECT_CALL(mock_cplat_, cplat_tracer_write_with_source(_, CPLAT_TRACE_LEVEL_WARNING, _, _, _,
                                                            HasSubstr("WATCHDOG=1 を送りません")))
        .Times(1); // [Pre-Assert確認_異常系] - on_run 停滞を示す WARNING トレースが 1 回出力されること。

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, count_of(messages, "WATCHDOG=1")); // [確認_異常系] - WATCHDOG=1 が届かないこと。
}

// 動作中の段階の外 (on_start と on_stop の実行中) は WATCHDOG=1 を送らないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_does_not_send_watchdog_outside_running)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready());                // [状態確認] - 通知を受け取るソケットを準備できること。
    (void)::setenv("WATCHDOG_USEC", "2000000", 1); // [状態] - watchdog の期限を 2 [s] とする。
    g_on_start_action = []() { sleep_ms(1500); };  // [状態] - on_start は 1500 [ms] 止まる。
    g_on_stop_action = []() { sleep_ms(1500); };   // [状態] - on_stop は 1500 [ms] 止まる。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    int ready_index = index_of(messages, "READY=1");
    int stopping_index = index_of(messages, "STOPPING=1");
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    ASSERT_NE(-1, ready_index);      // [確認_正常系] - READY=1 が届くこと。
    ASSERT_NE(-1, stopping_index);   // [確認_正常系] - STOPPING=1 が届くこと。
    EXPECT_EQ(-1, index_of(std::vector<std::string>(messages.begin(), messages.begin() + ready_index),
                           "WATCHDOG=1")); // [確認_正常系] - READY=1 の前に WATCHDOG=1 が届かないこと。
    EXPECT_EQ(-1, index_of(messages, "WATCHDOG=1",
                           static_cast<size_t>(
                               stopping_index))); // [確認_正常系] - STOPPING=1 の後に WATCHDOG=1 が届かないこと。
}

// 停止の時にイベント監視スレッドが時間内に終了しない場合に資源を解放せずに異常終了することの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_aborts_when_events_thread_does_not_stop)
{
    // Arrange
    cplat_service_definition def = make_run_def();
    int exit_code = -1;
    ON_CALL(mock_cplat_, cplat_thread_timed_join(_, _))
        .WillByDefault(Return(CPLAT_ERR_TIMEOUT)); // [状態] - イベント監視スレッドが時間内に終了しない状態とする。
    ON_CALL(mock_cplat_, cplat_thread_detach(_)).WillByDefault(Invoke([](cplat_thread *) { g_detach_calls++; }));
    ON_CALL(mock_cplat_, cplat_close(_, _))
        .WillByDefault(Invoke(
            [](int fd, cplat_error *detail_out) -> int
            {
                g_close_calls++;
                return delegate_real_cplat_close(fd, detail_out);
            }));
    ON_CALL(mock_cplat_, cplat_abort_at(_, _, _, _, _))
        .WillByDefault(Invoke(
            [](cplat_abort_kind, const char *, const char *, int, const char *func)
            {
                (void)std::fprintf(stderr, "aborted in %s detach=%d close=%d\n", func, g_detach_calls.load(),
                                   g_close_calls.load());
                (void)std::fflush(stderr);
                std::_Exit(3);
            }));

    // Pre-Assert

    // Act
    // Assert
    EXPECT_DEATH(
        (void)cplat_internal_service_os_run(&def, &exit_code),
        "aborted in cplat_internal_service_linux_events_stop detach=0 close=0"); // [確認_異常系] - cplat_internal_service_linux_events_stop() から異常終了し detach=0 close=0 であること。
}

// on_start の実行中の SIGHUP は配送せず on_run の実行中の SIGHUP だけを on_reload へ配送することの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, reload_is_dispatched_only_while_running)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def(true);
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。
    int reload_count_after_start = -1;
    g_on_start_action = [&reload_count_after_start]()
    {
        (void)::kill(::getpid(), SIGHUP);
        sleep_ms(300);
        reload_count_after_start = g_reload_count.load();
    };
    g_on_run_action = []()
    {
        (void)::kill(::getpid(), SIGHUP);
        for (int i = 0; (i < 50) && (g_reload_count.load() == 0); i++)
        {
            (void)cplat_service_wait_for_stop(100);
        }
    };

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_service_os_run(
        &def, &exit_code); // [手順] - on_reload がある定義で cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    int reloading_index = index_of(messages, "RELOADING=1");
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0,
              reload_count_after_start); // [確認_正常系] - on_start 実行中の SIGHUP では on_reload が呼ばれないこと。
    EXPECT_EQ(1, g_reload_count.load()); // [確認_正常系] - on_reload が 1 回だけ呼ばれること。
    ASSERT_NE(-1, reloading_index);      // [確認_正常系] - RELOADING=1 が届くこと。
    EXPECT_NE(
        -1, index_of(messages, "READY=1",
                     static_cast<size_t>(reloading_index))); // [確認_正常系] - RELOADING=1 の後に READY=1 が届くこと。
}

// on_reload の実行中に停止が始まった場合 STOPPING=1 の後に READY=1 を送らず停止処理が callback 完了を待たないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, stopping_during_reload_suppresses_ready)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def(true);
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。
    g_on_reload_action = []()
    {
        std::unique_lock<std::mutex> lock(g_reload_mutex);
        g_reload_entered = true;
        g_reload_cv.notify_all();
        if (!g_reload_cv.wait_for(lock, std::chrono::seconds(5), []() { return g_release_reload; }))
        {
            g_reload_timed_out = 1;
        }
        g_reload_finished = true;
        g_reload_cv.notify_all();
    };
    g_on_run_action = []()
    {
        (void)::kill(::getpid(), SIGHUP);
        std::unique_lock<std::mutex> lock(g_reload_mutex);
        g_reload_started_before_stop =
            g_reload_cv.wait_for(lock, std::chrono::seconds(5), []() { return g_reload_entered; }) ? 1 : 0;
        lock.unlock();
        cplat_service_request_stop();
    };
    g_on_stop_action = [&notify]()
    {
        g_stopping_received_in_on_stop = notify.wait_for_message("STOPPING=1", 5000) ? 1 : 0;
        {
            std::lock_guard<std::mutex> lock(g_reload_mutex);
            g_release_reload = true;
        }
        g_reload_cv.notify_all();
        std::unique_lock<std::mutex> lock(g_reload_mutex);
        g_reload_finished_before_on_stop_return =
            g_reload_cv.wait_for(lock, std::chrono::seconds(5), []() { return g_reload_finished; }) ? 1 : 0;
    };

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_service_os_run(
        &def, &exit_code); // [手順] - on_reload と停止が競合する状態で cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    int stopping_index = index_of(messages, "STOPPING=1");
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    ASSERT_NE(-1, stopping_index);   // [確認_正常系] - STOPPING=1 が届くこと。
    EXPECT_NE(-1, index_of(messages, "RELOADING=1")); // [確認_正常系] - RELOADING=1 が届くこと。
    EXPECT_EQ(-1, index_of(messages, "READY=1",
                           static_cast<size_t>(stopping_index +
                                               1))); // [確認_正常系] - STOPPING=1 の後に READY=1 が届かないこと。
    EXPECT_EQ(1, g_reload_count.load());             // [確認_正常系] - on_reload が 1 回呼ばれること。
    EXPECT_EQ(1, g_reload_started_before_stop);      // [確認_正常系] - 停止要求の前に on_reload が始まること。
    EXPECT_EQ(1, g_stopping_received_in_on_stop);    // [確認_正常系] - on_stop 中に STOPPING=1 を確認すること。
    EXPECT_EQ(1,
              g_reload_finished_before_on_stop_return); // [確認_正常系] - on_stop 復帰前に on_reload が完了すること。
    EXPECT_EQ(0, g_reload_timed_out);                   // [確認_正常系] - on_reload が時間切れにならないこと。
}

// 停止中の段階では再読込の開始と完了を通知せず on_reload は 1 回呼び出すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, reload_notifications_are_not_sent_while_stopping)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def(true);
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。
    test_svc_linux_events_set_definition(&def);
    test_svc_linux_events_set_phase(2); // [状態] - サービスの段階を停止中にする。
    allow_any_trace();

    // Pre-Assert

    // Act
    cplat_internal_service_dispatch_reload(
        &def); // [手順] - 停止中に cplat_internal_service_dispatch_reload() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(0, count_of(messages, "RELOADING=1")); // [確認_正常系] - RELOADING=1 が届かないこと。
    EXPECT_EQ(0, count_of(messages, "READY=1"));     // [確認_正常系] - READY=1 が届かないこと。
    EXPECT_EQ(1, g_reload_count.load());             // [確認_正常系] - on_reload が 1 回呼ばれること。

    // Cleanup
    test_svc_linux_events_set_phase(0);
    test_svc_linux_events_set_definition(nullptr);
}

// 動作中の再読込完了で READY=1 を再送しても動作段階を維持することの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, ready_after_reload_keeps_running_phase)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def(true);
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。
    g_on_run_action = [&def]()
    {
        cplat_internal_service_dispatch_reload(&def);
        g_phase_after_reload = test_svc_linux_events_get_phase();
    };

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_service_os_run(&def, &exit_code); // [手順] - 動作中に再読込を実行する。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    int reloading_index = index_of(messages, "RELOADING=1");
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    ASSERT_NE(-1, reloading_index);  // [確認_正常系] - RELOADING=1 が届くこと。
    EXPECT_NE(-1, index_of(messages, "READY=1",
                           static_cast<size_t>(reloading_index +
                                               1))); // [確認_正常系] - RELOADING=1 の後に READY=1 が届くこと。
    EXPECT_EQ(1, g_phase_after_reload);              // [確認_正常系] - 再読込完了後も動作中段階 (1) を維持すること。
}

// 停止段階への遷移を待っていた watchdog が遷移後に WATCHDOG=1 を送らないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, watchdog_is_not_sent_after_stopping)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。
    g_on_run_action = []()
    {
        (void)cplat_service_wait_for_stop(1);
        cplat_internal_service_linux_events_notify_lock();
        g_track_watchdog_lock = 1;
        std::thread watchdog_thread(
            []()
            {
                test_svc_linux_events_run_watchdog_timer();
                g_watchdog_finished = 1;
            });
        for (int i = 0; (i < 5000) && (g_watchdog_lock_attempted.load() == 0); i++)
        {
            std::this_thread::yield();
        }
        g_watchdog_waited_for_lock =
            (g_watchdog_lock_attempted.load() != 0) && (g_watchdog_finished.load() == 0) ? 1 : 0;
        cplat_internal_service_linux_events_enter_stopping();
        g_track_watchdog_lock = 0;
        cplat_internal_service_linux_events_notify_unlock();
        watchdog_thread.join();
    };

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_event_source_set_time(_, _, _, testing::IsNull(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - sd_event_source_set_time() が nullptr を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - sd_event_source_set_time() から 0 を返す。
    EXPECT_CALL(mock_systemd_, sd_event_source_set_enabled(_, _, _, testing::IsNull(), SD_EVENT_ONESHOT))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - sd_event_source_set_enabled() が nullptr を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - sd_event_source_set_enabled() から 0 を返す。

    // Act
    int actual_ret = cplat_internal_service_os_run(
        &def, &exit_code); // [手順] - watchdog の競合を作って cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, g_watchdog_waited_for_lock);       // [確認_正常系] - watchdog が通知ロックを待つこと。
    EXPECT_EQ(1, g_watchdog_finished.load());       // [確認_正常系] - ロック解放後に watchdog callback が終了すること。
    EXPECT_EQ(0, count_of(messages, "WATCHDOG=1")); // [確認_正常系] - 停止段階の後に WATCHDOG=1 が届かないこと。
}

// 通知の抑止と sd_notify() の失敗を記録するときに通知ロックを保持していないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, notification_trace_is_written_after_unlock)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = -1;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。
    g_on_start_action = [&notify]()
    {
        test_svc_linux_events_set_phase(2);
        notify.remove_path();
    };
    allow_any_trace();

    // Pre-Assert
    EXPECT_CALL(mock_cplat_,
                cplat_tracer_write_with_source(_, CPLAT_TRACE_LEVEL_INFO, _, _, _, HasSubstr("READY=1 を送りません")))
        .WillOnce(Invoke(
            [](auto &&...)
            {
                EXPECT_EQ(nullptr, g_notification_lock_held);
                return CPLAT_OK;
            })); // [Pre-Assert確認_正常系] - READY=1 抑止トレース時に通知ロックを保持していないこと。
                 // [Pre-Assert手順] - cplat_tracer_write_with_source() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat_,
                cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("STOPPING=1 の送信に失敗しました")))
        .WillOnce(Invoke(
            [](auto &&...)
            {
                EXPECT_EQ(nullptr, g_notification_lock_held);
                return CPLAT_OK;
            })); // [Pre-Assert確認_正常系] - sd_notify() 失敗トレース時に通知ロックを保持していないこと。
                 // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_internal_service_os_run(&def, &exit_code); // [手順] - 抑止と送信失敗のトレースを発生させる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
}

// 通知ロックの生成に失敗した場合はイベント監視スレッドとサービスを開始しないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, notification_lock_creation_failure_fails_run_service)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。
    allow_any_trace();

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_local_lock_create(_))
        .WillOnce(Return(
            CPLAT_ERR_OUT_OF_MEMORY)); // [Pre-Assert確認_異常系] - cplat_local_lock_create() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_create() から CPLAT_ERR_OUT_OF_MEMORY を返す。
    EXPECT_CALL(mock_cplat_, cplat_thread_create(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_thread_create() が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_service_os_run(
        &def, &exit_code); // [手順] - 通知ロック生成を失敗させて cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, count_of(messages, "READY=1")); // [確認_異常系] - READY=1 が届かないこと。
    EXPECT_TRUE(g_run_calls.empty());            // [確認_異常系] - g_run_calls.empty() の戻り値が true であること。
}

// 動作中の段階でだけ OS イベントを on_event へ配送することの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, event_is_not_dispatched_outside_running)
{
    // Arrange
    cplat_service_definition def = make_run_def(false, true);
    cplat_service_event_info info = {CPLAT_SERVICE_EVENT_POWER_SUSPEND, 0U, nullptr};
    test_svc_linux_events_set_definition(&def);

    // Pre-Assert

    // Act
    test_svc_linux_events_set_phase(0);
    test_svc_linux_events_dispatch_event(&info); // [手順] - 起動中段階で配送関数を呼び出す。
    size_t count_while_starting = g_dispatched_events.size();
    test_svc_linux_events_set_phase(1);
    test_svc_linux_events_dispatch_event(&info); // [手順] - 動作中段階で配送関数を呼び出す。
    size_t count_while_running = g_dispatched_events.size();
    test_svc_linux_events_set_phase(2);
    test_svc_linux_events_dispatch_event(&info); // [手順] - 停止中段階で配送関数を呼び出す。
    size_t count_while_stopping = g_dispatched_events.size();

    // Assert
    EXPECT_EQ(0U, count_while_starting); // [確認_正常系] - 起動中段階では on_event が呼ばれないこと。
    EXPECT_EQ(1U, count_while_running);  // [確認_正常系] - 動作中段階では on_event が 1 回呼ばれること。
    EXPECT_EQ(1U, count_while_stopping); // [確認_正常系] - 停止中段階では on_event が呼ばれないこと。

    // Cleanup
    test_svc_linux_events_set_phase(0);
    test_svc_linux_events_set_definition(nullptr);
}

// on_reload がない場合に SIGHUP でプロセスが終了せず WARNING トレースを出すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, sighup_without_on_reload_is_traced)
{
    // Arrange
    cplat_service_definition def = make_run_def(false);
    int exit_code = -1;
    g_on_run_action = []()
    {
        (void)::kill(::getpid(), SIGHUP);
        sleep_ms(300);
    };

    // Pre-Assert
    allow_any_trace();
    EXPECT_CALL(mock_cplat_,
                cplat_tracer_write_with_source(_, CPLAT_TRACE_LEVEL_WARNING, _, _, _, HasSubstr("SIGHUP を無視します")))
        .Times(1); // [Pre-Assert確認_異常系] - SIGHUP 無視の WARNING トレースが 1 回出力されること。

    // Act
    int actual_ret = cplat_internal_service_os_run(
        &def, &exit_code); // [手順] - on_reload が nullptr の定義で cplat_internal_service_os_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
}

// SIGHUP の受信を登録できない場合に on_start を呼ばず CPLAT_ERR_UNKNOWN を返すことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_fails_when_sighup_source_cannot_be_registered)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    ASSERT_TRUE(notify.is_ready()); // [状態確認] - 通知を受け取るソケットを準備できること。

    // Pre-Assert
    allow_any_trace();
    expect_start_failure_trace();
    EXPECT_CALL(mock_systemd_, sd_event_add_signal(_, _, _, _, _, SIGHUP, _, _))
        .WillOnce(Return(
            -ENOMEM)); // [Pre-Assert確認_異常系] - sd_event_add_signal() が SIGHUP を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - sd_event_add_signal() から -ENOMEM を返す。

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_TRUE(g_run_calls.empty());             // [確認_異常系] - g_run_calls.empty() の戻り値が true であること。
    EXPECT_EQ(-1, index_of(messages, "READY=1")); // [確認_異常系] - READY=1 が届かないこと。
}

// SIGHUP がブロックされていない場合に実物の sd_event_add_signal() が起動を失敗させることの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, run_service_fails_when_sighup_is_not_blocked)
{
    // Arrange
    NotifySocket notify;
    cplat_service_definition def = make_run_def();
    int exit_code = 99;
    sigset_t unblock_set;
    ASSERT_TRUE(notify.is_ready());
    ASSERT_EQ(0, ::sigemptyset(&unblock_set));
    ASSERT_EQ(0, ::sigaddset(&unblock_set, SIGHUP));
    ASSERT_EQ(0, ::pthread_sigmask(SIG_UNBLOCK, &unblock_set, nullptr)); // [状態] - SIGHUP のブロックを外す。

    // Pre-Assert
    allow_any_trace();
    expect_start_failure_trace();

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。

    // Assert
    std::vector<std::string> messages = notify.receive_all();
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_service_os_run() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_TRUE(g_run_calls.empty());             // [確認_異常系] - g_run_calls.empty() の戻り値が true であること。
    EXPECT_EQ(-1, index_of(messages, "READY=1")); // [確認_異常系] - READY=1 が届かないこと。
}

// 停止の処理の後の SIGHUP でプロセスが終了せずシグナルが保留されることの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, sighup_after_stop_does_not_terminate_process)
{
    // Arrange
    cplat_service_definition def = make_run_def();
    int exit_code = -1;

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_internal_service_os_run(&def, &exit_code); // [手順] - cplat_internal_service_os_run() を呼び出す。
    ASSERT_EQ(0, ::kill(::getpid(), SIGHUP));            // [手順] - 停止後にプロセス宛てへ SIGHUP を送る。
    sigset_t pending;
    ASSERT_EQ(0, ::sigpending(&pending)); // [手順] - 保留中のシグナルを読む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_service_os_run() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, ::sigismember(&pending, SIGHUP)); // [確認_正常系] - SIGHUP が保留のままであること。
}

/* ============================================================
 *  logind D-Bus inhibitor lock のテスト
 * ============================================================ */

// 要求メッセージの作成に失敗した場合に作成済みメッセージを 1 回だけ解放することの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, build_inhibit_message_cleans_partial_message)
{
    // Arrange
    cplat_service_definition def = make_run_def();
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_bus(k_dummy_bus);
    test_svc_linux_events_set_definition(&def);
    prevent_real_dbus(mock_systemd_);

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_bus_message_new_method_call(_, _, _, _, _, _, _, _, _))
        .WillOnce(
            Return(-ENOMEM)); // [Pre-Assert確認_異常系] - sd_bus_message_new_method_call() が 1 回呼び出されること。
                              // [Pre-Assert手順] - sd_bus_message_new_method_call() から -ENOMEM を返す。
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - sd_bus_message_unref() が呼び出されないこと。

    // Act
    void *message = k_dummy_message;
    int actual_ret = test_svc_linux_events_build_inhibit_message("sleep", &message); // [手順] - 要求メッセージを作る。

    // Assert
    EXPECT_EQ(-ENOMEM, actual_ret); // [確認_異常系] - 戻り値が -ENOMEM であること。
    EXPECT_EQ(nullptr, message);    // [確認_異常系] - message が nullptr であること。
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));

    for (int fail_at = 0; fail_at < 4; fail_at++)
    {
        int seen = 0;
        prevent_real_dbus(mock_systemd_);
        EXPECT_CALL(mock_systemd_, sd_bus_message_new_method_call(_, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(SetArgPointee<4>(k_dummy_message), Return(0)));
        EXPECT_CALL(mock_systemd_, sd_bus_message_append_basic(_, _, _, k_dummy_message, 's', _))
            .Times(fail_at + 1)
            .WillRepeatedly(Invoke(
                [&seen, &fail_at](const char *, const int, const char *, sd_bus_message *, char, const void *) -> int
                {
                    int current = seen;
                    seen++;
                    return (current == fail_at) ? -EINVAL : 0;
                }));
        EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, k_dummy_message)).WillOnce(Return(nullptr));

        message = k_dummy_message;
        actual_ret = test_svc_linux_events_build_inhibit_message("sleep", &message);

        EXPECT_EQ(-EINVAL, actual_ret); // [確認_異常系] - append 失敗時の戻り値が -EINVAL であること。
        EXPECT_EQ(nullptr, message);    // [確認_異常系] - append 失敗時の message が nullptr であること。
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
    }

    // Cleanup
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_definition(nullptr);
}

// 起動時の同期呼び出しが成功と失敗のすべての経路で資源を 1 回解放することの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, startup_sync_call_cleans_resources_on_all_results)
{
    // Arrange
    NiceMock<Mock_fcntl> mock_fcntl;
    cplat_service_definition def = make_run_def();
    int pipe_fd[2] = {-1, -1};
    ASSERT_EQ(0, ::pipe(pipe_fd)); // [状態確認] - パイプを開けること。
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_bus(k_dummy_bus);
    test_svc_linux_events_set_definition(&def);
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    const int call_results[] = {-EIO, -ETIMEDOUT};
    for (int call_result : call_results)
    {
        // Pre-Assert
        expect_inhibit_message_built(mock_systemd_);
        EXPECT_CALL(mock_systemd_, sd_bus_call(_, _, _, k_dummy_bus, k_dummy_message, k_inhibit_timeout_usec, _, _))
            .WillOnce(DoAll(SetArgPointee<7>(k_dummy_reply), Return(call_result)));
        EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, _, _, _)).Times(0);
        EXPECT_CALL(mock_fcntl, fcntl(_, _, _, _, _, _)).Times(0);
        EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(2).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(1);
        EXPECT_CALL(mock_cplat_, cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("inhibitor lock")))
            .Times(1);

        // Act
        test_svc_linux_events_acquire_inhibit_sync(0); // [手順] - sleep の lock を同期で取得する。

        // Assert
        EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0)); // [確認_異常系] - fd が -1 のままであること。
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_fcntl));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_cplat_));
        prevent_real_dbus(mock_systemd_);
        allow_any_trace();
    }

    const int read_results[] = {-EINVAL, 0};
    for (int read_result : read_results)
    {
        expect_inhibit_message_built(mock_systemd_);
        EXPECT_CALL(mock_systemd_, sd_bus_call(_, _, _, _, _, k_inhibit_timeout_usec, _, _))
            .WillOnce(DoAll(SetArgPointee<7>(k_dummy_reply), Return(0)));
        EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
            .WillOnce(Invoke([read_result](const char *, const int, const char *, sd_bus_message *message, char type,
                                           void *out) -> int
                             { return read_basic_writes_fd(message, type, out, -1, read_result); }));
        EXPECT_CALL(mock_fcntl, fcntl(_, _, _, _, _, _)).Times(0);
        EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(2).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(1);
        EXPECT_CALL(mock_cplat_, cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("inhibitor lock")))
            .Times(1);

        test_svc_linux_events_acquire_inhibit_sync(0);

        EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0)); // [確認_異常系] - 読み取り失敗時の fd が -1 であること。
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_fcntl));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_cplat_));
        prevent_real_dbus(mock_systemd_);
        allow_any_trace();
    }

    expect_inhibit_message_built(mock_systemd_);
    EXPECT_CALL(mock_systemd_, sd_bus_call(_, _, _, _, _, k_inhibit_timeout_usec, _, _))
        .WillOnce(DoAll(SetArgPointee<7>(k_dummy_reply), Return(0)));
    EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
        .WillOnce(
            Invoke([&pipe_fd](const char *, const int, const char *, sd_bus_message *message, char type,
                              void *out) -> int { return read_basic_writes_fd(message, type, out, pipe_fd[0], 1); }));
    EXPECT_CALL(mock_fcntl, fcntl(_, _, _, pipe_fd[0], F_DUPFD_CLOEXEC, _)).WillOnce(Return(-1));
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(2).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(1);
    EXPECT_CALL(mock_cplat_, cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("inhibitor lock")))
        .Times(1);

    test_svc_linux_events_acquire_inhibit_sync(0);

    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0)); // [確認_異常系] - fcntl 失敗時の fd が -1 であること。
    EXPECT_NE(-1, delegate_real_fcntl(__FILE__, __LINE__, __func__, pipe_fd[0], F_GETFD,
                                      0)); // [確認_異常系] - 借用 fd が閉じられていないこと。
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_fcntl));
    prevent_real_dbus(mock_systemd_);

    expect_inhibit_message_built(mock_systemd_);
    EXPECT_CALL(mock_systemd_, sd_bus_call(_, _, _, _, _, k_inhibit_timeout_usec, _, _))
        .WillOnce(DoAll(SetArgPointee<7>(k_dummy_reply), Return(0)));
    EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
        .WillOnce(
            Invoke([&pipe_fd](const char *, const int, const char *, sd_bus_message *message, char type,
                              void *out) -> int { return read_basic_writes_fd(message, type, out, pipe_fd[0], 1); }));
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(2).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(1);

    test_svc_linux_events_acquire_inhibit_sync(0);

    int stored_fd = test_svc_linux_events_inhibit_fd(0);
    int stored_flags = delegate_real_fcntl(__FILE__, __LINE__, __func__, stored_fd, F_GETFD, 0);
    EXPECT_GE(stored_fd, 3);                 // [確認_正常系] - 複製した fd が 3 以上であること。
    EXPECT_NE(pipe_fd[0], stored_fd);        // [確認_正常系] - 複製した fd が借用 fd と異なること。
    EXPECT_NE(0, stored_flags & FD_CLOEXEC); // [確認_正常系] - 複製した fd に FD_CLOEXEC が付くこと。
    EXPECT_NE(-1, delegate_real_fcntl(__FILE__, __LINE__, __func__, pipe_fd[0], F_GETFD,
                                      0)); // [確認_正常系] - 借用 fd が閉じられていないこと。

    // Cleanup
    (void)delegate_real_cplat_close(stored_fd, nullptr);
    (void)::close(pipe_fd[0]);
    (void)::close(pipe_fd[1]);
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_definition(nullptr);
}

// 読み取りが 0 または負値のとき fcntl() を呼ばず fd を未取得のままにすることの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, store_inhibit_fd_does_not_duplicate_failed_or_empty_read)
{
    // Arrange
    NiceMock<Mock_fcntl> mock_fcntl;
    test_svc_linux_events_reset_inhibit();
    prevent_real_dbus(mock_systemd_);
    const int read_results[] = {-EINVAL, 0};

    for (int read_result : read_results)
    {
        test_svc_linux_events_set_inhibit_fd(0, -1);

        // Pre-Assert
        EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
            .WillOnce(Invoke([read_result](const char *, const int, const char *, sd_bus_message *message, char type,
                                           void *out) -> int
                             { return read_basic_writes_fd(message, type, out, -1, read_result); }));
        EXPECT_CALL(mock_fcntl, fcntl(_, _, _, _, _, _)).Times(0);

        // Act
        int actual_ret = test_svc_linux_events_store_inhibit_fd(0, k_dummy_reply); // [手順] - fd の保存を呼ぶ。

        // Assert
        EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);           // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
        EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0)); // [確認_異常系] - fd が -1 のままであること。
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_fcntl));
        prevent_real_dbus(mock_systemd_);
    }

    // Cleanup
    test_svc_linux_events_reset_inhibit();
}

// fcntl() が失敗した場合に借用 fd を閉じず lock を未取得のままにすることの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, store_inhibit_fd_leaves_lock_unacquired_when_fcntl_fails)
{
    // Arrange
    NiceMock<Mock_fcntl> mock_fcntl;
    int pipe_fd[2] = {-1, -1};
    ASSERT_EQ(0, ::pipe(pipe_fd)); // [状態確認] - パイプを開けること。
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_inhibit_fd(0, -1);
    prevent_real_dbus(mock_systemd_);

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
        .WillOnce(
            Invoke([&pipe_fd](const char *, const int, const char *, sd_bus_message *message, char type,
                              void *out) -> int { return read_basic_writes_fd(message, type, out, pipe_fd[0], 1); }));
    EXPECT_CALL(mock_fcntl, fcntl(_, _, _, pipe_fd[0], F_DUPFD_CLOEXEC, _)).WillOnce(Return(-1));

    // Act
    int actual_ret = test_svc_linux_events_store_inhibit_fd(0, k_dummy_reply); // [手順] - fd の保存を呼ぶ。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);           // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0)); // [確認_異常系] - fd が -1 のままであること。
    EXPECT_NE(-1, delegate_real_fcntl(__FILE__, __LINE__, __func__, pipe_fd[0], F_GETFD,
                                      0)); // [確認_異常系] - 借用 fd が閉じられていないこと。

    // Cleanup
    (void)::close(pipe_fd[0]);
    (void)::close(pipe_fd[1]);
    test_svc_linux_events_reset_inhibit();
}

// 復帰時に明示した期限の非同期要求だけを行い応答前は fd を持たないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, resume_requests_inhibit_asynchronously_with_explicit_timeout)
{
    // Arrange
    cplat_service_definition def = make_run_def();
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_bus(k_dummy_bus);
    test_svc_linux_events_set_definition(&def);
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    // Pre-Assert
    expect_inhibit_message_built(mock_systemd_);
    EXPECT_CALL(mock_systemd_, sd_bus_call(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - 同期の sd_bus_call() が呼び出されないこと。
    EXPECT_CALL(mock_systemd_,
                sd_bus_call_async(_, _, _, k_dummy_bus, _, k_dummy_message, _, _, k_inhibit_timeout_usec))
        .WillOnce(DoAll(
            SetArgPointee<4>(k_dummy_slot),
            Return(0))); // [Pre-Assert確認_正常系] - sd_bus_call_async() が 10 [s] の期限で 1 回呼び出されること。
                         // [Pre-Assert手順] - slot を設定して 0 を返す。
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, k_dummy_message))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_正常系] - 要求メッセージを 1 回 unref すること。

    // Act
    test_svc_linux_events_handle_prepare_for_sleep(0); // [手順] - 復帰 (starting == 0) を処理する。

    // Assert
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));             // [確認_正常系] - 応答前の fd が -1 であること。
    EXPECT_EQ(k_dummy_slot, test_svc_linux_events_inhibit_slot(0)); // [確認_正常系] - 応答待ちの slot が残ること。

    // Cleanup
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_definition(nullptr);
}

// 正常な非同期応答で複製した fd を保持し借用した引数を解放しないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, inhibit_reply_stores_duplicated_fd)
{
    // Arrange
    int pipe_fd[2] = {-1, -1};
    sd_bus_error borrowed_error = SD_BUS_ERROR_NULL;
    ASSERT_EQ(0, ::pipe(pipe_fd)); // [状態確認] - パイプを開けること。
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_inhibit_slot(0, k_dummy_slot);
    test_svc_linux_events_set_inhibit_fd(0, -1);
    prevent_real_dbus(mock_systemd_);

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, k_dummy_slot)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_systemd_, sd_bus_message_is_method_error(_, _, _, k_dummy_reply, nullptr)).WillOnce(Return(0));
    EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
        .WillOnce(
            Invoke([&pipe_fd](const char *, const int, const char *, sd_bus_message *message, char type,
                              void *out) -> int { return read_basic_writes_fd(message, type, out, pipe_fd[0], 1); }));
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(0);

    // Act
    int actual_ret = test_svc_linux_events_on_inhibit_reply(0, k_dummy_reply,
                                                            &borrowed_error); // [手順] - 正常応答のコールバックを呼ぶ。

    // Assert
    int stored_fd = test_svc_linux_events_inhibit_fd(0);
    int stored_flags = delegate_real_fcntl(__FILE__, __LINE__, __func__, stored_fd, F_GETFD, 0);
    EXPECT_EQ(0, actual_ret);                                  // [確認_正常系] - コールバックの戻り値が 0 であること。
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_正常系] - slot が nullptr であること。
    EXPECT_GE(stored_fd, 3);                                   // [確認_正常系] - 複製した fd が 3 以上であること。
    EXPECT_NE(pipe_fd[0], stored_fd);                          // [確認_正常系] - 複製した fd が借用 fd と異なること。
    EXPECT_NE(0, stored_flags & FD_CLOEXEC);                   // [確認_正常系] - 複製した fd に FD_CLOEXEC が付くこと。
    EXPECT_NE(-1, delegate_real_fcntl(__FILE__, __LINE__, __func__, pipe_fd[0], F_GETFD,
                                      0)); // [確認_正常系] - 借用 fd が閉じられていないこと。

    // Cleanup
    (void)delegate_real_cplat_close(stored_fd, nullptr);
    (void)::close(pipe_fd[0]);
    (void)::close(pipe_fd[1]);
    test_svc_linux_events_reset_inhibit();
}

// method error の応答で lock を未取得のままにし借用したエラーを解放しないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, inhibit_reply_error_leaves_lock_unacquired)
{
    // Arrange
    sd_bus_error borrowed_error = SD_BUS_ERROR_NULL;
    borrowed_error.name = "org.freedesktop.DBus.Error.NoReply";
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_inhibit_slot(0, k_dummy_slot);
    test_svc_linux_events_set_inhibit_fd(0, -1);
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, k_dummy_slot)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_systemd_, sd_bus_message_is_method_error(_, _, _, k_dummy_reply, nullptr)).WillOnce(Return(1));
    EXPECT_CALL(mock_systemd_, sd_bus_message_get_error(_, _, _, k_dummy_reply)).WillOnce(Return(&borrowed_error));
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_cplat_, cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("応答がエラーです")))
        .Times(1);

    // Act
    int actual_ret = test_svc_linux_events_on_inhibit_reply(
        0, k_dummy_reply, &borrowed_error); // [手順] - method error のコールバックを呼ぶ。

    // Assert
    EXPECT_EQ(0, actual_ret);                                  // [確認_異常系] - コールバックの戻り値が 0 であること。
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));        // [確認_異常系] - fd が -1 のままであること。
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_異常系] - slot が nullptr であること。

    // Cleanup
    test_svc_linux_events_reset_inhibit();
}

// 非同期応答の読み取りと fcntl() の失敗で slot を解除し lock を未取得のままにすることの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, inhibit_reply_read_and_fcntl_failures_release_slot)
{
    // Arrange
    NiceMock<Mock_fcntl> mock_fcntl;
    int pipe_fd[2] = {-1, -1};
    sd_bus_error borrowed_error = SD_BUS_ERROR_NULL;
    ASSERT_EQ(0, ::pipe(pipe_fd)); // [状態確認] - パイプを開けること。
    test_svc_linux_events_reset_inhibit();
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    const int read_results[] = {-EINVAL, 0};
    for (int read_result : read_results)
    {
        test_svc_linux_events_set_inhibit_slot(0, k_dummy_slot);
        test_svc_linux_events_set_inhibit_fd(0, -1);

        // Pre-Assert
        EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, k_dummy_slot)).WillOnce(Return(nullptr));
        EXPECT_CALL(mock_systemd_, sd_bus_message_is_method_error(_, _, _, k_dummy_reply, nullptr)).WillOnce(Return(0));
        EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
            .WillOnce(Invoke([read_result](const char *, const int, const char *, sd_bus_message *message, char type,
                                           void *out) -> int
                             { return read_basic_writes_fd(message, type, out, -1, read_result); }));
        EXPECT_CALL(mock_fcntl, fcntl(_, _, _, _, _, _)).Times(0);
        EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(0);
        EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(0);
        EXPECT_CALL(mock_cplat_,
                    cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("fd を保存できませんでした")))
            .Times(1);

        // Act
        int actual_ret = test_svc_linux_events_on_inhibit_reply(0, k_dummy_reply, &borrowed_error);

        // Assert
        EXPECT_EQ(0, actual_ret);                           // [確認_異常系] - コールバックの戻り値が 0 であること。
        EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0)); // [確認_異常系] - fd が -1 のままであること。
        EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_異常系] - slot が nullptr であること。
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_fcntl));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_cplat_));
        prevent_real_dbus(mock_systemd_);
        allow_any_trace();
    }

    test_svc_linux_events_set_inhibit_slot(0, k_dummy_slot);
    test_svc_linux_events_set_inhibit_fd(0, -1);

    EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, k_dummy_slot)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_systemd_, sd_bus_message_is_method_error(_, _, _, k_dummy_reply, nullptr)).WillOnce(Return(0));
    EXPECT_CALL(mock_systemd_, sd_bus_message_read_basic(_, _, _, k_dummy_reply, 'h', _))
        .WillOnce(
            Invoke([&pipe_fd](const char *, const int, const char *, sd_bus_message *message, char type,
                              void *out) -> int { return read_basic_writes_fd(message, type, out, pipe_fd[0], 1); }));
    EXPECT_CALL(mock_fcntl, fcntl(_, _, _, pipe_fd[0], F_DUPFD_CLOEXEC, _)).WillOnce(Return(-1));
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_systemd_, sd_bus_error_free(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_cplat_,
                cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("fd を保存できませんでした")))
        .Times(1);

    int actual_ret = test_svc_linux_events_on_inhibit_reply(
        0, k_dummy_reply, &borrowed_error); // [手順] - 複製に失敗する応答のコールバックを呼ぶ。

    EXPECT_EQ(0, actual_ret);                                  // [確認_異常系] - コールバックの戻り値が 0 であること。
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));        // [確認_異常系] - fd が -1 のままであること。
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_異常系] - slot が nullptr であること。
    EXPECT_NE(-1, delegate_real_fcntl(__FILE__, __LINE__, __func__, pipe_fd[0], F_GETFD,
                                      0)); // [確認_異常系] - 借用 fd が閉じられていないこと。

    // Cleanup
    (void)::close(pipe_fd[0]);
    (void)::close(pipe_fd[1]);
    test_svc_linux_events_reset_inhibit();
}

// 非同期要求の作成と登録が失敗した場合に message と slot を残さないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, inhibit_request_failure_leaves_no_slot)
{
    // Arrange
    cplat_service_definition def = make_run_def();
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_bus(k_dummy_bus);
    test_svc_linux_events_set_definition(&def);
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_bus_message_new_method_call(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(k_dummy_message), Return(-ENOMEM)));
    EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, k_dummy_message)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_systemd_, sd_bus_call_async(_, _, _, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_cplat_,
                cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("要求の作成に失敗しました")))
        .Times(1);

    // Act
    test_svc_linux_events_request_inhibit_async(0); // [手順] - sleep の lock を非同期で要求する。

    // Assert
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));        // [確認_異常系] - fd が -1 のままであること。
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_異常系] - slot が nullptr であること。
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_cplat_));
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    for (int fail_at = 0; fail_at < 4; fail_at++)
    {
        int seen = 0;
        EXPECT_CALL(mock_systemd_, sd_bus_message_new_method_call(_, _, _, _, _, _, _, _, _))
            .WillOnce(DoAll(SetArgPointee<4>(k_dummy_message), Return(0)));
        EXPECT_CALL(mock_systemd_, sd_bus_message_append_basic(_, _, _, k_dummy_message, 's', _))
            .Times(fail_at + 1)
            .WillRepeatedly(Invoke(
                [&seen, &fail_at](const char *, const int, const char *, sd_bus_message *, char, const void *) -> int
                {
                    int current = seen;
                    seen++;
                    return (current == fail_at) ? -EINVAL : 0;
                }));
        EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, k_dummy_message)).WillOnce(Return(nullptr));
        EXPECT_CALL(mock_systemd_, sd_bus_call_async(_, _, _, _, _, _, _, _, _)).Times(0);
        EXPECT_CALL(mock_cplat_,
                    cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("要求の作成に失敗しました")))
            .Times(1);

        test_svc_linux_events_request_inhibit_async(0);

        EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));        // [確認_異常系] - fd が -1 のままであること。
        EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_異常系] - slot が nullptr であること。
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_systemd_));
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&mock_cplat_));
        prevent_real_dbus(mock_systemd_);
        allow_any_trace();
    }

    {
        InSequence sequence;
        expect_inhibit_message_built(mock_systemd_);
        EXPECT_CALL(mock_systemd_,
                    sd_bus_call_async(_, _, _, k_dummy_bus, _, k_dummy_message, _, _, k_inhibit_timeout_usec))
            .WillOnce(DoAll(SetArgPointee<4>(k_dummy_slot), Return(-ENOMEM)));
        EXPECT_CALL(mock_systemd_, sd_bus_message_unref(_, _, _, k_dummy_message)).WillOnce(Return(nullptr));
        EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, k_dummy_slot)).WillOnce(Return(nullptr));
    }
    EXPECT_CALL(mock_cplat_,
                cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_WARNING, _, HasSubstr("非同期の要求に失敗しました")))
        .Times(1);

    test_svc_linux_events_request_inhibit_async(0);

    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));        // [確認_異常系] - fd が -1 のままであること。
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_異常系] - slot が nullptr であること。

    // Cleanup
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_definition(nullptr);
}

// サスペンド開始で応答待ちの slot を解除しその後にコールバックを実行しないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, suspend_start_cancels_pending_request_without_callback)
{
    // Arrange
    cplat_service_definition def = make_run_def();
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_bus(k_dummy_bus);
    test_svc_linux_events_set_definition(&def);
    test_svc_linux_events_set_inhibit_slot(0, k_dummy_slot);
    test_svc_linux_events_set_inhibit_fd(0, -1);
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, k_dummy_slot)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_systemd_, sd_bus_message_is_method_error(_, _, _, _, _)).Times(0);
    EXPECT_CALL(mock_systemd_, sd_bus_call_async(_, _, _, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(mock_cplat_, cplat_close(_, _)).Times(0);

    // Act
    test_svc_linux_events_handle_prepare_for_sleep(1); // [手順] - サスペンド開始 (starting != 0) を処理する。

    // Assert
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_正常系] - slot が nullptr であること。
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));        // [確認_正常系] - fd が -1 のままであること。

    // Cleanup
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_definition(nullptr);
}

// 応答待ちまたは fd 取得済みのとき復帰を繰り返しても要求を重ねないことの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, inhibit_request_is_not_duplicated)
{
    // Arrange
    cplat_service_definition def = make_run_def();
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_bus(k_dummy_bus);
    test_svc_linux_events_set_definition(&def);
    test_svc_linux_events_set_inhibit_slot(0, k_dummy_slot);
    test_svc_linux_events_set_inhibit_fd(0, -1);
    prevent_real_dbus(mock_systemd_);
    allow_any_trace();

    // Pre-Assert
    EXPECT_CALL(mock_systemd_, sd_bus_call_async(_, _, _, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(mock_systemd_, sd_bus_message_new_method_call(_, _, _, _, _, _, _, _, _)).Times(0);
    EXPECT_CALL(mock_cplat_, cplat_close(_, _)).Times(0);

    // Act
    test_svc_linux_events_handle_prepare_for_sleep(0); // [手順] - 応答待ちのまま復帰 (starting == 0) を処理する。

    // Assert
    EXPECT_EQ(k_dummy_slot, test_svc_linux_events_inhibit_slot(0)); // [確認_正常系] - 応答待ちの slot が残ること。
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));             // [確認_正常系] - fd が -1 のままであること。

    // Arrange_2
    test_svc_linux_events_set_inhibit_slot(0, nullptr);
    test_svc_linux_events_set_inhibit_fd(0, 8); // [状態] - 取得済み fd (8) を設定する。

    // Act_2
    test_svc_linux_events_handle_prepare_for_sleep(0); // [手順] - 取得済みのまま復帰を処理する。
    test_svc_linux_events_handle_prepare_for_sleep(0); // [手順] - 同じ復帰をもう一度処理する。

    // Assert_2
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_正常系] - 新しい slot を作らないこと。
    EXPECT_EQ(8, test_svc_linux_events_inhibit_fd(0));         // [確認_正常系] - 取得済みの fd が変わらないこと。

    // Cleanup
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_definition(nullptr);
}

// 解放のとき応答待ちの slot を先に解除し close が失敗しても fd を 1 回だけ閉じることの確認
// [サブ手順参照 名前=serviceLinuxRunTest.SetUp]
TEST_F(serviceLinuxRunTest, release_cancels_pending_requests_before_closing_fds)
{
    // Arrange
    test_svc_linux_events_reset_inhibit();
    test_svc_linux_events_set_inhibit_slot(0, k_dummy_slot);
    test_svc_linux_events_set_inhibit_fd(0, 7); // [状態] - 保持中の fd (7) を設定する。
    prevent_real_dbus(mock_systemd_);

    // Pre-Assert
    {
        InSequence sequence;
        EXPECT_CALL(mock_systemd_, sd_bus_slot_unref(_, _, _, k_dummy_slot))
            .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - fd を閉じる前に slot を 1 回 unref すること。
                                        // [Pre-Assert手順] - sd_bus_slot_unref() から nullptr を返す。
        EXPECT_CALL(mock_cplat_, cplat_close(7, _))
            .WillOnce(Return(
                CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_close() が 7 を指定して 1 回呼び出されること。
                                     // [Pre-Assert手順] - cplat_close() から CPLAT_ERR_UNKNOWN を返す。
    }

    // Act
    test_svc_linux_events_release_inhibit_locks(); // [手順] - すべての inhibitor lock を解放する。

    // Assert
    EXPECT_EQ(nullptr, test_svc_linux_events_inhibit_slot(0)); // [確認_異常系] - sleep の slot が nullptr であること。
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(0));        // [確認_異常系] - sleep の fd が -1 であること。
    EXPECT_EQ(nullptr,
              test_svc_linux_events_inhibit_slot(1));   // [確認_異常系] - shutdown の slot が nullptr であること。
    EXPECT_EQ(-1, test_svc_linux_events_inhibit_fd(1)); // [確認_異常系] - shutdown の fd が -1 のままであること。

    // Cleanup
    test_svc_linux_events_reset_inhibit();
}
