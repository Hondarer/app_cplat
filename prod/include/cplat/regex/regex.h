/**
 *******************************************************************************
 *  @file           regex.h
 *  @brief          UTF-8 文字列に対する正規表現の照合 API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/04
 *  @version        1.0.0
 *
 *  パターンと入力文字列をいずれも UTF-8 として受け取り、マッチ位置を
 *  UTF-8 バイト オフセットで返します。\n
 *  Linux と Windows で同一のエンジンを使用するため、照合結果は
 *  プラットフォームによらず一致します。
 *
 *  @par            文字モデル
 *  照合は **UTF-16 コード単位 1 個を 1 文字**として行います。\n
 *  基本多言語面 (BMP) の文字 (日本語を含む) は 1 文字として扱われますが、
 *  BMP 外の文字 (絵文字、CJK 拡張 B 以降など) は 2 文字として扱われるため、
 *  `.` 1 個には一致せず `.{2}` に一致します。\n
 *  これは ECMAScript の `u` フラグを指定しない正規表現、.NET、Java と同じ
 *  モデルです。\n
 *  返却するオフセットは常に UTF-8 のコード ポイント境界へ丸めるため、
 *  切り出した部分文字列が不正な UTF-8 になることはありません。
 *
 *  @warning        本 API は信頼できない入力をパターンとして受け付ける用途を
 *                  想定していません。\n
 *                  内部エンジンは再帰的なバックトラッキングで実装されており、
 *                  `(a+)+b` のような病的なパターンと長い入力の組み合わせでは、
 *                  照合が指数的な時間を要するか、スタックを消費し尽くして
 *                  プロセスが異常終了する可能性があります。\n
 *                  パターンは自プログラムが用意した定数を使用し、入力長には
 *                  上限を設けてください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_REGEX_REGEX_H
#define CPLAT_REGEX_REGEX_H

#include <stddef.h>

#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_REGEX
 *  @{
 */

/** @brief 不参加の捕捉グループを表すオフセット値。 */
#define CPLAT_REGEX_NPOS ((size_t)-1)

/** @brief 入力またはパターンとして受け付ける最大バイト数。 */
#define CPLAT_REGEX_MAX_LENGTH (1024U * 1024U)

/**
 *  @name           コンパイル フラグ
 *  @brief          @ref cplat_regex_create() の `flags` に指定するビット値です。
 *  @{
 */
#define CPLAT_REGEX_DEFAULT  0x0000U /**< ECMAScript 方言、大小の区別あり。 */
#define CPLAT_REGEX_EXTENDED 0x0001U /**< POSIX 拡張正規表現 (ERE) として解釈する。 */
#define CPLAT_REGEX_BASIC    0x0002U /**< POSIX 基本正規表現 (BRE) として解釈する。 */
#define CPLAT_REGEX_ICASE    0x0004U /**< 大小を区別しない (ASCII 範囲のみ)。 */
#define CPLAT_REGEX_NOSUB    0x0008U /**< 捕捉グループを記録しない。 */
#define CPLAT_REGEX_OPTIMIZE 0x0010U /**< 構築に時間をかけて照合を高速化する。 */
/** @} */

/**
 *  @name           照合フラグ
 *  @brief          照合を行う API の `match_flags` に指定するビット値です。
 *  @{
 */
#define CPLAT_REGEX_MATCH_DEFAULT  0x0000U /**< 既定の照合を行う。 */
#define CPLAT_REGEX_MATCH_NOTBOL   0x0001U /**< 照合開始位置を行頭とみなさない。 */
#define CPLAT_REGEX_MATCH_NOTEOL   0x0002U /**< 入力末尾を行末とみなさない。 */
#define CPLAT_REGEX_MATCH_NOTEMPTY 0x0004U /**< 空文字列への一致を認めない。 */
#define CPLAT_REGEX_MATCH_ANCHORED 0x0008U /**< 照合開始位置に固定して照合する。 */
/** @} */

/**
 *  @name           置換フラグ
 *  @brief          @ref cplat_regex_replace() の `flags` に、照合フラグと合わせて
 *                  指定するビット値です。
 *  @{
 */
#define CPLAT_REGEX_REPLACE_FIRST_ONLY 0x0100U /**< 最初に一致した箇所のみを置換する。 */
#define CPLAT_REGEX_REPLACE_NO_COPY    0x0200U /**< 一致しなかった部分を出力しない。 */
#define CPLAT_REGEX_REPLACE_SED        0x0400U /**< 置換文字列を sed の書式で解釈する。 */
/** @} */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /** @brief コンパイル済みパターンのハンドル (不透明構造体)。 */
    typedef struct cplat_regex cplat_regex;

    /**
     *  @brief          マッチした範囲を表す半開区間です。
     *
     *  `begin` 以上 `end` 未満のバイト範囲がマッチした部分です。\n
     *  不参加の捕捉グループでは、`begin` と `end` の双方が
     *  @ref CPLAT_REGEX_NPOS になります。
     */
    typedef struct cplat_regex_match
    {
        size_t begin; /**< マッチ開始位置 (UTF-8 バイト オフセット)。 */
        size_t end;   /**< マッチ終了位置 (UTF-8 バイト オフセット、この位置は含まない)。 */
    } cplat_regex_match;

    /**
     *  @brief          パターンをコンパイルし、ハンドルを生成します。
     *  @param[in]      pattern     UTF-8 のパターン文字列。NULL を渡してはなりません。
     *  @param[in]      flags       コンパイル フラグ (@ref CPLAT_REGEX_DEFAULT など) のビット和。
     *  @param[out]     regex_out   生成したハンドルの格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、@ref CPLAT_ERR_INVALID_PATTERN 、
     *                  @ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_LIMIT_EXCEEDED 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @ref CPLAT_REGEX_EXTENDED と @ref CPLAT_REGEX_BASIC の同時指定、および
     *  未定義のビットの指定は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。\n
     *  `pattern` が @ref CPLAT_REGEX_MAX_LENGTH バイトを超える場合は
     *  @ref CPLAT_ERR_LIMIT_EXCEEDED を返します。\n
     *  生成したハンドルは @ref cplat_regex_dispose() で破棄してください。
     *
     *  @note           @ref CPLAT_REGEX_ICASE の大小の畳み込みは ASCII 範囲に限ります。
     *                  `Ä` と `ä` は一致しません。\n
     *                  `\\w` `\\d` `[[:alpha:]]` などの文字クラスも ASCII 定義です。\n
     *                  Unicode 正規化は行わないため、`"が"` と `"か" + 濁点` は一致しません。\n
     *                  照合要素 `[[.x.]]` と等価クラス `[[=x=]]` はサポートしません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_regex_create(const char *pattern, unsigned int flags, cplat_regex **regex_out,
                                                  cplat_error *detail_out);

    /**
     *  @brief          ハンドルを破棄します。
     *  @param[in]      regex  破棄するハンドル。NULL を渡した場合は何もしません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるハンドルに対する呼び出しは同時に実行できます。\n
     *  同一 @p regex に対する操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_regex_dispose(cplat_regex *regex);

    /**
     *  @brief          マッチ範囲の記録に必要な要素数を取得します。
     *  @param[in]      regex  対象のハンドル。NULL を渡してはなりません。
     *  @return         捕捉グループ数に全体マッチの 1 を加えた値。
     *                  @p regex が NULL の場合は 0 を返します。
     *
     *  照合 API の `matches_out` には、本関数が返す要素数の配列を渡してください。\n
     *  @ref CPLAT_REGEX_NOSUB を指定してコンパイルした場合は 1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_regex_get_group_count(const cplat_regex *regex);

    /**
     *  @brief          入力の一部にパターンが一致するかを照合します。
     *  @param[in]      regex             対象のハンドル。NULL を渡してはなりません。
     *  @param[in]      text              UTF-8 の入力文字列。NULL を渡してはなりません。
     *  @param[in]      text_len          `text` のバイト数。
     *  @param[in]      start_offset      照合を開始するバイト オフセット。
     *                                    コード ポイント境界を指す必要があります。
     *  @param[in]      match_flags       照合フラグ (@ref CPLAT_REGEX_MATCH_DEFAULT など) のビット和。
     *  @param[out]     matches_out       マッチ範囲の格納先。NULL を指定した場合は格納しません。
     *  @param[in]      matches_capacity  `matches_out` の要素数。
     *  @param[out]     matched_out       一致した場合に 1、一致しなかった場合に 0 を格納します。
     *                                    NULL を渡してはなりません。
     *  @param[out]     detail_out        エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、@ref CPLAT_ERR_LIMIT_EXCEEDED 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  一致しなかったことはエラーではありません。戻り値は @ref CPLAT_OK となり、
     *  `matched_out` に 0 を格納します。\n
     *  `matches_out` の要素数が @ref cplat_regex_get_group_count() の戻り値より少ない場合、
     *  先頭から `matches_capacity` 個までを格納し、残りは切り捨てます。\n
     *  `start_offset` に 0 以外を指定した場合、それより前に文字が存在するものとして
     *  `^` と `\\b` を評価します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_regex_search(const cplat_regex *regex, const char *text, size_t text_len,
                                                  size_t start_offset, unsigned int match_flags,
                                                  cplat_regex_match *matches_out, size_t matches_capacity,
                                                  int *matched_out, cplat_error *detail_out);

    /**
     *  @brief          入力の全体がパターンに一致するかを照合します。
     *  @param[in]      regex             対象のハンドル。NULL を渡してはなりません。
     *  @param[in]      text              UTF-8 の入力文字列。NULL を渡してはなりません。
     *  @param[in]      text_len          `text` のバイト数。
     *  @param[in]      match_flags       照合フラグ (@ref CPLAT_REGEX_MATCH_DEFAULT など) のビット和。
     *  @param[out]     matches_out       マッチ範囲の格納先。NULL を指定した場合は格納しません。
     *  @param[in]      matches_capacity  `matches_out` の要素数。
     *  @param[out]     matched_out       一致した場合に 1、一致しなかった場合に 0 を格納します。
     *                                    NULL を渡してはなりません。
     *  @param[out]     detail_out        エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、@ref CPLAT_ERR_LIMIT_EXCEEDED 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @ref cplat_regex_search() と異なり、入力の先頭から末尾までがパターンに
     *  一致する場合のみ `matched_out` に 1 を格納します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_regex_matches(const cplat_regex *regex, const char *text, size_t text_len,
                                                   unsigned int match_flags, cplat_regex_match *matches_out,
                                                   size_t matches_capacity, int *matched_out, cplat_error *detail_out);

    /**
     *  @brief          一致した箇所を置換した文字列を生成します。
     *  @param[in]      regex              対象のハンドル。NULL を渡してはなりません。
     *  @param[in]      text               UTF-8 の入力文字列。NULL を渡してはなりません。
     *  @param[in]      text_len           `text` のバイト数。
     *  @param[in]      replacement        UTF-8 の置換文字列。NULL を渡してはなりません。
     *  @param[in]      flags              照合フラグと置換フラグのビット和。
     *  @param[out]     result_out         置換結果の格納先 (null 終端付き)。
     *                                     必要サイズの問い合わせでは NULL を指定します。
     *  @param[in]      result_size        `result_out` のバイト数。
     *                                     必要サイズの問い合わせでは 0 を指定します。
     *  @param[out]     required_size_out  必要なバイト数 (null 終端を含む) の格納先。
     *                                     NULL を指定した場合は格納しません。
     *  @param[out]     detail_out         エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  置換後のサイズは事前に見積もれないため、`result_out` に NULL、`result_size` に 0 を
     *  指定して必要サイズだけを求め、バッファーを確保してから再度呼び出せます。\n
     *  `result_size` が不足する場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *  `required_size_out` に必要なバイト数を格納します。
     *
     *  @note           置換文字列の書式は既定で ECMAScript の書式 (`$1` `$&` `` $` `` `$'` `$$`) です。\n
     *                  @ref CPLAT_REGEX_EXTENDED や @ref CPLAT_REGEX_BASIC を指定して
     *                  コンパイルした場合も、置換文字列の書式は変わりません。\n
     *                  sed の書式を使用する場合は @ref CPLAT_REGEX_REPLACE_SED を指定してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_regex_replace(const cplat_regex *regex, const char *text, size_t text_len,
                                                   const char *replacement, unsigned int flags, char *result_out,
                                                   size_t result_size, size_t *required_size_out,
                                                   cplat_error *detail_out);

    /** @brief 一致箇所を順に列挙するイテレーターのハンドル (不透明構造体)。 */
    typedef struct cplat_regex_iter cplat_regex_iter;

    /**
     *  @brief          一致箇所を順に列挙するイテレーターを生成します。
     *  @param[in]      regex        対象のハンドル。NULL を渡してはなりません。
     *  @param[in]      text         UTF-8 の入力文字列。NULL を渡してはなりません。
     *  @param[in]      text_len     `text` のバイト数。
     *  @param[in]      match_flags  照合フラグ (@ref CPLAT_REGEX_MATCH_DEFAULT など) のビット和。
     *  @param[out]     iter_out     生成したハンドルの格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out   エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、@ref CPLAT_ERR_LIMIT_EXCEEDED 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  `text` の内容は本関数の内部へ複製するため、`text` がイテレーターより長く
     *  存在している必要はありません。\n
     *  一方で `regex` はイテレーターを破棄するまで有効である必要があります。\n
     *  生成したハンドルは @ref cplat_regex_iter_dispose() で破棄してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_regex_iter_create(const cplat_regex *regex, const char *text, size_t text_len,
                                                       unsigned int match_flags, cplat_regex_iter **iter_out,
                                                       cplat_error *detail_out);

    /**
     *  @brief          次の一致箇所を取得します。
     *  @param[in]      iter              対象のイテレーター。NULL を渡してはなりません。
     *  @param[out]     matches_out       マッチ範囲の格納先。NULL を指定した場合は格納しません。
     *  @param[in]      matches_capacity  `matches_out` の要素数。
     *  @param[out]     has_match_out     一致箇所を取得した場合に 1、列挙が終わった場合に 0 を
     *                                    格納します。NULL を渡してはなりません。
     *  @param[out]     detail_out        エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  列挙が終わったことはエラーではありません。戻り値は @ref CPLAT_OK となり、
     *  `has_match_out` に 0 を格納します。\n
     *  空文字列への一致が発生した場合は、次の照合位置を 1 文字進めます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるハンドルに対する呼び出しは同時に実行できます。\n
     *  同一 @p iter に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_regex_iter_next(cplat_regex_iter *iter, cplat_regex_match *matches_out,
                                                     size_t matches_capacity, int *has_match_out,
                                                     cplat_error *detail_out);

    /**
     *  @brief          イテレーターを破棄します。
     *  @param[in]      iter  破棄するイテレーター。NULL を渡した場合は何もしません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるハンドルに対する呼び出しは同時に実行できます。\n
     *  同一 @p iter に対する操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_regex_iter_dispose(cplat_regex_iter *iter);

    /**
     *  @brief          パターンに一致する箇所を区切りとして入力を分割します。
     *  @param[in]      regex            対象のハンドル。NULL を渡してはなりません。
     *  @param[in]      text             UTF-8 の入力文字列。NULL を渡してはなりません。
     *  @param[in]      text_len         `text` のバイト数。
     *  @param[in]      max_parts        分割数の上限。0 を指定した場合は上限を設けません。
     *                                   上限に達した時点で、残りをすべて最後の要素とします。
     *  @param[in]      match_flags      照合フラグ (@ref CPLAT_REGEX_MATCH_DEFAULT など) のビット和。
     *  @param[out]     parts_out        分割結果の範囲を格納する配列。
     *                                   必要件数の問い合わせでは NULL を指定します。
     *  @param[in]      parts_capacity   `parts_out` の要素数。
     *                                   必要件数の問い合わせでは 0 を指定します。
     *  @param[out]     part_count_out   分割結果の件数の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out       エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_INVALID_ENCODING 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_LIMIT_EXCEEDED 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  一致箇所が 1 つも無い場合は、入力全体を表す 1 件を格納します。\n
     *  `parts_capacity` が不足する場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *  `part_count_out` に必要な件数を格納します。格納できる範囲までは `parts_out` へ書き込みます。\n
     *  `parts_out` に NULL、`parts_capacity` に 0 を指定した場合は、件数の問い合わせとして
     *  @ref CPLAT_OK を返し、`part_count_out` にのみ件数を格納します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_regex_split(const cplat_regex *regex, const char *text, size_t text_len,
                                                 size_t max_parts, unsigned int match_flags,
                                                 cplat_regex_match *parts_out, size_t parts_capacity,
                                                 size_t *part_count_out, cplat_error *detail_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_REGEX_REGEX_H */
