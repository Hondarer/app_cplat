/**
 *******************************************************************************
 *  @file           prompt_linux.c
 *  @brief          termios の raw mode を使用する Linux 向けプロンプト処理を実装します。
 *******************************************************************************
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

#include "prompt_platform.h"

#if defined(PLATFORM_LINUX)

    #include <sys/ioctl.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <signal.h>
    #include <string.h>
    #include <unistd.h>
    #include <cplat/base/result.h>

/* SIGWINCH 受信による端末リサイズ通知の保留フラグ。 */
static volatile sig_atomic_t s_resize_pending = 0;

/* 変更前の SIGWINCH ハンドラー設定。 */
static struct sigaction s_prev_sigwinch;

/* SIGWINCH ハンドラーの登録状態フラグ。 */
static int s_sigwinch_installed = 0;

    /* poll() で監視するファイル記述子数 (標準入力 + 中止通知パイプ) */
    #define PROMPT_LINUX_POLL_FD_COUNT 2U

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

/* テスト差し替えにも対応する pipe2() 呼び出し関数型と既定実装。 */
typedef int (*prompt_linux_pipe2_fn)(int fds[2], int flags);

static int prompt_linux_default_pipe2(int fds[2], const int flags)
{
    return pipe2(fds, flags);
}

static prompt_linux_pipe2_fn s_pipe2_fn = prompt_linux_default_pipe2;

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_init_cancelable(prompt_platform_context *context)
{
    int fds[2];

    if (context == NULL)
    {
        return -1;
    }

    context->cancel_pipe[0] = -1;
    context->cancel_pipe[1] = -1;
    context->cancel_pipe_active = 0;
    cplat_atomic_store_i32(&context->cancel_requested, 0, CPLAT_MEMORY_ORDER_RELAXED);

    /*
     * 他スレッドの fork()/exec() へ FD を漏らさないよう、O_NONBLOCK と O_CLOEXEC を
     * 付けて自己通知パイプを原子的に生成する。
     * see: https://man7.org/linux/man-pages/man2/pipe.2.html
     */
    if (s_pipe2_fn(fds, O_NONBLOCK | O_CLOEXEC) != 0)
    {
        return -1;
    }

    context->cancel_pipe[0] = fds[0];
    context->cancel_pipe[1] = fds[1];
    context->cancel_pipe_active = 1;

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void prompt_platform_dispose(prompt_platform_context *context)
{
    if (context == NULL || !context->cancel_pipe_active)
    {
        return;
    }

    /*
     * 解放対象は入力中止通知専用の自己パイプだけに限定し、標準入力 FD は閉じない。
     * 解放の関数であり、閉じる処理の失敗を受けても取れる行動がないため、この 2 文の戻り値は明示的に破棄する。
     */
    (void)close(context->cancel_pipe[0]);
    (void)close(context->cancel_pipe[1]);
    context->cancel_pipe[0] = -1;
    context->cancel_pipe[1] = -1;
    context->cancel_pipe_active = 0;
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

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_stop_input(prompt_platform_context *context)
{
    if (context == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /*
     * 待機スレッドが入出力境界で観測できるよう、パイプ書き込みより先に中止状態を原子的に保持する。
     * see: https://man7.org/linux/man-pages/man2/poll.2.html
     */
    cplat_atomic_store_i32(&context->cancel_requested, 1, CPLAT_MEMORY_ORDER_RELEASE);

    if (!context->cancel_pipe_active)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    for (;;)
    {
        unsigned char wake = 1U;
        ssize_t written = write(context->cancel_pipe[1], &wake, 1U);

        if (written == 1)
        {
            return CPLAT_OK;
        }

        if (written < 0 && errno == EINTR)
        {
            continue;
        }

        /* パイプが既に満たされている場合は、読み取り側を起こす通知が存在するため受付済みとする。 */
        if (written < 0 && (errno == EAGAIN
    #if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
                            || errno == EWOULDBLOCK
    #endif
                            ))
        {
            return CPLAT_OK;
        }

        return CPLAT_ERR_UNKNOWN;
    }
}

/* 標準入力と中止通知パイプを poll で同時に待機し、1 バイトまたは制御通知を返す。 */
static int prompt_linux_read_byte(prompt_platform_context *context, const int timeout_ms, const int report_resize)
{
    for (;;)
    {
        struct pollfd fds[PROMPT_LINUX_POLL_FD_COUNT];
        nfds_t nfds = 1U;
        int ready;
        unsigned char c;
        ssize_t n;

        if (prompt_platform_is_input_canceled(context))
        {
            return PROMPT_PLATFORM_CANCELED;
        }

        if (report_resize && s_resize_pending)
        {
            s_resize_pending = 0;
            return PROMPT_PLATFORM_RESIZE;
        }

        /* fd_set の FD 上限へ依存しないよう、追加の中止通知パイプを含めて poll() で待機する。 */
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        if (context->cancel_pipe_active)
        {
            fds[1].fd = context->cancel_pipe[0];
            fds[1].events = POLLIN;
            fds[1].revents = 0;
            nfds = PROMPT_LINUX_POLL_FD_COUNT;
        }

        ready = poll(fds, nfds, timeout_ms);
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                if (prompt_platform_is_input_canceled(context))
                {
                    return PROMPT_PLATFORM_CANCELED;
                }

                if (report_resize && s_resize_pending)
                {
                    s_resize_pending = 0;
                    return PROMPT_PLATFORM_RESIZE;
                }

                continue;
            }

            return PROMPT_PLATFORM_ERROR;
        }

        if (prompt_platform_is_input_canceled(context))
        {
            return PROMPT_PLATFORM_CANCELED;
        }

        if (ready == 0)
        {
            if (report_resize && s_resize_pending)
            {
                s_resize_pending = 0;
                return PROMPT_PLATFORM_RESIZE;
            }

            return PROMPT_PLATFORM_TIMEOUT;
        }

        if (nfds == PROMPT_LINUX_POLL_FD_COUNT && (fds[1].revents & (POLLERR | POLLNVAL)) != 0)
        {
            return PROMPT_PLATFORM_ERROR;
        }

        if (nfds == PROMPT_LINUX_POLL_FD_COUNT && (fds[1].revents & POLLIN) != 0)
        {
            return PROMPT_PLATFORM_CANCELED;
        }

        if ((fds[0].revents & POLLNVAL) != 0)
        {
            return PROMPT_PLATFORM_ERROR;
        }

        /*
         * パイプの送信側が閉じても未読データが残る間は POLLIN と POLLHUP が同時に立つため、
         * POLLIN を先に確認して残存データを読み切ってから EOF とする。
         */
        if ((fds[0].revents & POLLIN) != 0)
        {
            n = read(STDIN_FILENO, &c, 1);

            if (prompt_platform_is_input_canceled(context))
            {
                return PROMPT_PLATFORM_CANCELED;
            }

            if (n == 1)
            {
                return (int)c;
            }

            if (n < 0 && errno == EINTR)
            {
                if (report_resize && s_resize_pending)
                {
                    s_resize_pending = 0;
                    return PROMPT_PLATFORM_RESIZE;
                }

                continue;
            }

            if (n == 0)
            {
                return PROMPT_PLATFORM_EOF;
            }

            return PROMPT_PLATFORM_ERROR;
        }

        if ((fds[0].revents & POLLERR) != 0)
        {
            return PROMPT_PLATFORM_ERROR;
        }

        if ((fds[0].revents & POLLHUP) != 0)
        {
            return PROMPT_PLATFORM_EOF;
        }

        return PROMPT_PLATFORM_ERROR;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_char(prompt_platform_context *context)
{
    return prompt_linux_read_byte(context, -1, 1);
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_char_nb(prompt_platform_context *context)
{
    return prompt_linux_read_byte(context, PROMPT_PLATFORM_SHORT_WAIT_MS, 1);
}

/* Doxygen コメントは、ヘッダーに記載 */

int prompt_platform_read_line(prompt_platform_context *context, char *buf, const size_t buf_size)
{
    size_t len = 0U;
    int saw_newline = 0;
    int saw_eof = 0;

    if (context == NULL || buf == NULL || buf_size == 0U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    buf[0] = '\0';

    if (prompt_platform_is_input_canceled(context))
    {
        return CPLAT_ERR_CANCELED;
    }

    if (buf_size == 1U)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* 部分行や残余データも待機可能な共通の 1 バイト経路で読み取る。 */
    while (len + 1U < buf_size)
    {
        int ch = prompt_linux_read_byte(context, -1, 0);

        if (ch == PROMPT_PLATFORM_CANCELED)
        {
            buf[0] = '\0';
            return CPLAT_ERR_CANCELED;
        }

        if (ch == PROMPT_PLATFORM_ERROR || ch == PROMPT_PLATFORM_TIMEOUT || ch == PROMPT_PLATFORM_RESIZE)
        {
            buf[0] = '\0';
            return CPLAT_ERR_UNKNOWN;
        }

        if (ch == PROMPT_PLATFORM_EOF)
        {
            if (len == 0U)
            {
                buf[0] = '\0';
                return CPLAT_ERR_EOF;
            }

            saw_eof = 1;
            break;
        }

        buf[len++] = (char)(unsigned char)ch;

        if (ch == '\n')
        {
            saw_newline = 1;
            break;
        }
    }

    if (!saw_newline && !saw_eof)
    {
        buf[0] = '\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    if (len > 0U && buf[len - 1U] == '\n')
    {
        len--;
        buf[len] = '\0';
    }

    if (len > 0U && buf[len - 1U] == '\r')
    {
        len--;
        buf[len] = '\0';
    }

    buf[len] = '\0';

    return CPLAT_OK;
}

#endif /* PLATFORM_LINUX */
