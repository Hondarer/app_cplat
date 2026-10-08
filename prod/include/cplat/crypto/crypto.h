/**
 *******************************************************************************
 *  @file           crypto.h
 *  @brief          データを暗号化および復号する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/03/12
 *  @version        1.0.0
 *
 *  プラットフォームごとに異なる暗号 API を共通インターフェースで抽象化します。\n
 *
 *  | OS      | 使用ライブラリ                                           | アルゴリズム    |
 *  | ------- | -------------------------------------------------------- | --------------- |
 *  | Linux   | OpenSSL (libcrypto) EVP_aes_256_gcm() / EVP_sha256()     | AES-256-GCM / SHA-256 |
 *  | Windows | CNG (BCrypt) BCRYPT_AES_ALGORITHM / BCRYPT_SHA256_ALGORITHM | AES-256-GCM / SHA-256 |
 *
 *  Table: プラットフォーム別の暗号ライブラリとアルゴリズム
 *
 *  暗号化ペイロードのフォーマット:
    @code
    [暗号文: src_len バイト] [GCM 認証タグ: CPLAT_CRYPTO_TAG_SIZE バイト]
    @endcode
 *
 *  ノンス構成 (12 バイト):
    @code
    [session_id: 4B NBO] [seq_num: 4B NBO] [padding: 4B 0x00]
    @endcode
 *
 *  追加認証データ (AAD): 呼び出し元が任意のバイト列を指定可能。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <stddef.h>
#include <stdint.h>

/**
 *  @ingroup        CPLAT_CRYPTO
 *  @{
 */

#define CPLAT_CRYPTO_KEY_SIZE   32U /**< AES-256-GCM 鍵サイズ (バイト)。 */
#define CPLAT_CRYPTO_NONCE_SIZE 12U /**< AES-256-GCM ノンス サイズ (バイト)。 */
#define CPLAT_CRYPTO_TAG_SIZE   16U /**< AES-256-GCM 認証タグ サイズ (バイト)。暗号文の直後に付加する。 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          AES-256-GCM でデータを暗号化します。
     *  @param[out]     dst      暗号化後データを格納するバッファー。\n
     *                           内容: [暗号文: src_len バイト][GCM タグ: CPLAT_CRYPTO_TAG_SIZE バイト]\n
     *                           dst == src の in-place 暗号化も可能。
     *  @param[in,out]  dst_len  入力: dst のバッファー サイズ (>= src_len + CPLAT_CRYPTO_TAG_SIZE)。\n
     *                           出力: 書き込んだバイト数 (= src_len + CPLAT_CRYPTO_TAG_SIZE)。
     *  @param[in]      src      平文データへのポインター。
     *  @param[in]      src_len  平文データのバイト数。
     *  @param[in]      key      AES-256 鍵 (CPLAT_CRYPTO_KEY_SIZE バイト)。
     *  @param[in]      nonce    ノンス (CPLAT_CRYPTO_NONCE_SIZE バイト)。
     *  @param[in]      aad      追加認証データへのポインター。NULL の場合は AAD なし。
     *  @param[in]      aad_len  AAD のバイト数。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_crypto_encrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len,
                                                    const uint8_t *key, const uint8_t *nonce, const uint8_t *aad,
                                                    size_t aad_len);

    /**
     *  @brief          AES-256-GCM でデータを復号し、認証タグを検証します。
     *  @param[out]     dst      復号後データを格納するバッファー。
     *  @param[in,out]  dst_len  入力: dst のバッファー サイズ (>= src_len - CPLAT_CRYPTO_TAG_SIZE)。\n
     *                           出力: 書き込んだバイト数 (= src_len - CPLAT_CRYPTO_TAG_SIZE)。
     *  @param[in]      src      暗号化データへのポインター。\n
     *                           内容: [暗号文: src_len - CPLAT_CRYPTO_TAG_SIZE バイト][GCM タグ: CPLAT_CRYPTO_TAG_SIZE バイト]
     *  @param[in]      src_len  暗号化データのバイト数 (タグを含む)。>= CPLAT_CRYPTO_TAG_SIZE。
     *  @param[in]      key      AES-256 鍵 (CPLAT_CRYPTO_KEY_SIZE バイト)。
     *  @param[in]      nonce    ノンス (CPLAT_CRYPTO_NONCE_SIZE バイト)。
     *  @param[in]      aad      追加認証データへのポインター。NULL の場合は AAD なし。
     *  @param[in]      aad_len  AAD のバイト数。
     *  @return         @ref CPLAT_OK (認証タグ検証 OK)、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN (認証タグ不一致含む) のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_crypto_decrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len,
                                                    const uint8_t *key, const uint8_t *nonce, const uint8_t *aad,
                                                    size_t aad_len);

    /**
     *  @brief          任意のパスフレーズを SHA-256 ハッシュにより AES-256 鍵に変換します。
     *
     *  入力バイト列の SHA-256 ダイジェストをそのまま鍵として使用します。\n
     *  送受信の双方で同一のパスフレーズを指定すれば、同一の鍵が導出されます。
     *
     *  @param[out]     key             出力鍵バッファー (CPLAT_CRYPTO_KEY_SIZE = 32 バイト)。
     *  @param[in]      passphrase      パスフレーズへのポインター (任意のバイト列)。
     *  @param[in]      passphrase_len  パスフレーズの長さ (バイト)。0 の場合も有効。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_passphrase_to_key(uint8_t *key, const uint8_t *passphrase, size_t passphrase_len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CRYPTO_H */
