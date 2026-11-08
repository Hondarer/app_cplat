/**
 *******************************************************************************
 *  @file           sym_loader_dispose.c
 *  @brief          cplat_sym_loader_entry ポインター配列を解放します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/crt/stdlib.h>
#include <cplat/runtime/sym_loader.h>
#include <cplat/sync/atomic.h>

#include "sym_loader_internal.h"

/* 開いたモジュールを 1 回閉じます。ハンドルが NULL のときは何もしません。 */
static void close_module_handle(CPLAT_MODULE_HANDLE handle)
{
    if (handle == NULL)
    {
        return;
    }

#if defined(PLATFORM_LINUX)
    dlclose(handle);
#elif defined(PLATFORM_WINDOWS)
    FreeLibrary(handle);
#endif /* PLATFORM_ */
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
        close_module_handle(list[index].handle);
        list[index].handle = NULL;
        cplat_free(list[index].lib_name);
        cplat_free(list[index].func_name);
    }

    cplat_free(list);
}

static void free_entry_impl(cplat_sym_loader_entry_impl *impl)
{
    if (impl == NULL)
    {
        return;
    }

    free_name_pairs(impl->pending, impl->pending_count);
    free_reject_items(impl->duplicates, impl->duplicate_count);
    free_usable_items(impl->usable, impl->usable_count);
    free_reject_items(impl->rejects, impl->reject_count);
    cplat_free(impl->config_path);
    cplat_free(impl);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_sym_loader_dispose(cplat_sym_loader_entry *const *fobj_array, const size_t fobj_length)
{
    size_t fobj_index;

    /*
     * DllMain / destructor コンテキストから呼ばれるため、
     * ローダー ロック保持中にミューテックスを取得すると
     * デッドロックを引き起こす恐れがある。
     * このコンテキストではシングルスレッド動作が保証されるため、
     * ロックなしで解放する。ロック自体は破棄しない。
     */
    for (fobj_index = 0; fobj_index < fobj_length; fobj_index++)
    {
        cplat_sym_loader_entry *cache = fobj_array[fobj_index];

        free_entry_impl(cache->impl);
        cache->impl = NULL;
        cplat_atomic_store_i32(&cache->resolved, 0, CPLAT_MEMORY_ORDER_RELAXED);
        cplat_atomic_store_i32(&cache->failed, 0, CPLAT_MEMORY_ORDER_RELAXED);
    }
}
