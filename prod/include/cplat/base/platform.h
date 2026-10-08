/**
 *******************************************************************************
 *  @file           platform.h
 *  @brief          プラットフォームとアーキテクチャーを検出するマクロを提供します。
 *  @author         Tetsuo Honda
 *  @date           2025/11/22
 *
 *  ビルド対象の OS とプロセッサ アーキテクチャーを検出し、統一的なマクロを定義します。\n
 *  cplat は 64 ビット環境専用です。32 ビット環境ではコンパイル時にエラーとします。\n
 *  MSVC の対応対象は x64 に限定します。
 *
 *  @section        platform_detection プラットフォーム検出マクロ
 *
 *  検出されたプラットフォームに応じて、以下のマクロを定義します。
 *
 *  | プラットフォーム | 識別マクロ           | PLATFORM_NAME       |
 *  | ---------------- | -------------------- | ------------------- |
 *  | Windows          | PLATFORM_WINDOWS     | "Windows"           |
 *  | Linux            | PLATFORM_LINUX       | "Linux"             |
 *  | その他           | PLATFORM_UNKNOWN     | "Unknown"           |
 *
 *  Table: プラットフォームの識別マクロと名称
 *
 *  @section        arch_detection アーキテクチャー検出マクロ
 *
 *  検出されたアーキテクチャーに応じて、以下のマクロを定義します。
 *
 *  | アーキテクチャ | ARCH_NAME |
 *  | -------------- | --------- |
 *  | x86_64         | "x64"     |
 *  | その他         | "Unknown" |
 *
 *  Table: アーキテクチャーの名称
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2025-2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "compiler.h"

#if defined(__cplusplus)
static_assert(sizeof(void *) == 8, "cplat: requires a 64-bit environment");
#else
_Static_assert(sizeof(void *) == 8, "cplat: requires a 64-bit environment");
#endif /* __cplusplus */

#if defined(COMPILER_MSVC) && !defined(_M_X64)
    #error "cplat: MSVC requires the x64 target"
#endif /* COMPILER_MSVC && !_M_X64 */

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#ifdef DOXYGEN
    #define PLATFORM_WINDOWS        /**< Windows の場合に定義されます。 */
    #define PLATFORM_LINUX          /**< Linux の場合に定義されます。 */
    #define PLATFORM_UNKNOWN        /**< 未知のプラットフォームの場合に定義されます。 */
    #define PLATFORM_NAME    "name" /**< プラットフォーム名の文字列 ("Windows", "Linux", "Unknown")。 */
#else                               /* !DOXYGEN */
    #if defined(_WIN32)
        #define PLATFORM_WINDOWS
        #define PLATFORM_NAME "Windows"
    #elif defined(__linux__)
        #define PLATFORM_LINUX
        #define PLATFORM_NAME "Linux"
    #else
        #define PLATFORM_UNKNOWN
        #define PLATFORM_NAME "Unknown"
    #endif
#endif /* DOXYGEN */

#ifdef DOXYGEN
    #define ARCH_NAME "name" /**< アーキテクチャー名の文字列 ("x64", "Unknown")。 */
#else                        /* !DOXYGEN */
    #if defined(__x86_64__) || defined(_M_X64)
        #define ARCH_NAME "x64"
    #else
        #define ARCH_NAME "Unknown"
    #endif
#endif /* DOXYGEN */

/** @} */

#endif /* PLATFORM_H */
