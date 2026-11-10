/**
 *******************************************************************************
 *  @file           wchar_conv.h
 *  @brief          Windows で UTF-8 とワイド文字列を相互変換する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/09
 *  @version        1.0.0
 *
 *  Windows 上で UTF-8 文字列とワイド文字列 (UTF-16LE) を相互変換する
 *  関数を提供します。\n
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

#include <cplat/cplat_export.h>

#if defined(PLATFORM_WINDOWS)

    #include <stddef.h>
    #include <wchar.h>

    #ifdef __cplusplus
extern "C"
{
    #endif

    /**
     *  @brief          UTF-8 文字列をワイド文字列に変換します。
     *  @param[out]     dest       変換結果の書き込み先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest の要素数。0 および `INT_MAX` を超える値を渡してはなりません。
     *  @param[in]      utf8_text  変換元の UTF-8 文字列。NULL を渡してはなりません。
     *  @return         変換後の文字数 (null 終端含む)。失敗時は -1 を返します。
     *
     *  パスの区切り文字は正規化しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_utf8_to_wstr(wchar_t *dest, size_t dest_size, const char *utf8_text);

    /**
     *  @brief          ワイド文字列を UTF-8 に変換します。
     *  @param[out]     dest       変換結果の書き込み先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest のバイト数。0 および `INT_MAX` を超える値を渡してはなりません。
     *  @param[in]      wtext      変換元のワイド文字列。NULL を渡してはなりません。
     *  @return         変換後のバイト数 (null 終端含む)。失敗時は -1 を返します。
     *
     *  パスの区切り文字は正規化しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_wstr_to_utf8(char *dest, size_t dest_size, const wchar_t *wtext);

    /**
     *  @brief          UTF-8 パス文字列をワイド文字列に変換します。
     *  @param[out]     dest       変換結果の書き込み先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest の要素数。0 および `INT_MAX` を超える値を渡してはなりません。
     *  @param[in]      utf8_path  変換元の UTF-8 パス文字列。NULL を渡してはなりません。
     *  @return         変換後の文字数 (null 終端含む)。失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_utf8_to_wpath(wchar_t *dest, size_t dest_size, const char *utf8_path);

    /**
     *  @brief          ワイド文字列パスを UTF-8 に変換します。
     *
     *  変換後の文字列中の `\\` を `/` に正規化します。
     *  @param[out]     dest        変換結果の書き込み先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size   @p dest のバイト数。0 および `INT_MAX` を超える値を渡してはなりません。
     *  @param[in]      wpath       変換元のワイド文字列パス。NULL を渡してはなりません。
     *  @return         変換後のバイト数 (null 終端含む)。失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_wpath_to_utf8(char *dest, size_t dest_size, const wchar_t *wpath);

    /**
     *  @brief          UTF-8 文字列をワイド文字列に変換し、確保した領域へ格納して返します。
     *
     *  返却した領域は @ref cplat_free で解放してください。
     *
     *  @param[in]      utf8_text   変換元の UTF-8 文字列。NULL を渡してはなりません。
     *  @return         変換したワイド文字列へのポインター。失敗時は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT wchar_t *CPLAT_API cplat_utf8_to_wstr_alloc(const char *utf8_text);

    /**
     *  @brief          ワイド文字列を UTF-8 に変換し、確保した領域へ格納して返します。
     *
     *  返却した領域は @ref cplat_free で解放してください。
     *
     *  @param[in]      wtext       変換元のワイド文字列。NULL を渡してはなりません。
     *  @return         変換した UTF-8 文字列へのポインター。失敗時は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT char *CPLAT_API cplat_wstr_to_utf8_alloc(const wchar_t *wtext);

    #ifdef __cplusplus
}
    #endif

#endif /* PLATFORM_WINDOWS */

/** @} */

#endif /* CPLAT_CRT_WCHAR_CONV_H */
