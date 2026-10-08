/**
 *******************************************************************************
 *  @file           sync_descriptor.h
 *  @brief          プロセス間同期ディスクリプターの直列化機能 (内部共有) を宣言します。
 *
 *  プロセス間同期オブジェクトを他プロセスへ受け渡すためのディスクリプター
 *  (バイト列) の直列化と逆直列化を提供します。
 *  ワイヤ フォーマットはプラットフォーム非依存であり、本ヘッダーと
 *  sync_descriptor.c が唯一の定義箇所です。
 *
 *  フォーマット (リトル エンディアン):
 *  - offset 0-3: マジック "CULK"
 *  - offset 4: バージョン
 *  - offset 5: 種別 (INTERPROCESS_SYNC_KIND_*)
 *  - offset 6: バックエンド (cplat_interprocess_sync_backend)
 *  - offset 7: 予約 (0)
 *  - offset 8-11: identity 長 (uint32)
 *  - offset 12-19: 予約 (0)
 *  - offset 20-: identity (NUL 終端なし)
 *******************************************************************************
 */
#ifndef CPLAT_SYNC_DESCRIPTOR_H
#define CPLAT_SYNC_DESCRIPTOR_H

#include <stddef.h>
#include <stdint.h>

#include <cplat/sync/sync.h>

/** ディスクリプターのフォーマット バージョン。 */
#define CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_VERSION 1U

/** ディスクリプターのヘッダー バイト数。 */
#define CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE 20U

/** 種別: プロセス間ロック。 */
#define CPLAT_INTERPROCESS_SYNC_KIND_LOCK 1U

/** 種別: プロセス間リーダー ライター ロック。 */
#define CPLAT_INTERPROCESS_SYNC_KIND_RWLOCK 2U

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     *  @brief          プロセス間同期オブジェクトのディスクリプターを直列化します。
     *  @param[in]      identity         同期オブジェクトの識別文字列。
     *  @param[in]      kind             種別 (INTERPROCESS_SYNC_KIND_*)。
     *  @param[in]      backend          バックエンド種別。
     *  @param[out]     descriptor       直列化結果を格納するバッファー。
     *  @param[in,out]  descriptor_size  入力はバッファーのバイト数、出力は必要バイト数。
     *  @return         成功時 CPLAT_OK。
     *                  identity または descriptor_size が NULL の場合 CPLAT_ERR_INVALID_ARGUMENT。
     *                  バッファー不足時は CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *                  descriptor_size に必要バイト数を格納します。
     */
    int cplat_internal_interprocess_sync_descriptor_export(const char *identity, uint8_t kind, uint8_t backend,
                                                           void *descriptor, size_t *descriptor_size);

    /**
     *  @brief          ディスクリプターを検証して identity を取り出します。
     *  @param[in]      descriptor       逆直列化するディスクリプター。
     *  @param[in]      descriptor_size  ディスクリプターのバイト数。
     *  @param[in]      kind             期待する種別 (INTERPROCESS_SYNC_KIND_*)。
     *  @param[in]      backend          期待するバックエンド種別。
     *  @param[out]     identity_out     取り出した identity 文字列 (ヒープ確保、NUL 終端) の格納先。
     *                                   呼び出し元が free すること。
     *  @return         成功時 CPLAT_OK。
     *                  descriptor または identity_out が NULL の場合 CPLAT_ERR_INVALID_ARGUMENT。
     *                  形式版の不一致時 CPLAT_ERR_VERSION_MISMATCH。
     *                  それ以外のフォーマット不正時 CPLAT_ERR_CORRUPT_DESCRIPTOR。
     *                  メモリ確保失敗時 CPLAT_ERR_UNKNOWN。
     */
    int cplat_internal_interprocess_sync_descriptor_import(const void *descriptor, size_t descriptor_size, uint8_t kind,
                                                           uint8_t backend, char **identity_out);

#ifdef __cplusplus
}
#endif

#endif /* CPLAT_SYNC_DESCRIPTOR_H */
