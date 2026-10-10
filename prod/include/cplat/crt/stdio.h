/**
 *******************************************************************************
 *  @file           stdio.h
 *  @brief          stdio 系の C 標準入出力関数を抽象化する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/22
 *
 *  C 標準ファイル I/O 関数をプラットフォーム差異なしで使用できるラッパーを提供します。\n
 *  ファイル パスを受け取る関数は UTF-8 文字列として扱い、Windows では内部で
 *  Unicode (_W 系関数) に変換します。\n
 *  出力パス (@p path_out 等) はプラットフォームによらず @ref PLATFORM_PATH_SEP (`"/"`) に
 *  統一されます。パスセパレータの詳細な方針は @ref path.h を参照してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_CRT_STDIO_H
#define CPLAT_CRT_STDIO_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <cplat/base/compiler.h>
#include <cplat/base/error.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          標準入力から書式化データを読み取ります (`scanf` ラッパー)。
     *  @param[in]      format  scanf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[out]     ...     変換結果の格納先。
     *  @return         成功時は変換した項目数、失敗または EOF 時は EOF を返します。
     *
     *  `%s`、`%S`、`%[` で文字列を格納するときは、必ず宛先バッファー容量より小さい幅を指定してください。
     *  `%c`、`%C` は終端文字を追加しないため、指定幅以上の要素数を持つ宛先を渡してください。
     *  非信頼な標準入力は `fgets` で 1 行を読み取ってから @ref cplat_sscanf で解析することを推奨します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  標準入力に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_scanf(const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(scanf, 1, 2)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          標準入力から書式化データを読み取ります (`cplat_scanf` の `va_list` 版)。
     *  @param[in]      format  scanf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[in]      args    書式引数リスト。
     *  @return         成功時は変換した項目数、失敗または EOF 時は EOF を返します。
     *
     *  文字列とスキャン セットの変換には @ref cplat_scanf と同じ幅指定規約が適用されます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  標準入力に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vscanf(const char *format, va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(scanf, 1, 0)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          ストリームから書式化データを読み取ります (`fscanf` ラッパー)。
     *  @param[in]      stream  読み取り元ストリーム。NULL を渡してはなりません。
     *  @param[in]      format  scanf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[out]     ...     変換結果の格納先。
     *  @return         成功時は変換した項目数、失敗または EOF 時は EOF を返します。
     *
     *  `%s`、`%S`、`%[` で文字列を格納するときは、必ず宛先バッファー容量より小さい幅を指定してください。
     *  `%c`、`%C` は終端文字を追加しないため、指定幅以上の要素数を持つ宛先を渡してください。
     *  非信頼なストリーム入力は `fgets` で 1 行を読み取ってから @ref cplat_sscanf で解析することを推奨します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_fscanf(FILE *stream, const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(scanf, 2, 3)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          ストリームから書式化データを読み取ります (`cplat_fscanf` の `va_list` 版)。
     *  @param[in]      stream  読み取り元ストリーム。NULL を渡してはなりません。
     *  @param[in]      format  scanf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[in]      args    書式引数リスト。
     *  @return         成功時は変換した項目数、失敗または EOF 時は EOF を返します。
     *
     *  文字列とスキャン セットの変換には @ref cplat_fscanf と同じ幅指定規約が適用されます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vfscanf(FILE *stream, const char *format, va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(scanf, 2, 0)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          バッファーへ書式化して書き込みます (`snprintf` の切り詰め検出付き版)。
     *
     *  `snprintf` と異なり、戻り値は書き込んだ文字数ではなく共通結果コードです。\n
     *  出力が @p dest に収まらない場合は切り詰めた結果を残さず、@p dest を空文字列にして
     *  @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *
     *  @param[out]     dest       書き込み先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest のサイズ (バイト)。0 を渡してはなりません。
     *  @param[in]      format     printf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[in]      ...        書式引数。
     *  @return         成功時は @ref CPLAT_OK 、引数不正時は @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  出力が @p dest に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  書式化そのものに失敗した場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  書き込んだ文字数が必要な場合は、成功後に @p dest へ `strlen` を適用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_snprintf(char *dest, size_t dest_size, const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 3, 4)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          バッファーへ書式化して書き込みます (@ref cplat_snprintf の `va_list` 版)。
     *  @param[out]     dest       書き込み先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest のサイズ (バイト)。0 を渡してはなりません。
     *  @param[in]      format     printf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[in]      args       書式引数リスト。
     *  @return         成功時は @ref CPLAT_OK 、引数不正時は @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  出力が @p dest に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  書式化そのものに失敗した場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  切り詰め時の振る舞いは @ref cplat_snprintf と同じです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vsnprintf(char *dest, size_t dest_size, const char *format, va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 3, 0)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          ストリームから 1 行を読み取ります (`fgets` の切り詰め検出付き版)。
     *
     *  `fgets` と異なり、行の切り詰めと EOF を戻り値で区別します。\n
     *  取得した行の末尾にある改行 (LF、CR、CRLF) は除去して格納します。
     *
     *  @param[out]     dest       行の格納先バッファー。NULL を渡してはなりません。
     *  @param[in]      dest_size  @p dest のサイズ (バイト)。0 を渡してはなりません。
     *  @param[in]      stream     読み取り元ストリーム。NULL を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         1 行を取得した場合は @ref CPLAT_OK を返します。
     *  @return         読み取る行がない場合は @ref CPLAT_ERR_EOF を返します。
     *  @return         引数不正時は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         行が @p dest に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。\n
     *                  このとき読み取り位置は行の途中に留まるため、行の残りは次の呼び出しで取得されます。
     *  @return         ストリーム エラーの場合は @ref CPLAT_ERR_UNKNOWN を返し、@p detail_out へ詳細を格納します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_fgets(char *dest, size_t dest_size, FILE *stream, cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスでファイルを開きます (`fopen` ラッパー)。
     *  @param[in]      path       開くファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      modes      fopen 互換のモード文字列 ("r"、"w"、"rb" など)。NULL を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は FILE*、失敗時は NULL を返します。
     *
     *  @par            共有モード
     *  Linux では `fopen` は強制ロックを持たず常に共有可です。\n
     *  Windows では内部で `_wfsopen` を `_SH_DENYNO` 指定で呼び出し、他プロセス/スレッドからの
     *  読み書きを許可します ([cplat_open](@ref cplat_open) と同じ既定です)。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT FILE *CPLAT_API cplat_fopen(const char *path, const char *modes, cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスでストリームを再オープンします (`freopen` ラッパー)。
     *  @param[in]      path       再オープンするファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      modes      freopen 互換のモード文字列 ("r"、"w"、"rb" など)。NULL を渡してはなりません。
     *  @param[in,out]  stream     再オープン対象のストリーム。NULL を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は FILE*、失敗時は NULL を返します。
     *
     *  @par            共有モード
     *  Linux では `freopen` は強制ロックを持たず常に共有可です。\n
     *  Windows では内部で `_wfsopen` を `_SH_DENYNO` 指定で呼び出し、`_dup2` で @p stream の
     *  ファイル記述子に複製することで、共有可能な新ファイルへの再オープンを実現します。
     *  FILE* 内部状態のテキスト/バイナリ モード フラグは元の @p stream のまま引き継がれるため、
     *  再オープン時に異なるモードを指定する用途には推奨しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT FILE *CPLAT_API cplat_freopen(const char *path, const char *modes, FILE *stream,
                                               cplat_error *detail_out);

    /**
     *  @brief          ストリームを閉じます (`fclose` ラッパー)。
     *  @param[in]      stream     閉じるストリーム。NULL を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は 0、失敗時は EOF を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream を複数スレッドで同時に閉じる操作は二重クローズとなるため、同一 @p stream に対する操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_fclose(FILE *stream, cplat_error *detail_out);

    /**
     *  @brief          ストリームのバッファーを反映します (`fflush` ラッパー)。
     *  @param[in]      stream     対象のストリーム。NULL の場合は開いている全出力ストリームを対象にします。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は 0、失敗時は EOF を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_fflush(FILE *stream, cplat_error *detail_out);

    /**
     *  @brief          ストリームから要素を読み取ります (`fread` ラッパー)。
     *  @param[out]     buffer     読み取り先。
     *  @param[in]      size       1 要素のバイト数。
     *  @param[in]      count      読み取る要素数。
     *  @param[in]      stream     読み取り元ストリーム。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時およびファイル終端時は空の値を格納します。
     *  @return         読み取った要素数を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_fread(void *buffer, size_t size, size_t count, FILE *stream,
                                              cplat_error *detail_out);

    /**
     *  @brief          ストリームへ要素を書き込みます (`fwrite` ラッパー)。
     *  @param[in]      buffer     書き込むデータ。
     *  @param[in]      size       1 要素のバイト数。
     *  @param[in]      count      書き込む要素数。
     *  @param[in]      stream     書き込み先ストリーム。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         書き込んだ要素数を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_fwrite(const void *buffer, size_t size, size_t count, FILE *stream,
                                               cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスのファイルを削除します (`remove` / `_wremove` ラッパー)。
     *  @param[in]      path  削除するファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は 0、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_remove(const char *path, cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスのファイルを改名します (`rename` / `_wrename` ラッパー)。
     *  @param[in]      oldpath  変更前のパス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      newpath  変更後のパス (UTF-8)。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は 0、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_rename(const char *oldpath, const char *newpath, cplat_error *detail_out);

    /**
     *  @brief          ストリームへ書式化出力します (`fprintf` ラッパー)。
     *  @param[in]      stream  出力先のストリーム。NULL を渡してはなりません。
     *  @param[in]      format  printf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[in]      ...     書式引数。
     *  @return         書き込んだ文字数を返します。失敗時は負値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_fprintf(FILE *stream, const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 2, 3)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          ストリームへ書式化出力します (`cplat_fprintf` の `va_list` 版)。
     *  @param[in]      stream  出力先のストリーム。NULL を渡してはなりません。
     *  @param[in]      format  printf 形式の書式文字列。NULL を渡してはなりません。
     *  @param[in]      args    書式引数リスト。
     *  @return         書き込んだ文字数を返します。失敗時は負値を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vfprintf(FILE *stream, const char *format, va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 2, 0)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          ストリーム位置を移動します (64bit 対応 `fseek` ラッパー)。
     *  @param[in]      stream  対象のストリーム。NULL を渡してはなりません。
     *  @param[in]      offset  移動量 (バイト)。
     *  @param[in]      whence  基点 (SEEK_SET、SEEK_CUR、SEEK_END)。
     *  @return         成功時は 0、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_fseek(FILE *stream, int64_t offset, int whence);

    /**
     *  @brief          ストリームの現在位置を取得します (64bit 対応 `ftell` ラッパー)。
     *  @param[in]      stream  対象のストリーム。NULL を渡してはなりません。
     *  @return         成功時は現在位置 (バイト)、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるストリームに対する呼び出しは同時に実行できます。\n
     *  同一 @p stream に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int64_t CPLAT_API cplat_ftell(FILE *stream);

    /**
     *  @brief          書式指定パスでファイルを開きます。
     *  @param[in]      modes      fopen 互換のモード文字列。NULL を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format     パスを構築する printf 形式の書式文字列。
     *  @param[in]      ...        書式引数。
     *  @return         成功時は FILE*、失敗時は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT FILE *CPLAT_API cplat_fopen_fmt(const char *modes, cplat_error *detail_out, const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 3, 4)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          書式指定パスでファイルを開きます (`cplat_fopen_fmt` の `va_list` 版)。
     *  @param[in]      modes      fopen 互換のモード文字列。NULL を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format     パスを構築する printf 形式の書式文字列。
     *  @param[in]      args       書式引数リスト。
     *  @return         成功時は FILE*、失敗時は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT FILE *CPLAT_API cplat_vfopen_fmt(const char *modes, cplat_error *detail_out, const char *format,
                                                  va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 3, 0)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          書式指定パスのファイルを削除します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format  パスを構築する printf 形式の書式文字列。
     *  @param[in]      ...     書式引数。
     *  @return         成功時は 0、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_remove_fmt(cplat_error *detail_out, const char *format, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 2, 3)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          書式指定パスのファイルを削除します (`cplat_remove_fmt` の `va_list` 版)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      format  パスを構築する printf 形式の書式文字列。
     *  @param[in]      args    書式引数リスト。
     *  @return         成功時は 0、失敗時は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vremove_fmt(cplat_error *detail_out, const char *format, va_list args)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 2, 0)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          一意な一時ファイルを atomic に作成し、指定されたモードで開きます。
     *  @param[in]      prefix       ファイル名先頭につける識別子 (UTF-8)。NULL 可。
     *                               4 文字以上を渡した場合は先頭 3 文字を採用します。
     *  @param[in]      modes        fopen 互換のモード文字列 ("wb", "w", "w+b" など)。
     *                               NULL を渡した場合は NULL を返し、@p detail_out に errno ドメインの
     *                               EINVAL を格納します。
     *                               一時ファイルは常に新規作成のため "r"/"rb" は意味を持ちませんが、
     *                               API 層での制限は課しません。
     *  @param[out]     path_out     生成された一時ファイル絶対パス (UTF-8) の格納先。
     *  @param[in]      path_size    @p path_out のサイズ (バイト)。PLATFORM_PATH_MAX 以上を推奨します。
     *  @param[out]     detail_out   エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時はオープンされた FILE*、失敗時は NULL を返します。
     *
     *  Linux 環境では TMPDIR (未設定なら "/tmp") に "{prefix}XXXXXX" のテンプレートで
     *  mkostemp() によりファイルを atomic に作成し、その fd を fdopen(@p modes) で FILE* に
     *  変換します。\n
     *  @p modes に "w"/"w+" を指定しても fdopen() の仕様上ファイルの切り詰めは発生しません。
     *  mkostemp() が新規作成したファイルは常に空のため、実用上の影響はありません。\n
     *  Windows 環境では GetTempPathW + GetTempFileNameW でユニーク名を生成し、
     *  _wfsopen() で `_SH_DENYNO` を指定して開きます。@p path_out は wchar→UTF-8 変換した結果が
     *  格納されます。\n
     *  呼び出し元は不要になったら fclose() でクローズし、
     *  必要なら cplat_remove() でファイルを削除する責任があります。
     *
     *  @par            共有モード
     *  Linux では `fdopen` 経由のため強制ロックを持たず常に共有可です。\n
     *  Windows でも `_wfsopen` を `_SH_DENYNO` 指定で呼び出すため、他プロセス/スレッドからの
     *  読み書きを許可します ([cplat_fopen](@ref cplat_fopen) と同じ既定です)。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  Linux 環境では環境変数 `TMPDIR` を参照します。\n
     *  他スレッドが環境変数を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に環境変数を変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT FILE *CPLAT_API cplat_fopen_temp(const char *prefix, const char *modes, char *path_out,
                                                  size_t path_size, cplat_error *detail_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRT_STDIO_H */
