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

void cplat_sym_loader_dispose(cplat_sym_loader_entry *const *fobj_array, const size_t fobj_length)
{
    size_t fobj_index;

    /*
     * DllMain / destructor コンテキストから呼ばれるため、
     * ローダー ロック保持中にミューテックスを取得すると
     * デッドロックを引き起こす恐れがある。
     * このコンテキストではシングルスレッド動作が保証されるため、
     * ロックなしで解放する。
     */
    for (fobj_index = 0; fobj_index < fobj_length; fobj_index++)
    {
        cplat_sym_loader_entry *cache = fobj_array[fobj_index];

        if (cache->handle == NULL)
        {
            continue;
        }

#if defined(PLATFORM_LINUX)
        dlclose(cache->handle);
#elif defined(PLATFORM_WINDOWS)
        FreeLibrary(cache->handle);
#endif /* PLATFORM_ */

        cache->handle = NULL;
        cplat_atomic_store_ptr(&cache->func_ptr, NULL, CPLAT_MEMORY_ORDER_RELAXED);
    }
}
