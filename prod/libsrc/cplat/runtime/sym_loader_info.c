/**
 *******************************************************************************
 *  @file           sym_loader_info.c
 *  @brief          cplat_sym_loader_entry の利用側の列を標準出力へ表示します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  各エントリの利用側の列を表示します。診断へ外した先は表示しません。\n
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/runtime/sym_loader.h>
#include <stdio.h>

#include "sym_loader_internal.h"

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_info(cplat_sym_loader_entry *const *fobj_array, const size_t fobj_length)
{
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

    for (fobj_index = 0; fobj_index < fobj_length; fobj_index++)
    {
        cplat_sym_loader_entry *fobj = fobj_array[fobj_index];
        int is_default = 0;
        size_t count = 0;
        size_t target_index;
        int ensured;

        ensured = cplat_sym_loader_ensure(fobj);
        if ((ensured == 0) && (fobj->impl != NULL))
        {
            is_default = fobj->impl->is_default;
            count = fobj->impl->usable_count;
        }

        printf("- [%zu] %s\n", fobj_index, fobj->func_key);
        printf("    - is_default : %d\n", is_default);
        printf("    - count : %zu\n", count);

        if ((ensured != 0) || (fobj->impl == NULL))
        {
            continue;
        }

        for (target_index = 0; target_index < fobj->impl->usable_count; target_index++)
        {
            const cplat_sym_loader_usable *target = &fobj->impl->usable[target_index];

            printf("    - [%zu] lib=%s func=%s ptr=%p\n", target_index, target->lib_name, target->func_name,
                   target->func_ptr);
        }
    }

    return CPLAT_OK;
}
