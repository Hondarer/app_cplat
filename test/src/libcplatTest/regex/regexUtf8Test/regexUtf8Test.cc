#include <testfw.h>

#include <cplat/regex/regex_utf8.h>

#include <string>
#include <vector>

using cplat::regex_detail::cplat_internal_regex_index_of_offset;
using cplat::regex_detail::cplat_internal_regex_offset_of_begin;
using cplat::regex_detail::cplat_internal_regex_offset_of_end;
using cplat::regex_detail::cplat_internal_regex_utf8_decode;
using cplat::regex_detail::cplat_internal_regex_utf8_encode;

class regexUtf8Test : public Test
{
};

// ASCII 文字列が 1 バイト 1 コード単位へ変換されることの確認
TEST_F(regexUtf8Test, decode_ascii_maps_one_byte_to_one_unit)
{
    // Arrange
    const std::string text = "abc";
    std::wstring units;
    std::vector<std::size_t> offsets;
    bool decoded = false;

    // Pre-Assert

    // Act
    decoded = cplat_internal_regex_utf8_decode(text.data(), text.size(), units,
                                               offsets); // [手順] - ASCII 文字列 "abc" をコード単位列へ変換する。

    // Assert
    EXPECT_TRUE(decoded); // [確認_正常系] - cplat_internal_regex_utf8_decode の戻り値が true であること。
    EXPECT_EQ((std::size_t)3, units.size());   // [確認_正常系] - コード単位数が 3 であること。
    EXPECT_EQ(std::wstring(L"abc"), units);    // [確認_正常系] - コード単位列が L"abc" であること。
    ASSERT_EQ((std::size_t)4, offsets.size()); // [確認_正常系] - 写像表の要素数がコード単位数 + 1 であること。
    EXPECT_EQ((std::size_t)0, offsets[0]);     // [確認_正常系] - 1 文字目のバイト オフセットが 0 であること。
    EXPECT_EQ((std::size_t)1, offsets[1]);     // [確認_正常系] - 2 文字目のバイト オフセットが 1 であること。
    EXPECT_EQ((std::size_t)2, offsets[2]);     // [確認_正常系] - 3 文字目のバイト オフセットが 2 であること。
    EXPECT_EQ((std::size_t)3, offsets[3]);     // [確認_正常系] - 終端のバイト オフセットが 3 であること。
}

// 日本語 (BMP) の 1 文字が 1 コード単位 3 バイトへ対応することの確認
TEST_F(regexUtf8Test, decode_japanese_maps_three_bytes_to_one_unit)
{
    // Arrange
    const std::string text = u8"あいう";
    std::wstring units;
    std::vector<std::size_t> offsets;
    bool decoded = false;

    // Pre-Assert

    // Act
    decoded = cplat_internal_regex_utf8_decode(text.data(), text.size(), units,
                                               offsets); // [手順] - 日本語 3 文字 "あいう" をコード単位列へ変換する。

    // Assert
    EXPECT_TRUE(decoded); // [確認_正常系] - cplat_internal_regex_utf8_decode の戻り値が true であること。
    EXPECT_EQ((std::size_t)3, units.size());   // [確認_正常系] - コード単位数が 3 であること。
    ASSERT_EQ((std::size_t)4, offsets.size()); // [確認_正常系] - 写像表の要素数が 4 であること。
    EXPECT_EQ((std::size_t)0, offsets[0]);     // [確認_正常系] - "あ" のバイト オフセットが 0 であること。
    EXPECT_EQ((std::size_t)3, offsets[1]);     // [確認_正常系] - "い" のバイト オフセットが 3 であること。
    EXPECT_EQ((std::size_t)6, offsets[2]);     // [確認_正常系] - "う" のバイト オフセットが 6 であること。
    EXPECT_EQ((std::size_t)9, offsets[3]);     // [確認_正常系] - 終端のバイト オフセットが 9 であること。
}

// BMP 外の 1 文字がサロゲート ペア 2 コード単位へ対応することの確認
TEST_F(regexUtf8Test, decode_astral_maps_one_code_point_to_surrogate_pair)
{
    // Arrange
    const std::string text = u8"\U0001F600";
    std::wstring units;
    std::vector<std::size_t> offsets;
    bool decoded = false;

    // Pre-Assert

    // Act
    decoded = cplat_internal_regex_utf8_decode(text.data(), text.size(), units,
                                               offsets); // [手順] - BMP 外の 1 文字 U+1F600 をコード単位列へ変換する。

    // Assert
    EXPECT_TRUE(decoded); // [確認_正常系] - cplat_internal_regex_utf8_decode の戻り値が true であること。
    ASSERT_EQ((std::size_t)2, units.size());   // [確認_正常系] - コード単位数が 2 (サロゲート ペア) であること。
    EXPECT_EQ((wchar_t)0xD83D, units[0]);      // [確認_正常系] - 上位サロゲートが 0xD83D であること。
    EXPECT_EQ((wchar_t)0xDE00, units[1]);      // [確認_正常系] - 下位サロゲートが 0xDE00 であること。
    ASSERT_EQ((std::size_t)3, offsets.size()); // [確認_正常系] - 写像表の要素数が 3 であること。
    EXPECT_EQ((std::size_t)0, offsets[0]);     // [確認_正常系] - 上位サロゲートのバイト オフセットが 0 であること。
    EXPECT_EQ((std::size_t)0,
              offsets[1]); // [確認_正常系] - 下位サロゲートにも同じ開始バイト オフセット 0 が割り当たること。
    EXPECT_EQ((std::size_t)4, offsets[2]); // [確認_正常系] - 終端のバイト オフセットが 4 であること。
}

// 不正な UTF-8 が変換を拒否されることの確認
TEST_F(regexUtf8Test, decode_rejects_invalid_utf8)
{
    // Arrange
    std::wstring units;
    std::vector<std::size_t> offsets;
    const std::string overlong("\xC0\x80", 2);
    const std::string surrogate("\xED\xA0\x80", 3);
    const std::string too_large("\xF5\x80\x80\x80", 4);
    const std::string truncated("\xE3\x81", 2);
    const std::string lone_trail("\x80", 1);

    // Pre-Assert

    // Act

    // Assert
    EXPECT_FALSE(cplat_internal_regex_utf8_decode(
        overlong.data(), overlong.size(), units,
        offsets)); // [確認_異常系] - オーバー ロング表現 C0 80 に対する cplat_internal_regex_utf8_decode の戻り値が
                   // false であること。
    EXPECT_FALSE(cplat_internal_regex_utf8_decode(
        surrogate.data(), surrogate.size(), units,
        offsets)); // [確認_異常系] - 単独サロゲート ED A0 80 に対する cplat_internal_regex_utf8_decode の戻り値が
                   // false であること。
    EXPECT_FALSE(cplat_internal_regex_utf8_decode(
        too_large.data(), too_large.size(), units,
        offsets)); // [確認_異常系] - U+10FFFF を超える F5 80 80 80 に対する cplat_internal_regex_utf8_decode の
                   // 戻り値が false であること。
    EXPECT_FALSE(cplat_internal_regex_utf8_decode(
        truncated.data(), truncated.size(), units,
        offsets)); // [確認_異常系] - 途中で切れた E3 81 に対する cplat_internal_regex_utf8_decode の戻り値が
                   // false であること。
    EXPECT_FALSE(cplat_internal_regex_utf8_decode(
        lone_trail.data(), lone_trail.size(), units,
        offsets)); // [確認_異常系] - 後続バイト単独の 80 に対する cplat_internal_regex_utf8_decode の戻り値が
                   // false であること。
}

// コード単位列を UTF-8 へ戻せることの確認
TEST_F(regexUtf8Test, encode_restores_original_text)
{
    // Arrange
    const std::string text = u8"aあ\U0001F600z";
    std::wstring units;
    std::vector<std::size_t> offsets;
    std::string encoded;
    bool decoded = false;
    bool result = false;

    decoded = cplat_internal_regex_utf8_decode(
        text.data(), text.size(), units,
        offsets);         // [状態] - ASCII、日本語、BMP 外を含む文字列をコード単位列へ変換しておく。
    ASSERT_TRUE(decoded); // [状態確認] - cplat_internal_regex_utf8_decode の戻り値が true であること。

    // Pre-Assert

    // Act
    result = cplat_internal_regex_utf8_encode(units, encoded); // [手順] - コード単位列を UTF-8 へ戻す。

    // Assert
    EXPECT_TRUE(result);      // [確認_正常系] - cplat_internal_regex_utf8_encode の戻り値が true であること。
    EXPECT_EQ(text, encoded); // [確認_正常系] - 変換結果が元の UTF-8 文字列と一致すること。
}

// 単独の下位サロゲートが符号化を拒否されることの確認
TEST_F(regexUtf8Test, encode_rejects_lone_surrogate)
{
    // Arrange
    std::wstring units;
    std::string encoded;
    bool result = false;

    units.push_back((wchar_t)0xDE00); // [状態] - 対となる上位サロゲートを持たない下位サロゲートのみを用意する。

    // Pre-Assert

    // Act
    result = cplat_internal_regex_utf8_encode(units, encoded); // [手順] - 単独の下位サロゲートを UTF-8 へ変換する。

    // Assert
    EXPECT_FALSE(result); // [確認_異常系] - cplat_internal_regex_utf8_encode の戻り値が false であること。
}

// サロゲート ペアの内側を指す索引が丸められることの確認
TEST_F(regexUtf8Test, offset_conversion_rounds_inside_surrogate_pair)
{
    // Arrange
    const std::string text = u8"\U0001F600z";
    std::wstring units;
    std::vector<std::size_t> offsets;
    std::size_t begin_offset = 0;
    std::size_t end_offset = 0;

    ASSERT_TRUE(cplat_internal_regex_utf8_decode(
        text.data(), text.size(), units,
        offsets)); // [状態] - BMP 外の 1 文字と ASCII 1 文字からなる文字列を変換しておく。
                   // [状態確認] - cplat_internal_regex_utf8_decode の戻り値が true であること。

    // Pre-Assert
    ASSERT_EQ((std::size_t)3, units.size()); // [Pre-Assert確認_正常系] - コード単位数が 3 であること。

    // Act
    begin_offset =
        cplat_internal_regex_offset_of_begin(units, offsets,
                                             1); // [手順] - 下位サロゲートを指す索引 1 を開始位置として変換する。
    end_offset =
        cplat_internal_regex_offset_of_end(units, offsets,
                                           1); // [手順] - 下位サロゲートを指す索引 1 を終了位置として変換する。

    // Assert
    EXPECT_EQ(
        (std::size_t)0,
        begin_offset); // [確認_正常系] - cplat_internal_regex_offset_of_begin の戻り値がコード ポイント先頭の 0 であること。
    EXPECT_EQ(
        (std::size_t)4,
        end_offset); // [確認_正常系] - cplat_internal_regex_offset_of_end の戻り値がコード ポイント末尾の 4 であること。
}

// バイト オフセットからコード単位索引を復元できることの確認
TEST_F(regexUtf8Test, index_of_offset_accepts_code_point_boundary_only)
{
    // Arrange
    const std::string text = u8"あい";
    std::wstring units;
    std::vector<std::size_t> offsets;
    std::size_t index = 0;

    ASSERT_TRUE(cplat_internal_regex_utf8_decode(text.data(), text.size(), units,
                                                 offsets)); // [状態] - 日本語 2 文字の文字列を変換しておく。
    // [状態確認] - cplat_internal_regex_utf8_decode の戻り値が true であること。

    // Pre-Assert

    // Act

    // Assert
    EXPECT_TRUE(cplat_internal_regex_index_of_offset(
        offsets, 3,
        index)); // [確認_正常系] - 境界であるオフセット 3 に対する cplat_internal_regex_index_of_offset の
                 // 戻り値が true であること。
    EXPECT_EQ((std::size_t)1, index); // [確認_正常系] - 復元された索引が 1 であること。
    EXPECT_TRUE(cplat_internal_regex_index_of_offset(
        offsets, 6,
        index)); // [確認_正常系] - 終端であるオフセット 6 に対する cplat_internal_regex_index_of_offset の
                 // 戻り値が true であること。
    EXPECT_EQ((std::size_t)2, index); // [確認_正常系] - 復元された索引が 2 であること。
    EXPECT_FALSE(cplat_internal_regex_index_of_offset(
        offsets, 1,
        index)); // [確認_異常系] - 文字の途中であるオフセット 1 に対する cplat_internal_regex_index_of_offset
                 // の戻り値が false であること。
    EXPECT_FALSE(cplat_internal_regex_index_of_offset(
        offsets, 7,
        index)); // [確認_異常系] - 範囲外であるオフセット 7 に対する cplat_internal_regex_index_of_offset の
                 // 戻り値が false であること。
}

// NULL の空入力と不正な後続バイトが安全に処理されることの確認
TEST_F(regexUtf8Test, decode_handles_empty_null_and_invalid_trail_inputs)
{
    // Arrange
    std::wstring units;
    std::vector<std::size_t> offsets;
    const std::string invalid_trail("\xE2\x28\xA1", 3);
    const std::string overlong_three("\xE0\x80\x80", 3);
    const std::string overlong_four("\xF0\x80\x80\x80", 4);

    // Pre-Assert

    // Act
    bool empty_result =
        cplat_internal_regex_utf8_decode(NULL, 0U, units, offsets); // [手順] - NULL と長さ 0 の入力を変換する。
    bool null_nonempty_result =
        cplat_internal_regex_utf8_decode(NULL, 1U, units, offsets); // [手順] - NULL と長さ 1 の入力を変換する。
    bool invalid_trail_result =
        cplat_internal_regex_utf8_decode(invalid_trail.data(), invalid_trail.size(), units,
                                         offsets); // [手順] - 不正な後続バイトを含む入力を変換する。
    bool overlong_three_result =
        cplat_internal_regex_utf8_decode(overlong_three.data(), overlong_three.size(), units,
                                         offsets); // [手順] - 3 バイトのオーバー ロング表現を変換する。
    bool overlong_four_result =
        cplat_internal_regex_utf8_decode(overlong_four.data(), overlong_four.size(), units,
                                         offsets); // [手順] - 4 バイトのオーバー ロング表現を変換する。

    // Assert
    EXPECT_TRUE(
        empty_result); // [確認_正常系] - NULL の空入力に対する cplat_internal_regex_utf8_decode の戻り値が true であること。
    EXPECT_TRUE(units.empty()); // [確認_正常系] - NULL の空入力でコード単位列が空であること。
    EXPECT_FALSE(
        null_nonempty_result); // [確認_異常系] - NULL の非空入力に対する cplat_internal_regex_utf8_decode の戻り値が false であること。
    EXPECT_FALSE(
        invalid_trail_result); // [確認_異常系] - 不正な後続バイトに対する cplat_internal_regex_utf8_decode の戻り値が false であること。
    EXPECT_FALSE(overlong_three_result); // [確認_異常系] - 3 バイトのオーバー ロング表現が拒否されること。
    EXPECT_FALSE(overlong_four_result);  // [確認_異常系] - 4 バイトのオーバー ロング表現が拒否されること。
}

// UTF-16 コード単位の不正なサロゲート構成が符号化を拒否することの確認
TEST_F(regexUtf8Test, encode_rejects_incomplete_surrogate_pairs)
{
    // Arrange
    std::wstring high_only;
    std::wstring high_then_ascii;
    std::string encoded;
    high_only.push_back((wchar_t)0xD800);       // [状態] - 上位サロゲートだけを用意する。
    high_then_ascii.push_back((wchar_t)0xD800); // [状態] - 上位サロゲートの後へ ASCII を置く。
    high_then_ascii.push_back(L'a');

    // Pre-Assert

    // Act
    bool high_only_result =
        cplat_internal_regex_utf8_encode(high_only, encoded); // [手順] - 上位サロゲートだけを UTF-8 へ変換する。
    bool high_then_ascii_result = cplat_internal_regex_utf8_encode(
        high_then_ascii, encoded); // [手順] - 下位サロゲートでない単位に続く上位サロゲートを変換する。
    const std::wstring two_byte_units = L"\x00E9";
    bool two_byte_result =
        cplat_internal_regex_utf8_encode(two_byte_units, encoded); // [手順] - 2 バイト文字を UTF-8 へ変換する。

    // Assert
    EXPECT_FALSE(
        high_only_result); // [確認_異常系] - 上位サロゲート単独の cplat_internal_regex_utf8_encode が false であること。
    EXPECT_FALSE(
        high_then_ascii_result); // [確認_異常系] - 不完全なサロゲート ペアの cplat_internal_regex_utf8_encode が false であること。
    EXPECT_TRUE(
        two_byte_result); // [確認_正常系] - 2 バイト文字の cplat_internal_regex_utf8_encode が true であること。
}

// オフセット変換の空写像、終端超過、サロゲート内部を処理することの確認
TEST_F(regexUtf8Test, offset_helpers_handle_empty_and_out_of_range_indices)
{
    // Arrange
    const std::wstring units = L"a";
    const std::wstring two_units = L"ab";
    const std::vector<std::size_t> offsets = {0U, 1U};
    const std::vector<std::size_t> two_offsets = {0U, 1U, 2U};
    const std::vector<std::size_t> empty_offsets;

    // Pre-Assert

    // Act
    std::size_t empty_begin =
        cplat_internal_regex_offset_of_begin(units, empty_offsets, 0U); // [手順] - 空写像の開始オフセットを取得する。
    std::size_t out_begin =
        cplat_internal_regex_offset_of_begin(units, offsets, 99U); // [手順] - 範囲外の開始索引を変換する。
    std::size_t empty_end =
        cplat_internal_regex_offset_of_end(units, empty_offsets, 0U); // [手順] - 空写像の終了オフセットを取得する。
    std::size_t out_end =
        cplat_internal_regex_offset_of_end(units, offsets, 99U); // [手順] - 範囲外の終了索引を変換する。
    std::size_t first_end =
        cplat_internal_regex_offset_of_end(units, offsets, 0U); // [手順] - 先頭索引の終了オフセットを変換する。
    std::size_t regular_end =
        cplat_internal_regex_offset_of_end(two_units, two_offsets, 1U); // [手順] - 通常文字の終了オフセットを変換する。
    std::size_t index = 99U;
    bool empty_index_result =
        cplat_internal_regex_index_of_offset(empty_offsets, 0U, index); // [手順] - 空写像からオフセットを検索する。

    // Assert
    EXPECT_EQ(0U, empty_begin);       // [確認_正常系] - 空写像の開始オフセットが 0 であること。
    EXPECT_EQ(1U, out_begin);         // [確認_正常系] - 範囲外の開始オフセットが終端へ丸められること。
    EXPECT_EQ(0U, empty_end);         // [確認_正常系] - 空写像の終了オフセットが 0 であること。
    EXPECT_EQ(1U, out_end);           // [確認_正常系] - 範囲外の終了オフセットが終端へ丸められること。
    EXPECT_EQ(0U, first_end);         // [確認_正常系] - 先頭索引の終了オフセットが 0 であること。
    EXPECT_EQ(1U, regular_end);       // [確認_正常系] - 通常文字の終了オフセットが対応するオフセットであること。
    EXPECT_FALSE(empty_index_result); // [確認_異常系] - 空写像の検索が false になること。
}

// 有効な 2 バイト列と BMP 上位文字、終端索引を処理することの確認
TEST_F(regexUtf8Test, decode_encode_cover_two_byte_and_boundary_units)
{
    // Arrange
    const std::string two_byte = "\xC3\xA9";
    std::wstring decoded_units;
    std::vector<std::size_t> decoded_offsets;
    std::wstring private_use;
    std::string encoded;
    const std::string astral = u8"\U0001F600";
    std::wstring astral_units;
    std::vector<std::size_t> astral_offsets;
    private_use.push_back((wchar_t)0xE000); // [状態] - 下位サロゲート上限を超える BMP 文字 U+E000 を用意する。

    // Pre-Assert

    // Act
    bool two_byte_result =
        cplat_internal_regex_utf8_decode(two_byte.data(), two_byte.size(), decoded_units,
                                         decoded_offsets); // [手順] - 有効な 2 バイト文字 U+00E9 を変換する。
    bool private_use_result =
        cplat_internal_regex_utf8_encode(private_use, encoded); // [手順] - U+E000 を UTF-8 へ変換する。
    ASSERT_TRUE(cplat_internal_regex_utf8_decode(astral.data(), astral.size(), astral_units, astral_offsets));
    // [確認_正常系] - `cplat_internal_regex_utf8_decode(astral.data(), astral.size(), astral_units, astral_offsets)` が true であること。
    std::size_t end_index_offset = cplat_internal_regex_offset_of_end(
        astral_units, astral_offsets, astral_units.size()); // [手順] - コード単位数と等しい終了索引を変換する。

    // Assert
    EXPECT_TRUE(
        two_byte_result); // [確認_正常系] - 有効な 2 バイト文字に対する cplat_internal_regex_utf8_decode の戻り値が true であること。
    ASSERT_EQ((std::size_t)1, decoded_units.size()); // [確認_正常系] - 2 バイト文字のコード単位数が 1 であること。
    EXPECT_EQ((wchar_t)0x00E9, decoded_units[0]);    // [確認_正常系] - 変換結果が U+00E9 であること。
    EXPECT_TRUE(private_use_result); // [確認_正常系] - U+E000 の cplat_internal_regex_utf8_encode が true であること。
    EXPECT_EQ(
        astral_offsets.back(),
        end_index_offset); // [確認_正常系] - 終端索引の cplat_internal_regex_offset_of_end が写像表の末尾と一致すること。
}
