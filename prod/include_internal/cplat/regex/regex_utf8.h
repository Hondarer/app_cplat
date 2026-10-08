/**
 *******************************************************************************
 *  @file           regex_utf8.h
 *  @brief          regex モジュール内部の UTF-8 変換ヘルパーを提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/04
 *  @version        1.0.0
 *
 *  UTF-8 文字列を「UTF-16 コード単位を `wchar_t` へ 1 個ずつ格納した文字列」へ
 *  変換し、コード単位の索引から UTF-8 バイト オフセットを復元するための
 *  写像表をあわせて生成します。\n
 *  Linux の `wchar_t` は 32 ビットですが、あえて UTF-16 コード単位を格納する
 *  ことで、`wchar_t` が 16 ビットである Windows と照合セマンティクスを
 *  一致させます。
 *
 *  本ヘッダーは C++ 専用であり、`cplat_internal.h` には含めません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_REGEX_REGEX_UTF8_H
#define CPLAT_REGEX_REGEX_UTF8_H

#include <cstddef>
#include <string>
#include <vector>

namespace cplat
{
namespace regex_detail
{

/**
 *  @brief          UTF-8 文字列を UTF-16 コード単位の列へ変換します。
 *  @param[in]      text        変換元の UTF-8 文字列。
 *  @param[in]      text_len    `text` のバイト数。
 *  @param[out]     units_out   変換結果のコード単位列。
 *  @param[out]     offsets_out コード単位索引から UTF-8 バイト オフセットへの写像表。
 *                              要素数は `units_out.size() + 1` になります。
 *  @return         変換に成功した場合は true、`text` が不正な UTF-8 の場合は false。
 *
 *  オーバー ロング表現、単独のサロゲート、U+10FFFF を超える値、
 *  途中で切れた列は、いずれも不正として false を返します。\n
 *  サロゲート ペアを構成する 2 個のコード単位には、同一の UTF-8 バイト
 *  オフセット (そのコード ポイントの開始位置) を割り当てます。
 */
bool cplat_internal_regex_utf8_decode(const char *text, std::size_t text_len, std::wstring &units_out,
                                      std::vector<std::size_t> &offsets_out);

/**
 *  @brief          UTF-16 コード単位の列を UTF-8 文字列へ変換します。
 *  @param[in]      units      変換元のコード単位列。
 *  @param[out]     text_out   変換結果の UTF-8 文字列。
 *  @return         変換に成功した場合は true、`units` に不正なサロゲートが
 *                  含まれる場合は false。
 */
bool cplat_internal_regex_utf8_encode(const std::wstring &units, std::string &text_out);

/**
 *  @brief          マッチ開始位置のコード単位索引を UTF-8 バイト オフセットへ変換します。
 *  @param[in]      units    照合に使用したコード単位列。
 *  @param[in]      offsets  @ref cplat_internal_regex_utf8_decode() が生成した写像表。
 *  @param[in]      index    コード単位索引。
 *  @return         UTF-8 バイト オフセット。
 *
 *  索引がサロゲート ペアの内側を指す場合は、そのコード ポイントの
 *  開始位置へ丸めます。
 */
std::size_t cplat_internal_regex_offset_of_begin(const std::wstring &units, const std::vector<std::size_t> &offsets,
                                                 std::size_t index);

/**
 *  @brief          マッチ終了位置のコード単位索引を UTF-8 バイト オフセットへ変換します。
 *  @param[in]      units    照合に使用したコード単位列。
 *  @param[in]      offsets  @ref cplat_internal_regex_utf8_decode() が生成した写像表。
 *  @param[in]      index    コード単位索引。
 *  @return         UTF-8 バイト オフセット。
 *
 *  索引がサロゲート ペアの内側を指す場合は、そのコード ポイントの
 *  終了位置へ丸めます。
 */
std::size_t cplat_internal_regex_offset_of_end(const std::wstring &units, const std::vector<std::size_t> &offsets,
                                               std::size_t index);

/**
 *  @brief          UTF-8 バイト オフセットをコード単位索引へ変換します。
 *  @param[in]      offsets    @ref cplat_internal_regex_utf8_decode() が生成した写像表。
 *  @param[in]      offset     UTF-8 バイト オフセット。
 *  @param[out]     index_out  変換結果のコード単位索引。
 *  @return         `offset` がコード ポイント境界を指す場合は true、
 *                  それ以外は false。
 */
bool cplat_internal_regex_index_of_offset(const std::vector<std::size_t> &offsets, std::size_t offset,
                                          std::size_t &index_out);

} /* namespace regex_detail */
} /* namespace cplat */

#endif /* CPLAT_REGEX_REGEX_UTF8_H */
