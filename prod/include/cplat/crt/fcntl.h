/**
 *******************************************************************************
 *  @file           fcntl.h
 *  @brief          fcntl 系の CRT 関数を抽象化する API を提供します。
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

#ifndef CPLAT_CRT_FCNTL_H
#define CPLAT_CRT_FCNTL_H

#include <stdarg.h>
#include <fcntl.h>
#include <cplat/base/compiler.h>
#include <cplat/base/error.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          UTF-8 パスでファイル記述子を開きます (`open` / `_wopen` ラッパー)。
     *  @param[in]      path   開くファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      flags  オープン フラグ (O_RDONLY、O_WRONLY、O_RDWR など)。
     *  @param[in]      mode   ファイル生成時のパーミッション (O_CREAT 指定時に使用)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時はファイル記述子、失敗時は -1 を返します。
     *
     *  @par            共有モード
     *  Linux の `open` は共有拒否モードを持たないため、他プロセスの読み書きを拒否しません。\n
     *  Windows では `_wsopen_s` を `_SH_DENYNO` 指定で呼び出し、Linux と同じ既定動作にします。
     *  本関数は共有モードによるアクセス排他を提供しないため、プロセス間のアクセスを直列化する場合は
     *  @ref cplat_interprocess_lock または @ref cplat_interprocess_rwlock を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_open(const char *path, int flags, int mode, cplat_error *detail_out);

    /**
     *  @brief          書式指定パスでファイル記述子を開きます。
     *  @param[in]      flags   オープン フラグ (O_RDONLY、O_WRONLY、O_RDWR など)。
     *  @param[in]      mode    ファイル生成時のパーミッション (O_CREAT 指定時に使用)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format  パスを構築する printf 形式の書式文字列。
     *  @param[in]      ...     書式引数。
     *  @return         成功時はファイル記述子、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_open_fmt(int flags, int mode, cplat_error *detail_out, const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 4, 5)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          書式指定パスでファイル記述子を開きます (`cplat_open_fmt` の `va_list` 版)。
     *  @param[in]      flags   オープン フラグ (O_RDONLY、O_WRONLY、O_RDWR など)。
     *  @param[in]      mode    ファイル生成時のパーミッション (O_CREAT 指定時に使用)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format  パスを構築する printf 形式の書式文字列。
     *  @param[in]      args    書式引数リスト。
     *  @return         成功時はファイル記述子、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vopen_fmt(int flags, int mode, cplat_error *detail_out, const char *format,
                                               va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 4, 0)))
#endif /* COMPILER_GCC */
        ;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRT_FCNTL_H */
