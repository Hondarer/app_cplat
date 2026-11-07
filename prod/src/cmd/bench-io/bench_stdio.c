/**
 *******************************************************************************
 *  @file           bench_stdio.c
 *  @brief          `stdio` ラッパー API による各アクセス パターンの測定処理を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/29
 *  @version        1.0.0
 *
 *  レコード単位の読み書きと、ブロック単位でまとめた読み書きの 2 形態を実装します。\n
 *  `cplat` には `setvbuf` のラッパーがないため、ブロック単位の制御は
 *  自前バッファーへの読み書きで行い、CRT のバッファー サイズは既定のままとします。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>

#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>

#include "bench_case.h"

/**
 *  @brief  ブロック単位アクセスで使用する作業バッファーです。
 */
typedef struct stdio_state
{
    bench_record *block; /**< ブロック単位の読み書きに使うバッファー。 */
} stdio_state;

/**
 *  @brief          レコードから集計値を求めます。
 *  @param[in]      record  対象のレコード。NULL を渡してはなりません。
 *  @return         集計値。
 *
 *  読み取った内容が最適化で除去されないよう、呼び出し側で加算して保持します。
 */
static uint64_t accumulate(const bench_record *record)
{
    return record->id + (uint64_t)(uint32_t)record->value_a + (uint64_t)(uint32_t)record->value_b;
}

/**
 *  @brief          レコード単位で逐次読み取りを行います。
 *  @param[in,out]  ctx  共有状態。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int seq_read_by_record(bench_context *ctx)
{
    FILE *stream = cplat_fopen(ctx->path, "rb", NULL);
    size_t index;

    if (stream == NULL)
    {
        return -1;
    }

    for (index = 0U; index < ctx->touch_count; index++)
    {
        bench_record record;

        if (fread(&record, sizeof(record), 1U, stream) != 1U)
        {
            (void)fclose(stream);
            return -1;
        }

        ctx->checksum += accumulate(&record);
    }

    (void)fclose(stream);

    return 0;
}

/**
 *  @brief          ブロック単位で逐次読み取りを行います。
 *  @param[in,out]  ctx    共有状態。NULL を渡してはなりません。
 *  @param[in,out]  block  作業バッファー。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int seq_read_by_block(bench_context *ctx, bench_record *block)
{
    FILE *stream = cplat_fopen(ctx->path, "rb", NULL);
    size_t remaining = ctx->touch_count;

    if (stream == NULL)
    {
        return -1;
    }

    while (remaining > 0U)
    {
        size_t chunk = (size_t)BENCH_BLOCK_RECORDS;
        size_t index;

        if (chunk > remaining)
        {
            chunk = remaining;
        }

        if (fread(block, sizeof(*block), chunk, stream) != chunk)
        {
            (void)fclose(stream);
            return -1;
        }

        for (index = 0U; index < chunk; index++)
        {
            ctx->checksum += accumulate(&block[index]);
        }

        remaining -= chunk;
    }

    (void)fclose(stream);

    return 0;
}

/**
 *  @brief          レコード単位で逐次書き込みを行います。
 *  @param[in,out]  ctx      共有状態。NULL を渡してはなりません。
 *  @param[in]      durable  書き込み後に `fflush` を行う場合は 1。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int seq_write_by_record(bench_context *ctx, int durable)
{
    FILE *stream = cplat_fopen(ctx->path, "r+b", NULL);
    size_t index;

    if (stream == NULL)
    {
        return -1;
    }

    for (index = 0U; index < ctx->touch_count; index++)
    {
        bench_record record;

        bench_fill_record(index, &record);

        if (fwrite(&record, sizeof(record), 1U, stream) != 1U)
        {
            (void)fclose(stream);
            return -1;
        }
    }

    /*
     * fsync 相当は行わない。cplat にそのラッパーがない。
     * 正本は app/cplat/docs/fileio-api-selection-guideline.md の「耐久性を要求した場合」節である。
     */
    if (durable != 0 && fflush(stream) != 0)
    {
        (void)fclose(stream);
        return -1;
    }

    (void)fclose(stream);

    return 0;
}

/**
 *  @brief          ブロック単位で逐次書き込みを行います。
 *  @param[in,out]  ctx      共有状態。NULL を渡してはなりません。
 *  @param[in,out]  block    作業バッファー。NULL を渡してはなりません。
 *  @param[in]      durable  書き込み後に `fflush` を行う場合は 1。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int seq_write_by_block(bench_context *ctx, bench_record *block, int durable)
{
    FILE *stream = cplat_fopen(ctx->path, "r+b", NULL);
    size_t written = 0U;

    if (stream == NULL)
    {
        return -1;
    }

    while (written < ctx->touch_count)
    {
        size_t chunk = ctx->touch_count - written;
        size_t index;

        if (chunk > (size_t)BENCH_BLOCK_RECORDS)
        {
            chunk = (size_t)BENCH_BLOCK_RECORDS;
        }

        for (index = 0U; index < chunk; index++)
        {
            bench_fill_record(written + index, &block[index]);
        }

        if (fwrite(block, sizeof(*block), chunk, stream) != chunk)
        {
            (void)fclose(stream);
            return -1;
        }

        written += chunk;
    }

    /*
     * fsync 相当は行わない。cplat にそのラッパーがない。
     * 正本は app/cplat/docs/fileio-api-selection-guideline.md の「耐久性を要求した場合」節である。
     */
    if (durable != 0 && fflush(stream) != 0)
    {
        (void)fclose(stream);
        return -1;
    }

    (void)fclose(stream);

    return 0;
}

/**
 *  @brief          乱数順にレコードを読み取ります。
 *  @param[in,out]  ctx  共有状態。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int rand_read(bench_context *ctx)
{
    FILE *stream = cplat_fopen(ctx->path, "rb", NULL);
    size_t index;

    if (stream == NULL)
    {
        return -1;
    }

    for (index = 0U; index < ctx->touch_count; index++)
    {
        bench_record record;
        int64_t offset = (int64_t)(ctx->order[index] * sizeof(record));

        if (cplat_fseek(stream, offset, SEEK_SET) != 0)
        {
            (void)fclose(stream);
            return -1;
        }

        if (fread(&record, sizeof(record), 1U, stream) != 1U)
        {
            (void)fclose(stream);
            return -1;
        }

        ctx->checksum += accumulate(&record);
    }

    (void)fclose(stream);

    return 0;
}

/**
 *  @brief          乱数順にレコードを read-modify-write します。
 *  @param[in,out]  ctx      共有状態。NULL を渡してはなりません。
 *  @param[in]      durable  書き込み後に `fflush` を行う場合は 1。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int rand_update(bench_context *ctx, int durable)
{
    FILE *stream = cplat_fopen(ctx->path, "r+b", NULL);
    size_t index;

    if (stream == NULL)
    {
        return -1;
    }

    for (index = 0U; index < ctx->touch_count; index++)
    {
        bench_record record;
        int64_t offset = (int64_t)(ctx->order[index] * sizeof(record));

        if (cplat_fseek(stream, offset, SEEK_SET) != 0)
        {
            (void)fclose(stream);
            return -1;
        }

        if (fread(&record, sizeof(record), 1U, stream) != 1U)
        {
            (void)fclose(stream);
            return -1;
        }

        record.counter++;
        ctx->checksum += accumulate(&record);

        if (cplat_fseek(stream, offset, SEEK_SET) != 0)
        {
            (void)fclose(stream);
            return -1;
        }

        if (fwrite(&record, sizeof(record), 1U, stream) != 1U)
        {
            (void)fclose(stream);
            return -1;
        }
    }

    /*
     * fsync 相当は行わない。cplat にそのラッパーがない。
     * 正本は app/cplat/docs/fileio-api-selection-guideline.md の「耐久性を要求した場合」節である。
     */
    if (durable != 0 && fflush(stream) != 0)
    {
        (void)fclose(stream);
        return -1;
    }

    (void)fclose(stream);

    return 0;
}

/**
 *  @brief          ファイルを開いて 1 レコードだけ読み、閉じます。
 *  @param[in,out]  ctx  共有状態。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int point_lookup(bench_context *ctx)
{
    FILE *stream = cplat_fopen(ctx->path, "rb", NULL);
    bench_record record;
    int64_t offset = (int64_t)(ctx->order[0] * sizeof(record));

    if (stream == NULL)
    {
        return -1;
    }

    if (cplat_fseek(stream, offset, SEEK_SET) != 0)
    {
        (void)fclose(stream);
        return -1;
    }

    if (fread(&record, sizeof(record), 1U, stream) != 1U)
    {
        (void)fclose(stream);
        return -1;
    }

    ctx->checksum += accumulate(&record);
    (void)fclose(stream);

    return 0;
}

/**
 *  @brief          ファイルを開いて閉じるだけの処理を行います。
 *  @param[in,out]  ctx  共有状態。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int open_close(bench_context *ctx)
{
    FILE *stream = cplat_fopen(ctx->path, "rb", NULL);

    if (stream == NULL)
    {
        return -1;
    }

    ctx->checksum++; /* 読み取るレコードがなく、最適化による除去を防ぐ */
    (void)fclose(stream);

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int bench_stdio_setup(bench_context *ctx, const bench_case *item)
{
    stdio_state *state;

    if (ctx == NULL || item == NULL)
    {
        return -1;
    }

    if (item->api != BENCH_API_STDIO_BLK)
    {
        ctx->state = NULL;
        return 0;
    }

    state = (stdio_state *)cplat_malloc_zerofill(sizeof(*state));
    if (state == NULL)
    {
        return -1;
    }

    state->block = (bench_record *)cplat_calloc((size_t)BENCH_BLOCK_RECORDS, sizeof(bench_record));
    if (state->block == NULL)
    {
        cplat_free(state);
        return -1;
    }

    ctx->state = state;

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int bench_stdio_iterate(bench_context *ctx, const bench_case *item)
{
    stdio_state *state;
    int result;

    if (ctx == NULL || item == NULL)
    {
        return -1;
    }

    state = (stdio_state *)ctx->state;

    switch (item->pattern)
    {
    case BENCH_PATTERN_SEQ_READ:
        if (item->api == BENCH_API_STDIO_BLK)
        {
            result = seq_read_by_block(ctx, state->block);
        }
        else
        {
            result = seq_read_by_record(ctx);
        }

        break;
    case BENCH_PATTERN_SEQ_WRITE:
        if (item->api == BENCH_API_STDIO_BLK)
        {
            result = seq_write_by_block(ctx, state->block, item->durable);
        }
        else
        {
            result = seq_write_by_record(ctx, item->durable);
        }

        break;
    case BENCH_PATTERN_RAND_READ:
        result = rand_read(ctx);
        break;
    case BENCH_PATTERN_RAND_UPDATE:
        result = rand_update(ctx, item->durable);
        break;
    case BENCH_PATTERN_POINT_LOOKUP:
        result = point_lookup(ctx);
        break;
    case BENCH_PATTERN_OPEN_CLOSE:
        result = open_close(ctx);
        break;
    case BENCH_PATTERN_COUNT:
    default:
        result = -1;
        break;
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

void bench_stdio_teardown(bench_context *ctx, const bench_case *item)
{
    stdio_state *state;

    if (ctx == NULL || item == NULL)
    {
        return;
    }

    state = (stdio_state *)ctx->state;
    if (state == NULL)
    {
        return;
    }

    cplat_free(state->block);
    cplat_free(state);
    ctx->state = NULL;
}
