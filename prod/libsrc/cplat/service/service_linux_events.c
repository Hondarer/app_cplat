/**
 *******************************************************************************
 *  @file           service_linux_events.c
 *  @brief          Linux の OS イベントを監視するスレッドを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/28
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <assert.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <stdint.h>
    #include <string.h>
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
    #include <sys/signalfd.h>
    #include <time.h>
    #include <unistd.h>

    #include <systemd/sd-bus.h>
    #include <systemd/sd-daemon.h>
    #include <systemd/sd-event.h>

    #include <cplat/base/error.h>
    #include <cplat/base/error_message.h>
    #include <cplat/crt/stdio.h>
    #include <cplat/crt/unistd.h>
    #include <cplat/runtime/abort.h>
    #include <cplat/service/service.h>
    #include <cplat/service/service_internal.h>
    #include <cplat/service/service_linux_events.h>
    #include <cplat/sync/atomic.h>
    #include <cplat/sync/sync.h>

    /** logind の Inhibit の呼び出しの期限 [ms]。 */
    #define SVC_INHIBIT_CALL_TIMEOUT_MS 10000U

    /** 起動時に同期で呼ぶ Inhibit の回数 (sleep と shutdown)。 */
    #define SVC_INHIBIT_STARTUP_CALL_COUNT 2U

    /**
     *  systemd の既定の起動タイムアウト (DefaultTimeoutStartSec=) [ms]。
     *  see: https://www.freedesktop.org/software/systemd/man/latest/systemd-system.conf.html
     */
    #define SVC_SYSTEMD_DEFAULT_TIMEOUT_START_MS 90000U

    /**
     *  systemd の既定の停止タイムアウト (DefaultTimeoutStopSec=) [ms]。
     *  see: https://www.freedesktop.org/software/systemd/man/latest/systemd-system.conf.html
     */
    #define SVC_SYSTEMD_DEFAULT_TIMEOUT_STOP_MS 90000U

    /** イベント監視スレッド終了待ちの既定上限 [ms]。 */
    #define SVC_EVENTS_JOIN_TIMEOUT_MAX_MS 30000

    /** イベント監視スレッド終了待ちの動的調整下限 [ms]。 */
    #define SVC_EVENTS_JOIN_TIMEOUT_MIN_MS 1000

static_assert((SVC_INHIBIT_CALL_TIMEOUT_MS * SVC_INHIBIT_STARTUP_CALL_COUNT) < SVC_SYSTEMD_DEFAULT_TIMEOUT_START_MS,
              "inhibit calls at startup must finish before TimeoutStartSec");
static_assert(SVC_EVENTS_JOIN_TIMEOUT_MAX_MS < SVC_SYSTEMD_DEFAULT_TIMEOUT_STOP_MS,
              "join timeout must be shorter than TimeoutStopSec");
static_assert(SVC_EVENTS_JOIN_TIMEOUT_MAX_MS <= (CPLAT_SERVICE_DEFAULT_WATCHDOG_SEC * 1000U),
              "default join timeout must not exceed default WatchdogSec");

    /** watchdog の確認周期を求めるための除数 (期限の 1/4)。 */
    #define SVC_WATCHDOG_CHECK_DIVISOR 4U

static_assert(SVC_WATCHDOG_CHECK_DIVISOR >= 2U, "watchdog check interval must be at most half of WatchdogSec");

    /** watchdog のタイマー精度 [us] (0 は sd_event_add_time() の既定精度)。 */
    #define SVC_WATCHDOG_TIMER_ACCURACY_USEC 0U

    /** systemd-logind の D-Bus 接続先。 */
    #define LOGIND_SERVICE   "org.freedesktop.login1"
    #define LOGIND_OBJECT    "/org/freedesktop/login1"
    #define LOGIND_INTERFACE "org.freedesktop.login1.Manager"

    /** エラー メッセージ整形用バッファーの大きさ [byte]。 */
    #define ERR_MSG_BUFFER_SIZE 256U

typedef enum svc_linux_events_init_state
{
    SVC_LINUX_EVENTS_INIT_PENDING = 0,
    SVC_LINUX_EVENTS_INIT_SUCCEEDED = 1,
    SVC_LINUX_EVENTS_INIT_FAILED = 2
} svc_linux_events_init_state;

typedef enum svc_linux_events_phase
{
    SVC_LINUX_EVENTS_PHASE_STARTING = 0,
    SVC_LINUX_EVENTS_PHASE_RUNNING = 1,
    SVC_LINUX_EVENTS_PHASE_STOPPING = 2
} svc_linux_events_phase;

typedef struct svc_inhibit_lock
{
    const char *what;
    sd_bus_slot *slot;
    int fd;
    int pad;
} svc_inhibit_lock;

typedef struct svc_linux_events_ctx
{
    const cplat_service_definition *def;
    cplat_thread *thread;
    sd_event *event;
    sd_bus *bus;
    sd_event_source *stop_source;
    sd_event_source *reload_source;
    sd_event_source *watchdog_source;
    cplat_local_lock *init_lock;
    cplat_condvar *init_cv;
    cplat_local_lock *notify_lock;
    uint64_t watchdog_interval_usec;
    uint32_t watchdog_seen_count;
    int watchdog_warned;
    int init_state;
    int stop_fd;
    int join_timeout_ms;
    int pad;
    svc_inhibit_lock inhibit_sleep;
    svc_inhibit_lock inhibit_shutdown;
} svc_linux_events_ctx;

static void format_errno_message(const int err_num, char *buf, const size_t buf_size);
static void record_loop_failure(void);
static int build_inhibit_message(const char *what, sd_bus_message **message);
static int store_inhibit_fd(svc_inhibit_lock *lock, sd_bus_message *reply);
static void acquire_inhibit_lock_sync(svc_inhibit_lock *lock);
static int on_inhibit_reply(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void cancel_inhibit_request(svc_inhibit_lock *lock);
static void request_inhibit_lock_async(svc_inhibit_lock *lock);
static void release_inhibit_lock(svc_inhibit_lock *lock);
static void release_inhibit_locks(void);
static void dispatch_event_while_running(const cplat_service_event_info *info);
static void handle_prepare_for_sleep(int starting);
static void handle_prepare_for_shutdown(int starting);
static int on_prepare_for_sleep(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int on_prepare_for_shutdown(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int dispatch_session_signal(sd_bus_message *m, const cplat_service_event_type type);
static int on_session_new(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int on_session_removed(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static int on_stop_requested(sd_event_source *source, int fd, uint32_t revents, void *userdata);
static int on_reload_requested(sd_event_source *source, const struct signalfd_siginfo *info, void *userdata);
static int on_watchdog_timer(sd_event_source *source, uint64_t usec, void *userdata);
static int setup_watchdog_timer(void);
static void setup_bus_monitoring(void);
static void notify_init_state(const int state);
static void release_event_loop(void);
static int setup_required_sources(void);
static void events_thread_func(void *arg);
static void release_local_resources(void);

static svc_linux_events_ctx s_ctx = {NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     0U,
                                     0U,
                                     0,
                                     SVC_LINUX_EVENTS_INIT_PENDING,
                                     -1,
                                     SVC_EVENTS_JOIN_TIMEOUT_MAX_MS,
                                     0,
                                     {"sleep", NULL, -1, 0},
                                     {"shutdown", NULL, -1, 0}};

static cplat_atomic_i32 s_loop_failed = CPLAT_ATOMIC_INIT(0);
static svc_linux_events_phase s_phase = SVC_LINUX_EVENTS_PHASE_STARTING;

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_linux_events_loop_failed(void)
{
    return (cplat_atomic_load_i32(&s_loop_failed, CPLAT_MEMORY_ORDER_ACQUIRE) != 0) ? 1 : 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_linux_events_notify_lock(void)
{
    if (s_ctx.notify_lock != NULL)
    {
        cplat_local_lock_lock(s_ctx.notify_lock);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_linux_events_notify_unlock(void)
{
    if (s_ctx.notify_lock != NULL)
    {
        cplat_local_lock_unlock(s_ctx.notify_lock);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_linux_events_enter_running(void)
{
    if (s_phase == SVC_LINUX_EVENTS_PHASE_STARTING)
    {
        s_phase = SVC_LINUX_EVENTS_PHASE_RUNNING;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_linux_events_enter_stopping(void)
{
    s_phase = SVC_LINUX_EVENTS_PHASE_STOPPING;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_linux_events_is_running(void)
{
    return (s_phase == SVC_LINUX_EVENTS_PHASE_RUNNING) ? 1 : 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_linux_events_is_stopping(void)
{
    return (s_phase == SVC_LINUX_EVENTS_PHASE_STOPPING) ? 1 : 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_linux_events_start(const cplat_service_definition *def)
{
    int result;
    int init_state;

    if (def == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (s_ctx.thread != NULL)
    {
        return CPLAT_OK;
    }

    s_ctx.def = def;
    s_ctx.init_state = SVC_LINUX_EVENTS_INIT_PENDING;
    s_ctx.join_timeout_ms = SVC_EVENTS_JOIN_TIMEOUT_MAX_MS;
    cplat_atomic_store_i32(&s_loop_failed, 0, CPLAT_MEMORY_ORDER_RELEASE);

    if ((cplat_local_lock_create(&s_ctx.notify_lock) != CPLAT_OK) || (s_ctx.notify_lock == NULL))
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "通知ロックを生成できないため、イベント監視スレッドを起動しません。");
        release_local_resources();
        return CPLAT_ERR_UNKNOWN;
    }

    cplat_internal_service_linux_events_notify_lock();
    s_phase = SVC_LINUX_EVENTS_PHASE_STARTING;
    cplat_internal_service_linux_events_notify_unlock();

    if ((cplat_local_lock_create(&s_ctx.init_lock) != CPLAT_OK) || (cplat_condvar_create(&s_ctx.init_cv) != CPLAT_OK))
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "イベント監視スレッドの起動を待ち合わせるロックまたは条件変数を生成できません。");
        release_local_resources();
        return CPLAT_ERR_UNKNOWN;
    }

    s_ctx.stop_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (s_ctx.stop_fd < 0)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "停止指示用 eventfd の生成に失敗しました。");
        release_local_resources();
        return CPLAT_ERR_UNKNOWN;
    }

    result = cplat_thread_create(&s_ctx.thread, events_thread_func, NULL);
    if (result != CPLAT_OK)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "イベント監視スレッドの起動に失敗しました (結果コード: %d)。", result);
        s_ctx.thread = NULL;
        release_local_resources();
        return CPLAT_ERR_UNKNOWN;
    }

    /*
     * スレッドの必須の初期化の完了を無期限に待つ。
     * see: https://www.freedesktop.org/software/systemd/man/latest/sd_bus_call.html
     */
    cplat_local_lock_lock(s_ctx.init_lock);
    while (s_ctx.init_state == SVC_LINUX_EVENTS_INIT_PENDING)
    {
        cplat_condvar_wait(s_ctx.init_cv, s_ctx.init_lock);
    }
    init_state = s_ctx.init_state;
    cplat_local_lock_unlock(s_ctx.init_lock);

    if (init_state != SVC_LINUX_EVENTS_INIT_SUCCEEDED)
    {
        cplat_thread_join(s_ctx.thread);
        s_ctx.thread = NULL;
        release_local_resources();
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_linux_events_stop(void)
{
    int result;
    uint64_t value;
    ssize_t written;

    if (s_ctx.thread != NULL)
    {
        value = 1;
        written = write(s_ctx.stop_fd, &value, sizeof(value));
        (void)written;

        result = cplat_thread_timed_join(s_ctx.thread, s_ctx.join_timeout_ms);
        if (result != CPLAT_OK)
        {
            /*
             * スレッドが動いたまま終了を続けると、スレッドが解放済みのサービス定義とトレースを参照する。
             * 続行が危険なため、資源を解放せずにプロセスを異常終了させる。
             */
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "イベント監視スレッドが %d [ms] 以内に終了しないため、資源を解放せずに"
                                      "プロセスを異常終了します (結果コード: %d)。",
                                      s_ctx.join_timeout_ms, result);
            CPLAT_ABORT("イベント監視スレッドが時間内に終了しません");
        }

        s_ctx.thread = NULL;
    }

    release_local_resources();

    if (cplat_atomic_load_i32(&s_loop_failed, CPLAT_MEMORY_ORDER_ACQUIRE) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}

/*
 * ============================================================
 *  内部ヘルパー
 * ============================================================
 */

static void format_errno_message(const int err_num, char *buf, const size_t buf_size)
{
    cplat_error err;
    cplat_error_capture_errno(&err, err_num);
    if (cplat_error_message(buf, buf_size, &err) != CPLAT_OK)
    {
        (void)cplat_snprintf(buf, buf_size, "errno=%d", err_num);
    }
}

static void record_loop_failure(void)
{
    cplat_atomic_store_i32(&s_loop_failed, 1, CPLAT_MEMORY_ORDER_RELEASE);
    cplat_service_request_stop();
}

static int build_inhibit_message(const char *what, sd_bus_message **message)
{
    const char *reason = "イベント コールバックの実行猶予を確保するため";
    const char *mode = "delay";
    const char *service_name;
    int rc;

    *message = NULL;
    rc = sd_bus_message_new_method_call(s_ctx.bus, message, LOGIND_SERVICE, LOGIND_OBJECT, LOGIND_INTERFACE, "Inhibit");
    if (rc < 0)
    {
        if (*message != NULL)
        {
            (void)sd_bus_message_unref(*message);
            *message = NULL;
        }
        return rc;
    }

    service_name = s_ctx.def->name;
    rc = sd_bus_message_append_basic(*message, 's', &what);
    if (rc >= 0)
    {
        rc = sd_bus_message_append_basic(*message, 's', &service_name);
    }
    if (rc >= 0)
    {
        rc = sd_bus_message_append_basic(*message, 's', &reason);
    }
    if (rc >= 0)
    {
        rc = sd_bus_message_append_basic(*message, 's', &mode);
    }

    if (rc < 0)
    {
        (void)sd_bus_message_unref(*message);
        *message = NULL;
        return rc;
    }

    return CPLAT_OK;
}

static int store_inhibit_fd(svc_inhibit_lock *lock, sd_bus_message *reply)
{
    int fd;
    int lock_fd;

    if (lock->fd != -1)
    {
        return CPLAT_OK;
    }

    fd = -1;
    if (sd_bus_message_read_basic(reply, 'h', &fd) <= 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /*
     * fd は reply メッセージが所有するため、複製して保持する。
     * see: https://man7.org/linux/man-pages/man2/fcntl.2.html
     */
    lock_fd = fcntl(fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
    if (lock_fd < 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    lock->fd = lock_fd;
    return CPLAT_OK;
}

static void acquire_inhibit_lock_sync(svc_inhibit_lock *lock)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus_message *reply = NULL;
    char err_msg[ERR_MSG_BUFFER_SIZE];
    int rc;

    lock->fd = -1;
    rc = build_inhibit_message(lock->what, &m);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "inhibitor lock (%s) の要求の作成に失敗しました: %s", lock->what, err_msg);
    }
    else
    {
        rc = sd_bus_call(s_ctx.bus, m, (uint64_t)SVC_INHIBIT_CALL_TIMEOUT_MS * 1000ULL, &error, &reply);
        if (rc < 0)
        {
            format_errno_message(-rc, err_msg, sizeof(err_msg));
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                      "inhibitor lock (%s) の取得に失敗しました: %s", lock->what, err_msg);
        }
        else if (store_inhibit_fd(lock, reply) != CPLAT_OK)
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                      "inhibitor lock (%s) の fd を保存できませんでした。", lock->what);
        }
    }

    if (m != NULL)
    {
        (void)sd_bus_message_unref(m);
    }
    if (reply != NULL)
    {
        (void)sd_bus_message_unref(reply);
    }
    sd_bus_error_free(&error);
}

static int on_inhibit_reply(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    svc_inhibit_lock *lock = (svc_inhibit_lock *)userdata;
    const sd_bus_error *bus_error;
    const char *error_name;

    if (lock->slot != NULL)
    {
        lock->slot = sd_bus_slot_unref(lock->slot);
    }

    (void)ret_error;

    if (sd_bus_message_is_method_error(m, NULL) != 0)
    {
        bus_error = sd_bus_message_get_error(m);
        error_name = ((bus_error != NULL) && (bus_error->name != NULL)) ? bus_error->name : "";
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "inhibitor lock (%s) の応答がエラーです: %s", lock->what, error_name);
        return 0;
    }

    if (store_inhibit_fd(lock, m) != CPLAT_OK)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "inhibitor lock (%s) の fd を保存できませんでした。", lock->what);
    }

    return 0;
}

static void cancel_inhibit_request(svc_inhibit_lock *lock)
{
    if (lock->slot != NULL)
    {
        lock->slot = sd_bus_slot_unref(lock->slot);
    }
}

static void request_inhibit_lock_async(svc_inhibit_lock *lock)
{
    sd_bus_message *m = NULL;
    char err_msg[ERR_MSG_BUFFER_SIZE];
    int rc;

    if ((lock->fd >= 0) || (lock->slot != NULL))
    {
        return;
    }

    rc = build_inhibit_message(lock->what, &m);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "inhibitor lock (%s) の要求の作成に失敗しました: %s", lock->what, err_msg);
        return;
    }

    rc = sd_bus_call_async(s_ctx.bus, &lock->slot, m, on_inhibit_reply, lock,
                           (uint64_t)SVC_INHIBIT_CALL_TIMEOUT_MS * 1000ULL);
    (void)sd_bus_message_unref(m);

    if (rc < 0)
    {
        if (lock->slot != NULL)
        {
            lock->slot = sd_bus_slot_unref(lock->slot);
        }
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "inhibitor lock (%s) の非同期の要求に失敗しました: %s", lock->what, err_msg);
    }
}

static void release_inhibit_lock(svc_inhibit_lock *lock)
{
    cancel_inhibit_request(lock);

    if (lock->fd >= 0)
    {
        /*
         * Linux の close は失敗しても fd を解放して再試行できないため、戻り値は明示的に破棄する。
         * see: https://man7.org/linux/man-pages/man2/close.2.html
         */
        (void)cplat_close(lock->fd, NULL);
        lock->fd = -1;
    }
}

static void release_inhibit_locks(void)
{
    release_inhibit_lock(&s_ctx.inhibit_sleep);
    release_inhibit_lock(&s_ctx.inhibit_shutdown);
}

static void dispatch_event_while_running(const cplat_service_event_info *info)
{
    int running;

    cplat_internal_service_linux_events_notify_lock();
    running = cplat_internal_service_linux_events_is_running();
    cplat_internal_service_linux_events_notify_unlock();

    if (running == 0)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                  "動作中ではないため OS イベントを配送しません (種別: %d)。", (int)info->type);
        return;
    }

    cplat_internal_service_dispatch_event(s_ctx.def, info);
}

static void handle_prepare_for_sleep(int starting)
{
    cplat_service_event_info info;

    info.pad = 0U;
    info.session_id = NULL;

    if (starting != 0)
    {
        cancel_inhibit_request(&s_ctx.inhibit_sleep);
        info.type = CPLAT_SERVICE_EVENT_POWER_SUSPEND;
        dispatch_event_while_running(&info);

        if (s_ctx.inhibit_sleep.fd >= 0)
        {
            (void)cplat_close(s_ctx.inhibit_sleep.fd, NULL);
            s_ctx.inhibit_sleep.fd = -1;
        }
    }
    else
    {
        info.type = CPLAT_SERVICE_EVENT_POWER_RESUME;
        dispatch_event_while_running(&info);
        request_inhibit_lock_async(&s_ctx.inhibit_sleep);
    }
}

static void handle_prepare_for_shutdown(int starting)
{
    cplat_service_event_info info;

    info.pad = 0U;
    info.session_id = NULL;

    if (starting != 0)
    {
        cancel_inhibit_request(&s_ctx.inhibit_shutdown);
        info.type = CPLAT_SERVICE_EVENT_PRESHUTDOWN;
        dispatch_event_while_running(&info);

        if (s_ctx.inhibit_shutdown.fd >= 0)
        {
            (void)cplat_close(s_ctx.inhibit_shutdown.fd, NULL);
            s_ctx.inhibit_shutdown.fd = -1;
        }
    }
    else
    {
        request_inhibit_lock_async(&s_ctx.inhibit_shutdown);
    }
}

static int on_prepare_for_sleep(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    int starting;

    (void)userdata;
    (void)ret_error;

    if (sd_bus_message_read(m, "b", &starting) < 0)
    {
        return 0;
    }

    handle_prepare_for_sleep(starting);
    return 0;
}

static int on_prepare_for_shutdown(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    int starting;

    (void)userdata;
    (void)ret_error;

    if (sd_bus_message_read(m, "b", &starting) < 0)
    {
        return 0;
    }

    handle_prepare_for_shutdown(starting);
    return 0;
}

static int dispatch_session_signal(sd_bus_message *m, const cplat_service_event_type type)
{
    const char *session_id = NULL;
    const char *object_path = NULL;
    cplat_service_event_info info;

    if (sd_bus_message_read(m, "so", &session_id, &object_path) < 0)
    {
        return 0;
    }

    info.type = type;
    info.pad = 0U;
    info.session_id = session_id;
    dispatch_event_while_running(&info);
    return 0;
}

static int on_session_new(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    (void)userdata;
    (void)ret_error;
    return dispatch_session_signal(m, CPLAT_SERVICE_EVENT_SESSION_LOGON);
}

static int on_session_removed(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    (void)userdata;
    (void)ret_error;
    return dispatch_session_signal(m, CPLAT_SERVICE_EVENT_SESSION_LOGOFF);
}

static int on_stop_requested(sd_event_source *source, int fd, uint32_t revents, void *userdata)
{
    uint64_t value;
    ssize_t bytes;

    (void)revents;
    (void)userdata;

    bytes = read(fd, &value, sizeof(value));
    (void)bytes;

    (void)sd_event_exit(sd_event_source_get_event(source), 0);
    return 0;
}

static int on_reload_requested(sd_event_source *source, const struct signalfd_siginfo *info, void *userdata)
{
    int running;

    (void)source;
    (void)info;
    (void)userdata;

    if (s_ctx.def->on_reload == NULL)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                 "on_reload が設定されていないため、SIGHUP を無視します。");
        return 0;
    }

    cplat_internal_service_linux_events_notify_lock();
    running = cplat_internal_service_linux_events_is_running();
    cplat_internal_service_linux_events_notify_unlock();

    if (running == 0)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                 "動作中ではないため設定再読込要求を配送しません。");
        return 0;
    }

    cplat_internal_service_dispatch_reload(s_ctx.def);
    return 0;
}

static int on_watchdog_timer(sd_event_source *source, uint64_t usec, void *userdata)
{
    int rc;
    int running;
    int progressed;
    int stop_requested;
    int trace_stall_allowed;
    int notify_result = 0;
    int trace_stall = 0;
    char err_msg[ERR_MSG_BUFFER_SIZE];

    (void)userdata;

    progressed = cplat_internal_service_on_run_made_progress(&s_ctx.watchdog_seen_count);
    stop_requested = cplat_service_stop_requested();
    trace_stall_allowed = (s_ctx.watchdog_warned == 0) ? 1 : 0;

    cplat_internal_service_linux_events_notify_lock();
    running = cplat_internal_service_linux_events_is_running();

    if ((running != 0) && (stop_requested == 0))
    {
        if (progressed != 0)
        {
            notify_result = cplat_internal_service_linux_sd_notify_send("WATCHDOG=1");
        }
        else if (trace_stall_allowed != 0)
        {
            trace_stall = 1;
        }
    }
    cplat_internal_service_linux_events_notify_unlock();

    if (trace_stall != 0)
    {
        s_ctx.watchdog_warned = 1;
        (void)cplat_tracer_write(
            cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
            "on_run が cplat_service_wait_for_stop() を呼んでいないため、WATCHDOG=1 を送りません。");
    }
    else if (notify_result < 0)
    {
        format_errno_message(-notify_result, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "WATCHDOG=1 の送信に失敗しました: %s", err_msg);
    }

    rc = sd_event_source_set_time(source, usec + s_ctx.watchdog_interval_usec);
    if (rc >= 0)
    {
        rc = sd_event_source_set_enabled(source, SD_EVENT_ONESHOT);
    }

    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "watchdog のタイマーを再設定できないため、サービスを停止します: %s", err_msg);
        record_loop_failure();
    }

    return 0;
}

static int setup_watchdog_timer(void)
{
    uint64_t watchdog_usec = 0U;
    uint64_t now = 0U;
    char err_msg[ERR_MSG_BUFFER_SIZE];
    int rc;

    rc = sd_watchdog_enabled(0, &watchdog_usec);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "watchdog の設定を読み取れません: %s", err_msg);
        return -1;
    }

    if (rc == 0)
    {
        return 0;
    }

    /*
     * WATCHDOG_USEC が 30 [s] 未満のときは、停止時の join 待機上限を
     * min(30000 [ms], watchdog_usec / 1000 [ms]) (下限 1000 [ms]) へ自動調整する。
     */
    if (watchdog_usec < ((uint64_t)SVC_EVENTS_JOIN_TIMEOUT_MAX_MS * 1000ULL))
    {
        uint64_t watchdog_ms = watchdog_usec / 1000ULL;
        if (watchdog_ms < (uint64_t)SVC_EVENTS_JOIN_TIMEOUT_MIN_MS)
        {
            watchdog_ms = (uint64_t)SVC_EVENTS_JOIN_TIMEOUT_MIN_MS;
        }
        s_ctx.join_timeout_ms = (int)watchdog_ms;
    }

    s_ctx.watchdog_interval_usec = watchdog_usec / SVC_WATCHDOG_CHECK_DIVISOR;
    s_ctx.watchdog_warned = 0;

    s_ctx.watchdog_seen_count = 0U;
    (void)cplat_internal_service_on_run_made_progress(&s_ctx.watchdog_seen_count);

    rc = sd_event_now(s_ctx.event, CLOCK_MONOTONIC, &now);
    if (rc >= 0)
    {
        rc = sd_event_add_time(s_ctx.event, &s_ctx.watchdog_source, CLOCK_MONOTONIC, now + s_ctx.watchdog_interval_usec,
                               SVC_WATCHDOG_TIMER_ACCURACY_USEC, on_watchdog_timer, NULL);
    }

    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "watchdog のタイマーを登録できません: %s", err_msg);
        s_ctx.watchdog_source = NULL;
        return -1;
    }

    return 0;
}

static void setup_bus_monitoring(void)
{
    char err_msg[ERR_MSG_BUFFER_SIZE];
    int rc;

    rc = sd_bus_open_system(&s_ctx.bus);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "D-Bus (system bus) に接続できないため電源・セッション イベントは無効です: %s",
                                  err_msg);
        s_ctx.bus = NULL;
        return;
    }

    rc = sd_bus_match_signal(s_ctx.bus, NULL, LOGIND_SERVICE, LOGIND_OBJECT, LOGIND_INTERFACE, "PrepareForSleep",
                             on_prepare_for_sleep, NULL);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "PrepareForSleep の購読に失敗しました: %s", err_msg);
    }

    rc = sd_bus_match_signal(s_ctx.bus, NULL, LOGIND_SERVICE, LOGIND_OBJECT, LOGIND_INTERFACE, "PrepareForShutdown",
                             on_prepare_for_shutdown, NULL);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "PrepareForShutdown の購読に失敗しました: %s", err_msg);
    }

    rc = sd_bus_match_signal(s_ctx.bus, NULL, LOGIND_SERVICE, LOGIND_OBJECT, LOGIND_INTERFACE, "SessionNew",
                             on_session_new, NULL);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "SessionNew の購読に失敗しました: %s", err_msg);
    }

    rc = sd_bus_match_signal(s_ctx.bus, NULL, LOGIND_SERVICE, LOGIND_OBJECT, LOGIND_INTERFACE, "SessionRemoved",
                             on_session_removed, NULL);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "SessionRemoved の購読に失敗しました: %s", err_msg);
    }

    acquire_inhibit_lock_sync(&s_ctx.inhibit_sleep);
    acquire_inhibit_lock_sync(&s_ctx.inhibit_shutdown);

    rc = sd_bus_attach_event(s_ctx.bus, s_ctx.event, SD_EVENT_PRIORITY_NORMAL);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "D-Bus をイベント ループに接続できないため電源・セッション イベントは無効です: %s",
                                  err_msg);
        release_inhibit_locks();
        sd_bus_flush_close_unref(s_ctx.bus);
        s_ctx.bus = NULL;
    }
}

static void notify_init_state(const int state)
{
    cplat_local_lock_lock(s_ctx.init_lock);
    s_ctx.init_state = state;
    cplat_condvar_broadcast(s_ctx.init_cv);
    cplat_local_lock_unlock(s_ctx.init_lock);
}

static void release_event_loop(void)
{
    if (s_ctx.watchdog_source != NULL)
    {
        s_ctx.watchdog_source = sd_event_source_unref(s_ctx.watchdog_source);
    }
    if (s_ctx.reload_source != NULL)
    {
        s_ctx.reload_source = sd_event_source_unref(s_ctx.reload_source);
    }
    if (s_ctx.stop_source != NULL)
    {
        s_ctx.stop_source = sd_event_source_unref(s_ctx.stop_source);
    }
    if (s_ctx.event != NULL)
    {
        s_ctx.event = sd_event_unref(s_ctx.event);
    }
}

static int setup_required_sources(void)
{
    char err_msg[ERR_MSG_BUFFER_SIZE];
    int rc;

    rc = sd_event_new(&s_ctx.event);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "sd_event の生成に失敗しました: %s", err_msg);
        s_ctx.event = NULL;
        return -1;
    }

    if (setup_watchdog_timer() != 0)
    {
        return -1;
    }

    rc = sd_event_add_io(s_ctx.event, &s_ctx.stop_source, s_ctx.stop_fd, EPOLLIN, on_stop_requested, NULL);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "停止監視の登録に失敗しました: %s", err_msg);
        s_ctx.stop_source = NULL;
        return -1;
    }

    rc = sd_event_source_set_priority(s_ctx.stop_source, SD_EVENT_PRIORITY_IMPORTANT);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "停止監視の優先度を設定できませんでした: %s", err_msg);
    }

    rc = sd_event_add_signal(s_ctx.event, &s_ctx.reload_source, SIGHUP, on_reload_requested, NULL);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "reload 監視の登録に失敗しました: %s", err_msg);
        s_ctx.reload_source = NULL;
        return -1;
    }

    return 0;
}

static void events_thread_func(void *arg)
{
    char err_msg[ERR_MSG_BUFFER_SIZE];
    int rc;

    (void)arg;

    if (setup_required_sources() != 0)
    {
        release_event_loop();
        notify_init_state(SVC_LINUX_EVENTS_INIT_FAILED);
        return;
    }

    if (s_ctx.def->on_event != NULL)
    {
        setup_bus_monitoring();
    }

    notify_init_state(SVC_LINUX_EVENTS_INIT_SUCCEEDED);

    rc = sd_event_loop(s_ctx.event);
    if (rc < 0)
    {
        format_errno_message(-rc, err_msg, sizeof(err_msg));
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "イベント ループがエラーで終了したため、サービスを停止します: %s", err_msg);
        record_loop_failure();
    }

    release_inhibit_locks();

    if (s_ctx.bus != NULL)
    {
        sd_bus_flush_close_unref(s_ctx.bus);
        s_ctx.bus = NULL;
    }

    release_event_loop();
}

static void release_local_resources(void)
{
    if (s_ctx.stop_fd >= 0)
    {
        (void)cplat_close(s_ctx.stop_fd, NULL);
        s_ctx.stop_fd = -1;
    }

    if (s_ctx.init_cv != NULL)
    {
        cplat_condvar_dispose(s_ctx.init_cv);
        s_ctx.init_cv = NULL;
    }

    if (s_ctx.init_lock != NULL)
    {
        cplat_local_lock_dispose(s_ctx.init_lock);
        s_ctx.init_lock = NULL;
    }

    if (s_ctx.notify_lock != NULL)
    {
        cplat_local_lock_dispose(s_ctx.notify_lock);
        s_ctx.notify_lock = NULL;
    }

    s_ctx.def = NULL;
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif
