/**
 *******************************************************************************
 *  @file           prompt_windows.c
 *  @brief          SetConsoleMode の VT 入力を使用する Windows 向けプロンプト処理を実装します。
 *******************************************************************************
 */

#include "prompt_platform.h"

#if defined(PLATFORM_WINDOWS)

    #include <stdio.h>
    #include <cplat/base/result.h>
    #include <cplat/crt/stdio.h>

typedef BOOL(WINAPI *prompt_windows_cancel_sync_io_fn)(HANDLE thread_handle);

static BOOL WINAPI prompt_windows_default_cancel_sync_io(HANDLE thread_handle)
{
    return CancelSynchronousIo(thread_handle);
}

static prompt_windows_cancel_sync_io_fn s_cancel_sync_io_fn = prompt_windows_default_cancel_sync_io;

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_init_cancelable(prompt_platform_context *context)
{
    if (context == NULL)
    {
        return -1;
    }

    context->input_thread = NULL;
    context->input_lock = NULL;
    context->input_done_cv = NULL;
    context->input_active = 0;
    cplat_atomic_store_i32(&context->cancel_requested, 0, CPLAT_MEMORY_ORDER_RELAXED);

    if (cplat_local_lock_create(&context->input_lock) != CPLAT_OK)
    {
        return -1;
    }

    if (cplat_condvar_create(&context->input_done_cv) != CPLAT_OK)
    {
        cplat_local_lock_dispose(context->input_lock);
        context->input_lock = NULL;
        return -1;
    }

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void prompt_platform_dispose(prompt_platform_context *context)
{
    if (context == NULL)
    {
        return;
    }

    /* stdin_handle は標準入力共有ハンドルのため閉じず、複製したスレッド ハンドル等だけを解放する。 */
    if (context->input_thread != NULL)
    {
        (void)CloseHandle(context->input_thread);
        context->input_thread = NULL;
    }

    if (context->input_done_cv != NULL)
    {
        cplat_condvar_dispose(context->input_done_cv);
        context->input_done_cv = NULL;
    }

    if (context->input_lock != NULL)
    {
        cplat_local_lock_dispose(context->input_lock);
        context->input_lock = NULL;
    }

    context->input_active = 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_is_input_canceled(const prompt_platform_context *context)
{
    if (context == NULL)
    {
        return 0;
    }

    return cplat_atomic_load_i32(&context->cancel_requested, CPLAT_MEMORY_ORDER_ACQUIRE) != 0 ? 1 : 0;
}

/* 読み取りスレッドを排他区間下で入力区間へ登録し、中止側が他 I/O を誤って止めないようにする。 */
static int prompt_windows_begin_sync_input(prompt_platform_context *context)
{
    HANDLE self_process;
    HANDLE duplicated;

    if (prompt_platform_is_input_canceled(context))
    {
        return PROMPT_PLATFORM_CANCELED;
    }

    if (context->input_lock == NULL)
    {
        return 0;
    }

    cplat_local_lock_lock(context->input_lock);

    if (prompt_platform_is_input_canceled(context))
    {
        cplat_local_lock_unlock(context->input_lock);
        return PROMPT_PLATFORM_CANCELED;
    }

    /*
     * GetCurrentThread() が返す擬似ハンドルは使用側のスレッドを指すため、別スレッドから
     * CancelSynchronousIo() で読み取り側を指定できるよう THREAD_TERMINATE 権限付きで複製する。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthread
     * see: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelsynchronousio
     */
    self_process = GetCurrentProcess();
    duplicated = NULL;
    if (!DuplicateHandle(self_process, GetCurrentThread(), self_process, &duplicated, THREAD_TERMINATE, FALSE, 0U))
    {
        cplat_local_lock_unlock(context->input_lock);
        return PROMPT_PLATFORM_ERROR;
    }

    context->input_thread = duplicated;
    context->input_active = 1;
    cplat_local_lock_unlock(context->input_lock);

    return 0;
}

/* 入力区間の終了を排他区間下で確定して待機中の中止要求へ通知し、終了時の中止観測有無を返す。 */
static int prompt_windows_end_sync_input(prompt_platform_context *context)
{
    HANDLE thread_to_close = NULL;
    int canceled;

    if (context->input_lock == NULL)
    {
        return prompt_platform_is_input_canceled(context);
    }

    cplat_local_lock_lock(context->input_lock);
    thread_to_close = context->input_thread;
    context->input_thread = NULL;
    context->input_active = 0;
    canceled = prompt_platform_is_input_canceled(context);

    if (context->input_done_cv != NULL)
    {
        cplat_condvar_broadcast(context->input_done_cv);
    }

    cplat_local_lock_unlock(context->input_lock);

    if (thread_to_close != NULL)
    {
        (void)CloseHandle(thread_to_close);
    }

    return canceled;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_stop_input(prompt_platform_context *context)
{
    if (context == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    cplat_atomic_store_i32(&context->cancel_requested, 1, CPLAT_MEMORY_ORDER_RELEASE);

    if (context->input_lock == NULL || context->input_done_cv == NULL)
    {
        return CPLAT_OK;
    }

    cplat_local_lock_lock(context->input_lock);

    /*
     * 排他区間内で入力区間の在籍を確認してから CancelSynchronousIo() を呼び出す。
     * - 呼び出し元には THREAD_TERMINATE 権限が必要。
     * - 開始直前の競合では ERROR_NOT_FOUND となるため、入力区間が終わるまで 50 ms 上限で待機して再試行する。
     * - CancelSynchronousIo() の TRUE は受け付けだけであり、対象 I/O が通常完了して cplat_fgets などが次の
     *   ReadFile へ進んだ場合にも input_active が継続し得るため、TRUE 後も 50 ms 上限の timed_wait で待機し、
     *   区間継続中なら再要求する。
     * - condvar 待機中は排他を一時解放し、CPLAT_OK / CPLAT_ERR_TIMEOUT 以外の待機失敗ではロックを解放して
     *   CPLAT_ERR_UNKNOWN を返す。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelsynchronousio
     */
    while (context->input_active && context->input_thread != NULL)
    {
        int wait_ret;

        if (!s_cancel_sync_io_fn(context->input_thread) && GetLastError() != ERROR_NOT_FOUND)
        {
            cplat_local_lock_unlock(context->input_lock);

            return CPLAT_ERR_UNKNOWN;
        }

        wait_ret = cplat_condvar_timed_wait(context->input_done_cv, context->input_lock, PROMPT_PLATFORM_SHORT_WAIT_MS);
        if (wait_ret != CPLAT_OK && wait_ret != CPLAT_ERR_TIMEOUT)
        {
            cplat_local_lock_unlock(context->input_lock);

            return CPLAT_ERR_UNKNOWN;
        }
    }

    cplat_local_lock_unlock(context->input_lock);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_os_get_size(int *cols_out, int *rows_out)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    /*
     * コンソール スクリーン バッファー情報からウィンドウの表示幅と高さを取得する。
     * see: https://learn.microsoft.com/en-us/windows/console/getconsolescreenbufferinfo
     */
    if (output != INVALID_HANDLE_VALUE && output != NULL && GetConsoleScreenBufferInfo(output, &info))
    {
        *cols_out = (int)info.srWindow.Right - (int)info.srWindow.Left + 1;
        *rows_out = (int)info.srWindow.Bottom - (int)info.srWindow.Top + 1;
        return 0;
    }

    return -1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_os_enter_raw(prompt_platform_context *context)
{
    DWORD new_mode;

    /* すでに raw モードが有効な場合は重複移行を防ぐ。 */
    if (context->raw_active)
    {
        return -1;
    }

    /* 標準入力ハンドルを取得する。 */
    context->stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (context->stdin_handle == INVALID_HANDLE_VALUE || context->stdin_handle == NULL)
    {
        return -1;
    }

    /* 変更前のコンソール モードを退避する。 */
    if (!GetConsoleMode(context->stdin_handle, &context->orig_in_mode))
    {
        return -1;
    }

    /*
     * ENABLE_VIRTUAL_TERMINAL_INPUT: ESC シーケンスを仮想端末入力として受け取る。
     * ENABLE_ECHO_INPUT: エコー バックを無効化する。
     * ENABLE_LINE_INPUT: 行単位読み取りを無効化し、文字単位に変更する。
     * ENABLE_PROCESSED_INPUT: Ctrl+C をシグナルではなく文字として受け取る。
     */
    new_mode = (context->orig_in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT) &
               ~((DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT));
    if (SetConsoleMode(context->stdin_handle, new_mode))
    {
        context->raw_active = 1;
        return 0;
    }

    return -1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_os_leave_raw(prompt_platform_context *context)
{
    /* raw モードが無効な場合は何もしない。 */
    if (!context->raw_active)
    {
        return 0;
    }

    /* 変更前のコンソール モードを復元する。 */
    if (!SetConsoleMode(context->stdin_handle, context->orig_in_mode))
    {
        return -1;
    }

    context->raw_active = 0;

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_char(prompt_platform_context *context)
{
    cplat_internal_prompt_utf16_input *input = &context->windows_input;

    if (prompt_platform_is_input_canceled(context))
    {
        return PROMPT_PLATFORM_CANCELED;
    }

    /* UTF-8 バイト列が揃うまで入力読み取りを繰り返す。 */
    while (input->offset == input->length)
    {
        unsigned int unit;

        if (input->has_pending_unit)
        {
            unit = input->pending_unit;
            input->has_pending_unit = 0U;
        }
        else
        {
            DWORD n_read;
            WCHAR character;
            int sync_status;
            BOOL ok;

            if (prompt_platform_is_input_canceled(context))
            {
                return PROMPT_PLATFORM_CANCELED;
            }

            /* 入力を 100 [ms] 待機し、タイムアウト時はリサイズ確認契機とする。 */
            DWORD result = WaitForSingleObject(context->stdin_handle, 100U);

            if (prompt_platform_is_input_canceled(context))
            {
                return PROMPT_PLATFORM_CANCELED;
            }

            if (result == WAIT_TIMEOUT)
            {
                return PROMPT_PLATFORM_RESIZE;
            }

            if (result != WAIT_OBJECT_0)
            {
                return PROMPT_PLATFORM_ERROR;
            }

            sync_status = prompt_windows_begin_sync_input(context);
            if (sync_status != 0)
            {
                return sync_status;
            }

            /*
             * サロゲートを個別に UTF-8 変換すると置換文字になるため、UTF-16 で読み取る。
             * see: https://learn.microsoft.com/en-us/windows/console/readconsole
             * see: https://github.com/microsoft/terminal/blob/main/src/host/stream.cpp
             */
            ok = ReadConsoleW(context->stdin_handle, &character, 1U, &n_read, NULL);

            if (prompt_windows_end_sync_input(context) != 0)
            {
                return PROMPT_PLATFORM_CANCELED;
            }

            if (!ok)
            {
                return PROMPT_PLATFORM_ERROR;
            }

            if (n_read == 0U)
            {
                return PROMPT_PLATFORM_EOF;
            }

            unit = (unsigned int)character;
        }

        cplat_internal_prompt_edit_utf16_feed(input, unit);
    }

    return (int)input->bytes[input->offset++];
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_char_nb(prompt_platform_context *context)
{
    if (prompt_platform_is_input_canceled(context))
    {
        return PROMPT_PLATFORM_CANCELED;
    }

    /* バッファー内に未処理の UTF-8 バイト列または保留ユニットがあれば即座に読み取る。 */
    if (context->windows_input.offset < context->windows_input.length || context->windows_input.has_pending_unit)
    {
        return prompt_platform_read_char(context);
    }

    DWORD result = WaitForSingleObject(context->stdin_handle, (DWORD)PROMPT_PLATFORM_SHORT_WAIT_MS); /* 50 [ms] */

    if (prompt_platform_is_input_canceled(context))
    {
        return PROMPT_PLATFORM_CANCELED;
    }

    if (result == WAIT_TIMEOUT)
    {
        return PROMPT_PLATFORM_TIMEOUT;
    }

    if (result != WAIT_OBJECT_0)
    {
        return PROMPT_PLATFORM_ERROR;
    }

    return prompt_platform_read_char(context);
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_line(prompt_platform_context *context, char *buf, const size_t buf_size)
{
    int sync_status;
    int ret;

    if (context == NULL || buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    buf[0] = '\0';
    sync_status = prompt_windows_begin_sync_input(context);
    if (sync_status == PROMPT_PLATFORM_CANCELED)
    {
        return CPLAT_ERR_CANCELED;
    }

    if (sync_status != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    ret = cplat_fgets(buf, buf_size, stdin, NULL);

    if (prompt_windows_end_sync_input(context) != 0)
    {
        if (ferror(stdin) != 0)
        {
            clearerr(stdin);
        }

        buf[0] = '\0';
        return CPLAT_ERR_CANCELED;
    }

    return ret;
}

#endif /* PLATFORM_WINDOWS */
