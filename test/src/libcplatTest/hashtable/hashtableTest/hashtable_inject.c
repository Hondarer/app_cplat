/**
 *  @file           hashtable_inject.c
 *  @brief          hashtable の内部状態へテストから到達するための注入コードです。
 *
 *  実装のモジュール私有ヘッダーを取り込み、内部アクセサーと永続化ヘッダーを
 *  テスト用の外部リンケージ関数として公開します。
 */

#include "hashtable.h" /* prod/libsrc/cplat/hashtable のモジュール私有ヘッダー */

#include "hashtable_inject.h"

uint64_t *test_hashtable_bucket_head(const cplat_hashtable *ht)
{
    return hashtable_bucket_head(ht);
}

unsigned char *test_hashtable_entry_status(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_entry_status(ht, rec);
}

uint64_t *test_hashtable_entry_next(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_entry_next(ht, rec);
}

char *test_hashtable_entry_key(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_entry_key(ht, rec);
}

uint32_t *test_hashtable_entry_key_length(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_entry_key_length(ht, rec);
}

void *test_hashtable_key_ref_at(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_key_ref_at(ht, rec);
}

void *test_hashtable_value_ref_at(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_value_ref_at(ht, rec);
}

uint64_t *test_hashtable_entry_generation(const cplat_hashtable *ht, size_t rec)
{
    return hashtable_entry_generation(ht, rec);
}

void test_hashtable_set_next_empty(cplat_hashtable *ht, uint64_t value)
{
    ht->hdr->next_empty = value;
}

void test_hashtable_set_config(cplat_hashtable *ht, const cplat_hashtable_config *config)
{
    ht->hdr->config = *config;
    /* アクセサーが参照するレイアウトの控えを、書き換えた設定へ合わせる。 */
    hashtable_refresh_layout(ht);
}

void test_hashtable_set_counts(cplat_hashtable *ht, uint64_t in_use_count, uint64_t deleted_count)
{
    ht->hdr->in_use_count = in_use_count;
    ht->hdr->deleted_count = deleted_count;
}

void *test_hashtable_value_free_list(const cplat_hashtable *ht)
{
    return hashtable_value_free_list(ht);
}

uint64_t test_hashtable_value_free_count(const cplat_hashtable *ht)
{
    return ht->hdr->value_free_count;
}

void test_hashtable_set_value_free_count(cplat_hashtable *ht, uint64_t count)
{
    ht->hdr->value_free_count = count;
}
