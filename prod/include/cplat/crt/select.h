/**
 *******************************************************************************
 *  @file           select.h
 *  @brief          select 系の FD 集合操作を安全に使用する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/05
 *
 *  Linux と Windows の `fd_set` へ FD を追加する操作を提供します。
 *  ネイティブの `FD_SET()` が検査しない、または通知しない追加の失敗を検出し、
 *  集合が正しく更新されないまま処理が続くことを防ぎます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_CRT_SELECT_H
#define CPLAT_CRT_SELECT_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #include <sys/select.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif /* PLATFORM_ */

#include <cplat/runtime/abort.h>

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

/* Linux の FD_SET() は、FD_SETSIZE 以上の値を検査せず fd_set へ書き込みます。
 * see: https://man7.org/linux/man-pages/man3/FD_SET.3.html */

/* Windows の fd_set は SOCKET の配列と格納数であり、FD_SETSIZE は格納できる
 * SOCKET の数の上限です。SOCKET の値そのものは FD_SETSIZE と無関係のため、
 * 値の範囲ではなく格納数の上限を検査します。
 * Windows の FD_SET() は集合が満杯の場合、追加せずに通知もしません。
 * see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-fd_set */

#ifdef DOXYGEN
    /**
     *  @brief          FD を集合へ範囲検査付きで追加します。
     *
     *  @param[in]      fd   追加する FD です。
     *  @param[in,out]  set  FD 集合の格納先です。NULL を渡してはなりません。
     *
     *  @warning         @p fd を集合へ追加できない場合は、@ref CPLAT_ASSERT により
     *                   プロセスを異常終了させます。追加できないのは、Linux では
     *                   @p fd が 0 未満または `FD_SETSIZE` 以上の場合、Windows では
     *                   @p fd が `INVALID_SOCKET` の場合と、集合がすでに
     *                   `FD_SETSIZE` 個の SOCKET を保持している場合です。
     *
     *  @par            スレッド セーフ
     *  本マクロはスレッド セーフではありません。\n
     *  同一の @p set を複数スレッドから同時に操作しないことを呼び出し側で保証してください。
     */
    #define CPLAT_FD_SET(fd, set) \
        do \
        { \
            (void)(fd); \
            (void)(set); \
        } while (0)
#elif defined(PLATFORM_LINUX)
    #define CPLAT_FD_SET(fd, set) \
        do \
        { \
            const int cplat_fd_set_fd = (fd); \
            fd_set *const cplat_fd_set_set = (set); \
            CPLAT_ASSERT((cplat_fd_set_fd >= 0) && (cplat_fd_set_fd < FD_SETSIZE)); \
            FD_SET(cplat_fd_set_fd, cplat_fd_set_set); \
        } while (0)
#elif defined(PLATFORM_WINDOWS)
    /* 定数を fd へ渡す使用も意図したものであるため、検査条件が定数になる場合の C4127 を
     * 抑制します。__pragma は MSVC の拡張のため、COMPILER_MSVC の場合だけ展開します。
     * suppress はマクロを展開した 1 行に効くため、抑制範囲はマクロ本体全体です。
     * see: https://learn.microsoft.com/en-us/cpp/preprocessor/warning */
    #if defined(COMPILER_MSVC)
        #define CPLAT_FD_SET_SUPPRESS_CONST_COND __pragma(warning(suppress : 4127))
    #else /* !COMPILER_MSVC */
        #define CPLAT_FD_SET_SUPPRESS_CONST_COND
    #endif /* COMPILER_MSVC */

    #define CPLAT_FD_SET(fd, set) \
        do \
        { \
            const SOCKET cplat_fd_set_fd = (SOCKET)(fd); \
            fd_set *const cplat_fd_set_set = (set); \
            CPLAT_FD_SET_SUPPRESS_CONST_COND CPLAT_ASSERT(cplat_fd_set_fd != INVALID_SOCKET); \
            FD_SET(cplat_fd_set_fd, cplat_fd_set_set); \
            CPLAT_ASSERT(FD_ISSET(cplat_fd_set_fd, cplat_fd_set_set) != 0); \
        } while (0)
#endif /* DOXYGEN / PLATFORM_ */

/** @} */

#endif /* CPLAT_CRT_SELECT_H */
