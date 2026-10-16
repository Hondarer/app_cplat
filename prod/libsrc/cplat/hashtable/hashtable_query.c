/**
 *******************************************************************************
 *  @file           hashtable_query.c
 *  @brief          キーとレコード番号による参照と、件数の取得を担います。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
 *  @version        1.0.0
 *
 *  テーブルの状態を変更しない読み出し API をまとめています。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "hashtable.h"

#include <string.h>

/**
 *  @brief          キーで使用中のレコードを探します。
 *  @param[in]      ht          対象。NULL を渡してはなりません。
 *  @param[in]      key         キー。NULL を渡してはなりません。
 *  @param[in]      key_length  バイナリ キーの有効長。文字列キーでは使いません。
 *  @param[out]     rec_out     0 相対のスロット インデックスの格納先。成功時だけ書きます。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_OUT_OF_RANGE 、@ref CPLAT_ERR_NOT_FOUND 。
 *
 *  削除済みは見つからない扱いです。キーは残っていても返しません。
 */
static int hashtable_lookup(const cplat_hashtable *ht, const void *key, size_t key_length, size_t *rec_out)
{
    uint64_t cur;

    if (!hashtable_key_fits(ht, key, key_length))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    cur = hashtable_bucket_head(ht)[hashtable_hash_key(ht, key, key_length)];
    while (cur != 0)
    {
        size_t rec = (size_t)(cur - 1);

        if (hashtable_key_equal(ht, rec, key, key_length))
        {
            if (*hashtable_entry_status(ht, rec) == REC_IN_USE)
            {
                *rec_out = rec;
                return CPLAT_OK;
            }
            return CPLAT_ERR_NOT_FOUND;
        }
        cur = *hashtable_entry_next(ht, rec);
    }
    return CPLAT_ERR_NOT_FOUND;
}

static int find_value_ref(const cplat_hashtable *ht, const void *key, size_t key_length, const void **value_out)
{
    size_t rec;
    int ret;

    if ((ht == NULL) || (key == NULL) || (value_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = hashtable_lookup(ht, key, key_length, &rec);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *value_out = hashtable_data_at(ht, rec);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_value_ref(const cplat_hashtable *ht, const void *key, const void **value_out)
{
    return find_value_ref(ht, key, hashtable_default_key_length(ht), value_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_value_ref_with_key_length(const cplat_hashtable *ht, const void *key, size_t key_length,
                                                   const void **value_out)
{
    if (hashtable_rejects_key_length(ht) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return find_value_ref(ht, key, key_length, value_out);
}

static size_t stored_value_size(const cplat_hashtable *ht, size_t rec)
{
    if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
    {
        return (size_t)hashtable_value_ref_at(ht, rec)->length;
    }
    if (ht->hdr->config.value_type == CPLAT_HASHTABLE_FIELD_FIXED_STRING)
    {
        return strlen((const char *)hashtable_data_at(ht, rec)) + 1u;
    }
    return ht->hdr->config.value_size;
}

static size_t stored_key_size(const cplat_hashtable *ht, size_t rec)
{
    if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
    {
        return (size_t)hashtable_key_ref_at(ht, rec)->length;
    }
    if (ht->hdr->config.key_type == CPLAT_HASHTABLE_FIELD_FIXED_STRING)
    {
        return strlen(hashtable_entry_key(ht, rec)) + 1u;
    }
    return hashtable_stored_key_length(ht, rec);
}

static int copy_field(const void *src, size_t required_size, void *dest, size_t dest_size, size_t *required_size_out)
{
    if ((required_size_out == NULL) || ((dest == NULL) && (dest_size != 0)) || ((dest != NULL) && (dest_size == 0)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *required_size_out = required_size;
    if (dest == NULL)
    {
        return CPLAT_OK;
    }
    if (dest_size < required_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    memcpy(dest, src, required_size);
    return CPLAT_OK;
}

static int copy_arguments_are_valid(const void *dest, size_t dest_size, const size_t *required_size_out)
{
    return (required_size_out != NULL) &&
           (((dest == NULL) && (dest_size == 0)) || ((dest != NULL) && (dest_size != 0)));
}

static int find_recno(const cplat_hashtable *ht, const void *key, size_t key_length, uint64_t *record_out)
{
    size_t rec;
    int ret;

    if ((ht == NULL) || (key == NULL) || (record_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = hashtable_lookup(ht, key, key_length, &rec);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *record_out = (uint64_t)(rec + 1);
    return CPLAT_OK;
}

static int find_value_copy(const cplat_hashtable *ht, const void *key, size_t key_length, void *dest, size_t dest_size,
                           size_t *required_size_out)
{
    uint64_t record;
    int ret;

    if (copy_arguments_are_valid(dest, dest_size, required_size_out) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = find_recno(ht, key, key_length, &record);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    return copy_field(hashtable_data_at(ht, (size_t)(record - 1)), stored_value_size(ht, (size_t)(record - 1)), dest,
                      dest_size, required_size_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_value_copy(const cplat_hashtable *ht, const void *key, void *dest, size_t dest_size,
                                    size_t *required_size_out)
{
    return find_value_copy(ht, key, hashtable_default_key_length(ht), dest, dest_size, required_size_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_value_copy_with_key_length(const cplat_hashtable *ht, const void *key, size_t key_length,
                                                    void *dest, size_t dest_size, size_t *required_size_out)
{
    if (hashtable_rejects_key_length(ht) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return find_value_copy(ht, key, key_length, dest, dest_size, required_size_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_recno(const cplat_hashtable *ht, const void *key, uint64_t *record_out)
{
    return find_recno(ht, key, hashtable_default_key_length(ht), record_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_recno_with_key_length(const cplat_hashtable *ht, const void *key, size_t key_length,
                                               uint64_t *record_out)
{
    if (hashtable_rejects_key_length(ht) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return find_recno(ht, key, key_length, record_out);
}

static int find_timestamp_ref(const cplat_hashtable *ht, const void *key, size_t key_length,
                              const cplat_timespec **timestamp_out)
{
    size_t rec;
    int ret;

    if ((ht == NULL) || (key == NULL) || (timestamp_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_has_record_timestamp(ht) == 0)
    {
        return CPLAT_ERR_UNSUPPORTED;
    }
    ret = hashtable_lookup(ht, key, key_length, &rec);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *timestamp_out = hashtable_entry_timestamp(ht, rec);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_timestamp_ref(const cplat_hashtable *ht, const void *key, const cplat_timespec **timestamp_out)
{
    return find_timestamp_ref(ht, key, hashtable_default_key_length(ht), timestamp_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_timestamp_ref_with_key_length(const cplat_hashtable *ht, const void *key, size_t key_length,
                                                       const cplat_timespec **timestamp_out)
{
    if (hashtable_rejects_key_length(ht) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return find_timestamp_ref(ht, key, key_length, timestamp_out);
}

static int find_timestamp_val(const cplat_hashtable *ht, const void *key, size_t key_length,
                              cplat_timespec *timestamp_out)
{
    const cplat_timespec *src;
    int ret;

    if (timestamp_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = find_timestamp_ref(ht, key, key_length, &src);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *timestamp_out = *src;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_timestamp_val(const cplat_hashtable *ht, const void *key, cplat_timespec *timestamp_out)
{
    return find_timestamp_val(ht, key, hashtable_default_key_length(ht), timestamp_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_timestamp_val_with_key_length(const cplat_hashtable *ht, const void *key, size_t key_length,
                                                       cplat_timespec *timestamp_out)
{
    if (hashtable_rejects_key_length(ht) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return find_timestamp_val(ht, key, key_length, timestamp_out);
}

static int find_generation(const cplat_hashtable *ht, const void *key, size_t key_length, uint64_t *generation_out)
{
    size_t rec;
    int ret;

    if ((ht == NULL) || (key == NULL) || (generation_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_has_record_timestamp(ht) == 0)
    {
        return CPLAT_ERR_UNSUPPORTED;
    }
    ret = hashtable_lookup(ht, key, key_length, &rec);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *generation_out = *hashtable_entry_generation(ht, rec);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_generation(const cplat_hashtable *ht, const void *key, uint64_t *generation_out)
{
    return find_generation(ht, key, hashtable_default_key_length(ht), generation_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_find_generation_with_key_length(const cplat_hashtable *ht, const void *key, size_t key_length,
                                                    uint64_t *generation_out)
{
    if (hashtable_rejects_key_length(ht) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return find_generation(ht, key, key_length, generation_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_key_ref(const cplat_hashtable *ht, uint64_t record, const void **key_out,
                                size_t *key_length_out)
{
    size_t rec;

    if ((ht == NULL) || (key_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    rec = (size_t)(record - 1);
    /* 空は失敗。削除済みは削除直前のキーを返す。 */
    if (*hashtable_entry_status(ht, rec) == REC_EMPTY)
    {
        return CPLAT_ERR_NOT_FOUND;
    }
    *key_out = hashtable_entry_key(ht, rec);
    if (key_length_out != NULL)
    {
        *key_length_out = stored_key_size(ht, rec);
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_key_copy(const cplat_hashtable *ht, uint64_t record, void *dest, size_t dest_size,
                                 size_t *required_size_out)
{
    const void *src;
    size_t key_length;
    int ret;

    if (copy_arguments_are_valid(dest, dest_size, required_size_out) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_hashtable_get_key_ref(ht, record, &src, &key_length);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    return copy_field(src, key_length, dest, dest_size, required_size_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_value_ref(const cplat_hashtable *ht, uint64_t record, const void **value_out)
{
    size_t rec;

    if ((ht == NULL) || (value_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    rec = (size_t)(record - 1);
    /* 空は失敗。削除済みは削除直前の値を返す。 */
    if (*hashtable_entry_status(ht, rec) == REC_EMPTY)
    {
        return CPLAT_ERR_NOT_FOUND;
    }
    *value_out = hashtable_data_at(ht, rec);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_value_copy(const cplat_hashtable *ht, uint64_t record, void *dest, size_t dest_size,
                                   size_t *required_size_out)
{
    const void *src;
    int ret;

    if (copy_arguments_are_valid(dest, dest_size, required_size_out) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_hashtable_get_value_ref(ht, record, &src);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    return copy_field(src, stored_value_size(ht, (size_t)(record - 1)), dest, dest_size, required_size_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_status(const cplat_hashtable *ht, uint64_t record, int *status_out)
{
    if ((ht == NULL) || (status_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *status_out = (int)*hashtable_entry_status(ht, (size_t)(record - 1));
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_next_record(const cplat_hashtable *ht, uint64_t from, unsigned int status_mask,
                                uint64_t *record_out, int *has_record_out)
{
    unsigned int known_bits = CPLAT_HASHTABLE_SCAN_IN_USE | CPLAT_HASHTABLE_SCAN_DELETED | CPLAT_HASHTABLE_SCAN_EMPTY;
    size_t i;

    if ((ht == NULL) || (record_out == NULL) || (has_record_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((status_mask == 0) || ((status_mask & ~known_bits) != 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    /* from は直前に返したレコード番号。capacity と等しいときは走査済みで、終端を返す。 */
    if (from > ht->hdr->config.capacity)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *has_record_out = 0;
    for (i = (size_t)from; i < ht->hdr->config.capacity; i++)
    {
        unsigned char status = *hashtable_entry_status(ht, i);
        unsigned int bit;

        if (status == REC_EMPTY)
        {
            bit = CPLAT_HASHTABLE_SCAN_EMPTY;
        }
        else if (status == REC_IN_USE)
        {
            bit = CPLAT_HASHTABLE_SCAN_IN_USE;
        }
        else
        {
            bit = CPLAT_HASHTABLE_SCAN_DELETED;
        }
        if ((status_mask & bit) != 0)
        {
            *record_out = (uint64_t)(i + 1);
            *has_record_out = 1;
            return CPLAT_OK;
        }
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_timestamp_ref(const cplat_hashtable *ht, uint64_t record, const cplat_timespec **timestamp_out)
{
    size_t rec;

    if ((ht == NULL) || (timestamp_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_has_record_timestamp(ht) == 0)
    {
        return CPLAT_ERR_UNSUPPORTED;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    rec = (size_t)(record - 1);
    if (*hashtable_entry_status(ht, rec) == REC_EMPTY)
    {
        return CPLAT_ERR_NOT_FOUND;
    }
    *timestamp_out = hashtable_entry_timestamp(ht, rec);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_timestamp_val(const cplat_hashtable *ht, uint64_t record, cplat_timespec *timestamp_out)
{
    const cplat_timespec *src;
    int ret;

    if (timestamp_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_hashtable_get_timestamp_ref(ht, record, &src);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *timestamp_out = *src;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_generation(const cplat_hashtable *ht, uint64_t record, uint64_t *generation_out)
{
    size_t rec;

    if ((ht == NULL) || (generation_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_has_record_timestamp(ht) == 0)
    {
        return CPLAT_ERR_UNSUPPORTED;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    rec = (size_t)(record - 1);
    if (*hashtable_entry_status(ht, rec) == REC_EMPTY)
    {
        return CPLAT_ERR_NOT_FOUND;
    }
    *generation_out = *hashtable_entry_generation(ht, rec);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_table_timestamp_ref(const cplat_hashtable *ht, const cplat_timespec **timestamp_out)
{
    if ((ht == NULL) || (timestamp_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *timestamp_out = &ht->hdr->table_timestamp;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_table_timestamp_val(const cplat_hashtable *ht, cplat_timespec *timestamp_out)
{
    const cplat_timespec *src;
    int ret;

    if (timestamp_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_hashtable_get_table_timestamp_ref(ht, &src);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *timestamp_out = *src;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_table_generation(const cplat_hashtable *ht, uint64_t *generation_out)
{
    if ((ht == NULL) || (generation_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *generation_out = ht->hdr->table_generation;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_count_status(const cplat_hashtable *ht, size_t *in_use_out, size_t *deleted_out, size_t *empty_out)
{
    if (ht == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 永続化領域のカウンターをそのまま返す。capacity に依存しない一定時間。 */
    if (in_use_out != NULL)
    {
        *in_use_out = (size_t)ht->hdr->in_use_count;
    }
    if (deleted_out != NULL)
    {
        *deleted_out = (size_t)ht->hdr->deleted_count;
    }
    if (empty_out != NULL)
    {
        *empty_out = (size_t)(ht->hdr->config.capacity - ht->hdr->in_use_count - ht->hdr->deleted_count);
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_count(const cplat_hashtable *ht, size_t *count_out)
{
    if ((ht == NULL) || (count_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return cplat_hashtable_count_status(ht, count_out, NULL, NULL);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_deleted_count(const cplat_hashtable *ht, size_t *count_out)
{
    if ((ht == NULL) || (count_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return cplat_hashtable_count_status(ht, NULL, count_out, NULL);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_empty_count(const cplat_hashtable *ht, size_t *count_out)
{
    if ((ht == NULL) || (count_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return cplat_hashtable_count_status(ht, NULL, NULL, count_out);
}
