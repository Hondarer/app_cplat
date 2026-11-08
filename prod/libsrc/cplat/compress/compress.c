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
 * zlib の avail_in と avail_out は uInt のため、1 回に渡せる長さは 4 [GiB] 未満である。
 * uInt に収まらない remaining では uInt の最大値を返し、収まるときは remaining を返す。
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

    /* ヘッダーの直後に圧縮データを 1 [byte] 以上書く必要がある。 */
    if (*dst_len < CPLAT_COMPRESS_HEADER_SIZE + 1U)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* 先頭 8 [byte] に元サイズ (NBO) を書く。 */
    orig_len_nbo = cplat_hton64((uint64_t)src_len);
    memcpy(dst, &orig_len_nbo, CPLAT_COMPRESS_HEADER_SIZE);

    dst_capacity = *dst_len;
    out_remaining = dst_capacity - CPLAT_COMPRESS_HEADER_SIZE;

    /* raw DEFLATE (windowBits = -15) で圧縮する。 */
    z.next_in = (Bytef *)(uintptr_t)src; /* z_const 未定義時の next_in は非 const であり -Wcast-qual を避ける */

    z.avail_in = (uInt)src_len; /* CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE 以下のため uInt に収まる */
    z.next_out = dst + CPLAT_COMPRESS_HEADER_SIZE;

    /*
     * 第 5 引数の 8 は memLevel であり、zlib の既定値である。
     * see: https://zlib.net/manual.html
     */
    ret = deflateInit2(&z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_MEM_ERROR)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    if (ret != Z_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /*
     * avail_out は uInt までである。Z_FINISH が Z_OK の間は、出力の残りを渡して deflate を繰り返す。
     * see: https://zlib.net/manual.html
     */
    do
    {
        uInt avail_out;

        avail_out = zlib_avail(out_remaining);
        z.avail_out = avail_out;
        ret = deflate(&z, Z_FINISH);
        out_remaining -= (size_t)(avail_out - z.avail_out);
    } while (ret == Z_OK);

    /*
     * deflateInit2 が成功した後は deflateEnd で内部状態を解放する。
     * 未完了のとき deflateEnd は Z_DATA_ERROR を返すため、その戻り値は使わず deflate の結果で判定する。
     * see: https://zlib.net/manual.html
     */
    deflateEnd(&z);

    if (ret == Z_MEM_ERROR)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    if (ret != Z_STREAM_END)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /*
     * out_remaining の初期値はヘッダーを除いた容量である。
     * dst_capacity との差が、ヘッダーを含む書き込み長になる。
     */
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

    /* 先頭 8 [byte] から元サイズを取得する。 */
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

    /* avail_in/avail_out の幅に依存せず、宣言された長さを越えて dst に書かない。
     * 宣言長へ到達した後は scratch で終端と過剰な出力を区別する。
     * see: https://zlib.net/manual.html
     */
    {
        size_t input_remaining = src_len - CPLAT_COMPRESS_HEADER_SIZE;
        size_t output_remaining = (size_t)orig_len;
        uint8_t scratch;
        int result = CPLAT_ERR_CORRUPT_DATA;

        z.next_in = (Bytef *)(uintptr_t)(src + CPLAT_COMPRESS_HEADER_SIZE);
        z.next_out = dst;
        ret = inflateInit2(&z, -15);
        if (ret != Z_OK)
        {
            return ret == Z_MEM_ERROR ? CPLAT_ERR_OUT_OF_MEMORY : CPLAT_ERR_UNKNOWN;
        }

        for (;;)
        {
            uInt input_count = zlib_avail(input_remaining);
            uInt output_count = output_remaining == 0U ? 1U : zlib_avail(output_remaining);
            size_t consumed;
            size_t produced;

            z.avail_in = input_count;
            z.avail_out = output_count;
            if (output_remaining == 0U)
            {
                z.next_out = &scratch;
            }
            ret = inflate(&z, Z_NO_FLUSH);
            consumed = (size_t)(input_count - z.avail_in);
            produced = (size_t)(output_count - z.avail_out);
            input_remaining -= consumed;
            if (produced > output_remaining)
            {
                break;
            }
            output_remaining -= produced;
            if (ret == Z_STREAM_END)
            {
                result = output_remaining == 0U ? CPLAT_OK : CPLAT_ERR_CORRUPT_DATA;
                break;
            }
            if (ret == Z_MEM_ERROR)
            {
                result = CPLAT_ERR_OUT_OF_MEMORY;
                break;
            }
            if (ret != Z_OK || (consumed == 0U && produced == 0U))
            {
                if (ret != Z_OK && ret != Z_BUF_ERROR && ret != Z_DATA_ERROR && ret != Z_NEED_DICT)
                {
                    result = CPLAT_ERR_UNKNOWN;
                }
                break;
            }
        }
        inflateEnd(&z);
        if (result != CPLAT_OK)
        {
            return result;
        }
    }
    *dst_len = (size_t)orig_len;
    return CPLAT_OK;
}
