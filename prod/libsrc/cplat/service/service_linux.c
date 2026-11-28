/**
 *******************************************************************************
 *  @file           service_linux.c
 *  @brief          クロスプラットフォーム サービス基盤の Linux 向け処理を実装します。
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
    #include <errno.h>
    #include <pthread.h>
    #include <signal.h>
    #include <stddef.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>

    #include <systemd/sd-daemon.h>

    #include <cplat/base/error.h>
    #include <cplat/base/error_internal.h>
    #include <cplat/base/error_message.h>
    #include <cplat/crt/stdio.h>
    #include <cplat/runtime/elevated_process.h>
    #include <cplat/runtime/process.h>
    #include <cplat/service/service.h>
    #include <cplat/service/service_internal.h>
    #include <cplat/service/service_linux_events.h>

    /** systemd ユニット ファイルのインストール先ディレクトリ。 */
    #define SYSTEMD_UNIT_DIR "/etc/systemd/system"

    /** ManagedOOMPreference= が導入された systemd のバージョン。 */
    #define SYSTEMD_MANAGED_OOM_PREFERENCE_VERSION 248

    /** systemctl --version の 1 行目を読む配列の大きさ [byte] (終端 NUL を含む)。 */
    #define SYSTEMCTL_VERSION_LINE_SIZE 128U

    /** sd_notify の通知文を組み立てる配列の大きさ [byte] (終端 NUL を含む)。 */
    #define SD_NOTIFY_MESSAGE_SIZE 512U

    /** サービス名を複製する配列の大きさ [byte] (終端 NUL を含む)。 */
    #define SVC_NAME_BUFFER_SIZE 256U

    /** ユニット ファイルのパスを格納する配列の大きさ [byte] (終端 NUL を含む)。 */
    #define SYSTEMD_UNIT_PATH_SIZE 512U

    /** ユニット ファイルの内容のうち、実行ファイル パス以外の分の大きさ [byte]。 */
    #define UNIT_CONTENT_EXTRA_BYTES 2048U

    /** エラー メッセージ整形用バッファーの大きさ [byte]。 */
    #define ERR_MSG_BUFFER_SIZE 256U

static_assert((sizeof(SYSTEMD_UNIT_DIR) - 1U) + 1U + (SVC_NAME_BUFFER_SIZE - 1U) + (sizeof(".service") - 1U) + 1U <=
                  SYSTEMD_UNIT_PATH_SIZE,
              "unit path buffer must hold SYSTEMD_UNIT_DIR and the longest service name");

static_assert(((CPLAT_SERVICE_DEFAULT_START_LIMIT_BURST - 1U) * CPLAT_SERVICE_DEFAULT_RESTART_DELAY_SEC) <
                  CPLAT_SERVICE_DEFAULT_START_LIMIT_INTERVAL_SEC,
              "default start limit must be reachable with default restart delay");

static int validate_start_limit(const unsigned int burst, const unsigned int delay_sec,
                                const unsigned int interval_sec);
static int check_root_privilege(const char *operation_name);
static int run_command(char *const argv[]);
static int parse_systemd_major_version(const char *version_line);
static int get_systemd_major_version(void);
static void format_errno_message(const int err_num, char *buf, const size_t buf_size);
static void trace_notify_failure(const char *message, const int result);

/*
 * ============================================================
 *  sd_notify ヘルパー
 * ============================================================
 */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_linux_sd_notify_send(const char *message)
{
    return sd_notify(0, message);
}

/*
 * ============================================================
 *  OS フック実装
 * ============================================================
 */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_prepare_run(void)
{
    sigset_t set;
    int rc;

    /*
     * 有効なシグナル番号 SIGHUP を渡すため、sigemptyset と sigaddset は失敗しない。
     * see: https://man7.org/linux/man-pages/man3/sigemptyset.3p.html
     * see: https://man7.org/linux/man-pages/man3/sigaddset.3p.html
     */
    (void)sigemptyset(&set);
    (void)sigaddset(&set, SIGHUP);

    /*
     * 失敗時は errno ではなくエラー番号を返す。
     * ブロックしたマスクはプロセス終了まで戻さず、fork と execve を越えて子へ引き継がれる。
     * see: https://man7.org/linux/man-pages/man3/pthread_sigmask.3.html
     * see: https://man7.org/linux/man-pages/man7/signal.7.html
     */
    rc = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (rc != 0)
    {
        return cplat_internal_error_report_errno(NULL, rc);
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_ready(void)
{
    int result = 0;
    int suppressed = 0;

    cplat_internal_service_linux_events_notify_lock();
    if ((cplat_internal_service_linux_events_loop_failed() != 0) ||
        (cplat_internal_service_linux_events_is_stopping() != 0))
    {
        suppressed = 1;
    }
    else
    {
        result = cplat_internal_service_linux_sd_notify_send("READY=1");
        cplat_internal_service_linux_events_enter_running();
    }
    cplat_internal_service_linux_events_notify_unlock();

    if (suppressed != 0)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                 "イベント ループの異常終了または停止中のため、READY=1 を送りません。");
    }
    else
    {
        trace_notify_failure("READY=1", result);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_stopping(void)
{
    int result;

    cplat_internal_service_linux_events_notify_lock();
    cplat_internal_service_linux_events_enter_stopping();
    result = cplat_internal_service_linux_sd_notify_send("STOPPING=1");
    cplat_internal_service_linux_events_notify_unlock();

    trace_notify_failure("STOPPING=1", result);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_reloading(void)
{
    int result = 0;
    int suppressed = 0;

    cplat_internal_service_linux_events_notify_lock();
    if (cplat_internal_service_linux_events_is_running() == 0)
    {
        suppressed = 1;
    }
    else
    {
        result = cplat_internal_service_linux_sd_notify_send("RELOADING=1");
    }
    cplat_internal_service_linux_events_notify_unlock();

    if (suppressed != 0)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                 "動作中ではないため、RELOADING=1 を送りません。");
    }
    else
    {
        trace_notify_failure("RELOADING=1", result);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_status(const char *text)
{
    char message[SD_NOTIFY_MESSAGE_SIZE];

    if (text == NULL)
    {
        return;
    }

    /*
     * cplat_snprintf は切り詰めで空文字にするため使わない。
     * 状態の通知文は長すぎる場合の切り詰めを許容するため、戻り値は明示的に破棄する。
     */
    (void)snprintf(message, sizeof(message), "STATUS=%s", text); /* 置換対象外: 通知文の意図的な切り詰め */
    /* STATUS 通知は補助情報であり、失敗でサービス状態を変えないため、戻り値は明示的に破棄する。 */
    (void)cplat_internal_service_linux_sd_notify_send(message);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_run(const cplat_service_definition *def, int *exit_code_out)
{
    int ret;
    int stop_ret;

    ret = cplat_internal_service_linux_events_start(def);
    if (ret != CPLAT_OK)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "イベント監視スレッドを起動できないため、サービスを開始しません。");
        return CPLAT_ERR_UNKNOWN;
    }

    /*
     * Type=notify のため fork せず、フォアグラウンドのまま常駐する。
     * shutdown.h が SIGTERM / SIGINT を捕捉して cplat_service_request_stop() を呼び出す。
     */
    ret = cplat_internal_service_run_lifecycle(def, exit_code_out);

    stop_ret = cplat_internal_service_linux_events_stop();
    if (ret == CPLAT_OK && stop_ret != CPLAT_OK)
    {
        ret = CPLAT_ERR_UNKNOWN;
    }

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_install(const cplat_service_definition *def, const cplat_service_install_options *options)
{
    char exec_path[CPLAT_SERVICE_EXEC_PATH_MAX];
    char unit_path[SYSTEMD_UNIT_PATH_SIZE];
    char unit_content[CPLAT_SERVICE_EXEC_PATH_MAX + UNIT_CONTENT_EXTRA_BYTES];
    char svc_name_buf[SVC_NAME_BUFFER_SIZE];
    char start_limit_section[128];
    char restart_section[128];
    char watchdog_section[64];
    char oom_section[128];
    char exec_start_line[CPLAT_SERVICE_EXEC_PATH_MAX + 512U];
    char err_msg[ERR_MSG_BUFFER_SIZE];
    const char *run_args;
    const char *description;
    const char *exec_reload_line;
    unsigned int restart_delay_sec;
    unsigned int start_limit_interval_sec;
    unsigned int start_limit_burst;
    unsigned int watchdog_sec;
    int disable_auto_restart;
    int disable_watchdog;
    int disable_oom_protection;
    FILE *fp;
    cplat_error err;
    char *argv_daemon_reload[] = {"systemctl", "daemon-reload", NULL};
    char *argv_enable[] = {"systemctl", "enable", svc_name_buf, NULL};
    int ret;
    int cmd_rc;

    run_args = (options != NULL && options->run_args != NULL) ? options->run_args : "run";
    restart_delay_sec = (options != NULL && options->restart_delay_sec != 0U) ? options->restart_delay_sec
                                                                              : CPLAT_SERVICE_DEFAULT_RESTART_DELAY_SEC;
    start_limit_interval_sec = (options != NULL && options->start_limit_interval_sec != 0U)
                                   ? options->start_limit_interval_sec
                                   : CPLAT_SERVICE_DEFAULT_START_LIMIT_INTERVAL_SEC;
    start_limit_burst = (options != NULL && options->start_limit_burst != 0U) ? options->start_limit_burst
                                                                              : CPLAT_SERVICE_DEFAULT_START_LIMIT_BURST;
    watchdog_sec =
        (options != NULL && options->watchdog_sec != 0U) ? options->watchdog_sec : CPLAT_SERVICE_DEFAULT_WATCHDOG_SEC;
    disable_auto_restart = (options != NULL) ? options->disable_auto_restart : 0;
    disable_watchdog = (options != NULL) ? options->disable_watchdog : 0;
    disable_oom_protection = (options != NULL) ? options->disable_oom_protection : 0;
    description = (def->description != NULL) ? def->description : "";

    if (disable_auto_restart == 0)
    {
        ret = validate_start_limit(start_limit_burst, restart_delay_sec, start_limit_interval_sec);
        if (ret != CPLAT_OK)
        {
            /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
            (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                     "StartLimit 設定が再起動遅延に対して到達不能です。");
            return ret;
        }
    }

    if (cplat_snprintf(svc_name_buf, sizeof(svc_name_buf), "%s", def->name) != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "サービス名が長すぎます。");
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    ret = check_root_privilege("install");
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    ret = cplat_process_get_executable_path(exec_path, sizeof(exec_path));
    if (ret != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "実行ファイルのパスを取得できませんでした。");
        return ret;
    }

    if (run_args[0] == '\0')
    {
        ret = cplat_snprintf(exec_start_line, sizeof(exec_start_line), "ExecStart=%s\n", exec_path);
    }
    else
    {
        ret = cplat_snprintf(exec_start_line, sizeof(exec_start_line), "ExecStart=%s %s\n", exec_path, run_args);
    }
    if (ret != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "ExecStart 行が長すぎます。");
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    start_limit_section[0] = '\0';
    restart_section[0] = '\0';
    if (disable_auto_restart == 0)
    {
        /*
         * 設定は systemd 229 以降の [Unit] に置く。
         * see: https://www.freedesktop.org/software/systemd/man/latest/systemd.unit.html
         */
        (void)cplat_snprintf(start_limit_section, sizeof(start_limit_section),
                             "StartLimitIntervalSec=%u\nStartLimitBurst=%u\n", start_limit_interval_sec,
                             start_limit_burst);
        (void)cplat_snprintf(restart_section, sizeof(restart_section), "Restart=on-failure\nRestartSec=%u\n",
                             restart_delay_sec);
    }

    watchdog_section[0] = '\0';
    if (disable_watchdog == 0)
    {
        (void)cplat_snprintf(watchdog_section, sizeof(watchdog_section), "WatchdogSec=%u\n", watchdog_sec);
    }

    oom_section[0] = '\0';
    if (disable_oom_protection == 0)
    {
        int systemd_major_version = get_systemd_major_version();
        if (systemd_major_version >= SYSTEMD_MANAGED_OOM_PREFERENCE_VERSION)
        {
            (void)cplat_snprintf(oom_section, sizeof(oom_section), "OOMScoreAdjust=-1000\nManagedOOMPreference=omit\n");
        }
        else
        {
            if (systemd_major_version < 0)
            {
                (void)cplat_tracer_write(
                    cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                    "systemd のバージョンを取得できないため ManagedOOMPreference=omit は設定しません。");
            }
            else
            {
                (void)cplat_tracer_writef(
                    cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                    "systemd %d は ManagedOOMPreference= に未対応のため ManagedOOMPreference=omit は設定しません。",
                    systemd_major_version);
            }
            (void)cplat_snprintf(oom_section, sizeof(oom_section), "OOMScoreAdjust=-1000\n");
        }
    }

    exec_reload_line = (def->on_reload != NULL) ? "ExecReload=/bin/kill -HUP $MAINPID\n" : "";

    if (cplat_snprintf(unit_path, sizeof(unit_path), "%s/%s.service", SYSTEMD_UNIT_DIR, def->name) != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "ユニット ファイルのパスが長すぎます。");
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    if (cplat_snprintf(unit_content, sizeof(unit_content),
                       "[Unit]\n"
                       "Description=%s\n"
                       "After=network.target\n"
                       "%s"
                       "\n"
                       "[Service]\n"
                       "Type=notify\n"
                       "%s"
                       "%s"
                       "%s"
                       "%s"
                       "%s"
                       "\n"
                       "[Install]\n"
                       "WantedBy=multi-user.target\n",
                       description, start_limit_section, exec_start_line, exec_reload_line, restart_section,
                       watchdog_section, oom_section) != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "ユニット ファイルの内容が長すぎます。");
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    cplat_error_clear(&err);
    fp = cplat_fopen(unit_path, "w", &err);
    if (fp == NULL)
    {
        (void)cplat_error_message(err_msg, sizeof(err_msg), &err);
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "%s を開けませんでした: %s", unit_path, err_msg);
        return cplat_error_to_result(&err);
    }

    if (fputs(unit_content, fp) == EOF)
    {
        int saved_errno = errno;
        ret = cplat_internal_error_report_errno(&err, saved_errno);
        (void)cplat_error_message(err_msg, sizeof(err_msg), &err);
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "%s への書き込みに失敗しました: %s", unit_path, err_msg);
        (void)cplat_fclose(fp, NULL);
        cplat_error_set_last(&err);
        return ret;
    }

    if (cplat_fclose(fp, &err) == EOF)
    {
        (void)cplat_error_message(err_msg, sizeof(err_msg), &err);
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "%s への書き込みに失敗しました: %s", unit_path, err_msg);
        return cplat_error_to_result(&err);
    }

    (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                              "ユニット ファイルを書き込みました: %s", unit_path);

    cmd_rc = run_command(argv_daemon_reload);
    if (cmd_rc != 0)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "systemctl daemon-reload が失敗しました (終了コード %d)。", cmd_rc);
        return CPLAT_ERR_UNKNOWN;
    }

    cmd_rc = run_command(argv_enable);
    if (cmd_rc != 0)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "systemctl enable %s が失敗しました (終了コード %d)。", def->name, cmd_rc);
        return CPLAT_ERR_UNKNOWN;
    }

    (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                              "サービス '%s' を登録しました。", def->name);
    (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                              "開始するには: sudo systemctl start %s", def->name);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_uninstall(const cplat_service_definition *def)
{
    char unit_path[SYSTEMD_UNIT_PATH_SIZE];
    char svc_name_buf[SVC_NAME_BUFFER_SIZE];
    char err_msg[ERR_MSG_BUFFER_SIZE];
    char *argv_stop[] = {"systemctl", "stop", svc_name_buf, NULL};
    char *argv_disable[] = {"systemctl", "disable", svc_name_buf, NULL};
    char *argv_daemon_reload[] = {"systemctl", "daemon-reload", NULL};
    cplat_error err;
    int ret;
    int cmd_rc;

    if (cplat_snprintf(svc_name_buf, sizeof(svc_name_buf), "%s", def->name) != CPLAT_OK)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "サービス名が長すぎます。");
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    ret = check_root_privilege("uninstall");
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    if (cplat_snprintf(unit_path, sizeof(unit_path), "%s/%s.service", SYSTEMD_UNIT_DIR, def->name) != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "ユニット ファイルのパスが長すぎます。");
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* サービスが動作中でない場合もあり、停止の失敗をエラーとせず継続するため、戻り値は明示的に破棄する。 */
    (void)run_command(argv_stop);

    cmd_rc = run_command(argv_disable);
    if (cmd_rc != 0)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "systemctl disable %s が失敗しました (終了コード %d)。", def->name, cmd_rc);
    }

    cplat_error_clear(&err);
    if (cplat_remove(unit_path, &err) != 0 && cplat_error_get_errno(&err) != ENOENT)
    {
        (void)cplat_error_message(err_msg, sizeof(err_msg), &err);
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "%s の削除に失敗しました: %s", unit_path, err_msg);
        return cplat_error_to_result(&err);
    }

    (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                              "ユニット ファイルを削除しました: %s", unit_path);

    cmd_rc = run_command(argv_daemon_reload);
    if (cmd_rc != 0)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "systemctl daemon-reload が失敗しました (終了コード %d)。", cmd_rc);
    }

    (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                              "サービス '%s' を解除しました。", def->name);

    return CPLAT_OK;
}

/*
 * ============================================================
 *  内部ヘルパー
 * ============================================================
 */

/**
 *  @brief          StartLimitBurst / RestartSec / StartLimitIntervalSec の到達可能性を検査します。
 *  @param[in]      burst         期間内最大起動回数 (1 以上)。
 *  @param[in]      delay_sec     再起動遅延 [s]。
 *  @param[in]      interval_sec  集計期間 [s]。
 *  @return         到達可能な場合は @ref CPLAT_OK 、到達不能またはオーバーフロー時は @ref CPLAT_ERR_INVALID_ARGUMENT。
 */
static int validate_start_limit(const unsigned int burst, const unsigned int delay_sec, const unsigned int interval_sec)
{
    uint64_t restarts = (uint64_t)(burst - 1U);
    uint64_t delay = (uint64_t)delay_sec;
    uint64_t interval = (uint64_t)interval_sec;

    if (delay > 0U && restarts > (UINT64_MAX / delay))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if ((restarts * delay) >= interval)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return CPLAT_OK;
}

/**
 *  @brief          root 権限があるかを確認します (自動昇格は行いません)。
 *  @param[in]      operation_name  操作名 (トレース出力用)。
 *  @return         root の場合は @ref CPLAT_OK 、非 root は @ref CPLAT_ERR_PERMISSION_DENIED。
 */
static int check_root_privilege(const char *operation_name)
{
    int elevated = 0;
    int ret = cplat_elevated_process_is_elevated(&elevated);

    if (ret != CPLAT_OK)
    {
        return ret;
    }

    if (elevated == 0)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "%s には root 権限 (sudo) が必要です。", operation_name);
        return CPLAT_ERR_PERMISSION_DENIED;
    }

    return CPLAT_OK;
}

/**
 *  @brief          外部コマンドを同期実行します。
 *  @param[in]      argv  コマンドと引数の配列 (NULL 終端)。
 *  @return         コマンドの終了コード。実行失敗時は -1。
 */
static int run_command(char *const argv[])
{
    int exit_code = -1;
    cplat_process_options options = {0};
    int result;

    options.argv = argv;

    result = cplat_process_run_sync(&options, &exit_code);
    if (result != CPLAT_OK)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "外部コマンドの実行に失敗しました: %s", argv[0]);
        return -1;
    }

    return exit_code;
}

/**
 *  @brief          "systemd 248" 形式のバージョン行から major version を取得します。
 *  @param[in]      version_line  systemctl --version の 1 行目。
 *  @return         取得できた major version。失敗時は -1。
 */
static int parse_systemd_major_version(const char *version_line)
{
    const char prefix[] = "systemd ";
    size_t index = 0U;
    int version = 0;
    int has_digit = 0;

    while (prefix[index] != '\0')
    {
        if (version_line[index] != prefix[index])
        {
            return -1;
        }
        index++;
    }

    while (version_line[index] >= '0' && version_line[index] <= '9')
    {
        has_digit = 1;
        version = (version * 10) + (version_line[index] - '0');
        index++;
    }

    if (has_digit == 0)
    {
        return -1;
    }

    return version;
}

/**
 *  @brief          systemd の major version を取得します。
 *  @return         取得できた major version。失敗時は -1。
 */
static int get_systemd_major_version(void)
{
    char line[SYSTEMCTL_VERSION_LINE_SIZE];
    FILE *pipe;
    int version;

    pipe = popen("systemctl --version", "r");
    if (pipe == NULL)
    {
        return -1;
    }

    if (fgets(line, sizeof(line), pipe) == NULL)
    {
        (void)pclose(pipe);
        return -1;
    }

    if (pclose(pipe) == -1)
    {
        return -1;
    }

    version = parse_systemd_major_version(line);
    if (version < 0)
    {
        return -1;
    }

    return version;
}

/**
 *  @brief          errno 値から人間可読メッセージを生成します。
 *  @param[in]      err_num   errno の値。
 *  @param[out]     buf       格納先バッファー。
 *  @param[in]      buf_size  バッファー サイズ [byte]。
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

/**
 *  @brief          sd_notify(3) の失敗をトレースへ記録します。
 *  @param[in]      message  通知名。
 *  @param[in]      result   sd_notify(3) の戻り値。
 */
static void trace_notify_failure(const char *message, const int result)
{
    if (result < 0)
    {
        char err_msg[ERR_MSG_BUFFER_SIZE];
        format_errno_message(-result, err_msg, sizeof(err_msg));
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                  "sd_notify: %s の送信に失敗しました: %s", message, err_msg);
    }
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif
