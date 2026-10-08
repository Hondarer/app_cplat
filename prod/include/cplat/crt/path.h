/**
 *******************************************************************************
 *  @file           path.h
 *  @brief          CRT 抽象層で使用するパス関連の型と定数を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/22
 *
 *  プラットフォームに依存せず使用できるパス関連の定数と API を提供します。
 *
 *  **パスセパレータ方針**\n
 *  本ライブラリはパスセパレータを全プラットフォームで @ref PLATFORM_PATH_SEP (`"/"`) に統一します。
 *
 *  - **入力パス** (呼び出し元が渡す): `"/"` 区切りを推奨します。
 *    Windows では `"\\"` 区切りも受け付け、必要に応じて内部で `"/"` へ正規化します。
 *    Linux では `"\\"` を通常文字として扱います。
 *  - **出力パス** (`path_out` など): Windows API が返す `"\\"` は内部で `"/"` に正規化して
 *    呼び出し元に返します。常に `'/'` 区切りのパスを受け取れます。
 *  - **パス構築**: ライブラリ内部でセパレータが必要な場合は @ref PLATFORM_PATH_SEP を使用します。
 *  - **外部由来パス**: 環境変数や設定ファイルから取得したパスは
 *    cplat_path_normalize_sep() で正規化できます。
 *
 *  **basename / dirname / extension 系の例外**\n
 *  cplat_path_basename() / cplat_path_dirname() / cplat_path_extension() /
 *  cplat_path_strip_extension() は、コンパイラが生成する `__FILE__` など
 *  Windows 由来のパス文字列を全プラットフォームで解析できるよう、上記の方針に対する
 *  例外として `'\\'` も `'/'` と同様にセパレータとして扱います。\n
 *  Linux でファイル名自体に `'\\'` を含むケースでは、この 4 関数のみ通常の
 *  パス関数と異なる切り出し結果になる点に注意してください。\n
 *  cplat_path_join_n() (パス構築系) はこの例外に含まれず、`'\\'` を正規化しません。
 *
 *  本ヘッダーの `detail_out` はプラットフォーム別の詳細エラーを格納します。\n
 *  Linux では errno ドメイン、Windows では実装が使用した API に応じて errno ドメインまたは
 *  Win32 ドメインの値が入り、ドメインは @ref cplat_error の `domain` で識別できます。\n
 *  値の取り出しは cplat_error_get_errno() / cplat_error_get_windows_error()、
 *  文字列化は cplat_error_message()、要因判定は cplat_error_is() を使用してください。\n
 *  同じ詳細は cplat_error_get_last() でも取得できます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_CRT_PATH_H
#define CPLAT_CRT_PATH_H

#include <cplat/base/error.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <stdarg.h>
#include <stddef.h>

#ifdef DOXYGEN
    /**
     *  @brief          OS 固有のパス最大長です。
     *                  Linux では PATH_MAX、Windows では MAX_PATH に対応する値を使用します。
     */
    #define PLATFORM_PATH_MAX 4096
#else /* !DOXYGEN */
    #if defined(PLATFORM_LINUX)
        #include <limits.h>
        #define PLATFORM_PATH_MAX PATH_MAX
    #elif defined(PLATFORM_WINDOWS)
        #include <cplat/base/windows_sdk.h>
        #define PLATFORM_PATH_MAX MAX_PATH
    #endif /* PLATFORM_ */
#endif     /* DOXYGEN */

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

/**
 *  @brief          ファイル パス区切り文字列。全プラットフォームで `"/"` に統一します。
 */
#define PLATFORM_PATH_SEP "/"
/**
 *  @brief          ファイル パス区切り文字 (char 型)。全プラットフォームで `'/'` に統一します。
 */
#define PLATFORM_PATH_SEP_CHR '/'

/**
 *  @brief          標準入出力を破棄する null device のパスです。
 *                  Linux では `"/dev/null"`、Windows では `"NUL"` です。
 */
#ifdef DOXYGEN
    #define PLATFORM_NULL_DEVICE_PATH "/dev/null"
#elif defined(PLATFORM_LINUX)
    #define PLATFORM_NULL_DEVICE_PATH "/dev/null"
#elif defined(PLATFORM_WINDOWS)
    #define PLATFORM_NULL_DEVICE_PATH "NUL"
#endif /* DOXYGEN */

#define CPLAT_PATH_CONCAT_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, count, \
                                     ...) \
    count
#define CPLAT_PATH_CONCAT_COUNT(...) \
    CPLAT_PATH_CONCAT_COUNT_IMPL(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

/**
 *  @brief          パス断片を指定順にそのまま連結します。
 *
 *  @param[out]     path_out   連結結果の格納先。NULL を渡してはなりません。
 *  @param[in]      path_size  @p path_out のサイズ (バイト)。0 を渡してはなりません。
 *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
 *                  エラー詳細を設定せず、返却しません。
 *                  NULL 以外を指定した場合、成功時は空の値を格納します。
 *  @param[in]      ...        連結する UTF-8 文字列断片。少なくとも 1 つ必要です。
 *
 *  断片は自動補正せず、そのまま連結されます。\n
 *  パス区切り文字が必要な場合は @ref PLATFORM_PATH_SEP を明示的に指定してください。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフです。
 */
#define cplat_path_concat(path_out, path_size, detail_out, ...) \
    cplat_path_concat_n((path_out), (path_size), (detail_out), CPLAT_PATH_CONCAT_COUNT(__VA_ARGS__), __VA_ARGS__)

/**
 *  @brief          パス断片をパス区切り文字で自動補完しながら連結します。
 *
 *  @param[out]     path_out   連結結果の格納先。NULL を渡してはなりません。
 *  @param[in]      path_size  @p path_out のサイズ (バイト)。0 を渡してはなりません。
 *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
 *                  エラー詳細を設定せず、返却しません。
 *                  NULL 以外を指定した場合、成功時は空の値を格納します。
 *  @param[in]      ...        連結する UTF-8 文字列断片。少なくとも 1 つ必要です。
 *
 *  cplat_path_concat() と異なり、断片間に @ref PLATFORM_PATH_SEP を自動的に補完します。\n
 *  詳細は cplat_path_join_n() を参照してください。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフです。
 */
#define cplat_path_join(path_out, path_size, detail_out, ...) \
    cplat_path_join_n((path_out), (path_size), (detail_out), CPLAT_PATH_CONCAT_COUNT(__VA_ARGS__), __VA_ARGS__)

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          パス文字列内の '\\' を '/' に正規化します (インプレース)。
     *  @param[in,out]  path  正規化対象のパス文字列 (UTF-8)。NULL を渡してはなりません。
     *  @return         path を返します (連鎖呼び出し用)。
     *
     *  環境変数や設定ファイルから読み取ったパスに Windows スタイルの '\\' が含まれる場合に
     *  使用します。\n
     *  本ライブラリが出力するパス (@p path_out 等) はすでに @ref PLATFORM_PATH_SEP (`"/"`) に
     *  正規化済みのため、本関数を呼び出す必要はありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT char *CPLAT_API cplat_path_normalize_sep(char *path);

    /**
     *  @brief          パスを絶対化し、区切り文字を '/' に正規化して返します。
     *  @param[out]     path_out    絶対化済みパス (UTF-8) の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size   @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      path        入力パス (UTF-8)。NULL および空文字は渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  相対パスはカレント ディレクトリ基準で絶対化します。\n
     *  Linux では realpath() による symlink 解決を可能な範囲で試み、失敗した場合は
     *  '.' / '..' を解消した絶対パス文字列を返します。\n
     *  正規化用メモリを確保できない場合、@p detail_out に errno ドメインの `ENOMEM` を格納します。\n
     *  Windows では GetFullPathNameW() により絶対化し、返却値は常に
     *  @ref PLATFORM_PATH_SEP (`"/"`) 区切りへ正規化されます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_path_get_full(char *path_out, size_t path_size, cplat_error *detail_out,
                                                   const char *path);

    /**
     *  @brief          2 つのパスが同じ実体を指すか比較します。
     *  @param[in]      lhs        比較対象の 1 つ目のパス (UTF-8)。NULL および空文字は渡してはなりません。
     *  @param[in]      rhs        比較対象の 2 つ目のパス (UTF-8)。NULL および空文字は渡してはなりません。
     *  @param[out]     equal_out  一致時は 1、不一致時は 0 の格納先。NULL を渡してはなりません。
     *                             戻り値が @ref CPLAT_OK の場合のみ有効です。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  内部でそれぞれのパスに対して cplat_path_get_full() を呼び、絶対化と
     *  区切り文字正規化を行ったうえで比較します。\n
     *  Windows ではファイル システムの慣習に合わせて大小文字を区別せず比較します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_path_equal(const char *lhs, const char *rhs, int *equal_out,
                                                cplat_error *detail_out);

    /**
     *  @brief          プラットフォームの一時ディレクトリのパスを取得します。
     *  @param[out]     path_out    一時ディレクトリの絶対パス (UTF-8) の格納先。
     *                              末尾パス区切り文字 (@ref PLATFORM_PATH_SEP_CHR) は付きません。
     *                              NULL を渡してはなりません。
     *  @param[in]      path_size   @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  Linux 環境では環境変数 `TMPDIR` を参照し、未設定または空の場合は `"/tmp"` を使用します。\n
     *  Windows 環境では `GetTempPathW()` で取得したパスを UTF-8 に変換して使用します。\n
     *  出力パスは常に @ref PLATFORM_PATH_SEP (`"/"`) 区切りで正規化されており、末尾の区切り文字は含まれません。\n
     *  ファイル パスを構築する際は @ref PLATFORM_PATH_SEP を挟んでください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_path_get_temp_dir(char *path_out, size_t path_size, cplat_error *detail_out);

    /**
     *  @brief          パス断片を指定順にそのまま連結します。
     *  @param[out]     path_out    連結結果の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size   @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      part_count  連結する断片数。1 以上を渡してください。
     *  @param[in]      ...         連結する UTF-8 文字列断片。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *
     *  断片は自動補正せず、そのまま連結されます。\n
     *  いずれかの断片が NULL、または @p part_count が 0 の場合は EINVAL を返します。\n
     *  結果が @p path_out に収まらない場合は ENAMETOOLONG を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_path_concat_n(char *path_out, size_t path_size, cplat_error *detail_out,
                                                   size_t part_count, ...);

    /**
     *  @brief          パス断片を指定順にそのまま連結します (`cplat_path_concat_n` の `va_list` 版)。
     *  @param[out]     path_out    連結結果の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size   @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      part_count  連結する断片数。1 以上を渡してください。
     *  @param[in]      args        連結する UTF-8 文字列断片の引数リスト。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vpath_concat_n(char *path_out, size_t path_size, cplat_error *detail_out,
                                                    size_t part_count, va_list args);

    /**
     *  @brief          パスのベース名 (最後のセパレータの次の位置) を指すポインターを返します。
     *  @param[in]      path  対象パス (UTF-8)。NULL 可。
     *  @return         @p path 内のベース名先頭を指すポインター。
     *
     *  GNU basename() 相当の非破壊動作です。@p path 内を指すポインターを返すため、
     *  複製やバッファーは必要ありません。\n
     *  セパレータが見つからない場合は @p path 自身を返します。\n
     *  末尾がセパレータの場合は終端 `'\0'` を指す空文字列を返します
     *  (例: `"/opt/bin/"` → `""`)。\n
     *  @p path が NULL の場合は NULL を返します。\n
     *  本関数は `'\\'` もセパレータとして扱います。詳細は本ヘッダー冒頭の
     *  「basename / dirname / extension 系の例外」を参照してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_path_basename(const char *path);

    /**
     *  @brief          パスの親ディレクトリ部分を取得します。
     *  @param[out]     path_out   親ディレクトリ パス (UTF-8) の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size  @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      path       入力パス (UTF-8)。NULL および空文字は渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *
     *  POSIX dirname() 相当の規約で親ディレクトリ部分を求めます。\n
     *  末尾のセパレータ群を除去したうえで、最後のセパレータより前を返します。\n
     *  セパレータが見つからない場合は `"."` を返します。\n
     *  ルート (`"/"`) や `"/name"` のようにセパレータより前が空になる場合は `"/"` を返します。\n
     *  出力は常に @ref PLATFORM_PATH_SEP (`"/"`) 区切りに正規化されます。\n
     *  本関数は `'\\'` もセパレータとして扱います。詳細は本ヘッダー冒頭の
     *  「basename / dirname / extension 系の例外」を参照してください。\n
     *  いずれかの引数が不正な場合は EINVAL、結果が @p path_out に収まらない場合は
     *  ENAMETOOLONG を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_path_dirname(char *path_out, size_t path_size, cplat_error *detail_out,
                                                  const char *path);

    /**
     *  @brief          パスの拡張子 (ドット込み) を指すポインターを返します。
     *  @param[in]      path  対象パス (UTF-8)。NULL 可。
     *  @return         @p path 内の拡張子 (先頭の `'.'` を含む) を指すポインター。
     *
     *  ベース名部分のみを対象に、最後の `'.'` 以降を拡張子として返します
     *  (例: `"a/b.tar.gz"` → `".gz"`)。\n
     *  ベース名の先頭文字が `'.'` の場合 (ドットファイル、例: `".bashrc"`) はそのドットを
     *  拡張子とみなしません。\n
     *  拡張子が見つからない場合は @p path の終端 (`'\0'` を指す空文字列) を返します。\n
     *  @p path が NULL の場合は NULL を返します。\n
     *  本関数は `'\\'` もセパレータとして扱います。詳細は本ヘッダー冒頭の
     *  「basename / dirname / extension 系の例外」を参照してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_path_extension(const char *path);

    /**
     *  @brief          パスから拡張子を除いた文字列を取得します。
     *  @param[out]     path_out   拡張子を除いたパス (UTF-8) の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size  @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      path       入力パス (UTF-8)。NULL および空文字は渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *
     *  cplat_path_extension() が返す拡張子部分を除いて @p path を @p path_out へ
     *  コピーします。\n
     *  拡張子が見つからない場合は @p path をそのままコピーします。\n
     *  本関数は `'\\'` もセパレータとして扱います。詳細は本ヘッダー冒頭の
     *  「basename / dirname / extension 系の例外」を参照してください。\n
     *  いずれかの引数が不正な場合は EINVAL、結果が @p path_out に収まらない場合は
     *  ENAMETOOLONG を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_path_strip_extension(char *path_out, size_t path_size, cplat_error *detail_out,
                                                          const char *path);

    /**
     *  @brief          パス断片をパス区切り文字で自動補完しながら連結します。
     *  @param[out]     path_out    連結結果の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size   @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      part_count  連結する断片数。1 以上を渡してください。
     *  @param[in]      ...         連結する UTF-8 文字列断片。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *
     *  隣り合う非空断片の間に @ref PLATFORM_PATH_SEP_CHR がちょうど 1 つになるよう
     *  自動的に補完・重複除去して連結します
     *  (例: `"a/"` + `"/b"` → `"a/b"`、`"a"` + `"b"` → `"a/b"`)。\n
     *  空文字列の断片は結合対象から除外されます。\n
     *  先頭断片が `'/'` から始まる場合、その絶対パスとしての性質は保持されます。\n
     *  断片の途中にある連続セパレータや `'\\'` は正規化しません。必要な場合は
     *  事前に cplat_path_normalize_sep() を適用してください。\n
     *  いずれかの断片が NULL、または @p part_count が 0 の場合は EINVAL を返します。\n
     *  結果が @p path_out に収まらない場合は ENAMETOOLONG を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_path_join_n(char *path_out, size_t path_size, cplat_error *detail_out,
                                                 size_t part_count, ...);

    /**
     *  @brief          パス断片をパス区切り文字で自動補完しながら連結します (`cplat_path_join_n` の `va_list` 版)。
     *  @param[out]     path_out    連結結果の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size   @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @param[in]      part_count  連結する断片数。1 以上を渡してください。
     *  @param[in]      args        連結する UTF-8 文字列断片の引数リスト。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_vpath_join_n(char *path_out, size_t path_size, cplat_error *detail_out,
                                                  size_t part_count, va_list args);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRT_PATH_H */
