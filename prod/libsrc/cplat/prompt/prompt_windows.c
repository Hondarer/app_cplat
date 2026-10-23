/**
 *  @file           prompt_windows.c
 *  @brief          SetConsoleMode の VT 入力を使用する Windows 向けプロンプト処理を実装します。
 */

#include <cplat/prompt/prompt_internal.h>

#if defined(PLATFORM_WINDOWS)

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_platform_get_columns(void)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    /* see: https://learn.microsoft.com/en-us/windows/console/getconsolescreenbufferinfo */
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) && info.srWindow.Right >= info.srWindow.Left)
    {
        return (size_t)(info.srWindow.Right - info.srWindow.Left) + 1U;
    }
    return 80U;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_platform_enter_raw(cplat_prompt *p)
{
    DWORD new_mode;
    if (p->raw_active)
    {
        return;
    }
    p->stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (p->stdin_handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    if (!GetConsoleMode(p->stdin_handle, &p->orig_in_mode))
    {
        return;
    }
    /* ENABLE_VIRTUAL_TERMINAL_INPUT : ESC シーケンスを仮想端末入力として受け取る
     * ENABLE_ECHO_INPUT              : エコー無効
     * ENABLE_LINE_INPUT              : 行単位読み取り無効 (文字単位に変更)
     * ENABLE_PROCESSED_INPUT         : Ctrl+C をシグナルではなく文字として受け取る */
    new_mode = (p->orig_in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT) &
               ~((DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT));
    if (SetConsoleMode(p->stdin_handle, new_mode))
    {
        p->raw_active = 1;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_platform_leave_raw(cplat_prompt *p)
{
    if (!p->raw_active)
    {
        return;
    }
    SetConsoleMode(p->stdin_handle, p->orig_in_mode);
    p->raw_active = 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_platform_read_char(cplat_prompt *p)
{
    cplat_internal_prompt_utf16_input *input = &p->windows_input;
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
            DWORD result = WaitForSingleObject(p->stdin_handle, 100U);
            if (result == WAIT_TIMEOUT)
            {
                return -2;
            }
            if (result != WAIT_OBJECT_0)
            {
                return -1;
            }
            /* サロゲートを個別に UTF-8 変換すると置換文字になるため、UTF-16 で読み取ります。
             * see: https://learn.microsoft.com/en-us/windows/console/readconsole
             * see: https://github.com/microsoft/terminal/blob/main/src/host/stream.cpp */
            if (!ReadConsoleW(p->stdin_handle, &character, 1U, &n_read, NULL) || n_read == 0U)
            {
                return -1;
            }
            unit = (unsigned int)character;
        }
        cplat_internal_prompt_edit_utf16_feed(input, unit);
    }
    return (int)input->bytes[input->offset++];
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_platform_read_char_nb(cplat_prompt *p)
{
    if (p->windows_input.offset < p->windows_input.length || p->windows_input.has_pending_unit)
    {
        return cplat_internal_prompt_platform_read_char(p);
    }

    DWORD result = WaitForSingleObject(p->stdin_handle, 50); /* 50ms */
    if (result != WAIT_OBJECT_0)
    {
        return -1;
    }
    return cplat_internal_prompt_platform_read_char(p);
}

#endif /* PLATFORM_WINDOWS */
