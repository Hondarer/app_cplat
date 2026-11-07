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
#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if defined(PLATFORM_WINDOWS)

    /**
     *  @brief          UTF-8 パスをワイド文字列にしたときに PLATFORM_PATH_MAX へ収まるかを検査します。
     *  @param[in]      utf8_path   検査する UTF-8 パス。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL の場合は設定しません。
     *  @return         収まる場合は CPLAT_OK を返し、@p detail_out は変更しません。\n
     *                  収まらない場合は ENAMETOOLONG に対応する結果コードを返します。
     *
     *  PLATFORM_PATH_MAX (MAX_PATH) を超える通常形式のパスは、長いパスを宣言していない
     *  プロセスでは CreateFileW が ERROR_PATH_NOT_FOUND で失敗し、原因を区別できません。\n
     *  そのため、Win32 API へ渡す前に本関数で長さを検査し、ENAMETOOLONG として報告します。\n
     *  判定はワイド文字数 (null 終端含む) で行い、@ref cplat_utf8_to_wpath に
     *  PLATFORM_PATH_MAX 要素のバッファーを渡した場合と同じ結果になります。
     */
    static inline int crt_check_wpath_length(const char *utf8_path, cplat_error *detail_out)
    {
        /*
         * 出力先を NULL、サイズを 0 にすると、必要なワイド文字数 (null 終端含む) を返す。
         * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
         */
        const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, NULL, 0);

        if (needed <= 0 || needed > PLATFORM_PATH_MAX)
        {
            return cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
        }

        return CPLAT_OK;
    }

#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRT_PRIVATE_H */
