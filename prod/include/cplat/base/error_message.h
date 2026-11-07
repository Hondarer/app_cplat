/**
 *******************************************************************************
 *  @file           error_message.h
 *  @brief          結果コードと詳細エラーを人間可読の文字列へ変換する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/30
 *  @version        1.0.0
 *
 *  ログやユーザー向けメッセージへエラーを表示するための文字列化を提供します。\n
 *  共通結果コード (@ref CPLAT_OK 、`CPLAT_ERR_*`) と、ドメイン付きの
 *  @ref cplat_error の双方を扱います。
 *
 *  公開 API は生の OS エラー値を受け取りません。@ref cplat_error が保持する
 *  ドメインに基づいて errno と Win32 エラー コードの文字列化を振り分けます。
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

#ifndef CPLAT_BASE_ERROR_MESSAGE_H
#define CPLAT_BASE_ERROR_MESSAGE_H

#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <stddef.h>

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          共通結果コードの名称を返します。
     *  @param[in]      result  共通結果コード (@ref CPLAT_OK または `CPLAT_ERR_*`)。
     *  @return         結果コードに対応する静的文字列を返します。NULL は返しません。
     *
     *  返す文字列はコード名に対応する英小文字の短い説明です。
     *  (例: @ref CPLAT_ERR_INVALID_ARGUMENT なら "invalid argument")。\n
     *  未知の値には "unknown result code" を返します。
     *
     *  返却する文字列は静的領域にあり、呼び出し側で解放しないでください。\n
     *  書式文字列としてではなく、`%s` の引数として使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_result_to_string(int result);

    /**
     *  @brief          詳細エラーに対応するメッセージを取得します。
     *  @param[out]     buf      メッセージの格納先。NULL を渡してはなりません。常に NUL 終端します。
     *  @param[in]      buf_size @p buf のバイト数。1 以上を指定してください。
     *  @param[in]      error    文字列化する詳細エラー。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。\n
     *                  メッセージが収まらない場合も切り詰めて格納し @ref CPLAT_OK を返します。
     *
     *  errno ドメインはスレッド セーフな CRT API、Win32 ドメインは
     *  `FormatMessageW` を使用して UTF-8 のメッセージへ変換します。\n
     *  ドメインが @ref CPLAT_ERROR_DOMAIN_NONE の場合は "no error" を格納します。\n
     *  Linux で Win32 ドメインを指定した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_error_message(char *buf, size_t buf_size, const cplat_error *error);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_BASE_ERROR_MESSAGE_H */
