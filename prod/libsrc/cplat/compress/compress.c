/**
 *******************************************************************************
 *  @file           compress.c
 *  @brief          zlib を使用してデータを圧縮および展開する共通機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/03/05
 *  @version        1.0.0
 *
 *  zlib の deflate/inflate を raw DEFLATE (windowBits = -15) モードで使用します。\n
 *  Linux と Windows で app/zlib の同じ実装を使用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#include <string.h>

#include <zlib.h>

#include <cplat/base/result.h>
#include <cplat/compress/compress.h>
#include <cplat/net/byteorder.h>

/*
 * zlib の avail_in / avail_out は uInt のため、1 回に渡せる長さは 4 GiB 未満です。
 * see: https://zlib.net/manual.html
 */
static uInt zlib_avail(const size_t remaining)
{
    if (remaining > (size_t)((uInt)-1))
    {
        return (uInt)-1;
    }

    return (uInt)remaining;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_compress(uint8_t *dst, size_t *dst_len, const uint8_t *src, const size_t src_len)
{
    uint64_t orig_len_nbo;
    z_stream z = {0};
    int ret;
    size_t dst_capacity;
    size_t out_remaining;

    if (dst == NULL || dst_len == NULL || src == NULL || src_len == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (src_len > (size_t)CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE)
    {
        return CPLAT_ERR_LIMIT_EXCEEDED;
    }

    if (*dst_len < CPLAT_COMPRESS_HEADER_SIZE + 1U)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* 先頭 8 バイトに元サイズ (NBO) を書く */
    orig_len_nbo = cplat_hton64((uint64_t)src_len);
    memcpy(dst, &orig_len_nbo, CPLAT_COMPRESS_HEADER_SIZE);

    dst_capacity = *dst_len;
    out_remaining = dst_capacity - CPLAT_COMPRESS_HEADER_SIZE;

    /* raw DEFLATE (windowBits = -15) で圧縮 */
    z.next_in = (Bytef *)(uintptr_t)src;
    /* src_len は CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE 以下のため uInt に収まる */
    z.avail_in = (uInt)src_len;
    z.next_out = dst + CPLAT_COMPRESS_HEADER_SIZE;

    ret = deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_MEM_ERROR)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    if (ret != Z_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    do
    {
        uInt avail_out;

        avail_out = zlib_avail(out_remaining);
        z.avail_out = avail_out;
        ret = deflate(&z, Z_FINISH);
        out_remaining -= (size_t)(avail_out - z.avail_out);
    } while (ret == Z_OK);

    deflateEnd(&z);

    if (ret == Z_MEM_ERROR)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    if (ret != Z_STREAM_END)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *dst_len = dst_capacity - out_remaining;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_decompress(uint8_t *dst, size_t *dst_len, const uint8_t *src, const size_t src_len)
{
    uint64_t orig_len_nbo;
    uint64_t orig_len;
    z_stream z = {0};
    int ret;

    if (dst == NULL || dst_len == NULL || src == NULL || src_len <= CPLAT_COMPRESS_HEADER_SIZE)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 先頭 8 バイトから元サイズを取得 */
    memcpy(&orig_len_nbo, src, CPLAT_COMPRESS_HEADER_SIZE);
    orig_len = cplat_ntoh64(orig_len_nbo);

    if (orig_len > (uint64_t)CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE)
    {
        return CPLAT_ERR_LIMIT_EXCEEDED;
    }

    if (*dst_len < (size_t)orig_len)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* raw DEFLATE を展開 */
    z.next_in = (Bytef *)(uintptr_t)(src + CPLAT_COMPRESS_HEADER_SIZE);
    z.avail_in = (uInt)(src_len - CPLAT_COMPRESS_HEADER_SIZE);
    z.next_out = (Bytef *)dst;
    z.avail_out = (uInt)*dst_len;

    ret = inflateInit2(&z, -15);
    if (ret == Z_MEM_ERROR)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    if (ret != Z_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    ret = inflate(&z, Z_FINISH);
    inflateEnd(&z);

    if (ret == Z_MEM_ERROR)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    if (ret != Z_STREAM_END)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *dst_len = (size_t)z.total_out;
    return CPLAT_OK;
}
