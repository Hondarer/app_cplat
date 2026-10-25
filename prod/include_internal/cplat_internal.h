/**
 *******************************************************************************
 *  @file           cplat_internal.h
 *  @brief          cplat ライブラリの公開 API と内部 API をまとめて取り込みます。
 *  @author         Tetsuo Honda
 *  @date           2026/05/21
 *  @version        1.0.0
 *
 *  cplat ライブラリの内部ヘッダーを 1 つにまとめたヘッダーです。\n
 *  利用者は `#include <cplat_internal.h>` で本ライブラリの全公開 + 全内部 API にアクセスできます。
 *
 *  アンブレラ ヘッダーは利便性と引き換えにコンパイル時間がかかります。\n
 *  個別ヘッダーを利用するか、アンブレラ ヘッダーを利用するかは利用者にて選択してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_INTERNAL_H
#define CPLAT_INTERNAL_H

#include <cplat.h> /* 内部 API で公開定数、公開型、公開関数などに依存している可能性を考慮 */

#include <cplat/base/error_internal.h>
#include <cplat/base/error_message_internal.h>
#include <cplat/base/result_internal.h>

#include <cplat/console/console_internal.h>

#include <cplat/crt/path_format.h>

#include <cplat/prompt/prompt_edit.h>

#include <cplat/string_catalog/catalog_internal.h>
#include <cplat/string_catalog/format_internal.h>

#include <cplat/trace/tracer_internal.h>
#include <cplat/trace/backends/etw/etw_internal.h>
#include <cplat/trace/backends/file/trace_file_internal.h>
#include <cplat/trace/backends/syslog/syslog_internal.h>

#endif /* CPLAT_INTERNAL_H */
