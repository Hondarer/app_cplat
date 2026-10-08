/**
 *  @file           pinned_prompt.c
 *  @brief          端末下部に入力行を固定するプロンプト API を実装します。
 *
 *  入力行、履歴、ステータス領域を管理し、アプリケーション出力後に固定領域を再描画します。
 */

#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/prompt/pinned_prompt.h>

#if defined(PLATFORM_LINUX)
    #include <signal.h>
#endif
#include <cplat/console/console.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/string.h>
#include <cplat/crt/unistd.h>
#include <cplat/prompt/prompt_edit.h>
#include <cplat/sync/sync.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PINNED_PROMPT_INPUT_INITIAL_DEFAULT 256U

#if defined(PLATFORM_LINUX)
    #include <errno.h>
    #include <sys/ioctl.h>
    #include <cplat/crt/select.h>
    #include <termios.h>
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif

typedef enum pinned_prompt_key
{
    PINNED_PROMPT_KEY_CHAR = 0,
    PINNED_PROMPT_KEY_ENTER,
    PINNED_PROMPT_KEY_BACKSPACE,
    PINNED_PROMPT_KEY_DELETE,
    PINNED_PROMPT_KEY_LEFT,
    PINNED_PROMPT_KEY_RIGHT,
    PINNED_PROMPT_KEY_UP,
    PINNED_PROMPT_KEY_DOWN,
    PINNED_PROMPT_KEY_HOME,
    PINNED_PROMPT_KEY_END,
    PINNED_PROMPT_KEY_CTRL_C,
    PINNED_PROMPT_KEY_CLEAR,
    PINNED_PROMPT_KEY_RESIZE,
    PINNED_PROMPT_KEY_UNKNOWN,
    PINNED_PROMPT_KEY_EOF
} pinned_prompt_key;

typedef struct pinned_prompt_layout
{
    int prompt_row;
    int top_status_row;
    int prompt_sep_row;
    int bottom_sep_row;
    int bottom_status_row;
    int main_bottom_row;
    int show_top_status;
    int show_bottom_status;
} pinned_prompt_layout;

typedef struct pinned_prompt_history_ctx
{
    const char *file;
    char **entries;
    char *saved_line;
    size_t count;
    size_t head;
    int line;
    int browse_idx;
} pinned_prompt_history_ctx;

struct cplat_pinned_prompt
{
    cplat_local_lock *mutex;

    char *edit_buf;
    char *prompt_buf;
    char *fmt_buf;

    char *status_top_left;
    char *status_top_right;
    char *status_bottom_left;
    char *status_bottom_right;

    size_t edit_len;
    size_t edit_cap;
    size_t cursor;
    size_t view_start;
    size_t history_max;
    size_t input_max_bytes;
    size_t prompt_cap;
    size_t fmt_cap;
    size_t history_ctx_count;
    size_t history_ctx_cap;

    size_t status_top_left_cap;
    size_t status_top_right_cap;
    size_t status_bottom_left_cap;
    size_t status_bottom_right_cap;

    pinned_prompt_history_ctx *history_contexts;

    int mutex_active;
    int is_tty;
    int raw_active;
    int prompt_visible;
    int status_top_enabled;
    int status_bottom_enabled;
    int status_dirty;
    int cols;
    int rows;
    int prev_main_bottom_row; /* 前回描画時の main_bottom_row (9999=未設定) */

#if defined(PLATFORM_LINUX)
    struct termios orig_term;
    char padding[4];
#elif defined(PLATFORM_WINDOWS)
    HANDLE stdin_handle;
    DWORD orig_in_mode;
    char padding[4];
#endif
};

#define PINNED_PROMPT_HIST_IDX(s, ctx, i) (((ctx)->head + (i)) % (s)->history_max)

static size_t cstr_len(const char *s)
{
    if (s != NULL)
    {
        return strlen(s);
    }
    else
    {
        return 0U;
    }
}

static size_t utf8_char_display_width(const char *buf, size_t len, size_t pos)
{
    unsigned char first_byte;
    unsigned int code_point;

    if (pos >= len)
        return 0U;

    first_byte = (unsigned char)buf[pos];

    if ((first_byte & 0x80U) == 0U)
    {
        return 1U;
    }

    if ((first_byte & 0xE0U) == 0xC0U && pos + 1U < len)
    {
        code_point = ((unsigned int)(first_byte & 0x1FU) << 6U) | (unsigned int)((unsigned char)buf[pos + 1U] & 0x3FU);
        if (code_point >= 0x0300U && code_point <= 0x036FU)
            return 0U;
        return 1U;
    }

    if ((first_byte & 0xF0U) == 0xE0U && pos + 2U < len)
    {
        code_point = ((unsigned int)(first_byte & 0x0FU) << 12U) |
                     ((unsigned int)((unsigned char)buf[pos + 1U] & 0x3FU) << 6U) |
                     (unsigned int)((unsigned char)buf[pos + 2U] & 0x3FU);

        if (code_point >= 0x4E00U && code_point <= 0x9FFFU)
            return 2U;
        if (code_point >= 0x3040U && code_point <= 0x309FU)
            return 2U;
        if (code_point >= 0x30A0U && code_point <= 0x30FFU)
            return 2U;
        if (code_point >= 0x3400U && code_point <= 0x4DBFU)
            return 2U;
        return 1U;
    }

    if ((first_byte & 0xF8U) == 0xF0U && pos + 3U < len)
    {
        code_point = ((unsigned int)(first_byte & 0x07U) << 18U) |
                     ((unsigned int)((unsigned char)buf[pos + 1U] & 0x3FU) << 12U) |
                     ((unsigned int)((unsigned char)buf[pos + 2U] & 0x3FU) << 6U) |
                     (unsigned int)((unsigned char)buf[pos + 3U] & 0x3FU);

        if (code_point >= 0x20000U && code_point <= 0x2FFFFU)
            return 2U;
        return 1U;
    }

    return 1U;
}

static size_t ansi_sgr_sequence_len(const char *buf, size_t len, size_t pos)
{
    size_t i;

    if (pos + 2U >= len || (unsigned char)buf[pos] != 0x1BU || buf[pos + 1U] != '[')
    {
        return 0U;
    }

    i = pos + 2U;
    while (i < len)
    {
        unsigned char ch;

        ch = (unsigned char)buf[i];
        if (ch == 'm')
        {
            return i - pos + 1U;
        }
        if ((ch >= 0x30U && ch <= 0x3FU) || (ch >= 0x20U && ch <= 0x2FU))
        {
            i++;
            continue;
        }
        return 0U;
    }
    return 0U;
}

static size_t pinned_prompt_visible_bytes_from(const char *buf, size_t len, size_t start, size_t max_cols)
{
    size_t pos;
    size_t cols;
    size_t char_width;
    size_t sgr_len;

    pos = start;
    cols = 0U;
    while (pos < len)
    {
        sgr_len = ansi_sgr_sequence_len(buf, len, pos);
        if (sgr_len > 0U)
        {
            pos += sgr_len;
            continue;
        }
        if (cols >= max_cols)
        {
            break;
        }
        char_width = utf8_char_display_width(buf, len, pos);
        if (cols + char_width > max_cols)
            break;
        cols += char_width;
        pos = cplat_internal_prompt_edit_utf8_next_boundary(buf, len, pos);
    }
    return pos - start;
}

static size_t pinned_prompt_display_width_between(const char *buf, size_t len, size_t start, size_t end)
{
    size_t pos;
    size_t width;
    size_t sgr_len;

    if (end > len)
        end = len;

    pos = start;
    width = 0U;
    while (pos < end)
    {
        sgr_len = ansi_sgr_sequence_len(buf, end, pos);
        if (sgr_len > 0U)
        {
            pos += sgr_len;
            continue;
        }
        width += utf8_char_display_width(buf, len, pos);
        pos = cplat_internal_prompt_edit_utf8_next_boundary(buf, len, pos);
    }
    return width;
}

static FILE *pinned_prompt_channel_file(cplat_pinned_prompt_channel channel)
{
    if (channel == CPLAT_PINNED_PROMPT_CHANNEL_STDERR)
    {
        return stderr;
    }
    else
    {
        return stdout;
    }
}

static void pinned_prompt_lock(cplat_pinned_prompt *screen)
{
    if (screen->mutex_active)
    {
        (void)cplat_local_lock_lock(screen->mutex, CPLAT_SYNC_WAIT_FOREVER);
    }
}

static void pinned_prompt_unlock(cplat_pinned_prompt *screen)
{
    if (screen->mutex_active)
    {
        (void)cplat_local_lock_unlock(screen->mutex);
    }
}

#if defined(PLATFORM_LINUX)

static volatile sig_atomic_t s_pinned_resize_pending = 0;
static struct sigaction s_pinned_prev_sigwinch;
static int s_pinned_sigwinch_installed = 0;

static void pinned_prompt_sigwinch_handler(int sig)
{
    (void)sig;
    s_pinned_resize_pending = 1;
}

static int pinned_prompt_platform_is_tty(void)
{
    return cplat_isatty(CPLAT_STREAM_STDIN) && cplat_isatty(CPLAT_STREAM_STDOUT);
}

static void pinned_prompt_platform_get_size(int *cols, int *rows)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0U && ws.ws_row > 0U)
    {
        *cols = (int)ws.ws_col;
        *rows = (int)ws.ws_row;
        return;
    }
    *cols = 80;
    *rows = 24;
}

static void pinned_prompt_platform_enter_raw(cplat_pinned_prompt *screen)
{
    struct termios raw;
    struct sigaction sa;

    if (screen->raw_active)
    {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &screen->orig_term) != 0)
    {
        return;
    }
    raw = screen->orig_term;
    raw.c_lflag &= ~((tcflag_t)(ICANON | ECHO | ISIG));
    raw.c_iflag &= ~((tcflag_t)(ICRNL | IXON));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        return;
    }
    screen->raw_active = 1;

    if (!s_pinned_sigwinch_installed)
    {
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = pinned_prompt_sigwinch_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGWINCH, &sa, &s_pinned_prev_sigwinch);
        s_pinned_sigwinch_installed = 1;
    }
}

static void pinned_prompt_platform_leave_raw(cplat_pinned_prompt *screen)
{
    if (!screen->raw_active)
    {
        return;
    }
    (void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &screen->orig_term);
    screen->raw_active = 0;

    if (s_pinned_sigwinch_installed)
    {
        sigaction(SIGWINCH, &s_pinned_prev_sigwinch, NULL);
        s_pinned_sigwinch_installed = 0;
    }
}

static int pinned_prompt_platform_read_char(cplat_pinned_prompt *screen)
{
    unsigned char c;
    ssize_t n;

    (void)screen;
    for (;;)
    {
        n = read(STDIN_FILENO, &c, 1);
        if (n == 1)
        {
            return (int)c;
        }
        if (n < 0 && errno == EINTR)
        {
            if (s_pinned_resize_pending)
            {
                s_pinned_resize_pending = 0;
                return -2; /* リサイズ通知 */
            }
            continue;
        }
        return -1;
    }
}

static int pinned_prompt_platform_read_char_nb(cplat_pinned_prompt *screen)
{
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    CPLAT_FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 50000;

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0)
    {
        return -1;
    }
    return pinned_prompt_platform_read_char(screen);
}

#elif defined(PLATFORM_WINDOWS)

static int pinned_prompt_platform_is_tty(void)
{
    return cplat_isatty(CPLAT_STREAM_STDIN) && cplat_isatty(CPLAT_STREAM_STDOUT);
}

static void pinned_prompt_platform_get_size(int *cols, int *rows)
{
    HANDLE out_handle;
    CONSOLE_SCREEN_BUFFER_INFO info;

    out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (out_handle != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(out_handle, &info))
    {
        *cols = (int)(info.srWindow.Right - info.srWindow.Left + 1);
        *rows = (int)(info.srWindow.Bottom - info.srWindow.Top + 1);
        return;
    }
    *cols = 80;
    *rows = 24;
}

static void pinned_prompt_platform_enter_raw(cplat_pinned_prompt *screen)
{
    DWORD new_mode;

    if (screen->raw_active)
    {
        return;
    }
    screen->stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (screen->stdin_handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    if (!GetConsoleMode(screen->stdin_handle, &screen->orig_in_mode))
    {
        return;
    }
    new_mode = (screen->orig_in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT) &
               ~((DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT));
    if (SetConsoleMode(screen->stdin_handle, new_mode))
    {
        screen->raw_active = 1;
    }
}

static void pinned_prompt_platform_leave_raw(cplat_pinned_prompt *screen)
{
    if (!screen->raw_active)
    {
        return;
    }
    (void)SetConsoleMode(screen->stdin_handle, screen->orig_in_mode);
    screen->raw_active = 0;
}

static int pinned_prompt_platform_read_char(cplat_pinned_prompt *screen)
{
    DWORD result;
    DWORD n_read;
    char ch;

    result = WaitForSingleObject(screen->stdin_handle, 100U);
    if (result == WAIT_TIMEOUT)
    {
        return -2; /* リサイズ チェック用タイムアウト */
    }
    if (result != WAIT_OBJECT_0)
    {
        return -1;
    }
    if (!ReadFile(screen->stdin_handle, &ch, 1, &n_read, NULL) || n_read == 0U)
    {
        return -1;
    }
    return (unsigned char)ch;
}

static int pinned_prompt_platform_read_char_nb(cplat_pinned_prompt *screen)
{
    DWORD result;

    result = WaitForSingleObject(screen->stdin_handle, 50);
    if (result != WAIT_OBJECT_0)
    {
        return -1;
    }
    return pinned_prompt_platform_read_char(screen);
}

#else

static int pinned_prompt_platform_is_tty(void)
{
    return 0;
}

static void pinned_prompt_platform_get_size(int *cols, int *rows)
{
    *cols = 80;
    *rows = 24;
}

static void pinned_prompt_platform_enter_raw(cplat_pinned_prompt *screen)
{
    (void)screen;
}

static void pinned_prompt_platform_leave_raw(cplat_pinned_prompt *screen)
{
    (void)screen;
}

static int pinned_prompt_platform_read_char(cplat_pinned_prompt *screen)
{
    (void)screen;
    return -1;
}

static int pinned_prompt_platform_read_char_nb(cplat_pinned_prompt *screen)
{
    (void)screen;
    return -1;
}

#endif

static void pinned_prompt_update_size(cplat_pinned_prompt *screen)
{
    pinned_prompt_platform_get_size(&screen->cols, &screen->rows);
}

static int pinned_prompt_set_prompt(cplat_pinned_prompt *screen, const char *prompt_str)
{
    size_t len;
    char *new_buf;

    len = cstr_len(prompt_str);
    if (len + 1U > screen->prompt_cap)
    {
        new_buf = (char *)cplat_realloc(screen->prompt_buf, len + 1U, 1U);
        if (new_buf == NULL)
        {
            return -1;
        }
        screen->prompt_buf = new_buf;
        screen->prompt_cap = len + 1U;
    }
    if (prompt_str != NULL)
    {
        (void)cplat_strcpy(screen->prompt_buf, screen->prompt_cap, prompt_str);
    }
    else
    {
        screen->prompt_buf[0] = '\0';
    }
    return 0;
}

static void pinned_prompt_adjust_view(cplat_pinned_prompt *screen)
{
    size_t prompt_cols;
    size_t input_cols;
    size_t cursor_cols;

    prompt_cols = cstr_len(screen->prompt_buf);
    if ((size_t)screen->cols > prompt_cols + 1U)
    {
        input_cols = (size_t)screen->cols - prompt_cols - 1U;
    }
    else
    {
        input_cols = 1U;
    }

    screen->view_start =
        cplat_internal_prompt_edit_utf8_sanitize_boundary(screen->edit_buf, screen->edit_len, screen->view_start);
    if (screen->cursor < screen->view_start)
    {
        screen->view_start = screen->cursor;
    }
    cursor_cols =
        pinned_prompt_display_width_between(screen->edit_buf, screen->edit_len, screen->view_start, screen->cursor);
    while (cursor_cols > input_cols)
    {
        screen->view_start =
            cplat_internal_prompt_edit_utf8_next_boundary(screen->edit_buf, screen->edit_len, screen->view_start);
        cursor_cols =
            pinned_prompt_display_width_between(screen->edit_buf, screen->edit_len, screen->view_start, screen->cursor);
    }
}

static void pinned_prompt_render_separator(cplat_pinned_prompt *screen, int row)
{
    int i;
    int separator_width;

    (void)printf("\033[%d;1H\033[2K", row);
    separator_width = screen->cols - 1;
    for (i = 0; i < separator_width; i++)
    {
        (void)fputc('-', stdout);
    }
}

static void pinned_prompt_render_status_line(cplat_pinned_prompt *screen, int row, const char *left_content,
                                             const char *right_content)
{
    size_t left_len;
    size_t right_len;
    size_t left_width;
    size_t right_width;
    size_t middle_spaces;
    int i;
    int line_width;

    left_len = cstr_len(left_content);
    right_len = cstr_len(right_content);

    left_width = pinned_prompt_display_width_between(left_content, left_len, 0U, left_len);
    right_width = pinned_prompt_display_width_between(right_content, right_len, 0U, right_len);

    (void)printf("\033[%d;1H\033[2K", row);

    line_width = screen->cols - 1;

    if (left_content != NULL && left_len > 0U)
    {
        (void)fputs(left_content, stdout);
    }

    if (left_width + right_width < (size_t)line_width)
    {
        middle_spaces = (size_t)line_width - left_width - right_width;
        for (i = 0; i < (int)middle_spaces; i++)
        {
            (void)fputc(' ', stdout);
        }
    }

    if (right_content != NULL && right_len > 0U)
    {
        (void)fputs(right_content, stdout);
    }
}

static void pinned_prompt_calc_layout(cplat_pinned_prompt *screen, pinned_prompt_layout *layout)
{
    int rows;
    int first_control_row;

    if (screen->rows > 0)
    {
        rows = screen->rows;
    }
    else
    {
        rows = 1;
    }

    layout->show_bottom_status = screen->status_bottom_enabled && rows >= 3;
    if (layout->show_bottom_status)
    {
        layout->prompt_row = rows - 2;
    }
    else
    {
        layout->prompt_row = rows;
    }
    layout->prompt_sep_row = layout->prompt_row - 1;

    layout->show_top_status = screen->status_top_enabled && layout->prompt_row >= 4;
    if (layout->show_top_status)
    {
        layout->top_status_row = layout->prompt_sep_row - 1;
        first_control_row = layout->top_status_row;
    }
    else
    {
        layout->top_status_row = 0;
        first_control_row = layout->prompt_sep_row;
    }
    layout->main_bottom_row = first_control_row - 1;

    if (layout->show_bottom_status)
    {
        layout->bottom_sep_row = layout->prompt_row + 1;
        layout->bottom_status_row = layout->prompt_row + 2;
    }
    else
    {
        layout->bottom_sep_row = 0;
        layout->bottom_status_row = 0;
    }

    if (layout->prompt_sep_row < 1)
    {
        layout->prompt_sep_row = 1;
    }
    if (layout->main_bottom_row < 1)
    {
        layout->main_bottom_row = 1;
    }
}

static void pinned_prompt_clear_control_area(cplat_pinned_prompt *screen, const pinned_prompt_layout *layout)
{
    int clear_from;

    clear_from = layout->main_bottom_row;
    if (screen->prev_main_bottom_row < clear_from)
    {
        clear_from = screen->prev_main_bottom_row;
    }
    clear_from++;

    if (clear_from <= screen->rows)
    {
        (void)printf("\033[%d;1H\033[J", clear_from);
    }
}

static void pinned_prompt_render_locked(cplat_pinned_prompt *screen)
{
    size_t prompt_cols;
    size_t input_cols;
    size_t visible_bytes;
    size_t cursor_cols;
    size_t cursor_col;
    pinned_prompt_layout layout;

    if (!screen->is_tty || !screen->prompt_visible)
    {
        return;
    }

    pinned_prompt_update_size(screen);
    pinned_prompt_adjust_view(screen);
    pinned_prompt_calc_layout(screen, &layout);

    prompt_cols = cstr_len(screen->prompt_buf);
    if ((size_t)screen->cols > prompt_cols + 1U)
    {
        input_cols = (size_t)screen->cols - prompt_cols - 1U;
    }
    else
    {
        input_cols = 1U;
    }
    visible_bytes =
        pinned_prompt_visible_bytes_from(screen->edit_buf, screen->edit_len, screen->view_start, input_cols);
    cursor_cols =
        pinned_prompt_display_width_between(screen->edit_buf, screen->edit_len, screen->view_start, screen->cursor);
    cursor_col = prompt_cols + cursor_cols + 1U;
    if (cursor_col > (size_t)screen->cols)
    {
        cursor_col = (size_t)screen->cols;
    }

    (void)printf("\033[?25l");

    if (screen->status_dirty)
    {
        pinned_prompt_clear_control_area(screen, &layout);

        if ((!screen->status_top_enabled || layout.show_top_status) &&
            (!screen->status_bottom_enabled || layout.show_bottom_status))
        {
            screen->status_dirty = 0;
        }
    }

    if (layout.show_top_status)
    {
        pinned_prompt_render_status_line(screen, layout.top_status_row, screen->status_top_left,
                                         screen->status_top_right);
    }
    pinned_prompt_render_separator(screen, layout.prompt_sep_row);

    if (layout.show_bottom_status)
    {
        pinned_prompt_render_separator(screen, layout.bottom_sep_row);
        pinned_prompt_render_status_line(screen, layout.bottom_status_row, screen->status_bottom_left,
                                         screen->status_bottom_right);
    }

    (void)printf("\033[%d;1H\033[2K", layout.prompt_row);
    (void)fputs(screen->prompt_buf, stdout);
    if (visible_bytes > 0U)
    {
        (void)fwrite(screen->edit_buf + screen->view_start, 1U, visible_bytes, stdout);
    }

    (void)printf("\033[%d;%zuH\033[?25h", layout.prompt_row, cursor_col);
    (void)fflush(stdout);
    screen->prev_main_bottom_row = layout.main_bottom_row;
}

static void pinned_prompt_hide_prompt_locked(cplat_pinned_prompt *screen)
{
    pinned_prompt_layout layout;

    if (!screen->is_tty || !screen->prompt_visible)
    {
        return;
    }
    pinned_prompt_update_size(screen);
    pinned_prompt_calc_layout(screen, &layout);
    (void)printf("\033[%d;1H\033[2K", layout.prompt_row);
    (void)fflush(stdout);
}

static void pinned_prompt_finish_prompt_locked(cplat_pinned_prompt *screen)
{
    if (!screen->is_tty || !screen->prompt_visible)
    {
        return;
    }
    pinned_prompt_hide_prompt_locked(screen);
    screen->prompt_visible = 0;
}

static void pinned_prompt_cleanup_terminal_locked(cplat_pinned_prompt *screen)
{
    pinned_prompt_layout layout;

    if (!screen->is_tty)
    {
        return;
    }

    pinned_prompt_update_size(screen);
    pinned_prompt_calc_layout(screen, &layout);
    (void)printf("\033[r");
    pinned_prompt_clear_control_area(screen, &layout);
    (void)printf("\033[%d;1H\033[?25h", layout.prompt_row);
    (void)fflush(stdout);
    screen->prompt_visible = 0;
}

static void pinned_prompt_prepare_output_locked(cplat_pinned_prompt *screen)
{
    pinned_prompt_layout layout;

    if (!screen->is_tty)
    {
        return;
    }
    pinned_prompt_update_size(screen);
    pinned_prompt_calc_layout(screen, &layout);
    if (screen->prompt_visible)
    {
        pinned_prompt_hide_prompt_locked(screen);
    }

    if (layout.main_bottom_row < screen->rows)
    {
        (void)printf("\033[1;%dr", layout.main_bottom_row);
    }
    (void)printf("\033[%d;1H\033[2K", layout.main_bottom_row);
    (void)fflush(stdout);
}

static pinned_prompt_key pinned_prompt_read_key(cplat_pinned_prompt *screen, int *ch_out)
{
    int c;
    int c2;
    int c3;
    int c4;

    c = pinned_prompt_platform_read_char(screen);
    if (c == -1)
    {
        return PINNED_PROMPT_KEY_EOF;
    }
    if (c == -2)
    {
        return PINNED_PROMPT_KEY_RESIZE;
    }
    if (c == '\r' || c == '\n')
    {
        return PINNED_PROMPT_KEY_ENTER;
    }
    if (c == 0x03)
    {
        return PINNED_PROMPT_KEY_CTRL_C;
    }
    if (c == 0x7F || c == 0x08)
    {
        return PINNED_PROMPT_KEY_BACKSPACE;
    }
    if (c == 0x1B)
    {
        c2 = pinned_prompt_platform_read_char_nb(screen);
        if (c2 == -1)
        {
            return PINNED_PROMPT_KEY_CLEAR;
        }
        if (c2 == '[')
        {
            c3 = pinned_prompt_platform_read_char_nb(screen);
            switch (c3)
            {
            case 'A':
                return PINNED_PROMPT_KEY_UP;
            case 'B':
                return PINNED_PROMPT_KEY_DOWN;
            case 'C':
                return PINNED_PROMPT_KEY_RIGHT;
            case 'D':
                return PINNED_PROMPT_KEY_LEFT;
            case 'H':
                return PINNED_PROMPT_KEY_HOME;
            case 'F':
                return PINNED_PROMPT_KEY_END;
            case '1':
                c4 = pinned_prompt_platform_read_char_nb(screen);
                if (c4 == '~')
                {
                    return PINNED_PROMPT_KEY_HOME;
                }
                return PINNED_PROMPT_KEY_UNKNOWN;
            case '3':
                c4 = pinned_prompt_platform_read_char_nb(screen);
                if (c4 == '~')
                {
                    return PINNED_PROMPT_KEY_DELETE;
                }
                return PINNED_PROMPT_KEY_UNKNOWN;
            case '4':
                c4 = pinned_prompt_platform_read_char_nb(screen);
                if (c4 == '~')
                {
                    return PINNED_PROMPT_KEY_END;
                }
                return PINNED_PROMPT_KEY_UNKNOWN;
            default:
                return PINNED_PROMPT_KEY_UNKNOWN;
            }
        }
        return PINNED_PROMPT_KEY_UNKNOWN;
    }
    if (c >= 0x20)
    {
        *ch_out = c;
        return PINNED_PROMPT_KEY_CHAR;
    }
    return PINNED_PROMPT_KEY_UNKNOWN;
}

static pinned_prompt_history_ctx *pinned_prompt_find_or_create_history_ctx(cplat_pinned_prompt *screen,
                                                                           const char *file, int line)
{
    size_t i;
    pinned_prompt_history_ctx *ctx;

    for (i = 0U; i < screen->history_ctx_count; i++)
    {
        if (screen->history_contexts[i].file == file && screen->history_contexts[i].line == line)
        {
            return &screen->history_contexts[i];
        }
    }

    if (screen->history_ctx_count == screen->history_ctx_cap)
    {
        size_t new_cap;
        pinned_prompt_history_ctx *new_contexts;

        if (screen->history_ctx_cap != 0U)
        {
            new_cap = screen->history_ctx_cap * 2U;
        }
        else
        {
            new_cap = 4U;
        }
        new_contexts =
            (pinned_prompt_history_ctx *)cplat_realloc(screen->history_contexts, new_cap, sizeof(*new_contexts));
        if (new_contexts == NULL)
        {
            return NULL;
        }
        screen->history_contexts = new_contexts;
        screen->history_ctx_cap = new_cap;
    }

    ctx = &screen->history_contexts[screen->history_ctx_count++];
    memset(ctx, 0, sizeof(*ctx));
    ctx->file = file;
    ctx->line = line;
    ctx->entries = (char **)cplat_calloc(screen->history_max, sizeof(char *));
    ctx->saved_line = (char *)cplat_malloc(screen->input_max_bytes);
    ctx->browse_idx = -1;

    if (ctx->entries == NULL || ctx->saved_line == NULL)
    {
        cplat_free(ctx->entries);
        cplat_free(ctx->saved_line);
        ctx->entries = NULL;
        ctx->saved_line = NULL;
        screen->history_ctx_count--;
        return NULL;
    }
    ctx->saved_line[0] = '\0';
    return ctx;
}

static void pinned_prompt_history_add(cplat_pinned_prompt *screen, pinned_prompt_history_ctx *ctx, const char *line)
{
    size_t len;
    size_t slot;

    if (screen->history_max == 0U || ctx == NULL || line == NULL || line[0] == '\0')
    {
        return;
    }
    if (ctx->count > 0U)
    {
        slot = PINNED_PROMPT_HIST_IDX(screen, ctx, ctx->count - 1U);
        if (ctx->entries[slot] != NULL && strcmp(ctx->entries[slot], line) == 0)
        {
            return;
        }
    }
    if (ctx->count == screen->history_max)
    {
        cplat_free(ctx->entries[ctx->head]);
        ctx->entries[ctx->head] = NULL;
        ctx->head = (ctx->head + 1U) % screen->history_max;
    }
    else
    {
        ctx->count++;
    }

    slot = PINNED_PROMPT_HIST_IDX(screen, ctx, ctx->count - 1U);
    len = strlen(line) + 1U;
    ctx->entries[slot] = (char *)cplat_malloc(len);
    if (ctx->entries[slot] != NULL)
    {
        (void)cplat_strcpy(ctx->entries[slot], len, line);
    }
}

static void pinned_prompt_set_edit_line(cplat_pinned_prompt *screen, const char *line)
{
    size_t len;

    len = cstr_len(line);
    if (cplat_internal_prompt_edit_ensure_capacity(&screen->edit_buf, &screen->edit_cap, screen->input_max_bytes,
                                                   len + 1U) != 0)
    {
        len = screen->edit_cap - 1U;
    }
    if (line != NULL && len > 0U)
    {
        memcpy(screen->edit_buf, line, len);
    }
    screen->edit_buf[len] = '\0';
    screen->edit_len = len;
    screen->cursor = len;
    screen->view_start = 0U;
}

static void pinned_prompt_clear_edit_line(cplat_pinned_prompt *screen, pinned_prompt_history_ctx *ctx)
{
    screen->edit_len = 0U;
    screen->edit_buf[0] = '\0';
    screen->cursor = 0U;
    screen->view_start = 0U;
    if (ctx != NULL)
    {
        ctx->browse_idx = -1;
    }
}

static void pinned_prompt_history_prev(cplat_pinned_prompt *screen, pinned_prompt_history_ctx *ctx)
{
    const char *entry;
    size_t slot;

    if (ctx == NULL || ctx->count == 0U)
    {
        return;
    }
    if (ctx->browse_idx == -1)
    {
        (void)cplat_strcpy(ctx->saved_line, screen->edit_cap, screen->edit_buf);
        ctx->browse_idx = (int)ctx->count - 1;
    }
    else if (ctx->browse_idx > 0)
    {
        ctx->browse_idx--;
    }
    else
    {
        return;
    }
    slot = PINNED_PROMPT_HIST_IDX(screen, ctx, (size_t)ctx->browse_idx);
    entry = ctx->entries[slot];
    if (entry != NULL)
    {
        pinned_prompt_set_edit_line(screen, entry);
    }
}

static void pinned_prompt_history_next(cplat_pinned_prompt *screen, pinned_prompt_history_ctx *ctx)
{
    const char *entry;
    size_t slot;

    if (ctx == NULL || ctx->browse_idx == -1)
    {
        return;
    }
    if (ctx->browse_idx < (int)ctx->count - 1)
    {
        ctx->browse_idx++;
        slot = PINNED_PROMPT_HIST_IDX(screen, ctx, (size_t)ctx->browse_idx);
        entry = ctx->entries[slot];
        if (entry != NULL)
        {
            pinned_prompt_set_edit_line(screen, entry);
        }
    }
    else
    {
        pinned_prompt_set_edit_line(screen, ctx->saved_line);
        ctx->browse_idx = -1;
    }
}

static void pinned_prompt_insert_byte(cplat_pinned_prompt *screen, int ch)
{
    if (cplat_internal_prompt_edit_ensure_capacity(&screen->edit_buf, &screen->edit_cap, screen->input_max_bytes,
                                                   screen->edit_len + 2U) != 0)
    {
        return;
    }
    memmove(screen->edit_buf + screen->cursor + 1U, screen->edit_buf + screen->cursor,
            screen->edit_len - screen->cursor + 1U);
    screen->edit_buf[screen->cursor] = (char)ch;
    screen->cursor++;
    screen->edit_len++;
}

static void pinned_prompt_backspace(cplat_pinned_prompt *screen)
{
    size_t prev;

    if (screen->cursor == 0U)
    {
        return;
    }
    prev = cplat_internal_prompt_edit_utf8_prev_boundary(screen->edit_buf, screen->cursor);
    memmove(screen->edit_buf + prev, screen->edit_buf + screen->cursor, screen->edit_len - screen->cursor + 1U);
    screen->edit_len -= screen->cursor - prev;
    screen->cursor = prev;
}

static void pinned_prompt_delete(cplat_pinned_prompt *screen)
{
    size_t next;

    if (screen->cursor >= screen->edit_len)
    {
        return;
    }
    next = cplat_internal_prompt_edit_utf8_next_boundary(screen->edit_buf, screen->edit_len, screen->cursor);
    memmove(screen->edit_buf + screen->cursor, screen->edit_buf + next, screen->edit_len - next + 1U);
    screen->edit_len -= next - screen->cursor;
}

static int pinned_prompt_format_prompt(cplat_pinned_prompt *screen, const char *fmt, va_list ap)
{
    va_list ap_copy;
    int needed;
    char *new_buf;

    if (screen->fmt_buf == NULL)
    {
        screen->fmt_cap = 256U;
        screen->fmt_buf = (char *)cplat_malloc(screen->fmt_cap);
        if (screen->fmt_buf == NULL)
        {
            return -1;
        }
    }

    for (;;)
    {
        va_copy(ap_copy, ap);
        if (fmt != NULL)
        {
            needed = vsnprintf(screen->fmt_buf, screen->fmt_cap, fmt, ap_copy); /* 置換対象外: 必要長の照会 */
        }
        else
        {
            needed = 0;
        }
        va_end(ap_copy);

        if (needed < 0)
        {
            screen->fmt_buf[0] = '\0';
            return 0;
        }
        if ((size_t)needed < screen->fmt_cap)
        {
            return 0;
        }
        new_buf = (char *)cplat_realloc(screen->fmt_buf, (size_t)needed + 1U, 1U);
        if (new_buf == NULL)
        {
            screen->fmt_buf[screen->fmt_cap - 1U] = '\0';
            return 0;
        }
        screen->fmt_buf = new_buf;
        screen->fmt_cap = (size_t)needed + 1U;
    }
}

/**
 *  @brief          非 TTY または raw 移行失敗時に 1 行を読み取ります。
 *  @param[out]     buf         入力の格納先です。
 *  @param[in]      buf_size    @p buf のサイズです。
 *  @param[in]      prompt_str  表示するプロンプトです。NULL のときは表示しません。
 *  @return         @ref cplat_fgets の結果コードです。
 */
static int pinned_prompt_readline_fallback(char *buf, const size_t buf_size, const char *prompt_str)
{
    if (prompt_str != NULL)
    {
        (void)fputs(prompt_str, stdout);
        (void)fflush(stdout);
    }
    return cplat_fgets(buf, buf_size, stdin, NULL);
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_pinned_prompt *cplat_pinned_prompt_create(const cplat_pinned_prompt_options *options)
{
    cplat_pinned_prompt *screen;
    size_t history_max;
    size_t input_initial_capacity;
    size_t input_max_bytes;

    cplat_console_init();

    screen = (cplat_pinned_prompt *)cplat_calloc(1U, sizeof(*screen));
    if (screen == NULL)
    {
        return NULL;
    }

    if (options != NULL)
    {
        cplat_internal_prompt_edit_resolve_options(options->input.history_max, options->input.input_initial_capacity,
                                                   options->input.input_max_bytes, PINNED_PROMPT_INPUT_INITIAL_DEFAULT,
                                                   &history_max, &input_initial_capacity, &input_max_bytes);
    }
    else
    {
        cplat_internal_prompt_edit_resolve_options(0U, 0U, 0U, PINNED_PROMPT_INPUT_INITIAL_DEFAULT, &history_max,
                                                   &input_initial_capacity, &input_max_bytes);
    }
    screen->history_max = history_max;
    screen->input_max_bytes = input_max_bytes;
    screen->edit_cap = input_initial_capacity;
    screen->is_tty = pinned_prompt_platform_is_tty();
    pinned_prompt_update_size(screen);

    if (cplat_local_lock_create(&screen->mutex) != CPLAT_OK)
    {
        cplat_free(screen);
        return NULL;
    }
    screen->mutex_active = 1;

    screen->edit_buf = (char *)cplat_malloc(screen->edit_cap);
    screen->prompt_buf = (char *)cplat_malloc(1U);
    screen->status_top_left = (char *)cplat_malloc(1U);
    screen->status_top_right = (char *)cplat_malloc(1U);
    screen->status_bottom_left = (char *)cplat_malloc(1U);
    screen->status_bottom_right = (char *)cplat_malloc(1U);
    if (screen->edit_buf == NULL || screen->prompt_buf == NULL || screen->status_top_left == NULL ||
        screen->status_top_right == NULL || screen->status_bottom_left == NULL || screen->status_bottom_right == NULL)
    {
        cplat_pinned_prompt_dispose(screen);
        return NULL;
    }
    screen->edit_buf[0] = '\0';
    screen->prompt_buf[0] = '\0';
    screen->prompt_cap = 1U;
    screen->status_top_left[0] = '\0';
    screen->status_top_left_cap = 1U;
    screen->status_top_right[0] = '\0';
    screen->status_top_right_cap = 1U;
    screen->status_bottom_left[0] = '\0';
    screen->status_bottom_left_cap = 1U;
    screen->status_bottom_right[0] = '\0';
    screen->status_bottom_right_cap = 1U;
    screen->status_dirty = 1;
    screen->prev_main_bottom_row = 9999;

    return screen;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_pinned_prompt_dispose(cplat_pinned_prompt *screen)
{
    size_t i;
    size_t j;

    if (screen == NULL)
    {
        return;
    }
    pinned_prompt_lock(screen);
    pinned_prompt_cleanup_terminal_locked(screen);
    pinned_prompt_platform_leave_raw(screen);
    pinned_prompt_unlock(screen);

    if (screen->history_contexts != NULL)
    {
        for (i = 0U; i < screen->history_ctx_count; i++)
        {
            pinned_prompt_history_ctx *ctx;

            ctx = &screen->history_contexts[i];
            if (ctx->entries != NULL)
            {
                for (j = 0U; j < screen->history_max; j++)
                {
                    cplat_free(ctx->entries[j]);
                }
            }
            cplat_free(ctx->entries);
            cplat_free(ctx->saved_line);
        }
    }
    cplat_free(screen->history_contexts);
    cplat_free(screen->edit_buf);
    cplat_free(screen->prompt_buf);
    cplat_free(screen->fmt_buf);
    cplat_free(screen->status_top_left);
    cplat_free(screen->status_top_right);
    cplat_free(screen->status_bottom_left);
    cplat_free(screen->status_bottom_right);
    if (screen->mutex_active)
    {
        (void)cplat_local_lock_dispose(screen->mutex);
    }
    cplat_free(screen);
}
/**
 *  @brief          入力欄の初期値を指定して 1 行のコマンド入力を受け取ります。
 *  @param[in,out]  screen          固定プロンプト画面の状態。
 *  @param[out]     buf             入力結果の格納先。
 *  @param[in]      buf_size        @p buf のバイト数。
 *  @param[in]      prompt_str      プロンプト。NULL の場合は表示しません。
 *  @param[in]      initial_text    入力欄の初期値。検証済みであること。NULL は空文字列として扱います。
 *  @param[in]      initial_length  @p initial_text のバイト数 (NUL を除く) です。
 *  @param[in]      file            呼び出し元のファイル名。
 *  @param[in]      line            呼び出し元の行番号。
 *
 *  cplat_pinned_prompt_readline_at() と cplat_pinned_prompt_readline_with_initial_at() が共有する本体です。\n
 *  TTY でない場合は初期値を使用しません。入力側が行全体を与えるためです。\n
 *  履歴をさかのぼる最初の操作で編集中の行を退避するため、初期値 (編集途中ならその内容) へ ↓ で戻れます。
 */
static int pinned_prompt_readline_core(cplat_pinned_prompt *screen, char *buf, const size_t buf_size,
                                       const char *prompt_str, const char *initial_text, const size_t initial_length,
                                       const char *file, int line)
{
    int done;
    int result;
    pinned_prompt_history_ctx *history_ctx;

    buf[0] = '\0';

    if (!screen->is_tty)
    {
        return pinned_prompt_readline_fallback(buf, buf_size, prompt_str);
    }

    pinned_prompt_platform_enter_raw(screen);
    if (!screen->raw_active)
    {
        return pinned_prompt_readline_fallback(buf, buf_size, prompt_str);
    }

    pinned_prompt_lock(screen);
    history_ctx = pinned_prompt_find_or_create_history_ctx(screen, file, line);
    if (history_ctx == NULL)
    {
        pinned_prompt_unlock(screen);
        pinned_prompt_platform_leave_raw(screen);
        return CPLAT_ERR_UNKNOWN;
    }
    if (pinned_prompt_set_prompt(screen, prompt_str) != 0)
    {
        pinned_prompt_unlock(screen);
        pinned_prompt_platform_leave_raw(screen);
        return CPLAT_ERR_UNKNOWN;
    }
    screen->edit_len = 0U;
    screen->edit_buf[0] = '\0';
    screen->cursor = 0U;
    screen->view_start = 0U;
    history_ctx->browse_idx = -1;
    if (initial_length > 0U)
    {
        /* 検証済みの長さは上限以内のため、確保に失敗するのはメモリ不足のときだけ */
        if (cplat_internal_prompt_edit_ensure_capacity(&screen->edit_buf, &screen->edit_cap, screen->input_max_bytes,
                                                       initial_length + 1U) != 0)
        {
            pinned_prompt_unlock(screen);
            pinned_prompt_platform_leave_raw(screen);
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        /* カーソルは末尾に置く。表示範囲は描画時にカーソルが見える位置へ調整される */
        pinned_prompt_set_edit_line(screen, initial_text);
    }
    screen->prompt_visible = 1;
    pinned_prompt_render_locked(screen);
    pinned_prompt_unlock(screen);

    done = 0;
    result = CPLAT_ERR_UNKNOWN;
    while (!done)
    {
        int ch;
        pinned_prompt_key key;

        ch = 0;
        key = pinned_prompt_read_key(screen, &ch);

        pinned_prompt_lock(screen);
        switch (key)
        {
        case PINNED_PROMPT_KEY_ENTER:
        {
            size_t copy;

            if (screen->edit_len < buf_size - 1U)
            {
                copy = screen->edit_len;
            }
            else
            {
                copy = buf_size - 1U;
            }
            if (copy > 0U)
            {
                memcpy(buf, screen->edit_buf, copy);
            }
            buf[copy] = '\0';
            pinned_prompt_history_add(screen, history_ctx, screen->edit_buf);
            pinned_prompt_finish_prompt_locked(screen);
            result = CPLAT_OK;
            done = 1;
            break;
        }
        case PINNED_PROMPT_KEY_EOF:
            buf[0] = '\0';
            pinned_prompt_finish_prompt_locked(screen);
            result = CPLAT_ERR_EOF;
            done = 1;
            break;
        case PINNED_PROMPT_KEY_CTRL_C:
            buf[0] = '\0';
            pinned_prompt_cleanup_terminal_locked(screen);
            result = CPLAT_ERR_CANCELED;
            done = 1;
            break;
        case PINNED_PROMPT_KEY_CLEAR:
            pinned_prompt_clear_edit_line(screen, history_ctx);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_BACKSPACE:
            pinned_prompt_backspace(screen);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_DELETE:
            pinned_prompt_delete(screen);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_LEFT:
            screen->cursor = cplat_internal_prompt_edit_utf8_prev_boundary(screen->edit_buf, screen->cursor);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_RIGHT:
            screen->cursor =
                cplat_internal_prompt_edit_utf8_next_boundary(screen->edit_buf, screen->edit_len, screen->cursor);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_HOME:
            screen->cursor = 0U;
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_END:
            screen->cursor = screen->edit_len;
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_UP:
            pinned_prompt_history_prev(screen, history_ctx);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_DOWN:
            pinned_prompt_history_next(screen, history_ctx);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_CHAR:
            pinned_prompt_insert_byte(screen, ch);
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_RESIZE:
            pinned_prompt_update_size(screen);
            screen->status_dirty = 1;
            pinned_prompt_render_locked(screen);
            break;
        case PINNED_PROMPT_KEY_UNKNOWN:
        default:
            pinned_prompt_render_locked(screen);
            break;
        }
        pinned_prompt_unlock(screen);
    }

    pinned_prompt_platform_leave_raw(screen);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_pinned_prompt_readline_at(cplat_pinned_prompt *screen, char *buf, const size_t buf_size,
                                    const char *prompt_str, const char *file, int line)
{
    if (screen == NULL || buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return pinned_prompt_readline_core(screen, buf, buf_size, prompt_str, NULL, 0U, file, line);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_pinned_prompt_readline_with_initial_at(cplat_pinned_prompt *screen, char *buf, const size_t buf_size,
                                                 const char *prompt_str, const char *initial_text, const char *file,
                                                 int line)
{
    size_t initial_length;
    int ret;

    if (screen == NULL || buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    buf[0] = '\0';

    /* TTY かどうかによらず同じ規則で検証し、契約を経路に依存させない */
    ret = cplat_internal_prompt_edit_validate_initial_text(initial_text, screen->input_max_bytes, &initial_length);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    return pinned_prompt_readline_core(screen, buf, buf_size, prompt_str, initial_text, initial_length, file, line);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_pinned_prompt_readline_fmt_at(cplat_pinned_prompt *screen, char *buf, size_t buf_size, const char *file,
                                        int line, const char *fmt, ...)
{
    va_list ap;
    int rc;

    if (screen == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    va_start(ap, fmt);
    rc = pinned_prompt_format_prompt(screen, fmt, ap);
    va_end(ap);
    if (rc != 0)
    {
        return cplat_pinned_prompt_readline_at(screen, buf, buf_size, "", file, line);
    }
    return cplat_pinned_prompt_readline_at(screen, buf, buf_size, screen->fmt_buf, file, line);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_pinned_prompt_write(cplat_pinned_prompt *screen, cplat_pinned_prompt_channel channel, const void *data,
                              size_t size, size_t *written_out)
{
    FILE *out;
    size_t written;

    if (written_out != NULL)
    {
        *written_out = 0U;
    }
    if (screen == NULL || (data == NULL && size != 0U))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    out = pinned_prompt_channel_file(channel);
    if (!screen->is_tty)
    {
        if (size > 0U)
        {
            written = fwrite(data, 1U, size, out);
        }
        else
        {
            written = 0U;
        }
        (void)fflush(out);
        if (written_out != NULL)
        {
            *written_out = written;
        }
        if (written != size)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        return CPLAT_OK;
    }

    pinned_prompt_lock(screen);
    pinned_prompt_prepare_output_locked(screen);
    if (size > 0U)
    {
        written = fwrite(data, 1U, size, out);
    }
    else
    {
        written = 0U;
    }
    (void)fflush(out);
    (void)printf("\033[r");
    pinned_prompt_render_locked(screen);
    pinned_prompt_unlock(screen);

    if (written_out != NULL)
    {
        *written_out = written;
    }
    if (written != size)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_pinned_prompt_printf(cplat_pinned_prompt *screen, cplat_pinned_prompt_channel channel, const char *fmt, ...)
{
    va_list ap;
    va_list ap_copy;
    int needed;
    char *buf;
    size_t written = 0U;
    int write_result;

    if (screen == NULL)
    {
        return -1;
    }

    va_start(ap, fmt);
    va_copy(ap_copy, ap);
    if (fmt != NULL)
    {
        needed = vsnprintf(NULL, 0U, fmt, ap_copy); /* 置換対象外: 必要長の照会 */
    }
    else
    {
        needed = 0;
    }
    va_end(ap_copy);
    if (needed < 0)
    {
        va_end(ap);
        return -1;
    }

    buf = (char *)cplat_malloc((size_t)needed + 1U);
    if (buf == NULL)
    {
        va_end(ap);
        return -1;
    }
    if (fmt != NULL)
    {
        (void)cplat_vsnprintf(buf, (size_t)needed + 1U, fmt, ap);
    }
    else
    {
        buf[0] = '\0';
    }
    va_end(ap);

    write_result = cplat_pinned_prompt_write(screen, channel, buf, (size_t)needed, &written);
    cplat_free(buf);
    if (write_result != CPLAT_OK)
    {
        return -1;
    }
    return (int)written;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_pinned_prompt_status_enable(cplat_pinned_prompt *screen, cplat_pinned_prompt_status_position position,
                                      int enable)
{
    if (screen == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    pinned_prompt_lock(screen);
    if (position == CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP)
    {
        if (enable)
        {
            screen->status_top_enabled = 1;
        }
        else
        {
            screen->status_top_enabled = 0;
        }
    }
    else if (position == CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM)
    {
        if (enable)
        {
            screen->status_bottom_enabled = 1;
        }
        else
        {
            screen->status_bottom_enabled = 0;
        }
    }
    else
    {
        pinned_prompt_unlock(screen);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    screen->status_dirty = 1;
    pinned_prompt_render_locked(screen);
    pinned_prompt_unlock(screen);
    return CPLAT_OK;
}

static int pinned_prompt_set_status_content(char **buf, size_t *cap, const char *content)
{
    size_t len;
    char *new_buf;

    len = cstr_len(content);
    if (len + 1U > *cap)
    {
        new_buf = (char *)cplat_realloc(*buf, len + 1U, 1U);
        if (new_buf == NULL)
        {
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        *buf = new_buf;
        *cap = len + 1U;
    }
    if (content != NULL)
    {
        (void)cplat_strcpy(*buf, *cap, content);
    }
    else
    {
        (*buf)[0] = '\0';
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_pinned_prompt_status_set(cplat_pinned_prompt *screen, cplat_pinned_prompt_status_position position,
                                   cplat_pinned_prompt_status_align align, const char *content)
{
    int ret;

    if (screen == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    pinned_prompt_lock(screen);
    if (position == CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP)
    {
        if (align == CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT)
        {
            ret = pinned_prompt_set_status_content(&screen->status_top_left, &screen->status_top_left_cap, content);
        }
        else if (align == CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT)
        {
            ret = pinned_prompt_set_status_content(&screen->status_top_right, &screen->status_top_right_cap, content);
        }
        else
        {
            pinned_prompt_unlock(screen);
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }
    else if (position == CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM)
    {
        if (align == CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT)
        {
            ret =
                pinned_prompt_set_status_content(&screen->status_bottom_left, &screen->status_bottom_left_cap, content);
        }
        else if (align == CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT)
        {
            ret = pinned_prompt_set_status_content(&screen->status_bottom_right, &screen->status_bottom_right_cap,
                                                   content);
        }
        else
        {
            pinned_prompt_unlock(screen);
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }
    else
    {
        pinned_prompt_unlock(screen);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (ret == CPLAT_OK)
    {
        screen->status_dirty = 1;
        pinned_prompt_render_locked(screen);
    }
    pinned_prompt_unlock(screen);
    return ret;
}
