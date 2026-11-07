/**
 *******************************************************************************
 *  @file           prompt_windows.c
 *  @brief          SetConsoleMode の VT 入力を使用する Windows 向けプロンプト処理を実装します。
 *******************************************************************************
 */

#include "prompt_platform.h"

#if defined(PLATFORM_WINDOWS)

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

            /* 入力を 100 [ms] 待機し、タイムアウト時はリサイズ確認契機とする。 */
            DWORD result = WaitForSingleObject(context->stdin_handle, 100U);
            if (result == WAIT_TIMEOUT)
            {
                return PROMPT_PLATFORM_RESIZE;
            }

            if (result != WAIT_OBJECT_0)
            {
                return PROMPT_PLATFORM_ERROR;
            }

            /*
             * サロゲートを個別に UTF-8 変換すると置換文字になるため、UTF-16 で読み取る。
             * see: https://learn.microsoft.com/en-us/windows/console/readconsole
             * see: https://github.com/microsoft/terminal/blob/main/src/host/stream.cpp
             */
            if (!ReadConsoleW(context->stdin_handle, &character, 1U, &n_read, NULL))
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
    /* バッファー内に未処理の UTF-8 バイト列または保留ユニットがあれば即座に読み取る。 */
    if (context->windows_input.offset < context->windows_input.length || context->windows_input.has_pending_unit)
    {
        return prompt_platform_read_char(context);
    }

    DWORD result = WaitForSingleObject(context->stdin_handle, 50); /* 50 [ms] */
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

#endif /* PLATFORM_WINDOWS */
