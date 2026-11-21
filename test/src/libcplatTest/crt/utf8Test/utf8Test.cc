#include <testfw.h>
#include <cplat/crt/utf8.h>

class utf8Test : public Test
{
};

// 2〜4 バイト文字の途中切り詰め、完結境界、不正な並び、空入力で適切な長さを返すことの確認
TEST_F(utf8Test, trimmed_length_handles_utf8_boundaries_and_invalid_sequences)
{
    // Arrange
    struct Utf8TrimCase
    {
        const char *name;
        const char *text;
        size_t len;
        size_t expected;
    };

    const Utf8TrimCase cases[] = {
        {"empty_zero_len", "", 0U, 0U},
        {"null_zero_len", nullptr, 0U, 0U},
        {"null_nonzero_len", nullptr, 3U, 0U},
        {"non_empty_zero_len", "abc", 0U, 0U},
        {"ascii_complete", "abc", 3U, 3U},
        {"two_byte_complete", "a\xC2\xA9", 3U, 3U},
        {"two_byte_cut_after_1", "a\xC2\xA9", 2U, 1U},
        {"three_byte_complete", "a\xE3\x81\x82", 4U, 4U},
        {"three_byte_cut_after_2", "a\xE3\x81\x82", 3U, 1U},
        {"three_byte_cut_after_1", "a\xE3\x81\x82", 2U, 1U},
        {"three_byte_at_start_cut_after_2", "\xE3\x81\x82", 2U, 0U},
        {"three_byte_at_start_cut_after_1", "\xE3\x81\x82", 1U, 0U},
        {"four_byte_complete", "a\xF0\x9F\x98\x80", 5U, 5U},
        {"four_byte_cut_after_3", "a\xF0\x9F\x98\x80", 4U, 1U},
        {"four_byte_cut_after_2", "a\xF0\x9F\x98\x80", 3U, 1U},
        {"four_byte_cut_after_1", "a\xF0\x9F\x98\x80", 2U, 1U},
        {"orphan_single_cont", "\x80", 1U, 1U},
        {"orphan_two_cont", "\x80\x80", 2U, 2U},
        {"orphan_five_cont", "\x80\x80\x80\x80\x80", 5U, 5U},
        {"ascii_then_orphan_cont", "a\x80\x80", 3U, 3U},
        {"two_byte_then_orphan_cont", "\xC2\xA9\x80", 3U, 3U},
        {"four_byte_then_orphan_cont", "\xF0\x9F\x98\x80\x80", 5U, 5U},
    }; // [状態] - 空入力、ASCII、2〜4 [byte] 文字の境界・途中切り詰め、孤立後続バイトのテスト ケース表を用意する。

    constexpr size_t case_count = sizeof(cases) / sizeof(cases[0]);
    size_t actual_lengths[case_count] = {};

    // Pre-Assert

    // Act
    for (size_t i = 0U; i < case_count; i++)
    {
        actual_lengths[i] = cplat_internal_utf8_trimmed_length(
            cases[i].text,
            cases[i]
                .len); // [手順] - 各テスト ケースの文字列と長さを指定して cplat_internal_utf8_trimmed_length() を呼び出す。
    }

    // Assert
    for (size_t i = 0U; i < case_count; i++)
    {
        SCOPED_TRACE(cases[i].name);
        EXPECT_EQ(
            cases[i].expected,
            actual_lengths
                [i]); // [確認_正常系 回数=22] - cplat_internal_utf8_trimmed_length() の戻り値が各ケースの期待長さと一致すること。
    }
}
