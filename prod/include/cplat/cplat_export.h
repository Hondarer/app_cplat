/**
 *******************************************************************************
 *  @file           cplat_export.h
 *  @brief          cplat の Windows DLL エクスポートおよび呼び出し規約マクロを定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/21
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_EXPORT_H
#define CPLAT_EXPORT_H

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#ifdef DOXYGEN

    /**
     *  @brief          DLL エクスポート/インポート制御マクロです。
     *
     *  ビルド条件に応じて次の値を取ります。
     *
     *  | 条件                                               | 値                                        |
     *  | -------------------------------------------------- | ----------------------------------------- |
     *  | Linux / `CPLAT_STATIC` 定義時 (静的リンク)      | (空)                                      |
     *  | Linux / 共有ライブラリ (静的リンクでない)          | `__attribute__((visibility("default")))` |
     *  | Windows / `__INTELLISENSE__` 定義時                | (空)                                      |
     *  | Windows / `CPLAT_STATIC` 定義時 (静的リンク)    | (空)                                      |
     *  | Windows / `CPLAT_EXPORTS` 定義時 (DLL ビルド)   | `__declspec(dllexport)`                   |
     *  | Windows / `CPLAT_EXPORTS` 未定義時 (DLL 利用側) | `__declspec(dllimport)`                   |
     *
     *  Table: CPLAT_EXPORT のビルド条件別の展開値
     */
    #define CPLAT_EXPORT

    /**
     *  @brief          呼び出し規約マクロです。
     *
     *  Windows 環境では `__stdcall` 呼び出し規約を指定します。\n
     *  Linux (非 Windows) 環境では空に展開されます。
     */
    #define CPLAT_API

#else /* !DOXYGEN */

    #ifndef CPLAT_STATIC
        #define CPLAT_STATIC 0
    #endif /* CPLAT_STATIC */
    #ifndef CPLAT_EXPORTS
        #define CPLAT_EXPORTS 0
    #endif /* CPLAT_EXPORTS */
    #include <cplat/base/dll_exports.h>
    #define CPLAT_EXPORT CPLAT_DLL_EXPORT(CPLAT)
    #define CPLAT_API    CPLAT_DLL_API(CPLAT)

#endif /* DOXYGEN */

/** @} */

#endif /* CPLAT_EXPORT_H */
