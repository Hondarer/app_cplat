/**
 *******************************************************************************
 *  @file           sym_loader_is_default.c
 *  @brief          cplat_sym_loader_entry が明示的既定値かどうかを返します。
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

int cplat_sym_loader_is_default(cplat_sym_loader_entry *fobj)
{
    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE) == 0)
    {
        (void)cplat_sym_loader_resolve(fobj);
    }

    if (cplat_atomic_load_i32(&fobj->resolved, CPLAT_MEMORY_ORDER_ACQUIRE) == 2)
    {
        return 1;
    }

    return 0;
}
