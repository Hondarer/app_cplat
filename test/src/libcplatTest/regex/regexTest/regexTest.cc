#include <testfw.h>

#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/regex/regex.h>

#include "regex.inject.h"

#include <cstring>
#include <string>
#include <vector>

class regexTest : public Test
{
  protected:
    /* text のうち match が示す範囲を取り出す。 */
    std::string slice(const std::string &text, const cplat_regex_match &match)
    {
        if ((match.begin == CPLAT_REGEX_NPOS) || (match.end == CPLAT_REGEX_NPOS))
        {
            return std::string();
        }
        return text.substr(match.begin, match.end - match.begin);
    }
};

// パターンをコンパイルして捕捉グループ数を取得できることの確認
TEST_F(regexTest, create_reports_group_count)
{
    // Arrange
    cplat_regex *regex = NULL;
    int result = CPLAT_ERR_UNKNOWN;

    // Pre-Assert

    // Act
    result = cplat_regex_create("(a)(b)(c)", CPLAT_REGEX_DEFAULT, &regex,
                                NULL); // [手順] - 捕捉グループを 3 個持つパターンをコンパイルする。

    // Assert
    ASSERT_EQ(CPLAT_OK, result);           // [確認_正常系] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_NE((cplat_regex *)NULL, regex); // [確認_正常系] - ハンドルが NULL でないこと。
    EXPECT_EQ((size_t)4,
              cplat_regex_get_group_count(
                  regex)); // [確認_正常系] - cplat_regex_get_group_count の戻り値が捕捉グループ数 + 1 の 4 であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 部分一致の検索でマッチ範囲がバイト オフセットとして返ることの確認
TEST_F(regexTest, search_returns_byte_offsets)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match matches[1];
    int matched = -1;
    int result = CPLAT_ERR_UNKNOWN;
    const std::string text = "xxabcyy";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a.c", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "a.c" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    result = cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 1, &matched,
                                NULL); // [手順] - "xxabcyy" の先頭から検索する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);            // [確認_正常系] - cplat_regex_search の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, matched);                  // [確認_正常系] - 一致したことを示す 1 が matched へ格納されること。
    EXPECT_EQ((size_t)2, matches[0].begin); // [確認_正常系] - マッチ開始位置が 2 であること。
    EXPECT_EQ((size_t)5, matches[0].end);   // [確認_正常系] - マッチ終了位置が 5 であること。
    EXPECT_EQ(std::string("abc"), slice(text, matches[0])); // [確認_正常系] - 切り出した部分文字列が "abc" であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 一致しない場合がエラーではなく matched = 0 で表されることの確認
TEST_F(regexTest, search_reports_no_match_as_success)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match matches[1];
    int matched = -1;
    int result = CPLAT_ERR_UNKNOWN;
    const std::string text = "xyz";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("abc", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "abc" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    result = cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 1, &matched,
                                NULL); // [手順] - 一致しない入力 "xyz" を検索する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_regex_search の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, matched);       // [確認_正常系] - 一致しなかったことを示す 0 が matched へ格納されること。
    EXPECT_EQ(CPLAT_REGEX_NPOS,
              matches[0].begin); // [確認_正常系] - マッチ開始位置に CPLAT_REGEX_NPOS が格納されること。
    EXPECT_EQ(CPLAT_REGEX_NPOS,
              matches[0].end); // [確認_正常系] - マッチ終了位置に CPLAT_REGEX_NPOS が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 全体一致の判定が部分一致と区別されることの確認
TEST_F(regexTest, matches_requires_whole_text)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched_partial = -1;
    int matched_whole = -1;
    const std::string text = "xxabcyy";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a.c", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "a.c" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0,
                                           &matched_partial,
                                           NULL)); // [手順] - "xxabcyy" を cplat_regex_search で照合する。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_partial, NULL)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_whole,
                                  NULL)); // [手順] - 同じ入力を cplat_regex_matches で照合する。
    // [確認_正常系] - `cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_whole, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched_partial); // [確認_正常系] - cplat_regex_search では一致を示す 1 が格納されること。
    EXPECT_EQ(0, matched_whole);   // [確認_正常系] - cplat_regex_matches では不一致を示す 0 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 日本語 (BMP) の 1 文字が 1 文字として扱われることの確認
TEST_F(regexTest, matches_treats_bmp_character_as_one_character)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched_three = -1;
    int matched_two = -1;
    const std::string three = u8"あいう";
    const std::string two = u8"あい";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("^.{3}$", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "^.{3}$" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_matches(regex, three.data(), three.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_three,
                                  NULL)); // [手順] - 日本語 3 文字 "あいう" を照合する。
    // [確認_正常系] - `cplat_regex_matches(regex, three.data(), three.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_three, NULL)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_matches(regex, two.data(), two.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_two,
                                  NULL)); // [手順] - 日本語 2 文字 "あい" を照合する。
    // [確認_正常系] - `cplat_regex_matches(regex, two.data(), two.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_two, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched_three); // [確認_正常系] - "あいう" の照合で一致を示す 1 が格納されること。
    EXPECT_EQ(0, matched_two);   // [確認_正常系] - "あい" の照合で不一致を示す 0 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// BMP 外の 1 文字が UTF-16 コード単位 2 個として扱われることの確認
TEST_F(regexTest, matches_treats_astral_character_as_two_characters)
{
    // Arrange
    cplat_regex *one_regex = NULL;
    cplat_regex *two_regex = NULL;
    cplat_regex_match matches[1];
    int matched_one = -1;
    int matched_two = -1;
    const std::string text = u8"\U0001F600";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("^.$", CPLAT_REGEX_DEFAULT, &one_regex,
                                           NULL)); // [状態] - パターン "^.$" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_create("^.{2}$", CPLAT_REGEX_DEFAULT, &two_regex,
                                           NULL)); // [状態] - パターン "^.{2}$" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_matches(one_regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_one,
                                  NULL)); // [手順] - "^.$" で U+1F600 を照合する。
    // [確認_正常系] - `cplat_regex_matches(one_regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_one, NULL)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_matches(two_regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, matches, 1,
                                            &matched_two,
                                            NULL)); // [手順] - "^.{2}$" で U+1F600 を照合する。
    // [確認_正常系] - `cplat_regex_matches(two_regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, matches, 1, &matched_two, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(0, matched_one);              // [確認_正常系] - "^.$" の照合で不一致を示す 0 が格納されること。
    EXPECT_EQ(1, matched_two);              // [確認_正常系] - "^.{2}$" の照合で一致を示す 1 が格納されること。
    EXPECT_EQ((size_t)0, matches[0].begin); // [確認_正常系] - マッチ開始位置が 0 であること。
    EXPECT_EQ((size_t)4,
              matches[0].end); // [確認_正常系] - マッチ終了位置が UTF-8 の 4 バイト境界であること。

    // Cleanup
    cplat_regex_dispose(one_regex);
    cplat_regex_dispose(two_regex);
}

// 捕捉グループの範囲が日本語を含む入力でも正しく返ることの確認
TEST_F(regexTest, search_stores_capture_groups)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match matches[3];
    int matched = -1;
    const std::string text = u8"名前=あきら";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("^(.+)=(.+)$", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "^(.+)=(.+)$" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert
    ASSERT_EQ((size_t)3,
              cplat_regex_get_group_count(
                  regex)); // [Pre-Assert確認_正常系] - cplat_regex_get_group_count の戻り値が 3 であること。

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 3, &matched,
                                 NULL)); // [手順] - "名前=あきら" を検索する。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 3, &matched, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched);                                       // [確認_正常系] - 一致を示す 1 が格納されること。
    EXPECT_EQ(text, slice(text, matches[0]));                    // [確認_正常系] - グループ 0 が入力全体であること。
    EXPECT_EQ(std::string(u8"名前"), slice(text, matches[1]));   // [確認_正常系] - グループ 1 が "名前" であること。
    EXPECT_EQ(std::string(u8"あきら"), slice(text, matches[2])); // [確認_正常系] - グループ 2 が "あきら" であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 不参加の捕捉グループが CPLAT_REGEX_NPOS になることの確認
TEST_F(regexTest, search_marks_unmatched_group_as_npos)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match matches[3];
    int matched = -1;
    const std::string text = "b";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("^(a)?(b)$", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "^(a)?(b)$" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 3, &matched,
                                 NULL)); // [手順] - グループ 1 が一致しない入力 "b" を検索する。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 3, &matched, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched); // [確認_正常系] - 一致を示す 1 が格納されること。
    EXPECT_EQ(CPLAT_REGEX_NPOS,
              matches[1].begin); // [確認_正常系] - 不参加グループの開始位置が CPLAT_REGEX_NPOS であること。
    EXPECT_EQ(CPLAT_REGEX_NPOS,
              matches[1].end); // [確認_正常系] - 不参加グループの終了位置が CPLAT_REGEX_NPOS であること。
    EXPECT_EQ(std::string("b"), slice(text, matches[2])); // [確認_正常系] - グループ 2 が "b" であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// matches_capacity が不足する場合に先頭から切り捨てられることの確認
TEST_F(regexTest, search_truncates_groups_to_capacity)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match matches[2];
    int matched = -1;
    const std::string text = "abc";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("(a)(b)(c)", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 捕捉グループを 3 個持つパターンをコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert
    ASSERT_EQ((size_t)4,
              cplat_regex_get_group_count(
                  regex)); // [Pre-Assert確認_正常系] - cplat_regex_get_group_count の戻り値が 4 であること。

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 2, &matched,
                                 NULL)); // [手順] - 要素数 2 の配列を渡して検索する。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, matches, 2, &matched, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched);                                  // [確認_正常系] - 一致を示す 1 が格納されること。
    EXPECT_EQ(std::string("abc"), slice(text, matches[0])); // [確認_正常系] - グループ 0 が格納されること。
    EXPECT_EQ(std::string("a"), slice(text, matches[1]));   // [確認_正常系] - グループ 1 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// CPLAT_REGEX_NOSUB 指定時に捕捉グループが記録されないことの確認
TEST_F(regexTest, create_with_nosub_reports_single_group)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched = -1;
    const std::string text = "abc";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("(a)(b)(c)", CPLAT_REGEX_NOSUB, &regex,
                                           NULL)); // [状態] - CPLAT_REGEX_NOSUB を指定してコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched,
                                  NULL)); // [手順] - "abc" を照合する。
    // [確認_正常系] - `cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched); // [確認_正常系] - 一致を示す 1 が格納されること。
    EXPECT_EQ(
        (size_t)1,
        cplat_regex_get_group_count(regex)); // [確認_正常系] - cplat_regex_get_group_count の戻り値が 1 であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// CPLAT_REGEX_ICASE の畳み込みが ASCII 範囲に限られることの確認
TEST_F(regexTest, icase_folds_ascii_only)
{
    // Arrange
    cplat_regex *ascii_regex = NULL;
    cplat_regex *latin_regex = NULL;
    int matched_ascii = -1;
    int matched_latin = -1;
    const std::string ascii_text = "XABCX";
    const std::string latin_text = u8"ä";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("abc", CPLAT_REGEX_ICASE, &ascii_regex,
                                           NULL)); // [状態] - ASCII のパターン "abc" を ICASE でコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_create(u8"Ä", CPLAT_REGEX_ICASE, &latin_regex,
                                           NULL)); // [状態] - 非 ASCII のパターン "Ä" を ICASE で
                                                   // コンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(ascii_regex, ascii_text.data(), ascii_text.size(), 0,
                                           CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_ascii,
                                           NULL)); // [手順] - パターン "abc" で "XABCX" を検索する。
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(latin_regex, latin_text.data(), latin_text.size(), 0,
                                           CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_latin,
                                           NULL)); // [手順] - パターン "Ä" で "ä" を検索する。
    // [確認_正常系] - `cplat_regex_search(ascii_regex, ascii_text.data(), ascii_text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_ascii, NULL)` の戻り値が `CPLAT_OK` であること。
    // [確認_正常系] - `cplat_regex_search(latin_regex, latin_text.data(), latin_text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_latin, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched_ascii); // [確認_正常系] - ASCII では大小が畳み込まれ、一致を示す 1 が格納されること。
    EXPECT_EQ(0, matched_latin); // [確認_正常系] - 非 ASCII では畳み込まれず、不一致を示す 0 が格納されること。

    // Cleanup
    cplat_regex_dispose(ascii_regex);
    cplat_regex_dispose(latin_regex);
}

// CPLAT_REGEX_EXTENDED で POSIX 拡張正規表現として解釈されることの確認
TEST_F(regexTest, create_with_extended_uses_posix_ere)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched = -1;
    const std::string text = "ababc";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("(ab)+c", CPLAT_REGEX_EXTENDED, &regex,
                                           NULL)); // [状態] - CPLAT_REGEX_EXTENDED を指定してコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched,
                                  NULL)); // [手順] - "ababc" を照合する。
    // [確認_正常系] - `cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched); // [確認_正常系] - 一致を示す 1 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 文字クラスが ASCII 定義で機能することの確認
TEST_F(regexTest, search_supports_ascii_character_classes)
{
    // Arrange
    cplat_regex *posix_regex = NULL;
    cplat_regex *escape_regex = NULL;
    cplat_regex_match posix_match[1];
    cplat_regex_match escape_match[1];
    int matched_posix = -1;
    int matched_escape = -1;
    const std::string text = u8"あ123い";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("[[:digit:]]+", CPLAT_REGEX_DEFAULT, &posix_regex,
                                           NULL)); // [状態] - パターン "[[:digit:]]+" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_create("\\d+", CPLAT_REGEX_DEFAULT, &escape_regex,
                                           NULL)); // [状態] - パターン "\\d+" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(posix_regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT,
                                           posix_match, 1, &matched_posix,
                                           NULL)); // [手順] - "[[:digit:]]+" で "あ123い" を検索する。
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(escape_regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT,
                                           escape_match, 1, &matched_escape,
                                           NULL)); // [手順] - "\\d+" で "あ123い" を検索する。
    // [確認_正常系] - `cplat_regex_search(posix_regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, posix_match, 1, &matched_posix, NULL)` の戻り値が `CPLAT_OK` であること。
    // [確認_正常系] - `cplat_regex_search(escape_regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, escape_match, 1, &matched_escape, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched_posix); // [確認_正常系] - "[[:digit:]]+" の検索で一致を示す 1 が格納されること。
    EXPECT_EQ(std::string("123"),
              slice(text, posix_match[0])); // [確認_正常系] - "[[:digit:]]+" のマッチ範囲が "123" であること。
    EXPECT_EQ(1, matched_escape);           // [確認_正常系] - "\\d+" の検索で一致を示す 1 が格納されること。
    EXPECT_EQ(std::string("123"),
              slice(text, escape_match[0])); // [確認_正常系] - "\\d+" のマッチ範囲が "123" であること。

    // Cleanup
    cplat_regex_dispose(posix_regex);
    cplat_regex_dispose(escape_regex);
}

// 途中位置からの検索で行頭アンカーが誤って一致しないことの確認
TEST_F(regexTest, search_from_offset_does_not_treat_offset_as_line_start)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched_from_head = -1;
    int matched_from_offset = -1;
    const std::string text = "ab";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("^b", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "^b" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0,
                                           &matched_from_head,
                                           NULL)); // [手順] - start_offset に 0 を指定して検索する。
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(regex, text.data(), text.size(), 1, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0,
                                           &matched_from_offset,
                                           NULL)); // [手順] - start_offset に 1 を指定して検索する。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_from_head, NULL)` の戻り値が `CPLAT_OK` であること。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 1, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_from_offset, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(0, matched_from_head); // [確認_正常系] - start_offset が 0 の検索で不一致を示す 0 が格納されること。
    EXPECT_EQ(0,
              matched_from_offset); // [確認_正常系] - start_offset が 1 の検索でも不一致を示す 0 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// CPLAT_REGEX_MATCH_ANCHORED が照合開始位置を固定することの確認
TEST_F(regexTest, search_with_anchored_matches_only_at_start_offset)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched_default = -1;
    int matched_anchored = -1;
    const std::string text = "xxab";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("ab", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "ab" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0,
                                           &matched_default,
                                           NULL)); // [手順] - CPLAT_REGEX_MATCH_DEFAULT を指定して検索する。
    ASSERT_EQ(CPLAT_OK, cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_ANCHORED, NULL, 0,
                                           &matched_anchored,
                                           NULL)); // [手順] - CPLAT_REGEX_MATCH_ANCHORED を指定して検索する。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched_default, NULL)` の戻り値が `CPLAT_OK` であること。
    // [確認_正常系] - `cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_ANCHORED, NULL, 0, &matched_anchored, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched_default);  // [確認_正常系] - 既定の検索で一致を示す 1 が格納されること。
    EXPECT_EQ(0, matched_anchored); // [確認_正常系] - 固定照合では不一致を示す 0 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

#if !defined(CPLAT_REGEX_NO_EXCEPTIONS)
// 不正なパターンが CPLAT_ERR_INVALID_PATTERN になることの確認
TEST_F(regexTest, create_rejects_invalid_pattern)
{
    // Arrange
    cplat_regex *regex = NULL;

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_PATTERN,
              cplat_regex_create("(", CPLAT_REGEX_DEFAULT, &regex,
                                 NULL)); // [確認_異常系] - パターン "(" に対する cplat_regex_create の
                                         // 戻り値が CPLAT_ERR_INVALID_PATTERN であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_PATTERN,
              cplat_regex_create("[a-", CPLAT_REGEX_DEFAULT, &regex,
                                 NULL)); // [確認_異常系] - パターン "[a-" に対する cplat_regex_create の
                                         // 戻り値が CPLAT_ERR_INVALID_PATTERN であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_PATTERN,
              cplat_regex_create("a{2,1}", CPLAT_REGEX_DEFAULT, &regex,
                                 NULL));   // [確認_異常系] - パターン "a{2,1}" に対する cplat_regex_create の
                                           // 戻り値が CPLAT_ERR_INVALID_PATTERN であること。
    EXPECT_EQ((cplat_regex *)NULL, regex); // [確認_異常系] - 失敗時にハンドルが NULL のままであること。
}
#endif /* !CPLAT_REGEX_NO_EXCEPTIONS */

// 不正なフラグの組み合わせが CPLAT_ERR_INVALID_ARGUMENT になることの確認
TEST_F(regexTest, create_rejects_invalid_flags)
{
    // Arrange
    cplat_regex *regex = NULL;

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_create("a", CPLAT_REGEX_EXTENDED | CPLAT_REGEX_BASIC, &regex,
                                 NULL)); // [確認_異常系] - EXTENDED と BASIC の同時指定に対する
                                         // cplat_regex_create の戻り値が CPLAT_ERR_INVALID_ARGUMENT
                                         // であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_create("a", 0x8000U, &regex,
                                 NULL)); // [確認_異常系] - 未定義ビット 0x8000 の指定に対する
                                         // cplat_regex_create の戻り値が CPLAT_ERR_INVALID_ARGUMENT
                                         // であること。
}

// 不正な UTF-8 が CPLAT_ERR_INVALID_ENCODING になることの確認
TEST_F(regexTest, rejects_invalid_utf8)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex *invalid_regex = NULL;
    int matched = -1;
    const std::string invalid_text("\xC0\x80", 2);

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "a" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, cplat_regex_create(invalid_text.c_str(), CPLAT_REGEX_DEFAULT, &invalid_regex,
                                                             NULL)); // [確認_異常系] - 不正な UTF-8 のパターンに対する
    // cplat_regex_create の戻り値が CPLAT_ERR_INVALID_ENCODING
    // であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING,
              cplat_regex_search(regex, invalid_text.data(), invalid_text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0,
                                 &matched,
                                 NULL)); // [確認_異常系] - 不正な UTF-8 の入力に対する cplat_regex_search の
                                         // 戻り値が CPLAT_ERR_INVALID_ENCODING であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// NULL 引数が CPLAT_ERR_INVALID_ARGUMENT になることの確認
TEST_F(regexTest, rejects_null_arguments)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched = -1;
    const std::string text = "a";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "a" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_create(NULL, CPLAT_REGEX_DEFAULT, &regex,
                                 NULL)); // [確認_異常系] - pattern が NULL のときの cplat_regex_create の
                                         // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_create("a", CPLAT_REGEX_DEFAULT, NULL,
                                 NULL)); // [確認_異常系] - regex_out が NULL のときの cplat_regex_create の
                                         // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_search(NULL, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched,
                                 NULL)); // [確認_異常系] - regex が NULL のときの cplat_regex_search の
                                         // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_search(regex, NULL, 0, 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched,
                                 NULL)); // [確認_異常系] - text が NULL のときの cplat_regex_search の
                                         // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_search(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, NULL,
                                 NULL)); // [確認_異常系] - matched_out が NULL のときの cplat_regex_search の
                                         // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_matches(NULL, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched,
                                  NULL)); // [確認_異常系] - regex が NULL のときの cplat_regex_matches の
                                          // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ((size_t)0,
              cplat_regex_get_group_count(
                  NULL)); // [確認_異常系] - regex が NULL のときの cplat_regex_get_group_count の戻り値が
                          // 0 であること。

    // Cleanup
    cplat_regex_dispose(regex);
    cplat_regex_dispose(NULL);
}

// 不正な start_offset が CPLAT_ERR_INVALID_ARGUMENT になることの確認
TEST_F(regexTest, search_rejects_invalid_start_offset)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched = -1;
    const std::string text = u8"あい";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "a" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_search(regex, text.data(), text.size(), text.size() + 1, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0,
                                 &matched,
                                 NULL)); // [確認_異常系] - 入力長を超える start_offset に対する
                                         // cplat_regex_search の戻り値が CPLAT_ERR_INVALID_ARGUMENT
                                         // であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_search(regex, text.data(), text.size(), 1, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched,
                                 NULL)); // [確認_異常系] - 文字の途中を指す start_offset 1 に対する
                                         // cplat_regex_search の戻り値が CPLAT_ERR_INVALID_ARGUMENT
                                         // であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 空の入力を照合できることの確認
TEST_F(regexTest, matches_accepts_empty_text)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched = -1;
    const std::string text;

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("^$", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "^$" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched,
                                  NULL)); // [手順] - 空の入力を照合する。
    // [確認_正常系] - `cplat_regex_matches(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &matched, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, matched); // [確認_正常系] - 一致を示す 1 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// detail_out が成功時にクリアされ、失敗時に直前値へ反映されることの確認
TEST_F(regexTest, reports_error_detail)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex *invalid_regex = NULL;
    cplat_error detail;
    cplat_error last;

    std::memset(&detail, 0xFF, sizeof(detail)); // [状態] - detail を未初期化に相当する値で埋めておく。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex,
                                           &detail)); // [手順] - 正しいパターン "a" をコンパイルする。
    // [確認_正常系] - `cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex, &detail)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - 成功時に detail_out がクリアされること。

#if !defined(CPLAT_REGEX_NO_EXCEPTIONS)
    // Arrange_2

    // Pre-Assert_2

    // Act_2
    ASSERT_EQ(CPLAT_ERR_INVALID_PATTERN, cplat_regex_create("(", CPLAT_REGEX_DEFAULT, &invalid_regex,
                                                            &detail)); // [手順] - 不正なパターン "(" をコンパイルする。
    // [確認_異常系] - `cplat_regex_create("(", CPLAT_REGEX_DEFAULT, &invalid_regex, &detail)` の戻り値が `CPLAT_ERR_INVALID_PATTERN` であること。

    // Assert_2
    EXPECT_EQ(0,
              cplat_error_is_set(&detail)); // [確認_正常系] - OS 由来ではない失敗のため detail_out がクリアされること。
    cplat_error_get_last(&last);
    EXPECT_EQ(0, cplat_error_is_set(&last)); // [確認_正常系] - 直前値もクリアされること。
#else
    (void)invalid_regex;
    (void)last;
#endif /* !CPLAT_REGEX_NO_EXCEPTIONS */

    // Cleanup
    cplat_regex_dispose(regex);
}

// 一致箇所を置換できることの確認
TEST_F(regexTest, replace_substitutes_all_matches)
{
    // Arrange
    cplat_regex *regex = NULL;
    char buffer[64];
    size_t required_size = 0;
    int result = CPLAT_ERR_UNKNOWN;
    const std::string text = "a1b22c";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("[0-9]+", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "[0-9]+" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    result = cplat_regex_replace(regex, text.data(), text.size(), "#", CPLAT_REGEX_MATCH_DEFAULT, buffer,
                                 sizeof(buffer), &required_size,
                                 NULL); // [手順] - "a1b22c" の数字列をすべて "#" へ置換する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_regex_replace の戻り値が CPLAT_OK であること。
    EXPECT_EQ(std::string("a#b#c"), std::string(buffer)); // [確認_正常系] - 置換結果が "a#b#c" であること。
    EXPECT_EQ((size_t)6,
              required_size); // [確認_正常系] - required_size_out に null 終端を含む 6 が格納されること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 置換文字列の後方参照が展開されることの確認
TEST_F(regexTest, replace_expands_back_references)
{
    // Arrange
    cplat_regex *regex = NULL;
    char buffer[64];
    const std::string text = "user@host";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("(\\w+)@(\\w+)", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "(\\w+)@(\\w+)" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_replace(regex, text.data(), text.size(), "$2:$1", CPLAT_REGEX_MATCH_DEFAULT, buffer,
                                            sizeof(buffer), NULL,
                                            NULL)); // [手順] - 置換文字列 "$2:$1" で置換する。
    // [確認_正常系] - `cplat_regex_replace(regex, text.data(), text.size(), "$2:$1", CPLAT_REGEX_MATCH_DEFAULT, buffer, sizeof(buffer), NULL, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(std::string("host:user"),
              std::string(buffer)); // [確認_正常系] - 置換結果が "host:user" であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 置換フラグが機能することの確認
TEST_F(regexTest, replace_honors_replace_flags)
{
    // Arrange
    cplat_regex *regex = NULL;
    char first_only[64];
    char no_copy[64];
    const std::string text = "a1b2c";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("[0-9]", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "[0-9]" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_replace(regex, text.data(), text.size(), "#", CPLAT_REGEX_REPLACE_FIRST_ONLY,
                                            first_only, sizeof(first_only), NULL,
                                            NULL)); // [手順] - CPLAT_REGEX_REPLACE_FIRST_ONLY を指定して置換する。
    ASSERT_EQ(CPLAT_OK, cplat_regex_replace(regex, text.data(), text.size(), "#", CPLAT_REGEX_REPLACE_NO_COPY, no_copy,
                                            sizeof(no_copy), NULL,
                                            NULL)); // [手順] - CPLAT_REGEX_REPLACE_NO_COPY を指定して置換する。
    // [確認_正常系] - `cplat_regex_replace(regex, text.data(), text.size(), "#", CPLAT_REGEX_REPLACE_FIRST_ONLY, first_only, sizeof(first_only), NULL, NULL)` の戻り値が `CPLAT_OK` であること。
    // [確認_正常系] - `cplat_regex_replace(regex, text.data(), text.size(), "#", CPLAT_REGEX_REPLACE_NO_COPY, no_copy, sizeof(no_copy), NULL, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(std::string("a#b2c"),
              std::string(first_only)); // [確認_正常系] - FIRST_ONLY では最初の 1 件のみ置換されること。
    EXPECT_EQ(std::string("##"),
              std::string(no_copy)); // [確認_正常系] - NO_COPY では非一致部分が出力されないこと。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 必要サイズの問い合わせとバッファー不足が扱えることの確認 (マルチ フェーズ テスト)
TEST_F(regexTest, replace_reports_required_size)
{
    // Arrange
    cplat_regex *regex = NULL;
    size_t required_size = 0;
    char buffer[8];
    int result = CPLAT_ERR_UNKNOWN;
    const std::string text = "aaa";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "a" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    result =
        cplat_regex_replace(regex, text.data(), text.size(), "xy", CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &required_size,
                            NULL); // [手順] - result_out に NULL を渡して必要サイズを問い合わせる。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 問い合わせ時の cplat_regex_replace の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)7,
              required_size); // [確認_正常系] - "xyxyxy" と null 終端を合わせた 7 が格納されること。

    // Arrange_2
    required_size = 0;

    // Pre-Assert_2

    // Act_2
    result =
        cplat_regex_replace(regex, text.data(), text.size(), "xy", CPLAT_REGEX_MATCH_DEFAULT, buffer, 6, &required_size,
                            NULL); // [手順] - 必要サイズより 1 バイト小さい 6 を指定して置換する。

    // Assert_2
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              result);                   // [確認_異常系] - バッファー不足時の cplat_regex_replace の戻り値が
                                         // CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ((size_t)7, required_size); // [確認_異常系] - required_size_out に必要な 7 が格納されること。

    // Arrange_3
    required_size = 0;

    // Pre-Assert_3

    // Act_3
    result =
        cplat_regex_replace(regex, text.data(), text.size(), "xy", CPLAT_REGEX_MATCH_DEFAULT, buffer, 7, &required_size,
                            NULL); // [手順] - ちょうどの 7 を指定して置換する。

    // Assert_3
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - ちょうどのサイズでの cplat_regex_replace の戻り値が CPLAT_OK
                       // であること。
    EXPECT_EQ(std::string("xyxyxy"), std::string(buffer)); // [確認_正常系] - 置換結果が "xyxyxy" であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 一致箇所を順に列挙できることの確認
TEST_F(regexTest, iter_enumerates_all_matches)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_iter *iter = NULL;
    cplat_regex_match match;
    int has_match = -1;
    std::vector<std::string> found;
    const std::string text = "a1bb22ccc333";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("[0-9]+", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "[0-9]+" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_iter_create(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, &iter,
                                                NULL)); // [状態] - 入力に対するイテレーターを生成しておく。
    // [状態確認] - cplat_regex_iter_create の戻り値が CPLAT_OK であること。

    // Pre-Assert
    ASSERT_NE((cplat_regex_iter *)NULL, iter); // [Pre-Assert確認_正常系] - イテレーターが NULL でないこと。

    // Act
    for (;;)
    {
        ASSERT_EQ(CPLAT_OK, cplat_regex_iter_next(iter, &match, 1, &has_match,
                                                  NULL)); // [手順] - 次の一致箇所を取得する。
        // [確認_正常系 回数=3+1] - `cplat_regex_iter_next(iter, &match, 1, &has_match, NULL)` の戻り値が `CPLAT_OK` であること。
        if (has_match == 0)
        {
            break;
        }
        found.push_back(slice(text, match));
    }

    // Assert
    ASSERT_EQ((size_t)3, found.size());      // [確認_正常系] - 列挙された件数が 3 であること。
    EXPECT_EQ(std::string("1"), found[0]);   // [確認_正常系] - 1 件目が "1" であること。
    EXPECT_EQ(std::string("22"), found[1]);  // [確認_正常系] - 2 件目が "22" であること。
    EXPECT_EQ(std::string("333"), found[2]); // [確認_正常系] - 3 件目が "333" であること。
    EXPECT_EQ(0, has_match);                 // [確認_正常系] - 列挙終了時に has_match_out へ 0 が格納されること。

    // Cleanup
    cplat_regex_iter_dispose(iter);
    cplat_regex_dispose(regex);
}

// 空文字列に一致するパターンでも列挙が終了することの確認
TEST_F(regexTest, iter_terminates_on_empty_matches)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_iter *iter = NULL;
    int has_match = -1;
    int count = 0;
    const std::string text = "ab";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a*", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 空一致しうるパターン "a*" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_iter_create(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, &iter,
                                                NULL)); // [状態] - 入力 "ab" のイテレーターを生成しておく。
    // [状態確認] - cplat_regex_iter_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    for (;;)
    {
        ASSERT_EQ(CPLAT_OK, cplat_regex_iter_next(iter, NULL, 0, &has_match,
                                                  NULL)); // [手順] - 次の一致箇所を取得する。
        // [確認_正常系 回数=3+1] - `cplat_regex_iter_next(iter, NULL, 0, &has_match, NULL)` の戻り値が `CPLAT_OK` であること。
        if (has_match == 0)
        {
            break;
        }
        count++;
        ASSERT_LT(count, 100); // 無限ループの検出
        // [確認_正常系 回数=3] - `count` が `100` より小さいこと。
    }

    // Assert
    EXPECT_LT(count, 100);   // [確認_正常系] - 空一致でも列挙が有限回で終了すること。
    EXPECT_EQ(0, has_match); // [確認_正常系] - 列挙終了時に has_match_out へ 0 が格納されること。

    // Cleanup
    cplat_regex_iter_dispose(iter);
    cplat_regex_dispose(regex);
}

// イテレーターが入力文字列を複製して保持することの確認
TEST_F(regexTest, iter_copies_input_text)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_iter *iter = NULL;
    cplat_regex_match match;
    int has_match = -1;
    std::string text = "abc";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("b", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "b" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_iter_create(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, &iter,
                                                NULL)); // [状態] - 入力 "abc" のイテレーターを生成しておく。
    // [状態確認] - cplat_regex_iter_create の戻り値が CPLAT_OK であること。
    text = "zzz"; // [状態] - イテレーター生成後に元の文字列を書き換える。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_regex_iter_next(iter, &match, 1, &has_match,
                                              NULL)); // [手順] - 次の一致箇所を取得する。
    // [確認_正常系] - `cplat_regex_iter_next(iter, &match, 1, &has_match, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1, has_match);           // [確認_正常系] - 一致を示す 1 が格納されること。
    EXPECT_EQ((size_t)1, match.begin); // [確認_正常系] - 元の "abc" に対するマッチ開始位置 1 が返ること。
    EXPECT_EQ((size_t)2, match.end);   // [確認_正常系] - 元の "abc" に対するマッチ終了位置 2 が返ること。

    // Cleanup
    cplat_regex_iter_dispose(iter);
    cplat_regex_dispose(regex);
}

// 入力を分割できることの確認
TEST_F(regexTest, split_divides_text_by_matches)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match parts[8];
    size_t part_count = 0;
    const std::string text = "a,,b,c";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create(",", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 区切りのパターン "," をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, parts, 8, &part_count,
                                NULL)); // [手順] - "a,,b,c" を上限なしで分割する。
    // [確認_正常系] - `cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, parts, 8, &part_count, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    ASSERT_EQ((size_t)4, part_count);                   // [確認_正常系] - 分割件数が 4 であること。
    EXPECT_EQ(std::string("a"), slice(text, parts[0])); // [確認_正常系] - 1 件目が "a" であること。
    EXPECT_EQ(std::string(""), slice(text, parts[1]));  // [確認_正常系] - 2 件目が空文字列であること。
    EXPECT_EQ(std::string("b"), slice(text, parts[2])); // [確認_正常系] - 3 件目が "b" であること。
    EXPECT_EQ(std::string("c"), slice(text, parts[3])); // [確認_正常系] - 4 件目が "c" であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 区切りが先頭と末尾にある場合に空の要素が生成されることの確認
TEST_F(regexTest, split_keeps_empty_parts_at_boundaries)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match parts[8];
    size_t part_count = 0;
    const std::string text = ",a,";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create(",", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 区切りのパターン "," をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, parts, 8, &part_count,
                                NULL)); // [手順] - ",a," を上限なしで分割する。
    // [確認_正常系] - `cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, parts, 8, &part_count, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    ASSERT_EQ((size_t)3, part_count);                   // [確認_正常系] - 分割件数が 3 であること。
    EXPECT_EQ(std::string(""), slice(text, parts[0]));  // [確認_正常系] - 1 件目が空文字列であること。
    EXPECT_EQ(std::string("a"), slice(text, parts[1])); // [確認_正常系] - 2 件目が "a" であること。
    EXPECT_EQ(std::string(""), slice(text, parts[2]));  // [確認_正常系] - 3 件目が空文字列であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 一致箇所が無い場合に入力全体が 1 件として返ることの確認
TEST_F(regexTest, split_returns_whole_text_when_no_match)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match parts[2];
    size_t part_count = 0;
    const std::string text = "abc";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create(",", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 区切りのパターン "," をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, parts, 2, &part_count,
                                NULL)); // [手順] - 区切りを含まない "abc" を分割する。
    // [確認_正常系] - `cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, parts, 2, &part_count, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    ASSERT_EQ((size_t)1, part_count);                     // [確認_正常系] - 分割件数が 1 であること。
    EXPECT_EQ(std::string("abc"), slice(text, parts[0])); // [確認_正常系] - 1 件目が入力全体であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// max_parts で分割数を打ち切れることの確認
TEST_F(regexTest, split_honors_max_parts)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match parts[8];
    size_t part_count = 0;
    const std::string text = "a,b,c,d";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create(",", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 区切りのパターン "," をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK,
              cplat_regex_split(regex, text.data(), text.size(), 2, CPLAT_REGEX_MATCH_DEFAULT, parts, 8, &part_count,
                                NULL)); // [手順] - max_parts に 2 を指定して分割する。
    // [確認_正常系] - `cplat_regex_split(regex, text.data(), text.size(), 2, CPLAT_REGEX_MATCH_DEFAULT, parts, 8, &part_count, NULL)` の戻り値が `CPLAT_OK` であること。

    // Assert
    ASSERT_EQ((size_t)2, part_count);                       // [確認_正常系] - 分割件数が 2 であること。
    EXPECT_EQ(std::string("a"), slice(text, parts[0]));     // [確認_正常系] - 1 件目が "a" であること。
    EXPECT_EQ(std::string("b,c,d"), slice(text, parts[1])); // [確認_正常系] - 2 件目が残り全体の "b,c,d" であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 分割結果の件数を問い合わせられることの確認 (マルチ フェーズ テスト)
TEST_F(regexTest, split_reports_required_count)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match parts[2];
    size_t part_count = 0;
    int result = CPLAT_ERR_UNKNOWN;
    const std::string text = "a,b,c";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create(",", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 区切りのパターン "," をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    result = cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, &part_count,
                               NULL); // [手順] - parts_out に NULL を渡して件数を問い合わせる。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 問い合わせ時の cplat_regex_split の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)3, part_count); // [確認_正常系] - 必要な件数 3 が格納されること。

    // Arrange_2
    part_count = 0;

    // Pre-Assert_2

    // Act_2
    result = cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, parts, 2, &part_count,
                               NULL); // [手順] - 必要件数より少ない要素数 2 の配列を渡して分割する。

    // Assert_2
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              result);                // [確認_異常系] - 件数不足時の cplat_regex_split の戻り値が
                                      // CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ((size_t)3, part_count); // [確認_異常系] - part_count_out に必要な件数 3 が格納されること。
    EXPECT_EQ(std::string("a"), slice(text, parts[0])); // [確認_異常系] - 格納できる範囲まで書き込まれていること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 置換・列挙・分割が NULL 引数を拒否することの確認
TEST_F(regexTest, replace_iter_split_reject_null_arguments)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_iter *iter = NULL;
    char buffer[8];
    size_t part_count = 0;
    int has_match = -1;
    const std::string text = "a";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - パターン "a" をコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_replace(NULL, text.data(), text.size(), "x", CPLAT_REGEX_MATCH_DEFAULT, buffer,
                                  sizeof(buffer), NULL,
                                  NULL)); // [確認_異常系] - regex が NULL のときの cplat_regex_replace の
                                          // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_replace(regex, text.data(), text.size(), NULL, CPLAT_REGEX_MATCH_DEFAULT, buffer,
                                  sizeof(buffer), NULL,
                                  NULL)); // [確認_異常系] - replacement が NULL のときの cplat_regex_replace
                                          // の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_iter_create(regex, text.data(), text.size(), CPLAT_REGEX_MATCH_DEFAULT, NULL,
                                      NULL)); // [確認_異常系] - iter_out が NULL のときの
                                              // cplat_regex_iter_create の戻り値が
                                              // CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_iter_next(NULL, NULL, 0, &has_match,
                                    NULL)); // [確認_異常系] - iter が NULL のときの cplat_regex_iter_next の
                                            // 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_regex_split(regex, text.data(), text.size(), 0, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0, NULL,
                                NULL)); // [確認_異常系] - part_count_out が NULL のときの cplat_regex_split
                                        // の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ((size_t)0, part_count);   // [確認_異常系] - part_count が変更されていないこと。

    // Cleanup
    cplat_regex_iter_dispose(iter);
    cplat_regex_dispose(regex);
}

// regex_traits が ASCII 文字クラス、大小変換、基数変換を実装することの確認
TEST_F(regexTest, traits_cover_ascii_classes_and_conversions)
{
    // Arrange
    const unsigned int alnum = test_regex_lookup_classname(L"ALNUM", false);
    const unsigned int alpha = test_regex_lookup_classname(L"alpha", false);
    const unsigned int blank = test_regex_lookup_classname(L"blank", false);
    const unsigned int cntrl = test_regex_lookup_classname(L"cntrl", false);
    const unsigned int digit = test_regex_lookup_classname(L"digit", false);
    const unsigned int digit_short = test_regex_lookup_classname(L"D", false);
    const unsigned int graph = test_regex_lookup_classname(L"graph", false);
    const unsigned int lower = test_regex_lookup_classname(L"lower", false);
    const unsigned int print = test_regex_lookup_classname(L"print", false);
    const unsigned int punct = test_regex_lookup_classname(L"punct", false);
    const unsigned int space = test_regex_lookup_classname(L"space", false);
    const unsigned int space_short = test_regex_lookup_classname(L"S", false);
    const unsigned int upper = test_regex_lookup_classname(L"upper", false);
    const unsigned int xdigit = test_regex_lookup_classname(L"xdigit", false);
    const unsigned int word = test_regex_lookup_classname(L"w", false);
    const unsigned int unknown = test_regex_lookup_classname(L"unknown", false);
    const unsigned int lower_icase = test_regex_lookup_classname(L"lower", true);
    const unsigned int upper_icase = test_regex_lookup_classname(L"upper", true);

    // Pre-Assert

    // Act
    bool alpha_result = test_regex_isctype(L'A', alpha);            // [手順] - 大文字を alpha クラスで判定する。
    bool lower_result = test_regex_isctype(L'a', lower);            // [手順] - 小文字を lower クラスで判定する。
    bool digit_result = test_regex_isctype(L'9', digit);            // [手順] - 数字を digit クラスで判定する。
    bool blank_result = test_regex_isctype(L'\t', blank);           // [手順] - タブを blank クラスで判定する。
    bool space_result = test_regex_isctype(L' ', space);            // [手順] - 空白を space クラスで判定する。
    bool control_result = test_regex_isctype(L'\x01', cntrl);       // [手順] - 制御文字を cntrl クラスで判定する。
    bool graph_result = test_regex_isctype(L'!', graph);            // [手順] - 記号を graph クラスで判定する。
    bool print_result = test_regex_isctype(L' ', print);            // [手順] - 空白を print クラスで判定する。
    bool punct_result = test_regex_isctype(L'!', punct);            // [手順] - 記号を punct クラスで判定する。
    bool xdigit_result = test_regex_isctype(L'F', xdigit);          // [手順] - 16 進数字を xdigit クラスで判定する。
    bool word_result = test_regex_isctype(L'_', word);              // [手順] - 下線を word クラスで判定する。
    bool non_ascii_result = test_regex_isctype(L'あ', alpha);       // [手順] - 非 ASCII 文字を alpha クラスで判定する。
    wchar_t upper_fold = test_regex_translate_nocase(L'Q');         // [手順] - 大文字を小文字へ変換する。
    wchar_t lower_fold = test_regex_translate_nocase(L'q');         // [手順] - 小文字をそのまま変換する。
    std::wstring transformed = test_regex_transform(L"Ab");         // [手順] - 文字列をそのまま変換する。
    std::wstring primary = test_regex_transform_primary(L"Ab");     // [手順] - 文字列を primary 変換する。
    std::wstring collatename = test_regex_lookup_collatename(L"x"); // [手順] - 未対応の照合要素を変換する。
    int digit_value = test_regex_value(L'7', 10);                   // [手順] - 10 進数字の値を取得する。
    int lower_hex_value = test_regex_value(L'f', 16);               // [手順] - 小文字の 16 進数字の値を取得する。
    int upper_hex_value = test_regex_value(L'F', 16);               // [手順] - 大文字の 16 進数字の値を取得する。
    int invalid_value = test_regex_value(L'z', 16);                 // [手順] - 不正な数字の値を取得する。
    int radix_value = test_regex_value(L'F', 10);                   // [手順] - 基数外の数字の値を取得する。
    std::string locale_name = test_regex_imbue(L"");                // [手順] - classic locale を traits へ設定する。
    bool classic_locale = test_regex_getloc_is_classic();           // [手順] - traits の locale を取得する。

    // Assert
    EXPECT_NE(0U, alnum);              // [確認_正常系] - alnum クラスが定義されること。
    EXPECT_NE(0U, alpha);              // [確認_正常系] - alpha クラスが定義されること。
    EXPECT_NE(0U, blank);              // [確認_正常系] - blank クラスが定義されること。
    EXPECT_NE(0U, cntrl);              // [確認_正常系] - cntrl クラスが定義されること。
    EXPECT_NE(0U, digit);              // [確認_正常系] - digit クラスが定義されること。
    EXPECT_EQ(digit, digit_short);     // [確認_正常系] - d 短縮名が digit と同じであること。
    EXPECT_NE(0U, graph);              // [確認_正常系] - graph クラスが定義されること。
    EXPECT_NE(0U, lower);              // [確認_正常系] - lower クラスが定義されること。
    EXPECT_NE(0U, print);              // [確認_正常系] - print クラスが定義されること。
    EXPECT_NE(0U, punct);              // [確認_正常系] - punct クラスが定義されること。
    EXPECT_NE(0U, space);              // [確認_正常系] - space クラスが定義されること。
    EXPECT_EQ(space, space_short);     // [確認_正常系] - s 短縮名が space と同じであること。
    EXPECT_NE(0U, upper);              // [確認_正常系] - upper クラスが定義されること。
    EXPECT_NE(0U, xdigit);             // [確認_正常系] - xdigit クラスが定義されること。
    EXPECT_NE(0U, word);               // [確認_正常系] - w 短縮名が word と同じであること。
    EXPECT_EQ(0U, unknown);            // [確認_異常系] - 未知のクラス名が 0 になること。
    EXPECT_EQ(alpha, lower_icase);     // [確認_正常系] - lower の icase が alpha になること。
    EXPECT_EQ(alpha, upper_icase);     // [確認_正常系] - upper の icase が alpha になること。
    EXPECT_TRUE(alpha_result);         // [確認_正常系] - 大文字が alpha と判定されること。
    EXPECT_TRUE(lower_result);         // [確認_正常系] - 小文字が lower と判定されること。
    EXPECT_TRUE(digit_result);         // [確認_正常系] - 数字が digit と判定されること。
    EXPECT_TRUE(blank_result);         // [確認_正常系] - タブが blank と判定されること。
    EXPECT_TRUE(space_result);         // [確認_正常系] - 空白が space と判定されること。
    EXPECT_TRUE(control_result);       // [確認_正常系] - 制御文字が cntrl と判定されること。
    EXPECT_TRUE(graph_result);         // [確認_正常系] - 記号が graph と判定されること。
    EXPECT_TRUE(print_result);         // [確認_正常系] - 空白が print と判定されること。
    EXPECT_TRUE(punct_result);         // [確認_正常系] - 記号が punct と判定されること。
    EXPECT_TRUE(xdigit_result);        // [確認_正常系] - F が xdigit と判定されること。
    EXPECT_TRUE(word_result);          // [確認_正常系] - 下線が word と判定されること。
    EXPECT_FALSE(non_ascii_result);    // [確認_異常系] - 非 ASCII 文字が alpha と判定されないこと。
    EXPECT_EQ(L'q', upper_fold);       // [確認_正常系] - Q が q へ変換されること。
    EXPECT_EQ(L'q', lower_fold);       // [確認_正常系] - q が q のままであること。
    EXPECT_EQ(L"Ab", transformed);     // [確認_正常系] - transform が入力を保持すること。
    EXPECT_EQ(L"ab", primary);         // [確認_正常系] - transform_primary が大小を変換すること。
    EXPECT_TRUE(collatename.empty());  // [確認_正常系] - 未対応照合要素が空になること。
    EXPECT_EQ(7, digit_value);         // [確認_正常系] - 7 の値が 7 であること。
    EXPECT_EQ(15, lower_hex_value);    // [確認_正常系] - f の値が 15 であること。
    EXPECT_EQ(15, upper_hex_value);    // [確認_正常系] - F の値が 15 であること。
    EXPECT_EQ(-1, invalid_value);      // [確認_異常系] - z の値が -1 であること。
    EXPECT_EQ(-1, radix_value);        // [確認_異常系] - 基数外の F の値が -1 であること。
    EXPECT_FALSE(locale_name.empty()); // [確認_正常系] - imbue の locale 名が空でないこと。
    EXPECT_TRUE(classic_locale);       // [確認_正常系] - getloc が classic locale を返すこと。
}

#if !defined(CPLAT_REGEX_NO_EXCEPTIONS)
// regex の例外変換が主要な std::regex 例外を共通結果へ分類することの確認
TEST_F(regexTest, translate_exception_maps_regex_and_runtime_errors)
{
    // Arrange

    // Pre-Assert

    // Act
    int complexity_result =
        test_regex_translate_error(std::regex_constants::error_complexity); // [手順] - complexity 例外を変換する。
    int space_result = test_regex_translate_error(std::regex_constants::error_space); // [手順] - space 例外を変換する。
    int pattern_result =
        test_regex_translate_error(std::regex_constants::error_paren); // [手順] - pattern 例外を変換する。
    int invalid_error_value = 999;
    int default_result = test_regex_translate_error(
        static_cast<std::regex_constants::error_type>(invalid_error_value)); // [手順] - 未分類の regex 例外を変換する。
    int alloc_result = test_regex_translate_bad_alloc();                     // [手順] - bad_alloc 例外を変換する。
    int unknown_result = test_regex_translate_unknown();                     // [手順] - 未知の例外を変換する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              complexity_result);                         // [確認_異常系] - complexity が LIMIT_EXCEEDED になること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, space_result);     // [確認_異常系] - space が OUT_OF_MEMORY になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_PATTERN, pattern_result); // [確認_異常系] - pattern が INVALID_PATTERN になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_PATTERN,
              default_result); // [確認_異常系] - 未分類 regex 例外が INVALID_PATTERN になること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, alloc_result); // [確認_異常系] - bad_alloc が OUT_OF_MEMORY になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unknown_result);     // [確認_異常系] - 未知の例外が UNKNOWN になること。
}
#endif /* !CPLAT_REGEX_NO_EXCEPTIONS */

// 正規表現の追加フラグが変換されて照合へ渡ることの確認
TEST_F(regexTest, option_and_match_flags_cover_basic_optimize_and_boundaries)
{
    // Arrange
    cplat_regex *regex = NULL;
    int matched = -1;
    const std::string text = "a";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_BASIC | CPLAT_REGEX_OPTIMIZE, &regex,
                                           NULL)); // [状態] - BASIC と OPTIMIZE を指定してコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_regex_search(regex, text.data(), text.size(), 0,
                                    CPLAT_REGEX_MATCH_NOTBOL | CPLAT_REGEX_MATCH_NOTEOL | CPLAT_REGEX_MATCH_NOTEMPTY,
                                    NULL, 0, &matched,
                                    NULL); // [手順] - 行境界、空一致、最適化に関する照合フラグを指定して検索する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);                 // [確認_正常系] - 追加フラグ付き検索の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, matched);                       // [確認_正常系] - 追加フラグ付き検索が一致を返すこと。
    EXPECT_EQ(L'a', test_regex_translate(L'a')); // [確認_正常系] - traits の translate が入力文字を保持すること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 置換、列挙、分割の境界引数が分類されることの確認
TEST_F(regexTest, replace_iter_split_reject_limits_and_invalid_encoding)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_iter *iter = NULL;
    cplat_regex_match parts[2];
    char buffer[8] = {};
    size_t part_count = 0U;
    int matched = -1;
    const std::string invalid("\xC0\x80", 2);
    const std::string large(CPLAT_REGEX_MAX_LENGTH + 1U, 'a');

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 境界テスト用のパターンをコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int replace_null_result = cplat_regex_replace(regex, "a", 1U, "x", CPLAT_REGEX_DEFAULT, NULL, 1U, NULL,
                                                  NULL); // [手順] - 出力先 NULL と非ゼロ容量を指定して置換する。
    int replace_flag_result = cplat_regex_replace(regex, "a", 1U, "x", 0x8000U, buffer, sizeof(buffer), NULL,
                                                  NULL); // [手順] - 未定義フラグを指定して置換する。
    int replace_text_limit_result =
        cplat_regex_replace(regex, large.data(), large.size(), "x", CPLAT_REGEX_DEFAULT, buffer, sizeof(buffer), NULL,
                            NULL); // [手順] - 最大長を超える入力を置換する。
    int replace_text_encoding_result = cplat_regex_replace(regex, invalid.data(), invalid.size(), "x",
                                                           CPLAT_REGEX_DEFAULT, buffer, sizeof(buffer), NULL,
                                                           NULL); // [手順] - 不正 UTF-8 の入力を置換する。
    int replace_replacement_encoding_result =
        cplat_regex_replace(regex, "a", 1U, invalid.data(), CPLAT_REGEX_DEFAULT, buffer, sizeof(buffer), NULL,
                            NULL); // [手順] - 不正 UTF-8 の置換文字列を指定する。
    int search_flag_result = cplat_regex_search(regex, "a", 1U, 0U, 0x8000U, NULL, 0U, &matched,
                                                NULL); // [手順] - 未定義照合フラグを指定して検索する。
    int search_limit_result =
        cplat_regex_search(regex, large.data(), large.size(), 0U, CPLAT_REGEX_DEFAULT, NULL, 0U, &matched,
                           NULL); // [手順] - 最大長を超える入力を検索する。
    int iter_flag_result = cplat_regex_iter_create(regex, "a", 1U, 0x8000U, &iter,
                                                   NULL); // [手順] - 未定義列挙フラグを指定する。
    int iter_limit_result = cplat_regex_iter_create(regex, large.data(), large.size(), CPLAT_REGEX_DEFAULT, &iter,
                                                    NULL); // [手順] - 最大長を超える入力で列挙する。
    int iter_encoding_result =
        cplat_regex_iter_create(regex, invalid.data(), invalid.size(), CPLAT_REGEX_MATCH_DEFAULT, &iter,
                                NULL); // [手順] - 不正 UTF-8 の入力で列挙する。
    int split_null_result = cplat_regex_split(regex, "a", 1U, 0U, CPLAT_REGEX_DEFAULT, NULL, 1U, &part_count,
                                              NULL); // [手順] - 分割出力 NULL と非ゼロ容量を指定する。
    int split_flag_result = cplat_regex_split(regex, "a", 1U, 0U, 0x8000U, parts, 2U, &part_count,
                                              NULL); // [手順] - 未定義分割フラグを指定する。
    int split_limit_result =
        cplat_regex_split(regex, large.data(), large.size(), 0U, CPLAT_REGEX_DEFAULT, parts, 2U, &part_count,
                          NULL); // [手順] - 最大長を超える入力を分割する。
    int split_encoding_result =
        cplat_regex_split(regex, invalid.data(), invalid.size(), 0U, CPLAT_REGEX_DEFAULT, parts, 2U, &part_count,
                          NULL); // [手順] - 不正 UTF-8 の入力を分割する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              replace_null_result); // [確認_異常系] - 出力先 NULL の置換が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              replace_flag_result); // [確認_異常系] - 不正置換フラグが INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              replace_text_limit_result); // [確認_異常系] - 長過ぎる置換入力が LIMIT_EXCEEDED になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING,
              replace_text_encoding_result); // [確認_異常系] - 不正置換入力が INVALID_ENCODING になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING,
              replace_replacement_encoding_result); // [確認_異常系] - 不正置換文字列が INVALID_ENCODING になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              search_flag_result); // [確認_異常系] - 不正検索フラグが INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              search_limit_result); // [確認_異常系] - 長過ぎる検索入力が LIMIT_EXCEEDED になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              iter_flag_result); // [確認_異常系] - 不正列挙フラグが INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              iter_limit_result); // [確認_異常系] - 長過ぎる列挙入力が LIMIT_EXCEEDED になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING,
              iter_encoding_result); // [確認_異常系] - 不正列挙入力が INVALID_ENCODING になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              split_null_result); // [確認_異常系] - 分割出力 NULL が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              split_flag_result); // [確認_異常系] - 不正分割フラグが INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              split_limit_result); // [確認_異常系] - 長過ぎる分割入力が LIMIT_EXCEEDED になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING,
              split_encoding_result); // [確認_異常系] - 不正分割入力が INVALID_ENCODING になること。

    // Cleanup
    cplat_regex_iter_dispose(iter);
    cplat_regex_dispose(regex);
}

// イテレーターがサロゲート ペアを飛び越えて空一致を進めることの確認
TEST_F(regexTest, iter_advances_empty_match_over_astral_code_point)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_iter *iter = NULL;
    int has_match = -1;
    const std::string text = u8"\U0001F600";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a*", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 空一致するパターンをコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_regex_iter_create(regex, text.data(), text.size(), CPLAT_REGEX_DEFAULT, &iter,
                                                NULL)); // [状態] - BMP 外文字の列挙器を生成しておく。
    // [状態確認] - cplat_regex_iter_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int first_result = cplat_regex_iter_next(iter, NULL, 0U, &has_match,
                                             NULL); // [手順] - BMP 外文字上の空一致を取得する。
    int second_result = cplat_regex_iter_next(iter, NULL, 0U, &has_match,
                                              NULL); // [手順] - サロゲート ペアを越えた次の空一致を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, first_result);  // [確認_正常系] - 1 回目の列挙が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, second_result); // [確認_正常系] - 2 回目の列挙が CPLAT_OK であること。
    EXPECT_EQ(1, has_match);            // [確認_正常系] - 2 回目も空一致を示すこと。

    // Cleanup
    cplat_regex_iter_dispose(iter);
    cplat_regex_dispose(regex);
}

// 置換文字列の上限と sed 書式が処理されることの確認
TEST_F(regexTest, replace_checks_replacement_limit_and_sed_flag)
{
    // Arrange
    cplat_regex *regex = NULL;
    char buffer[32] = {};
    const std::string large_replacement(CPLAT_REGEX_MAX_LENGTH + 1U, 'x');

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("(a)", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 捕捉グループを持つパターンをコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int limit_result =
        cplat_regex_replace(regex, "a", 1U, large_replacement.data(), CPLAT_REGEX_DEFAULT, buffer, sizeof(buffer), NULL,
                            NULL); // [手順] - 最大長を超える置換文字列を指定する。
    int sed_result = cplat_regex_replace(regex, "a", 1U, "\\1", CPLAT_REGEX_REPLACE_SED, buffer, sizeof(buffer), NULL,
                                         NULL); // [手順] - sed 書式の後方参照を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              limit_result);         // [確認_異常系] - 長過ぎる置換文字列が LIMIT_EXCEEDED になること。
    EXPECT_EQ(CPLAT_OK, sed_result); // [確認_正常系] - sed 書式の置換が CPLAT_OK になること。
    EXPECT_STREQ("a", buffer);       // [確認_正常系] - sed 書式の後方参照結果が a であること。

    // Cleanup
    cplat_regex_dispose(regex);
}

// 空一致する区切りがサロゲート ペア内で無限ループしないことの確認
TEST_F(regexTest, split_advances_empty_match_over_astral_code_point)
{
    // Arrange
    cplat_regex *regex = NULL;
    cplat_regex_match parts[2];
    size_t part_count = 0U;
    const std::string text = u8"\U0001F600";

    ASSERT_EQ(CPLAT_OK, cplat_regex_create("a*", CPLAT_REGEX_DEFAULT, &regex,
                                           NULL)); // [状態] - 空一致する区切りパターンをコンパイルしておく。
                                                   // [状態確認] - cplat_regex_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_regex_split(regex, text.data(), text.size(), 0U, CPLAT_REGEX_DEFAULT, parts, 2U, &part_count,
                                   NULL); // [手順] - BMP 外文字を空一致パターンで分割する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - 空一致分割の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, part_count);   // [確認_正常系] - 空一致分割で入力全体の 1 件が返ること。
    EXPECT_EQ(text, std::string(text.data() + parts[0].begin,
                                parts[0].end - parts[0].begin)); // [確認_正常系] - 入力全体が保持されること。

    // Cleanup
    cplat_regex_dispose(regex);
}
