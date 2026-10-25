/**
 *******************************************************************************
 *  @file           prompt_linux.c
 *  @brief          termios の raw mode を使用する Linux 向けプロンプト処理を実装します。
 *******************************************************************************
 */

#include "prompt_platform.h"

#if defined(PLATFORM_LINUX)

    #include <sys/ioctl.h>
    #include <errno.h>
    #include <signal.h>
    #include <string.h>
    #include <unistd.h>
    #include <cplat/crt/select.h>

static volatile sig_atomic_t s_resize_pending = 0;
static struct sigaction s_prev_sigwinch;
static int s_sigwinch_installed = 0;

static void prompt_sigwinch_handler(int sig)
{
    (void)sig;
    s_resize_pending = 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_os_get_size(int *cols_out, int *rows_out)
{
    struct winsize size = {0};
    /* see: https://man7.org/linux/man-pages/man2/TIOCGWINSZ.2const.html */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0)
    {
        *cols_out = (int)size.ws_col;
        *rows_out = (int)size.ws_row;
        return 0;
    }
    return -1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_os_enter_raw(prompt_platform_context *context)
{
    struct termios raw;
    struct sigaction sa;

    if (context->raw_active || s_sigwinch_installed)
    {
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &context->orig_term) != 0)
    {
        return -1;
    }
    raw = context->orig_term;

    /* ICANON : 行単位読み取り無効 (文字単位に変更)
     * ECHO   : エコー無効 (自前で再描画)
     * ISIG   : Ctrl+C をシグナルではなく KEY_CTRL_C として受け取る
     * ICRNL  : \r を \n に変換しない
     * IXON   : Ctrl+S/Q による出力一時停止を無効 */
    raw.c_lflag &= ~((tcflag_t)(ICANON | ECHO | ISIG));
    raw.c_iflag &= ~((tcflag_t)(ICRNL | IXON));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        return -1;
    }
    context->raw_active = 1;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = prompt_sigwinch_handler;
    if (sigemptyset(&sa.sa_mask) != 0 || sigaction(SIGWINCH, &sa, &s_prev_sigwinch) != 0)
    {
        return -1;
    }
    s_sigwinch_installed = 1;
    context->signal_active = 1;
    s_resize_pending = 0;
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_os_leave_raw(prompt_platform_context *context)
{
    int result = 0;
    if (context->raw_active)
    {
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &context->orig_term) == 0)
        {
            context->raw_active = 0;
        }
        else
        {
            result = -1;
        }
    }
    if (context->signal_active)
    {
        if (sigaction(SIGWINCH, &s_prev_sigwinch, NULL) == 0)
        {
            s_sigwinch_installed = 0;
            context->signal_active = 0;
        }
        else
        {
            result = -1;
        }
    }
    if (result == 0)
    {
        s_resize_pending = 0;
    }
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_char(prompt_platform_context *context)
{
    unsigned char c;
    ssize_t n;
    (void)context;

    for (;;)
    {
        if (s_resize_pending)
        {
            s_resize_pending = 0;
            return PROMPT_PLATFORM_RESIZE;
        }
        n = read(STDIN_FILENO, &c, 1);
        if (n == 1)
        {
            return (int)c;
        }
        if (n < 0 && errno == EINTR)
        {
            if (s_resize_pending)
            {
                s_resize_pending = 0;
                return PROMPT_PLATFORM_RESIZE; /* リサイズ通知 */
            }
            continue;
        }
        if (n == 0)
        {
            return PROMPT_PLATFORM_EOF;
        }
        return PROMPT_PLATFORM_ERROR;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_char_nb(prompt_platform_context *context)
{
    fd_set fds;
    struct timeval tv;
    (void)context;

    for (;;)
    {
        int ready;
        if (s_resize_pending)
        {
            s_resize_pending = 0;
            return PROMPT_PLATFORM_RESIZE;
        }
        FD_ZERO(&fds);
        /* STDIN_FILENO は定数 0 なので、FD 範囲検査の偽の 2 分岐には到達しません。
         * see: https://gcovr.com/en/8.6/guide/exclusion-markers.html */
        CPLAT_FD_SET(STDIN_FILENO, &fds); /* GCOVR_EXCL_BR_WITHOUT_HIT: 2/4 */
        tv.tv_sec = 0;
        tv.tv_usec = 50000;
        ready = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (ready > 0)
        {
            return prompt_platform_read_char(context);
        }
        if (ready == 0)
        {
            return PROMPT_PLATFORM_TIMEOUT;
        }
        if (errno != EINTR)
        {
            return PROMPT_PLATFORM_ERROR;
        }
    }
}

#endif /* PLATFORM_LINUX */
