/**
 *******************************************************************************
 *  @file           byteorder.h
 *  @brief          ホスト バイト オーダーとネットワーク バイト オーダーを相互変換する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/11
 *  @version        1.0.0
 *
 *  変換はシフト演算とバイト列の再構成だけで行い、`htons` などの OS API を使用しません。\n
 *  ホストのバイト オーダーを判定せずに正しく変換できるため、プラットフォーム分岐も持ちません。
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

#ifndef CPLAT_NET_BYTEORDER_H
#define CPLAT_NET_BYTEORDER_H

#include <stdint.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_NET
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          16 bit 値をホスト バイト オーダーからネットワーク バイト オーダーへ変換します。
     *  @param[in]      value 変換元の値 (ホスト バイト オーダー)。
     *  @return         ネットワーク バイト オーダーの値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint16_t CPLAT_API cplat_hton16(uint16_t value);

    /**
     *  @brief          16 bit 値をネットワーク バイト オーダーからホスト バイト オーダーへ変換します。
     *  @param[in]      value 変換元の値 (ネットワーク バイト オーダー)。
     *  @return         ホスト バイト オーダーの値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint16_t CPLAT_API cplat_ntoh16(uint16_t value);

    /**
     *  @brief          32 bit 値をホスト バイト オーダーからネットワーク バイト オーダーへ変換します。
     *  @param[in]      value 変換元の値 (ホスト バイト オーダー)。
     *  @return         ネットワーク バイト オーダーの値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint32_t CPLAT_API cplat_hton32(uint32_t value);

    /**
     *  @brief          32 bit 値をネットワーク バイト オーダーからホスト バイト オーダーへ変換します。
     *  @param[in]      value 変換元の値 (ネットワーク バイト オーダー)。
     *  @return         ホスト バイト オーダーの値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint32_t CPLAT_API cplat_ntoh32(uint32_t value);

    /**
     *  @brief          64 bit 値をホスト バイト オーダーからネットワーク バイト オーダーへ変換します。
     *  @param[in]      value 変換元の値 (ホスト バイト オーダー)。
     *  @return         ネットワーク バイト オーダーの値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint64_t CPLAT_API cplat_hton64(uint64_t value);

    /**
     *  @brief          64 bit 値をネットワーク バイト オーダーからホスト バイト オーダーへ変換します。
     *  @param[in]      value 変換元の値 (ネットワーク バイト オーダー)。
     *  @return         ホスト バイト オーダーの値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint64_t CPLAT_API cplat_ntoh64(uint64_t value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_NET_BYTEORDER_H */
