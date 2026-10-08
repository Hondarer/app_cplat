/**
 *******************************************************************************
 *  @file           pinned-prompt.c
 *  @brief          固定プロンプト API の動作を確認するコマンドを実装します。
 *
 *  入力、並行出力、ステータス領域を組み合わせた対話操作を提供します。
 *
 *******************************************************************************
 */

#include <cplat/argparser/argparser.h>
#include <cplat/console/console.h>
#include <cplat/crt/string.h>
#include <cplat/prompt/pinned_prompt.h>
#include <cplat/sync/sync.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PINNED_PROMPT_CLI_LINE_MAX 1024U
#define PINNED_PROMPT_CLI_TICK_MS  1000U

typedef struct pinned_prompt_cli_worker
{
    cplat_local_lock *mutex;
    cplat_condvar *condvar;
    cplat_thread *thread;

    cplat_pinned_prompt *screen;
    const char *name;

    uint64_t tick_count;

    cplat_pinned_prompt_channel channel;
    int sync_initialized;
    int thread_running;
    int stop_requested;
} pinned_prompt_cli_worker;

typedef struct pinned_prompt_cli_session
{
    cplat_pinned_prompt *screen;
    pinned_prompt_cli_worker stdout_worker;
    pinned_prompt_cli_worker stderr_worker;
    int exit_requested;
    int reserved;
} pinned_prompt_cli_session;

static char *skip_spaces(char *s)
{
    while (*s == ' ' || *s == '\t')
    {
        s++;
    }
    return s;
}

static int split_command(char *line, char **command, char **arg)
{
    char *cursor;

    cursor = line;
    cursor = skip_spaces(cursor);
    if (*cursor == '\0')
    {
        *command = NULL;
        *arg = NULL;
        return 0;
    }

    *command = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
    {
        cursor++;
    }
    if (*cursor != '\0')
    {
        *cursor = '\0';
        cursor++;
    }
    cursor = skip_spaces(cursor);
    if (*cursor != '\0')
    {
        *arg = cursor;
    }
    else
    {
        *arg = NULL;
    }
    return 0;
}

static void decode_cli_escapes(char *s)
{
    char *read_pos;
    char *write_pos;

    if (s == NULL)
    {
        return;
    }

    read_pos = s;
    write_pos = s;
    while (*read_pos != '\0')
    {
        if (*read_pos == '\\')
        {
            read_pos++;
            if (*read_pos == 'e' || *read_pos == 'E')
            {
                *write_pos = '\033';
                write_pos++;
                read_pos++;
                continue;
            }
            if (*read_pos == '\\')
            {
                *write_pos = '\\';
                write_pos++;
                read_pos++;
                continue;
            }
            *write_pos = '\\';
            write_pos++;
            if (*read_pos == '\0')
            {
                break;
            }
        }
        *write_pos = *read_pos;
        write_pos++;
        read_pos++;
    }
    *write_pos = '\0';
}

static void print_help(cplat_pinned_prompt *screen)
{
    cplat_pinned_prompt_printf(screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
                               "commands:\n"
                               "  help          show this help\n"
                               "  echo TEXT     write TEXT to stdout (\\e emits ESC)\n"
                               "  read primary  read one line with primary history\n"
                               "  read secondary read one line with secondary history\n"
                               "  read formatted read one line with formatted prompt\n"
                               "  start stdout  start stdout tick output\n"
                               "  start stderr  start stderr tick output\n"
                               "  stop stdout   stop stdout tick output\n"
                               "  stop stderr   stop stderr tick output\n"
                               "  stop all      stop all tick output\n"
                               "  status show [top|bottom|all]    show status area(s)\n"
                               "  status hide [top|bottom|all]    hide status area(s)\n"
                               "  status set-top-left TEXT    set top-left status (\\e emits ESC)\n"
                               "  status set-top-right TEXT   set top-right status (\\e emits ESC)\n"
                               "  status set-bottom-left TEXT    set bottom-left status (\\e emits ESC)\n"
                               "  status set-bottom-right TEXT   set bottom-right status (\\e emits ESC)\n"
                               "  exit, quit    exit pinned-prompt\n");
}

static void worker_init(pinned_prompt_cli_worker *worker, cplat_pinned_prompt *screen,
                        cplat_pinned_prompt_channel channel, const char *name)
{
    memset(worker, 0, sizeof(*worker));
    worker->screen = screen;
    worker->channel = channel;
    worker->name = name;
}

static int worker_ensure_sync(pinned_prompt_cli_worker *worker)
{
    if (worker->sync_initialized)
    {
        return 0;
    }
    if (cplat_local_lock_create(&worker->mutex) != CPLAT_OK)
    {
        return -1;
    }
    if (cplat_condvar_create(&worker->condvar) != CPLAT_OK)
    {
        (void)cplat_local_lock_dispose(worker->mutex);
        return -1;
    }
    worker->sync_initialized = 1;
    return 0;
}

static void worker_thread_proc(void *arg)
{
    pinned_prompt_cli_worker *worker;
    int stop;

    worker = (pinned_prompt_cli_worker *)arg;
    (void)cplat_local_lock_lock(worker->mutex, CPLAT_SYNC_WAIT_FOREVER);
    for (;;)
    {
        if (worker->stop_requested)
        {
            break;
        }

        (void)cplat_condvar_wait(worker->condvar, worker->mutex, PINNED_PROMPT_CLI_TICK_MS);
        if (worker->stop_requested)
        {
            break;
        }

        worker->tick_count++;
        stop = worker->stop_requested;
        (void)cplat_local_lock_unlock(worker->mutex);

        if (!stop)
        {
            cplat_pinned_prompt_printf(worker->screen, worker->channel, "[%s tick %llu]\n", worker->name,
                                       (unsigned long long)worker->tick_count);
        }

        (void)cplat_local_lock_lock(worker->mutex, CPLAT_SYNC_WAIT_FOREVER);
    }
    worker->thread_running = 0;
    (void)cplat_local_lock_unlock(worker->mutex);
}

static void worker_start(pinned_prompt_cli_worker *worker)
{
    int running;
    int started;
    int failed;

    if (worker_ensure_sync(worker) != 0)
    {
        cplat_pinned_prompt_printf(worker->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                   "failed to initialize %s worker\n", worker->name);
        return;
    }

    started = 0;
    failed = 0;
    (void)cplat_local_lock_lock(worker->mutex, CPLAT_SYNC_WAIT_FOREVER);
    running = worker->thread_running;
    if (!running)
    {
        worker->stop_requested = 0;
        worker->tick_count = 0U;
        if (cplat_thread_create(&worker->thread, worker_thread_proc, worker) == CPLAT_OK)
        {
            worker->thread_running = 1;
            started = 1;
        }
        else
        {
            failed = 1;
        }
    }
    (void)cplat_local_lock_unlock(worker->mutex);

    if (running)
    {
        cplat_pinned_prompt_printf(worker->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s worker already running\n",
                                   worker->name);
    }
    else if (started)
    {
        cplat_pinned_prompt_printf(worker->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s worker started\n",
                                   worker->name);
    }
    else if (failed)
    {
        cplat_pinned_prompt_printf(worker->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR, "failed to start %s worker\n",
                                   worker->name);
    }
}

static void worker_stop(pinned_prompt_cli_worker *worker, int announce)
{
    int running;

    if (!worker->sync_initialized)
    {
        if (announce)
        {
            cplat_pinned_prompt_printf(worker->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s worker not running\n",
                                       worker->name);
        }
        return;
    }

    (void)cplat_local_lock_lock(worker->mutex, CPLAT_SYNC_WAIT_FOREVER);
    running = worker->thread_running;
    if (running)
    {
        worker->stop_requested = 1;
        (void)cplat_condvar_signal(worker->condvar);
    }
    (void)cplat_local_lock_unlock(worker->mutex);

    if (running)
    {
        cplat_thread_join(worker->thread, CPLAT_SYNC_WAIT_FOREVER);
        if (announce)
        {
            cplat_pinned_prompt_printf(worker->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s worker stopped\n",
                                       worker->name);
        }
    }
    else if (announce)
    {
        cplat_pinned_prompt_printf(worker->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s worker not running\n",
                                   worker->name);
    }
}

static void worker_dispose(pinned_prompt_cli_worker *worker)
{
    if (worker->sync_initialized)
    {
        worker_stop(worker, 0);
        (void)cplat_condvar_dispose(worker->condvar);
        (void)cplat_local_lock_dispose(worker->mutex);
        worker->sync_initialized = 0;
    }
}

static pinned_prompt_cli_worker *find_worker(pinned_prompt_cli_session *session, const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }
    if (strcmp(name, "stdout") == 0)
    {
        return &session->stdout_worker;
    }
    if (strcmp(name, "stderr") == 0)
    {
        return &session->stderr_worker;
    }
    return NULL;
}

static void process_start(pinned_prompt_cli_session *session, const char *arg)
{
    pinned_prompt_cli_worker *worker;

    worker = find_worker(session, arg);
    if (worker == NULL)
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR, "usage: start stdout|stderr\n");
        return;
    }
    worker_start(worker);
}

static void process_stop(pinned_prompt_cli_session *session, const char *arg)
{
    pinned_prompt_cli_worker *worker;

    if (arg != NULL && strcmp(arg, "all") == 0)
    {
        worker_stop(&session->stdout_worker, 1);
        worker_stop(&session->stderr_worker, 1);
        return;
    }

    worker = find_worker(session, arg);
    if (worker == NULL)
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                   "usage: stop stdout|stderr|all\n");
        return;
    }
    worker_stop(worker, 1);
}

static void process_status(pinned_prompt_cli_session *session, const char *arg)
{
    char *subcmd;
    char *subarg;
    char arg_copy[PINNED_PROMPT_CLI_LINE_MAX];

    if (arg == NULL)
    {
        cplat_pinned_prompt_printf(
            session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
            "usage: status show|hide|set-top-left|set-top-right|set-bottom-left|set-bottom-right\n");
        return;
    }

    (void)cplat_strcpy(arg_copy, sizeof(arg_copy), arg);
    (void)split_command(arg_copy, &subcmd, &subarg);

    if (strcmp(subcmd, "show") == 0)
    {
        if (subarg == NULL || strcmp(subarg, "all") == 0)
        {
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, 1);
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, 1);
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "status areas enabled\n");
        }
        else if (strcmp(subarg, "top") == 0)
        {
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, 1);
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
                                       "top status area enabled\n");
        }
        else if (strcmp(subarg, "bottom") == 0)
        {
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, 1);
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
                                       "bottom status area enabled\n");
        }
        else
        {
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                       "usage: status show [top|bottom|all]\n");
        }
    }
    else if (strcmp(subcmd, "hide") == 0)
    {
        if (subarg == NULL || strcmp(subarg, "all") == 0)
        {
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, 0);
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, 0);
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "status areas disabled\n");
        }
        else if (strcmp(subarg, "top") == 0)
        {
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP, 0);
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
                                       "top status area disabled\n");
        }
        else if (strcmp(subarg, "bottom") == 0)
        {
            (void)cplat_pinned_prompt_status_enable(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM, 0);
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT,
                                       "bottom status area disabled\n");
        }
        else
        {
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                       "usage: status hide [top|bottom|all]\n");
        }
    }
    else if (strcmp(subcmd, "set-top-left") == 0)
    {
        (void)cplat_pinned_prompt_status_set(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
                                             CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT, subarg);
    }
    else if (strcmp(subcmd, "set-top-right") == 0)
    {
        (void)cplat_pinned_prompt_status_set(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP,
                                             CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT, subarg);
    }
    else if (strcmp(subcmd, "set-bottom-left") == 0)
    {
        (void)cplat_pinned_prompt_status_set(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM,
                                             CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT, subarg);
    }
    else if (strcmp(subcmd, "set-bottom-right") == 0)
    {
        (void)cplat_pinned_prompt_status_set(session->screen, CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM,
                                             CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT, subarg);
    }
    else
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                   "unknown status subcommand: %s\n", subcmd);
    }
}

static int read_primary_history(cplat_pinned_prompt *screen, char *buf, size_t buf_size)
{
    return cplat_pinned_prompt_readline(screen, buf, buf_size, "primary> ");
}

static int read_secondary_history(cplat_pinned_prompt *screen, char *buf, size_t buf_size)
{
    return cplat_pinned_prompt_readline(screen, buf, buf_size, "secondary> ");
}

static int read_formatted_history(cplat_pinned_prompt *screen, char *buf, size_t buf_size)
{
    return cplat_pinned_prompt_readline_fmt(screen, buf, buf_size, "%s> ", "formatted");
}

static void process_read(pinned_prompt_cli_session *session, const char *arg)
{
    char buf[PINNED_PROMPT_CLI_LINE_MAX];
    int ret;

    if (arg == NULL)
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                   "usage: read primary|secondary|formatted\n");
        return;
    }

    if (strcmp(arg, "primary") == 0)
    {
        ret = read_primary_history(session->screen, buf, sizeof(buf));
    }
    else if (strcmp(arg, "secondary") == 0)
    {
        ret = read_secondary_history(session->screen, buf, sizeof(buf));
    }
    else if (strcmp(arg, "formatted") == 0)
    {
        ret = read_formatted_history(session->screen, buf, sizeof(buf));
    }
    else
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR,
                                   "usage: read primary|secondary|formatted\n");
        return;
    }

    if (ret == CPLAT_OK)
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "read %s: %s\n", arg, buf);
    }
    else
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "read %s cancelled\n", arg);
    }
}

static void process_line(pinned_prompt_cli_session *session, char *line)
{
    char original_line[PINNED_PROMPT_CLI_LINE_MAX];
    char *command;
    char *arg;

    (void)cplat_strcpy(original_line, sizeof(original_line), line);

    (void)split_command(line, &command, &arg);
    if (command == NULL)
    {
        return;
    }

    cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "echo: %s\n", original_line);

    if (strcmp(command, "help") == 0)
    {
        print_help(session->screen);
    }
    else if (strcmp(command, "echo") == 0)
    {
        decode_cli_escapes(arg);
        {
            const char *echo_str;
            if (arg != NULL)
            {
                echo_str = arg;
            }
            else
            {
                echo_str = "";
            }
            cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "%s\n", echo_str);
        }
    }
    else if (strcmp(command, "start") == 0)
    {
        process_start(session, arg);
    }
    else if (strcmp(command, "stop") == 0)
    {
        process_stop(session, arg);
    }
    else if (strcmp(command, "status") == 0)
    {
        decode_cli_escapes(arg);
        process_status(session, arg);
    }
    else if (strcmp(command, "read") == 0)
    {
        process_read(session, arg);
    }
    else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0)
    {
        session->exit_requested = 1;
    }
    else
    {
        cplat_pinned_prompt_printf(session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR, "unknown command: %s\n",
                                   command);
    }
}

static int session_init(pinned_prompt_cli_session *session)
{
    memset(session, 0, sizeof(*session));
    session->screen = cplat_pinned_prompt_create(NULL);
    if (session->screen == NULL)
    {
        fprintf(stderr, "pinned-prompt の作成に失敗しました。\n");
        return -1;
    }

    worker_init(&session->stdout_worker, session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDOUT, "stdout");
    worker_init(&session->stderr_worker, session->screen, CPLAT_PINNED_PROMPT_CHANNEL_STDERR, "stderr");
    return 0;
}

static void session_dispose(pinned_prompt_cli_session *session)
{
    worker_dispose(&session->stdout_worker);
    worker_dispose(&session->stderr_worker);
    cplat_pinned_prompt_dispose(session->screen);
    session->screen = NULL;
}

int main(int argc, char *argv[])
{
    pinned_prompt_cli_session session;
    char line[PINNED_PROMPT_CLI_LINE_MAX];
    int ret;

    cplat_console_init();

    int need_help = 0;

    cplat_argparser_init(argc, argv, "固定プロンプト API を対話的に確認します。");
    cplat_argparser_register_flag("-h", "--help", "ヘルプを表示します。", &need_help);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        cplat_argparser_print_register_error_messages(stderr);
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    if (need_help != 0)
    {
        cplat_argparser_print_usage(stdout);
        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        cplat_argparser_print_error_messages(stderr);
        cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (session_init(&session) != 0)
    {
        return EXIT_FAILURE;
    }

    print_help(session.screen);
    while (!session.exit_requested)
    {
        ret = cplat_pinned_prompt_readline(session.screen, line, sizeof(line), "pinned-prompt> ");
        if (ret != CPLAT_OK)
        {
            break;
        }
        process_line(&session, line);
    }

    session_dispose(&session);
    return EXIT_SUCCESS;
}
