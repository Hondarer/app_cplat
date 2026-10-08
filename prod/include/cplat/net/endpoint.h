/**
 *******************************************************************************
 *  @file           endpoint.h
 *  @brief          IPv4 の通信エンドポイントを表す型と、アドレスの解決および整形 API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/11
 *  @version        1.0.0
 *
 *  プラットフォームごとに異なるアドレス表現を共通の型で抽象化します。\n
 *  `struct sockaddr_in` を公開面へ出さないため、利用側はシステムのソケット ヘッダーへ
 *  依存せずにアドレスを扱えます。
 *
 *  | OS      | 使用 API                                        |
 *  | ------- | ----------------------------------------------- |
 *  | Linux   | `inet_pton` / `inet_ntop` / `getaddrinfo`        |
 *  | Windows | `inet_pton` / `inet_ntop` / `getaddrinfo` (Winsock) |
 *
 *  Table: プラットフォーム別のアドレス処理 API
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_NET_ENDPOINT_H
#define CPLAT_NET_ENDPOINT_H

#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <cplat/net/byteorder.h>
#include <stddef.h>
#include <stdint.h>

/** IPv4 アドレス文字列を格納するために必要なバッファー サイズ (終端を含む)。 */
#define CPLAT_IPV4_ADDR_STRLEN 16U

/** 任意のアドレスを表す IPv4 アドレス (0.0.0.0)。ネットワーク バイト オーダー。 */
#define CPLAT_IPV4_ADDR_ANY ((uint32_t)0U)

/** ループバック アドレス (127.0.0.1)。ネットワーク バイト オーダー。 */
#define CPLAT_IPV4_ADDR_LOOPBACK (cplat_hton32((uint32_t)0x7F000001U))

/**
 *  @ingroup        CPLAT_NET
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          IPv4 の通信エンドポイント (アドレスとポート) を表します。
     *
     *  アドレスとポートは、いずれもネットワーク バイト オーダーで保持します。\n
     *  ホスト バイト オーダーの値を設定する場合は、@ref cplat_hton32 と
     *  @ref cplat_hton16 を使用します。
     */
    typedef struct cplat_ipv4_endpoint
    {
        uint32_t address; /**< IPv4 アドレス (ネットワーク バイト オーダー)。 */
        uint16_t port;    /**< ポート番号 (ネットワーク バイト オーダー)。 */
        uint16_t pad;     /**< 明示的アラインメント */
    } cplat_ipv4_endpoint;

    /**
     *  @brief          ドット区切りの IPv4 アドレス文字列を数値へ変換します。
     *  @param[in]      text        変換元の文字列 (例: "192.168.0.1")。
     *  @param[out]     address_out 変換後のアドレス (ネットワーク バイト オーダー) の格納先。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT のいずれかを返します。
     *
     *  名前解決は行いません。ホスト名を解決する場合は @ref cplat_ipv4_resolve を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_ipv4_parse(const char *text, uint32_t *address_out);

    /**
     *  @brief          ホスト名または IPv4 アドレス文字列を数値へ解決します。
     *  @param[in]      text        ホスト名または IPv4 アドレス文字列。
     *  @param[out]     address_out 解決後のアドレス (ネットワーク バイト オーダー) の格納先。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  複数のアドレスが解決された場合は、先頭の IPv4 アドレスを採用します。\n
     *  解決に失敗した場合、@p detail_out には @ref CPLAT_ERROR_DOMAIN_GAI ドメインの
     *  エラー コードを格納します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_ipv4_resolve(const char *text, uint32_t *address_out, cplat_error *detail_out);

    /**
     *  @brief          IPv4 アドレスをドット区切りの文字列へ変換します。
     *  @param[in]      address     変換元のアドレス (ネットワーク バイト オーダー)。
     *  @param[out]     buffer      文字列の格納先。
     *  @param[in]      buffer_size @p buffer のバイト数。@ref CPLAT_IPV4_ADDR_STRLEN 以上を指定します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_ipv4_to_string(uint32_t address, char *buffer, size_t buffer_size,
                                                    cplat_error *detail_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_NET_ENDPOINT_H */
