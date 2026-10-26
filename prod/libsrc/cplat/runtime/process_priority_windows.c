/**
 *******************************************************************************
 *  @file           process_priority_windows.c
 *  @brief          Windows のプロセスとスレッドの優先度の設定・取得を OS API へ委譲します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/26
 *
 *  ID 0 は擬似ハンドル (GetCurrentProcess()、GetCurrentThread()) で操作し、それ以外の ID は
 *  必要なアクセス権で開いて操作した後に閉じます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <errno.h>
    #include <stdint.h>
    #include <string.h>

    #include <cplat/base/windows_sdk.h>
    #include <cplat/base/error_internal.h>
    #include <cplat/runtime/process_priority_windows.h>

/* OpenProcess() / OpenThread() は、存在しない ID に ERROR_INVALID_PARAMETER を返す。
   ERROR_INVALID_PARAMETER は他の用途でも使われるため、対象を開く箇所でだけ対象不在へ変換する。
   see: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-openprocess */
static int report_open_error(cplat_error *detail_out, DWORD error_code)
{
    if (error_code == ERROR_INVALID_PARAMETER)
    {
        return cplat_internal_error_report_windows_error_as(detail_out, error_code, CPLAT_ERR_NOT_FOUND);
    }
    return cplat_internal_error_report_windows_error(detail_out, error_code);
}

static int open_process(uint32_t process_id, DWORD desired_access, HANDLE *process, cplat_error *detail_out)
{
    HANDLE opened;

    if (process_id == 0U)
    {
        *process = GetCurrentProcess();
        return CPLAT_OK;
    }

    opened = OpenProcess(desired_access, FALSE, (DWORD)process_id);
    if (opened == NULL)
    {
        return report_open_error(detail_out, GetLastError());
    }
    *process = opened;
    return CPLAT_OK;
}

static int open_thread(uint32_t thread_id, DWORD desired_access, HANDLE *thread, cplat_error *detail_out)
{
    HANDLE opened;

    if (thread_id == 0U)
    {
        *thread = GetCurrentThread();
        return CPLAT_OK;
    }

    opened = OpenThread(desired_access, FALSE, (DWORD)thread_id);
    if (opened == NULL)
    {
        return report_open_error(detail_out, GetLastError());
    }
    *thread = opened;
    return CPLAT_OK;
}

/* 擬似ハンドルは閉じない。 */
static void close_target(uint32_t id, HANDLE handle)
{
    if (id != 0U)
    {
        (void)CloseHandle(handle);
    }
}

static int to_os_class(cplat_process_priority_windows_class priority_class, DWORD *os_class)
{
    switch (priority_class)
    {
    case CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_IDLE:
        *os_class = IDLE_PRIORITY_CLASS;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_BELOW_NORMAL:
        *os_class = BELOW_NORMAL_PRIORITY_CLASS;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_NORMAL:
        *os_class = NORMAL_PRIORITY_CLASS;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_ABOVE_NORMAL:
        *os_class = ABOVE_NORMAL_PRIORITY_CLASS;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_HIGH:
        *os_class = HIGH_PRIORITY_CLASS;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_REAL_TIME:
        *os_class = REALTIME_PRIORITY_CLASS;
        return 1;
    default:
        return 0;
    }
}

static int from_os_class(DWORD os_class, cplat_process_priority_windows_class *priority_class)
{
    switch (os_class)
    {
    case IDLE_PRIORITY_CLASS:
        *priority_class = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_IDLE;
        return 1;
    case BELOW_NORMAL_PRIORITY_CLASS:
        *priority_class = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_BELOW_NORMAL;
        return 1;
    case NORMAL_PRIORITY_CLASS:
        *priority_class = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_NORMAL;
        return 1;
    case ABOVE_NORMAL_PRIORITY_CLASS:
        *priority_class = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_ABOVE_NORMAL;
        return 1;
    case HIGH_PRIORITY_CLASS:
        *priority_class = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_HIGH;
        return 1;
    case REALTIME_PRIORITY_CLASS:
        *priority_class = CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_REAL_TIME;
        return 1;
    default:
        return 0;
    }
}

static int to_os_thread_priority(cplat_process_priority_windows_thread_priority thread_priority, int *os_priority)
{
    switch (thread_priority)
    {
    case CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_IDLE:
        *os_priority = THREAD_PRIORITY_IDLE;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_LOWEST:
        *os_priority = THREAD_PRIORITY_LOWEST;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_BELOW_NORMAL:
        *os_priority = THREAD_PRIORITY_BELOW_NORMAL;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_NORMAL:
        *os_priority = THREAD_PRIORITY_NORMAL;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL:
        *os_priority = THREAD_PRIORITY_ABOVE_NORMAL;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_HIGHEST:
        *os_priority = THREAD_PRIORITY_HIGHEST;
        return 1;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_TIME_CRITICAL:
        *os_priority = THREAD_PRIORITY_TIME_CRITICAL;
        return 1;
    default:
        return 0;
    }
}

static int from_os_thread_priority(int os_priority, cplat_process_priority_windows_thread_priority *thread_priority)
{
    switch (os_priority)
    {
    case THREAD_PRIORITY_IDLE:
        *thread_priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_IDLE;
        return 1;
    case THREAD_PRIORITY_LOWEST:
        *thread_priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_LOWEST;
        return 1;
    case THREAD_PRIORITY_BELOW_NORMAL:
        *thread_priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_BELOW_NORMAL;
        return 1;
    case THREAD_PRIORITY_NORMAL:
        *thread_priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_NORMAL;
        return 1;
    case THREAD_PRIORITY_ABOVE_NORMAL:
        *thread_priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL;
        return 1;
    case THREAD_PRIORITY_HIGHEST:
        *thread_priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_HIGHEST;
        return 1;
    case THREAD_PRIORITY_TIME_CRITICAL:
        *thread_priority = CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_TIME_CRITICAL;
        return 1;
    default:
        return 0;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_set_class(uint32_t process_id, cplat_process_priority_windows_class priority_class,
                                             cplat_error *detail_out)
{
    HANDLE process;
    DWORD os_class;
    DWORD previous_class = 0;
    DWORD current_class;
    int result;

    if (!to_os_class(priority_class, &os_class))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, &process,
                          detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    if (os_class == REALTIME_PRIORITY_CLASS)
    {
        previous_class = GetPriorityClass(process);
        if (previous_class == 0)
        {
            result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
            close_target(process_id, process);
            return result;
        }
    }

    if (!SetPriorityClass(process, os_class))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }

    if (os_class == REALTIME_PRIORITY_CLASS)
    {
        /* 特権 SeIncreaseBasePriorityPrivilege がない場合、OS は失敗を返さずに HIGH を設定する。
           この挙動は公式文書に記載がないため、OS の特権判定を再現せず、設定後の結果を確認する。
           格下げを検出した場合は、失敗時に状態を変えない契約を保つため元のクラスへ戻す。
           see: https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities */
        current_class = GetPriorityClass(process);
        if (current_class == 0)
        {
            result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
            close_target(process_id, process);
            return result;
        }
        if (current_class != REALTIME_PRIORITY_CLASS)
        {
            (void)SetPriorityClass(process, previous_class);
            close_target(process_id, process);
            return cplat_internal_error_report_windows_error(detail_out, ERROR_PRIVILEGE_NOT_HELD);
        }
    }

    close_target(process_id, process);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_get_class(uint32_t process_id,
                                             cplat_process_priority_windows_class *priority_class_out,
                                             cplat_error *detail_out)
{
    HANDLE process;
    DWORD os_class;
    cplat_process_priority_windows_class priority_class;
    int result;

    if (priority_class_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_QUERY_LIMITED_INFORMATION, &process, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    os_class = GetPriorityClass(process);
    if (os_class == 0)
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }
    close_target(process_id, process);

    if (!from_os_class(os_class, &priority_class))
    {
        return cplat_internal_error_report_windows_error(detail_out, ERROR_INVALID_DATA);
    }
    *priority_class_out = priority_class;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_begin_background_mode(cplat_error *detail_out)
{
    DWORD error_code;

    if (!SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN))
    {
        /* すでにモード中の場合、OS は ERROR_PROCESS_MODE_ALREADY_BACKGROUND で失敗する。
           要求どおりの状態であり失敗ではないため、省略として扱う。
           see: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass */
        error_code = GetLastError();
        if (error_code == ERROR_PROCESS_MODE_ALREADY_BACKGROUND)
        {
            (void)cplat_internal_error_report_success(detail_out);
            return CPLAT_SKIPPED;
        }
        return cplat_internal_error_report_windows_error(detail_out, error_code);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_end_background_mode(cplat_error *detail_out)
{
    DWORD error_code;

    if (!SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END))
    {
        /* モード外の場合、OS は ERROR_PROCESS_MODE_NOT_BACKGROUND で失敗する。
           see: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass */
        error_code = GetLastError();
        if (error_code == ERROR_PROCESS_MODE_NOT_BACKGROUND)
        {
            (void)cplat_internal_error_report_success(detail_out);
            return CPLAT_SKIPPED;
        }
        return cplat_internal_error_report_windows_error(detail_out, error_code);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_set_memory_priority(uint32_t process_id,
                                                       cplat_process_priority_windows_memory_priority memory_priority,
                                                       cplat_error *detail_out)
{
    HANDLE process;
    MEMORY_PRIORITY_INFORMATION information;
    int result;

    /* 列挙子の値は MEMORY_PRIORITY_* と一致させている。 */
    if (((int)memory_priority < (int)CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_VERY_LOW) ||
        ((int)memory_priority > (int)CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_NORMAL))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_SET_INFORMATION, &process, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    memset(&information, 0, sizeof(information));
    information.MemoryPriority = (ULONG)memory_priority;
    if (!SetProcessInformation(process, ProcessMemoryPriority, &information, (DWORD)sizeof(information)))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }

    close_target(process_id, process);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_get_memory_priority(
    uint32_t process_id, cplat_process_priority_windows_memory_priority *memory_priority_out, cplat_error *detail_out)
{
    HANDLE process;
    MEMORY_PRIORITY_INFORMATION information;
    int result;

    if (memory_priority_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_QUERY_LIMITED_INFORMATION, &process, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    memset(&information, 0, sizeof(information));
    if (!GetProcessInformation(process, ProcessMemoryPriority, &information, (DWORD)sizeof(information)))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }
    close_target(process_id, process);

    if ((information.MemoryPriority < (ULONG)CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_VERY_LOW) ||
        (information.MemoryPriority > (ULONG)CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_NORMAL))
    {
        return cplat_internal_error_report_windows_error(detail_out, ERROR_INVALID_DATA);
    }
    *memory_priority_out = (cplat_process_priority_windows_memory_priority)information.MemoryPriority;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_set_power_throttling(
    uint32_t process_id, cplat_process_priority_windows_power_throttling power_throttling, cplat_error *detail_out)
{
    HANDLE process;
    PROCESS_POWER_THROTTLING_STATE state;
    int result;

    if ((power_throttling != CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_SYSTEM_MANAGED) &&
        (power_throttling != CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_ENABLED) &&
        (power_throttling != CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_DISABLED))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, &process,
                          detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    /* SetProcessInformation() は状態全体を置き換えるため、取得できる場合は現在の状態を基にして
       実行速度の制御ビットだけを変更し、他の制御 (タイマー分解能など) を保つ。
       see: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation */
    memset(&state, 0, sizeof(state));
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    if (!GetProcessInformation(process, ProcessPowerThrottling, &state, (DWORD)sizeof(state)))
    {
        memset(&state, 0, sizeof(state));
        state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    }

    switch (power_throttling)
    {
    case CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_ENABLED:
        state.ControlMask |= PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask |= PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        break;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_DISABLED:
        state.ControlMask |= PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask &= ~(ULONG)PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        break;
    case CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_SYSTEM_MANAGED:
    default:
        state.ControlMask &= ~(ULONG)PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask &= ~(ULONG)PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        break;
    }

    if (!SetProcessInformation(process, ProcessPowerThrottling, &state, (DWORD)sizeof(state)))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }

    close_target(process_id, process);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_get_power_throttling(
    uint32_t process_id, cplat_process_priority_windows_power_throttling *power_throttling_out,
    cplat_error *detail_out)
{
    HANDLE process;
    PROCESS_POWER_THROTTLING_STATE state;
    int result;

    if (power_throttling_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_QUERY_LIMITED_INFORMATION, &process, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    memset(&state, 0, sizeof(state));
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    if (!GetProcessInformation(process, ProcessPowerThrottling, &state, (DWORD)sizeof(state)))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }
    close_target(process_id, process);

    if ((state.ControlMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) == 0U)
    {
        *power_throttling_out = CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_SYSTEM_MANAGED;
    }
    else if ((state.StateMask & PROCESS_POWER_THROTTLING_EXECUTION_SPEED) != 0U)
    {
        *power_throttling_out = CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_ENABLED;
    }
    else
    {
        *power_throttling_out = CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_DISABLED;
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_set_priority_boost(uint32_t process_id, int enabled, cplat_error *detail_out)
{
    HANDLE process;
    int result;

    if ((enabled != 0) && (enabled != 1))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_SET_INFORMATION, &process, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    /* OS の引数は「無効にするか」であり、enabled と逆の意味を持つ。 */
    if (!SetProcessPriorityBoost(process, (enabled != 0) ? FALSE : TRUE))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }

    close_target(process_id, process);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_get_priority_boost(uint32_t process_id, int *enabled_out, cplat_error *detail_out)
{
    HANDLE process;
    BOOL disabled = FALSE;
    int result;

    if (enabled_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_process(process_id, PROCESS_QUERY_LIMITED_INFORMATION, &process, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    if (!GetProcessPriorityBoost(process, &disabled))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(process_id, process);
        return result;
    }
    close_target(process_id, process);

    *enabled_out = disabled ? 0 : 1;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_set_thread_priority(uint32_t thread_id,
                                                       cplat_process_priority_windows_thread_priority thread_priority,
                                                       cplat_error *detail_out)
{
    HANDLE thread;
    int os_priority;
    int result;

    if (!to_os_thread_priority(thread_priority, &os_priority))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_thread(thread_id, THREAD_SET_LIMITED_INFORMATION, &thread, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    if (!SetThreadPriority(thread, os_priority))
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(thread_id, thread);
        return result;
    }

    close_target(thread_id, thread);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_windows_get_thread_priority(
    uint32_t thread_id, cplat_process_priority_windows_thread_priority *thread_priority_out, cplat_error *detail_out)
{
    HANDLE thread;
    int os_priority;
    cplat_process_priority_windows_thread_priority thread_priority;
    int result;

    if (thread_priority_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    result = open_thread(thread_id, THREAD_QUERY_LIMITED_INFORMATION, &thread, detail_out);
    if (result != CPLAT_OK)
    {
        return result;
    }

    os_priority = GetThreadPriority(thread);
    if (os_priority == THREAD_PRIORITY_ERROR_RETURN)
    {
        result = cplat_internal_error_report_windows_error(detail_out, GetLastError());
        close_target(thread_id, thread);
        return result;
    }
    close_target(thread_id, thread);

    /* REALTIME クラス内でだけ使える値 (-7 〜 -3、3 〜 6) は列挙型に含めない。 */
    if (!from_os_thread_priority(os_priority, &thread_priority))
    {
        return cplat_internal_error_report_windows_error(detail_out, ERROR_INVALID_DATA);
    }
    *thread_priority_out = thread_priority;
    return cplat_internal_error_report_success(detail_out);
}

#endif /* PLATFORM_WINDOWS */
