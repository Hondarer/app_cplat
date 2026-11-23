/**
 *******************************************************************************
 *  @file           crt.h
 *  @brief          crt モジュール内で共有する補助処理を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/22
 *  @version        1.0.0
 *
 *  本ヘッダーは `prod/libsrc/cplat/crt/` のモジュール私有ヘッダーです。\n
 *  同一ディレクトリ内の実装ファイルからのみ `#include "crt.h"` で取り込みます。
 *
 *  本ヘッダーで定義する関数は、呼び出し元が同一ディレクトリ内に限定されるため、NULL チェックは行いません。\n
 *  前提条件は各関数の Doxygen コメントに記載します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CRT_PRIVATE_H
#define CRT_PRIVATE_H

#include <cplat/base/error_internal.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>

#include <errno.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/win32/wchar_conv_internal.h>
#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if defined(PLATFORM_WINDOWS)

    /** Windows `_wfsopen()` 用のモード変換ワイド文字列バッファーの要素数 [wchar_t 要素数] (終端 NUL を含む) です。 */
    #define CRT_WMODES_CAPACITY (64U)

    /**
     *  @brief          UTF-8 パスをワイド文字列にしたときに PLATFORM_PATH_MAX へ収まるかを検査します。
     *  @param[in]      utf8_path   検査する UTF-8 パス。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL の場合は設定しません。
     *  @return         収まる場合は CPLAT_OK を返し、@p detail_out は変更しません。\n
     *                  変換の失敗は対応する結果コードを返し、容量不足は ENAMETOOLONG として記録します。
     *
     *  PLATFORM_PATH_MAX (MAX_PATH) を超える通常形式のパスは、長いパスを宣言していない
     *  プロセスでは CreateFileW が ERROR_PATH_NOT_FOUND で失敗し、原因を区別できません。\n
     *  そのため、Win32 API へ渡す前に厳密な UTF-8 変換で長さを検査し、不正な文字コードは EILSEQ、
     *  容量不足は ENAMETOOLONG として報告します。判定はワイド文字数 (NUL 終端を含む) で行います。
     */
    static inline int crt_check_wpath_length(const char *utf8_path, cplat_error *detail_out)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];
        const int result =
            cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), NULL, utf8_path);

        if (result != CPLAT_OK)
        {
            return cplat_internal_error_report_errno(detail_out, cplat_internal_errno_from_conv_result(result, 1));
        }

        return CPLAT_OK;
    }

#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRT_PRIVATE_H */
