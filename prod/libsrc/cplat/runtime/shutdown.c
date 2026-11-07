/**
 *******************************************************************************
 *  @file           shutdown.c
 *  @brief          プロセスを終了する共通処理を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/05/06
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/sync/sync.h>

#include <stdlib.h>

#if defined(PLATFORM_LINUX)
    #include <signal.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif

typedef struct shutdown_callback_entry
{
    cplat_shutdown_fn callback;
    void *context;
    struct shutdown_callback_entry *next;
} shutdown_callback_entry;

static shutdown_callback_entry *s_shutdown_callbacks = NULL;
static shutdown_callback_entry *s_shutdown_request_callbacks = NULL;
static cplat_local_lock *s_shutdown_lock;
static cplat_once_flag s_shutdown_lock_once = {0};
static cplat_once_flag s_shutdown_hook_once = {0};
static int s_shutdown_started = 0;
static int s_shutdown_request_started = 0;

/*
 * cplat_exit はロックなしで書き、atexit 経路が読む。
 * 終了コードを書いてから有効フラグを release で書き、有効フラグを acquire で読んでから終了コードを読む。
 */
static cplat_atomic_i32 s_exit_code = CPLAT_ATOMIC_INIT(0);
static cplat_atomic_i32 s_exit_code_valid = CPLAT_ATOMIC_INIT(0);

static void init_shutdown_lock(void)
{
    (void)cplat_local_lock_create(&s_shutdown_lock);
}

static void shutdown_lock(void)
{
    cplat_call_once(&s_shutdown_lock_once, init_shutdown_lock);
    cplat_local_lock_lock(s_shutdown_lock, CPLAT_SYNC_WAIT_FOREVER);
}

static void shutdown_unlock(void)
{
    cplat_local_lock_unlock(s_shutdown_lock);
}

static cplat_shutdown_event make_normal_exit_event(void)
{
    cplat_shutdown_event event;

    event.reason = CPLAT_SHUTDOWN_REASON_NORMAL_EXIT;

    if (cplat_atomic_load_i32(&s_exit_code_valid, CPLAT_MEMORY_ORDER_ACQUIRE) != 0)
    {
        event.code_kind = CPLAT_SHUTDOWN_CODE_KIND_EXIT_CODE;
        event.code = (int)cplat_atomic_load_i32(&s_exit_code, CPLAT_MEMORY_ORDER_RELAXED);
    }
    else
    {
        event.code_kind = CPLAT_SHUTDOWN_CODE_KIND_NONE;
        event.code = 0;
    }

    return event;
}

static void free_callback_list(shutdown_callback_entry *entry)
{
    while (entry != NULL)
    {
        shutdown_callback_entry *next = entry->next;
        cplat_free(entry);
        entry = next;
    }
}

/*
 * 登録済みのシャットダウン コールバックを一度だけ順に呼び出し、エントリを解放する。
 * コールバック実行中にロックを保持し続けないよう、リストを取り出してロック外で実行する。
 * event が NULL の場合は -1、すでに実行開始済みの場合は 1、実行した場合は 0 を返す。
 */
static int invoke_shutdown_callbacks_once(const cplat_shutdown_event *event)
{
    shutdown_callback_entry *callbacks;
    shutdown_callback_entry *entry;

    if (event == NULL)
    {
        return -1;
    }

    shutdown_lock();

    if (s_shutdown_started)
    {
        shutdown_unlock();
        return 1;
    }

    s_shutdown_started = 1;
    callbacks = s_shutdown_callbacks;
    s_shutdown_callbacks = NULL;
    shutdown_unlock();

    entry = callbacks;
    while (entry != NULL)
    {
        shutdown_callback_entry *next = entry->next;
        entry->callback(event, entry->context);
        cplat_free(entry);
        entry = next;
    }

    return 0;
}

/*
 * 登録済みのシャットダウン要求コールバックを一度だけ順に呼び出し、エントリを解放する。
 * 登録エントリがあり処理された場合は handled_out に 1、それ以外は 0 を設定する。
 * event が NULL の場合は -1、すでに通常シャットダウンまたは要求処理が開始済みの場合は 1、
 * 実行した場合は 0 を返す。
 */
static int invoke_shutdown_request_callbacks_once(const cplat_shutdown_event *event, int *handled_out)
{
    shutdown_callback_entry *callbacks;
    shutdown_callback_entry *entry;

    if (event == NULL)
    {
        return -1;
    }

    if (handled_out != NULL)
    {
        *handled_out = 0;
    }

    shutdown_lock();

    if (s_shutdown_started || s_shutdown_request_started)
    {
        shutdown_unlock();
        return 1;
    }

    s_shutdown_request_started = 1;
    callbacks = s_shutdown_request_callbacks;
    s_shutdown_request_callbacks = NULL;
    shutdown_unlock();

    if (handled_out != NULL && callbacks != NULL)
    {
        *handled_out = 1;
    }

    entry = callbacks;
    while (entry != NULL)
    {
        shutdown_callback_entry *next = entry->next;
        entry->callback(event, entry->context);
        cplat_free(entry);
        entry = next;
    }

    return 0;
}

static void shutdown_atexit_callback(void)
{
    cplat_shutdown_event event = make_normal_exit_event();
    (void)invoke_shutdown_callbacks_once(&event);
}

#if defined(PLATFORM_LINUX)
static void shutdown_signal_handler(int signal_number)
{
    cplat_shutdown_event event;
    int handled = 0;
    int request_result;

    event.reason = CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT;
    event.code_kind = CPLAT_SHUTDOWN_CODE_KIND_SIGNAL_NUMBER;
    event.code = signal_number;

    request_result = invoke_shutdown_request_callbacks_once(&event, &handled);
    if (request_result == 0 && handled)
    {
        return;
    }

    if (request_result == 1)
    {
        return;
    }

    (void)invoke_shutdown_callbacks_once(&event);

    /* 既定の動作に戻して同じシグナルを再送出し、シグナルによる終了として親プロセスへ伝える。 */
    signal(signal_number, SIG_DFL);
    raise(signal_number);
}
#elif defined(PLATFORM_WINDOWS)
/*
 * コンソール制御イベント (Ctrl+C、終了通知など) を処理する。
 * 要求コールバックによって処理された場合はイベント消費を示す TRUE を返し、
 * それ以外はシャットダウン コールバックを実行した後に FALSE を返して OS の既定処理に委ねる。
 * see: https://learn.microsoft.com/en-us/windows/console/handlerroutine
 */
static BOOL WINAPI shutdown_console_ctrl_handler(DWORD ctrl_type)
{
    cplat_shutdown_event event;
    int handled = 0;
    int request_result;

    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT)
    {
        event.reason = CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT;
    }
    else
    {
        event.reason = CPLAT_SHUTDOWN_REASON_PROCESS_TERMINATING;
    }

    event.code_kind = CPLAT_SHUTDOWN_CODE_KIND_CONSOLE_CTRL_TYPE;
    event.code = (int)ctrl_type;

    request_result = invoke_shutdown_request_callbacks_once(&event, &handled);
    if (request_result == 0 && handled)
    {
        return TRUE;
    }

    if (request_result == 1)
    {
        return TRUE;
    }

    (void)invoke_shutdown_callbacks_once(&event);

    return FALSE;
}
#endif

static void install_shutdown_hooks(void)
{
    (void)atexit(shutdown_atexit_callback);

#if defined(PLATFORM_LINUX)
    signal(SIGINT, shutdown_signal_handler);
    signal(SIGTERM, shutdown_signal_handler);
#elif defined(PLATFORM_WINDOWS)
    SetConsoleCtrlHandler(shutdown_console_ctrl_handler, TRUE);
#endif
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_shutdown_register(cplat_shutdown_fn callback, void *context)
{
    shutdown_callback_entry *entry;
    int result = CPLAT_OK;

    if (callback == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    cplat_call_once(&s_shutdown_hook_once, install_shutdown_hooks);

    entry = (shutdown_callback_entry *)cplat_malloc(sizeof(*entry));
    if (entry == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    entry->callback = callback;
    entry->context = context;

    shutdown_lock();

    if (s_shutdown_started)
    {
        result = CPLAT_ERR_UNKNOWN;
    }
    else
    {
        entry->next = s_shutdown_callbacks;
        s_shutdown_callbacks = entry;
    }

    shutdown_unlock();

    if (result != CPLAT_OK)
    {
        cplat_free(entry);
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_exit(const int code)
{
    int effective_code = code;

    if (code < 0 || code > (CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1))
    {
        effective_code = CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE;
    }

    cplat_atomic_store_i32(&s_exit_code, (int32_t)effective_code, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_atomic_store_i32(&s_exit_code_valid, 1, CPLAT_MEMORY_ORDER_RELEASE);
    exit(effective_code);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_shutdown_request_register(cplat_shutdown_fn callback, void *context)
{
    shutdown_callback_entry *entry;
    int result = CPLAT_OK;

    if (callback == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    cplat_call_once(&s_shutdown_hook_once, install_shutdown_hooks);

    entry = (shutdown_callback_entry *)cplat_malloc(sizeof(*entry));
    if (entry == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    entry->callback = callback;
    entry->context = context;

    shutdown_lock();

    if (s_shutdown_started || s_shutdown_request_started)
    {
        result = CPLAT_ERR_UNKNOWN;
    }
    else
    {
        entry->next = s_shutdown_request_callbacks;
        s_shutdown_request_callbacks = entry;
    }

    shutdown_unlock();

    if (result != CPLAT_OK)
    {
        cplat_free(entry);
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_shutdown_invoke_for_test(const cplat_shutdown_event *event, int *invoked_out)
{
    int rc = invoke_shutdown_callbacks_once(event);

    if (rc < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (invoked_out != NULL)
    {
        if (rc == 0)
        {
            *invoked_out = 1;
        }
        else
        {
            *invoked_out = 0;
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_shutdown_request_invoke_for_test(const cplat_shutdown_event *event, int *invoked_out)
{
    int rc = invoke_shutdown_request_callbacks_once(event, NULL);

    if (rc < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (invoked_out != NULL)
    {
        if (rc == 0)
        {
            *invoked_out = 1;
        }
        else
        {
            *invoked_out = 0;
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_shutdown_reset_for_test(void)
{
    shutdown_callback_entry *shutdown_entry;
    shutdown_callback_entry *request_entry;

    shutdown_lock();
    shutdown_entry = s_shutdown_callbacks;
    s_shutdown_callbacks = NULL;
    request_entry = s_shutdown_request_callbacks;
    s_shutdown_request_callbacks = NULL;
    s_shutdown_started = 0;
    s_shutdown_request_started = 0;
    cplat_atomic_store_i32(&s_exit_code, 0, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_atomic_store_i32(&s_exit_code_valid, 0, CPLAT_MEMORY_ORDER_RELAXED);
    shutdown_unlock();

    free_callback_list(shutdown_entry);
    free_callback_list(request_entry);
}
