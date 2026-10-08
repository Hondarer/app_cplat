/**
 *******************************************************************************
 *  @file           hashtable_modify.c
 *  @brief          キーと値の追加、更新、削除を担います。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
 *  @version        1.0.0
 *
 *  追加、更新、削除、削除済みの押し出しと消去に加え、変更時刻と世代カウンターの
 *  刻印を担います。\n
 *  領域不足を検出した場合は拡張要求を返し、自動拡張は hashtable_grow.c が担います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "hashtable.h"

#include <cplat/clock/clock.h>
#include <string.h>

static int input_points_into_table(const cplat_hashtable *ht, const void *input)
{
    uintptr_t p = (uintptr_t)input;
    uintptr_t mgmt = (uintptr_t)hashtable_bytes(ht);
    uintptr_t data = (uintptr_t)hashtable_data(ht);
    size_t mgmt_size = 0;
    size_t data_size = 0;

    (void)hashtable_mgmt_layout(&ht->hdr->config, NULL, NULL, &mgmt_size);
    (void)hashtable_data_region_size(&ht->hdr->config, &data_size);
    return ((p >= mgmt) && (p - mgmt < mgmt_size)) || ((p >= data) && (p - data < data_size));
}

/**
 *  @brief          start_idx 以降の最小空きスロットを 1 相対で返します。
 *  @param[in]      ht         対象。NULL を渡してはなりません。
 *  @param[in]      start_idx  走査開始の 0 相対インデックス。
 *  @return         1 相対のレコード番号です。無ければ 0 です。
 */
static uint64_t scan_next_empty(cplat_hashtable *ht, size_t start_idx)
{
    size_t i;

    for (i = start_idx; i < ht->hdr->config.capacity; i++)
    {
        if (*hashtable_entry_status(ht, i) == REC_EMPTY)
        {
            return (uint64_t)(i + 1);
        }
    }
    return 0;
}

/**
 *  @brief          スロットへ現在の実時刻を刻みます。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。
 */
static void stamp_record(cplat_hashtable *ht, size_t rec)
{
    cplat_timespec now;

    cplat_clock_get_realtime(&now);
    /* 実時刻は時計の巻き戻しで逆行しうるため、順序判定は世代カウンターで行う。 */
    ht->hdr->table_generation++;
    if (hashtable_has_record_timestamp(ht) != 0)
    {
        *hashtable_entry_timestamp(ht, rec) = now;
        *hashtable_entry_generation(ht, rec) = ht->hdr->table_generation;
    }
    ht->hdr->table_timestamp = now;
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_stamp_table(cplat_hashtable *ht)
{
    cplat_clock_get_realtime(&ht->hdr->table_timestamp);
    ht->hdr->table_generation++;
}

/* Doxygen コメントは、ヘッダーに記載 */

uint64_t hashtable_find_best_deleted_record(const cplat_hashtable *ht, const unsigned char *keep)
{
    size_t capacity = ht->hdr->config.capacity;
    int has_record_timestamp = hashtable_has_record_timestamp(ht);
    size_t best_rec = 0;
    int found = 0;
    unsigned char best_status = 0;
    uint64_t best_generation = 0;
    size_t i;

    for (i = 0; i < capacity; i++)
    {
        unsigned char status = *hashtable_entry_status(ht, i);
        int better;

        if (status < REC_DELETED)
        {
            continue;
        }
        if ((keep != NULL) && (keep[i] == 0))
        {
            continue;
        }
        if (!found)
        {
            better = 1;
        }
        else if (status > best_status)
        {
            better = 1;
        }
        else if ((status == best_status) && (has_record_timestamp != 0) &&
                 (*hashtable_entry_generation(ht, i) < best_generation))
        {
            better = 1;
        }
        else
        {
            better = 0;
        }
        if (better != 0)
        {
            found = 1;
            best_rec = i;
            best_status = status;
            if (has_record_timestamp != 0)
            {
                best_generation = *hashtable_entry_generation(ht, i);
            }
        }
    }
    if (found == 0)
    {
        return 0;
    }
    return (uint64_t)(best_rec + 1);
}

/**
 *  @brief          レコードをバケット チェーンから切り離します。
 *
 *  レコードの状態・キー・値は変更しません。
 *
 *  @param[in,out]  ht          対象のハンドルです。
 *  @param[in]      rec         切り離すレコードの 0 起点の番号です。
 */
static void hashtable_unlink_record(cplat_hashtable *ht, size_t rec)
{
    size_t idx = hashtable_hash_key(ht, hashtable_entry_key(ht, rec));
    uint64_t *link = &hashtable_bucket_head(ht)[idx];
    uint64_t target = (uint64_t)(rec + 1);

    while (*link != 0)
    {
        if (*link == target)
        {
            *link = *hashtable_entry_next(ht, rec);
            *hashtable_entry_next(ht, rec) = 0;
            return;
        }
        link = hashtable_entry_next(ht, (size_t)(*link - 1));
    }
}

/**
 *  @brief          追加した / 更新したの別を、必要なら呼び出し側へ書きます。
 *  @param[out]     inserted_out  格納先。NULL を渡せます。
 *  @param[in]      inserted      新規追加なら 1、既存更新なら 0。
 */
static void store_inserted(int *inserted_out, int inserted)
{
    if (inserted_out != NULL)
    {
        *inserted_out = inserted;
    }
}

static size_t storage_min_for_transaction(uint64_t used, size_t input_size)
{
    size_t minimum;

    if (hashtable_add_checked((size_t)used, input_size, &minimum) != 0)
    {
        return SIZE_MAX;
    }
    return minimum;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_put(cplat_hashtable *ht, const void *key, const void *value,
                  cplat_hashtable_add_deleted_policy deleted_policy, int allow_update, int *inserted_out,
                  struct hashtable_growth_request *growth_out)
{
    size_t key_storage_offset = 0;
    size_t value_storage_offset = 0;
    size_t idx;
    uint64_t *bucket_head;
    uint64_t cur;
    uint64_t rec_no;
    size_t rec;
    int key_has_space;
    int value_has_space;

    if (growth_out != NULL)
    {
        memset(growth_out, 0, sizeof(*growth_out));
    }

    if ((ht == NULL) || (key == NULL) || (value == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((input_points_into_table(ht, key) != 0) || (input_points_into_table(ht, value) != 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((deleted_policy != CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE) &&
        (deleted_policy != CPLAT_HASHTABLE_ADD_DELETED_REVIVE))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (!hashtable_key_fits(ht, key))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }
    if (hashtable_fixed_string_fits(ht->hdr->config.value_type, ht->hdr->config.value_size, value) == 0)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    idx = hashtable_hash_key(ht, key);
    bucket_head = hashtable_bucket_head(ht);
    cur = bucket_head[idx];
    while (cur != 0)
    {
        rec = (size_t)(cur - 1);
        if (hashtable_key_equal(ht, hashtable_entry_key(ht, rec), key))
        {
            if (*hashtable_entry_status(ht, rec) == REC_IN_USE)
            {
                if (allow_update == 0)
                {
                    return CPLAT_ERR_DUPLICATE_KEY;
                }
                /* upsert は使用中の同一キーの値を書き換える。 */
                if (hashtable_value_storage_find_free(ht, rec, 1, value, &value_storage_offset) == 0)
                {
                    if (growth_out != NULL)
                    {
                        growth_out->pressure |= HASHTABLE_GROWTH_VALUE_STORAGE;
                        growth_out->value_storage_min = storage_min_for_transaction(
                            ht->hdr->value_storage_used,
                            hashtable_field_input_size(ht->hdr->config.value_type, 0, value));
                    }
                    return CPLAT_ERR_STORAGE_FULL;
                }
                hashtable_release_value(ht, rec);
                hashtable_value_store(ht, rec, value, value_storage_offset);
                stamp_record(ht, rec);
                store_inserted(inserted_out, 0);
                return CPLAT_OK;
            }
            /* 削除済みの同一キーは、同じレコードを再利用する。 */
            if (deleted_policy == CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE)
            {
                if (hashtable_value_storage_find_free(ht, rec, 1, value, &value_storage_offset) == 0)
                {
                    if (growth_out != NULL)
                    {
                        growth_out->pressure |= HASHTABLE_GROWTH_VALUE_STORAGE;
                        growth_out->value_storage_min = storage_min_for_transaction(
                            ht->hdr->value_storage_used,
                            hashtable_field_input_size(ht->hdr->config.value_type, 0, value));
                    }
                    return CPLAT_ERR_STORAGE_FULL;
                }
                hashtable_release_value(ht, rec);
                hashtable_value_store(ht, rec, value, value_storage_offset);
            }
            /* REVIVE はスロットに残っている削除前の値をそのまま使う。 */
            *hashtable_entry_status(ht, rec) = REC_IN_USE;
            stamp_record(ht, rec);
            ht->hdr->deleted_count--;
            ht->hdr->in_use_count++;
            store_inserted(inserted_out, 1);
            return CPLAT_OK;
        }
        cur = *hashtable_entry_next(ht, rec);
    }

    rec_no = ht->hdr->next_empty;
    if ((rec_no == 0) && (ht->hdr->config.reuse_deleted != 0))
    {
        rec_no = hashtable_find_best_deleted_record(ht, NULL);
    }
    if (rec_no == 0)
    {
        if (growth_out != NULL)
        {
            growth_out->pressure |= HASHTABLE_GROWTH_CAPACITY;
            if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
            {
                growth_out->key_storage_min = storage_min_for_transaction(
                    ht->hdr->key_storage_used, hashtable_field_input_size(ht->hdr->config.key_type, 0, key));
                if (growth_out->key_storage_min > ht->hdr->config.key_storage_size)
                {
                    growth_out->pressure |= HASHTABLE_GROWTH_KEY_STORAGE;
                }
            }
            if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
            {
                growth_out->value_storage_min = storage_min_for_transaction(
                    ht->hdr->value_storage_used, hashtable_field_input_size(ht->hdr->config.value_type, 0, value));
                if (growth_out->value_storage_min > ht->hdr->config.value_storage_size)
                {
                    growth_out->pressure |= HASHTABLE_GROWTH_VALUE_STORAGE;
                }
            }
        }
        return CPLAT_ERR_LIMIT_EXCEEDED;
    }

    rec = (size_t)(rec_no - 1);
    key_has_space = hashtable_key_storage_find_free(ht, rec, *hashtable_entry_status(ht, rec) != REC_EMPTY, key,
                                                    &key_storage_offset);
    value_has_space = hashtable_value_storage_find_free(ht, rec, *hashtable_entry_status(ht, rec) != REC_EMPTY, value,
                                                        &value_storage_offset);
    if (key_has_space == 0)
    {
        if (growth_out != NULL)
        {
            growth_out->pressure |= HASHTABLE_GROWTH_KEY_STORAGE;
            growth_out->key_storage_min = storage_min_for_transaction(
                ht->hdr->key_storage_used, hashtable_field_input_size(ht->hdr->config.key_type, 0, key));
        }
    }
    if (value_has_space == 0)
    {
        if (growth_out != NULL)
        {
            growth_out->pressure |= HASHTABLE_GROWTH_VALUE_STORAGE;
            growth_out->value_storage_min = storage_min_for_transaction(
                ht->hdr->value_storage_used, hashtable_field_input_size(ht->hdr->config.value_type, 0, value));
        }
    }
    if ((key_has_space == 0) || (value_has_space == 0))
    {
        return CPLAT_ERR_STORAGE_FULL;
    }
    if (*hashtable_entry_status(ht, rec) != REC_EMPTY)
    {
        hashtable_unlink_record(ht, rec); /* 削除中の既存キーをチェーンから外す。 */
        ht->hdr->deleted_count--;
        hashtable_release_key(ht, rec);
        hashtable_release_value(ht, rec);
    }
    hashtable_value_store(ht, rec, value, value_storage_offset);
    *hashtable_entry_status(ht, rec) = REC_IN_USE;
    hashtable_key_store(ht, rec, key, key_storage_offset);
    stamp_record(ht, rec);
    /* 新しいレコードをチェーン先頭へ挿す。 */
    *hashtable_entry_next(ht, rec) = bucket_head[idx];
    bucket_head[idx] = rec_no;
    if (rec_no == ht->hdr->next_empty) /* 空きスロットを使ったときだけ、次の空きを探し直す。 */
    {
        ht->hdr->next_empty = scan_next_empty(ht, rec + 1);
    }
    ht->hdr->in_use_count++;
    store_inserted(inserted_out, 1);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_add(cplat_hashtable *ht, const void *key, const void *value,
                        cplat_hashtable_add_deleted_policy deleted_policy)
{
    return hashtable_put_with_growth(ht, key, value, deleted_policy, 0, NULL);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_upsert(cplat_hashtable *ht, const void *key, const void *value, int *inserted_out)
{
    return hashtable_put_with_growth(ht, key, value, CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE, 1, inserted_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_insert_direct(cplat_hashtable *ht, uint64_t record, const void *key, int status, const void *value,
                                  const cplat_timespec *timestamp, uint64_t generation)
{
    size_t key_storage_offset = 0;
    size_t value_storage_offset = 0;
    size_t idx;
    uint64_t *bucket_head;
    uint64_t cur;
    size_t rec;

    if ((ht == NULL) || (key == NULL) || (value == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((input_points_into_table(ht, key) != 0) || (input_points_into_table(ht, value) != 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_has_record_timestamp(ht) != 0)
    {
        if (timestamp == NULL)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
    }
    else if ((timestamp != NULL) || (generation != 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((status < REC_IN_USE) || (status > CPLAT_HASHTABLE_LIFETIME_INFINITE))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (!hashtable_key_fits(ht, key))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }
    if (hashtable_fixed_string_fits(ht->hdr->config.value_type, ht->hdr->config.value_size, value) == 0)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    /* 有限寿命では、加齢後に存在し得ない status は書かずに省略する。 */
    if ((status >= REC_DELETED) && (ht->hdr->config.lifetime != CPLAT_HASHTABLE_LIFETIME_INFINITE) &&
        (status >= (int)ht->hdr->config.lifetime))
    {
        return CPLAT_SKIPPED;
    }

    rec = (size_t)(record - 1);
    if (*hashtable_entry_status(ht, rec) != REC_EMPTY)
    {
        return CPLAT_ERR_DUPLICATE_DEFINITION;
    }

    idx = hashtable_hash_key(ht, key);
    bucket_head = hashtable_bucket_head(ht);
    cur = bucket_head[idx];
    while (cur != 0)
    {
        size_t existing = (size_t)(cur - 1);

        if (hashtable_key_equal(ht, hashtable_entry_key(ht, existing), key))
        {
            return CPLAT_ERR_DUPLICATE_KEY;
        }
        cur = *hashtable_entry_next(ht, existing);
    }

    if ((hashtable_key_storage_find_free(ht, rec, 0, key, &key_storage_offset) == 0) ||
        (hashtable_value_storage_find_free(ht, rec, 0, value, &value_storage_offset) == 0))
    {
        return CPLAT_ERR_STORAGE_FULL;
    }
    hashtable_value_store(ht, rec, value, value_storage_offset);
    hashtable_key_store(ht, rec, key, key_storage_offset);
    *hashtable_entry_status(ht, rec) = (unsigned char)status;
    if (hashtable_has_record_timestamp(ht) != 0)
    {
        *hashtable_entry_timestamp(ht, rec) = *timestamp;
        if (cplat_timespec_cmp(timestamp, &ht->hdr->table_timestamp) > 0)
        {
            ht->hdr->table_timestamp = *timestamp;
        }
        *hashtable_entry_generation(ht, rec) = generation;
        if (generation > ht->hdr->table_generation)
        {
            ht->hdr->table_generation = generation;
        }
    }
    *hashtable_entry_next(ht, rec) = bucket_head[idx];
    bucket_head[idx] = record;
    /* 最小空きを使ったときだけ、次の空きを探し直す。 */
    if (ht->hdr->next_empty == record)
    {
        ht->hdr->next_empty = scan_next_empty(ht, rec + 1);
    }
    if (status == REC_IN_USE)
    {
        ht->hdr->in_use_count++;
    }
    else
    {
        ht->hdr->deleted_count++;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_update(cplat_hashtable *ht, const void *key, const void *value,
                     struct hashtable_growth_request *growth_out)
{
    size_t value_storage_offset = 0;
    size_t idx;
    uint64_t *bucket_head;
    uint64_t cur;

    if (growth_out != NULL)
    {
        memset(growth_out, 0, sizeof(*growth_out));
    }

    if ((ht == NULL) || (key == NULL) || (value == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (input_points_into_table(ht, value) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (!hashtable_key_fits(ht, key))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }
    if (hashtable_fixed_string_fits(ht->hdr->config.value_type, ht->hdr->config.value_size, value) == 0)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    idx = hashtable_hash_key(ht, key);
    bucket_head = hashtable_bucket_head(ht);
    cur = bucket_head[idx];
    while (cur != 0)
    {
        size_t rec = (size_t)(cur - 1);

        if (hashtable_key_equal(ht, hashtable_entry_key(ht, rec), key))
        {
            /* 削除済みは更新対象にしない。add のような再利用はしない。 */
            if (*hashtable_entry_status(ht, rec) != REC_IN_USE)
            {
                return CPLAT_ERR_NOT_FOUND;
            }
            if (hashtable_value_storage_find_free(ht, rec, 1, value, &value_storage_offset) == 0)
            {
                if (growth_out != NULL)
                {
                    growth_out->pressure |= HASHTABLE_GROWTH_VALUE_STORAGE;
                    growth_out->value_storage_min = storage_min_for_transaction(
                        ht->hdr->value_storage_used, hashtable_field_input_size(ht->hdr->config.value_type, 0, value));
                }
                return CPLAT_ERR_STORAGE_FULL;
            }
            hashtable_release_value(ht, rec);
            hashtable_value_store(ht, rec, value, value_storage_offset);
            stamp_record(ht, rec);
            return CPLAT_OK;
        }
        cur = *hashtable_entry_next(ht, rec);
    }
    return CPLAT_ERR_NOT_FOUND;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_update(cplat_hashtable *ht, const void *key, const void *value)
{
    return hashtable_update_with_growth(ht, key, value);
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_update_rec(cplat_hashtable *ht, uint64_t record, const void *value,
                         struct hashtable_growth_request *growth_out)
{
    size_t rec;
    size_t value_storage_offset = 0;

    if (growth_out != NULL)
    {
        memset(growth_out, 0, sizeof(*growth_out));
    }

    if ((ht == NULL) || (value == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (input_points_into_table(ht, value) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_fixed_string_fits(ht->hdr->config.value_type, ht->hdr->config.value_size, value) == 0)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    rec = (size_t)(record - 1);
    if (*hashtable_entry_status(ht, rec) != REC_IN_USE)
    {
        return CPLAT_ERR_NOT_FOUND;
    }
    if (hashtable_value_storage_find_free(ht, rec, 1, value, &value_storage_offset) == 0)
    {
        if (growth_out != NULL)
        {
            growth_out->pressure |= HASHTABLE_GROWTH_VALUE_STORAGE;
            growth_out->value_storage_min = storage_min_for_transaction(
                ht->hdr->value_storage_used, hashtable_field_input_size(ht->hdr->config.value_type, 0, value));
        }
        return CPLAT_ERR_STORAGE_FULL;
    }
    hashtable_release_value(ht, rec);
    hashtable_value_store(ht, rec, value, value_storage_offset);
    stamp_record(ht, rec);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_update_rec(cplat_hashtable *ht, uint64_t record, const void *value)
{
    return hashtable_update_rec_with_growth(ht, record, value);
}

/**
 *  @brief          スロットを空へ戻し、キーと値と変更時刻を消します。
 *  @param[in]      ht   対象。NULL を渡してはなりません。
 *  @param[in]      rec  0 相対のスロット インデックス。
 *
 *  チェーンからの切り離しはしません。呼び出し側が外します。\n
 *  SCOPE_RECORD のときは変更時刻も 0 埋めします。
 */
static void hashtable_expire_record(cplat_hashtable *ht, size_t rec)
{
    *hashtable_entry_status(ht, rec) = REC_EMPTY;
    hashtable_release_value(ht, rec);
    hashtable_release_key(ht, rec);
    if (hashtable_has_record_timestamp(ht) != 0)
    {
        memset(hashtable_entry_timestamp(ht, rec), 0, sizeof(cplat_timespec));
        *hashtable_entry_generation(ht, rec) = 0;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_delete(cplat_hashtable *ht, const void *key)
{
    size_t idx;
    uint64_t *link;

    if ((ht == NULL) || (key == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (!hashtable_key_fits(ht, key))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    idx = hashtable_hash_key(ht, key);
    link = &hashtable_bucket_head(ht)[idx];
    while (*link != 0)
    {
        size_t rec = (size_t)(*link - 1);

        if (hashtable_key_equal(ht, hashtable_entry_key(ht, rec), key))
        {
            if (*hashtable_entry_status(ht, rec) != REC_IN_USE)
            {
                return CPLAT_ERR_NOT_FOUND;
            }
            *hashtable_entry_status(ht, rec) = REC_DELETED;
            stamp_record(ht, rec);
            ht->hdr->in_use_count--;
            /* lifetime が 2 のときは、削除済みのまま置けないので直ちに空へ戻す。 */
            if (REC_DELETED >= ht->hdr->config.lifetime)
            {
                uint64_t rec_no = (uint64_t)(rec + 1);

                hashtable_expire_record(ht, rec);
                *link = *hashtable_entry_next(ht, rec);
                *hashtable_entry_next(ht, rec) = 0;
                if ((ht->hdr->next_empty == 0) || (rec_no < ht->hdr->next_empty))
                {
                    ht->hdr->next_empty = rec_no;
                }
            }
            else
            {
                ht->hdr->deleted_count++;
            }
            return CPLAT_OK;
        }
        link = hashtable_entry_next(ht, rec);
    }
    return CPLAT_ERR_NOT_FOUND;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_delete_rec(cplat_hashtable *ht, uint64_t record)
{
    size_t rec;

    if (ht == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((record == 0) || (record > ht->hdr->config.capacity))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    rec = (size_t)(record - 1);
    if (*hashtable_entry_status(ht, rec) != REC_IN_USE)
    {
        return CPLAT_ERR_NOT_FOUND;
    }
    return cplat_hashtable_delete(ht, hashtable_entry_key(ht, rec));
}

/**
 *  @brief          空きになったスロットを全バケットのチェーンから外します。
 *  @param[in]      ht  対象。NULL を渡してはなりません。
 *
 *  expire はチェーンを触らないため、加齢や一括回収のあとで呼びます。
 */
static void hashtable_unlink_empty_chains(cplat_hashtable *ht)
{
    uint64_t *bucket_head = hashtable_bucket_head(ht);
    size_t i;

    for (i = 0; i < ht->hdr->config.capacity; i++)
    {
        uint64_t *link = &bucket_head[i];

        while (*link != 0)
        {
            size_t rec = (size_t)(*link - 1);

            if (*hashtable_entry_status(ht, rec) == REC_EMPTY)
            {
                *link = *hashtable_entry_next(ht, rec);
                *hashtable_entry_next(ht, rec) = 0;
            }
            else
            {
                link = hashtable_entry_next(ht, rec);
            }
        }
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_push_deleted(cplat_hashtable *ht)
{
    uint64_t new_next_empty = 0;
    uint64_t expired_count = 0;
    size_t i;

    if (ht == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    for (i = 0; i < ht->hdr->config.capacity; i++)
    {
        unsigned char *status = hashtable_entry_status(ht, i);

        /* 255 は増やさない。無限寿命では空へ戻さない。 */
        if ((*status >= REC_DELETED) && (*status < CPLAT_HASHTABLE_LIFETIME_INFINITE))
        {
            (*status)++;
            if ((*status >= ht->hdr->config.lifetime) &&
                (ht->hdr->config.lifetime != CPLAT_HASHTABLE_LIFETIME_INFINITE))
            {
                hashtable_expire_record(ht, i);
                expired_count++;
            }
        }
        if ((new_next_empty == 0) && (*status == REC_EMPTY))
        {
            new_next_empty = (uint64_t)(i + 1);
        }
    }

    hashtable_unlink_empty_chains(ht);
    ht->hdr->next_empty = new_next_empty;
    ht->hdr->deleted_count -= expired_count;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_purge_deleted(cplat_hashtable *ht)
{
    uint64_t new_next_empty = 0;
    size_t i;

    if (ht == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    for (i = 0; i < ht->hdr->config.capacity; i++)
    {
        /* 加齢せず、削除済みをすべて空へ戻す。255 も含める。 */
        if (*hashtable_entry_status(ht, i) >= REC_DELETED)
        {
            hashtable_expire_record(ht, i);
        }
        if ((new_next_empty == 0) && (*hashtable_entry_status(ht, i) == REC_EMPTY))
        {
            new_next_empty = (uint64_t)(i + 1);
        }
    }

    hashtable_unlink_empty_chains(ht);
    ht->hdr->next_empty = new_next_empty;
    ht->hdr->deleted_count = 0; /* 削除済みは無条件で全て空へ戻すため。 */
    return CPLAT_OK;
}
