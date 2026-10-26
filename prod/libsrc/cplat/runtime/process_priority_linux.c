#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif /* _GNU_SOURCE */

/**
 *******************************************************************************
 *  @file           process_priority_linux.c
 *  @brief          Linux のスケジューリングと優先度の設定・取得を OS API へ委譲します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/26
 *
 *  `sched_setattr()`、`sched_getattr()`、`ioprio_set()`、`ioprio_get()` は glibc 2.28 に
 *  ラッパー関数がないため、`syscall()` で呼び出します。\n
 *  構造体 `sched_attr` と `IOPRIO_*` の定数は、glibc とカーネル ヘッダーの版に依存しないよう
 *  本ファイルで定義します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <errno.h>
    #include <stdint.h>
    #include <string.h>
    #include <sched.h>
    #include <sys/resource.h>
    #include <sys/syscall.h>
    #include <unistd.h>

    #include <cplat/base/error_internal.h>
    #include <cplat/runtime/process_priority_linux.h>

    /* see: https://man7.org/linux/man-pages/man7/sched.7.html */
    #ifndef SCHED_BATCH
        #define SCHED_BATCH 3
    #endif
    #ifndef SCHED_IDLE
        #define SCHED_IDLE 5
    #endif
    #ifndef SCHED_DEADLINE
        #define SCHED_DEADLINE 6
    #endif
    #ifndef SCHED_RESET_ON_FORK
        #define SCHED_RESET_ON_FORK 0x40000000
    #endif

    /* sched_attr.sched_flags の値。
       see: https://man7.org/linux/man-pages/man2/sched_setattr.2.html */
    #define PROCESS_PRIORITY_SCHED_FLAG_RESET_ON_FORK  0x01ULL
    #define PROCESS_PRIORITY_SCHED_FLAG_KEEP_POLICY    0x08ULL
    #define PROCESS_PRIORITY_SCHED_FLAG_KEEP_PARAMS    0x10ULL
    #define PROCESS_PRIORITY_SCHED_FLAG_UTIL_CLAMP_MIN 0x20ULL
    #define PROCESS_PRIORITY_SCHED_FLAG_UTIL_CLAMP_MAX 0x40ULL

    /* sched_attr の版ごとの大きさ。最初の版はデッド ラインまで、
       2 番目の版 (Linux 5.3 以降) は CPU 使用率クランプまでを持つ。 */
    #define PROCESS_PRIORITY_SCHED_ATTR_SIZE_VERSION0 48U
    #define PROCESS_PRIORITY_SCHED_ATTR_SIZE_VERSION1 56U

    /* CPU 使用率クランプの上限値 (SCHED_CAPACITY_SCALE)。 */
    #define PROCESS_PRIORITY_UTILIZATION_SCALE 1024U

    /* see: https://man7.org/linux/man-pages/man2/ioprio_set.2.html */
    #define PROCESS_PRIORITY_IOPRIO_WHO_PROCESS 1
    #define PROCESS_PRIORITY_IOPRIO_CLASS_SHIFT 13
    #define PROCESS_PRIORITY_IOPRIO_LEVEL_MASK  0x7
    #define PROCESS_PRIORITY_IOPRIO_LEVEL_MAX   7

    /* CPU 使用率クランプ (CONFIG_UCLAMP_TASK) が有効なカーネルにだけ存在する sysctl。
       無効なカーネルでも sched_getattr() は成功して 0 と 0 を返すため、この有無で判定する。
       see: https://docs.kernel.org/admin-guide/sysctl/kernel.html#sched-util-clamp-min */
    #define PROCESS_PRIORITY_UTILIZATION_CLAMP_SYSCTL "/proc/sys/kernel/sched_util_clamp_min"

/* カーネルの struct sched_attr (2 番目の版) と同じ配置。 */
typedef struct process_priority_sched_attr
{
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
    uint32_t sched_util_min;
    uint32_t sched_util_max;
} process_priority_sched_attr;

static int to_os_thread_id(uint32_t thread_id, pid_t *os_thread_id)
{
    if (thread_id > (uint32_t)INT32_MAX)
    {
        return 0;
    }
    *os_thread_id = (pid_t)thread_id;
    return 1;
}

static int to_os_policy(cplat_process_priority_linux_policy policy, int *os_policy)
{
    switch (policy)
    {
    case CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER:
        *os_policy = SCHED_OTHER;
        return 1;
    case CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH:
        *os_policy = SCHED_BATCH;
        return 1;
    case CPLAT_PROCESS_PRIORITY_LINUX_POLICY_IDLE:
        *os_policy = SCHED_IDLE;
        return 1;
    case CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO:
        *os_policy = SCHED_FIFO;
        return 1;
    case CPLAT_PROCESS_PRIORITY_LINUX_POLICY_ROUND_ROBIN:
        *os_policy = SCHED_RR;
        return 1;
    case CPLAT_PROCESS_PRIORITY_LINUX_POLICY_DEADLINE:
        /* デッド ラインは sched_setattr() で設定するため、sched_setscheduler() では扱わない。 */
        return 0;
    default:
        return 0;
    }
}

static int from_os_policy(int os_policy, cplat_process_priority_linux_policy *policy)
{
    switch (os_policy)
    {
    case SCHED_OTHER:
        *policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER;
        return 1;
    case SCHED_BATCH:
        *policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH;
        return 1;
    case SCHED_IDLE:
        *policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_IDLE;
        return 1;
    case SCHED_FIFO:
        *policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO;
        return 1;
    case SCHED_RR:
        *policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_ROUND_ROBIN;
        return 1;
    case SCHED_DEADLINE:
        *policy = CPLAT_PROCESS_PRIORITY_LINUX_POLICY_DEADLINE;
        return 1;
    default:
        return 0;
    }
}

static int is_valid_static_priority(cplat_process_priority_linux_policy policy, int static_priority)
{
    if ((policy == CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO) ||
        (policy == CPLAT_PROCESS_PRIORITY_LINUX_POLICY_ROUND_ROBIN))
    {
        return (static_priority >= 1) && (static_priority <= 99);
    }
    return static_priority == 0;
}

static long sched_setattr_call(pid_t os_thread_id, process_priority_sched_attr *attr)
{
    #if defined(SYS_sched_setattr)
    return syscall(SYS_sched_setattr, os_thread_id, attr, 0U);
    #else
    (void)os_thread_id;
    (void)attr;
    errno = ENOSYS;
    return -1;
    #endif
}

static long sched_getattr_call(pid_t os_thread_id, process_priority_sched_attr *attr, unsigned int size)
{
    #if defined(SYS_sched_getattr)
    return syscall(SYS_sched_getattr, os_thread_id, attr, size, 0U);
    #else
    (void)os_thread_id;
    (void)attr;
    (void)size;
    errno = ENOSYS;
    return -1;
    #endif
}

static int is_utilization_clamp_available(void)
{
    return access(PROCESS_PRIORITY_UTILIZATION_CLAMP_SYSCTL, F_OK) == 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_set_policy(uint32_t thread_id, cplat_process_priority_linux_policy policy,
                                            int static_priority, int reset_on_fork, cplat_error *detail_out)
{
    pid_t os_thread_id;
    int os_policy;
    struct sched_param param;

    if (!to_os_thread_id(thread_id, &os_thread_id) || !to_os_policy(policy, &os_policy) ||
        !is_valid_static_priority(policy, static_priority) || ((reset_on_fork != 0) && (reset_on_fork != 1)))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    if (reset_on_fork != 0)
    {
        os_policy |= SCHED_RESET_ON_FORK;
    }
    memset(&param, 0, sizeof(param));
    param.sched_priority = static_priority;

    if (sched_setscheduler(os_thread_id, os_policy, &param) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_get_policy(uint32_t thread_id, cplat_process_priority_linux_policy *policy_out,
                                            int *static_priority_out, int *reset_on_fork_out,
                                            cplat_error *detail_out)
{
    pid_t os_thread_id;
    int os_policy;
    struct sched_param param;
    cplat_process_priority_linux_policy policy;

    if (!to_os_thread_id(thread_id, &os_thread_id) || (policy_out == NULL) || (static_priority_out == NULL) ||
        (reset_on_fork_out == NULL))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    os_policy = sched_getscheduler(os_thread_id);
    if (os_policy == -1)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    memset(&param, 0, sizeof(param));
    if (sched_getparam(os_thread_id, &param) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    if (!from_os_policy(os_policy & ~SCHED_RESET_ON_FORK, &policy))
    {
        return cplat_internal_error_report_errno(detail_out, EOVERFLOW);
    }

    *policy_out = policy;
    *static_priority_out = param.sched_priority;
    *reset_on_fork_out = ((os_policy & SCHED_RESET_ON_FORK) != 0) ? 1 : 0;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_set_deadline(uint32_t thread_id,
                                              const cplat_process_priority_linux_deadline *deadline,
                                              int reset_on_fork, cplat_error *detail_out)
{
    pid_t os_thread_id;
    process_priority_sched_attr attr;

    if (!to_os_thread_id(thread_id, &os_thread_id) || (deadline == NULL) ||
        ((reset_on_fork != 0) && (reset_on_fork != 1)))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    memset(&attr, 0, sizeof(attr));
    attr.size = PROCESS_PRIORITY_SCHED_ATTR_SIZE_VERSION0;
    attr.sched_policy = SCHED_DEADLINE;
    attr.sched_flags = (reset_on_fork != 0) ? PROCESS_PRIORITY_SCHED_FLAG_RESET_ON_FORK : 0U;
    attr.sched_runtime = deadline->runtime_nanoseconds;
    attr.sched_deadline = deadline->deadline_nanoseconds;
    attr.sched_period = deadline->period_nanoseconds;

    if (sched_setattr_call(os_thread_id, &attr) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_get_deadline(uint32_t thread_id, cplat_process_priority_linux_deadline *deadline_out,
                                              cplat_error *detail_out)
{
    pid_t os_thread_id;
    process_priority_sched_attr attr;

    if (!to_os_thread_id(thread_id, &os_thread_id) || (deadline_out == NULL))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    memset(&attr, 0, sizeof(attr));
    if (sched_getattr_call(os_thread_id, &attr, PROCESS_PRIORITY_SCHED_ATTR_SIZE_VERSION0) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }

    /* デッド ライン スケジューリング中でない場合、カーネルは 3 つの時間に 0 を返す。 */
    deadline_out->runtime_nanoseconds = attr.sched_runtime;
    deadline_out->deadline_nanoseconds = attr.sched_deadline;
    deadline_out->period_nanoseconds = attr.sched_period;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_set_nice(uint32_t thread_id, int nice_value, cplat_error *detail_out)
{
    pid_t os_thread_id;

    /* setpriority() は範囲外の値を黙って丸めるため、範囲を先に検査する。
       see: https://man7.org/linux/man-pages/man2/setpriority.2.html */
    if (!to_os_thread_id(thread_id, &os_thread_id) || (nice_value < -20) || (nice_value > 19))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    if (setpriority(PRIO_PROCESS, (id_t)os_thread_id, nice_value) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_get_nice(uint32_t thread_id, int *nice_value_out, cplat_error *detail_out)
{
    pid_t os_thread_id;
    int nice_value;

    if (!to_os_thread_id(thread_id, &os_thread_id) || (nice_value_out == NULL))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    /* getpriority() は正当な値として -1 を返すため、errno を 0 にしてから呼び出し、
       errno の変化で失敗を判定する。
       see: https://man7.org/linux/man-pages/man2/getpriority.2.html */
    errno = 0;
    nice_value = getpriority(PRIO_PROCESS, (id_t)os_thread_id);
    if ((nice_value == -1) && (errno != 0))
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }

    *nice_value_out = nice_value;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_set_io_priority(uint32_t thread_id, cplat_process_priority_linux_io_class io_class,
                                                 int level, cplat_error *detail_out)
{
    pid_t os_thread_id;
    int valid_level;
    int ioprio;

    switch (io_class)
    {
    case CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_REAL_TIME:
    case CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_BEST_EFFORT:
        valid_level = (level >= 0) && (level <= PROCESS_PRIORITY_IOPRIO_LEVEL_MAX);
        break;
    case CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_NONE:
    case CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE:
        valid_level = (level == 0);
        break;
    default:
        valid_level = 0;
        break;
    }
    if (!to_os_thread_id(thread_id, &os_thread_id) || !valid_level)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    /* 列挙子の値は IOPRIO_CLASS_* と一致させている。 */
    ioprio = ((int)io_class << PROCESS_PRIORITY_IOPRIO_CLASS_SHIFT) | level;
    #if defined(SYS_ioprio_set)
    if (syscall(SYS_ioprio_set, PROCESS_PRIORITY_IOPRIO_WHO_PROCESS, os_thread_id, ioprio) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    #else
    (void)ioprio;
    return cplat_internal_error_report_errno(detail_out, ENOSYS);
    #endif
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_get_io_priority(uint32_t thread_id,
                                                 cplat_process_priority_linux_io_class *io_class_out, int *level_out,
                                                 cplat_error *detail_out)
{
    pid_t os_thread_id;
    long ioprio;
    long os_class;

    if (!to_os_thread_id(thread_id, &os_thread_id) || (io_class_out == NULL) || (level_out == NULL))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    #if defined(SYS_ioprio_get)
    ioprio = syscall(SYS_ioprio_get, PROCESS_PRIORITY_IOPRIO_WHO_PROCESS, os_thread_id);
    if (ioprio == -1)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }
    #else
    return cplat_internal_error_report_errno(detail_out, ENOSYS);
    #endif

    os_class = ioprio >> PROCESS_PRIORITY_IOPRIO_CLASS_SHIFT;
    if ((os_class < (long)CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_NONE) ||
        (os_class > (long)CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE))
    {
        return cplat_internal_error_report_errno(detail_out, EOVERFLOW);
    }

    /* Linux 6.5 以降はレベルの上位ビットに I/O 優先度のヒントを持つため、レベルだけを取り出す。 */
    *io_class_out = (cplat_process_priority_linux_io_class)os_class;
    *level_out = (int)(ioprio & PROCESS_PRIORITY_IOPRIO_LEVEL_MASK);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_set_utilization_clamp(uint32_t thread_id, uint32_t minimum, uint32_t maximum,
                                                       cplat_error *detail_out)
{
    pid_t os_thread_id;
    process_priority_sched_attr attr;

    if (!to_os_thread_id(thread_id, &os_thread_id) || (maximum > PROCESS_PRIORITY_UTILIZATION_SCALE) ||
        (minimum > maximum))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    /* KEEP_POLICY と KEEP_PARAMS で、ポリシーと優先度を変えずにクランプだけを変更する。 */
    memset(&attr, 0, sizeof(attr));
    attr.size = PROCESS_PRIORITY_SCHED_ATTR_SIZE_VERSION1;
    attr.sched_flags = PROCESS_PRIORITY_SCHED_FLAG_KEEP_POLICY | PROCESS_PRIORITY_SCHED_FLAG_KEEP_PARAMS |
                       PROCESS_PRIORITY_SCHED_FLAG_UTIL_CLAMP_MIN | PROCESS_PRIORITY_SCHED_FLAG_UTIL_CLAMP_MAX;
    attr.sched_util_min = minimum;
    attr.sched_util_max = maximum;

    if (sched_setattr_call(os_thread_id, &attr) != 0)
    {
        const int errno_value = errno;

        /* 2 番目の版の sched_attr を知らないカーネル (Linux 5.3 より前) は E2BIG を返す。
           E2BIG は他の用途と共有されるため、共通写像ではなくここで非対応へ変換する。
           see: https://man7.org/linux/man-pages/man2/sched_setattr.2.html */
        if (errno_value == E2BIG)
        {
            return cplat_internal_error_report_errno_as(detail_out, errno_value, CPLAT_ERR_UNSUPPORTED);
        }
        return cplat_internal_error_report_errno(detail_out, errno_value);
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_process_priority_linux_get_utilization_clamp(uint32_t thread_id, uint32_t *minimum_out,
                                                       uint32_t *maximum_out, cplat_error *detail_out)
{
    pid_t os_thread_id;
    process_priority_sched_attr attr;

    if (!to_os_thread_id(thread_id, &os_thread_id) || (minimum_out == NULL) || (maximum_out == NULL))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    if (!is_utilization_clamp_available())
    {
        return cplat_internal_error_report_errno(detail_out, EOPNOTSUPP);
    }

    memset(&attr, 0, sizeof(attr));
    if (sched_getattr_call(os_thread_id, &attr, PROCESS_PRIORITY_SCHED_ATTR_SIZE_VERSION1) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }

    /* カーネルは自身が知る版の大きさを size に返す。2 番目の版に満たなければクランプを持たない。 */
    if (attr.size < PROCESS_PRIORITY_SCHED_ATTR_SIZE_VERSION1)
    {
        return cplat_internal_error_report_errno(detail_out, EOPNOTSUPP);
    }

    *minimum_out = attr.sched_util_min;
    *maximum_out = attr.sched_util_max;
    return cplat_internal_error_report_success(detail_out);
}

#endif /* PLATFORM_LINUX */
