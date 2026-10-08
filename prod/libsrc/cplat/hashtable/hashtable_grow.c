/**
 *******************************************************************************
 *  @file           hashtable_grow.c
 *  @brief          容量とストレージの拡張、および再構築を担います。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
 *  @version        1.0.0
 *
 *  移行計画の作成と適用、明示的な再設定、別領域への再構築、詰め直し、消去と、
 *  更新系 API からの自動拡張を担います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "hashtable.h"

#include <cplat/crt/stdlib.h>
#include <string.h>

/**
 *  @brief          設定のうち、伸長・縮小で変えてよい項目以外が一致するかを判定します。
 *  @param[in]      current  現在の設定。NULL を渡してはなりません。
 *  @param[in]      next     移行後の設定。NULL を渡してはなりません。
 *  @return         一致すれば 1、異なれば 0 です。
 *
 *  変えてよいのは capacity、key_storage_size、value_storage_size の 3 つだけです。
 *  フィールド形式やレイアウト要件まで変えるのは create と insert_direct の役割です。
 */
static int hashtable_config_is_compatible(const cplat_hashtable_config *current, const cplat_hashtable_config *next)
{
    if ((current->key_type != next->key_type) || (current->value_type != next->value_type))
    {
        return 0;
    }
    if ((current->key_size != next->key_size) || (current->value_size != next->value_size))
    {
        return 0;
    }
    if ((current->value_align != next->value_align) || (current->timestamp_scope != next->timestamp_scope))
    {
        return 0;
    }
    if ((current->lifetime != next->lifetime) || (current->reuse_deleted != next->reuse_deleted))
    {
        return 0;
    }
    return 1;
}

/**
 *  @brief          移行で残すレコードを決めます。
 *  @param[in]      src         移行元。NULL を渡してはなりません。
 *  @param[in]      new_config  移行後の設定。NULL を渡してはなりません。
 *  @param[out]     keep_out    capacity 個のマスクの格納先。成功時は呼び出し側が解放します。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
 *                  @ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_STORAGE_FULL 。
 *
 *  移行元を変更しません。移行を始める前に、収まるかどうかだけを判定します。\n
 *  使用中のレコードは必ず残します。削除済みのレコードは、reuse_deleted が 0 なら
 *  必ず残し、収まらなければ失敗します。reuse_deleted が非 0 なら、add の追い出しと
 *  同じ規則で古い順に除外します。
 */
static int hashtable_plan_migration(const cplat_hashtable *src, const cplat_hashtable_config *new_config,
                                    unsigned char **keep_out)
{
    size_t src_capacity = src->hdr->config.capacity;
    size_t new_capacity = new_config->capacity;
    unsigned char *keep;
    size_t kept = 0;
    size_t key_used = 0;
    size_t value_used = 0;
    size_t i;

    keep = (unsigned char *)cplat_calloc(src_capacity, sizeof(unsigned char));
    if (keep == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    for (i = 0; i < src_capacity; i++)
    {
        if (*hashtable_entry_status(src, i) != REC_EMPTY)
        {
            keep[i] = 1;
            kept++;
        }
    }

    if (src->hdr->in_use_count > (uint64_t)new_capacity)
    {
        cplat_free(keep);
        return CPLAT_ERR_LIMIT_EXCEEDED;
    }
    if (kept > new_capacity)
    {
        /* reuse_deleted が 0 の設定は、削除済みを自動的には手放さないという意思表示。 */
        if (src->hdr->config.reuse_deleted == 0)
        {
            cplat_free(keep);
            return CPLAT_ERR_LIMIT_EXCEEDED;
        }
        while (kept > new_capacity)
        {
            uint64_t victim = hashtable_find_best_deleted_record(src, keep);

            if (victim == 0)
            {
                cplat_free(keep);
                return CPLAT_ERR_LIMIT_EXCEEDED;
            }
            keep[victim - 1u] = 0;
            kept--;
        }
    }

    for (i = 0; i < src_capacity; i++)
    {
        if (keep[i] == 0)
        {
            continue;
        }
        if (hashtable_field_is_variable(src->hdr->config.key_type) != 0)
        {
            key_used += (size_t)hashtable_key_ref_at(src, i)->length;
        }
        if (hashtable_field_is_variable(src->hdr->config.value_type) != 0)
        {
            value_used += (size_t)hashtable_value_ref_at(src, i)->length;
        }
    }
    if ((key_used > new_config->key_storage_size) || (value_used > new_config->value_storage_size))
    {
        cplat_free(keep);
        return CPLAT_ERR_STORAGE_FULL;
    }

    *keep_out = keep;
    return CPLAT_OK;
}

/**
 *  @brief          決めたレコードを移行先へ書き込みます。
 *  @param[in]      src   移行元。NULL を渡してはなりません。
 *  @param[in,out]  dst   移行先。構築直後の空テーブルを渡してください。
 *  @param[in]      keep  @ref hashtable_plan_migration が決めたマスク。
 *  @return         @ref CPLAT_OK 、または @ref cplat_hashtable_insert_direct の失敗値。
 *
 *  拡大 (移行先の capacity が移行元以上) ではレコード番号をそのまま写します。\n
 *  縮小では 1 から順に詰めて番号を振り直します。\n
 *  変更時刻と世代カウンターは、レコードもテーブルも移行元の値を引き継ぎます。
 */
static int hashtable_apply_migration(const cplat_hashtable *src, cplat_hashtable *dst, const unsigned char *keep)
{
    size_t src_capacity = src->hdr->config.capacity;
    int preserve_record = 0;
    uint64_t next_record = 1;
    size_t i;

    if (dst->hdr->config.capacity >= src_capacity)
    {
        preserve_record = 1;
    }

    for (i = 0; i < src_capacity; i++)
    {
        const cplat_timespec *timestamp = NULL;
        uint64_t generation = 0;
        uint64_t record;
        int status;
        int ret;

        if (keep[i] == 0)
        {
            continue;
        }
        status = (int)*hashtable_entry_status(src, i);
        if (hashtable_has_record_timestamp(src) != 0)
        {
            timestamp = hashtable_entry_timestamp(src, i);
            generation = *hashtable_entry_generation(src, i);
        }
        if (preserve_record != 0)
        {
            record = (uint64_t)(i + 1);
        }
        else
        {
            record = next_record;
            next_record++;
        }
        ret = cplat_hashtable_insert_direct(dst, record, hashtable_entry_key(src, i), status, hashtable_data_at(src, i),
                                            timestamp, generation);
        if (ret != CPLAT_OK)
        {
            return ret;
        }
    }

    dst->hdr->table_timestamp = src->hdr->table_timestamp;
    dst->hdr->table_generation = src->hdr->table_generation;
    return CPLAT_OK;
}

static int hashtable_growth_target(size_t current, size_t minimum, size_t maximum, size_t *target_out)
{
    size_t doubled;
    size_t target;

    if ((maximum != 0) && (minimum > maximum))
    {
        return 0;
    }
    if (minimum <= current)
    {
        *target_out = current;
        return 1;
    }
    if (current > (SIZE_MAX / 2u))
    {
        doubled = SIZE_MAX;
    }
    else
    {
        doubled = current * 2u;
    }
    if (doubled < minimum)
    {
        target = minimum;
    }
    else
    {
        target = doubled;
    }
    if ((maximum != 0) && (target > maximum))
    {
        target = maximum;
    }
    if (target < minimum)
    {
        return 0;
    }
    *target_out = target;
    return 1;
}

static int hashtable_stage_growth(const cplat_hashtable *src, const struct hashtable_growth_request *growth,
                                  int pressure_error, cplat_hashtable **staged_out)
{
    cplat_hashtable_config next = src->hdr->config;
    cplat_hashtable *staged = NULL;
    unsigned char *keep = NULL;
    size_t minimum;
    int ret;

    if ((growth->pressure & HASHTABLE_GROWTH_CAPACITY) != 0u)
    {
        if (src->hdr->config.capacity == SIZE_MAX)
        {
            return pressure_error;
        }
        minimum = src->hdr->config.capacity + 1u;
        if (hashtable_growth_target(src->hdr->config.capacity, minimum, src->growth.max_capacity, &next.capacity) == 0)
        {
            return pressure_error;
        }
    }
    if ((growth->pressure & HASHTABLE_GROWTH_KEY_STORAGE) != 0u)
    {
        if (hashtable_growth_target(src->hdr->config.key_storage_size, growth->key_storage_min,
                                    src->growth.max_key_storage_size, &next.key_storage_size) == 0)
        {
            return pressure_error;
        }
    }
    if ((growth->pressure & HASHTABLE_GROWTH_VALUE_STORAGE) != 0u)
    {
        if (hashtable_growth_target(src->hdr->config.value_storage_size, growth->value_storage_min,
                                    src->growth.max_value_storage_size, &next.value_storage_size) == 0)
        {
            return pressure_error;
        }
    }

    ret = hashtable_plan_migration(src, &next, &keep);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = cplat_hashtable_create(&next, NULL, 0, NULL, 0, &staged);
    if (ret != CPLAT_OK)
    {
        cplat_free(keep);
        if (ret == CPLAT_ERR_INVALID_ARGUMENT)
        {
            return pressure_error;
        }
        else
        {
            return ret;
        }
    }
    ret = hashtable_apply_migration(src, staged, keep);
    cplat_free(keep);
    if (ret != CPLAT_OK)
    {
        cplat_hashtable_dispose(staged);
        return ret;
    }
    *staged_out = staged;
    return CPLAT_OK;
}

static void hashtable_commit_staged(cplat_hashtable *ht, cplat_hashtable *staged)
{
    struct hashtable_persist_header *old_hdr = ht->hdr;

    ht->hdr = staged->hdr;
    ht->data = staged->data;
    /* 設定が変わったため、レイアウトの控えを作り直す。 */
    hashtable_refresh_layout(ht);
    cplat_free(old_hdr);
    cplat_free(staged);
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_put_with_growth(cplat_hashtable *ht, const void *key, const void *value,
                              cplat_hashtable_add_deleted_policy deleted_policy, int allow_update, int *inserted_out)
{
    struct hashtable_growth_request growth;
    cplat_hashtable *staged = NULL;
    int ret = hashtable_put(ht, key, value, deleted_policy, allow_update, inserted_out, &growth);

    if ((ret == CPLAT_OK) || (ht == NULL) || (ht->growable == 0) || (growth.pressure == HASHTABLE_GROWTH_NONE))
    {
        return ret;
    }
    ret = hashtable_stage_growth(ht, &growth, ret, &staged);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = hashtable_put(staged, key, value, deleted_policy, allow_update, inserted_out, NULL);
    if (ret != CPLAT_OK)
    {
        cplat_hashtable_dispose(staged);
        return ret;
    }
    hashtable_commit_staged(ht, staged);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_update_with_growth(cplat_hashtable *ht, const void *key, const void *value)
{
    struct hashtable_growth_request growth;
    cplat_hashtable *staged = NULL;
    int ret = hashtable_update(ht, key, value, &growth);

    if ((ret == CPLAT_OK) || (ht == NULL) || (ht->growable == 0) || (growth.pressure == HASHTABLE_GROWTH_NONE))
    {
        return ret;
    }
    ret = hashtable_stage_growth(ht, &growth, ret, &staged);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = hashtable_update(staged, key, value, NULL);
    if (ret != CPLAT_OK)
    {
        cplat_hashtable_dispose(staged);
        return ret;
    }
    hashtable_commit_staged(ht, staged);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_update_rec_with_growth(cplat_hashtable *ht, uint64_t record, const void *value)
{
    struct hashtable_growth_request growth;
    cplat_hashtable *staged = NULL;
    int ret = hashtable_update_rec(ht, record, value, &growth);

    if ((ret == CPLAT_OK) || (ht == NULL) || (ht->growable == 0) || (growth.pressure == HASHTABLE_GROWTH_NONE))
    {
        return ret;
    }
    ret = hashtable_stage_growth(ht, &growth, ret, &staged);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = hashtable_update_rec(staged, record, value, NULL);
    if (ret != CPLAT_OK)
    {
        cplat_hashtable_dispose(staged);
        return ret;
    }
    hashtable_commit_staged(ht, staged);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_resize(cplat_hashtable *ht, const cplat_hashtable_config *new_config)
{
    cplat_hashtable *staged = NULL;
    unsigned char *keep = NULL;
    struct hashtable_persist_header *old_hdr;
    int ret;

    if ((ht == NULL) || (new_config == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (ht->owns_buffer == 0)
    {
        return CPLAT_ERR_UNSUPPORTED;
    }
    if (hashtable_validate_config(new_config) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_config_is_compatible(&ht->hdr->config, new_config) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((ht->growable != 0) &&
        (((ht->growth.max_capacity != 0) && (new_config->capacity > ht->growth.max_capacity)) ||
         ((ht->growth.max_key_storage_size != 0) && (new_config->key_storage_size > ht->growth.max_key_storage_size)) ||
         ((ht->growth.max_value_storage_size != 0) &&
          (new_config->value_storage_size > ht->growth.max_value_storage_size))))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = hashtable_plan_migration(ht, new_config, &keep);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = cplat_hashtable_create(new_config, NULL, 0, NULL, 0, &staged);
    if (ret != CPLAT_OK)
    {
        cplat_free(keep);
        return ret;
    }
    ret = hashtable_apply_migration(ht, staged, keep);
    cplat_free(keep);
    if (ret != CPLAT_OK)
    {
        cplat_hashtable_dispose(staged);
        return ret;
    }

    /* 移行が終わってから旧領域を解放する。失敗時は元のテーブルを変更しない。 */
    old_hdr = ht->hdr;
    ht->hdr = staged->hdr;
    ht->data = staged->data;
    /* 設定が変わったため、レイアウトの控えを作り直す。 */
    hashtable_refresh_layout(ht);
    cplat_free(old_hdr);
    /* 領域は ht が引き継いだため、staged は内部管理データだけを解放する。 */
    cplat_free(staged);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_rebuild_into(const cplat_hashtable *src, const cplat_hashtable_config *new_config, void *buf_mgmt,
                                 size_t buf_mgmt_size, void *buf_data, size_t buf_data_size, cplat_hashtable **ht_out)
{
    cplat_hashtable *dst = NULL;
    unsigned char *keep = NULL;
    int ret;

    if (ht_out != NULL)
    {
        *ht_out = NULL;
    }
    if ((src == NULL) || (new_config == NULL) || (ht_out == NULL) || (buf_mgmt == NULL) || (buf_data == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_validate_config(new_config) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_config_is_compatible(&src->hdr->config, new_config) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = hashtable_plan_migration(src, new_config, &keep);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = cplat_hashtable_create(new_config, buf_mgmt, buf_mgmt_size, buf_data, buf_data_size, &dst);
    if (ret != CPLAT_OK)
    {
        cplat_free(keep);
        return ret;
    }
    ret = hashtable_apply_migration(src, dst, keep);
    cplat_free(keep);
    if (ret != CPLAT_OK)
    {
        cplat_hashtable_dispose(dst);
        return ret;
    }

    *ht_out = dst;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_compact(cplat_hashtable *ht)
{
    if (ht == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_field_is_variable(ht->hdr->config.key_type) != 0)
    {
        struct hashtable_arena arena;

        hashtable_key_arena(ht, &arena);
        hashtable_arena_compact(ht, &arena, hashtable_key_ref_at, ht->hdr->key_storage_used);
    }
    if (hashtable_field_is_variable(ht->hdr->config.value_type) != 0)
    {
        struct hashtable_arena arena;

        hashtable_value_arena(ht, &arena);
        hashtable_arena_compact(ht, &arena, hashtable_value_ref_at, ht->hdr->value_storage_used);
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_clear(cplat_hashtable *ht)
{
    size_t off_bucket_head;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    if (ht == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    (void)hashtable_mgmt_layout(&ht->hdr->config, &off_bucket_head, NULL, &mgmt_size);
    (void)hashtable_data_region_size(&ht->hdr->config, &data_size);
    /* 識別子、設定、テーブル時刻は残し、バケットとエントリだけを消す。 */
    memset(hashtable_bytes(ht) + off_bucket_head, 0, mgmt_size - off_bucket_head);
    /* 値配列(データ領域)も 0 埋めする。ポインター自体(ht->data)は変更しない。 */
    memset(ht->data, 0, data_size);
    ht->hdr->next_empty = 1;
    ht->hdr->in_use_count = 0;
    ht->hdr->deleted_count = 0;
    ht->hdr->key_storage_used = 0;
    ht->hdr->value_storage_used = 0;
    hashtable_reset_arenas(ht);
    hashtable_stamp_table(ht);
    return CPLAT_OK;
}
