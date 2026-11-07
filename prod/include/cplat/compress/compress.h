/**
 *******************************************************************************
 *  @file           compress.h
 *  @brief          データを圧縮および展開する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/03/05
 *  @version        1.0.0
 *
 *  Linux と Windows で app/zlib の deflate/inflate を使用します。\n
 *  raw DEFLATE (windowBits = -15) と元サイズを組み合わせ、両 OS で同じ形式を扱います。
 *
 *  圧縮ペイロードのフォーマット:
    @code
    [元サイズ: uint64_t (ネットワークバイトオーダー)] [raw DEFLATE データ]
    @endcode
 *
 *  扱える展開後の長さは @ref CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE 以下です。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef COMPRESS_H
#define COMPRESS_H

#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <stddef.h>
#include <stdint.h>

/**
 *  @ingroup        CPLAT_COMPRESS
 *  @{
 */

/** 圧縮ペイロード先頭に付加する展開後長さフィールドのバイト数。 */
#define CPLAT_COMPRESS_HEADER_SIZE 8U

/** 圧縮および展開が扱う展開後長さの上限 [byte]。4 [GiB] 未満です。 */
#define CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE UINT32_MAX

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          データを圧縮します。
     *  @param[out]     dst      圧縮後データを格納するバッファー。
     *                           先頭 8 [byte] に元サイズ (NBO) が書き込まれます。
     *  @param[in,out]  dst_len  入力: dst のバッファー サイズ。
     *                           出力: 書き込んだバイト数。
     *  @param[in]      src      圧縮前データへのポインター。
     *  @param[in]      src_len  圧縮前データのバイト数。
     *                           1 以上 @ref CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE 以下で指定します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  zlib の初期化または処理中のメモリ不足は @ref CPLAT_ERR_OUT_OF_MEMORY を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_compress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len);

    /**
     *  @brief          圧縮データを展開します。
     *  @param[out]     dst      展開後データを格納するバッファー。
     *  @param[in,out]  dst_len  入力: dst のバッファー サイズ。
     *                           出力: 書き込んだバイト数。
     *  @param[in]      src      圧縮後データへのポインター (先頭 8 [byte] は元サイズ)。
     *  @param[in]      src_len  圧縮後データのバイト数 (ヘッダーを含む)。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  zlib の初期化または処理中のメモリ不足は @ref CPLAT_ERR_OUT_OF_MEMORY を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_decompress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* COMPRESS_H */
