/**
 *******************************************************************************
 *  @file           hashtable_layout.c
 *  @brief          ハッシュ テーブルの領域レイアウトを算出します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
 *  @version        1.0.0
 *
 *  あふれ検査付きの算術、エントリのオフセットとストライド、管理領域とデータ領域の
 *  バイト数を求めます。\n
 *  算出結果を内部管理データへ控える @ref hashtable_refresh_layout も本ファイルが持ちます。\n
 *  配置そのものの説明は hashtable.h を参照してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "hashtable.h"

#include <stdint.h>

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_add_checked(size_t a, size_t b, size_t *sum_out)
{
    if (b > SIZE_MAX - a)
    {
        return 1;
    }
    *sum_out = a + b;
    return 0;
}

/**
 *  @brief          乗算のあふれを検出します。
 *  @param[in]      a    乗数。
 *  @param[in]      b    乗数。
 *  @param[out]     product_out  積の格納先。あふれ時は書きません。
 *  @return         成功なら 0、あふれなら 1 です。
 */
static int mul_checked(size_t a, size_t b, size_t *product_out)
{
    /* a が 0 なら積は 0 で、SIZE_MAX / a は未定義になるため検査しない。 */
    if ((a != 0) && (b > SIZE_MAX / a))
    {
        return 1;
    }
    *product_out = a * b;
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_align_up_checked(size_t offset, size_t alignment, size_t *aligned_out)
{
    size_t rem = offset % alignment;
    size_t pad;
    if (rem == 0)
    {
        pad = 0;
    }
    else
    {
        pad = (alignment - rem);
    }

    return hashtable_add_checked(offset, pad, aligned_out);
}

/**
 *  @brief          エントリ先頭からキーまでのオフセットを返します。
 *  @param[in]      config  設定。NULL を渡してはなりません。
 *  @return         キーのバイト オフセットです。
 *
 *  SCOPE_RECORD では next + status + pad + timespec + generation のあとです。\n
 *  SCOPE_TABLE では next + status の直後です。\n
 *  可変長キーは 16 バイトの永続 descriptor を置くため、uint64_t 境界へ切り上げます。
 *  切り上げないと descriptor が境界から外れ、未整列アクセスになります。
 */
static size_t entry_key_offset(const cplat_hashtable_config *config)
{
    size_t off = sizeof(uint64_t) + sizeof(unsigned char);
    size_t rem;

    if (config->timestamp_scope == CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD)
    {
        off += (size_t)ENTRY_TIMESTAMP_PAD + sizeof(cplat_timespec) + sizeof(uint64_t);
    }
    if (config->key_type != CPLAT_HASHTABLE_FIELD_VARIABLE_STRING)
    {
        return off;
    }
    /* off は最大でも 40 のため、切り上げであふれません。 */
    rem = off % _Alignof(struct hashtable_string_ref);
    if (rem != 0)
    {
        off += _Alignof(struct hashtable_string_ref) - rem;
    }
    return off;
}

static size_t key_slot_size(const cplat_hashtable_config *config)
{
    if (hashtable_field_is_variable(config->key_type) != 0)
    {
        return sizeof(struct hashtable_string_ref);
    }
    else
    {
        return config->key_size;
    }
}

/**
 *  @brief          固定長値 1 件が占めるバイト数を求めます。
 *  @param[in]      config      設定。NULL を渡してはなりません。
 *  @param[out]     stride_out  ストライドの格納先。あふれ時は書きません。
 *  @return         成功なら 0、あふれなら 1 です。
 *
 *  value_align が 0 のときは value_size をそのまま返し、値を隙間なく並べます。\n
 *  非 0 のときは value_size をその境界へ切り上げ、各値を境界へ整列させます。
 */
static int value_stride_checked(const cplat_hashtable_config *config, size_t *stride_out)
{
    if (config->value_align == 0)
    {
        *stride_out = config->value_size;
        return 0;
    }
    return hashtable_align_up_checked(config->value_size, config->value_align, stride_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t hashtable_data_region_align(const cplat_hashtable_config *config)
{
    if (hashtable_field_is_variable(config->value_type) != 0)
    {
        return _Alignof(struct hashtable_string_ref);
    }
    if (config->value_align == 0)
    {
        return 1u;
    }
    return config->value_align;
}

/**
 *  @brief          1 エントリのストライドを求めます。
 *  @param[in]      config  設定。NULL を渡してはなりません。
 *  @param[out]     stride_out  ストライドの格納先。あふれ時は書きません。
 *  @return         成功なら 0、あふれなら 1 です。
 *
 *  中身は next + status + (SCOPE_RECORD なら pad と timespec) + key を
 *  uint64_t 境界へ切り上げた値です。
 */
static int entry_stride_checked(const cplat_hashtable_config *config, size_t *stride_out)
{
    size_t raw;

    if (hashtable_add_checked(entry_key_offset(config), key_slot_size(config), &raw) != 0)
    {
        return 1;
    }
    return hashtable_align_up_checked(raw, _Alignof(uint64_t), stride_out);
}

/**
 *  @brief          空きリストのバイト数を求めます。
 *  @param[in]      config    設定。NULL を渡してはなりません。
 *  @param[out]     size_out  バイト数の格納先。NULL を渡してはなりません。
 *  @return         成功なら 0、あふれなら -1 です。
 *
 *  空きブロックは使用中ブロックで区切られるため、個数の上限は capacity + 1 です。\n
 *  可変長フィールドのときだけ領域を確保します。
 */
static int hashtable_free_list_region_size(const cplat_hashtable_config *config, size_t *size_out)
{
    size_t count;

    if (hashtable_add_checked(config->capacity, 1u, &count) != 0)
    {
        return -1;
    }
    if (mul_checked(count, sizeof(struct hashtable_free_block), size_out) != 0)
    {
        return -1;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_mgmt_layout(const cplat_hashtable_config *config, size_t *off_bucket_head, size_t *off_entries,
                          size_t *mgmt_size_out)
{
    size_t offset = sizeof(struct hashtable_persist_header);
    size_t region_size;
    size_t entry_stride;

    /* ヘッダー サイズは uint64_t 境界の倍数(構造体定義直後の _Static_assert で保証)。整列不要。 */

    /* あふれ検査より前に仮値で初期化し、早期 return でも呼び出し側へ未初期化値を渡さない。
       呼び出し側の大半は戻り値を捨てるため、この関数の契約として out パラメーターは
       常に定義済みの値を持つ必要がある。 */
    if (off_bucket_head != NULL)
    {
        *off_bucket_head = offset;
    }
    if (off_entries != NULL)
    {
        *off_entries = offset;
    }
    if (mgmt_size_out != NULL)
    {
        *mgmt_size_out = offset;
    }
    if (mul_checked(config->capacity, sizeof(uint64_t), &region_size) != 0)
    {
        return -1;
    }
    if (hashtable_add_checked(offset, region_size, &offset) != 0)
    {
        return -1;
    }

    if (entry_stride_checked(config, &entry_stride) != 0)
    {
        return -1;
    }

    /* 直前の offset は uint64_t 境界の倍数同士の和のため、既に境界上にある。整列不要。 */

    if (off_entries != NULL)
    {
        *off_entries = offset;
    }
    if (mul_checked(config->capacity, entry_stride, &region_size) != 0)
    {
        return -1;
    }
    if (hashtable_add_checked(offset, region_size, &offset) != 0)
    {
        return -1;
    }
    /* 空きリストはキー ストレージの直前に置く。キー ストレージは管理領域の末尾のままとする。 */
    if (hashtable_field_is_variable(config->key_type) != 0)
    {
        if (hashtable_free_list_region_size(config, &region_size) != 0)
        {
            return -1;
        }
        if (hashtable_add_checked(offset, region_size, &offset) != 0)
        {
            return -1;
        }
    }

    if (hashtable_add_checked(offset, config->key_storage_size, &offset) != 0)
    {
        return -1;
    }

    if (mgmt_size_out != NULL)
    {
        *mgmt_size_out = offset;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int hashtable_data_region_size(const cplat_hashtable_config *config, size_t *data_size_out)
{
    size_t size;
    size_t free_list_size;

    if (hashtable_field_is_variable(config->value_type) == 0)
    {
        size_t stride;

        if (value_stride_checked(config, &stride) != 0)
        {
            return -1;
        }
        if (mul_checked(config->capacity, stride, data_size_out) != 0)
        {
            return -1;
        }
        return 0;
    }
    if (mul_checked(config->capacity, sizeof(struct hashtable_string_ref), &size) != 0)
    {
        return -1;
    }
    /* 空きリストは値ストレージの直前に置く。値ストレージはデータ領域の末尾のままとする。 */
    if (hashtable_free_list_region_size(config, &free_list_size) != 0)
    {
        return -1;
    }
    if (hashtable_add_checked(size, free_list_size, &size) != 0)
    {
        return -1;
    }
    if (hashtable_add_checked(size, config->value_storage_size, data_size_out) != 0)
    {
        return -1;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void hashtable_refresh_layout(cplat_hashtable *ht)
{
    const cplat_hashtable_config *config = &ht->hdr->config;

    /* 構築済みテーブルの設定はあふれないため、算出は失敗しない。
       算出前に 0 で初期化し、万一の失敗時も未初期化値を残さない。 */
    ht->off_bucket_head = 0;
    ht->off_entries = 0;
    ht->mgmt_size = 0;
    ht->entry_stride = 0;
    ht->value_stride = 0;
    ht->free_list_size = 0;

    (void)hashtable_mgmt_layout(config, &ht->off_bucket_head, &ht->off_entries, &ht->mgmt_size);
    (void)entry_stride_checked(config, &ht->entry_stride);
    (void)value_stride_checked(config, &ht->value_stride);
    (void)hashtable_free_list_region_size(config, &ht->free_list_size);
    ht->key_offset = entry_key_offset(config);
    ht->key_is_variable = (unsigned char)(hashtable_field_is_variable(config->key_type) != 0);
    ht->value_is_variable = (unsigned char)(hashtable_field_is_variable(config->value_type) != 0);
}
