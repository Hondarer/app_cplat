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

/* SIGWINCH 受信による端末リサイズ通知の保留フラグ。 */
static volatile sig_atomic_t s_resize_pending = 0;

/* 変更前の SIGWINCH ハンドラー設定。 */
static struct sigaction s_prev_sigwinch;

/* SIGWINCH ハンドラーの登録状態フラグ。 */
static int s_sigwinch_installed = 0;

/*
 * SIGWINCH シグナルを受信した際に、端末サイズ変更の保留フラグを立てる。
 */
static void prompt_sigwinch_handler(int sig)
{
    (void)sig;
    s_resize_pending = 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_os_get_size(int *cols_out, int *rows_out)
{
    struct winsize size = {0};

    /*
     * ioctl() の TIOCGWINSZ 要求を用いて端末サイズを取得する。
     * see: https://man7.org/linux/man-pages/man2/TIOCGWINSZ.2const.html
     */
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
    struct sigaction sa = {0};

    /* すでに raw モードまたはシグナル ハンドラーが有効な場合は重複移行を防ぐ。 */
    if (context->raw_active || s_sigwinch_installed)
    {
        return -1;
    }

    /* 変更前の端末設定を退避する。 */
    if (tcgetattr(STDIN_FILENO, &context->orig_term) != 0)
    {
        return -1;
    }

    raw = context->orig_term;

    /*
     * ICANON: 行単位読み取りを無効化し、文字単位に変更する。
     * ECHO: エコー バックを無効化し、自前で再描画する。
     * ISIG: Ctrl+C をシグナルではなく文字 (KEY_CTRL_C) として受け取る。
     * ICRNL: \r を \n に変換しない。
     * IXON: Ctrl+S や Ctrl+Q による出力一時停止を無効化する。
     */
    raw.c_lflag &= ~((tcflag_t)(ICANON | ECHO | ISIG));
    raw.c_iflag &= ~((tcflag_t)(ICRNL | IXON));
    raw.c_cc[VMIN] = 1;  /* 1 [byte] 以上の入力で read を復帰させる */
    raw.c_cc[VTIME] = 0; /* 入力待ちタイムアウトを設定しない */

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        return -1;
    }

    context->raw_active = 1;

    sa.sa_handler = prompt_sigwinch_handler;

    /* SIGWINCH ハンドラーを登録し、変更前のハンドラーを退避する。 */
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

    /* 変更した端末設定を復元する。 */
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

    /* 登録した SIGWINCH ハンドラーを解除し、変更前のハンドラーを復元する。 */
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

    /* 1 [byte] 読み取るか、リサイズ通知・EOF・エラーが発生するまで繰り返す。 */
    for (;;)
    {
        /* 保留中のリサイズ通知があれば優先して返す。 */
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

        /* シグナル割り込み時は、リサイズ保留があれば通知し、それ以外は読み取りを再試行する。 */
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

    /* タイムアウト付きで入力を監視する。 */
    for (;;)
    {
        int ready;

        if (s_resize_pending)
        {
            s_resize_pending = 0;
            return PROMPT_PLATFORM_RESIZE;
        }

        FD_ZERO(&fds);

        /*
         * STDIN_FILENO は定数 0 なので、FD 範囲検査の偽の 2 分岐には到達しない。
         * see: https://gcovr.com/en/8.6/guide/exclusion-markers.html
         */
        CPLAT_FD_SET(STDIN_FILENO, &fds); /* GCOVR_EXCL_BR_WITHOUT_HIT: 2/4 */
        tv.tv_sec = 0;
        tv.tv_usec = 50000; /* 50 [ms] */
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
