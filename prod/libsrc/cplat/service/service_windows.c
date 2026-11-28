/**
 *******************************************************************************
 *  @file           service_windows.c
 *  @brief          クロスプラットフォーム サービス基盤の Windows 向け処理を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/28
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)
    /* Advapi32.lib は windows_sdk.h の #pragma comment(lib, "Advapi32.lib") で指定済みである。 */
    #include <cplat/base/windows_sdk.h>

    #include <assert.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

    #include <cplat/base/error_internal.h>
    #include <cplat/crt/stdio.h>
    #include <cplat/runtime/elevated_process.h>
    #include <cplat/runtime/process.h>
    #include <cplat/service/service.h>
    #include <cplat/service/service_internal.h>
    #include <cplat/sync/sync.h>
    #include <cplat/win32/win32.h>

    /** 失敗カウンターをリセットする無失敗期間 [s]。1 日 (24 時間)。 */
    #define SVC_FAILURE_RESET_PERIOD_SEC (24U * 60U * 60U)

    /** 起動開始の SERVICE_START_PENDING に付ける dwWaitHint [ms]。 */
    #define SVC_ON_START_WAIT_HINT_MS 3000U

    /** 制御ハンドラーの SERVICE_STOP_PENDING に付ける dwWaitHint [ms]。 */
    #define SVC_ON_RUN_RETURN_WAIT_HINT_MS 5000U

    /** on_run() 復帰後の SERVICE_STOP_PENDING に付ける dwWaitHint [ms]。 */
    #define SVC_ON_STOP_WAIT_HINT_MS 3000U

    /** セッション ID (DWORD の 10 進表記) を格納する配列の大きさ [byte]。 */
    #define SVC_SESSION_ID_TEXT_SIZE 16U

    /** binPath の実行ファイル パス以外の分 (引用符、空白、run_args) [byte]。 */
    #define SVC_BIN_PATH_EXTRA_BYTES 512U

    /** 停止完了を待つ上限 [ms]。 */
    #define SVC_STOP_TIMEOUT_MS 30000ULL

    /** 停止状態を問い合わせる間隔 [ms]。 */
    #define SVC_STOP_POLL_INTERVAL_MS 50U

static_assert(SVC_ON_START_WAIT_HINT_MS > 0U, "start wait hint must be positive");
static_assert(SVC_ON_RUN_RETURN_WAIT_HINT_MS > 0U, "stop request wait hint must be positive");
static_assert(SVC_ON_STOP_WAIT_HINT_MS > 0U, "stop wait hint must be positive");
static_assert(SVC_SESSION_ID_TEXT_SIZE >= (10U + 1U), "session id buffer must hold 10 digits and NUL");

static const cplat_service_definition *s_def = NULL;
static SERVICE_STATUS_HANDLE s_status_handle = NULL;
static SERVICE_STATUS s_status = {0};
static cplat_local_lock *s_status_lock = NULL;
static int s_callback_exit_code = 0;

static int check_admin_privilege(const char *operation_name);
static int map_win32_install_error(const DWORD err);
static int map_win32_uninstall_error(const DWORD err);
static void lock_service_status(void);
static void unlock_service_status(void);
static DWORD notify_service_status_locked(int *failed);
static DWORD set_service_status_locked(const DWORD state, const DWORD controls, const DWORD checkpoint,
                                       const DWORD wait_hint, int *failed);
static DWORD set_service_stopped_locked(const DWORD specific_exit_code, int *failed);
static void trace_service_status_failure(const DWORD state, const DWORD error, const int failed);
static void set_service_status(const DWORD state, const DWORD controls, const DWORD checkpoint, const DWORD wait_hint);
static void set_service_stopped(const DWORD specific_exit_code);
static DWORD accepted_controls(void);
static void write_dispatcher_error(const DWORD err);
static void handle_power_event(const DWORD ev_type);
static void handle_session_change(const DWORD ev_type, const LPVOID ev_data);
static DWORD WINAPI service_ctrl_handler(DWORD ctrl, DWORD ev_type, LPVOID ev_data, LPVOID context);
static VOID WINAPI service_main(DWORD argc, LPWSTR *argv);
static int wait_for_service_stop(SC_HANDLE service, int *timed_out);

/*
 * ============================================================
 *  OS フック実装
 * ============================================================
 */

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_ready(void)
{
    DWORD error = ERROR_SUCCESS;
    DWORD controls;
    int failed = 0;
    int suppressed = 0;

    if (s_status_handle == NULL)
    {
        return;
    }

    controls = accepted_controls();
    lock_service_status();
    if ((s_status.dwCurrentState == SERVICE_STOP_PENDING) || (s_status.dwCurrentState == SERVICE_STOPPED))
    {
        suppressed = 1;
    }
    else
    {
        error = set_service_status_locked(SERVICE_RUNNING, controls, 0, 0, &failed);
    }
    unlock_service_status();

    if (suppressed != 0)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                 "停止中のため SERVICE_RUNNING を通知しません。");
    }
    else
    {
        trace_service_status_failure(SERVICE_RUNNING, error, failed);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_stopping(void)
{
    DWORD error;
    int failed;

    if (s_status_handle == NULL)
    {
        return;
    }

    /*
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_status
     */
    lock_service_status();
    error = set_service_status_locked(SERVICE_STOP_PENDING, 0, 2, SVC_ON_STOP_WAIT_HINT_MS, &failed);
    unlock_service_status();

    trace_service_status_failure(SERVICE_STOP_PENDING, error, failed);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_reloading(void)
{
    /* SCM には reload 開始を通知する仕組みがないため、何もしない。 */
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_os_notify_status(const char *text)
{
    (void)text;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_prepare_run(void)
{
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_run(const cplat_service_definition *def, int *exit_code_out)
{
    cplat_local_lock *status_lock = NULL;
    int ret;

    /*
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-startservicectrldispatcherw
     */
    SERVICE_TABLE_ENTRYU dispatch_table[] = {{def->name, service_main}, {NULL, NULL}};

    ret = cplat_local_lock_create(&status_lock);
    if (ret != CPLAT_OK || status_lock == NULL)
    {
        if (status_lock != NULL)
        {
            cplat_local_lock_dispose(status_lock);
        }
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "サービス ステータス ロックを生成できません。");
        return (ret != CPLAT_OK) ? ret : CPLAT_ERR_OUT_OF_MEMORY;
    }

    s_status_lock = status_lock;
    s_def = def;
    s_callback_exit_code = 0;

    (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                             "SCM ディスパッチャーに接続します...");

    ret = CPLAT_OK;
    if (!StartServiceCtrlDispatcherU(dispatch_table))
    {
        DWORD err = GetLastError();
        (void)cplat_internal_error_report_windows_error(NULL, err);
        write_dispatcher_error(err);
        ret = CPLAT_ERR_UNKNOWN;
    }
    else if (exit_code_out != NULL)
    {
        *exit_code_out = s_callback_exit_code;
    }

    cplat_local_lock_dispose(s_status_lock);
    s_status_lock = NULL;
    s_def = NULL;
    s_status_handle = NULL;

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_install(const cplat_service_definition *def, const cplat_service_install_options *options)
{
    SC_HANDLE scm = NULL;
    SC_HANDLE svc = NULL;
    char bin_path[CPLAT_SERVICE_EXEC_PATH_MAX + SVC_BIN_PATH_EXTRA_BYTES];
    char exe_path[CPLAT_SERVICE_EXEC_PATH_MAX];
    const char *run_args;
    const char *description_text;
    unsigned int restart_delay_sec;
    unsigned int preshutdown_timeout_ms;
    int disable_auto_restart;
    int ret;

    if (def->display_name == NULL || def->display_name[0] == '\0')
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    run_args = (options != NULL && options->run_args != NULL) ? options->run_args : "run";
    restart_delay_sec = (options != NULL && options->restart_delay_sec != 0U) ? options->restart_delay_sec
                                                                              : CPLAT_SERVICE_DEFAULT_RESTART_DELAY_SEC;
    preshutdown_timeout_ms = (options != NULL && options->preshutdown_timeout_ms != 0U)
                                 ? options->preshutdown_timeout_ms
                                 : CPLAT_SERVICE_DEFAULT_PRESHUTDOWN_TIMEOUT_MS;
    disable_auto_restart = (options != NULL) ? options->disable_auto_restart : 0;
    description_text = (def->description != NULL) ? def->description : "";

    if (disable_auto_restart == 0 && restart_delay_sec > (UINT32_MAX / 1000U))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = check_admin_privilege("install");
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    ret = cplat_process_get_executable_path(exe_path, sizeof(exe_path));
    if (ret != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "実行ファイルのパスを取得できませんでした。");
        return ret;
    }

    /*
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-createservicew
     */
    if (run_args[0] == '\0')
    {
        ret = cplat_snprintf(bin_path, sizeof(bin_path), "\"%s\"", exe_path);
    }
    else
    {
        ret = cplat_snprintf(bin_path, sizeof(bin_path), "\"%s\" %s", exe_path, run_args);
    }
    if (ret != CPLAT_OK)
    {
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "パスの生成に失敗しました。");
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    scm = OpenSCManagerU(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (scm == NULL)
    {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        {
            (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                     "SCM へのアクセスが拒否されました。管理者として実行してください。");
        }
        else
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "OpenSCManagerU が失敗しました (エラー コード: %lu)。", err);
        }
        return map_win32_install_error(err);
    }

    svc = CreateServiceU(scm, def->name, def->display_name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, bin_path, NULL, NULL, NULL, NULL, NULL);

    if (svc == NULL)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS)
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "サービス '%s' は既に登録されています。", def->name);
        }
        else if (err == ERROR_ACCESS_DENIED)
        {
            (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                     "サービスの登録が拒否されました。管理者として実行してください。");
        }
        else if (err == ERROR_SERVICE_MARKED_FOR_DELETE)
        {
            (void)cplat_tracer_writef(
                cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                "サービス '%s' は削除待ち状態です。Windows を再起動してから再度実行してください。", def->name);
        }
        else
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "CreateServiceU が失敗しました (エラー コード: %lu)。", err);
        }
        ret = map_win32_install_error(err);
    }
    else
    {
        SERVICE_DESCRIPTIONA description;
        SERVICE_PRESHUTDOWN_INFO preshutdown_info;

        description.lpDescription = (char *)description_text;
        if (!ChangeServiceConfig2U(svc, SERVICE_CONFIG_DESCRIPTION, &description))
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                      "サービスの説明の設定に失敗しました (エラー コード: %lu)。", GetLastError());
        }

        preshutdown_info.dwPreshutdownTimeout = (DWORD)preshutdown_timeout_ms;
        if (!ChangeServiceConfig2W(svc, SERVICE_CONFIG_PRESHUTDOWN_INFO, &preshutdown_info))
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                      "PRESHUTDOWN 猶予の設定に失敗しました (エラー コード: %lu)。", GetLastError());
        }

        if (disable_auto_restart == 0)
        {
            SC_ACTION restart_action;
            SERVICE_FAILURE_ACTIONSW failure_actions;
            SERVICE_FAILURE_ACTIONS_FLAG failure_flag;

            restart_action.Type = SC_ACTION_RESTART;
            restart_action.Delay = (DWORD)(restart_delay_sec * 1000U);
            failure_actions.dwResetPeriod = SVC_FAILURE_RESET_PERIOD_SEC;
            failure_actions.lpRebootMsg = NULL;
            failure_actions.lpCommand = NULL;
            /*
             * see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_failure_actionsw
             */
            failure_actions.cActions = 1;
            failure_actions.lpsaActions = &restart_action;

            if (!ChangeServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &failure_actions))
            {
                (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                          "自動再起動の設定に失敗しました (エラー コード: %lu)。", GetLastError());
            }

            /*
             * see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_failure_actions_flag
             */
            failure_flag.fFailureActionsOnNonCrashFailures = TRUE;
            if (!ChangeServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &failure_flag))
            {
                (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_WARNING, NULL,
                                          "自動再起動フラグの設定に失敗しました (エラー コード: %lu)。",
                                          GetLastError());
            }
        }

        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                  "サービス '%s' を登録しました。", def->name);
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL, "実行ファイル: %s",
                                  bin_path);
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                  "開始するには: sc start %s", def->name);

        (void)CloseServiceHandle(svc);
        ret = CPLAT_OK;
    }

    (void)CloseServiceHandle(scm);
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_os_uninstall(const cplat_service_definition *def)
{
    SC_HANDLE scm = NULL;
    SC_HANDLE svc = NULL;
    int ret;

    ret = check_admin_privilege("uninstall");
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    scm = OpenSCManagerU(NULL, NULL, SC_MANAGER_CONNECT);
    if (scm == NULL)
    {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED)
        {
            (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                     "SCM へのアクセスが拒否されました。管理者として実行してください。");
        }
        else
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "OpenSCManagerU が失敗しました (エラー コード: %lu)。", err);
        }
        return map_win32_uninstall_error(err);
    }

    svc = OpenServiceU(scm, def->name, SERVICE_STOP | DELETE | SERVICE_QUERY_STATUS);
    if (svc == NULL)
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST)
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "サービス '%s' は登録されていません。", def->name);
        }
        else if (err == ERROR_ACCESS_DENIED)
        {
            (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                     "サービスへのアクセスが拒否されました。管理者として実行してください。");
        }
        else
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "OpenServiceU が失敗しました (エラー コード: %lu)。", err);
        }
        ret = map_win32_uninstall_error(err);
    }
    else
    {
        int timed_out = 0;

        /*
         * see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-deleteservice
         */
        if (!wait_for_service_stop(svc, &timed_out))
        {
            (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                     "サービスの停止を確認できないため、削除しません。");
            (void)CloseServiceHandle(svc);
            (void)CloseServiceHandle(scm);
            return (timed_out != 0) ? CPLAT_ERR_TIMEOUT : CPLAT_ERR_UNKNOWN;
        }

        if (!DeleteService(svc))
        {
            DWORD err = GetLastError();
            if (err == ERROR_SERVICE_MARKED_FOR_DELETE)
            {
                (void)cplat_tracer_writef(
                    cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                    "サービス '%s' は削除待ち状態です。Windows を再起動してから再度実行してください。", def->name);
            }
            else
            {
                (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                          "DeleteService が失敗しました (エラー コード: %lu)。", err);
            }
            ret = map_win32_uninstall_error(err);
        }
        else
        {
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                                      "サービス '%s' の削除を要求しました。", def->name);
            ret = CPLAT_OK;
        }

        (void)CloseServiceHandle(svc);
    }

    (void)CloseServiceHandle(scm);
    return ret;
}

/*
 * ============================================================
 *  内部ヘルパー
 * ============================================================
 */

static int check_admin_privilege(const char *operation_name)
{
    int elevated = 0;
    int ret = cplat_elevated_process_is_elevated(&elevated);

    if (ret != CPLAT_OK)
    {
        return ret;
    }

    if (elevated == 0)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "%s には管理者権限が必要です。", operation_name);
        return CPLAT_ERR_PERMISSION_DENIED;
    }

    return CPLAT_OK;
}

static int map_win32_install_error(const DWORD err)
{
    if (err == ERROR_ACCESS_DENIED)
    {
        return cplat_internal_error_report_windows_error_as(NULL, err, CPLAT_ERR_PERMISSION_DENIED);
    }
    if (err == ERROR_SERVICE_EXISTS)
    {
        return cplat_internal_error_report_windows_error_as(NULL, err, CPLAT_ERR_DUPLICATE_DEFINITION);
    }
    if (err == ERROR_SERVICE_MARKED_FOR_DELETE)
    {
        return cplat_internal_error_report_windows_error_as(NULL, err, CPLAT_ERR_BUSY);
    }
    return cplat_internal_error_report_windows_error(NULL, err);
}

static int map_win32_uninstall_error(const DWORD err)
{
    if (err == ERROR_ACCESS_DENIED)
    {
        return cplat_internal_error_report_windows_error_as(NULL, err, CPLAT_ERR_PERMISSION_DENIED);
    }
    if (err == ERROR_SERVICE_DOES_NOT_EXIST)
    {
        return cplat_internal_error_report_windows_error_as(NULL, err, CPLAT_ERR_NOT_FOUND);
    }
    if (err == ERROR_SERVICE_MARKED_FOR_DELETE)
    {
        return cplat_internal_error_report_windows_error_as(NULL, err, CPLAT_ERR_BUSY);
    }
    return cplat_internal_error_report_windows_error(NULL, err);
}

static void lock_service_status(void)
{
    if (s_status_lock != NULL)
    {
        cplat_local_lock_lock(s_status_lock);
    }
}

static void unlock_service_status(void)
{
    if (s_status_lock != NULL)
    {
        cplat_local_lock_unlock(s_status_lock);
    }
}

static DWORD notify_service_status_locked(int *failed)
{
    *failed = 0;
    if (!SetServiceStatus(s_status_handle, &s_status))
    {
        *failed = 1;
        return GetLastError();
    }
    return ERROR_SUCCESS;
}

static DWORD set_service_status_locked(const DWORD state, const DWORD controls, const DWORD checkpoint,
                                       const DWORD wait_hint, int *failed)
{
    s_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    s_status.dwCurrentState = state;
    s_status.dwControlsAccepted = controls;
    s_status.dwWin32ExitCode = NO_ERROR;
    s_status.dwServiceSpecificExitCode = 0;
    s_status.dwCheckPoint = checkpoint;
    s_status.dwWaitHint = wait_hint;
    return notify_service_status_locked(failed);
}

static DWORD set_service_stopped_locked(const DWORD specific_exit_code, int *failed)
{
    s_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    s_status.dwCurrentState = SERVICE_STOPPED;
    s_status.dwControlsAccepted = 0;
    s_status.dwWin32ExitCode = NO_ERROR;
    s_status.dwServiceSpecificExitCode = 0;

    if (specific_exit_code != 0)
    {
        s_status.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        s_status.dwServiceSpecificExitCode = specific_exit_code;
    }

    s_status.dwCheckPoint = 0;
    s_status.dwWaitHint = 0;
    return notify_service_status_locked(failed);
}

static void trace_service_status_failure(const DWORD state, const DWORD error, const int failed)
{
    if (failed != 0)
    {
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "SetServiceStatus が失敗しました (状態: %lu、エラー コード: %lu)。", state, error);
    }
}

static void set_service_status(const DWORD state, const DWORD controls, const DWORD checkpoint, const DWORD wait_hint)
{
    DWORD error;
    int failed;

    lock_service_status();
    error = set_service_status_locked(state, controls, checkpoint, wait_hint, &failed);
    unlock_service_status();

    trace_service_status_failure(state, error, failed);
}

static void set_service_stopped(const DWORD specific_exit_code)
{
    DWORD error;
    int failed;

    lock_service_status();
    error = set_service_stopped_locked(specific_exit_code, &failed);
    unlock_service_status();

    trace_service_status_failure(SERVICE_STOPPED, error, failed);
}

static DWORD accepted_controls(void)
{
    DWORD controls = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if (s_def != NULL && s_def->on_event != NULL)
    {
        controls |= SERVICE_ACCEPT_POWEREVENT | SERVICE_ACCEPT_SESSIONCHANGE | SERVICE_ACCEPT_PRESHUTDOWN;
    }

    if (s_def != NULL && s_def->on_reload != NULL)
    {
        controls |= SERVICE_ACCEPT_PARAMCHANGE;
    }

    return controls;
}

static void write_dispatcher_error(const DWORD err)
{
    cplat_tracer *tracer = cplat_internal_service_get_tracer();

    if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
    {
        (void)cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_ERROR, NULL, "SCM への接続に失敗しました。");
        (void)cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "サービスとして登録された後、SCM から起動してください。");
        return;
    }

    (void)cplat_tracer_writef(tracer, CPLAT_TRACE_LEVEL_ERROR, NULL,
                              "StartServiceCtrlDispatcherU が失敗しました (エラー コード: %lu)。", err);
}

static void handle_power_event(const DWORD ev_type)
{
    cplat_service_event_info info;

    info.pad = 0U;
    info.session_id = NULL;

    if (ev_type == PBT_APMSUSPEND)
    {
        info.type = CPLAT_SERVICE_EVENT_POWER_SUSPEND;
        cplat_internal_service_dispatch_event(s_def, &info);
    }
    else if (ev_type == PBT_APMRESUMEAUTOMATIC || ev_type == PBT_APMRESUMESUSPEND)
    {
        info.type = CPLAT_SERVICE_EVENT_POWER_RESUME;
        cplat_internal_service_dispatch_event(s_def, &info);
    }
}

static void handle_session_change(const DWORD ev_type, const LPVOID ev_data)
{
    const WTSSESSION_NOTIFICATION *notification;
    char session_id_buf[SVC_SESSION_ID_TEXT_SIZE];
    cplat_service_event_info info;

    if (ev_data == NULL)
    {
        return;
    }

    if (ev_type != WTS_SESSION_LOGON && ev_type != WTS_SESSION_LOGOFF)
    {
        return;
    }

    notification = (const WTSSESSION_NOTIFICATION *)ev_data;
    (void)cplat_snprintf(session_id_buf, sizeof(session_id_buf), "%lu", (unsigned long)notification->dwSessionId);

    info.type = (ev_type == WTS_SESSION_LOGON) ? CPLAT_SERVICE_EVENT_SESSION_LOGON : CPLAT_SERVICE_EVENT_SESSION_LOGOFF;
    info.pad = 0U;
    info.session_id = session_id_buf;
    cplat_internal_service_dispatch_event(s_def, &info);
}

static DWORD WINAPI service_ctrl_handler(DWORD ctrl, DWORD ev_type, LPVOID ev_data, LPVOID context)
{
    cplat_service_event_info info;
    DWORD error;
    DWORD state;
    int failed;

    (void)context;

    if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN)
    {
        set_service_status(SERVICE_STOP_PENDING, 0, 1, SVC_ON_RUN_RETURN_WAIT_HINT_MS);
        cplat_service_request_stop();
    }
    else if (ctrl == SERVICE_CONTROL_PRESHUTDOWN)
    {
        info.type = CPLAT_SERVICE_EVENT_PRESHUTDOWN;
        info.pad = 0U;
        info.session_id = NULL;
        cplat_internal_service_dispatch_event(s_def, &info);
        set_service_status(SERVICE_STOP_PENDING, 0, 1, SVC_ON_RUN_RETURN_WAIT_HINT_MS);
        cplat_service_request_stop();
    }
    else if (ctrl == SERVICE_CONTROL_POWEREVENT)
    {
        handle_power_event(ev_type);
    }
    else if (ctrl == SERVICE_CONTROL_SESSIONCHANGE)
    {
        handle_session_change(ev_type, ev_data);
    }
    else if (ctrl == SERVICE_CONTROL_PARAMCHANGE)
    {
        cplat_internal_service_dispatch_reload(s_def);
    }
    else if (ctrl == SERVICE_CONTROL_INTERROGATE)
    {
        lock_service_status();
        state = s_status.dwCurrentState;
        error = notify_service_status_locked(&failed);
        unlock_service_status();
        trace_service_status_failure(state, error, failed);
    }
    else
    {
        /* see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nc-winsvc-lphandler_function_ex */
        return ERROR_CALL_NOT_IMPLEMENTED;
    }

    return NO_ERROR;
}

static VOID WINAPI service_main(DWORD argc, LPWSTR *argv)
{
    int rc;

    (void)argc;
    (void)argv;

    if (s_def == NULL)
    {
        return;
    }

    s_status_handle = RegisterServiceCtrlHandlerExU(s_def->name, service_ctrl_handler, NULL);
    if (s_status_handle == NULL)
    {
        return;
    }

    set_service_status(SERVICE_START_PENDING, 0, 1, SVC_ON_START_WAIT_HINT_MS);

    rc = cplat_internal_service_run_callbacks(s_def);
    s_callback_exit_code = rc;
    set_service_stopped((DWORD)rc);
}

static int wait_for_service_stop(SC_HANDLE service, int *timed_out)
{
    SERVICE_STATUS_PROCESS status;
    DWORD bytes_needed;
    SERVICE_STATUS requested_status;
    ULONGLONG started = GetTickCount64();

    *timed_out = 0;

    /*
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-queryservicestatusex
     */
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, (DWORD)sizeof(status), &bytes_needed))
    {
        return 0;
    }

    if ((status.dwCurrentState != SERVICE_STOPPED) && (status.dwCurrentState != SERVICE_STOP_PENDING))
    {
        if (!ControlService(service, SERVICE_CONTROL_STOP, &requested_status))
        {
            return 0;
        }
    }

    while (status.dwCurrentState != SERVICE_STOPPED)
    {
        if (GetTickCount64() - started >= SVC_STOP_TIMEOUT_MS)
        {
            *timed_out = 1;
            return 0;
        }

        Sleep(SVC_STOP_POLL_INTERVAL_MS);

        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status, (DWORD)sizeof(status),
                                  &bytes_needed))
        {
            return 0;
        }
    }

    return 1;
}

#endif /* PLATFORM_WINDOWS */
