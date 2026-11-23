/**
 *******************************************************************************
 *  @file           wchar_conv.h
 *  @brief          Windows で UTF-8 とワイド文字列を相互変換する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/09
 *  @version        1.0.0
 *
 *  Windows 上で UTF-8 文字列とワイド文字列 (UTF-16LE) を相互変換する
 *  関数を提供します。変換できない文字列は置換せず、結果コードで通知します。\n
 *  Linux では本ヘッダーは何も宣言しません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_CRT_WCHAR_CONV_H
#define CPLAT_CRT_WCHAR_CONV_H

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

#if defined(PLATFORM_WINDOWS)

    #include <stddef.h>
    #include <wchar.h>

    #ifdef __cplusplus
extern "C"
{
    #endif

    /**
     *  @brief          UTF-8 文字列をワイド文字列に変換し、確保した領域へ格納します。
     *  @param[out]     wtext_out  変換した文字列の格納先。成功時は @ref cplat_free で解放してください。
     *                             失敗時は NULL を格納します。
     *  @param[in]      utf8_text  変換元の UTF-8 文字列 (NUL 終端)。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、その他の OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  UTF-8 として不正なバイト列 (不完全な並び、冗長な表現、サロゲートの符号位置など) を含む場合は、
     *  置換文字に置き換えず @ref CPLAT_ERR_INVALID_ENCODING を返します。\n
     *  @p wtext_out または @p utf8_text が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_utf8_to_wstr_alloc(wchar_t **wtext_out, const char *utf8_text);

    /**
     *  @brief          ワイド文字列を UTF-8 に変換し、確保した領域へ格納します。
     *  @param[out]     utf8_text_out 変換した文字列の格納先。成功時は @ref cplat_free で解放してください。
     *                                失敗時は NULL を格納します。
     *  @param[in]      wtext         変換元のワイド文字列 (NUL 終端)。
     *  @return         @ref cplat_utf8_to_wstr_alloc と同じ結果コードを返します。
     *
     *  対になっていないサロゲートを含む場合は @ref CPLAT_ERR_INVALID_ENCODING を返します。
     *  @p utf8_text_out または @p wtext が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_wstr_to_utf8_alloc(char **utf8_text_out, const wchar_t *wtext);

    /**
     *  @brief          UTF-8 文字列をワイド文字列に変換します。
     *  @param[out]     dest       変換結果の書き込み先。
     *  @param[in]      dest_size  @p dest の要素数。1 以上 `INT_MAX` 以下。
     *  @param[out]     count_out  書いた要素数 (NUL 終端を含む) の格納先。NULL 可。失敗時は 0 を格納します。
     *  @param[in]      utf8_text  変換元の UTF-8 文字列 (NUL 終端)。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、その他の OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  パスの区切り文字は正規化しません。失敗した場合、@p dest が NULL でなく @p dest_size が 1 以上なら、
     *  @p dest[0] を NUL にします。
     *  @p dest または @p utf8_text が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_utf8_to_wstr(wchar_t *dest, size_t dest_size, size_t *count_out,
                                                  const char *utf8_text);

    /**
     *  @brief          ワイド文字列を UTF-8 に変換します。
     *  @param[out]     dest       変換結果の書き込み先。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上 `INT_MAX` 以下。
     *  @param[out]     count_out  書いたバイト数 (NUL 終端を含む) の格納先。NULL 可。失敗時は 0 を格納します。
     *  @param[in]      wtext      変換元のワイド文字列 (NUL 終端)。
     *  @return         @ref cplat_utf8_to_wstr と同じ結果コードを返します。
     *
     *  パスの区切り文字は正規化しません。
     *  @p dest または @p wtext が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_wstr_to_utf8(char *dest, size_t dest_size, size_t *count_out,
                                                  const wchar_t *wtext);

    /**
     *  @brief          UTF-8 パスをワイド文字列に変換します。
     *  @param[out]     dest       変換結果の書き込み先。
     *  @param[in]      dest_size  @p dest の要素数。1 以上 `INT_MAX` 以下。
     *  @param[out]     count_out  書いた要素数 (NUL 終端を含む) の格納先。NULL 可。失敗時は 0 を格納します。
     *  @param[in]      utf8_path  変換元の UTF-8 パス (NUL 終端)。
     *  @return         @ref cplat_utf8_to_wstr と同じ結果コードを返します。
     *
     *  @p dest または @p utf8_path が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_utf8_to_wpath(wchar_t *dest, size_t dest_size, size_t *count_out,
                                                   const char *utf8_path);

    /**
     *  @brief          ワイド文字列パスを UTF-8 に変換します。
     *  @param[out]     dest       変換結果の書き込み先。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上 `INT_MAX` 以下。
     *  @param[out]     count_out  書いたバイト数 (NUL 終端を含む) の格納先。NULL 可。失敗時は 0 を格納します。
     *  @param[in]      wpath      変換元のワイド文字列パス (NUL 終端)。
     *  @return         @ref cplat_wstr_to_utf8 と同じ結果コードを返します。
     *
     *  変換後の `\\` を `/` に正規化します。
     *  @p dest または @p wpath が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_wpath_to_utf8(char *dest, size_t dest_size, size_t *count_out,
                                                   const wchar_t *wpath);

    #ifdef __cplusplus
}
    #endif

#endif /* PLATFORM_WINDOWS */

/** @} */

#endif /* CPLAT_CRT_WCHAR_CONV_H */
