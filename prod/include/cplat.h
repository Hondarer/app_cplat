/**
 *******************************************************************************
 *  @file           cplat.h
 *  @brief          cplat ライブラリの公開 API をまとめて取り込みます。
 *  @author         Tetsuo Honda
 *  @date           2026/05/21
 *  @version        1.0.0
 *
 *  cplat ライブラリの公開ヘッダーを 1 つにまとめたヘッダーです。\n
 *  利用者は `#include <cplat.h>` で本ライブラリの全公開 API にアクセスできます。
 *
 *  アンブレラ ヘッダーは利便性と引き換えにコンパイル時間がかかります。\n
 *  個別ヘッダーを利用するか、アンブレラ ヘッダーを利用するかは利用者にて選択してください。
 *
 *  @par            除外ヘッダー
 *  以下のヘッダーはインクルードされた翻訳単位に実装コードを出力するため、
 *  このアンブレラ ヘッダーには含まれていません。必要な場合は個別にインクルードしてください。
 *  - `<cplat/base/shared_lib_lifecycle.h>` :
 *    `__attribute__((constructor))` / `DllMain` を定義し、
 *    利用側に `onLoad()` / `onUnload()` の実装を要求します。
 *  - `<cplat/test/syslog_test.h>` :
 *    Linux 限定で `static` 関数 `syslog_test_fd_write()` を定義します。
 *
 *  また、以下のヘッダーは特定プラットフォーム専用の API のみを宣言するため、
 *  このアンブレラ ヘッダーには含まれていません。
 *  利用箇所を `#if defined(PLATFORM_*)` で明示する前提のもと、個別にインクルードしてください。
 *  - `<cplat/trace/etw.h>` : Windows 専用 (ETW TraceLogging)。
 *  - `<cplat/trace/eventlog.h>` : Windows 専用 (イベント ログ)。
 *  - `<cplat/trace/syslog.h>` : Linux 専用 (syslog)。
 *  - `<cplat/win32/win32.h>` : Windows 専用 (Win32 API の UTF-8 ラッパー)。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_H
#define CPLAT_H

/**
 *  @defgroup       CPLAT_PUBLIC_API 公開 API (cplat)
 *  @brief          cplat ライブラリの公開 API です。
 */

/* カテゴリ別サブグループの定義です。各公開ヘッダーは自身のカテゴリへ @ingroup します。 */

/**
 *  @defgroup       CPLAT_BASE 基盤 (base)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          コンパイラ / プラットフォーム判定、DLL エクスポート、結果コードなどの基盤機能です。
 */

/**
 *  @defgroup       CPLAT_ARGPARSER 引数解析 (argparser)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          コマンド ライン引数の登録と解析を行う API です。
 */

/**
 *  @defgroup       CPLAT_CLOCK 時刻 (clock)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          単調時刻・実時刻の取得、標準時刻型 cplat_timespec とその演算 API です。
 */

/**
 *  @defgroup       CPLAT_COMPRESS 圧縮 (compress)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          データの圧縮・伸張 API です。
 */

/**
 *  @defgroup       CPLAT_CONSOLE コンソール (console)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          コンソールの初期化と UTF-8 出力を抽象化する API です。
 */

/**
 *  @defgroup       CPLAT_CRT CRT 抽象 (crt)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          C 標準ライブラリ / POSIX 関数のプラットフォーム差異を抽象化する API です。
 */

/**
 *  @defgroup       CPLAT_CRYPTO 暗号 (crypto)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          AES-256-GCM による暗号化・復号 API です。
 */

/**
 *  @defgroup       CPLAT_HASHTABLE ハッシュ テーブル (hashtable)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          固定長スロットと遅延削除を持つハッシュ テーブル API です。
 */

/**
 *  @defgroup       CPLAT_LOCALE ロケール (locale)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          実行環境が示す表示言語を取得する API です。
 */

/**
 *  @defgroup       CPLAT_MMAP メモリ マップド ファイル (mmap)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          メモリ マップド ファイルの割り当てと共有 API です。
 */

/**
 *  @defgroup       CPLAT_NET ネットワーク (net)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          IPv4 ソケットの生成、接続、送受信、アドレス解決 API です。
 */

/**
 *  @defgroup       CPLAT_PROMPT プロンプト (prompt)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          対話プロンプトの入力・表示 API です。
 */

/**
 *  @defgroup       CPLAT_REGEX 正規表現 (regex)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          UTF-8 文字列に対する正規表現の照合 API です。
 */

/**
 *  @defgroup       CPLAT_RUNTIME ランタイム (runtime)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          プロセス、モジュール、シャットダウン、メモリ ロックなどのランタイム支援 API です。
 */

/**
 *  @defgroup       CPLAT_STRING_CATALOG 文字列カタログ (string_catalog)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          文字列キー、引数スキーマ、言語別リソースを分離して文字列を組み立てる API です。
 */

/**
 *  @defgroup       CPLAT_SYNC 同期 (sync)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          スレッド間・プロセス間の同期プリミティブおよびアトミック操作 API です。
 */

/**
 *  @defgroup       CPLAT_TRACE トレース (trace)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          レベル別トレース出力と各バックエンド (ファイル、syslog、ETW、イベント ログ) の API です。
 */

/**
 *  @defgroup       CPLAT_WIN32 Win32 UTF-8 ラッパー (win32)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          Win32 API を UTF-8 文字列で呼び出す U サフィックスのラッパー API です。
 */

/**
 *  @defgroup       CPLAT_TEST テスト支援 (test)
 *  @ingroup        CPLAT_PUBLIC_API
 *  @brief          テストからの利用を想定した補助 API です。
 */

#include <cplat/base/compiler.h>
#include <cplat/base/platform.h>
#include <cplat/base/dll_exports.h>
#include <cplat/base/error.h>
#include <cplat/base/error_message.h>
#include <cplat/base/result.h>
#include <cplat/base/windows_sdk.h>

#include <cplat/argparser/argparser.h>

#include <cplat/clock/clock.h>
#include <cplat/clock/timespec.h>
#include <cplat/compress/compress.h>
#include <cplat/console/console.h>

#include <cplat/crt/crt.h>
#include <cplat/crt/fcntl.h>
#include <cplat/crt/file.h>
#include <cplat/crt/path.h>
#include <cplat/crt/select.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/crt/sys/stat.h>
#include <cplat/crt/time.h>
#include <cplat/crt/unistd.h>
#include <cplat/crt/wchar_conv.h>

#include <cplat/crypto/crypto.h>
#include <cplat/crypto/random.h>

#include <cplat/hashtable/hashtable.h>

#include <cplat/locale/ui_language.h>

#include <cplat/mmap/mmap.h>

#include <cplat/net/byteorder.h>
#include <cplat/net/endpoint.h>
#include <cplat/net/socket.h>

#include <cplat/prompt/pinned_prompt.h>
#include <cplat/prompt/prompt.h>

#include <cplat/regex/regex.h>

#include <cplat/runtime/assertion.h>
#include <cplat/runtime/host.h>
#include <cplat/runtime/module.h>
#include <cplat/runtime/memory_lock.h>
#include <cplat/runtime/process.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/runtime/sym_loader.h>

#include <cplat/string_catalog/argument.h>
#include <cplat/string_catalog/catalog.h>
#include <cplat/string_catalog/language.h>
#include <cplat/string_catalog/string_catalog.h>

#include <cplat/sync/sync.h>

#include <cplat/trace/trace_file.h>
#include <cplat/trace/tracer.h>

#endif /* CPLAT_H */
