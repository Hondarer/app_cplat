/**
 *******************************************************************************
 *  @file           module.h
 *  @brief          モジュール情報を取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/22
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_MODULE_H
#define CPLAT_MODULE_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif /* _GNU_SOURCE */
#endif     /* PLATFORM_LINUX */

#include <stddef.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          関数アドレスが属するモジュールの完全なパスを取得します。
     *  @param[out]     path_out      完全なパス (UTF-8) の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size     @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[in]      func_addr     モジュールを特定するための関数アドレス。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_module_get_path(char *path_out, size_t path_size, const void *func_addr);

    /**
     *  @brief          関数アドレスが属するモジュールのベース名を取得します。
     *  @param[out]     basename_out   ベース名 (UTF-8) の格納先。NULL を渡してはなりません。
     *  @param[in]      basename_size  @p basename_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[in]      func_addr      モジュールを特定するための関数アドレス。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_module_get_basename(char *basename_out, size_t basename_size,
                                                         const void *func_addr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_MODULE_H */
