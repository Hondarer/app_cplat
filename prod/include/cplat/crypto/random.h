/**
 *******************************************************************************
 *  @file           random.h
 *  @brief          暗号論的に安全な乱数を取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/30
 *  @version        1.0.0
 *
 *  Linux では OpenSSL の `RAND_bytes`、Windows では CNG の `BCryptGenRandom` を
 *  使用して、OS が提供する暗号論的乱数源からバイト列を取得します。\n
 *  鍵、ノンス、セッション識別子など、推測されてはならない値の生成に使用します。
 *
 *  @warning        `rand()` は使用しないでください。glibc は 31 bit、MSVC は
 *                  15 bit しか返さず、いずれも予測可能です。
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

#ifndef CPLAT_CRYPTO_RANDOM_H
#define CPLAT_CRYPTO_RANDOM_H

#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <limits.h>
#include <stddef.h>

#define CPLAT_CRYPTO_RANDOM_MAX_BYTES ((size_t)INT_MAX) /**< @ref cplat_random_bytes で指定できる最大バイト数。 */

/**
 *  @ingroup        CPLAT_CRYPTO
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          暗号論的に安全な乱数でバッファーを満たします。
     *  @param[out]     dest       乱数の格納先。@p dest_size が 0 の場合に限り NULL も指定できます。
     *  @param[in]      dest_size  @p dest の容量と、生成を要求するバイト数。@ref CPLAT_CRYPTO_RANDOM_MAX_BYTES 以下を指定します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  要求したバイト数をすべて満たした場合のみ @ref CPLAT_OK を返します。\n
     *  部分的に満たすことはありません。
     *  @p dest_size が @ref CPLAT_CRYPTO_RANDOM_MAX_BYTES を超える場合は、
     *  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @warning        戻り値を無視してはなりません。乱数源が利用できない場合に
     *                  @p dest の内容は不定であり、そのまま鍵やノンスとして使用すると
     *                  暗号強度が失われます。失敗時は処理を継続せず、呼び出し元へ
     *                  エラーを伝播してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_random_bytes(void *dest, size_t dest_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRYPTO_RANDOM_H */
