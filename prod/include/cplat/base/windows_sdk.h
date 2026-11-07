/**
 *******************************************************************************
 *  @file           windows_sdk.h
 *  @brief          Windows SDK の共通ヘッダーをまとめて取り込みます。
 *  @author         Tetsuo Honda
 *  @date           2026/04/20
 *  @version        1.0.0
 *
 *  cplat の公開ヘッダーから Windows SDK を参照する際の正本です。
 *  winsock2.h / ws2tcpip.h を windows.h より先に取り込むことで、
 *  winsock.h との衝突を防ぎます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_WINDOWS_SDK_H
#define CPLAT_WINDOWS_SDK_H

#include <cplat/base/platform.h>

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#if defined(PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif /* WIN32_LEAN_AND_MEAN */
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "Advapi32.lib")
    #ifdef byte
        #undef byte /* C++17 std::byte と Windows SDK byte typedef の競合を解消 */
    #endif          /* byte */
#endif              /* PLATFORM_WINDOWS */

/** @} */

#endif /* CPLAT_WINDOWS_SDK_H */
