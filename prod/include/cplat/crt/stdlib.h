/**
 *******************************************************************************
 *  @file           stdlib.h
 *  @brief          stdlib 系の CRT 関数を抽象化する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/05/01
 *
 *  C 標準ユーティリティ関数をプラットフォーム差異なしで使用できるラッパーを提供します。\n
 *  Windows では MSVC が非推奨とする関数の代替安全版 (_dupenv_s 等) を使用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_CRT_STDLIB_H
#define CPLAT_CRT_STDLIB_H

#include <stddef.h>
#include <stdint.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
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
     *  @brief          環境変数の値を取得します。
     *
     *  指定された環境変数が設定されている場合、その値を @p buf に格納します。\n
     *  @p buf に NULL を渡した場合は存在確認のみ行い、値のコピーを省略します。\n
     *  Windows では `_dupenv_s` を使用して MSVC セキュリティ警告を回避します。
     *
     *  @param[in]      name        環境変数名 (null 終端文字列)。NULL を渡してはなりません。
     *  @param[out]     buf         値の格納先です。NULL を指定すると存在確認のみ行います。\n
     *                              変数が設定されていない場合は空文字列を格納します。
     *  @param[in]      buf_size    @p buf のバイト数。@p buf が NULL の場合は無視します。
     *  @param[out]     exists_out  変数が設定されている場合は 1、設定されていない場合は 0 を格納します。\n
     *                              NULL も指定できます。戻り値が @ref CPLAT_OK または
     *                              @ref CPLAT_ERR_BUFFER_TOO_SMALL の場合に有効です。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は @ref CPLAT_OK を返します。変数の設定有無は @p exists_out で確認します。
     *  @return         @p name が NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         値の格納先が不足している場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  他スレッドが環境変数を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に環境変数を変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_getenv(const char *name, char *buf, size_t buf_size, int *exists_out,
                                            cplat_error *detail_out);

    /**
     *  @brief          環境変数の値を設定します。
     *
     *  Linux では `setenv`、Windows では `_putenv_s` を使用します。\n
     *  設定は呼び出し元プロセスにのみ反映され、親プロセスへは伝わりません。
     *
     *  @param[in]      name       環境変数名 (null 終端文字列)。NULL、空文字列、
     *                             `'='` を含む文字列を渡してはなりません。
     *  @param[in]      value      設定する値 (null 終端文字列)。NULL を渡してはなりません。
     *  @param[in]      overwrite  変数がすでに設定されている場合に上書きするかどうか。\n
     *                             0 のとき既存の値を保持します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は @ref CPLAT_OK 、失敗時は共通結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  環境変数の変更は、他スレッドによる読み取りと競合します。\n
     *  マルチスレッド化の前に設定を完了させるか、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_setenv(const char *name, const char *value, int overwrite,
                                            cplat_error *detail_out);

    /**
     *  @brief          環境変数を削除します。
     *
     *  Linux では `unsetenv`、Windows では値に空文字列を指定した `_putenv_s` を使用します。\n
     *  Windows は空文字列の設定を削除として扱うため、値が空の環境変数を作ることはできません。
     *
     *  @param[in]      name  環境変数名 (null 終端文字列)。NULL、空文字列、
     *                        `'='` を含む文字列を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は @ref CPLAT_OK 、失敗時は共通結果コードを返します。\n
     *                  変数が設定されていない場合も成功として扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  環境変数の変更は、他スレッドによる読み取りと競合します。\n
     *  マルチスレッド化の前に設定を完了させるか、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_unsetenv(const char *name, cplat_error *detail_out);

    /**
     *  @brief          文字列を 64 bit 符号付き整数へ変換します (`strtoll` の安全版)。
     *
     *  `strtoll` と異なり、変換位置 (`endptr`) を返しません。\n
     *  文字列全体が整数として解釈されたことを関数側で検査し、末尾に余分な文字が残る場合は失敗とします。\n
     *  先頭の空白と符号は許容します。空文字列と、空白のみの文字列は失敗とします。
     *
     *  @param[out]     value_out  変換結果の格納先。NULL を渡してはなりません。\n
     *                             失敗時の内容は不定です。
     *  @param[in]      text       変換する文字列 (null 終端)。NULL を渡してはなりません。
     *  @param[in]      base       基数。2 から 36、または 0 (接頭辞による自動判別) を指定します。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p value_out もしくは @p text が NULL の場合、または @p base が範囲外の場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         整数として解釈できない場合、または末尾に余分な文字が残る場合は
     *                  @ref CPLAT_ERR_INVALID_INTEGER を返します。
     *  @return         `int64_t` で表現できない値の場合は @ref CPLAT_ERR_OUT_OF_RANGE を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_parse_int64(int64_t *value_out, const char *text, int base);

    /**
     *  @brief          文字列を 64 bit 符号なし整数へ変換します (`strtoull` の安全版)。
     *
     *  検査の方針は @ref cplat_parse_int64 と同じです。\n
     *  加えて、符号 `'-'` で始まる文字列を @ref CPLAT_ERR_OUT_OF_RANGE として拒否します。
     *
     *  @param[out]     value_out  変換結果の格納先。NULL を渡してはなりません。\n
     *                             失敗時の内容は不定です。
     *  @param[in]      text       変換する文字列 (null 終端)。NULL を渡してはなりません。
     *  @param[in]      base       基数。2 から 36、または 0 (接頭辞による自動判別) を指定します。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p value_out もしくは @p text が NULL の場合、または @p base が範囲外の場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         整数として解釈できない場合、または末尾に余分な文字が残る場合は
     *                  @ref CPLAT_ERR_INVALID_INTEGER を返します。
     *  @return         負値が指定された場合、または `uint64_t` で表現できない値の場合は
     *                  @ref CPLAT_ERR_OUT_OF_RANGE を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_parse_uint64(uint64_t *value_out, const char *text, int base);

    /**
     *  @brief          文字列を `int` へ変換します (`atoi` の安全版)。
     *
     *  @ref cplat_parse_int64 で解析したうえで、`int` の範囲に収まることを検査します。
     *
     *  @param[out]     value_out  変換結果の格納先。NULL を渡してはなりません。\n
     *                             失敗時の内容は不定です。
     *  @param[in]      text       変換する文字列 (null 終端)。NULL を渡してはなりません。
     *  @param[in]      base       基数。2 から 36、または 0 (接頭辞による自動判別) を指定します。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p value_out もしくは @p text が NULL の場合、または @p base が範囲外の場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         整数として解釈できない場合、または末尾に余分な文字が残る場合は
     *                  @ref CPLAT_ERR_INVALID_INTEGER を返します。
     *  @return         `int` で表現できない値の場合は @ref CPLAT_ERR_OUT_OF_RANGE を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_parse_int(int *value_out, const char *text, int base);

    /**
     *  @brief          文字列を `double` へ変換します (`atof` の安全版)。
     *
     *  検査の方針は @ref cplat_parse_int64 と同じで、文字列全体が数値として解釈されたことを検査します。\n
     *  `strtod` が受け付ける表記 (指数表記、16 進浮動小数、`inf`、`nan`) をそのまま受け付けます。
     *
     *  @param[out]     value_out  変換結果の格納先。NULL を渡してはなりません。\n
     *                             失敗時の内容は不定です。
     *  @param[in]      text       変換する文字列 (null 終端)。NULL を渡してはなりません。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p value_out もしくは @p text が NULL の場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         数値として解釈できない場合、または末尾に余分な文字が残る場合は
     *                  @ref CPLAT_ERR_INVALID_INTEGER を返します。
     *  @return         `double` で表現できない値の場合は @ref CPLAT_ERR_OUT_OF_RANGE を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_parse_double(double *value_out, const char *text);

    /**
     *  @brief          メモリを確保します (`malloc` の安全版)。
     *
     *  確保した領域はゼロ初期化しません。単一オブジェクトとバイト バッファーに使用します。\n
     *  要素数を伴う配列には @ref cplat_calloc を使用してください。
     *
     *  確保した領域は @ref cplat_free で解放してください。
     *
     *  @param[in]      size  確保するバイト数。0 を指定した場合は確保しません。
     *  @return         確保した領域へのポインター。@p size が 0 の場合と確保に失敗した場合は NULL を返します。
     *
     *  @attention      本関数は @ref CPLAT_OK 系の戻り値規約の適用対象外です。
     *                  確保した領域へのポインターを返し、失敗を NULL で表します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_malloc(size_t size);

    /**
     *  @brief          ゼロ初期化したメモリを確保します。
     *
     *  検査の方針は @ref cplat_malloc と同じで、確保した領域全体を 0 で埋めます。\n
     *  要素数を伴う配列には @ref cplat_calloc を使用してください。
     *
     *  確保した領域は @ref cplat_free で解放してください。
     *
     *  @param[in]      size  確保するバイト数。0 を指定した場合は確保しません。
     *  @return         確保した領域へのポインター。@p size が 0 の場合と確保に失敗した場合は NULL を返します。
     *
     *  @attention      本関数は @ref CPLAT_OK 系の戻り値規約の適用対象外です。
     *                  確保した領域へのポインターを返し、失敗を NULL で表します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_malloc_zerofill(size_t size);

    /**
     *  @brief          要素数を伴うメモリを確保します (`calloc` の安全版)。
     *
     *  @p count と @p size の乗算が `size_t` を回り込む場合、確保を行わずに失敗とします。\n
     *  確保した領域全体を 0 で埋めます。
     *
     *  確保した領域は @ref cplat_free で解放してください。
     *
     *  @param[in]      count  要素数。0 を指定した場合は確保しません。
     *  @param[in]      size   要素 1 個あたりのバイト数。0 を指定した場合は確保しません。
     *  @return         確保した領域へのポインター。@p count もしくは @p size が 0 の場合、
     *                  乗算が回り込む場合、確保に失敗した場合は NULL を返します。
     *
     *  @attention      本関数は @ref CPLAT_OK 系の戻り値規約の適用対象外です。
     *                  確保した領域へのポインターを返し、失敗を NULL で表します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_calloc(size_t count, size_t size);

    /**
     *  @brief          確保済みのメモリを再確保します (`realloc` の安全版)。
     *
     *  標準の `realloc` と異なり、要素数と要素サイズを分けて受け取り、乗算の回り込みを検査します。\n
     *  拡張した範囲はゼロ初期化しません。ゼロ初期化が必要な場合は @ref cplat_realloc_zerofill を使用してください。
     *
     *  失敗した場合、@p ptr が指す領域は解放されず、内容も保持されます。\n
     *  戻り値は @p ptr とは別の変数で受け、NULL でないことを確認してから @p ptr へ代入してください。
     *
     *  確保した領域は @ref cplat_free で解放してください。
     *
     *  @param[in]      ptr    再確保する領域。NULL を指定した場合は新規確保として動作します。
     *  @param[in]      count  再確保後の要素数。0 を指定した場合は確保しません。
     *  @param[in]      size   要素 1 個あたりのバイト数。0 を指定した場合は確保しません。
     *  @return         再確保した領域へのポインター。@p count もしくは @p size が 0 の場合、
     *                  乗算が回り込む場合、確保に失敗した場合は NULL を返します。
     *
     *  @attention      本関数は @ref CPLAT_OK 系の戻り値規約の適用対象外です。
     *                  確保した領域へのポインターを返し、失敗を NULL で表します。
     *  @attention      @p count に 0 を指定しても @p ptr は解放しません。
     *                  標準の `realloc` とは異なる扱いです。@ref cplat_free で明示的に解放してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるメモリ領域に対する操作は同時に実行できます。\n
     *  同一 @p ptr に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_realloc(void *ptr, size_t count, size_t size);

    /**
     *  @brief          確保済みのメモリを再確保し、拡張した範囲をゼロ初期化します。
     *
     *  検査の方針は @ref cplat_realloc と同じです。\n
     *  加えて、@p old_count から @p count までの範囲の要素を 0 で埋めます。\n
     *  @p old_count が @p count 以上の場合はゼロ初期化を行いません。
     *
     *  確保した領域は @ref cplat_free で解放してください。
     *
     *  @param[in]      ptr        再確保する領域。NULL を指定した場合は新規確保として動作します。
     *  @param[in]      old_count  再確保前の要素数。@p ptr が NULL の場合は 0 を指定してください。
     *  @param[in]      count      再確保後の要素数。0 を指定した場合は確保しません。
     *  @param[in]      size       要素 1 個あたりのバイト数。0 を指定した場合は確保しません。
     *  @return         再確保した領域へのポインター。@p count もしくは @p size が 0 の場合、
     *                  乗算が回り込む場合、確保に失敗した場合は NULL を返します。
     *
     *  @attention      本関数は @ref CPLAT_OK 系の戻り値規約の適用対象外です。
     *                  確保した領域へのポインターを返し、失敗を NULL で表します。
     *  @attention      @p count に 0 を指定しても @p ptr は解放しません。
     *                  標準の `realloc` とは異なる扱いです。@ref cplat_free で明示的に解放してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるメモリ領域に対する操作は同時に実行できます。\n
     *  同一 @p ptr に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_realloc_zerofill(void *ptr, size_t old_count, size_t count, size_t size);

    /**
     *  @brief          確保したメモリを解放します (`free` の代替)。
     *
     *  @ref cplat_malloc 、@ref cplat_malloc_zerofill 、@ref cplat_calloc 、
     *  @ref cplat_realloc 、@ref cplat_realloc_zerofill 、@ref cplat_strdup が返した領域を解放します。
     *
     *  確保と解放を cplat 内で完結させ、共有ライブラリの境界をまたぐ解放を避けるために使用します。
     *
     *  @param[in]      ptr  解放する領域。NULL を指定した場合は何も行いません。
     *
     *  @attention      本関数は @ref CPLAT_OK 系の戻り値規約の適用対象外です。
     *                  戻り値を持ちません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるメモリ領域に対する操作は同時に実行できます。\n
     *  同一 @p ptr に対する操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_free(void *ptr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRT_STDLIB_H */
