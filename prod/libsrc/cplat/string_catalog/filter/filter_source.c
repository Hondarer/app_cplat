/**
 *******************************************************************************
 *  @file           filter_source.c
 *  @brief          フィルター オブジェクトを受け渡すソース領域の公開と読み取りを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/03
 *  @version        0.1.0
 *
 *  公開と読み取りは seqlock と同じ手順です。書き込み側は版番号を奇数にしてから内容を書き、
 *  最後に新しい偶数の値を書き込みます。読み取り側はロックを取らず、読み取りの前後で版番号を比べます。\n
 *  see: https://www.hpl.hp.com/techreports/2012/HPL-2012-68.pdf
 *
 *  内容の読み取りは書き込みと重なり得ます。重なった内容は版番号の比較で捨て、
 *  比較をすり抜けた破損もフィルター オブジェクトのハッシュ値の検証で拒否します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#include "filter.h"

#include <cplat/base/result.h>
#include <cplat/clock/clock.h>
#include <cplat/runtime/process.h>

#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(string_catalog_filter_source_header) == CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE,
               "cplat: source header size");

/** FNV-1a 64 ビットの初期値です。カタログの識別値の計算に使います。 */
#define CATALOG_ID_OFFSET_BASIS 14695981039346656037ULL

/** FNV-1a 64 ビットの乗数です。 */
#define CATALOG_ID_PRIME 1099511628211ULL

/** NULL の文字列を表す長さです。空の文字列と区別するために使います。 */
#define CATALOG_ID_NULL_LENGTH 0xFFFFFFFFU

/** 版番号のうち、書き込み中を表すビットです。 */
#define REVISION_WRITING_BIT 1ULL

/** 版番号が上限を超えた場合に戻る値です。0 は未公開を表すため、0 でない最小の偶数とします。 */
#define REVISION_WRAPPED 2ULL

/** ヘッダーが、0 で埋まった未公開の領域か、本ライブラリの形式の領域であるかを確かめます。 */
static int check_known_header(const string_catalog_filter_source_header *header)
{
    if (header->signature == 0U)
    {
        return CPLAT_OK;
    }
    return string_catalog_filter_source_check_header(header);
}

/**
 *  @brief          前回の版番号から、新しい版番号を求めます。
 *  @param[in]      base 前回の版番号 (偶数)。未公開の場合は 0。
 *  @return         0 でない偶数で、@p base と異なる値。
 *
 *  前回の値に 2 を加えます。書き込み側どうしは排他で直列化するため、同じ版番号を 2 回公開しません。\n
 *  ファイルをマップした領域は OS の再起動を越えて残りますが、版番号は時刻に依存しないため、前回の値から増やし続けます。
 *
 *  前回の値に 2 を加えると 64 ビットを超える場合は、@ref REVISION_WRAPPED へ戻します。\n
 *  読み取り側は大小ではなく不一致で変化を判定するため、戻った値でも取り込みます。
 *  1 秒に 10 億回公開しても上限に届くには約 292 年かかるため、通常は、壊れた値や意図的に大きくした値でだけ起こります。
 */
static uint64_t next_revision(const uint64_t base)
{
    if (base <= (UINT64_MAX - 3U))
    {
        return base + 2U;
    }
    return REVISION_WRAPPED;
}

/* Doxygen コメントは、ヘッダーに記載 */

int string_catalog_filter_source_check_header(const string_catalog_filter_source_header *header)
{
    /* 署名だけでは別の形式と破損を区別できないため、署名の不一致は破損として扱う */
    if (header->signature != STRING_CATALOG_FILTER_SOURCE_SIGNATURE)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    if (header->format_version != STRING_CATALOG_FILTER_SOURCE_FORMAT_VERSION)
    {
        return CPLAT_ERR_VERSION_MISMATCH;
    }
    if (header->header_size != CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    return CPLAT_OK;
}

/** バイト列を識別値のハッシュへ加えます。 */
static uint64_t hash_bytes(uint64_t hash, const unsigned char *bytes, const size_t length)
{
    for (size_t index = 0; index < length; index++)
    {
        hash ^= bytes[index];
        hash *= CATALOG_ID_PRIME;
    }
    return hash;
}

/** 32 ビットの値を、実行環境のバイト順序によらずリトル エンディアンの 4 バイトとしてハッシュへ加えます。 */
static uint64_t hash_u32(const uint64_t hash, const uint32_t value)
{
    const unsigned char bytes[4] = {(unsigned char)(value & 0xFFU), (unsigned char)((value >> 8) & 0xFFU),
                                    (unsigned char)((value >> 16) & 0xFFU), (unsigned char)((value >> 24) & 0xFFU)};

    return hash_bytes(hash, bytes, sizeof(bytes));
}

/** 文字列を、長さと本体としてハッシュへ加えます。NULL は空の文字列と区別します。 */
static uint64_t hash_text(const uint64_t hash, const char *text)
{
    if (text == NULL)
    {
        return hash_u32(hash, CATALOG_ID_NULL_LENGTH);
    }
    return hash_bytes(hash_u32(hash, (uint32_t)strlen(text)), (const unsigned char *)text, strlen(text));
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_get_catalog_id(const cplat_string_catalog *catalog, uint64_t *catalog_id_out)
{
    uint64_t hash = CATALOG_ID_OFFSET_BASIS;

    if ((catalog == NULL) || (catalog_id_out == NULL) || (catalog->entry_count < 0) ||
        ((catalog->entry_count > 0) && (catalog->entries == NULL)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 判定に関わる定義だけを使う。書式や説明文を変えても、条件式の解釈は変わらないため */
    hash = hash_u32(hash, (uint32_t)catalog->entry_count);
    for (int entry_index = 0; entry_index < catalog->entry_count; entry_index++)
    {
        const cplat_string_catalog_entry *entry = &catalog->entries[entry_index];
        int argument_count;
        if ((entry->argument_count > 0) && (entry->arguments != NULL))
        {
            argument_count = entry->argument_count;
        }
        else
        {
            argument_count = 0;
        }

        hash = hash_u32(hash, (uint32_t)entry->key);
        hash = hash_u32(hash, (uint32_t)entry->category);
        hash = hash_text(hash, entry->id);
        hash = hash_u32(hash, (uint32_t)argument_count);
        for (int argument = 0; argument < argument_count; argument++)
        {
            hash = hash_u32(hash, (uint32_t)entry->arguments[argument].kind);
            hash = hash_text(hash, entry->arguments[argument].name);
        }
    }

    /* 0 は識別値を持たない以前の版の領域を表すため、使わない */
    if (hash == 0U)
    {
        *catalog_id_out = 1U;
    }
    else
    {
        *catalog_id_out = hash;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

bool string_catalog_filter_source_is_region_valid(const void *source, const size_t source_size, const size_t image_size)
{
    if ((source == NULL) || (((uintptr_t)source % CPLAT_STRING_CATALOG_FILTER_SOURCE_ALIGNMENT) != 0U))
    {
        return false;
    }
    return (source_size >= CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE) &&
           ((source_size - CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE) >= image_size);
}

/* Doxygen コメントは、ヘッダーに記載 */

uint64_t string_catalog_filter_source_begin_read(const void *source)
{
    const string_catalog_filter_source_header *header = (const string_catalog_filter_source_header *)source;

    return cplat_atomic_load_u64(&header->published_revision, CPLAT_MEMORY_ORDER_ACQUIRE);
}

/* Doxygen コメントは、ヘッダーに記載 */

bool string_catalog_filter_source_end_read(const void *source, const uint64_t revision)
{
    const string_catalog_filter_source_header *header = (const string_catalog_filter_source_header *)source;

    /* 内容の読み取りが、版番号の読み直しより後へ並べ替わらないようにする */
    cplat_atomic_thread_fence(CPLAT_MEMORY_ORDER_ACQUIRE);
    return cplat_atomic_load_u64(&header->published_revision, CPLAT_MEMORY_ORDER_RELAXED) == revision;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_source_publish(void *source, const size_t source_size, const void *image,
                                               const size_t image_size, const uint64_t catalog_id,
                                               const cplat_string_catalog_filter_source_lock *lock,
                                               uint64_t *revision_out)
{
    string_catalog_filter_source_header *header = (string_catalog_filter_source_header *)source;
    cplat_string_catalog_filter_info info;
    cplat_timespec realtime;
    uint64_t base;
    uint64_t next;
    int ret;

    if ((source == NULL) || (image == NULL) || !string_catalog_filter_source_is_region_valid(source, source_size, 0U) ||
        (catalog_id == 0U) || ((lock != NULL) && ((lock->lock == NULL) || (lock->unlock == NULL))))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_string_catalog_filter_validate(image, image_size);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    (void)cplat_string_catalog_filter_get_info(image, image_size, &info);
    if (!string_catalog_filter_source_is_region_valid(source, source_size, (size_t)info.image_size))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    /* ヘッダーの確認と書き込みは排他の下で行う。ほかの書き込み側と競合せずにヘッダーを読めるようにするため */
    if (lock != NULL)
    {
        ret = lock->lock(lock->context);
        if (ret != CPLAT_OK)
        {
            return ret;
        }
    }
    ret = check_known_header(header);
    if (ret != CPLAT_OK)
    {
        if (lock != NULL)
        {
            lock->unlock(lock->context);
        }
        return ret;
    }

    /* 書き込みの途中で中断した領域では版番号が奇数のまま残るため、偶数へ戻して基準にする */
    base = cplat_atomic_load_u64(&header->published_revision, CPLAT_MEMORY_ORDER_RELAXED) & ~REVISION_WRITING_BIT;

    /* 版番号を奇数にしてから書き込む。内容の書き込みが、奇数にするより前へ並べ替わらないようにする */
    cplat_atomic_store_u64(&header->published_revision, base | REVISION_WRITING_BIT, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_atomic_thread_fence(CPLAT_MEMORY_ORDER_RELEASE);

    cplat_clock_get_realtime(&realtime);
    header->signature = STRING_CATALOG_FILTER_SOURCE_SIGNATURE;
    header->format_version = (uint16_t)STRING_CATALOG_FILTER_SOURCE_FORMAT_VERSION;
    header->header_size = (uint16_t)CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE;
    header->line_capacity = info.line_capacity;
    header->line_width = info.line_width;
    header->image_size = info.image_size;
    header->published_realtime_seconds = (int64_t)realtime.tv_sec;
    header->published_realtime_nanoseconds = realtime.tv_nsec;
    header->publisher_process_id = cplat_process_get_pid();
    header->reserved = 0U;
    header->catalog_id = catalog_id;
    memcpy((unsigned char *)source + CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE, image, (size_t)info.image_size);

    next = next_revision(base);
    cplat_atomic_store_u64(&header->published_revision, next, CPLAT_MEMORY_ORDER_RELEASE);
    if (lock != NULL)
    {
        lock->unlock(lock->context);
    }

    if (revision_out != NULL)
    {
        *revision_out = next;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_source_get_info(const void *source, const size_t source_size,
                                                cplat_string_catalog_filter_source_info *info_out)
{
    const string_catalog_filter_source_header *header = (const string_catalog_filter_source_header *)source;
    string_catalog_filter_source_header copy;
    uint64_t revision;
    int ret;

    if ((info_out == NULL) || !string_catalog_filter_source_is_region_valid(source, source_size, 0U))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    memset(info_out, 0, sizeof(*info_out));

    revision = string_catalog_filter_source_begin_read(source);
    if (revision == 0U)
    {
        return CPLAT_OK;
    }
    if ((revision & REVISION_WRITING_BIT) != 0U)
    {
        return CPLAT_ERR_BUSY;
    }
    memcpy(&copy, header, sizeof(copy));
    if (!string_catalog_filter_source_end_read(source, revision))
    {
        return CPLAT_ERR_BUSY;
    }
    ret = string_catalog_filter_source_check_header(&copy);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    info_out->published_revision = revision;
    info_out->published_realtime.tv_sec = (time_t)copy.published_realtime_seconds;
    info_out->published_realtime.tv_nsec = copy.published_realtime_nanoseconds;
    info_out->publisher_process_id = copy.publisher_process_id;
    info_out->line_capacity = copy.line_capacity;
    info_out->line_width = copy.line_width;
    info_out->catalog_id = copy.catalog_id;
    return CPLAT_OK;
}
