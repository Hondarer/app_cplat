/**
 *******************************************************************************
 *  @file           stack_limit_linux.h
 *  @brief          Linux のスタック サイズの資源上限を取得・引き上げる API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/30
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_STACK_LIMIT_LINUX_H
#define CPLAT_STACK_LIMIT_LINUX_H

#include <stdint.h>
#include <cplat/base/platform.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

#if defined(PLATFORM_LINUX)

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          スタック サイズに上限がないこと (`RLIM_INFINITY`) を表す値です。
     */
    #define CPLAT_STACK_LIMIT_LINUX_UNLIMITED UINT64_MAX

    /**
     *  @brief          自プロセスのスタック サイズの資源上限 (`RLIMIT_STACK`) を取得します。
     *  @param[out]     soft_limit_bytes_out  ソフト リミット (バイト) の格納先。上限がない場合は
     *                                        @ref CPLAT_STACK_LIMIT_LINUX_UNLIMITED を格納します。
     *                                        NULL を渡してはなりません。
     *  @param[out]     hard_limit_bytes_out  ハード リミット (バイト) の格納先。上限がない場合は
     *                                        @ref CPLAT_STACK_LIMIT_LINUX_UNLIMITED を格納します。
     *                                        NULL を渡してはなりません。
     *  @param[out]     detail_out            エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                        エラー詳細を設定せず、返却しません。
     *                                        NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  Linux 専用の API です。呼び出し元は `#if defined(PLATFORM_LINUX)` の中でのみ使用してください。\n
     *  失敗時は @p detail_out とスレッド ローカルの直前値 (cplat_error_get_last()) に errno を記録し、
     *  成功時は両方をクリアします。\n
     *  失敗時に @p soft_limit_bytes_out と @p hard_limit_bytes_out は変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_stack_limit_linux_get(uint64_t *soft_limit_bytes_out,
                                                           uint64_t *hard_limit_bytes_out, cplat_error *detail_out);

    /**
     *  @brief          自プロセスのスタック サイズのソフト リミットを、ハード リミットまで引き上げます。
     *  @param[out]     soft_limit_bytes_out  処理後のソフト リミット (バイト) の格納先。上限がない場合は
     *                                        @ref CPLAT_STACK_LIMIT_LINUX_UNLIMITED を格納します。
     *                                        NULL を指定した場合は格納しません。
     *  @param[out]     detail_out            エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                        エラー詳細を設定せず、返却しません。
     *                                        NULL 以外を指定した場合、成功時と省略時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_SKIPPED 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  Linux 専用の API です。呼び出し元は `#if defined(PLATFORM_LINUX)` の中でのみ使用してください。\n
     *  自プロセスのスタック サイズの資源上限 (`RLIMIT_STACK`) のソフト リミットを
     *  ハード リミットまで引き上げます。ハード リミットは変更しません。\n
     *  特権を必要としません。ハード リミットが上限なしの場合、ソフト リミットも上限なしになります。\n
     *  ソフト リミットがすでにハード リミットと等しい場合は、何もせず @ref CPLAT_SKIPPED を返します。\n
     *  失敗時は @p detail_out とスレッド ローカルの直前値 (cplat_error_get_last()) に errno を記録し、
     *  成功時と省略時は両方をクリアします。\n
     *  失敗時に @p soft_limit_bytes_out は変更しません。
     *
     *  @par            効果が及ぶ範囲
     *  @parblock
     *  ソフト リミットは、メイン スレッドのスタックが伸びるたびにカーネルが参照します。
     *  引き上げは、呼び出した時点からメイン スレッドのスタックの拡張に適用されます。\n
     *  ただし、自プロセス内の次の範囲には及びません。
     *
     *  - 既存のスレッドと、以降に作成するスレッドのスタック。
     *    glibc は、スレッドの既定のスタック サイズをプロセスの起動時のソフト リミットから決定して保持します。
     *    大きなスタックが必要なスレッドは、作成時にスタック サイズを指定してください。
     *  - メイン スレッドのスタックと、その下に配置される共有ライブラリなどとの間隔。
     *    間隔はプログラムの起動時のソフト リミットから決まり、スタックはその間隔を超えて伸びません。
     *    アドレス空間配置のランダム化 (ASLR) が無効な場合、間隔は約 128 MiB です。
     *  @endparblock
     *
     *  @par            子プロセスへの引き継ぎ
     *  @parblock
     *  引き上げたソフト リミットは、`fork()` と `execve()` を越えて子プロセスへ引き継がれますが、
     *  子プロセスへ適用される範囲は起動方法と上限値によって異なります。
     *
     *  - `fork()` のみ (`execve()` しない) の場合:
     *    子プロセスのメイン スレッドには引き上げたソフト リミットが引き継がれます。
     *    ただし、スタック下の配置間隔と、glibc が保持するスレッドの既定のスタック サイズは、
     *    親プロセスの起動時の値のままです。
     *  - `fork()` の後に `execve()` した場合:
     *    子プロセスのメイン スレッドは引き上げたソフト リミットで起動し、スタック下の配置間隔も
     *    その値から決定されます。\n
     *    子プロセスが作成するスレッドの既定のスタック サイズは、引き上げたソフト リミットが
     *    有限値であればその値になります。一方、上限なし (`RLIM_INFINITY`) の場合、glibc は
     *    固定長のスタック領域を確保するためアーキテクチャー既定値 (`ARCH_STACK_DEFAULT_SIZE`、
     *    x86_64 では 2 MiB) を使用します。
     *  @endparblock
     *
     *  @attention      戻り値を `CPLAT_OK` との一致だけで判定すると、@ref CPLAT_SKIPPED を失敗として扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。他のスレッドが同時に資源上限を変更した場合は、
     *  後に実行された変更が残ります。
     */
    CPLAT_EXPORT int CPLAT_API cplat_stack_limit_linux_raise_to_hard_limit(uint64_t *soft_limit_bytes_out,
                                                                           cplat_error *detail_out);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_LINUX */

/** @} */

#endif /* CPLAT_STACK_LIMIT_LINUX_H */
