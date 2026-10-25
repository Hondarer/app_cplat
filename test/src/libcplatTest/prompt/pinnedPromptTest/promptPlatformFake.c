/* 描画の回帰試験では実共通処理へ委譲し、境界試験では失敗を注入します。
 * OS の分岐網羅は promptLinuxTest / promptWindowsTest が担当します。 */
#include "prompt_platform.h"
#include "pinned_prompt.inject.h"
#include <cplat/crt/unistd.h>
#include <string.h>
#if defined(PLATFORM_LINUX)
    #include <signal.h>
    #include <unistd.h>
    #include <sys/ioctl.h>
    #include <sys/select.h>
    #define _IN_OVERRIDE_HEADER_TERMIOS_H
    #include <mock_termios.h>
    #define _IN_OVERRIDE_HEADER_UNISTD_H
    #include <mock_unistd.h>
    #define _IN_OVERRIDE_HEADER_SIGNAL_H
    #include <mock_signal.h>
    #define _IN_OVERRIDE_HEADER_SYS_IOCTL_H
    #include <mock_ioctl.h>
    #define _IN_OVERRIDE_HEADER_SYS_SELECT_H
    #include <sys/mock_select.h>
#elif defined(PLATFORM_WINDOWS)
    #define _IN_OVERRIDE_HEADER_WINDOWS_H
    #include <mock_windows.h>
#endif

int dependency_enter_raw(prompt_platform_context *context);
int dependency_leave_raw(prompt_platform_context *context);
int dependency_read_char(prompt_platform_context *context);
int dependency_read_char_nb(prompt_platform_context *context);
#define prompt_platform_enter_raw dependency_enter_raw
#define prompt_platform_leave_raw dependency_leave_raw
#include "../../../../../prod/libsrc/cplat/prompt/prompt_platform.c"
#undef prompt_platform_enter_raw
#undef prompt_platform_leave_raw
#define prompt_platform_read_char    dependency_read_char
#define prompt_platform_read_char_nb dependency_read_char_nb
#if defined(PLATFORM_LINUX)
    #include "../../../../../prod/libsrc/cplat/prompt/prompt_linux.c"
void test_pinned_prompt_reset_platform_state(void)
{
    s_resize_pending = 0;
    memset(&s_prev_sigwinch, 0, sizeof(s_prev_sigwinch));
    s_sigwinch_installed = 0;
}
void test_pinned_prompt_set_resize_pending(int value)
{
    s_resize_pending = (sig_atomic_t)value;
}
int test_pinned_prompt_resize_pending(void)
{
    return (int)s_resize_pending;
}
void test_pinned_prompt_set_sigwinch_installed(int value)
{
    s_sigwinch_installed = value;
}
void test_pinned_prompt_raise_resize_handler(void)
{
    prompt_sigwinch_handler(SIGWINCH);
}
#elif defined(PLATFORM_WINDOWS)
    #include "../../../../../prod/libsrc/cplat/prompt/prompt_windows.c"
#endif
#undef prompt_platform_read_char
#undef prompt_platform_read_char_nb
static const int *s_input_results = NULL;
static size_t s_input_count = 0;
static size_t s_input_offset = 0;
static int s_enter_result = 0;
static int s_leave_result = 0;
void test_pinned_prompt_failures(int enter_result, int leave_result)
{
    s_enter_result = enter_result;
    s_leave_result = leave_result;
}
int prompt_platform_enter_raw(prompt_platform_context *context)
{
    if (s_enter_result == 2)
    {
        return 0;
    }
    if (s_enter_result != 0)
    {
        return s_enter_result;
    }
    return dependency_enter_raw(context);
}
int prompt_platform_leave_raw(prompt_platform_context *context)
{
    if (s_leave_result != 0)
    {
        return s_leave_result;
    }
    return dependency_leave_raw(context);
}

void test_pinned_prompt_input_results(const int *results, size_t count)
{
    s_input_results = results;
    s_input_count = count;
    s_input_offset = 0;
}
int prompt_platform_read_char(prompt_platform_context *context)
{
    if (s_input_results != NULL)
    {
        if (s_input_offset == s_input_count)
        {
            return PROMPT_PLATFORM_EOF;
        }
        return s_input_results[s_input_offset++];
    }
    return dependency_read_char(context);
}
int prompt_platform_read_char_nb(prompt_platform_context *context)
{
    if (s_input_results != NULL)
    {
        return prompt_platform_read_char(context);
    }
    return dependency_read_char_nb(context);
}
