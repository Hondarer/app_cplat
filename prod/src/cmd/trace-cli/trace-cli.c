/**
 *******************************************************************************
 *  @file           trace-cli.c
 *  @brief          tracer API の動作を対話的に確認するコマンドを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/28
 *  @version        1.0.0
 *
 *  `cplat/trace/tracer.h` の公開 API を対話的に呼び出すための確認用 CLI です。\n
 *  起動後に interactive CLI として動作し、1 セッションにつき 1 個の tracer handle を保持します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "trace-cli.h"

#include <cplat/argparser/argparser.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/crt/unistd.h>
#include <cplat/prompt/prompt.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACE_CLI_LINE_MAX 4096

static const char *const g_trace_level_names[] = {
    "CRITICAL", "ERROR", "WARNING", "INFO", "VERBOSE", "DEBUG", "NONE",
};

enum trace_cli_prompt_state
{
    TRACE_CLI_PROMPT_STATE_UNCREATED = 0,
    TRACE_CLI_PROMPT_STATE_DISPOSED = 1
};

static char *skip_spaces(char *p)
{
    while (p != NULL && *p != '\0' && isspace((unsigned char)*p))
    {
        p++;
    }
    return p;
}

static void trim_right(char *s)
{
    size_t len;

    if (s == NULL)
    {
        return;
    }

    len = strlen(s);
    while (len > 0U && isspace((unsigned char)s[len - 1U]))
    {
        len--;
    }
    s[len] = '\0';
}

static int str_case_equal(const char *lhs, const char *rhs)
{
    size_t i;

    if (lhs == NULL || rhs == NULL)
    {
        return 0;
    }

    for (i = 0U; lhs[i] != '\0' && rhs[i] != '\0'; i++)
    {
        if (toupper((unsigned char)lhs[i]) != toupper((unsigned char)rhs[i]))
        {
            return 0;
        }
    }

    return lhs[i] == '\0' && rhs[i] == '\0';
}

static int is_null_keyword(const char *token)
{
    return str_case_equal(token, "null");
}

static char *next_token(char **cursor)
{
    char *p;
    char quote;
    char *start;

    if (cursor == NULL || *cursor == NULL)
    {
        return NULL;
    }

    p = skip_spaces(*cursor);
    if (*p == '\0')
    {
        *cursor = p;
        return NULL;
    }

    if (*p == '"' || *p == '\'')
    {
        quote = *p;
        start = ++p;
        while (*p != '\0' && *p != quote)
        {
            p++;
        }
        if (*p == '\0')
        {
            *cursor = p;
            return NULL;
        }
        *p = '\0';
        *cursor = p + 1;
        return start;
    }

    start = p;
    while (*p != '\0' && !isspace((unsigned char)*p))
    {
        p++;
    }
    if (*p != '\0')
    {
        *p = '\0';
        p++;
    }
    *cursor = p;
    return start;
}

static char *rest_argument(char **cursor)
{
    char *p;
    char *quoted;

    if (cursor == NULL || *cursor == NULL)
    {
        return NULL;
    }

    p = skip_spaces(*cursor);
    if (*p == '\0')
    {
        *cursor = p;
        return NULL;
    }

    if (*p == '"' || *p == '\'')
    {
        quoted = next_token(&p);
        if (quoted == NULL)
        {
            *cursor = p;
            return NULL;
        }
        p = skip_spaces(p);
        if (*p != '\0')
        {
            *cursor = p;
            return NULL;
        }
        *cursor = p;
        return quoted;
    }

    *cursor = p + strlen(p);
    return p;
}

static const char *level_to_name(cplat_trace_level level)
{
    if ((int)level >= 0 && (size_t)level < sizeof(g_trace_level_names) / sizeof(g_trace_level_names[0]))
    {
        return g_trace_level_names[(size_t)level];
    }

    return "UNKNOWN";
}

static int parse_trace_level(const char *token, cplat_trace_level *level)
{
    size_t i;

    if (token == NULL || level == NULL)
    {
        return 0;
    }

    for (i = 0U; i < sizeof(g_trace_level_names) / sizeof(g_trace_level_names[0]); i++)
    {
        if (str_case_equal(token, g_trace_level_names[i]))
        {
            *level = (cplat_trace_level)i;
            return 1;
        }
    }

    return 0;
}

static int parse_int64_value(const char *token, int64_t *value)
{
    if (token == NULL || value == NULL)
    {
        return 0;
    }

    if (cplat_parse_int64(value, token, 10) != CPLAT_OK)
    {
        return 0;
    }
    return 1;
}

static int parse_int_value(const char *token, int *value)
{
    if (token == NULL || value == NULL)
    {
        return 0;
    }

    if (cplat_parse_int(value, token, 10) != CPLAT_OK)
    {
        return 0;
    }
    return 1;
}

static const char *tracer_state_to_name(cplat_tracer_state state)
{
    switch (state)
    {
    case CPLAT_TRACER_STATE_STARTED:
        return "started";
    case CPLAT_TRACER_STATE_STOPPED:
        return "stopped";
    case CPLAT_TRACER_STATE_DISPOSED:
    default:
        return "disposed";
    }
}

static const char *session_prompt_state_to_name(const trace_cli_session *session)
{
    if (session == NULL)
    {
        return "disposed";
    }
    if (session->handle != NULL)
    {
        return tracer_state_to_name(cplat_tracer_get_state(session->handle));
    }

    if (session->prompt_state == TRACE_CLI_PROMPT_STATE_DISPOSED)
    {
        return "disposed";
    }

    return "uncreated";
}

static int parse_size_value(const char *token, size_t *value)
{
    uint64_t parsed;

    if (token == NULL || value == NULL)
    {
        return 0;
    }

    if (cplat_parse_uint64(&parsed, token, 10) != CPLAT_OK)
    {
        return 0;
    }
    *value = (size_t)parsed;
    return 1;
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

static int parse_hex_bytes(const char *text, unsigned char **data, size_t *size)
{
    size_t digits = 0U;
    size_t index = 0U;
    const char *p;
    unsigned char *buf = NULL;
    int high = -1;

    if (text == NULL || data == NULL || size == NULL)
    {
        return 0;
    }

    for (p = text; *p != '\0'; p++)
    {
        if (isspace((unsigned char)*p))
        {
            continue;
        }
        if (hex_nibble(*p) < 0)
        {
            return 0;
        }
        digits++;
    }

    if ((digits % 2U) != 0U)
    {
        return 0;
    }

    if (digits == 0U)
    {
        *data = NULL;
        *size = 0U;
        return 1;
    }

    buf = (unsigned char *)cplat_malloc(digits / 2U);
    if (buf == NULL)
    {
        return 0;
    }

    for (p = text; *p != '\0'; p++)
    {
        int nibble;

        if (isspace((unsigned char)*p))
        {
            continue;
        }

        nibble = hex_nibble(*p);
        if (nibble < 0)
        {
            cplat_free(buf);
            return 0;
        }

        if (high < 0)
        {
            high = nibble;
        }
        else
        {
            buf[index++] = (unsigned char)((high << 4) | nibble);
            high = -1;
        }
    }

    *data = buf;
    *size = digits / 2U;
    return 1;
}

static void print_level_result(cplat_trace_level level)
{
    printf("level=%s(%d)\n", level_to_name(level), (int)level);
}

static void print_rc_result(int ret)
{
    if (!cplat_isatty(CPLAT_STREAM_STDOUT))
    {
        printf("rc=%d\n", ret);
        return;
    }

    if (ret == 0)
    {
        printf("\x1b[32mrc=%d\x1b[0m\n", ret);
        return;
    }

    printf("\x1b[31mrc=%d\x1b[0m\n", ret);
}

/**
 *  @brief  CLI コマンド 1 個の定義 (名前・usage・ハンドラー) です。
 *
 *  コマンドの追加は g_commands へのエントリ追加で完結します
 *  (help のコマンド一覧と usage 表示はテーブルから生成されます)。
 */
struct trace_cli_command
{
    /** コマンド名。 */
    const char *name;

    /** usage 表示用の引数説明 (先頭に空白を含む)。引数なしは ""。 */
    const char *usage_args;

    /** 1 の場合、ディスパッチ側で余剰トークンを拒否する (引数なしコマンド)。 */
    int no_args;

    /** パディング (handler をポインター境界に揃える)。 */
    int pad;

    /** コマンド本体。成功時 0、終了要求時 1、失敗時 -1 を返す。 */
    int (*handler)(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor);
};

static void print_command_usage(const struct trace_cli_command *cmd)
{
    fprintf(stderr, "使用方法: %s%s\n", cmd->name, cmd->usage_args);
}

void trace_cli_session_init(trace_cli_session *session)
{
    if (session == NULL)
    {
        return;
    }

    session->handle = NULL;
    session->prompt_state = TRACE_CLI_PROMPT_STATE_UNCREATED;
    session->exit_requested = 0;
}

void trace_cli_session_dispose(trace_cli_session *session)
{
    if (session == NULL)
    {
        return;
    }

    cplat_tracer_dispose(&session->handle);
    session->handle = NULL;
    session->prompt_state = TRACE_CLI_PROMPT_STATE_DISPOSED;
}

static void print_interactive_hint(void)
{
    printf("help でコマンド一覧を表示します。exit で終了します。\n");
}

static int cmd_help(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)session;
    (void)cmd;
    (void)cursor;
    trace_cli_print_help();
    return 0;
}

static int cmd_exit(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    session->exit_requested = 1;
    return 1;
}

static int cmd_create(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    if (session->handle != NULL)
    {
        fprintf(stderr, "エラー: 既存の handle を dispose してから create を実行してください。\n");
        return -1;
    }
    session->handle = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED);
    if (session->handle == NULL)
    {
        session->prompt_state = TRACE_CLI_PROMPT_STATE_UNCREATED;
        printf("handle=NULL\n");
    }
    else
    {
        printf("handle=created\n");
    }
    return 0;
}

static int cmd_dispose(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    cplat_tracer_dispose(&session->handle);
    session->handle = NULL;
    session->prompt_state = TRACE_CLI_PROMPT_STATE_DISPOSED;
    printf("handle=disposed\n");
    return 0;
}

static int cmd_start(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    print_rc_result(cplat_tracer_start(session->handle));
    return 0;
}

static int cmd_stop(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    print_rc_result(cplat_tracer_stop(session->handle));
    return 0;
}

static int cmd_set_name(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    char *name_token;
    char *identifier_token;
    const char *name = NULL;
    int64_t identifier = 0;
    int ret;

    name_token = next_token(cursor);
    if (name_token == NULL)
    {
        print_command_usage(cmd);
        return -1;
    }
    identifier_token = next_token(cursor);
    if (identifier_token != NULL && !parse_int64_value(identifier_token, &identifier))
    {
        fprintf(stderr, "エラー: identifier は整数で指定してください。\n");
        return -1;
    }
    if (next_token(cursor) != NULL)
    {
        print_command_usage(cmd);
        return -1;
    }

    if (!is_null_keyword(name_token))
    {
        name = name_token;
    }
    ret = cplat_tracer_set_name(session->handle, name, identifier);
    print_rc_result(ret);
    return 0;
}

static int cmd_get_os_level(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    print_level_result(cplat_tracer_get_os_level(session->handle));
    return 0;
}

static int cmd_set_os_level(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    char *level_token;
    cplat_trace_level level;

    level_token = next_token(cursor);
    if (level_token == NULL || next_token(cursor) != NULL)
    {
        print_command_usage(cmd);
        return -1;
    }
    if (!parse_trace_level(level_token, &level))
    {
        fprintf(stderr, "エラー: level が不正です。\n");
        return -1;
    }
    print_rc_result(cplat_tracer_set_os_level(session->handle, level));
    return 0;
}

static int cmd_get_file_level(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    print_level_result(cplat_tracer_get_file_level(session->handle));
    return 0;
}

static int cmd_set_file_level(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    char *path_token;
    char *level_token;
    char *max_bytes_token;
    char *generations_token;
    const char *path = NULL;
    cplat_trace_level level;
    size_t max_bytes = 0U;
    int generations = 0;
    int ret;

    path_token = next_token(cursor);
    level_token = next_token(cursor);
    if (path_token == NULL || level_token == NULL)
    {
        print_command_usage(cmd);
        return -1;
    }
    if (!parse_trace_level(level_token, &level))
    {
        fprintf(stderr, "エラー: level が不正です。\n");
        return -1;
    }

    max_bytes_token = next_token(cursor);
    if (max_bytes_token != NULL && !parse_size_value(max_bytes_token, &max_bytes))
    {
        fprintf(stderr, "エラー: max-bytes は 0 以上の整数で指定してください。\n");
        return -1;
    }

    generations_token = next_token(cursor);
    if (generations_token != NULL && !parse_int_value(generations_token, &generations))
    {
        fprintf(stderr, "エラー: generations は整数で指定してください。\n");
        return -1;
    }

    if (next_token(cursor) != NULL)
    {
        print_command_usage(cmd);
        return -1;
    }

    if (!is_null_keyword(path_token))
    {
        path = path_token;
    }
    ret = cplat_tracer_set_file_level(session->handle, path, level, max_bytes, generations, 0);
    print_rc_result(ret);
    return 0;
}

static int cmd_get_stderr_level(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    (void)cmd;
    (void)cursor;
    print_level_result(cplat_tracer_get_stderr_level(session->handle));
    return 0;
}

static int cmd_set_stderr_level(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    char *level_token;
    cplat_trace_level level;

    level_token = next_token(cursor);
    if (level_token == NULL || next_token(cursor) != NULL)
    {
        print_command_usage(cmd);
        return -1;
    }
    if (!parse_trace_level(level_token, &level))
    {
        fprintf(stderr, "エラー: level が不正です。\n");
        return -1;
    }
    print_rc_result(cplat_tracer_set_stderr_level(session->handle, level));
    return 0;
}

static int cmd_write(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    char *level_token;
    char *message;
    cplat_trace_level level;
    int ret;

    level_token = next_token(cursor);
    if (level_token == NULL)
    {
        print_command_usage(cmd);
        return -1;
    }
    if (!parse_trace_level(level_token, &level))
    {
        fprintf(stderr, "エラー: level が不正です。\n");
        return -1;
    }
    message = rest_argument(cursor);
    if (message == NULL)
    {
        print_command_usage(cmd);
        return -1;
    }

    if (strcmp(cmd->name, "write") == 0)
    {
        ret = cplat_tracer_write_at(session->handle, level, NULL, message);
    }
    else
    {
        ret = cplat_tracer_writef_at(session->handle, level, NULL, "%s", message);
    }
    print_rc_result(ret);
    return 0;
}

static int cmd_write_hex(trace_cli_session *session, const struct trace_cli_command *cmd, char **cursor)
{
    char *level_token;
    char *hex_token;
    char *label_cursor;
    char *label;
    cplat_trace_level level;
    unsigned char *data = NULL;
    size_t size = 0U;
    int ret;

    level_token = next_token(cursor);
    hex_token = next_token(cursor);
    if (level_token == NULL || hex_token == NULL)
    {
        print_command_usage(cmd);
        return -1;
    }
    if (!parse_trace_level(level_token, &level))
    {
        fprintf(stderr, "エラー: level が不正です。\n");
        return -1;
    }
    if (!parse_hex_bytes(hex_token, &data, &size))
    {
        fprintf(stderr, "エラー: hex は 16 進文字列で指定してください。\n");
        return -1;
    }

    label_cursor = *cursor;
    label = rest_argument(cursor);
    if (label == NULL && *skip_spaces(label_cursor) != '\0')
    {
        cplat_free(data);
        print_command_usage(cmd);
        return -1;
    }

    if (strcmp(cmd->name, "write-hex") == 0)
    {
        ret = cplat_tracer_write_hex_at(session->handle, level, NULL, data, size, label);
    }
    else
    {
        const char *label_str;
        if (label != NULL)
        {
            label_str = label;
        }
        else
        {
            label_str = "";
        }
        ret = cplat_tracer_write_hexf_at(session->handle, level, NULL, data, size, "%s", label_str);
    }
    print_rc_result(ret);
    cplat_free(data);
    return 0;
}

/** コマンド テーブル (help のコマンド一覧はこの順で表示される)。 */
static const struct trace_cli_command g_commands[] = {
    {"help", "", 1, 0, cmd_help},
    {"exit", "", 1, 0, cmd_exit},
    {"quit", "", 1, 0, cmd_exit},
    {"create", "", 1, 0, cmd_create},
    {"dispose", "", 1, 0, cmd_dispose},
    {"start", "", 1, 0, cmd_start},
    {"stop", "", 1, 0, cmd_stop},
    {"set-name", " <name|null> [identifier]", 0, 0, cmd_set_name},
    {"get-os-level", "", 1, 0, cmd_get_os_level},
    {"set-os-level", " <level>", 0, 0, cmd_set_os_level},
    {"get-file-level", "", 1, 0, cmd_get_file_level},
    {"set-file-level", " <path|null> <level> [max-bytes] [generations]", 0, 0, cmd_set_file_level},
    {"get-stderr-level", "", 1, 0, cmd_get_stderr_level},
    {"set-stderr-level", " <level>", 0, 0, cmd_set_stderr_level},
    {"write", " <level> <message...>", 0, 0, cmd_write},
    {"writef", " <level> <message...>", 0, 0, cmd_write},
    {"write-hex", " <level> <hex> [label...]", 0, 0, cmd_write_hex},
    {"write-hexf", " <level> <hex> [label...]", 0, 0, cmd_write_hex},
};

static const struct trace_cli_command *find_command(const char *name)
{
    size_t i;

    for (i = 0U; i < sizeof(g_commands) / sizeof(g_commands[0]); i++)
    {
        if (strcmp(name, g_commands[i].name) == 0)
        {
            return &g_commands[i];
        }
    }
    return NULL;
}

void trace_cli_print_help(void)
{
    size_t i;

    printf("trace-cli: cplat.tracer interactive CLI\n");
    printf("使用可能な level: CRITICAL ERROR WARNING INFO VERBOSE DEBUG NONE\n");
    printf("コマンド:\n");
    for (i = 0U; i < sizeof(g_commands) / sizeof(g_commands[0]); i++)
    {
        printf("  %s%s\n", g_commands[i].name, g_commands[i].usage_args);
    }
    printf("hex は 01ABFF または \"01 AB FF\" の形式で指定できます。\n");
}

int trace_cli_process_line(trace_cli_session *session, const char *line)
{
    char buffer[TRACE_CLI_LINE_MAX];
    char *cursor;
    char *command;
    const struct trace_cli_command *cmd;

    if (session == NULL || line == NULL)
    {
        return -1;
    }

    (void)cplat_strncpy(buffer, sizeof(buffer), line, sizeof(buffer) - 1U);
    trim_right(buffer);

    cursor = buffer;
    command = next_token(&cursor);
    if (command == NULL)
    {
        return 0;
    }

    cmd = find_command(command);
    if (cmd == NULL)
    {
        fprintf(stderr, "エラー: 不明なコマンドです: %s\n", command);
        return -1;
    }

    if (cmd->no_args && next_token(&cursor) != NULL)
    {
        print_command_usage(cmd);
        return -1;
    }

    return cmd->handler(session, cmd, &cursor);
}

int main(int argc, char *argv[])
{
    trace_cli_session session;
    char line[TRACE_CLI_LINE_MAX];
    cplat_prompt *prompt;

    cplat_console_init();

    int need_help = 0;

    cplat_argparser_init(argc, argv, "tracer API を対話的に確認します。");
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

    trace_cli_session_init(&session);
    prompt = cplat_prompt_create(NULL);
    print_interactive_hint();

    while (!session.exit_requested)
    {
        if (cplat_prompt_readline_fmt(prompt, line, sizeof(line), "trace-cli[%s]> ",
                                      session_prompt_state_to_name(&session)) != CPLAT_OK)
        {
            break;
        }
        if (line[0] == '\0')
        {
            continue;
        }
        trace_cli_process_line(&session, line);
    }

    trace_cli_session_dispose(&session);
    cplat_prompt_dispose(prompt);
    return EXIT_SUCCESS;
}
