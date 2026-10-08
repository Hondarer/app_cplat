/**
 *******************************************************************************
 *  @file           bench_mmap.c
 *  @brief          メモリ マップド ファイル API による各アクセス パターンの測定処理を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/29
 *  @version        1.0.0
 *
 *  アタッチの扱いで 3 形態を実装します。\n
 *  @ref BENCH_API_MMAP_ONCE は測定ループの外で 1 回だけアタッチし、
 *  @ref BENCH_API_MMAP_EACH は反復ごとにアタッチとデタッチを行います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <stdlib.h>
#include <string.h>

#include <cplat/crt/stdlib.h>
#include <cplat/mmap/mmap.h>

#include "bench_case.h"

/**
 *  @brief  アタッチ済みハンドルを測定ループ間で保持するための状態です。
 */
typedef struct mmap_state
{
    cplat_mmap *map; /**< @ref BENCH_API_MMAP_ONCE で保持するハンドル。 */
} mmap_state;

/**
 *  @brief          レコードから集計値を求めます。
 *  @param[in]      record  対象のレコード。NULL を渡してはなりません。
 *  @return         集計値。
 */
static uint64_t accumulate(const bench_record *record)
{
    return record->id + (uint64_t)(uint32_t)record->value_a + (uint64_t)(uint32_t)record->value_b;
}

/**
 *  @brief          書き込み系のアクセス パターンであるかを判定します。
 *  @param[in]      pattern  アクセス パターン。
 *  @return         書き込みを伴う場合は 1、それ以外は 0 を返します。
 */
static int is_write_pattern(bench_pattern pattern)
{
    if (pattern == BENCH_PATTERN_SEQ_WRITE || pattern == BENCH_PATTERN_RAND_UPDATE)
    {
        return 1;
    }
    return 0;
}

/**
 *  @brief          マップ済み領域に対してアクセス パターンを 1 回実行します。
 *  @param[in,out]  ctx   共有状態。NULL を渡してはなりません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 *  @param[in,out]  map   アタッチ済みハンドル。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int access_mapped(bench_context *ctx, const bench_case *item, cplat_mmap *map)
{
    bench_record *records = (bench_record *)cplat_mmap_get_address(map);
    size_t index;
    int result = 0;

    if (records == NULL)
    {
        return -1;
    }

    switch (item->pattern)
    {
    case BENCH_PATTERN_SEQ_READ:
        for (index = 0U; index < ctx->touch_count; index++)
        {
            ctx->checksum += accumulate(&records[index]);
        }
        break;
    case BENCH_PATTERN_SEQ_WRITE:
        for (index = 0U; index < ctx->touch_count; index++)
        {
            bench_fill_record(index, &records[index]);
        }
        break;
    case BENCH_PATTERN_RAND_READ:
        for (index = 0U; index < ctx->touch_count; index++)
        {
            ctx->checksum += accumulate(&records[ctx->order[index]]);
        }
        break;
    case BENCH_PATTERN_RAND_UPDATE:
        for (index = 0U; index < ctx->touch_count; index++)
        {
            bench_record *target = &records[ctx->order[index]];

            target->counter++;
            ctx->checksum += accumulate(target);
        }
        break;
    case BENCH_PATTERN_POINT_LOOKUP:
        ctx->checksum += accumulate(&records[ctx->order[0]]);
        break;
    case BENCH_PATTERN_OPEN_CLOSE:
        ctx->checksum++;
        break;
    case BENCH_PATTERN_COUNT:
    default:
        result = -1;
        break;
    }

    if (result == 0 && item->durable != 0 && is_write_pattern(item->pattern) != 0)
    {
        if (cplat_mmap_flush(map, NULL, 0U, NULL) != CPLAT_OK)
        {
            result = -1;
        }
    }
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int bench_mmap_setup(bench_context *ctx, const bench_case *item)
{
    mmap_state *state;

    if (ctx == NULL || item == NULL)
    {
        return -1;
    }
    if (item->api != BENCH_API_MMAP_ONCE)
    {
        ctx->state = NULL;
        return 0;
    }

    state = (mmap_state *)cplat_malloc_zerofill(sizeof(*state));
    if (state == NULL)
    {
        return -1;
    }
    if (cplat_mmap_attach(ctx->path, CPLAT_MMAP_ACCESS_READ_WRITE, ctx->file_size, &state->map, NULL) != CPLAT_OK)
    {
        cplat_free(state);
        return -1;
    }
    ctx->state = state;
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int bench_mmap_iterate(bench_context *ctx, const bench_case *item)
{
    cplat_mmap *map = NULL;
    int result;

    if (ctx == NULL || item == NULL)
    {
        return -1;
    }

    if (item->api == BENCH_API_MMAP_ONCE)
    {
        mmap_state *state = (mmap_state *)ctx->state;

        return access_mapped(ctx, item, state->map);
    }

    if (cplat_mmap_attach(ctx->path, CPLAT_MMAP_ACCESS_READ_WRITE, ctx->file_size, &map, NULL) != CPLAT_OK)
    {
        return -1;
    }

    result = access_mapped(ctx, item, map);

    (void)cplat_mmap_detach(map, NULL);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

void bench_mmap_teardown(bench_context *ctx, const bench_case *item)
{
    mmap_state *state;

    if (ctx == NULL || item == NULL)
    {
        return;
    }
    state = (mmap_state *)ctx->state;
    if (state == NULL)
    {
        return;
    }
    (void)cplat_mmap_detach(state->map, NULL);
    cplat_free(state);
    ctx->state = NULL;
}
