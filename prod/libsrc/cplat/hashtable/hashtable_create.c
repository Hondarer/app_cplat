/**
 *******************************************************************************
 *  @file           hashtable_create.c
 *  @brief          ハッシュ テーブルの構築、接続、破棄を担います。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
 *  @version        1.0.0
 *
 *  設定の検査、必要バイト数の算出、領域の確保と初期化、既存領域への接続、
 *  設定と領域の参照、内部管理データの解放を担います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "hashtable.h"

#include <cplat/crt/stdlib.h>
#include <string.h>

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_validate_config(const cplat_hashtable_config *config)
{
    if (config->capacity == 0)
    {
        return -1;
    }
    if ((config->key_type < CPLAT_HASHTABLE_FIELD_FIXED_BINARY) ||
        (config->key_type > CPLAT_HASHTABLE_FIELD_VARIABLE_STRING) ||
        (config->value_type < CPLAT_HASHTABLE_FIELD_FIXED_BINARY) ||
        (config->value_type > CPLAT_HASHTABLE_FIELD_VARIABLE_STRING))
    {
        return -1;
    }
    if ((config->timestamp_scope != CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE) &&
        (config->timestamp_scope != CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD))
    {
        return -1;
    }
    if (((hashtable_field_is_variable(config->key_type) != 0) &&
         ((config->key_size != 0) || (config->key_storage_size == 0))) ||
        ((hashtable_field_is_variable(config->key_type) == 0) &&
         ((config->key_size == 0) || (config->key_storage_size != 0))) ||
        ((hashtable_field_is_variable(config->value_type) != 0) &&
         ((config->value_size != 0) || (config->value_storage_size == 0))) ||
        ((hashtable_field_is_variable(config->value_type) == 0) &&
         ((config->value_size == 0) || (config->value_storage_size != 0))))
    {
        return -1;
    }
    if (hashtable_field_is_variable(config->value_type) != 0)
    {
        /* 可変長値はストレージ内オフセットで位置が決まるため、境界を保証できない。 */
        if (config->value_align != 0)
        {
            return -1;
        }
    }
    else if (config->value_align != 0)
    {
        if ((config->value_align > (size_t)CPLAT_HASHTABLE_VALUE_ALIGN_MAX) ||
            ((config->value_align & (config->value_align - 1u)) != 0))
        {
            return -1;
        }
    }
    /* REC_DELETED が 2 なので、寿命は削除直後に空へ戻す 2 以上が必要。 */
    if (config->lifetime < 2)
    {
        return -1;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_required_size(const cplat_hashtable_config *config, size_t *mgmt_size_out, size_t *data_size_out)
{
    size_t mgmt_size;
    size_t data_size;

    if ((config == NULL) || ((mgmt_size_out == NULL) && (data_size_out == NULL)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_validate_config(config) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_mgmt_layout(config, NULL, NULL, &mgmt_size) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_data_region_size(config, &data_size) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (mgmt_size_out != NULL)
    {
        *mgmt_size_out = mgmt_size;
    }
    if (data_size_out != NULL)
    {
        *data_size_out = data_size;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_create(const cplat_hashtable_config *config, void *buf_mgmt, size_t buf_mgmt_size, void *buf_data,
                           size_t buf_data_size, cplat_hashtable **ht_out)
{
    size_t mgmt_size;
    size_t data_size;
    struct hashtable_persist_header *hdr;
    unsigned char owns_buffer;
    unsigned char *data;
    cplat_hashtable *ht;

    if (ht_out != NULL)
    {
        *ht_out = NULL;
    }
    if ((ht_out == NULL) || (config == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_validate_config(config) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    /* 管理領域とデータ領域は、両方 NULL(内部確保)か両方非 NULL(外部指定)のいずれかに限る。 */
    if ((buf_mgmt == NULL) != (buf_data == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (hashtable_mgmt_layout(config, NULL, NULL, &mgmt_size) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (hashtable_data_region_size(config, &data_size) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (buf_mgmt == NULL)
    {
        size_t data_offset;
        size_t total_size;

        /* 管理領域の直後を、データ領域が必要とする境界へ切り上げる。 */
        if (hashtable_align_up_checked(mgmt_size, hashtable_data_region_align(config), &data_offset) != 0)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
        if (hashtable_add_checked(data_offset, data_size, &total_size) != 0)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
        hdr = (struct hashtable_persist_header *)cplat_calloc(1, total_size);
        if (hdr == NULL)
        {
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        data = (unsigned char *)hdr + data_offset;
        owns_buffer = 1;
    }
    else
    {
        if (buf_mgmt_size < mgmt_size)
        {
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
        /* 管理領域の先頭アドレスは、呼び出し側が満たすべき引数条件です。 */
        if (((uintptr_t)buf_mgmt % _Alignof(struct hashtable_persist_header)) != 0)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
        /* 可変長値の descriptor、および value_align が非 0 の値は境界を要求する。 */
        if (((uintptr_t)buf_data % hashtable_data_region_align(config)) != 0)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
        if (buf_data_size < data_size)
        {
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
        memset(buf_mgmt, 0, mgmt_size);
        memset(buf_data, 0, data_size);
        hdr = (struct hashtable_persist_header *)buf_mgmt;
        data = (unsigned char *)buf_data;
        owns_buffer = 0;
    }

    /* 内部管理データは、永続化領域・データ領域とは別に確保する。 */
    ht = (cplat_hashtable *)cplat_calloc(1, sizeof(struct cplat_hashtable));
    if (ht == NULL)
    {
        if (owns_buffer != 0)
        {
            cplat_free(hdr);
        }
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    hdr->magic = CPLAT_HASHTABLE_MAGIC;
    hdr->version = CPLAT_HASHTABLE_VERSION;
    hdr->config.capacity = config->capacity;
    hdr->config.key_type = config->key_type;
    hdr->config.value_type = config->value_type;
    hdr->config.timestamp_scope = config->timestamp_scope;
    hdr->config.key_size = config->key_size;
    hdr->config.value_size = config->value_size;
    hdr->config.key_storage_size = config->key_storage_size;
    hdr->config.value_storage_size = config->value_storage_size;
    hdr->config.value_align = config->value_align;
    hdr->config.lifetime = config->lifetime;
    hdr->config.reuse_deleted = config->reuse_deleted;
    hdr->next_empty = 1; /* 全スロット空きなので、最小空きはレコード 1。 */
    hdr->in_use_count = 0;
    hdr->deleted_count = 0;
    hdr->table_generation = 0;

    ht->hdr = hdr;
    ht->data = data;
    ht->owns_buffer = owns_buffer;
    /* アクセサーが参照するレイアウトの控えを作る。以降の走査より前に必要。 */
    hashtable_refresh_layout(ht);
    /* 可変長ストレージ全体を 1 個の空きブロックとして登録する。 */
    hashtable_reset_arenas(ht);

    *ht_out = ht;
    return CPLAT_OK;
}

static int growth_config_is_valid(const cplat_hashtable_config *initial_config,
                                  const cplat_hashtable_growth_config *growth_config)
{
    if ((growth_config->max_capacity != 0) && (growth_config->max_capacity < initial_config->capacity))
    {
        return 0;
    }
    if (hashtable_field_is_variable(initial_config->key_type) == 0)
    {
        if (growth_config->max_key_storage_size != 0)
        {
            return 0;
        }
    }
    else if ((growth_config->max_key_storage_size != 0) &&
             (growth_config->max_key_storage_size < initial_config->key_storage_size))
    {
        return 0;
    }
    if (hashtable_field_is_variable(initial_config->value_type) == 0)
    {
        if (growth_config->max_value_storage_size != 0)
        {
            return 0;
        }
    }
    else if ((growth_config->max_value_storage_size != 0) &&
             (growth_config->max_value_storage_size < initial_config->value_storage_size))
    {
        return 0;
    }
    return 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_create_growable(const cplat_hashtable_config *initial_config,
                                    const cplat_hashtable_growth_config *growth_config, cplat_hashtable **ht_out)
{
    cplat_hashtable *ht = NULL;
    int ret;

    if (ht_out != NULL)
    {
        *ht_out = NULL;
    }
    if ((initial_config == NULL) || (growth_config == NULL) || (ht_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((hashtable_validate_config(initial_config) != 0) ||
        (growth_config_is_valid(initial_config, growth_config) == 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_hashtable_create(initial_config, NULL, 0, NULL, 0, &ht);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ht->growth = *growth_config;
    ht->growable = 1;
    *ht_out = ht;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_config_ref(const cplat_hashtable *ht, const cplat_hashtable_config **config_out)
{
    if ((ht == NULL) || (config_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *config_out = &ht->hdr->config;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_get_config_val(const cplat_hashtable *ht, cplat_hashtable_config *config_out)
{
    const cplat_hashtable_config *src;
    int ret;

    if (config_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_hashtable_get_config_ref(ht, &src);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *config_out = *src;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_buffer_size(const cplat_hashtable *ht, size_t *mgmt_size_out, size_t *data_size_out)
{
    if ((ht == NULL) || ((mgmt_size_out == NULL) && (data_size_out == NULL)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (mgmt_size_out != NULL)
    {
        (void)hashtable_mgmt_layout(&ht->hdr->config, NULL, NULL, mgmt_size_out);
    }
    if (data_size_out != NULL)
    {
        (void)hashtable_data_region_size(&ht->hdr->config, data_size_out);
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_buffer_ref(const cplat_hashtable *ht, const void **mgmt_out, const void **data_out)
{
    if ((ht == NULL) || ((mgmt_out == NULL) && (data_out == NULL)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (mgmt_out != NULL)
    {
        *mgmt_out = hashtable_bytes(ht);
    }
    if (data_out != NULL)
    {
        *data_out = hashtable_data(ht);
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_hashtable_attach(void *buf_mgmt, size_t buf_mgmt_size, void *buf_data, size_t buf_data_size,
                           cplat_hashtable **ht_out)
{
    struct hashtable_persist_header *hdr;
    cplat_hashtable *ht;
    size_t mgmt_size;
    size_t data_size;

    if (ht_out != NULL)
    {
        *ht_out = NULL;
    }
    if ((ht_out == NULL) || (buf_mgmt == NULL) || (buf_data == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    /* マジックを読むために、まず永続化ヘッダー分だけを要求する。 */
    if (buf_mgmt_size < sizeof(struct hashtable_persist_header))
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    if (((uintptr_t)buf_mgmt % _Alignof(struct hashtable_persist_header)) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    hdr = (struct hashtable_persist_header *)buf_mgmt;

    if (hdr->magic != CPLAT_HASHTABLE_MAGIC)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    if (hdr->version != CPLAT_HASHTABLE_VERSION)
    {
        return CPLAT_ERR_VERSION_MISMATCH;
    }
    if (hashtable_validate_config(&hdr->config) != 0)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    if (((uintptr_t)buf_data % hashtable_data_region_align(&hdr->config)) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    /* 0 は満杯の正当値。capacity 超えだけを拒否する。 */
    if (hdr->next_empty > hdr->config.capacity)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    /* 減算方向で判定し、あふれを避ける。 */
    if (hdr->in_use_count > hdr->config.capacity)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    if (hdr->deleted_count > hdr->config.capacity - hdr->in_use_count)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    if ((hdr->key_storage_used > hdr->config.key_storage_size) ||
        (hdr->value_storage_used > hdr->config.value_storage_size))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    /* 空きリストの走査が領域外へ出ないよう、個数の上限だけは先に検査する。 */
    if ((hdr->key_free_count > (uint64_t)hdr->config.capacity + 1u) ||
        (hdr->value_free_count > (uint64_t)hdr->config.capacity + 1u))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    if (hashtable_mgmt_layout(&hdr->config, NULL, NULL, &mgmt_size) != 0)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    if (buf_mgmt_size < mgmt_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    if (hashtable_data_region_size(&hdr->config, &data_size) != 0)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    if (buf_data_size < data_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* 内部管理データは、永続化領域・データ領域とは別に確保する。 */
    ht = (cplat_hashtable *)cplat_calloc(1, sizeof(struct cplat_hashtable));
    if (ht == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    ht->hdr = hdr;
    /* data は実行時のみ有効なアドレスのため、呼び出し側が渡した値を必ず使う。 */
    ht->data = (unsigned char *)buf_data;
    /* 再接続後の所有は常に呼び出し側。永続化領域とデータ領域は dispose で解放しない。 */
    ht->owns_buffer = 0;
    /* アクセサーが参照するレイアウトの控えを作る。 */
    hashtable_refresh_layout(ht);
    *ht_out = ht;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_hashtable_dispose(cplat_hashtable *ht)
{
    if (ht == NULL)
    {
        return;
    }
    /* 内部確保時は永続化領域とデータ領域が1ブロックのため、hdr の解放で両方片付く。 */
    if (ht->owns_buffer != 0)
    {
        cplat_free(ht->hdr);
    }
    /* 内部管理データは、生成経路によらず常に解放する。 */
    cplat_free(ht);
}
