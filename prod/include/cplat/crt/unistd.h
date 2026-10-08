/**
 *******************************************************************************
 *  @file           unistd.h
 *  @brief          unistd および io 系の CRT 関数を抽象化する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/22
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_CRT_UNISTD_H
#define CPLAT_CRT_UNISTD_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cplat/base/compiler.h>
#include <cplat/base/error.h>
#include <cplat/base/platform.h>
#include <cplat/cplat_export.h>

#if defined(PLATFORM_LINUX)
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <io.h>
#endif /* PLATFORM_ */

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

#ifdef DOXYGEN
    #define CPLAT_ACCESS_FMT_F_OK 0 /**< ファイルの存在確認 (F_OK 相当)。 */
    #define CPLAT_ACCESS_FMT_R_OK 4 /**< 読み取り可能確認 (R_OK 相当)。 */
    #define CPLAT_ACCESS_FMT_W_OK 2 /**< 書き込み可能確認 (W_OK 相当)。 */
#else                               /* !DOXYGEN */
    #if defined(PLATFORM_LINUX)
        #define CPLAT_ACCESS_FMT_F_OK F_OK
        #define CPLAT_ACCESS_FMT_R_OK R_OK
        #define CPLAT_ACCESS_FMT_W_OK W_OK
    #elif defined(PLATFORM_WINDOWS)
        #define CPLAT_ACCESS_FMT_F_OK 0
        #define CPLAT_ACCESS_FMT_R_OK 4
        #define CPLAT_ACCESS_FMT_W_OK 2
    #endif /* PLATFORM_ */
#endif     /* DOXYGEN */

/**
 *  @brief          標準ストリームの種別を表す列挙型です。
 *
 *  cplat_isatty() の引数に使います。
 */
typedef enum cplat_stream
{
    CPLAT_STREAM_STDIN = 0,  /**< 標準入力 (stdin)。 */
    CPLAT_STREAM_STDOUT = 1, /**< 標準出力 (stdout)。 */
    CPLAT_STREAM_STDERR = 2  /**< 標準エラー出力 (stderr)。 */
} cplat_stream;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          指定したストリームが端末 (コンソール/TTY) に接続されているかを判定します。
     *  @param[in]      stream  判定するストリーム (@ref CPLAT_STREAM_STDIN 等)。
     *  @return         端末に接続されている場合は 1、それ以外 (リダイレクト、パイプ、
     *                  不正な @p stream 値) は 0 を返します。
     *
     *  Windows 環境では `GetFileType` が `FILE_TYPE_CHAR` を返し、
     *  かつ `GetConsoleMode` が成功する場合にのみ 1 を返します。\n
     *  Linux 環境では POSIX の `isatty()` を使用します。\n
     *  POSIX の `isatty()` と異なり、引数はファイル記述子ではなく
     *  ストリーム enum (@ref cplat_stream) です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_isatty(cplat_stream stream);

    /**
     *  @brief          ファイル記述子の読み書き位置を移動します (`lseek` / `_lseeki64` ラッパー)。
     *  @param[in]      fd      対象のファイル記述子。cplat_open() が返した値を渡します。
     *  @param[in]      offset  @p whence を基準とする移動量 (バイト)。
     *  @param[in]      whence  基準位置 (`SEEK_SET`、`SEEK_CUR`、`SEEK_END` のいずれか)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時はファイル先頭からの新しい読み書き位置、失敗時 (引数不正または OS エラー時) は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるファイル記述子に対する呼び出しは同時に実行できます。\n
     *  同一 @p fd の読み書き位置はスレッド間で共有されるため、同一 @p fd に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int64_t CPLAT_API cplat_lseek(int fd, int64_t offset, int whence, cplat_error *detail_out);

    /**
     *  @brief          ファイル記述子を閉じます (`close` / `_close` ラッパー)。
     *  @param[in]      fd  閉じるファイル記述子。cplat_open() が返した値を渡します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は 0、失敗時 (引数不正または OS エラー時) は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるファイル記述子に対する呼び出しは同時に実行できます。\n
     *  同一 @p fd を複数スレッドで同時に閉じる操作は二重クローズとなるため、同一 @p fd に対する操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_close(int fd, cplat_error *detail_out);

    /**
     *  @brief          ファイル記述子を複製します (`dup` / `_dup` ラッパー)。
     *  @param[in]      fd  複製元のファイル記述子。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は新しいファイル記述子、失敗時 (引数不正または OS エラー時) は -1 を返します。
     *
     *  複製されたファイル記述子は複製元と読み書き位置を共有します。\n
     *  不要になったら cplat_close() で閉じてください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_dup(int fd, cplat_error *detail_out);

    /**
     *  @brief          ファイル記述子を指定番号へ複製します (`dup2` / `_dup2` ラッパー)。
     *  @param[in]      oldfd  複製元のファイル記述子。
     *  @param[in]      newfd  複製先のファイル記述子番号。開いている場合は先に閉じられます。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は 0、失敗時 (引数不正または OS エラー時) は -1 を返します。
     *
     *  POSIX の `dup2` は成功時に @p newfd を返しますが、
     *  本関数は Windows の `_dup2` と挙動を揃えるため、成功時は常に 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_dup2(int oldfd, int newfd, cplat_error *detail_out);

    /**
     *  @brief          ファイル記述子からデータを読み取ります (`read` / `_read` ラッパー)。
     *  @param[in]      fd     読み取り元のファイル記述子。
     *  @param[out]     buf    読み取ったデータの格納先。NULL を渡してはなりません。
     *  @param[in]      count  読み取る最大バイト数。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は読み取ったバイト数 (ファイル終端では 0)、失敗時 (引数不正または OS エラー時) は -1 を返します。
     *
     *  要求した @p count より少ないバイト数で戻る場合があります。\n
     *  Windows 環境では 1 回の呼び出しで読み取れる上限が `INT_MAX` バイトであり、
     *  それを超える @p count は上限までに切り詰められます。\n
     *  必要なバイト数に達するまで読み取る場合は、呼び出し側でループしてください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるファイル記述子に対する呼び出しは同時に実行できます。\n
     *  同一 @p fd の読み書き位置はスレッド間で共有されるため、同一 @p fd に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int64_t CPLAT_API cplat_read(int fd, void *buf, size_t count, cplat_error *detail_out);

    /**
     *  @brief          ファイル記述子へデータを書き込みます (`write` / `_write` ラッパー)。
     *  @param[in]      fd     書き込み先のファイル記述子。
     *  @param[in]      buf    書き込むデータ。NULL を渡してはなりません。
     *  @param[in]      count  書き込むバイト数。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は書き込んだバイト数、失敗時 (引数不正または OS エラー時) は -1 を返します。
     *
     *  要求した @p count より少ないバイト数で戻る場合があります。\n
     *  Windows 環境では 1 回の呼び出しで書き込める上限が `INT_MAX` バイトであり、
     *  それを超える @p count は上限までに切り詰められます。\n
     *  必要なバイト数を書き終えるまで、呼び出し側でループしてください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるファイル記述子に対する呼び出しは同時に実行できます。\n
     *  同一 @p fd の読み書き位置はスレッド間で共有されるため、同一 @p fd に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int64_t CPLAT_API cplat_write(int fd, const void *buf, size_t count, cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスのアクセス確認 (`access` / `_waccess` ラッパー) です。
     *  @param[in]      path  確認対象のファイル パス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      mode  確認するアクセス種別 (@ref CPLAT_ACCESS_FMT_F_OK 等)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         アクセス可能時は 0、不可時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_access(const char *path, int mode, cplat_error *detail_out);

    /**
     *  @brief          書式指定パスのアクセス確認です。
     *  @param[in]      mode    確認するアクセス種別 (@ref CPLAT_ACCESS_FMT_F_OK 等)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format  パスを構築する printf 形式の書式文字列。
     *  @param[in]      ...     書式引数。
     *  @return         アクセス可能時は 0、不可時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_access_fmt(int mode, cplat_error *detail_out, const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 3, 4)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          書式指定パスのアクセス確認 (`cplat_access_fmt` の `va_list` 版) です。
     *  @param[in]      mode    確認するアクセス種別 (@ref CPLAT_ACCESS_FMT_F_OK 等)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format  パスを構築する printf 形式の書式文字列。
     *  @param[in]      args    書式引数リスト。
     *  @return         アクセス可能時は 0、不可時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vaccess_fmt(int mode, cplat_error *detail_out, const char *format, va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 3, 0)))
#endif /* COMPILER_GCC */
        ;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRT_UNISTD_H */
