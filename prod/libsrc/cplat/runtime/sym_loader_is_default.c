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

#include "sym_loader_internal.h"

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sym_loader_is_default(cplat_sym_loader_entry *fobj, int *value_out)
{
    if ((fobj == NULL) || (value_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (cplat_sym_loader_ensure(fobj) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *value_out = (fobj->impl == NULL) ? 0 : fobj->impl->is_default;
    return CPLAT_OK;
}
