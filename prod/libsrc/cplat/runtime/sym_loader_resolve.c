/**
 *******************************************************************************
 *  @file           sym_loader_resolve.c
 *  @brief          移譲先の列を解決し、件数と添字で関数ポインターを返します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  件数、取得、診断の取得はスレッド セーフです。
 *  内部で mutex (Linux) または SRW ロック (Windows) を使用して排他制御します。
 *
 *  @par            double-checked locking とメモリ順序
 *  fast path (ロックなし) の resolved 読み取りには acquire セマンティクスを使用します。
 *  これにより、weakly-ordered アーキテクチャー (ARM64 等) においても
 *  resolved が 1 に見えた時点で、利用側の列と診断の列の書き込みも可視です。\n
 *  アトミック操作は cplat/sync/atomic.h の API で行い、プラットフォームごとに書き分けません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/sym_loader.h>
#include <cplat/sync/atomic.h>

#if defined(PLATFORM_LINUX)
    #include <sched.h>
#endif /* PLATFORM_LINUX */
#if defined(PLATFORM_WINDOWS)
    #include <cplat/win32/win32.h>
#endif /* PLATFORM_WINDOWS */

#include <stdint.h>
#include <string.h>

#include "sym_loader_internal.h"

/** 設定名として解決できる終端を除く最大バイト数です。CPLAT_SYM_LOADER_NAME_MAX とは別です。 */
#define CONFIGURED_NAME_LIMIT 255u

#if defined(PLATFORM_LINUX)
static const char k_module_ext[] = ".so";
#elif defined(PLATFORM_WINDOWS)
static const char k_module_ext[] = ".dll";
#endif /* PLATFORM_ */

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
 * 成功時は 0、初期化に失敗した場合は -1 を返す。
 * 生成に失敗したときは lock_state を 0 に戻し、次の取得が再試行できるようにする。
 */
static int ensure_entry_lock_initialized(cplat_sym_loader_entry *fobj)
{
    int32_t expected = 0;

    if (cplat_atomic_compare_exchange_i32(&fobj->lock_state, &expected, 1, CPLAT_MEMORY_ORDER_ACQ_REL))
    {
        if (cplat_local_lock_create(&fobj->lock) != CPLAT_OK)
        {
            cplat_atomic_store_i32(&fobj->lock_state, 0, CPLAT_MEMORY_ORDER_RELEASE);
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

    return -1;
}

static void free_name_pairs(cplat_sym_loader_name_pair *list, size_t count)
{
    size_t index;

    if (list == NULL)
    {
        return;
    }

    for (index = 0; index < count; index++)
    {
        cplat_free(list[index].lib_name);
        cplat_free(list[index].func_name);
    }

    cplat_free(list);
}

static void free_reject_items(cplat_sym_loader_reject_item *list, size_t count)
{
    size_t index;

    if (list == NULL)
    {
        return;
    }

    for (index = 0; index < count; index++)
    {
        cplat_free(list[index].lib_name);
        cplat_free(list[index].func_name);
    }

    cplat_free(list);
}

static void free_usable_items(cplat_sym_loader_usable *list, size_t count)
{
    size_t index;

    if (list == NULL)
    {
        return;
    }

    for (index = 0; index < count; index++)
    {
        cplat_free(list[index].lib_name);
        cplat_free(list[index].func_name);
    }

    cplat_free(list);
}

static int append_reject_copy(cplat_sym_loader_reject_item **list, size_t *count, const char *lib_name,
                              const char *func_name, int reason)
{
    cplat_sym_loader_reject_item *grown;
    char *lib_copy;
    char *func_copy;

    if (*count == SIZE_MAX)
    {
        return -1;
    }

    lib_copy = cplat_strdup(lib_name);
    if (lib_copy == NULL)
    {
        return -1;
    }

    func_copy = cplat_strdup(func_name);
    if (func_copy == NULL)
    {
        cplat_free(lib_copy);
        return -1;
    }

    grown = (cplat_sym_loader_reject_item *)cplat_realloc(*list, *count + 1u, sizeof(*grown));
    if (grown == NULL)
    {
        cplat_free(lib_copy);
        cplat_free(func_copy);
        return -1;
    }

    *list = grown;
    grown[*count].lib_name = lib_copy;
    grown[*count].func_name = func_copy;
    grown[*count].reason = reason;
    *count += 1u;
    return 0;
}

static int names_are_both_default(const char *lib_name, const char *func_name)
{
    if ((lib_name == NULL) || (func_name == NULL))
    {
        return 0;
    }

    return (strcmp(lib_name, "default") == 0) && (strcmp(func_name, "default") == 0);
}

static int pending_has_both_default(const cplat_sym_loader_entry_impl *impl)
{
    size_t index;

    for (index = 0; index < impl->pending_count; index++)
    {
        if (names_are_both_default(impl->pending[index].lib_name, impl->pending[index].func_name) != 0)
        {
            return 1;
        }
    }

    return 0;
}

/* 開けたときは slot を埋め、reason を 0 にします。診断のときは reason を負の値にします。 */
static int open_configured_symbol(const char *lib_name, const char *func_name, int *reason,
                                  cplat_sym_loader_usable *slot)
{
    char lib_with_ext[CONFIGURED_NAME_LIMIT + (sizeof(k_module_ext) - 1u) + 1u];
    CPLAT_MODULE_HANDLE handle;
    void *func_ptr;
    char *lib_copy;
    char *func_copy;

    if (cplat_strcpy(lib_with_ext, sizeof(lib_with_ext), lib_name) != CPLAT_OK)
    {
        return -1;
    }

    if (cplat_strcat(lib_with_ext, sizeof(lib_with_ext), k_module_ext) != CPLAT_OK)
    {
        return -1;
    }

#if defined(PLATFORM_LINUX)
    handle = dlopen(lib_with_ext, RTLD_LAZY);
#elif defined(PLATFORM_WINDOWS)
    handle = LoadLibraryU(lib_with_ext);
#endif /* PLATFORM_ */
    if (handle == NULL)
    {
        *reason = CPLAT_SYM_LOADER_REJECT_OPEN_FAILED;
        return 0;
    }

    slot->handle = handle;
#if defined(PLATFORM_LINUX)
    func_ptr = dlsym(handle, func_name);
#elif defined(PLATFORM_WINDOWS)
    func_ptr = (void *)GetProcAddress(handle, func_name);
#endif /* PLATFORM_ */
    if (func_ptr == NULL)
    {
        if (cplat_sym_loader_close_usable(slot, 1u, 1) != 0)
        {
            return -1;
        }
        *reason = CPLAT_SYM_LOADER_REJECT_SYMBOL_NOT_FOUND;
        return 0;
    }

    lib_copy = cplat_strdup(lib_name);
    if (lib_copy == NULL)
    {
        return -1;
    }

    func_copy = cplat_strdup(func_name);
    if (func_copy == NULL)
    {
        cplat_free(lib_copy);
        return -1;
    }

    slot->lib_name = lib_copy;
    slot->func_name = func_copy;
    slot->handle = handle;
    slot->func_ptr = func_ptr;
    *reason = 0;
    return 0;
}

static int classify_one(const char *lib_name, const char *func_name, int *reason, cplat_sym_loader_usable *slot)
{
    int lib_is_default;
    int func_is_default;
    size_t lib_len;
    size_t func_len;

    if ((lib_name == NULL) || (func_name == NULL))
    {
        return -1;
    }

    lib_is_default = (strcmp(lib_name, "default") == 0);
    func_is_default = (strcmp(func_name, "default") == 0);
    if (lib_is_default != func_is_default)
    {
        *reason = CPLAT_SYM_LOADER_REJECT_PARTIAL_DEFAULT;
        return 0;
    }

    lib_len = strlen(lib_name);
    func_len = strlen(func_name);
    if ((lib_len == 0u) || (func_len == 0u))
    {
        *reason = CPLAT_SYM_LOADER_REJECT_EMPTY_NAME;
        return 0;
    }

    if ((lib_len > CONFIGURED_NAME_LIMIT) || (func_len > CONFIGURED_NAME_LIMIT))
    {
        *reason = CPLAT_SYM_LOADER_REJECT_NAME_TOO_LONG;
        return 0;
    }

    return open_configured_symbol(lib_name, func_name, reason, slot);
}

static void publish_resolved(cplat_sym_loader_entry *fobj, cplat_sym_loader_usable *usable, size_t usable_count,
                             cplat_sym_loader_reject_item *rejects, size_t reject_count, int is_default)
{
    cplat_sym_loader_entry_impl *impl = fobj->impl;

    free_name_pairs(impl->pending, impl->pending_count);
    impl->pending = NULL;
    impl->pending_count = 0;
    free_reject_items(impl->duplicates, impl->duplicate_count);
    impl->duplicates = NULL;
    impl->duplicate_count = 0;

    if ((impl->usable != NULL) && (impl->usable != usable))
    {
        free_usable_items(impl->usable, impl->usable_count);
    }

    if ((impl->rejects != NULL) && (impl->rejects != rejects))
    {
        free_reject_items(impl->rejects, impl->reject_count);
    }

    impl->usable = usable;
    impl->usable_count = usable_count;
    impl->rejects = rejects;
    impl->reject_count = reject_count;
    impl->is_default = is_default;
    impl->needs_reload = 0;
    cplat_atomic_store_i32(&fobj->failed, 0, CPLAT_MEMORY_ORDER_RELEASE);
    cplat_atomic_store_i32(&fobj->resolved, 1, CPLAT_MEMORY_ORDER_RELEASE);
}

static int classify_pending(cplat_sym_loader_entry *fobj)
{
    cplat_sym_loader_entry_impl *impl = fobj->impl;
    cplat_sym_loader_usable *usable = NULL;
    cplat_sym_loader_reject_item *rejects = NULL;
    size_t usable_count = 0;
    size_t reject_count = 0;
    size_t index;
    int is_default = 0;
    int failed = 0;

    for (index = 0; (failed == 0) && (index < impl->duplicate_count); index++)
    {
        if (append_reject_copy(&rejects, &reject_count, impl->duplicates[index].lib_name,
                               impl->duplicates[index].func_name, impl->duplicates[index].reason) != 0)
        {
            failed = 1;
        }
    }

    if ((failed == 0) && (impl->pending_count >= 2u) && (pending_has_both_default(impl) != 0))
    {
        for (index = 0; (failed == 0) && (index < impl->pending_count); index++)
        {
            if (append_reject_copy(&rejects, &reject_count, impl->pending[index].lib_name,
                                   impl->pending[index].func_name, CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT) != 0)
            {
                failed = 1;
            }
        }
    }
    else if ((failed == 0) && (impl->pending_count == 1u) &&
             (names_are_both_default(impl->pending[0].lib_name, impl->pending[0].func_name) != 0))
    {
        is_default = 1;
    }
    else if ((failed == 0) && (impl->pending_count > 0u))
    {
        usable = (cplat_sym_loader_usable *)cplat_calloc(impl->pending_count, sizeof(*usable));
        if (usable == NULL)
        {
            failed = 1;
        }
        for (index = 0; (failed == 0) && (index < impl->pending_count); index++)
        {
            int reason = 0;

            if (classify_one(impl->pending[index].lib_name, impl->pending[index].func_name, &reason,
                             &usable[usable_count]) != 0)
            {
                failed = 1;
            }
            else if (reason == 0)
            {
                usable_count++;
            }
            else if (append_reject_copy(&rejects, &reject_count, impl->pending[index].lib_name,
                                        impl->pending[index].func_name, reason) != 0)
            {
                failed = 1;
            }
        }
    }

    if (failed != 0)
    {
        size_t allocated_count = (usable != NULL) ? impl->pending_count : 0u;

        if (cplat_sym_loader_close_usable(usable, allocated_count, 0) != 0)
        {
            impl->usable = usable;
            impl->usable_count = allocated_count;
            impl->release_pending = 1;
        }
        else
        {
            free_usable_items(usable, allocated_count);
        }
        free_reject_items(rejects, reject_count);
        return -1;
    }

    publish_resolved(fobj, usable, usable_count, rejects, reject_count, is_default);
    return 0;
}

static void fail_locked(cplat_sym_loader_entry *fobj)
{
    cplat_atomic_store_i32(&fobj->resolved, 0, CPLAT_MEMORY_ORDER_RELEASE);
    cplat_atomic_store_i32(&fobj->failed, 1, CPLAT_MEMORY_ORDER_RELEASE);
}

int cplat_sym_loader_ensure(cplat_sym_loader_entry *fobj)
{
    int result;
    int32_t was_failed;
    cplat_sym_loader_entry_impl *impl;

    /*
     * resolved の acquire 読み取りは、列を公開したあとの release 書き込みと対になる。
     * 1 に見えた時点で、利用側の列と診断の列も見える。
     */
    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE) == 1)
    {
        return 0;
    }

    if (ensure_entry_lock_initialized(fobj) != 0)
    {
        fail_locked(fobj);
        return -1;
    }

    if (cplat_local_lock_lock(fobj->lock, CPLAT_SYNC_WAIT_FOREVER) != CPLAT_OK)
    {
        fail_locked(fobj);
        return -1;
    }

    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_RELAXED) == 1)
    {
        result = 0;
        goto unlock;
    }

    was_failed = cplat_atomic_load_i32(&fobj->failed, CPLAT_MEMORY_ORDER_RELAXED);
    impl = fobj->impl;
    if ((impl != NULL) && (impl->release_pending != 0))
    {
        fail_locked(fobj);
        result = -1;
        goto unlock;
    }
    cplat_atomic_store_i32(&fobj->failed, 0, CPLAT_MEMORY_ORDER_RELAXED);

    if (impl == NULL)
    {
        if (was_failed != 0)
        {
            fail_locked(fobj);
            result = -1;
            goto unlock;
        }

        cplat_atomic_store_i32(&fobj->failed, 0, CPLAT_MEMORY_ORDER_RELEASE);
        cplat_atomic_store_i32(&fobj->resolved, 1, CPLAT_MEMORY_ORDER_RELEASE);
        result = 0;
        goto unlock;
    }

    if (impl->needs_reload != 0)
    {
        char *path_copy;
        int apply_result;
        cplat_sym_loader_entry *reload_entries[1];

        if (impl->config_path == NULL)
        {
            fail_locked(fobj);
            result = -1;
            goto unlock;
        }

        /* apply が管理構造体を解放しても、パスのコピーは残す。 */
        path_copy = cplat_strdup(impl->config_path);
        if (path_copy == NULL)
        {
            fail_locked(fobj);
            result = -1;
            goto unlock;
        }

        reload_entries[0] = fobj;
        apply_result = cplat_sym_loader_apply_config(reload_entries, 1u, path_copy);
        cplat_free(path_copy);
        impl = fobj->impl;

        if ((apply_result != 0) || (impl == NULL) || (impl->needs_reload != 0) ||
            (cplat_atomic_load_i32(&fobj->failed, CPLAT_MEMORY_ORDER_RELAXED) != 0))
        {
            fail_locked(fobj);
            result = -1;
            goto unlock;
        }
    }

    if (classify_pending(fobj) != 0)
    {
        fail_locked(fobj);
        result = -1;
        goto unlock;
    }

    result = 0;

unlock:
    cplat_local_lock_unlock(fobj->lock);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_count(cplat_sym_loader_entry *fobj, size_t *count_out)
{
    if ((fobj == NULL) || (count_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (cplat_sym_loader_ensure(fobj) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *count_out = (fobj->impl == NULL) ? 0u : fobj->impl->usable_count;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_get(cplat_sym_loader_entry *fobj, size_t index, void **address_out)
{
    if ((fobj == NULL) || (address_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (cplat_sym_loader_ensure(fobj) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    if ((fobj->impl == NULL) || (index >= fobj->impl->usable_count))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *address_out = fobj->impl->usable[index].func_ptr;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_reject_count(cplat_sym_loader_entry *fobj, size_t *count_out)
{
    if ((fobj == NULL) || (count_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (cplat_sym_loader_ensure(fobj) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *count_out = (fobj->impl == NULL) ? 0u : fobj->impl->reject_count;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_reject_get(cplat_sym_loader_entry *fobj, size_t index, const char **lib_name,
                                const char **func_name, int *reason)
{
    const cplat_sym_loader_reject_item *item;

    if ((fobj == NULL) || (lib_name == NULL) || (func_name == NULL) || (reason == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_sym_loader_ensure(fobj) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    if ((fobj->impl == NULL) || (index >= fobj->impl->reject_count))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    item = &fobj->impl->rejects[index];
    *lib_name = item->lib_name;
    *func_name = item->func_name;
    *reason = item->reason;
    return CPLAT_OK;
}
