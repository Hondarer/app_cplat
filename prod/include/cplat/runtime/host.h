/**
 *******************************************************************************
 *  @file           host.h
 *  @brief          ホスト識別情報を取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/29
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

#ifndef CPLAT_RUNTIME_HOST_H
#define CPLAT_RUNTIME_HOST_H

#include <stddef.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

/**
 *  @brief          ホスト名の格納に使う配列サイズです (NUL 終端込み)。
 *
 *  `char name[CPLAT_HOST_NAME_MAX];` として @ref cplat_host_get_name へ渡せば、
 *  Linux と Windows の DNS ホスト名が収まる想定です。\n
 *  Windows の `DNS_MAX_NAME_BUFFER_LENGTH` (256) に合わせています。
 */
#define CPLAT_HOST_NAME_MAX 256

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          現在のホストの DNS ホスト名を取得します。
     *  @param[out]     name_out   UTF-8 ホスト名の格納先。NULL を渡してはなりません。
     *  @param[in]      name_size  @p name_out のサイズ [byte]。0 および `INT_MAX` を超える値を渡してはなりません。
     *  @retval         CPLAT_OK                    ホスト名を取得しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  @p name_out が NULL、@p name_size が 0、または `INT_MAX` を超えます。
     *  @retval         CPLAT_ERR_BUFFER_TOO_SMALL  @p name_out の容量が不足しています。
     *  @retval         CPLAT_ERR_UNSUPPORTED       現在のプラットフォームをサポートしていません。
     *  @return         上記以外の失敗時は、OS エラーを変換した共通結果コードを返します。
     *
     *  Linux では `gethostname`、Windows では `GetComputerNameExW(ComputerNameDnsHostname)` を使用します。\n
     *  返る値は OS が保持する DNS ホスト名であり、FQDN であることは保証しません。\n
     *  Windows では Winsock の `gethostname` を使わず、UTF-16 から UTF-8 へ変換します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_host_get_name(char *name_out, size_t name_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_RUNTIME_HOST_H */
