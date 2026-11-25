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
 *  Table: プラットフォームの識別マクロと名称
 *
 *  | プラットフォーム | 識別マクロ           | PLATFORM_NAME       |
 *  | ---------------- | -------------------- | ------------------- |
 *  | Windows          | PLATFORM_WINDOWS     | "Windows"           |
 *  | Linux            | PLATFORM_LINUX       | "Linux"             |
 *  | その他           | PLATFORM_UNKNOWN     | "Unknown"           |
 *
 *  @section        arch_detection アーキテクチャー検出マクロ
 *
 *  検出されたアーキテクチャーに応じて、以下のマクロを定義します。
 *
 *  Table: アーキテクチャーの名称
 *
 *  | アーキテクチャ | ARCH_NAME |
 *  | -------------- | --------- |
 *  | x86_64         | "x64"     |
 *  | その他         | "Unknown" |
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2025-2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "compiler.h"

/*
 * 機能検査マクロ (_GNU_SOURCE など) は、ビルドの定義 (app/cplat/makepart.mk) で、最初の標準ヘッダーより前に与える。
 * glibc は、最初に取り込んだ標準ヘッダーの時点で機能検査マクロを確定するため、ヘッダーやソースの
 * 中で #define しても、先に標準ヘッダーを取り込んだ翻訳単位では効かない。
 * see: https://man7.org/linux/man-pages/man7/feature_test_macros.7.html
 */
#if !defined(__cplusplus)
    #include <assert.h>
#endif /* !__cplusplus */

static_assert(sizeof(void *) == 8, "cplat: requires a 64-bit environment");

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
    #endif /* _WIN32 / __linux__ */
#endif     /* DOXYGEN */

#ifdef DOXYGEN
    #define ARCH_NAME "name" /**< アーキテクチャー名の文字列 ("x64", "Unknown")。 */
#else                        /* !DOXYGEN */
    #if defined(__x86_64__) || defined(_M_X64)
        #define ARCH_NAME "x64"
    #else
        #define ARCH_NAME "Unknown"
    #endif /* __x86_64__ / _M_X64 */
#endif     /* DOXYGEN */

/** @} */

#endif /* PLATFORM_H */
