/**
 *******************************************************************************
 *  @file           string_catalog_argument.c
 *  @brief          引数スキーマに従って可変長引数を値の配列へ取り出します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  可変長引数は先頭から順次安全に取り出す必要があり、`va_arg` の型指定が実引数の型 (既定引数拡張後)
 *  と適合しない場合は未定義動作になります。\n
 *  そのため、引数種別ごとの取り出し型を本ファイル内に集約し、
 *  他の実装モジュールは値の配列のみを取り扱います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "string_catalog.h"

#include <cplat/base/result.h>

#include <stdarg.h>
#include <stdint.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_string_catalog_collect_arguments(const cplat_string_catalog_entry *entry, va_list args,
                                                    cplat_internal_string_catalog_argument_value *values)
{
    int index;

    if ((entry == NULL) || (values == NULL) || (entry->argument_count < 0) ||
        (entry->argument_count > CPLAT_STRING_CATALOG_ARGUMENT_MAX) ||
        ((entry->argument_count > 0) && (entry->arguments == NULL)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 各引数定義の種別に応じた型で、可変長引数から値を順に取り出す。 */
    for (index = 0; index < entry->argument_count; index++)
    {
        const cplat_string_catalog_argument_kind kind = entry->arguments[index].kind;

        values[index].kind = kind;
        values[index].pad = 0U;

        switch (kind)
        {
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED:
            /* 引数を割り当てないインデックスのため、可変長引数から値を取り出さない。 */
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING:
            values[index].value.string_value = va_arg(args, const char *);
            break;

        /* 昇格後の int として取り出し、種別に応じた型幅へ変換する。 */
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR:
            values[index].value.char_value = (char)va_arg(args, int);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8:
            values[index].value.int8_value = (int8_t)va_arg(args, int);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX8:
            values[index].value.uint8_value = (uint8_t)va_arg(args, int);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16:
            values[index].value.int16_value = (int16_t)va_arg(args, int);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX16:
            values[index].value.uint16_value = (uint16_t)va_arg(args, int);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32:
            values[index].value.int32_value = va_arg(args, int32_t);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX32:
            values[index].value.uint32_value = va_arg(args, uint32_t);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE:
            values[index].value.int64_value = va_arg(args, int64_t);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64:
            values[index].value.uint64_value = va_arg(args, uint64_t);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE:
            values[index].value.size_value = va_arg(args, size_t);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER:
            values[index].value.pointer_value = va_arg(args, const void *);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE:
            values[index].value.double_value = va_arg(args, double);
            break;

        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE:
            values[index].value.error_code_value = va_arg(args, int);
            break;

        default:
            /* 未知の種別では取り出す型を特定できないため、以降の引数の走査も中止する。 */
            return CPLAT_ERR_MALFORMED_DEFINITION;
        }
    }

    return CPLAT_OK;
}
