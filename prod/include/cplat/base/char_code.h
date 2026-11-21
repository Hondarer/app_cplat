/**
 *******************************************************************************
 *  @file           char_code.h
 *  @brief          ASCII・UTF-8・UTF-16・Unicode の規格が定める定数を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/21
 *  @version        1.0.0
 *
 *  US-ASCII、RFC 3629 (UTF-8)、RFC 2781 / Unicode 標準 (UTF-16 サロゲート、
 *  置換文字、コード ポイント上限) が定める定数を集約します。\n
 *  公開ヘッダーとライブラリ内部実装の双方で共有する文字符号定数の正本です。
 *
 *  @see            https://datatracker.ietf.org/doc/html/rfc3629
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

#ifndef CPLAT_BASE_CHAR_CODE_H
#define CPLAT_BASE_CHAR_CODE_H

#include <assert.h>

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

/* ================================================================ */
/* ASCII 制御文字と印字可能範囲の境界                               */
/* ================================================================ */

/** @brief ASCII の ETX (End of Text、Ctrl+C) の文字コードです。 */
#define CPLAT_ASCII_ETX 0x03U

/** @brief ASCII の BS (Backspace) の文字コードです。 */
#define CPLAT_ASCII_BS 0x08U

/** @brief ASCII の ESC (Escape) の文字コードです。 */
#define CPLAT_ASCII_ESC 0x1BU

/** @brief ASCII 印字可能文字の下限 (SP、空白文字) です。 */
#define CPLAT_ASCII_PRINTABLE_MIN 0x20U

/** @brief ASCII の DEL (Delete) の文字コードです。 */
#define CPLAT_ASCII_DEL 0x7FU

/** @brief 7-bit ASCII 範囲の上限 + 1 (非 ASCII バイトの開始値) です。 */
#define CPLAT_ASCII_BOUND_U8 0x80U

/* ================================================================ */
/* UTF-8 の符号長・バイト判定・復号/符号化定数 (RFC 3629)           */
/* ================================================================ */

/**
 *  @brief          UTF-8 の 1 バイト文字 (ASCII) のバイト数 [byte] です。
 *  @see            https://datatracker.ietf.org/doc/html/rfc3629
 */
#define CPLAT_UTF8_BYTES_1 1U

/** @brief UTF-8 の 2 バイト文字のバイト数 [byte] です。 */
#define CPLAT_UTF8_BYTES_2 2U

/** @brief UTF-8 の 3 バイト文字のバイト数 [byte] です。 */
#define CPLAT_UTF8_BYTES_3 3U

/** @brief UTF-8 の 4 バイト文字のバイト数 [byte] です。 */
#define CPLAT_UTF8_BYTES_4 4U

/**
 *  @brief          UTF-8 の 1 文字の最大バイト数 [byte] です。
 *  @see            https://datatracker.ietf.org/doc/html/rfc3629
 */
#define CPLAT_UTF8_MAX_BYTES 4U

/** @brief UTF-8 の 1 文字に含まれる後続バイトの最大数です (4 [byte] 文字の 2〜4 バイト目)。 */
#define CPLAT_UTF8_MAX_CONT_BYTES 3U

/** @brief UTF-8 の 1 バイト ASCII 文字を判定する最上位ビット マスクです (`0xxxxxxx`)。 */
#define CPLAT_UTF8_ASCII_MASK 0x80U

/** @brief UTF-8 の後続バイトを判定するビット マスクです (上位 2 bit)。 */
#define CPLAT_UTF8_CONT_MASK 0xC0U

/** @brief UTF-8 の後続バイトの上位 2 bit の値です (`10xxxxxx`)。 */
#define CPLAT_UTF8_CONT_VALUE 0x80U

/** @brief UTF-8 の 2 バイト文字の先頭バイトを判定するビット マスクです (`110xxxxx`)。 */
#define CPLAT_UTF8_LEAD2_MASK 0xE0U

/** @brief UTF-8 の 2 バイト文字の先頭バイトの上位 3 bit の値です (`110xxxxx`)。 */
#define CPLAT_UTF8_LEAD2_VALUE 0xC0U

/** @brief UTF-8 の 3 バイト文字の先頭バイトを判定するビット マスクです (`1110xxxx`)。 */
#define CPLAT_UTF8_LEAD3_MASK 0xF0U

/** @brief UTF-8 の 3 バイト文字の先頭バイトの上位 4 bit の値です (`1110xxxx`)。 */
#define CPLAT_UTF8_LEAD3_VALUE 0xE0U

/** @brief UTF-8 の 4 バイト文字の先頭バイトを判定するビット マスクです (`11110xxx`)。 */
#define CPLAT_UTF8_LEAD4_MASK 0xF8U

/** @brief UTF-8 の 4 バイト文字の先頭バイトの上位 5 bit の値です (`11110xxx`)。 */
#define CPLAT_UTF8_LEAD4_VALUE 0xF0U

/**
 *  @brief          RFC 3629 が定める UTF-8 の有効な 2 バイト文字先頭バイトの下限です (`0xC0`/`0xC1` はオーバー ロングのため除外)。
 *  @see            https://datatracker.ietf.org/doc/html/rfc3629
 */
#define CPLAT_UTF8_LEAD2_MIN 0xC2U

/** @brief RFC 3629 が定める UTF-8 の有効な 2 バイト文字先頭バイトの上限です。 */
#define CPLAT_UTF8_LEAD2_MAX 0xDFU

/** @brief RFC 3629 が定める UTF-8 の有効な 3 バイト文字先頭バイトの下限です。 */
#define CPLAT_UTF8_LEAD3_MIN 0xE0U

/** @brief RFC 3629 が定める UTF-8 の有効な 3 バイト文字先頭バイトの上限です。 */
#define CPLAT_UTF8_LEAD3_MAX 0xEFU

/** @brief RFC 3629 が定める UTF-8 の有効な 4 バイト文字先頭バイトの下限です。 */
#define CPLAT_UTF8_LEAD4_MIN 0xF0U

/**
 *  @brief          RFC 3629 が定める UTF-8 の有効な 4 バイト文字先頭バイトの上限です (`U+10FFFF` 超を除外)。
 *  @see            https://datatracker.ietf.org/doc/html/rfc3629
 */
#define CPLAT_UTF8_LEAD4_MAX 0xF4U

/** @brief UTF-8 の 2 バイト文字の先頭バイトからデータ部 (下位 5 bit) を取り出すマスクです。 */
#define CPLAT_UTF8_LEAD2_DATA_MASK 0x1FU

/** @brief UTF-8 の 3 バイト文字の先頭バイトからデータ部 (下位 4 bit) を取り出すマスクです。 */
#define CPLAT_UTF8_LEAD3_DATA_MASK 0x0FU

/** @brief UTF-8 の 4 バイト文字の先頭バイトからデータ部 (下位 3 bit) を取り出すマスクです。 */
#define CPLAT_UTF8_LEAD4_DATA_MASK 0x07U

/** @brief UTF-8 の後続バイトからデータ部 (下位 6 bit) を取り出すマスクです。 */
#define CPLAT_UTF8_CONT_DATA_MASK 0x3FU

/** @brief UTF-8 の後続バイト 1 つあたりのビット シフト量 [bit] です。 */
#define CPLAT_UTF8_CONT_SHIFT 6U

/** @brief UTF-8 の 3 バイト文字の先頭バイトに対応するビット シフト量 [bit] です。 */
#define CPLAT_UTF8_LEAD3_SHIFT (CPLAT_UTF8_CONT_SHIFT * 2U)

/** @brief UTF-8 の 4 バイト文字の先頭バイトに対応するビット シフト量 [bit] です。 */
#define CPLAT_UTF8_LEAD4_SHIFT (CPLAT_UTF8_CONT_SHIFT * 3U)

/** @brief UTF-8 の 1 バイト符号化で表せるコード ポイントの上限 (`U+007F`) です。 */
#define CPLAT_UTF8_1BYTE_CP_MAX 0x7FU

/** @brief UTF-8 の 2 バイト符号化で表せるコード ポイントの上限 (`U+07FF`) です。 */
#define CPLAT_UTF8_2BYTE_CP_MAX 0x7FFU

/** @brief UTF-8 の 3 バイト符号化で表せるコード ポイントの上限 (`U+FFFF`) です。 */
#define CPLAT_UTF8_3BYTE_CP_MAX 0xFFFFU

/* ================================================================ */
/* UTF-16 サロゲート ペア範囲と Unicode 定数                        */
/* ================================================================ */

/** @brief UTF-16 上位サロゲートのコード単位の下限 (`U+D800`) です。 */
#define CPLAT_UTF16_HIGH_SURROGATE_MIN 0xD800U

/** @brief UTF-16 上位サロゲートのコード単位の上限 (`U+DBFF`) です。 */
#define CPLAT_UTF16_HIGH_SURROGATE_MAX 0xDBFFU

/** @brief UTF-16 下位サロゲートのコード単位の下限 (`U+DC00`) です。 */
#define CPLAT_UTF16_LOW_SURROGATE_MIN 0xDC00U

/** @brief UTF-16 下位サロゲートのコード単位の上限 (`U+DFFF`) です。 */
#define CPLAT_UTF16_LOW_SURROGATE_MAX 0xDFFFU

/** @brief UTF-16 サロゲート コード単位の上位 6 bit を判定するマスクです。 */
#define CPLAT_UTF16_SURROGATE_MASK 0xFC00U

/** @brief UTF-16 サロゲート コード単位の下位 10 bit を取り出すマスクです。 */
#define CPLAT_UTF16_SURROGATE_DATA_MASK 0x3FFU

/** @brief UTF-16 サロゲート ペアで表す補助平面のコード ポイント基数 (`U+10000`) です。 */
#define CPLAT_UTF16_SURROGATE_BASE 0x10000U

/** @brief UTF-16 上位サロゲートのビット シフト量 [bit] です。 */
#define CPLAT_UTF16_SURROGATE_SHIFT 10U

/**
 *  @brief          Unicode の有効なコード ポイントの上限 (`U+10FFFF`) です。
 *  @see            https://datatracker.ietf.org/doc/html/rfc3629
 */
#define CPLAT_UNICODE_CODE_POINT_MAX 0x10FFFFU

/** @brief 不正な符号位置や単独サロゲートの置換に用いる Unicode 置換文字 (`U+FFFD`) です。 */
#define CPLAT_UNICODE_REPLACEMENT_CHAR 0xFFFDU

static_assert(CPLAT_ASCII_BOUND_U8 == (CPLAT_ASCII_DEL + 1U), "cplat: ASCII bound must follow DEL");
static_assert(CPLAT_UTF8_MAX_BYTES == CPLAT_UTF8_BYTES_4, "cplat: UTF-8 max bytes must equal 4");
static_assert(CPLAT_UTF8_MAX_CONT_BYTES == (CPLAT_UTF8_MAX_BYTES - 1U),
              "cplat: UTF-8 max continuation bytes must be max bytes - 1");
static_assert((CPLAT_UTF8_CONT_MASK & CPLAT_UTF8_CONT_VALUE) == CPLAT_UTF8_CONT_VALUE,
              "cplat: UTF-8 continuation value must fit in mask");
static_assert(CPLAT_UTF8_LEAD2_MIN < CPLAT_UTF8_LEAD2_MAX, "cplat: UTF-8 2-byte lead range must be valid");
static_assert(CPLAT_UTF8_LEAD3_MIN < CPLAT_UTF8_LEAD3_MAX, "cplat: UTF-8 3-byte lead range must be valid");
static_assert(CPLAT_UTF8_LEAD4_MIN < CPLAT_UTF8_LEAD4_MAX, "cplat: UTF-8 4-byte lead range must be valid");
static_assert(CPLAT_UTF8_LEAD3_SHIFT == (CPLAT_UTF8_CONT_SHIFT * 2U), "cplat: UTF-8 3-byte lead shift must be 12");
static_assert(CPLAT_UTF8_LEAD4_SHIFT == (CPLAT_UTF8_CONT_SHIFT * 3U), "cplat: UTF-8 4-byte lead shift must be 18");
static_assert(CPLAT_UTF16_HIGH_SURROGATE_MIN < CPLAT_UTF16_HIGH_SURROGATE_MAX,
              "cplat: UTF-16 high surrogate range must be valid");
static_assert(CPLAT_UTF16_LOW_SURROGATE_MIN < CPLAT_UTF16_LOW_SURROGATE_MAX,
              "cplat: UTF-16 low surrogate range must be valid");
static_assert((CPLAT_UTF16_HIGH_SURROGATE_MAX + 1U) == CPLAT_UTF16_LOW_SURROGATE_MIN,
              "cplat: UTF-16 surrogate ranges must be contiguous");
static_assert(CPLAT_UNICODE_CODE_POINT_MAX > CPLAT_UTF16_SURROGATE_BASE,
              "cplat: Unicode max code point must exceed surrogate base");

/** @} */

#endif /* CPLAT_BASE_CHAR_CODE_H */
