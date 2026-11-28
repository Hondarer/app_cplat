/**
 *******************************************************************************
 *  @file           service.c
 *  @brief          クロスプラットフォーム サービス基盤の共通処理を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/28
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <stddef.h>
#include <stdint.h>

#include <cplat/console/console.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/service/service.h>
#include <cplat/service/service_internal.h>
#include <cplat/sync/atomic.h>
#include <cplat/sync/sync.h>

static int wait_for_stop_body(const int timeout_ms);
static void shutdown_request_callback(const cplat_shutdown_event *event, void *context);

/*
 * ============================================================
 *  内部状態 (プロセス グローバル)
 * ============================================================
 */

/** 現在アクティブなサービス操作で使用するトレーサー (所有権は利用側)。 */
static cplat_tracer *s_tracer = NULL;

/** 停止要求の有無。1 = 停止要求済み。 */
static int s_stop_requested = 0;

/** 停止フラグと条件変数を保護するミューテックス。 */
static cplat_local_lock *s_stop_lock = NULL;

/** 停止要求を待機スレッドへ通知する条件変数。 */
static cplat_condvar *s_stop_cv = NULL;

/** `cplat_service_wait_for_stop()` の入口・出口の進行計数。 */
static cplat_atomic_u32 s_wait_count = CPLAT_ATOMIC_INIT(0U);

/** `cplat_service_wait_for_stop()` の中にいる呼び出し数。 */
static cplat_atomic_i32 s_wait_depth = CPLAT_ATOMIC_INIT(0);

/*
 * ============================================================
 *  公開 API
 * ============================================================
 */

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT int CPLAT_API cplat_service_prepare_run(void)
{
    return cplat_internal_service_os_prepare_run();
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT int CPLAT_API cplat_service_install(const cplat_service_definition *def,
                                                 const cplat_service_install_options *options)
{
    int ret;

    if (def == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_internal_service_validate_name(def->name) != CPLAT_OK)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (options != NULL && cplat_internal_service_validate_run_args(options->run_args) != CPLAT_OK)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    s_tracer = def->tracer;
    ret = cplat_internal_service_os_install(def, options);
    s_tracer = NULL;

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT int CPLAT_API cplat_service_uninstall(const cplat_service_definition *def)
{
    int ret;

    if (def == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_internal_service_validate_name(def->name) != CPLAT_OK)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    s_tracer = def->tracer;
    ret = cplat_internal_service_os_uninstall(def);
    s_tracer = NULL;

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT int CPLAT_API cplat_service_run(const cplat_service_definition *def, int *exit_code_out)
{
    int ret;

    if (def == NULL || def->on_run == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_internal_service_validate_name(def->name) != CPLAT_OK)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    s_tracer = def->tracer;

    ret = cplat_internal_service_stop_init();
    if (ret != CPLAT_OK)
    {
        s_tracer = NULL;
        return ret;
    }

    ret = cplat_internal_service_os_run(def, exit_code_out);

    cplat_internal_service_stop_dispose();
    s_tracer = NULL;

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT int CPLAT_API cplat_service_run_console(const cplat_service_definition *def, int *exit_code_out)
{
    int ret;

    if (def == NULL || def->on_run == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_internal_service_validate_name(def->name) != CPLAT_OK)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    s_tracer = def->tracer;

    ret = cplat_internal_service_stop_init();
    if (ret != CPLAT_OK)
    {
        s_tracer = NULL;
        return ret;
    }

    ret = cplat_internal_service_run_lifecycle(def, exit_code_out);

    cplat_internal_service_stop_dispose();
    s_tracer = NULL;

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT int CPLAT_API cplat_service_wait_for_stop(const int timeout_ms)
{
    int ret;

    if (timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (s_stop_lock == NULL || s_stop_cv == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /*
     * 入口と出口で進行を記録する。待機中も on_run() の生存として扱うため、待機の前に depth を加算する。
     * 順序の保証は不要で、値の不可分性だけが要るため、RELAXED で更新する。
     */
    (void)cplat_atomic_fetch_add_u32(&s_wait_count, 1U, CPLAT_MEMORY_ORDER_RELAXED);
    (void)cplat_atomic_fetch_add_i32(&s_wait_depth, 1, CPLAT_MEMORY_ORDER_RELAXED);

    ret = wait_for_stop_body(timeout_ms);

    (void)cplat_atomic_fetch_sub_i32(&s_wait_depth, 1, CPLAT_MEMORY_ORDER_RELAXED);
    (void)cplat_atomic_fetch_add_u32(&s_wait_count, 1U, CPLAT_MEMORY_ORDER_RELAXED);

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT int CPLAT_API cplat_service_stop_requested(void)
{
    int requested;

    if (s_stop_lock == NULL)
    {
        return 0;
    }

    cplat_local_lock_lock(s_stop_lock);
    requested = s_stop_requested;
    cplat_local_lock_unlock(s_stop_lock);

    return requested;
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT void CPLAT_API cplat_service_request_stop(void)
{
    if (s_stop_lock == NULL || s_stop_cv == NULL)
    {
        return;
    }

    cplat_local_lock_lock(s_stop_lock);
    s_stop_requested = 1;
    cplat_local_lock_unlock(s_stop_lock);
    cplat_condvar_broadcast(s_stop_cv);
}

/* Doxygen コメントは、ヘッダーに記載 */

CPLAT_EXPORT void CPLAT_API cplat_service_set_status_text(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
    (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_VERBOSE, NULL, "状態テキスト: %s",
                              text);
    cplat_internal_service_os_notify_status(text);
}

/*
 * ============================================================
 *  内部共有関数
 * ============================================================
 */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_stop_init(void)
{
    int ret;

    s_stop_requested = 0;
    cplat_atomic_store_u32(&s_wait_count, 0U, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_atomic_store_i32(&s_wait_depth, 0, CPLAT_MEMORY_ORDER_RELAXED);

    ret = cplat_local_lock_create(&s_stop_lock);
    if (ret != CPLAT_OK)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "ミューテックスの生成に失敗しました。");
        s_stop_lock = NULL;
        return ret;
    }

    ret = cplat_condvar_create(&s_stop_cv);
    if (ret != CPLAT_OK)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                 "条件変数の生成に失敗しました。");
        cplat_local_lock_dispose(s_stop_lock);
        s_stop_lock = NULL;
        s_stop_cv = NULL;
        return ret;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_stop_dispose(void)
{
    if (s_stop_cv != NULL)
    {
        cplat_condvar_dispose(s_stop_cv);
        s_stop_cv = NULL;
    }

    if (s_stop_lock != NULL)
    {
        cplat_local_lock_dispose(s_stop_lock);
        s_stop_lock = NULL;
    }

    s_stop_requested = 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_on_run_made_progress(uint32_t *seen_count)
{
    uint32_t current;

    if (seen_count == NULL)
    {
        return 0;
    }

    current = cplat_atomic_load_u32(&s_wait_count, CPLAT_MEMORY_ORDER_RELAXED);

    if ((cplat_atomic_load_i32(&s_wait_depth, CPLAT_MEMORY_ORDER_RELAXED) > 0) || (current != *seen_count))
    {
        *seen_count = current;
        return 1;
    }

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_tracer *cplat_internal_service_get_tracer(void)
{
    return s_tracer;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_validate_name(const char *name)
{
    size_t i;

    if (name == NULL || name[0] == '\0')
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    for (i = 0U; name[i] != '\0'; i++)
    {
        unsigned char ch = (unsigned char)name[i];
        if (ch == '/' || ch == '\\' || ch < 0x20U || ch == 0x7FU)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_validate_run_args(const char *run_args)
{
    size_t i;

    if (run_args == NULL)
    {
        return CPLAT_OK;
    }

    for (i = 0U; run_args[i] != '\0'; i++)
    {
        if (run_args[i] == '\r' || run_args[i] == '\n')
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_run_callbacks(const cplat_service_definition *def)
{
    int callback_rc = 0;

    if (def->on_start != NULL)
    {
        callback_rc = def->on_start(def->user_data);
        if (callback_rc != 0)
        {
            /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "on_start が失敗しました (戻り値: %d)。", callback_rc);
        }
    }

    if (callback_rc == 0)
    {
        int run_rc;
        int stop_rc = 0;

        cplat_internal_service_os_notify_ready();

        run_rc = def->on_run(def->user_data);
        if (run_rc != 0)
        {
            /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
            (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                      "on_run が失敗しました (戻り値: %d)。", run_rc);
        }

        cplat_internal_service_os_notify_stopping();

        if (def->on_stop != NULL)
        {
            stop_rc = def->on_stop(def->user_data);
            if (stop_rc != 0)
            {
                /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
                (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                          "on_stop が失敗しました (戻り値: %d)。", stop_rc);
            }
        }

        if (run_rc != 0)
        {
            callback_rc = run_rc;
        }
        else
        {
            callback_rc = stop_rc;
        }
    }

    return callback_rc;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_service_run_lifecycle(const cplat_service_definition *def, int *exit_code_out)
{
    int ret;
    int callback_rc;

    cplat_console_init();

    ret = cplat_shutdown_request_register(shutdown_request_callback, NULL);
    if (ret != CPLAT_OK)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "停止要求の登録に失敗しました (結果コード: %d)。", ret);
        return ret;
    }

    callback_rc = cplat_internal_service_run_callbacks(def);
    if (exit_code_out != NULL)
    {
        *exit_code_out = callback_rc;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_dispatch_event(const cplat_service_definition *def, const cplat_service_event_info *info)
{
    if (def == NULL || info == NULL || def->on_event == NULL)
    {
        return;
    }

    /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
    (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                              "OS イベントを配送します (種別: %d)。", (int)info->type);
    def->on_event(info, def->user_data);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_service_dispatch_reload(const cplat_service_definition *def)
{
    if (def == NULL || def->on_reload == NULL)
    {
        return;
    }

    /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
    (void)cplat_tracer_write(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_INFO, NULL,
                             "設定再読込要求を配送します。");
    cplat_internal_service_os_notify_reloading();
    def->on_reload(def->user_data);
    cplat_internal_service_os_notify_ready();
}

/*
 * ============================================================
 *  内部ヘルパー
 * ============================================================
 */

/**
 *  @brief          `cplat_service_wait_for_stop()` の待機本体です。
 *  @param[in]      timeout_ms  タイムアウト [ms] (0 以上)。
 *  @return         停止要求到達時は @ref CPLAT_OK 、時間切れは @ref CPLAT_ERR_TIMEOUT 、
 *                  待機失敗時は条件変数待機のエラー コード。
 */
static int wait_for_stop_body(const int timeout_ms)
{
    int requested;
    int wait_result = CPLAT_OK;

    cplat_local_lock_lock(s_stop_lock);

    if (s_stop_requested == 0)
    {
        /* spurious wakeup 対策のため待機後に再確認する。 */
        wait_result = cplat_condvar_timed_wait(s_stop_cv, s_stop_lock, timeout_ms);
    }

    requested = s_stop_requested;
    cplat_local_lock_unlock(s_stop_lock);

    if (wait_result != CPLAT_OK && wait_result != CPLAT_ERR_TIMEOUT)
    {
        /* トレースは補助の記録であり、出力の失敗で処理を変えないため、戻り値は明示的に破棄する。 */
        (void)cplat_tracer_writef(cplat_internal_service_get_tracer(), CPLAT_TRACE_LEVEL_ERROR, NULL,
                                  "停止要求の待機に失敗しました (結果コード: %d)。", wait_result);
        return wait_result;
    }

    if (requested != 0)
    {
        return CPLAT_OK;
    }

    return CPLAT_ERR_TIMEOUT;
}

/**
 *  @brief          終了要求 callback (`cplat_shutdown_request_register()` 経由)。
 *  @param[in]      event    終了イベント情報 (未使用)。
 *  @param[in]      context  未使用。
 */
static void shutdown_request_callback(const cplat_shutdown_event *event, void *context)
{
    (void)event;
    (void)context;
    cplat_service_request_stop();
}
