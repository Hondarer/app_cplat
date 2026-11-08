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

int cplat_sym_loader_dispose(cplat_sym_loader_entry *const *fobj_array, const size_t fobj_length)
{
    size_t fobj_index;
    int failed = 0;

    if ((fobj_length > 0u) && (fobj_array == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    for (fobj_index = 0; fobj_index < fobj_length; fobj_index++)
    {
        if (fobj_array[fobj_index] == NULL)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }

    for (fobj_index = 0; fobj_index < fobj_length; fobj_index++)
    {
        cplat_sym_loader_entry *cache = fobj_array[fobj_index];
        size_t previous;

        for (previous = 0; previous < fobj_index; previous++)
        {
            if (fobj_array[previous] == cache)
            {
                break;
            }
        }
        if (previous != fobj_index)
        {
            continue;
        }

        if ((cache->impl != NULL) &&
            (cplat_sym_loader_close_usable(cache->impl->usable, cache->impl->usable_count, 1) != 0))
        {
            cache->impl->release_pending = 1;
            cplat_atomic_store_i32(&cache->resolved, 0, CPLAT_MEMORY_ORDER_RELEASE);
            cplat_atomic_store_i32(&cache->failed, 1, CPLAT_MEMORY_ORDER_RELEASE);
            failed = 1;
            continue;
        }

        free_entry_impl(cache->impl);
        cache->impl = NULL;
        if (cache->lock != NULL)
        {
            cplat_local_lock_dispose(cache->lock);
            cache->lock = NULL;
        }
        cplat_atomic_store_i32(&cache->lock_state, 0, CPLAT_MEMORY_ORDER_RELAXED);
        cplat_atomic_store_i32(&cache->resolved, 0, CPLAT_MEMORY_ORDER_RELAXED);
        cplat_atomic_store_i32(&cache->failed, 0, CPLAT_MEMORY_ORDER_RELAXED);
    }

    return (failed != 0) ? CPLAT_ERR_UNKNOWN : CPLAT_OK;
}
