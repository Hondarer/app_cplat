/**
 *******************************************************************************
 *  @file           result_internal.h
 *  @brief          OS エラー値を共通結果コードへ変換する内部 API を提供します。
 *
 *  errno および Windows の GetLastError() の値を、
 *  `cplat/base/result.h` の共通結果コードへ変換します。\n
 *  各モジュールに固有のエラー値の解釈 (例: 特定の errno を別コードへ割り当てる) は、
 *  本 API を呼び出す前段でモジュール側が判定してください。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_RESULT_INTERNAL_H
#define CPLAT_RESULT_INTERNAL_H

#include <cplat/base/platform.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          errno の値を共通結果コードへ変換します。
     *  @param[in]      errno_value errno の値。
     *  @return         対応する共通結果コード。`ENAMETOOLONG` と `ERANGE` は
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、対応がない値は
     *                  @ref CPLAT_ERR_UNKNOWN を返します。
     */
    int cplat_internal_result_from_errno(int errno_value);

#if defined(PLATFORM_WINDOWS)
    /**
     *  @brief          GetLastError() の値を共通結果コードへ変換します。
     *  @param[in]      error_code GetLastError() で取得したエラー コード。
     *  @return         対応する共通結果コード。対応がない値は @ref CPLAT_ERR_UNKNOWN を返します。
     */
    int cplat_internal_result_from_windows_error(unsigned long error_code);

    /**
     *  @brief          Winsock エラーを共通結果コードへ変換します。
     *  @param[in]      error_code WSAGetLastError() が返した値。
     *  @return         対応する共通結果コードを返します。
     *
     *  Winsock のエラー番号空間は Win32 の GetLastError() と異なるため、
     *  cplat_internal_result_from_windows_error() では分類できません。
     */
    int cplat_internal_result_from_winsock_error(unsigned long error_code);

    /**
     *  @brief          GetLastError() の値を errno 相当の値へ変換します。
     *  @param[in]      error_code GetLastError() で取得したエラー コード。
     *  @return         対応する errno の値。対応がない値は `EIO` を返します。
     *
     *  errno 互換の内部処理へ Win32 エラー コードをそのまま渡すと、
     *  errno として文字列化した際に無関係なメッセージになります。\n
     *  errno 互換値を必要とする内部処理に限って使用し、cplat_error には
     *  Win32 ドメインの値を変換せずに記録してください。
     */
    int cplat_internal_errno_from_windows_error(unsigned long error_code);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_RESULT_INTERNAL_H */
