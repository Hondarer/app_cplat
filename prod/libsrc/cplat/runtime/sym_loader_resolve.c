/**
 *******************************************************************************
 *  @file           sym_loader_resolve.c
 *  @brief          拡張可能な関数の動的ロードを行い、関数アドレスを返却します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  cplat_sym_loader_resolve_as はスレッド セーフです。
 *  内部で mutex (Linux) または SRW ロック (Windows) を使用して排他制御します。
 *
 *  @par            double-checked locking とメモリ順序
 *  fast path (ロックなし) の resolved 読み取りには acquire セマンティクスを使用します。
 *  これにより、weakly-ordered アーキテクチャー (ARM64 等) においても
 *  resolved != 0 が見えた時点で func_ptr の書き込みも可視であることが保証されます。\n
 *  アトミック操作は cplat/sync/atomic.h の API で行い、プラットフォームごとに書き分けません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/runtime/sym_loader.h>
#include <cplat/sync/atomic.h>
#include <cplat/crt/string.h>
#if defined(PLATFORM_LINUX)
    #include <sched.h>
#endif /* PLATFORM_LINUX */
#if defined(PLATFORM_WINDOWS)
    #include <cplat/win32/win32.h>
#endif /* PLATFORM_WINDOWS */
#include <string.h>

#if defined(PLATFORM_LINUX)
static void wait_for_entry_lock_initialization(cplat_sym_loader_entry *fobj)
{
    (void)fobj;
    sched_yield();
}

static void (*s_entry_lock_wait_hook)(cplat_sym_loader_entry *fobj) = wait_for_entry_lock_initialization;
#endif /* PLATFORM_LINUX */

/** ほかのスレッドがロックを初期化している間、実行権を譲ります。 */
static void yield_while_entry_lock_initializing(cplat_sym_loader_entry *fobj)
{
#if defined(PLATFORM_LINUX)
    s_entry_lock_wait_hook(fobj);
#elif defined(PLATFORM_WINDOWS)
    (void)fobj;
    (void)SwitchToThread();
#endif /* PLATFORM_ */
}

static int ensure_entry_lock_initialized(cplat_sym_loader_entry *fobj)
{
    int32_t expected = 0;

    if (cplat_atomic_compare_exchange_i32(&fobj->lock_state, &expected, 1, CPLAT_MEMORY_ORDER_ACQ_REL))
    {
        if (cplat_local_lock_create(&fobj->lock) != CPLAT_OK)
        {
            cplat_atomic_store_i32(&fobj->lock_state, -1, CPLAT_MEMORY_ORDER_RELEASE);
            return -1;
        }
        cplat_atomic_store_i32(&fobj->lock_state, 2, CPLAT_MEMORY_ORDER_RELEASE);
        return 0;
    }

    while ((expected = cplat_atomic_load_i32(&fobj->lock_state, CPLAT_MEMORY_ORDER_ACQUIRE)) == 1)
    {
        yield_while_entry_lock_initializing(fobj);
    }

    if (expected == 2)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

static void *unlock_entry_and_return_func_ptr(cplat_sym_loader_entry *fobj)
{
    cplat_local_lock_unlock(fobj->lock);
    return cplat_atomic_load_ptr(&fobj->func_ptr, CPLAT_MEMORY_ORDER_RELAXED);
}

/* Doxygen コメントは、ヘッダーに記載 */

void *cplat_sym_loader_resolve(cplat_sym_loader_entry *fobj)
{
#if defined(PLATFORM_LINUX)
    const char *ext = ".so";
#elif defined(PLATFORM_WINDOWS)
    const char *ext = ".dll";
#endif /* PLATFORM_ */
    char lib_with_ext[CPLAT_SYM_LOADER_NAME_MAX];

    /* ロード完了後は resolved が 0 以外になる。早期リターンで判定する。
     * resolved の acquire 読み取りは、func_ptr の書き込みのあとに行う resolved の release 書き込みと対になる。
     * resolved が 0 以外に見えた時点で、func_ptr の書き込みも見える。 */
    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE) != 0)
    {
        return cplat_atomic_load_ptr(&fobj->func_ptr, CPLAT_MEMORY_ORDER_RELAXED);
    }

    if (ensure_entry_lock_initialized(fobj) != 0)
    {
        return NULL;
    }

    /* ロード処理を排他制御する。
     * ロック取得後に再度 resolved を確認し、他スレッドが先にロードを
     * 完了していた場合は処理をスキップする (double-checked locking)。
     * ロックの下の読み取りは、ロックが順序を保証するため RELAXED でよい。 */
    if (cplat_local_lock_lock(fobj->lock, CPLAT_SYNC_WAIT_FOREVER) != CPLAT_OK)
    {
        return NULL;
    }

    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_RELAXED) == 0)
    {
        if (strcmp(fobj->lib_name, "default") == 0 && strcmp(fobj->func_name, "default") == 0)
        {
            /* resolved=2: 明示的既定値。func_ptr は NULL のまま */
            cplat_atomic_store_i32(&fobj->resolved, 2, CPLAT_MEMORY_ORDER_RELEASE);
            return unlock_entry_and_return_func_ptr(fobj);
        }
        if (fobj->lib_name[0] == '\0' || fobj->func_name[0] == '\0')
        {
            /* resolved=-1: 定義なし (定義ファイル不存在、定義行が不存在) */
            cplat_atomic_store_i32(&fobj->resolved, -1, CPLAT_MEMORY_ORDER_RELEASE);
            return unlock_entry_and_return_func_ptr(fobj);
        }
        if (strlen(fobj->lib_name) + strlen(ext) >= CPLAT_SYM_LOADER_NAME_MAX)
        {
            /* resolved=-2: 名称長さオーバー */
            cplat_atomic_store_i32(&fobj->resolved, -2, CPLAT_MEMORY_ORDER_RELEASE);
            return unlock_entry_and_return_func_ptr(fobj);
        }
        (void)cplat_strcpy(lib_with_ext, sizeof(lib_with_ext), fobj->lib_name);
        (void)cplat_strcat(lib_with_ext, sizeof(lib_with_ext), ext);

#if defined(PLATFORM_LINUX)
        fobj->handle = dlopen(lib_with_ext, RTLD_LAZY);
#elif defined(PLATFORM_WINDOWS)
        fobj->handle = LoadLibraryU(lib_with_ext);
#endif /* PLATFORM_ */
        if (fobj->handle == NULL)
        {
            /* resolved=-3: ライブラリ オープン エラー */
            cplat_atomic_store_i32(&fobj->resolved, -3, CPLAT_MEMORY_ORDER_RELEASE);
            return unlock_entry_and_return_func_ptr(fobj);
        }

        /* func_ptr を書き込んでから resolved を release で書き込む。
         * 高速経路の acquire 読み取りと対になり、func_ptr の可視性を保証する。 */
        {
#if defined(PLATFORM_LINUX)
            void *ptr = dlsym(fobj->handle, fobj->func_name);
#elif defined(PLATFORM_WINDOWS)
            void *ptr = (void *)GetProcAddress(fobj->handle, fobj->func_name);
#endif /* PLATFORM_ */

            cplat_atomic_store_ptr(&fobj->func_ptr, ptr, CPLAT_MEMORY_ORDER_RELAXED);
            if (ptr == NULL)
            {
#if defined(PLATFORM_LINUX)
                dlclose(fobj->handle);
#elif defined(PLATFORM_WINDOWS)
                FreeLibrary(fobj->handle);
#endif /* PLATFORM_ */
                fobj->handle = NULL;
            }
        }

        /* resolved=1: 解決済 */
        cplat_atomic_store_i32(&fobj->resolved, 1, CPLAT_MEMORY_ORDER_RELEASE);
    }

    return unlock_entry_and_return_func_ptr(fobj);
}
