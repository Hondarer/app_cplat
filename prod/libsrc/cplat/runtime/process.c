/**
 *******************************************************************************
 *  @file           process.c
 *  @brief          プロセス情報の取得と子プロセスの起動を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/07
 *  @version        1.2.0
 *
 *  現在のプロセスの実行ファイル本体の絶対パス取得と、Linux/Windows での子プロセス起動を
 *  提供します。管理者権限確認や昇格起動の責務は持たず、コンソール コンポーネントにも
 *  依存しません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/runtime/process.h>
#include <cplat/crt/stdlib.h>
#include <cplat/runtime/process_internal.h>
#include <cplat/base/result_internal.h>
#include <cplat/crt/path.h>
#include <cplat/crt/wchar_conv.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <fcntl.h>
    #include <signal.h>
    #include <sys/syscall.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <time.h>
    #include <unistd.h>

extern char **environ;
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/win32/win32.h>
    #include <wchar.h>
#endif /* PLATFORM_ */

/* Doxygen コメントは、ヘッダーに記載 */

struct cplat_process
{
#if defined(PLATFORM_LINUX)
    pid_t pid;
#elif defined(PLATFORM_WINDOWS)
    HANDLE process;
#endif /* PLATFORM_ */
    int exited;
    int exit_code;
};

static int has_valid_argv(char *const *argv)
{
    if (argv == NULL)
    {
        return 0;
    }
    if (argv[0] == NULL)
    {
        return 0;
    }
    if (argv[0][0] == '\0')
    {
        return 0;
    }
    return 1;
}

static void process_set_exit_code(cplat_process *process, const int exit_code)
{
    process->exited = 1;
    process->exit_code = exit_code;
}

#if defined(PLATFORM_LINUX)
static uint64_t monotonic_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static char *string_duplicate(const char *text)
{
    char *copy;
    size_t len;

    if (text == NULL)
    {
        return NULL;
    }

    len = strlen(text) + 1;
    copy = (char *)cplat_malloc(len);
    if (copy != NULL)
    {
        memcpy(copy, text, len);
    }
    return copy;
}

static size_t env_key_len(const char *entry)
{
    const char *sep;

    sep = strchr(entry, '=');
    if (sep == NULL)
    {
        return 0;
    }
    return (size_t)(sep - entry);
}

static int env_key_matches(const char *entry, const char *key, const size_t key_len)
{
    if (strncmp(entry, key, key_len) != 0)
    {
        return 0;
    }
    if (entry[key_len] != '=')
    {
        return 0;
    }
    return 1;
}

static void free_envp(char **envp)
{
    size_t i;

    if (envp == NULL)
    {
        return;
    }

    i = 0;
    while (envp[i] != NULL)
    {
        cplat_free(envp[i]);
        i++;
    }
    cplat_free(envp);
}

static int set_env_entry(char **envp, const size_t capacity, const char *entry)
{
    size_t key_len;
    size_t i;
    char *copy;

    key_len = env_key_len(entry);
    if (key_len == 0)
    {
        return -1;
    }

    copy = string_duplicate(entry);
    if (copy == NULL)
    {
        return -1;
    }

    i = 0;
    while (envp[i] != NULL)
    {
        if (env_key_matches(envp[i], entry, key_len) != 0)
        {
            cplat_free(envp[i]);
            envp[i] = copy;
            return 0;
        }
        i++;
    }

    if (i + 1 >= capacity)
    {
        cplat_free(copy);
        return -1;
    }

    envp[i] = copy;
    envp[i + 1] = NULL;
    return 0;
}

static char **build_environment(char *const *overrides)
{
    size_t env_count;
    size_t override_count;
    size_t capacity;
    size_t i;
    char **envp;

    env_count = 0;
    while (environ[env_count] != NULL)
    {
        env_count++;
    }

    override_count = 0;
    if (overrides != NULL)
    {
        while (overrides[override_count] != NULL)
        {
            if (env_key_len(overrides[override_count]) == 0)
            {
                return NULL;
            }
            override_count++;
        }
    }

    capacity = env_count + override_count + 1;
    envp = (char **)cplat_calloc(capacity, sizeof(*envp));
    if (envp == NULL)
    {
        return NULL;
    }

    i = 0;
    while (i < env_count)
    {
        envp[i] = string_duplicate(environ[i]);
        if (envp[i] == NULL)
        {
            free_envp(envp);
            return NULL;
        }
        i++;
    }
    envp[i] = NULL;

    i = 0;
    while (overrides != NULL && overrides[i] != NULL)
    {
        if (set_env_entry(envp, capacity, overrides[i]) != 0)
        {
            free_envp(envp);
            return NULL;
        }
        i++;
    }

    return envp;
}

static const char *find_env_value(char *const *envp, const char *key)
{
    size_t key_len;
    size_t i;

    key_len = strlen(key);
    i = 0;
    while (envp != NULL && envp[i] != NULL)
    {
        if (env_key_matches(envp[i], key, key_len) != 0)
        {
            return envp[i] + key_len + 1;
        }
        i++;
    }
    return NULL;
}

static int setup_child_stdio_one(const cplat_process_stdio *spec, const int target_fd, const int null_flags)
{
    int source_fd;

    if (spec->mode == CPLAT_PROCESS_STDIO_INHERIT)
    {
        return 0;
    }

    if (spec->mode == CPLAT_PROCESS_STDIO_NULL_DEVICE)
    {
        /* mode は O_CREAT を指定しないため使用されないが、testfw の open mock と
           Linux/Windows の可変引数差異を避けるため明示的に渡す。 */
        source_fd = open(PLATFORM_NULL_DEVICE_PATH, null_flags, 0);
        if (source_fd < 0)
        {
            return -1;
        }
    }
    else if (spec->mode == CPLAT_PROCESS_STDIO_NATIVE_HANDLE)
    {
        if (spec->native_handle < 0)
        {
            return -1;
        }
        source_fd = (int)spec->native_handle;
    }
    else
    {
        return -1;
    }

    if (dup2(source_fd, target_fd) < 0)
    {
        if (spec->mode == CPLAT_PROCESS_STDIO_NULL_DEVICE)
        {
            close(source_fd);
        }
        return -1;
    }

    if (spec->mode == CPLAT_PROCESS_STDIO_NULL_DEVICE)
    {
        close(source_fd);
    }
    return 0;
}

static int setup_child_stdio(const cplat_process_options *options)
{
    if (setup_child_stdio_one(&options->stdin_spec, STDIN_FILENO, O_RDONLY) != 0)
    {
        return -1;
    }
    if (setup_child_stdio_one(&options->stdout_spec, STDOUT_FILENO, O_WRONLY) != 0)
    {
        return -1;
    }
    if (setup_child_stdio_one(&options->stderr_spec, STDERR_FILENO, O_WRONLY) != 0)
    {
        return -1;
    }
    return 0;
}

static void exec_with_path(char *const *argv, char *const *envp)
{
    const char *path_value;
    const char *segment;
    const char *segment_end;
    size_t name_len;

    if (strchr(argv[0], '/') != NULL)
    {
        execve(argv[0], argv, envp);
        return;
    }

    path_value = find_env_value(envp, "PATH");
    if (path_value == NULL || path_value[0] == '\0')
    {
        path_value = "/bin:/usr/bin";
    }

    name_len = strlen(argv[0]);
    segment = path_value;
    while (*segment != '\0')
    {
        char candidate[PLATFORM_PATH_MAX];
        size_t dir_len;

        segment_end = strchr(segment, ':');
        if (segment_end == NULL)
        {
            segment_end = segment + strlen(segment);
        }
        dir_len = (size_t)(segment_end - segment);
        if (dir_len == 0)
        {
            dir_len = 1;
            segment = ".";
        }
        if (dir_len + 1 + name_len + 1 <= sizeof(candidate))
        {
            memcpy(candidate, segment, dir_len);
            candidate[dir_len] = '/';
            memcpy(candidate + dir_len + 1, argv[0], name_len + 1);
            execve(candidate, argv, envp);
        }
        if (*segment_end == '\0')
        {
            segment = segment_end;
        }
        else
        {
            segment = segment_end + 1;
        }
    }
}

/* fork 後の子プロセスでは、親の他スレッドが保持していたロックが解放されないまま複製される。
   stdio や strerror は内部でロックやロケールを使うため、非同期シグナル安全な関数だけで組み立てる。
   see: https://pubs.opengroup.org/onlinepubs/9799919799/functions/fork.html */

static size_t child_append_text(char *buf, const size_t buf_size, size_t len, const char *text)
{
    while (*text != '\0' && len + 1 < buf_size)
    {
        buf[len] = *text;
        len++;
        text++;
    }
    return len;
}

static size_t child_append_uint(char *buf, const size_t buf_size, size_t len, unsigned int magnitude)
{
    char digits[16];
    size_t count = 0;

    do
    {
        digits[count] = (char)('0' + (int)(magnitude % 10U));
        count++;
        magnitude /= 10U;
    } while (magnitude != 0U && count < sizeof(digits));
    while (count > 0 && len + 1 < buf_size)
    {
        count--;
        buf[len] = digits[count];
        len++;
    }
    return len;
}

static void child_report_exec_failure(const char *program, const int error_number)
{
    char message[512];
    size_t len = 0;
    ssize_t written;

    len = child_append_text(message, sizeof(message), len, "エラー: exec(\"");
    len = child_append_text(message, sizeof(message), len, program);
    len = child_append_text(message, sizeof(message), len, "\") が失敗しました: errno=");
    /* errno は正の値のため、符号なしで出力する。 */
    len = child_append_uint(message, sizeof(message), len, (unsigned int)error_number);
    len = child_append_text(message, sizeof(message), len, "\n");
    /* 書き込みの失敗は報告先がないため無視する。glibc の warn_unused_result は (void) で抑止できない。 */
    written = write(STDERR_FILENO, message, len);
    (void)written;
}

static int run_child_process(const cplat_process_options *options, char *const *envp)
{
    if (options->working_directory != NULL)
    {
        if (chdir(options->working_directory) != 0)
        {
            return 127;
        }
    }
    if (setup_child_stdio(options) != 0)
    {
        return 127;
    }
    exec_with_path(options->argv, envp);
    child_report_exec_failure(options->argv[0], errno);
    return 127;
}

#elif defined(PLATFORM_WINDOWS)
typedef struct wide_buffer
{
    wchar_t *data;
    size_t len;
    size_t cap;
} wide_buffer;

static int wide_buffer_reserve(wide_buffer *buf, const size_t need)
{
    wchar_t *new_data;
    size_t new_cap;

    if (need <= buf->cap)
    {
        return 0;
    }

    new_cap = buf->cap;
    if (new_cap == 0)
    {
        new_cap = 64;
    }
    while (new_cap < need)
    {
        new_cap *= 2;
    }

    new_data = (wchar_t *)cplat_realloc(buf->data, new_cap, sizeof(*new_data));
    if (new_data == NULL)
    {
        return -1;
    }
    buf->data = new_data;
    buf->cap = new_cap;
    return 0;
}

static int wide_buffer_append_char(wide_buffer *buf, const wchar_t ch)
{
    if (wide_buffer_reserve(buf, buf->len + 2) != 0)
    {
        return -1;
    }
    buf->data[buf->len] = ch;
    buf->len++;
    buf->data[buf->len] = L'\0';
    return 0;
}

static int wide_buffer_append_text(wide_buffer *buf, const wchar_t *text)
{
    size_t len;

    len = wcslen(text);
    if (wide_buffer_reserve(buf, buf->len + len + 1) != 0)
    {
        return -1;
    }
    memcpy(buf->data + buf->len, text, (len + 1) * sizeof(*text));
    buf->len += len;
    return 0;
}

static int append_windows_quoted_arg(wide_buffer *buf, const wchar_t *arg)
{
    size_t i;
    int need_quote;

    need_quote = 0;
    if (arg[0] == L'\0')
    {
        need_quote = 1;
    }
    i = 0;
    while (arg[i] != L'\0')
    {
        if (arg[i] == L' ' || arg[i] == L'\t' || arg[i] == L'"')
        {
            need_quote = 1;
        }
        i++;
    }

    if (need_quote == 0)
    {
        return wide_buffer_append_text(buf, arg);
    }

    if (wide_buffer_append_char(buf, L'"') != 0)
    {
        return -1;
    }

    i = 0;
    while (arg[i] != L'\0')
    {
        size_t slash_count;

        slash_count = 0;
        while (arg[i] == L'\\')
        {
            slash_count++;
            i++;
        }

        if (arg[i] == L'"')
        {
            while (slash_count > 0)
            {
                if (wide_buffer_append_char(buf, L'\\') != 0)
                {
                    return -1;
                }
                slash_count--;
            }
            if (wide_buffer_append_char(buf, L'\\') != 0 || wide_buffer_append_char(buf, L'"') != 0)
            {
                return -1;
            }
            i++;
        }
        else
        {
            while (slash_count > 0)
            {
                if (wide_buffer_append_char(buf, L'\\') != 0)
                {
                    return -1;
                }
                slash_count--;
            }
            if (arg[i] != L'\0')
            {
                if (wide_buffer_append_char(buf, arg[i]) != 0)
                {
                    return -1;
                }
                i++;
            }
        }
    }

    i = wcslen(arg);
    while (i > 0 && arg[i - 1] == L'\\')
    {
        if (wide_buffer_append_char(buf, L'\\') != 0)
        {
            return -1;
        }
        i--;
    }

    return wide_buffer_append_char(buf, L'"');
}

static wchar_t *build_command_line(char *const *argv)
{
    wide_buffer buf = {0};
    size_t i;

    i = 0;
    while (argv[i] != NULL)
    {
        wchar_t *wide_arg;
        int rc;

        wide_arg = cplat_utf8_to_wstr_alloc(argv[i]);
        if (wide_arg == NULL)
        {
            cplat_free(buf.data);
            return NULL;
        }
        if (i > 0)
        {
            if (wide_buffer_append_char(&buf, L' ') != 0)
            {
                cplat_free(wide_arg);
                cplat_free(buf.data);
                return NULL;
            }
        }
        rc = append_windows_quoted_arg(&buf, wide_arg);
        cplat_free(wide_arg);
        if (rc != 0)
        {
            cplat_free(buf.data);
            return NULL;
        }
        i++;
    }
    return buf.data;
}

static size_t wide_env_key_len(const wchar_t *entry)
{
    const wchar_t *sep;

    sep = wcschr(entry, L'=');
    if (sep == NULL)
    {
        return 0;
    }
    if (sep == entry)
    {
        return 0;
    }
    return (size_t)(sep - entry);
}

static int wide_env_key_matches(const wchar_t *entry, const wchar_t *key, const size_t key_len)
{
    if (_wcsnicmp(entry, key, key_len) != 0)
    {
        return 0;
    }
    if (entry[key_len] != L'=')
    {
        return 0;
    }
    return 1;
}

static void free_wide_env_list(wchar_t **list)
{
    size_t i;

    if (list == NULL)
    {
        return;
    }

    i = 0;
    while (list[i] != NULL)
    {
        cplat_free(list[i]);
        i++;
    }
    cplat_free(list);
}

static wchar_t *wide_string_duplicate(const wchar_t *text)
{
    wchar_t *copy;
    size_t len;

    len = wcslen(text) + 1;
    copy = (wchar_t *)cplat_calloc(len, sizeof(*copy));
    if (copy != NULL)
    {
        memcpy(copy, text, len * sizeof(*copy));
    }
    return copy;
}

static int set_wide_env_entry(wchar_t **list, const size_t capacity, const wchar_t *entry)
{
    size_t key_len;
    size_t i;
    wchar_t *copy;

    key_len = wide_env_key_len(entry);
    if (key_len == 0)
    {
        return -1;
    }

    copy = wide_string_duplicate(entry);
    if (copy == NULL)
    {
        return -1;
    }

    i = 0;
    while (list[i] != NULL)
    {
        if (wide_env_key_matches(list[i], entry, key_len) != 0)
        {
            cplat_free(list[i]);
            list[i] = copy;
            return 0;
        }
        i++;
    }

    if (i + 1 >= capacity)
    {
        cplat_free(copy);
        return -1;
    }
    list[i] = copy;
    list[i + 1] = NULL;
    return 0;
}

static wchar_t *build_environment_block(char *const *overrides)
{
    LPWCH current;
    LPWCH cursor;
    size_t env_count;
    size_t override_count;
    size_t capacity;
    wchar_t **list;
    size_t i;
    size_t block_len;
    wchar_t *block;
    wchar_t *out;

    if (overrides == NULL)
    {
        return NULL;
    }

    current = GetEnvironmentStringsW();
    if (current == NULL)
    {
        return NULL;
    }

    env_count = 0;
    cursor = current;
    while (*cursor != L'\0')
    {
        env_count++;
        cursor += wcslen(cursor) + 1;
    }

    override_count = 0;
    while (overrides[override_count] != NULL)
    {
        if (strchr(overrides[override_count], '=') == NULL)
        {
            FreeEnvironmentStringsW(current);
            return NULL;
        }
        override_count++;
    }

    capacity = env_count + override_count + 1;
    list = (wchar_t **)cplat_calloc(capacity, sizeof(*list));
    if (list == NULL)
    {
        FreeEnvironmentStringsW(current);
        return NULL;
    }

    i = 0;
    cursor = current;
    while (*cursor != L'\0')
    {
        list[i] = wide_string_duplicate(cursor);
        if (list[i] == NULL)
        {
            free_wide_env_list(list);
            FreeEnvironmentStringsW(current);
            return NULL;
        }
        i++;
        cursor += wcslen(cursor) + 1;
    }
    list[i] = NULL;
    FreeEnvironmentStringsW(current);

    i = 0;
    while (overrides[i] != NULL)
    {
        wchar_t *wide_entry;

        wide_entry = cplat_utf8_to_wstr_alloc(overrides[i]);
        if (wide_entry == NULL)
        {
            free_wide_env_list(list);
            return NULL;
        }
        if (set_wide_env_entry(list, capacity, wide_entry) != 0)
        {
            cplat_free(wide_entry);
            free_wide_env_list(list);
            return NULL;
        }
        cplat_free(wide_entry);
        i++;
    }

    block_len = 1;
    i = 0;
    while (list[i] != NULL)
    {
        block_len += wcslen(list[i]) + 1;
        i++;
    }

    block = (wchar_t *)cplat_calloc(block_len, sizeof(*block));
    if (block == NULL)
    {
        free_wide_env_list(list);
        return NULL;
    }

    out = block;
    i = 0;
    while (list[i] != NULL)
    {
        size_t len;

        len = wcslen(list[i]) + 1;
        memcpy(out, list[i], len * sizeof(*out));
        out += len;
        i++;
    }
    *out = L'\0';
    free_wide_env_list(list);
    return block;
}

static HANDLE open_windows_null_device(const DWORD access)
{
    return CreateFileU(PLATFORM_NULL_DEVICE_PATH, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
}

static int duplicate_inheritable_handle(HANDLE source, HANDLE *duplicated)
{
    HANDLE current;

    current = GetCurrentProcess();
    if (!DuplicateHandle(current, source, current, duplicated, 0, TRUE, DUPLICATE_SAME_ACCESS))
    {
        return -1;
    }
    return 0;
}

static int prepare_stdio_handle(const cplat_process_stdio *spec, const DWORD std_id, const DWORD null_access,
                                HANDLE *prepared)
{
    HANDLE source;

    *prepared = NULL;
    if (spec->mode == CPLAT_PROCESS_STDIO_INHERIT)
    {
        source = GetStdHandle(std_id);
        if (source == NULL || source == INVALID_HANDLE_VALUE)
        {
            source = open_windows_null_device(null_access);
            if (source == INVALID_HANDLE_VALUE)
            {
                return -1;
            }
            *prepared = source;
            return 0;
        }
        return duplicate_inheritable_handle(source, prepared);
    }

    if (spec->mode == CPLAT_PROCESS_STDIO_NULL_DEVICE)
    {
        source = open_windows_null_device(null_access);
        if (source == INVALID_HANDLE_VALUE)
        {
            return -1;
        }
        *prepared = source;
        return 0;
    }

    if (spec->mode == CPLAT_PROCESS_STDIO_NATIVE_HANDLE)
    {
        source = (HANDLE)spec->native_handle;
        if (source == NULL || source == INVALID_HANDLE_VALUE)
        {
            return -1;
        }
        return duplicate_inheritable_handle(source, prepared);
    }

    return -1;
}

static void close_stdio_handles(HANDLE handles[3])
{
    size_t i;

    i = 0;
    while (i < 3)
    {
        if (handles[i] != NULL && handles[i] != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handles[i]);
        }
        i++;
    }
}

/**
 *  @brief  cplat_process_start (Windows) が確保する一時リソースの集合です。
 *
 *  ゼロ初期化した状態から任意の時点で release_process_start_resources() へ
 *  渡せるため、確保段階ごとに解放シーケンスを書き分ける必要がありません。
 */
struct process_start_resources
{
    wchar_t *command_line;
    wchar_t *environment_block;
    wchar_t *working_directory;
    HANDLE stdio_handles[3];
    LPPROC_THREAD_ATTRIBUTE_LIST attribute_list;
    int attribute_list_initialized;
};

/**
 *  @brief          cplat_process_start (Windows) の一時リソースをすべて解放します。
 *  @param[in]      res  解放対象のリソース集合。未確保メンバーは 0/NULL であること。
 */
static void release_process_start_resources(struct process_start_resources *res)
{
    if (res->attribute_list_initialized)
    {
        DeleteProcThreadAttributeList(res->attribute_list);
    }
    cplat_free(res->attribute_list);
    close_stdio_handles(res->stdio_handles);
    cplat_free(res->working_directory);
    cplat_free(res->environment_block);
    cplat_free(res->command_line);
}
#endif /* PLATFORM_ */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_get_executable_path(char *path_out, const size_t path_size)
{
    if (path_out == NULL || path_size == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_LINUX)
    {
        ssize_t len;

        if (path_size == 1)
        {
            path_out[0] = '\0';
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
        len = readlink("/proc/self/exe", path_out, path_size - 1);
        if (len < 0)
        {
            path_out[0] = '\0';
            return cplat_internal_result_from_errno(errno);
        }
        if ((size_t)len >= path_size)
        {
            path_out[0] = '\0';
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
        path_out[len] = '\0';
        return CPLAT_OK;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wbuf[PLATFORM_PATH_MAX];
        DWORD n;
        int result;

        /* GetModuleFileNameU はパス区切り文字を正規化しない。
           '\\' を '/' へ正規化する cplat_wpath_to_utf8 を使うため、ここでは W 版を直接使う */
        n = GetModuleFileNameW(NULL, wbuf, (DWORD)(sizeof(wbuf) / sizeof(wbuf[0])));
        if (n == 0)
        {
            path_out[0] = '\0';
            return cplat_internal_result_from_windows_error(GetLastError());
        }
        if (n >= (DWORD)(sizeof(wbuf) / sizeof(wbuf[0])))
        {
            path_out[0] = '\0';
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
        wbuf[n] = L'\0';
        if (cplat_wpath_to_utf8(path_out, path_size, wbuf) < 0)
        {
            result = cplat_internal_result_from_windows_error(GetLastError());
            path_out[0] = '\0';
            return result;
        }
        return CPLAT_OK;
    }
#else
    path_out[0] = '\0';
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

uint32_t cplat_process_get_pid(void)
{
#if defined(PLATFORM_LINUX)
    return (uint32_t)getpid();
#elif defined(PLATFORM_WINDOWS)
    return (uint32_t)GetCurrentProcessId();
#else
    return 0;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

uint32_t cplat_process_get_tid(void)
{
#if defined(PLATFORM_LINUX)
    /* glibc がラッパー関数 gettid() を提供するのは 2.30 以降であり、本リポジトリが対象とする
       glibc 2.28 では宣言されない。移植先の glibc の版に依存しないよう、システム コールを直接呼ぶ。
       see: https://man7.org/linux/man-pages/man2/gettid.2.html */
    return (uint32_t)syscall(SYS_gettid);
#elif defined(PLATFORM_WINDOWS)
    return (uint32_t)GetCurrentThreadId();
#else
    return 0;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_start(const cplat_process_options *options, cplat_process **process)
{
    cplat_process *new_process;

    if (options == NULL || process == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (has_valid_argv(options->argv) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *process = NULL;

#if defined(PLATFORM_LINUX)
    {
        char **envp;
        pid_t pid;

        envp = build_environment(options->env_overrides);
        if (envp == NULL)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }

        new_process = (cplat_process *)cplat_calloc(1, sizeof(*new_process));
        if (new_process == NULL)
        {
            free_envp(envp);
            return CPLAT_ERR_UNKNOWN;
        }

        pid = fork();
        if (pid < 0)
        {
            free_envp(envp);
            cplat_free(new_process);
            return CPLAT_ERR_UNKNOWN;
        }

        /* GCOVR_EXCL_START: 子プロセスの _exit() 前に gcda を安全に保存できない。 */
        if (pid == 0)
        {
            _exit(run_child_process(options, envp));
        }
        /* GCOVR_EXCL_STOP */

        free_envp(envp);
        new_process->pid = pid;
        *process = new_process;
        return CPLAT_OK;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        STARTUPINFOEXW startup = {0};
        PROCESS_INFORMATION process_info = {0};
        SIZE_T attr_size;
        HANDLE inherit_handles[3] = {0};
        struct process_start_resources res = {0};
        DWORD create_flags;
        BOOL created;

        res.command_line = build_command_line(options->argv);
        if (res.command_line == NULL)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        res.environment_block = build_environment_block(options->env_overrides);
        if (options->env_overrides != NULL && res.environment_block == NULL)
        {
            release_process_start_resources(&res);
            return CPLAT_ERR_INVALID_ARGUMENT;
        }

        if (options->working_directory != NULL)
        {
            res.working_directory = cplat_utf8_to_wstr_alloc(options->working_directory);
            if (res.working_directory == NULL)
            {
                release_process_start_resources(&res);
                return CPLAT_ERR_UNKNOWN;
            }
        }

        if (prepare_stdio_handle(&options->stdin_spec, STD_INPUT_HANDLE, GENERIC_READ, &res.stdio_handles[0]) != 0 ||
            prepare_stdio_handle(&options->stdout_spec, STD_OUTPUT_HANDLE, GENERIC_WRITE, &res.stdio_handles[1]) != 0 ||
            prepare_stdio_handle(&options->stderr_spec, STD_ERROR_HANDLE, GENERIC_WRITE, &res.stdio_handles[2]) != 0)
        {
            release_process_start_resources(&res);
            return CPLAT_ERR_UNKNOWN;
        }

        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = res.stdio_handles[0];
        startup.StartupInfo.hStdOutput = res.stdio_handles[1];
        startup.StartupInfo.hStdError = res.stdio_handles[2];

        attr_size = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attr_size);
        res.attribute_list = (LPPROC_THREAD_ATTRIBUTE_LIST)cplat_calloc(1, attr_size);
        if (res.attribute_list == NULL)
        {
            release_process_start_resources(&res);
            return CPLAT_ERR_UNKNOWN;
        }
        if (!InitializeProcThreadAttributeList(res.attribute_list, 1, 0, &attr_size))
        {
            release_process_start_resources(&res);
            return CPLAT_ERR_UNKNOWN;
        }
        res.attribute_list_initialized = 1;
        startup.lpAttributeList = res.attribute_list;

        inherit_handles[0] = res.stdio_handles[0];
        inherit_handles[1] = res.stdio_handles[1];
        inherit_handles[2] = res.stdio_handles[2];
        if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit_handles,
                                       sizeof(inherit_handles), NULL, NULL))
        {
            release_process_start_resources(&res);
            return CPLAT_ERR_UNKNOWN;
        }

        create_flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
        /* res.command_line と res.environment_block は組み立て済みのワイド文字列であり、
           CreateProcessU を経由すると UTF-8 への往復変換が増えるだけになる */
        created = CreateProcessW(NULL, res.command_line, NULL, NULL, TRUE, create_flags, res.environment_block,
                                 res.working_directory, &startup.StartupInfo, &process_info);

        release_process_start_resources(&res);

        if (!created)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        CloseHandle(process_info.hThread);
        new_process = (cplat_process *)cplat_calloc(1, sizeof(*new_process));
        if (new_process == NULL)
        {
            CloseHandle(process_info.hProcess);
            return CPLAT_ERR_UNKNOWN;
        }
        new_process->process = process_info.hProcess;
        *process = new_process;
        return CPLAT_OK;
    }
#else
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_wait(cplat_process *process, const int timeout_ms)
{
    if (process == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (process->exited != 0)
    {
        return CPLAT_OK;
    }

#if defined(PLATFORM_LINUX)
    {
        int status;
        int wait_options;
        uint64_t deadline;

        wait_options = 0;
        if (timeout_ms != CPLAT_PROCESS_WAIT_FOREVER)
        {
            wait_options = WNOHANG;
            deadline = monotonic_ms() + (uint64_t)timeout_ms;
        }
        else
        {
            deadline = 0;
        }

        while (1)
        {
            pid_t rc;

            rc = waitpid(process->pid, &status, wait_options);
            if (rc == process->pid)
            {
                if (WIFEXITED(status))
                {
                    process_set_exit_code(process, WEXITSTATUS(status));
                }
                else
                {
                    process_set_exit_code(process, -1);
                }
                return CPLAT_OK;
            }
            if (rc < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return CPLAT_ERR_UNKNOWN;
            }
            if (timeout_ms == CPLAT_PROCESS_NO_WAIT)
            {
                return CPLAT_ERR_TIMEOUT;
            }
            if (timeout_ms != CPLAT_PROCESS_WAIT_FOREVER && monotonic_ms() >= deadline)
            {
                return CPLAT_ERR_TIMEOUT;
            }
            usleep(1000);
        }
    }
#elif defined(PLATFORM_WINDOWS)
    {
        DWORD wait_ms;
        DWORD wait_result;
        DWORD child_exit_code;

        if (timeout_ms == CPLAT_PROCESS_WAIT_FOREVER)
        {
            wait_ms = INFINITE;
        }
        else
        {
            wait_ms = (DWORD)timeout_ms;
        }

        wait_result = WaitForSingleObject(process->process, wait_ms);
        if (wait_result == WAIT_TIMEOUT)
        {
            return CPLAT_ERR_TIMEOUT;
        }
        if (wait_result != WAIT_OBJECT_0)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        child_exit_code = EXIT_FAILURE;
        if (!GetExitCodeProcess(process->process, &child_exit_code))
        {
            return CPLAT_ERR_UNKNOWN;
        }
        if (child_exit_code > INT_MAX)
        {
            process_set_exit_code(process, EXIT_FAILURE);
        }
        else
        {
            process_set_exit_code(process, (int)child_exit_code);
        }
        return CPLAT_OK;
    }
#else
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_get_exit_code(cplat_process *process, int *exit_code)
{
    if (process == NULL || exit_code == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (process->exited == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *exit_code = process->exit_code;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_terminate(cplat_process *process)
{
    if (process == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (process->exited != 0)
    {
        return CPLAT_OK;
    }

#if defined(PLATFORM_LINUX)
    if (kill(process->pid, SIGTERM) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    return CPLAT_OK;
#elif defined(PLATFORM_WINDOWS)
    if (!TerminateProcess(process->process, EXIT_FAILURE))
    {
        return CPLAT_ERR_UNKNOWN;
    }
    return CPLAT_OK;
#else
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_process_dispose(cplat_process *process)
{
    if (process == NULL)
    {
        return;
    }

#if defined(PLATFORM_WINDOWS)
    if (process->process != NULL)
    {
        CloseHandle(process->process);
    }
#endif /* PLATFORM_WINDOWS */
    cplat_free(process);
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_process *cplat_internal_process_adopt_native(const intptr_t native_handle)
{
    cplat_process *new_process;

    new_process = (cplat_process *)cplat_calloc(1, sizeof(*new_process));
    if (new_process == NULL)
    {
        return NULL;
    }

#if defined(PLATFORM_LINUX)
    new_process->pid = (pid_t)native_handle;
#elif defined(PLATFORM_WINDOWS)
    new_process->process = (HANDLE)native_handle;
#else
    cplat_free(new_process);
    return NULL;
#endif /* PLATFORM_ */
    return new_process;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_run_sync(const cplat_process_options *options, const int timeout_ms, int *exit_code)
{
    cplat_process *process;
    int result;

    if (exit_code == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *exit_code = EXIT_FAILURE;

    process = NULL;
    result = cplat_process_start(options, &process);
    if (result != CPLAT_OK)
    {
        return result;
    }

    result = cplat_process_wait(process, timeout_ms);
    if (result == CPLAT_OK)
    {
        result = cplat_process_get_exit_code(process, exit_code);
    }
    cplat_process_dispose(process);
    return result;
}
