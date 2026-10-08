/**
 *******************************************************************************
 *  @file           hashtable_arena.c
 *  @brief          可変長キーと可変長値のストレージを管理します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
 *  @version        1.0.0
 *
 *  空きリストの確保、解放、結合、整理と、descriptor の読み書きを担います。\n
 *  方式の解説は hashtable-storage-allocator.md を参照してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "hashtable.h"

#include <string.h>

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_key_arena(const cplat_hashtable *ht, struct hashtable_arena *arena)
{
    arena->storage = hashtable_key_storage(ht);
    arena->free_list = hashtable_key_free_list(ht);
    arena->free_count = &ht->hdr->key_free_count;
    arena->storage_size = ht->hdr->config.key_storage_size;
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_value_arena(const cplat_hashtable *ht, struct hashtable_arena *arena)
{
    arena->storage = hashtable_value_storage(ht);
    arena->free_list = hashtable_value_free_list(ht);
    arena->free_count = &ht->hdr->value_free_count;
    arena->storage_size = ht->hdr->config.value_storage_size;
}

/**
 *  @brief          空きリストを未使用の初期状態へ戻します。
 *  @param[in,out]  arena  操作対象。NULL を渡してはなりません。
 *
 *  ストレージ全体が 1 個の空きブロックになります。
 */
static void arena_reset(struct hashtable_arena *arena)
{
    arena->free_list[0].offset = 0;
    arena->free_list[0].length = (uint64_t)arena->storage_size;
    *arena->free_count = 1;
}

/**
 *  @brief          指定の空きブロックが、置き換え対象ブロックと隣接するかを判定します。
 *  @param[in]      arena       操作対象。NULL を渡してはなりません。
 *  @param[in]      index       空きブロックのインデックス。
 *  @param[in]      own_offset  置き換え対象ブロックの先頭オフセット。
 *  @param[in]      own_length  置き換え対象ブロックのバイト数。0 なら対象なしです。
 *  @return         隣接するなら 1、しないなら 0 です。
 */
static int arena_free_block_adjoins(const struct hashtable_arena *arena, uint64_t index, uint64_t own_offset,
                                    uint64_t own_length)
{
    if (own_length == 0)
    {
        return 0;
    }
    if (((arena->free_list[index].offset + arena->free_list[index].length) == own_offset) ||
        (arena->free_list[index].offset == (own_offset + own_length)))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 *  @brief          先着適合で空き領域を探します。
 *  @param[in]      arena       操作対象。NULL を渡してはなりません。
 *  @param[in]      own_offset  置き換え対象ブロックの先頭オフセット。
 *  @param[in]      own_length  置き換え対象ブロックのバイト数。0 なら対象なしです。
 *  @param[in]      needed      必要バイト数。0 を渡してはなりません。
 *  @param[out]     offset_out  見つかった先頭オフセットの格納先。
 *  @return         見つかれば 1、見つからなければ 0 です。
 *
 *  空きリストを変更しません。@p own_length が非 0 のときは、その区間を
 *  前後の隣接する空きブロックと結合したうえで空きとみなします。\n
 *  結合した空きも、オフセットの昇順の位置で評価します。
 */
static int arena_find_fit(const struct hashtable_arena *arena, uint64_t own_offset, uint64_t own_length, size_t needed,
                          size_t *offset_out)
{
    uint64_t count = *arena->free_count;
    uint64_t merged_offset = own_offset;
    uint64_t merged_length = own_length;
    uint64_t i;
    int merged_pending;
    if (own_length != 0)
    {
        merged_pending = 1;
    }
    else
    {
        merged_pending = 0;
    }

    if (own_length != 0)
    {
        for (i = 0; i < count; i++)
        {
            if (arena_free_block_adjoins(arena, i, own_offset, own_length) != 0)
            {
                merged_length += arena->free_list[i].length;
                if (arena->free_list[i].offset < merged_offset)
                {
                    merged_offset = arena->free_list[i].offset;
                }
            }
        }
    }
    for (i = 0; i < count; i++)
    {
        if ((merged_pending != 0) && (merged_offset < arena->free_list[i].offset))
        {
            if ((uint64_t)needed <= merged_length)
            {
                *offset_out = (size_t)merged_offset;
                return 1;
            }
            merged_pending = 0;
        }
        if (arena_free_block_adjoins(arena, i, own_offset, own_length) != 0)
        {
            continue; /* 結合済みのため、単独では評価しない。 */
        }
        if ((uint64_t)needed <= arena->free_list[i].length)
        {
            *offset_out = (size_t)arena->free_list[i].offset;
            return 1;
        }
    }
    if ((merged_pending != 0) && ((uint64_t)needed <= merged_length))
    {
        *offset_out = (size_t)merged_offset;
        return 1;
    }
    return 0;
}

/**
 *  @brief          指定区間を空きリストから取り除きます。
 *  @param[in,out]  arena   操作対象。NULL を渡してはなりません。
 *  @param[in]      offset  取り除く区間の先頭オフセット。
 *  @param[in]      length  取り除く区間のバイト数。0 を渡してはなりません。
 *
 *  区間全体が 1 個の空きブロックに収まっていることが前提です。\n
 *  途中を取り除く場合は空きブロックが 2 個へ分かれますが、同時に使用中ブロックが 1 個増えるため、
 *  要素数の上限 capacity + 1 は超えません。
 */
static void arena_take(struct hashtable_arena *arena, size_t offset, size_t length)
{
    uint64_t count = *arena->free_count;
    uint64_t start = (uint64_t)offset;
    uint64_t end = start + (uint64_t)length;
    uint64_t i;

    for (i = 0; i < count; i++)
    {
        uint64_t free_end = arena->free_list[i].offset + arena->free_list[i].length;

        if ((arena->free_list[i].offset > start) || (end > free_end))
        {
            continue;
        }
        if ((arena->free_list[i].offset == start) && (end == free_end))
        {
            memmove(&arena->free_list[i], &arena->free_list[i + 1u],
                    (size_t)(count - i - 1u) * sizeof(struct hashtable_free_block));
            *arena->free_count = count - 1u;
        }
        else if (arena->free_list[i].offset == start)
        {
            arena->free_list[i].offset = end;
            arena->free_list[i].length = free_end - end;
        }
        else if (end == free_end)
        {
            arena->free_list[i].length = start - arena->free_list[i].offset;
        }
        else
        {
            memmove(&arena->free_list[i + 2u], &arena->free_list[i + 1u],
                    (size_t)(count - i - 1u) * sizeof(struct hashtable_free_block));
            arena->free_list[i].length = start - arena->free_list[i].offset;
            arena->free_list[i + 1u].offset = end;
            arena->free_list[i + 1u].length = free_end - end;
            *arena->free_count = count + 1u;
        }
        return;
    }
}

/**
 *  @brief          指定区間を空きリストへ返します。
 *  @param[in,out]  arena   操作対象。NULL を渡してはなりません。
 *  @param[in]      offset  返す区間の先頭オフセット。
 *  @param[in]      length  返す区間のバイト数。0 なら何もしません。
 *
 *  前後に隣接する空きブロックがあれば結合し、空きブロックが常に極大であるように保ちます。
 */
static void arena_give(struct hashtable_arena *arena, size_t offset, size_t length)
{
    uint64_t count = *arena->free_count;
    uint64_t start = (uint64_t)offset;
    uint64_t end = start + (uint64_t)length;
    uint64_t i = 0;
    int merge_prev;
    int merge_next;

    if (length == 0)
    {
        return;
    }
    while ((i < count) && (arena->free_list[i].offset < start))
    {
        i++;
    }
    if ((i > 0u) && ((arena->free_list[i - 1u].offset + arena->free_list[i - 1u].length) == start))
    {
        merge_prev = 1;
    }
    else
    {
        merge_prev = 0;
    }
    if ((i < count) && (arena->free_list[i].offset == end))
    {
        merge_next = 1;
    }
    else
    {
        merge_next = 0;
    }
    if ((merge_prev != 0) && (merge_next != 0))
    {
        arena->free_list[i - 1u].length += (uint64_t)length + arena->free_list[i].length;
        memmove(&arena->free_list[i], &arena->free_list[i + 1u],
                (size_t)(count - i - 1u) * sizeof(struct hashtable_free_block));
        *arena->free_count = count - 1u;
    }
    else if (merge_prev != 0)
    {
        arena->free_list[i - 1u].length += (uint64_t)length;
    }
    else if (merge_next != 0)
    {
        arena->free_list[i].offset = start;
        arena->free_list[i].length += (uint64_t)length;
    }
    else
    {
        memmove(&arena->free_list[i + 1u], &arena->free_list[i],
                (size_t)(count - i) * sizeof(struct hashtable_free_block));
        arena->free_list[i].offset = start;
        arena->free_list[i].length = (uint64_t)length;
        *arena->free_count = count + 1u;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_arena_compact(cplat_hashtable *ht, struct hashtable_arena *arena, hashtable_ref_fn get_ref,
                             uint64_t used)
{
    uint64_t count = *arena->free_count;
    uint64_t shift = 0;
    uint64_t prev_end = 0;
    uint64_t running = 0;
    uint64_t i;
    size_t rec;

    for (i = 0; i < count; i++)
    {
        uint64_t run_length = arena->free_list[i].offset - prev_end;

        if ((run_length != 0) && (shift != 0))
        {
            memmove(arena->storage + (size_t)(prev_end - shift), arena->storage + (size_t)prev_end, (size_t)run_length);
        }
        shift += arena->free_list[i].length;
        prev_end = arena->free_list[i].offset + arena->free_list[i].length;
    }
    if ((prev_end < (uint64_t)arena->storage_size) && (shift != 0))
    {
        memmove(arena->storage + (size_t)(prev_end - shift), arena->storage + (size_t)prev_end,
                (size_t)((uint64_t)arena->storage_size - prev_end));
    }

    /* 空きブロックの長さを累積和へ置き換える。以降、この配列は移動量の索引としてだけ使う。 */
    for (i = 0; i < count; i++)
    {
        running += arena->free_list[i].length;
        arena->free_list[i].length = running;
    }
    for (rec = 0; rec < ht->hdr->config.capacity; rec++)
    {
        struct hashtable_string_ref *ref = get_ref(ht, rec);
        uint64_t low = 0;
        uint64_t high = count;

        if (ref->length == 0)
        {
            continue;
        }
        while (low < high)
        {
            uint64_t mid = low + ((high - low) / 2u);

            if (arena->free_list[mid].offset < ref->offset)
            {
                low = mid + 1u;
            }
            else
            {
                high = mid;
            }
        }
        if (low != 0u)
        {
            ref->offset -= arena->free_list[low - 1u].length;
        }
    }

    memset(arena->storage + (size_t)used, 0, arena->storage_size - (size_t)used);
    if ((uint64_t)arena->storage_size > used)
    {
        arena->free_list[0].offset = used;
        arena->free_list[0].length = (uint64_t)arena->storage_size - used;
        *arena->free_count = 1;
    }
    else
    {
        *arena->free_count = 0;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_release_key(cplat_hashtable *ht, size_t rec)
{
    if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
    {
        struct hashtable_string_ref *ref = hashtable_key_ref_at(ht, rec);
        struct hashtable_arena arena;

        hashtable_key_arena(ht, &arena);
        memset(hashtable_key_storage(ht) + (size_t)ref->offset, 0, (size_t)ref->length);
        arena_give(&arena, (size_t)ref->offset, (size_t)ref->length);
        ht->hdr->key_storage_used -= ref->length;
        ref->offset = 0;
        ref->length = 0;
    }
    else
    {
        memset(hashtable_entry_key(ht, rec), 0, ht->hdr->config.key_size);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_release_value(cplat_hashtable *ht, size_t rec)
{
    if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
    {
        struct hashtable_string_ref *ref = hashtable_value_ref_at(ht, rec);
        struct hashtable_arena arena;

        hashtable_value_arena(ht, &arena);
        memset(hashtable_value_storage(ht) + (size_t)ref->offset, 0, (size_t)ref->length);
        arena_give(&arena, (size_t)ref->offset, (size_t)ref->length);
        ht->hdr->value_storage_used -= ref->length;
        ref->offset = 0;
        ref->length = 0;
    }
    else
    {
        memset(hashtable_data_at(ht, rec), 0, ht->hdr->config.value_size);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_key_store(cplat_hashtable *ht, size_t rec, const void *key, size_t storage_offset)
{
    if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
    {
        size_t length = strlen((const char *)key) + 1u;
        struct hashtable_string_ref *ref;
        struct hashtable_arena arena;

        hashtable_key_arena(ht, &arena);
        arena_take(&arena, storage_offset, length);
        ref = hashtable_key_ref_at(ht, rec);
        ref->offset = (uint64_t)storage_offset;
        ref->length = (uint64_t)length;
        memcpy(hashtable_key_storage(ht) + (size_t)ref->offset, key, length);
        ht->hdr->key_storage_used += (uint64_t)length;
    }
    else if (ht->hdr->config.key_type == CPLAT_HASHTABLE_FIELD_FIXED_STRING)
    {
        size_t copy_len = strlen((const char *)key) + 1u;
        char *dst = hashtable_entry_key(ht, rec);

        memset(dst, 0, ht->hdr->config.key_size);
        memcpy(dst, key, copy_len);
    }
    else
    {
        memcpy(hashtable_entry_key(ht, rec), key, ht->hdr->config.key_size);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_value_store(cplat_hashtable *ht, size_t rec, const void *value, size_t storage_offset)
{
    if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
    {
        size_t length = strlen((const char *)value) + 1u;
        struct hashtable_string_ref *ref;
        struct hashtable_arena arena;

        hashtable_value_arena(ht, &arena);
        arena_take(&arena, storage_offset, length);
        ref = hashtable_value_ref_at(ht, rec);
        ref->offset = (uint64_t)storage_offset;
        ref->length = (uint64_t)length;
        memcpy(hashtable_value_storage(ht) + (size_t)ref->offset, value, length);
        ht->hdr->value_storage_used += (uint64_t)length;
    }
    else if (ht->hdr->config.value_type == CPLAT_HASHTABLE_FIELD_FIXED_STRING)
    {
        size_t copy_len = strlen((const char *)value) + 1u;

        memset(hashtable_data_at(ht, rec), 0, ht->hdr->config.value_size);
        memcpy(hashtable_data_at(ht, rec), value, copy_len);
    }
    else
    {
        memcpy(hashtable_data_at(ht, rec), value, ht->hdr->config.value_size);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_arena_validate(const struct hashtable_arena *arena, uint64_t used, size_t capacity)
{
    uint64_t count = *arena->free_count;
    uint64_t total = 0;
    uint64_t prev_end = 0;
    uint64_t i;

    if (count > (uint64_t)capacity + 1u)
    {
        return -1;
    }
    for (i = 0; i < count; i++)
    {
        if (arena->free_list[i].length == 0)
        {
            return -1;
        }
        if (arena->free_list[i].offset > (uint64_t)arena->storage_size)
        {
            return -1;
        }
        if (arena->free_list[i].length > ((uint64_t)arena->storage_size - arena->free_list[i].offset))
        {
            return -1;
        }
        /* 2 個目以降は、直前の空きブロックの終端より後ろから始まること。等しい場合は未結合で不正。 */
        if ((i != 0u) && (arena->free_list[i].offset <= prev_end))
        {
            return -1;
        }
        prev_end = arena->free_list[i].offset + arena->free_list[i].length;
        total += arena->free_list[i].length;
    }
    if (total != ((uint64_t)arena->storage_size - used))
    {
        return -1;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_arena_validate_used_block(const struct hashtable_arena *arena, uint64_t offset, uint64_t length)
{
    uint64_t count = *arena->free_count;
    uint64_t low = 0;
    uint64_t high = count;

    while (low < high)
    {
        uint64_t mid = low + ((high - low) / 2u);

        if (arena->free_list[mid].offset <= offset)
        {
            low = mid + 1u;
        }
        else
        {
            high = mid;
        }
    }
    if ((low != 0u) && ((arena->free_list[low - 1u].offset + arena->free_list[low - 1u].length) > offset))
    {
        return -1;
    }
    if ((low < count) && ((offset + length) > arena->free_list[low].offset))
    {
        return -1;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_reset_arenas(cplat_hashtable *ht)
{
    struct hashtable_arena arena;

    if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
    {
        hashtable_key_arena(ht, &arena);
        arena_reset(&arena);
    }
    if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
    {
        hashtable_value_arena(ht, &arena);
        arena_reset(&arena);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_key_storage_find_free(const cplat_hashtable *ht, size_t rec, int replace, const void *key,
                                    size_t *offset_out)
{
    struct hashtable_arena arena;
    uint64_t own_offset = 0;
    uint64_t own_length = 0;
    size_t needed;

    if (hashtable_field_is_variable(ht->hdr->config.key_type) == 0)
    {
        *offset_out = 0;
        return 1;
    }
    if (replace != 0)
    {
        const struct hashtable_string_ref *own = hashtable_key_ref_at(ht, rec);

        own_offset = own->offset;
        own_length = own->length;
    }
    hashtable_key_arena(ht, &arena);
    needed = hashtable_field_input_size(ht->hdr->config.key_type, 0, key);
    return arena_find_fit(&arena, own_offset, own_length, needed, offset_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_value_storage_find_free(const cplat_hashtable *ht, size_t rec, int replace, const void *value,
                                      size_t *offset_out)
{
    struct hashtable_arena arena;
    uint64_t own_offset = 0;
    uint64_t own_length = 0;
    size_t needed;

    if (hashtable_field_is_variable(ht->hdr->config.value_type) == 0)
    {
        *offset_out = 0;
        return 1;
    }
    if (replace != 0)
    {
        const struct hashtable_string_ref *own = hashtable_value_ref_at(ht, rec);

        own_offset = own->offset;
        own_length = own->length;
    }
    hashtable_value_arena(ht, &arena);
    needed = hashtable_field_input_size(ht->hdr->config.value_type, 0, value);
    return arena_find_fit(&arena, own_offset, own_length, needed, offset_out);
}
