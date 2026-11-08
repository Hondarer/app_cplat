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

#include <cplat/runtime/sym_loader.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_dispose(cplat_sym_loader_entry *const *fobj_array, const size_t fobj_length)
{
    size_t i;
    int result = CPLAT_OK;
    if (fobj_length > 0U && fobj_array == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    for (i = 0; i < fobj_length; ++i)
    {
        if (fobj_array[i] == NULL)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }
    /* loader lock 内でも mutex を取得しない。呼び出し側が単独実行を保証する。 */
    for (i = 0; i < fobj_length; ++i)
    {
        cplat_sym_loader_entry *cache = fobj_array[i];
        if (cache->handle != NULL)
        {
#if defined(PLATFORM_LINUX)
            int failed = dlclose(cache->handle) != 0;
#elif defined(PLATFORM_WINDOWS)
            int failed = !FreeLibrary(cache->handle);
#endif
            if (failed)
            {
                if (result == CPLAT_OK)
                {
                    result = CPLAT_ERR_UNKNOWN;
                }
                continue;
            }
            cache->handle = NULL;
        }
        if (cache->lock != NULL)
        {
            cplat_local_lock_dispose(cache->lock);
            cache->lock = NULL;
        }
        cplat_atomic_store_ptr(&cache->func_ptr, NULL, CPLAT_MEMORY_ORDER_RELAXED);
        cplat_atomic_store_i32(&cache->resolved, 0, CPLAT_MEMORY_ORDER_RELEASE);
        cplat_atomic_store_i32(&cache->lock_state, 0, CPLAT_MEMORY_ORDER_RELEASE);
    }
    return result;
}
