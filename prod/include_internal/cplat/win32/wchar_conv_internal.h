/**
 *******************************************************************************
 *  @file           wchar_conv_internal.h
 *  @brief          Windows の文字コード変換とエラー保持の内部 API を提供します。
 *
 *  長さを指定した UTF-8 変換、変換結果の最終エラーと errno への写像、
 *  最終エラーを保持した解放を、ライブラリ内部で共有します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_WIN32_WCHAR_CONV_INTERNAL_H
#define CPLAT_WIN32_WCHAR_CONV_INTERNAL_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/result.h>
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/stdlib.h>

    #include <errno.h>
    #include <wchar.h>

    #ifdef __cplusplus
extern "C"
{
    #endif

    /**
     *  @brief          長さを指定した UTF-8 を確保済みのワイド文字列へ変換します。
     *  @param[out]     wtext_out  変換した領域。成功時は呼び出し側が解放します。
     *  @param[out]     count_out  書いた UTF-16 要素数。NULL 可。
     *  @param[in]      src        変換元の UTF-8。
     *  @param[in]      src_len    入力バイト数。-1 は NUL 終端文字列を表します。
     *  @return         共通結果コードを返します。
     */
    int cplat_internal_utf8_to_wide_n(wchar_t **wtext_out, int *count_out, const char *src, int src_len);

    /**
     *  @brief          変換結果を Win32 の最終エラーへ写します。
     *  @param[in]      result 変換の結果コード。
     */
    static inline void cplat_internal_win32_set_last_error_from_result(const int result)
    {
        switch (result)
        {
            case CPLAT_ERR_INVALID_ARGUMENT:
                SetLastError(ERROR_INVALID_PARAMETER);
                break;
            case CPLAT_ERR_OUT_OF_MEMORY:
                SetLastError(ERROR_OUTOFMEMORY);
                break;
            case CPLAT_ERR_BUFFER_TOO_SMALL:
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                break;
            case CPLAT_ERR_INVALID_ENCODING:
                SetLastError(ERROR_NO_UNICODE_TRANSLATION);
                break;
            default:
                /* OS 由来の最終エラーを保持します。 */
                break;
        }
    }

    /**
     *  @brief          変換結果を errno へ写します。
     *  @param[in]      result 変換の結果コード。
     *  @param[in]      is_path パスの場合は 1、それ以外は 0。
     *  @return         対応する errno を返します。
     */
    static inline int cplat_internal_errno_from_conv_result(const int result, const int is_path)
    {
        switch (result)
        {
            case CPLAT_ERR_INVALID_ARGUMENT:
                return EINVAL;
            case CPLAT_ERR_OUT_OF_MEMORY:
                return ENOMEM;
            case CPLAT_ERR_BUFFER_TOO_SMALL:
                return is_path != 0 ? ENAMETOOLONG : ERANGE;
            case CPLAT_ERR_INVALID_ENCODING:
                return EILSEQ;
            default:
                return EIO;
        }
    }

    /**
     *  @brief          cplat_free() の前後で Win32 の最終エラーを保持します。
     *  @param[in]      ptr 解放する領域。
     *
     *  GetLastError() の値は失敗を示す関数の直後に取得します。
     *  see: https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-getlasterror
     */
    static inline void cplat_internal_win32_free_keep_last_error(void *ptr)
    {
        const DWORD error_code = GetLastError();

        cplat_free(ptr);
        SetLastError(error_code);
    }

    #ifdef __cplusplus
}
    #endif

#endif /* PLATFORM_WINDOWS */

#endif /* CPLAT_WIN32_WCHAR_CONV_INTERNAL_H */
