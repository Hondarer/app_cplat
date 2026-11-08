/**
 *******************************************************************************
 *  @file           sym_loader_resolve.c
 *  @brief          拡張可能な関数の動的ロードを行い、関数アドレスを返却します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  cplat_sym_loader_resolve はスレッド セーフです。
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
/*
 * エントリ ロックの初期化完了を待機する間、CPU を他のスレッドへ譲る。
 */
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

/*
 * キャッシュ エントリの排他制御ロックを一度だけ初期化する。
 * 成功時は CPLAT_OK、失敗時はロック生成の結果コードを返す。
 */
static int ensure_entry_lock_initialized(cplat_sym_loader_entry *fobj)
{
    int32_t expected = 0;

    if (cplat_atomic_compare_exchange_i32(&fobj->lock_state, &expected, 1, CPLAT_MEMORY_ORDER_ACQ_REL))
    {
        int ret = cplat_local_lock_create(&fobj->lock);
        if (ret != CPLAT_OK)
        {
            cplat_atomic_store_i32(&fobj->lock_state, ret, CPLAT_MEMORY_ORDER_RELEASE);
            return ret;
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
        return (int)expected;
    }
}

/* resolved は 1=関数、2=既定値、負値=公開結果コード、0=未解決。 */
static int cached_result(cplat_sym_loader_entry *fobj, void **function_out)
{
    int32_t state = cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE);
    if (state < 0)
    {
        return (int)state;
    }
    *function_out = cplat_atomic_load_ptr(&fobj->func_ptr, CPLAT_MEMORY_ORDER_RELAXED);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */
int cplat_sym_loader_resolve(cplat_sym_loader_entry *fobj, void **function_out)
{
#if defined(PLATFORM_LINUX)
    const char *ext = ".so";
#elif defined(PLATFORM_WINDOWS)
    const char *ext = ".dll";
#endif
    char lib_with_ext[CPLAT_SYM_LOADER_NAME_MAX];
    int ret;
    int state;

    if (fobj == NULL || function_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE) != 0)
    {
        return cached_result(fobj, function_out);
    }
    ret = ensure_entry_lock_initialized(fobj);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = cplat_local_lock_lock(fobj->lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_RELAXED) == 0)
    {
        state = CPLAT_ERR_NOT_FOUND;
        if (memchr(fobj->lib_name, '\0', sizeof(fobj->lib_name)) == NULL ||
            memchr(fobj->func_name, '\0', sizeof(fobj->func_name)) == NULL)
        {
            state = CPLAT_ERR_OUT_OF_RANGE;
        }
        else if (strcmp(fobj->lib_name, "default") == 0 && strcmp(fobj->func_name, "default") == 0)
        {
            state = 2;
        }
        else if (fobj->lib_name[0] != '\0' && fobj->func_name[0] != '\0')
        {
            if (strlen(fobj->lib_name) + strlen(ext) >= sizeof(lib_with_ext))
            {
                state = CPLAT_ERR_OUT_OF_RANGE;
            }
            else
            {
                (void)cplat_strcpy(lib_with_ext, sizeof(lib_with_ext), fobj->lib_name);
                (void)cplat_strcat(lib_with_ext, sizeof(lib_with_ext), ext);
#if defined(PLATFORM_LINUX)
                fobj->handle = dlopen(lib_with_ext, RTLD_LAZY);
#elif defined(PLATFORM_WINDOWS)
                fobj->handle = LoadLibraryU(lib_with_ext);
#endif
                if (fobj->handle == NULL)
                {
                    state = CPLAT_ERR_UNKNOWN;
                }
                else
                {
#if defined(PLATFORM_LINUX)
                    void *ptr = dlsym(fobj->handle, fobj->func_name);
#elif defined(PLATFORM_WINDOWS)
                    void *ptr = (void *)GetProcAddress(fobj->handle, fobj->func_name);
#endif
                    cplat_atomic_store_ptr(&fobj->func_ptr, ptr, CPLAT_MEMORY_ORDER_RELAXED);
                    state = ptr == NULL ? CPLAT_ERR_NOT_FOUND : 1;
                    /* 失敗した参照も dispose まで所有し、解放失敗でハンドルを失わない。 */
                }
            }
        }
        cplat_atomic_store_i32(&fobj->resolved, state, CPLAT_MEMORY_ORDER_RELEASE);
    }
    ret = cplat_local_lock_unlock(fobj->lock);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    return cached_result(fobj, function_out);
}
