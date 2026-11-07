/**
 *******************************************************************************
 *  @file           crt.h
 *  @brief          CRT 抽象 API をまとめて取り込みます。
 *  @author         Tetsuo Honda
 *  @date           2026/04/24
 *
 *  `cplat/crt` 配下の公開ヘッダーをまとめて取り込む標準入口です。\n
 *  CRT 抽象 API を広く利用する場合は、本ヘッダーの利用を推奨します。
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

#ifndef CPLAT_CRT_CRT_H
#define CPLAT_CRT_CRT_H

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

#include <cplat/crt/path.h>
#include <cplat/crt/fcntl.h>
#include <cplat/crt/file.h>
#include <cplat/crt/select.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/crt/time.h>
#include <cplat/crt/unistd.h>
#include <cplat/crt/sys/stat.h>
#include <cplat/crt/wchar_conv.h>

/** @} */

#endif /* CPLAT_CRT_CRT_H */
