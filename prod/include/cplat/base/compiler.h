/**
 *******************************************************************************
 *  @file           compiler.h
 *  @brief          コンパイラを検出し、差異を抽象化するマクロを提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/02/06
 *
 *  コンパイラの種類とバージョンを検出し、統一的なマクロを定義します。\n
 *  また、コンパイラ固有のインライン制御属性とスレッド ローカル記憶域を抽象化します。
 *
 *  @section        compiler_detection コンパイラ検出マクロ
 *
 *  検出されたコンパイラに応じて、以下のマクロを定義します。
 *
 *  | コンパイラ | 識別マクロ       | COMPILER_NAME | COMPILER_VERSION の形式             |
 *  | ---------- | ---------------- | ------------- | ----------------------------------- |
 *  | GCC        | COMPILER_GCC     | "GCC"         | major * 10000 + minor * 100 + patch |
 *  | MSVC       | COMPILER_MSVC    | "MSVC"        | _MSC_VER の値 (例: 1943)            |
 *  | その他     | COMPILER_UNKNOWN | "Unknown"     | 0                                   |
 *
 *  Table: コンパイラの識別マクロとバージョン形式
 *
 *  @section        inline_control インライン制御マクロ
 *
 *  コンパイラごとに異なるインライン強制・抑制の構文を、統一的なマクロで提供します。
 *
 *  | マクロ名     | GCC                                   | MSVC                  | その他 |
 *  | ------------ | ------------------------------------- | --------------------  | ------ |
 *  | FORCE_INLINE | inline __attribute__((always_inline)) | __forceinline         | inline |
 *  | NO_INLINE    | __attribute__((noinline))             | __declspec(noinline)  | (空)   |
 *
 *  Table: コンパイラ別のインライン制御マクロ
 *
 *  @section        thread_local_storage スレッド ローカル記憶域マクロ
 *
 *  | マクロ名              | GCC の C        | GCC の C++     | MSVC                 | その他 |
 *  | --------------------- | --------------- | -------------- | -------------------- | ------ |
 *  | THREAD_LOCAL          | `_Thread_local` | `thread_local` | `__declspec(thread)` | (空)   |
 *
 *  Table: コンパイラ別のスレッド ローカル記憶域マクロ
 *
 *  @section        usage 使用例
    @code{.c}
    #include "compiler.h"
    #include <cplat/crt/stdio.h>

    FORCE_INLINE int fast_add(int a, int b)
    {
        return a + b;
    }

    NO_INLINE void debug_dump(const char *msg)
    {
        cplat_fprintf(stderr, "%s\n", msg);
    }
    @endcode
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef COMPILER_H
#define COMPILER_H

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#ifdef DOXYGEN
    #define COMPILER_GCC            /**< GCC コンパイラの場合に定義されます。 */
    #define COMPILER_MSVC           /**< MSVC コンパイラの場合に定義されます。 */
    #define COMPILER_UNKNOWN        /**< 未知のコンパイラの場合に定義されます。 */
    #define COMPILER_NAME    "name" /**< コンパイラ名の文字列 ("MSVC", "GCC", "Unknown")。 */
    #define COMPILER_VERSION 0      /**< コンパイラ バージョンの数値。 */
#else                               /* !DOXYGEN */
    #if defined(_MSC_VER)
        #define COMPILER_MSVC
        #define COMPILER_NAME    "MSVC"
        #define COMPILER_VERSION _MSC_VER
    #elif defined(__GNUC__) && !defined(__clang__)
        #define COMPILER_GCC
        #define COMPILER_NAME    "GCC"
        #define COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    #else
        #define COMPILER_UNKNOWN
        #define COMPILER_NAME    "Unknown"
        #define COMPILER_VERSION 0
    #endif
#endif /* DOXYGEN */

#ifdef DOXYGEN
    #define FORCE_INLINE /**< インライン展開を強制します。コンパイラに応じた属性に展開されます。 */
    #define NO_INLINE    /**< インライン展開を抑制します。コンパイラに応じた属性に展開されます。 */
    #define NO_RETURN    /**< 関数が呼び出し元へ戻らないことを示します。コンパイラに応じた属性に展開されます。 */
#else                    /* !DOXYGEN */
    #if defined(COMPILER_GCC)
        #define FORCE_INLINE inline __attribute__((always_inline))
        #define NO_INLINE    __attribute__((noinline))
        #define NO_RETURN    __attribute__((noreturn))
    #elif defined(COMPILER_MSVC)
        #define FORCE_INLINE __forceinline
        #define NO_INLINE    __declspec(noinline)
        #define NO_RETURN    __declspec(noreturn)
    #else /* !COMPILER_GCC && !COMPILER_MSVC */
        #define FORCE_INLINE inline
        #define NO_INLINE
        #define NO_RETURN
    #endif /* COMPILER_ */
#endif     /* DOXYGEN */

#ifdef DOXYGEN
    #define THREAD_LOCAL /**< スレッドごとに独立した記憶域を定義します。 */
#else                    /* !DOXYGEN */
    #if defined(COMPILER_MSVC)
        #define THREAD_LOCAL __declspec(thread)
    #elif defined(COMPILER_GCC)
        #if defined(__cplusplus)
            #define THREAD_LOCAL thread_local
        #else
            #define THREAD_LOCAL _Thread_local
        #endif
    #else /* !COMPILER_MSVC && !COMPILER_GCC */
        #define THREAD_LOCAL
    #endif
#endif /* DOXYGEN */

/** @} */

#endif /* COMPILER_H */
