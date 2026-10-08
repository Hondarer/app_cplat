/**
 *******************************************************************************
 *  @file           dll_exports.h
 *  @brief          Windows DLL エクスポートおよび呼び出し規約マクロの共通テンプレートを提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/09
 *  @version        1.0.0
 *
 *  呼び出し側はプレフィックスごとの `PREFIX_STATIC` / `PREFIX_EXPORTS`
 *  を定義してから、このヘッダーをインクルードしてください。
 *
 *  ソース コード中で定義する場合は `0` または `1` を明示してください。
 *  makefile などからコンパイラ オプション (`-DNAME` / `/DNAME`) で値なし定義
 *  する場合は、対応コンパイラが暗黙に `1` を与える前提で扱います。
 *
 *  例:
    @code{.c}
    #ifndef CALC_STATIC
        #define CALC_STATIC 0
    #endif
    #ifndef CALC_EXPORTS
        #define CALC_EXPORTS 0
    #endif
    #include <cplat/base/dll_exports.h>
    #define CALC_EXPORT CPLAT_DLL_EXPORT(CALC)
    #define CALC_API    CPLAT_DLL_API(CALC)
    @endcode
 *
 *  makefile から渡す場合の例:
    @code{.mk}
    CFLAGS += /DCALC_EXPORTS
    @endcode
 *
 *  上記の例では判定に以下のプレフィックス派生マクロを参照します。
 *  - CALC_STATIC
 *  - CALC_EXPORTS
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#include <cplat/base/platform.h>

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#ifndef DOXYGEN
    #ifndef CPLAT_DLL_PP_CAT_IMPL
        #define CPLAT_DLL_PP_CAT_IMPL(a, b) a##b
    #endif /* CPLAT_DLL_PP_CAT_IMPL */
    #ifndef CPLAT_DLL_PP_CAT
        #define CPLAT_DLL_PP_CAT(a, b) CPLAT_DLL_PP_CAT_IMPL(a, b)
    #endif /* CPLAT_DLL_PP_CAT */
    #ifndef CPLAT_DLL_IF_0
        #define CPLAT_DLL_IF_0(true_branch, false_branch) false_branch
    #endif /* CPLAT_DLL_IF_0 */
    #ifndef CPLAT_DLL_IF_1
        #define CPLAT_DLL_IF_1(true_branch, false_branch) true_branch
    #endif /* CPLAT_DLL_IF_1 */
    #ifndef CPLAT_DLL_IF
        #define CPLAT_DLL_IF(cond) CPLAT_DLL_PP_CAT(CPLAT_DLL_IF_, cond)
    #endif /* CPLAT_DLL_IF */
#endif     /* !DOXYGEN */

#ifdef DOXYGEN
    /**
     *  @brief          共有ライブラリのエクスポート/インポート修飾子です。
     *
     *  プラットフォームとビルド構成に応じて展開されます。
     *
     *  | 条件                                              | 展開値                                      |
     *  | ------------------------------------------------- | ------------------------------------------- |
     *  | Linux / 静的リンク (`PREFIX_STATIC` 定義あり)     | (空)                                        |
     *  | Linux / 共有ライブラリ (静的リンクでない)         | `__attribute__((visibility("default")))`   |
     *  | Windows / 静的リンク (`PREFIX_STATIC` 定義あり)   | (空)                                        |
     *  | Windows / DLL ビルド (`PREFIX_EXPORTS` 定義あり)  | `__declspec(dllexport)`                     |
     *  | Windows / DLL 利用側                              | `__declspec(dllimport)`                     |
     *
     *  Table: 共有ライブラリのエクスポート・インポート修飾子
     *
     *  Linux の共有ライブラリはビルド時に `-fvisibility=hidden` を併用し、
     *  本マクロを付けた公開 API だけを動的シンボル表へ載せます。
     */
    #define CPLAT_DLL_EXPORT(prefix)

    /**
     *  @brief          呼び出し規約修飾子です。
     *
     *  プラットフォームに応じて展開されます。
     *
     *  | 条件    | 展開値        |
     *  | ------- | ------------- |
     *  | Linux   | (空)          |
     *  | Windows | `__stdcall`   |
     *
     *  Table: プラットフォーム別の呼び出し規約修飾子
     */
    #define CPLAT_DLL_API(prefix)
#else /* !DOXYGEN */
    #if defined(PLATFORM_LINUX)
        /*
         * 共有ライブラリでは既定を hidden にし (makelibsrc)、公開 API だけ default にする。
         * 静的リンク時は属性不要。Windows の dllexport/dllimport 分岐と異なり、
         * 利用側でも default 可視性の宣言で問題ないため EXPORTS は見ない。
         */
        #define CPLAT_DLL_EXPORT(prefix) \
            CPLAT_DLL_IF(CPLAT_DLL_PP_CAT(prefix, _STATIC))(, __attribute__((visibility("default"))))
        #define CPLAT_DLL_API(prefix)
    #elif defined(PLATFORM_WINDOWS)
        #ifndef __INTELLISENSE__
            #define CPLAT_DLL_EXPORT(prefix) \
                CPLAT_DLL_IF(CPLAT_DLL_PP_CAT(prefix, _STATIC)) \
                (, CPLAT_DLL_IF(CPLAT_DLL_PP_CAT(prefix, _EXPORTS))(__declspec(dllexport), __declspec(dllimport)))
        #else /* __INTELLISENSE__ */
            #define CPLAT_DLL_EXPORT(prefix)
        #endif /* __INTELLISENSE__ */
        #define CPLAT_DLL_API(prefix) __stdcall
    #else /* !PLATFORM_LINUX && !PLATFORM_WINDOWS */
        #define CPLAT_DLL_EXPORT(prefix)
        #define CPLAT_DLL_API(prefix)
    #endif /* PLATFORM_ */
#endif     /* DOXYGEN */

/** @} */
