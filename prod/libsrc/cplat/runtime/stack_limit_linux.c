/**
 *******************************************************************************
 *  @file           stack_limit_linux.c
 *  @brief          Linux のスタック サイズの資源上限の取得と引き上げを OS API へ委譲します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/30
 *
 *  公開ヘッダーに記載した効果の範囲は、次の実装に基づきます。
 *
 *  - カーネルは、スタックを拡張するたびに現在のソフト リミットと比較する (`acct_stack_growth()`)。
 *    see: https://github.com/torvalds/linux/blob/v6.6/mm/mmap.c
 *  - x86 のカーネルは、exec 時のソフト リミット (最小 128 MiB) から、スタックとその下の配置との
 *    間隔を決定する (`mmap_base()`)。
 *    see: https://github.com/torvalds/linux/blob/v6.6/arch/x86/mm/mmap.c
 *  - glibc は、起動時のソフト リミットをスレッドの既定のスタック サイズとして保持する
 *    (`RLIM_INFINITY` の場合は `ARCH_STACK_DEFAULT_SIZE` を使用する)。
 *    see: https://sourceware.org/git/?p=glibc.git;a=blob;f=nptl/nptl-init.c;hb=glibc-2.28
 *  - カーネルは、実効 ID が変わる、またはケーパビリティを得るプログラムの exec (secureexec) で、
 *    ソフト リミットを `_STK_LIM` (8 MiB) までに制限する (`begin_new_exec()`、`cap_bprm_creds_from_file()`)。
 *    see: https://github.com/torvalds/linux/blob/v6.6/fs/exec.c
 *    see: https://github.com/torvalds/linux/blob/v6.6/security/commoncap.c
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <errno.h>
    #include <stdint.h>
    #include <stddef.h>
    #include <sys/resource.h>

    #include <cplat/base/error_internal.h>
    #include <cplat/runtime/stack_limit_linux.h>

static uint64_t to_limit_bytes(rlim_t os_limit)
{
    if (os_limit == RLIM_INFINITY)
    {
        return CPLAT_STACK_LIMIT_LINUX_UNLIMITED;
    }
    return (uint64_t)os_limit;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_stack_limit_linux_get(uint64_t *soft_limit_bytes_out, uint64_t *hard_limit_bytes_out,
                                cplat_error *detail_out)
{
    struct rlimit os_limit;

    if ((soft_limit_bytes_out == NULL) || (hard_limit_bytes_out == NULL))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    if (getrlimit(RLIMIT_STACK, &os_limit) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }

    *soft_limit_bytes_out = to_limit_bytes(os_limit.rlim_cur);
    *hard_limit_bytes_out = to_limit_bytes(os_limit.rlim_max);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_stack_limit_linux_raise_to_hard_limit(uint64_t *soft_limit_bytes_out, cplat_error *detail_out)
{
    struct rlimit os_limit;

    if (getrlimit(RLIMIT_STACK, &os_limit) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }

    if (os_limit.rlim_cur == os_limit.rlim_max)
    {
        if (soft_limit_bytes_out != NULL)
        {
            *soft_limit_bytes_out = to_limit_bytes(os_limit.rlim_cur);
        }
        (void)cplat_internal_error_report_success(detail_out);
        return CPLAT_SKIPPED;
    }

    /* ソフト リミットは、特権なしでハード リミットまで引き上げられる。
       see: https://man7.org/linux/man-pages/man2/getrlimit.2.html */
    os_limit.rlim_cur = os_limit.rlim_max;
    if (setrlimit(RLIMIT_STACK, &os_limit) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }

    if (soft_limit_bytes_out != NULL)
    {
        *soft_limit_bytes_out = to_limit_bytes(os_limit.rlim_cur);
    }
    return cplat_internal_error_report_success(detail_out);
}

#endif /* PLATFORM_LINUX */
