/**
 *******************************************************************************
 *  @file           sym_loader_info.c
 *  @brief          cplat_sym_loader_entry ポインター配列の内容を標準出力に表示します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  各エントリを表示し、未解決のエントリがあれば解決を試みます。\n
 *  1 つでも解決失敗した場合は @ref CPLAT_ERR_UNKNOWN を返します。\n
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/runtime/sym_loader.h>
#include <stdio.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_info(cplat_sym_loader_entry *const *fobj_array, const size_t fobj_length)
{
    int result = CPLAT_OK;
    size_t fobj_index;

    if (fobj_length > 0 && fobj_array == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 配列要素に NULL が含まれていないか検証する。 */
    for (fobj_index = 0; fobj_index < fobj_length; fobj_index++)
    {
        if (fobj_array[fobj_index] == NULL)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }

    /* 各エントリの情報を標準出力へ出力し、未解決のエントリがあれば解決を試みる。 */
    for (fobj_index = 0; fobj_index < fobj_length; fobj_index++)
    {
        cplat_sym_loader_entry *fobj = fobj_array[fobj_index];
        int32_t resolved;

        if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE) == 0)
        {
            (void)cplat_sym_loader_resolve(fobj);
        }

        resolved = cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE);
        printf("- [%zu] %s\n", fobj_index, fobj->func_key);
        printf("    - resolved : %d\n", (int)resolved);
        printf("    - lib_name : %s\n", fobj->lib_name);
        printf("    - func_name: %s\n", fobj->func_name);
        printf("    - handle   : %p\n", (void *)fobj->handle);
        printf("    - func_ptr : %p\n", cplat_atomic_load_ptr(&fobj->func_ptr, CPLAT_MEMORY_ORDER_RELAXED));

        /* 1 つでも解決に失敗したエントリがある場合は、最終結果をエラーとする。 */
        if (resolved < 0)
        {
            result = CPLAT_ERR_UNKNOWN;
        }
    }

    return result;
}
