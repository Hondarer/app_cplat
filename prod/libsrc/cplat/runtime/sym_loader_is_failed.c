/**
 *******************************************************************************
 *  @file           sym_loader_is_failed.c
 *  @brief          cplat_sym_loader_entry の内部失敗が有効かを返します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/11/08
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/runtime/sym_loader.h>
#include <cplat/sync/atomic.h>

#include "sym_loader_internal.h"

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_is_failed(cplat_sym_loader_entry *fobj, int *value_out)
{
    if ((fobj == NULL) || (value_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /*
     * すでに有効な失敗は解消しない。
     * failed を acquire で読み、0 のときだけ未解決の解決を試みる。
     */
    if (cplat_atomic_load_i32(&fobj->failed, CPLAT_MEMORY_ORDER_ACQUIRE) == 1)
    {
        *value_out = 1;
        return CPLAT_OK;
    }

    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE) == 1)
    {
        *value_out = 0;
        return CPLAT_OK;
    }

    if (cplat_sym_loader_ensure(fobj) != 0)
    {
        *value_out = 1;
        return CPLAT_OK;
    }

    *value_out = 0;
    return CPLAT_OK;
}
