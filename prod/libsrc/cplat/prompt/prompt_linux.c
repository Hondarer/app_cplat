/**
 *  @file           prompt_linux.c
 *  @brief          termios の raw mode を使用する Linux 向けプロンプト処理を実装します。
 */

#include <cplat/prompt/prompt_internal.h>

#if defined(PLATFORM_LINUX)

    #include <sys/ioctl.h>
    #include <errno.h>
    #include <signal.h>

static volatile sig_atomic_t s_resize_pending = 0;
static struct sigaction s_prev_sigwinch;
static int s_sigwinch_installed = 0;

static void prompt_sigwinch_handler(int sig)
{
    (void)sig;
    s_resize_pending = 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_platform_get_columns(void)
{
    struct winsize size = {0};
    /* see: https://man7.org/linux/man-pages/man2/TIOCGWINSZ.2const.html */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0U)
    {
        return (size_t)size.ws_col;
    }
    return 80U;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_platform_enter_raw(cplat_prompt *p)
{
    struct termios raw;
    struct sigaction sa;

    if (p->raw_active)
    {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &p->orig_term) != 0)
    {
        return;
    }
    raw = p->orig_term;

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
        return;
    }
    p->raw_active = 1;

    if (!s_sigwinch_installed)
    {
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = prompt_sigwinch_handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGWINCH, &sa, &s_prev_sigwinch);
        s_sigwinch_installed = 1;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_platform_leave_raw(cplat_prompt *p)
{
    if (!p->raw_active)
    {
        return;
    }
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &p->orig_term);
    p->raw_active = 0;

    if (s_sigwinch_installed)
    {
        sigaction(SIGWINCH, &s_prev_sigwinch, NULL);
        s_sigwinch_installed = 0;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_platform_read_char(cplat_prompt *p)
{
    unsigned char c;
    ssize_t n;
    (void)p;

    for (;;)
    {
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
                return -2; /* リサイズ通知 */
            }
            continue;
        }
        return -1;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_platform_read_char_nb(cplat_prompt *p)
{
    fd_set fds;
    struct timeval tv;
    (void)p;

    FD_ZERO(&fds);
    CPLAT_FD_SET(STDIN_FILENO, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 50000; /* 50ms */

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0)
    {
        return -1;
    }
    return cplat_internal_prompt_platform_read_char(p);
}

#endif /* PLATFORM_LINUX */
