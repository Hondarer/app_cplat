/**
 *******************************************************************************
 *  @file           hashtable_validate.c
 *  @brief          管理領域とデータ領域の整合性を検査します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
 *  @version        1.0.0
 *
 *  バケット連鎖、実装状況、件数、可変長ストレージの空きリストを走査し、
 *  破損を検出します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "hashtable.h"

#include <cplat/crt/stdlib.h>

/**
 *  @brief          チェーンと空きヒントの整合を検査します。
 *  @param[in]      ht       対象。NULL を渡してはなりません。
 *  @param[in,out]  visited  capacity バイトの作業領域。呼び出し側が 0 埋めします。
 *  @return         整合なら 0、破損なら -1 です。
 */
static int hashtable_validate_impl(const cplat_hashtable *ht, unsigned char *visited)
{
    size_t capacity = ht->hdr->config.capacity;
    uint64_t *bucket_head = hashtable_bucket_head(ht);
    size_t idx;
    size_t min_empty = 0;
    uint64_t counted_in_use = 0;
    uint64_t counted_deleted = 0;
    uint64_t counted_key_storage = 0;
    uint64_t counted_value_storage = 0;
    struct hashtable_arena key_arena;
    struct hashtable_arena value_arena;
    size_t i;

    /* 空きリストは、descriptor の検査より先に構造を確かめる。以降は昇順を前提にできる。 */
    if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
    {
        hashtable_key_arena(ht, &key_arena);
        if (hashtable_arena_validate(&key_arena, ht->hdr->key_storage_used, capacity) != 0)
        {
            return -1;
        }
    }
    if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
    {
        hashtable_value_arena(ht, &value_arena);
        if (hashtable_arena_validate(&value_arena, ht->hdr->value_storage_used, capacity) != 0)
        {
            return -1;
        }
    }

    for (idx = 0; idx < capacity; idx++)
    {
        uint64_t cur = bucket_head[idx];
        size_t hops = 0;

        if (cur > capacity)
        {
            return -1;
        }
        while (cur != 0)
        {
            size_t rec;
            char *stored_key;
            uint64_t next;

            /* capacity を超える追跡は循環。 */
            if (hops++ >= capacity)
            {
                return -1;
            }
            rec = (size_t)(cur - 1);
            /* 同一スロットが複数チェーンに載ってはなりません。 */
            if (visited[rec] != 0)
            {
                return -1;
            }
            /* 空きはチェーンから外れているはず。 */
            if (*hashtable_entry_status(ht, rec) == REC_EMPTY)
            {
                return -1;
            }
            stored_key = hashtable_entry_key(ht, rec);
            if (!hashtable_key_fits(ht, stored_key))
            {
                return -1;
            }
            /* キーのハッシュ先と、載っているバケットが一致すること。 */
            if (hashtable_hash_key(ht, stored_key) != idx)
            {
                return -1;
            }
            visited[rec] = 1;

            next = *hashtable_entry_next(ht, rec);
            if (next > capacity)
            {
                return -1;
            }
            cur = next;
        }
    }

    for (i = 0; i < capacity; i++)
    {
        unsigned char status = *hashtable_entry_status(ht, i);

        if (status == REC_EMPTY)
        {
            if (((hashtable_field_is_variable(ht->hdr->config.key_type) != 0) &&
                 (hashtable_key_ref_at(ht, i)->length != 0)) ||
                ((hashtable_field_is_variable(ht->hdr->config.value_type) != 0) &&
                 (hashtable_value_ref_at(ht, i)->length != 0)))
            {
                return -1;
            }
            /* 空スロットの世代カウンターは 0 に戻っているはず。 */
            if ((hashtable_has_record_timestamp(ht) != 0) && (*hashtable_entry_generation(ht, i) != 0))
            {
                return -1;
            }
            if (min_empty == 0)
            {
                min_empty = i + 1;
            }
        }
        else
        {
            if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
            {
                struct hashtable_string_ref *ref = hashtable_key_ref_at(ht, i);

                if ((ref->length == 0) || (ref->offset > ht->hdr->config.key_storage_size) ||
                    (ref->length > ht->hdr->config.key_storage_size - ref->offset) ||
                    (hashtable_key_storage(ht)[ref->offset + ref->length - 1u] != '\0'))
                {
                    return -1;
                }
                if (hashtable_arena_validate_used_block(&key_arena, ref->offset, ref->length) != 0)
                {
                    return -1;
                }
                counted_key_storage += ref->length;
            }
            if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
            {
                struct hashtable_string_ref *ref = hashtable_value_ref_at(ht, i);

                if ((ref->length == 0) || (ref->offset > ht->hdr->config.value_storage_size) ||
                    (ref->length > ht->hdr->config.value_storage_size - ref->offset) ||
                    (hashtable_value_storage(ht)[ref->offset + ref->length - 1u] != '\0'))
                {
                    return -1;
                }
                if (hashtable_arena_validate_used_block(&value_arena, ref->offset, ref->length) != 0)
                {
                    return -1;
                }
                counted_value_storage += ref->length;
            }
            else if ((ht->hdr->config.value_type == CPLAT_HASHTABLE_FIELD_FIXED_STRING) &&
                     (hashtable_fixed_string_fits(ht->hdr->config.value_type, ht->hdr->config.value_size,
                                                  hashtable_data_at(ht, i)) == 0))
            {
                return -1;
            }
            if (visited[i] == 0)
            {
                /* 実装中または削除済みなのに、どのバケットからも辿れない。 */
                return -1;
            }
            /* レコードの世代は、テーブルの世代を追い越しません。 */
            if ((hashtable_has_record_timestamp(ht) != 0) &&
                (*hashtable_entry_generation(ht, i) > ht->hdr->table_generation))
            {
                return -1;
            }
            if (status == REC_IN_USE)
            {
                counted_in_use++;
            }
            else
            {
                /* 2 以上は加齢中の削除済み。255 も含める。 */
                counted_deleted++;
            }
        }
    }

    if (ht->hdr->next_empty != min_empty)
    {
        return -1;
    }
    if (counted_in_use != ht->hdr->in_use_count)
    {
        return -1;
    }
    if (counted_deleted != ht->hdr->deleted_count)
    {
        return -1;
    }
    if ((counted_key_storage != ht->hdr->key_storage_used) || (counted_value_storage != ht->hdr->value_storage_used))
    {
        return -1;
    }
    for (i = 0; i < capacity; i++)
    {
        size_t j;

        if (*hashtable_entry_status(ht, i) == REC_EMPTY)
        {
            continue;
        }
        for (j = i + 1u; j < capacity; j++)
        {
            if (*hashtable_entry_status(ht, j) == REC_EMPTY)
            {
                continue;
            }
            if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
            {
                struct hashtable_string_ref *a = hashtable_key_ref_at(ht, i);
                struct hashtable_string_ref *b = hashtable_key_ref_at(ht, j);

                if ((a->offset < b->offset + b->length) && (b->offset < a->offset + a->length))
                {
                    return -1;
                }
            }
            if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
            {
                struct hashtable_string_ref *a = hashtable_value_ref_at(ht, i);
                struct hashtable_string_ref *b = hashtable_value_ref_at(ht, j);

                if ((a->offset < b->offset + b->length) && (b->offset < a->offset + a->length))
                {
                    return -1;
                }
            }
        }
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_validate(const cplat_hashtable *ht)
{
    unsigned char *visited;
    int result;

    if (ht == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    visited = (unsigned char *)cplat_calloc(ht->hdr->config.capacity, 1);
    if (visited == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    result = hashtable_validate_impl(ht, visited);
    cplat_free(visited);
    if (result == 0)
    {
        return CPLAT_OK;
    }
    else
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
}
