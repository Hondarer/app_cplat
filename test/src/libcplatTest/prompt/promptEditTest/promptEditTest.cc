#include <testfw.h>
#include <cplat/prompt/prompt.h>
#include <cplat/prompt/prompt_edit.h>
#include <mock_cplat.h>

#include <cstdlib>
#include <cstring>
#include <limits>
#include <tuple>
#include <vector>
#include <string>

class promptEditTest : public Test
{
};

/*
 * cplat_internal_prompt_edit_utf8_prev_boundary
 */

// 位置 0 からは移動しないことの確認
TEST_F(promptEditTest, prev_boundary_stays_at_zero)
{
    // Arrange
    const char text[] = "abc"; // [状態] - ASCII のみの文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(
        text, 0u); // [手順] - 位置 0 を指定して cplat_internal_prompt_edit_utf8_prev_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(0u, pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_prev_boundary() の戻り値が 0 であること。
}

// ASCII 文字を 1 つ戻ることの確認
TEST_F(promptEditTest, prev_boundary_moves_one_byte_for_ascii)
{
    // Arrange
    const char text[] = "abc"; // [状態] - ASCII のみの文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(
        text, 3u); // [手順] - 終端の位置 3 を指定して cplat_internal_prompt_edit_utf8_prev_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(2u, pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_prev_boundary() の戻り値が 2 であること。
}

// マルチバイト文字の先頭まで戻ることの確認
TEST_F(promptEditTest, prev_boundary_skips_continuation_bytes)
{
    // Arrange
    const char text[] = "a\xE3\x81\x82"; // [状態] - ASCII 1 文字と 3 バイトの日本語 1 文字を含む文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(
        text, 4u); // [手順] - 終端の位置 4 を指定して cplat_internal_prompt_edit_utf8_prev_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(
        1u,
        pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_prev_boundary() の戻り値が 1 であること。(継続バイトを読み飛ばして日本語文字の先頭へ戻ったことの確認)
}

// 先頭文字の直後から戻る場合に継続バイト判定を行わないことの確認
TEST_F(promptEditTest, prev_boundary_stops_after_moving_to_zero)
{
    // Arrange
    const char text[] = "a"; // [状態] - ASCII 1 文字の文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(
        text, 1u); // [手順] - 位置 1 を指定して cplat_internal_prompt_edit_utf8_prev_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(0u, pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_prev_boundary() の戻り値が 0 であること。
}

/*
 * cplat_internal_prompt_edit_utf8_next_boundary
 */

// 終端以降では長さを返すことの確認
TEST_F(promptEditTest, next_boundary_returns_len_at_end)
{
    // Arrange
    const char text[] = "abc"; // [状態] - 長さ 3 の文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_next_boundary(
        text, 3u, 3u); // [手順] - 終端の位置 3 を指定して cplat_internal_prompt_edit_utf8_next_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(3u, pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_next_boundary() の戻り値が 3 であること。
}

// ASCII 文字を 1 つ進むことの確認
TEST_F(promptEditTest, next_boundary_moves_one_byte_for_ascii)
{
    // Arrange
    const char text[] = "abc"; // [状態] - ASCII のみの文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_next_boundary(
        text, 3u, 0u); // [手順] - 先頭の位置 0 を指定して cplat_internal_prompt_edit_utf8_next_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(1u, pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_next_boundary() の戻り値が 1 であること。
}

// マルチバイト文字の次の境界まで進むことの確認
TEST_F(promptEditTest, next_boundary_skips_continuation_bytes)
{
    // Arrange
    const char text[] = "\xE3\x81\x82"
                        "a"; // [状態] - 3 バイトの日本語 1 文字と ASCII 1 文字を含む文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_next_boundary(
        text, 4u, 0u); // [手順] - 先頭の位置 0 を指定して cplat_internal_prompt_edit_utf8_next_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(
        3u,
        pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_next_boundary() の戻り値が 3 であること。(継続バイトを読み飛ばして次の文字の先頭へ進んだことの確認)
}

// 末尾直前から進む場合に継続バイト判定を行わないことの確認
TEST_F(promptEditTest, next_boundary_stops_after_moving_to_end)
{
    // Arrange
    const char text[] = "a"; // [状態] - ASCII 1 文字の文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_next_boundary(
        text, 1u, 0u); // [手順] - 位置 0 を指定して cplat_internal_prompt_edit_utf8_next_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(1u, pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_next_boundary() の戻り値が 1 であること。
}

/*
 * cplat_internal_prompt_edit_utf8_sanitize_boundary
 */

// 長さを超える位置が長さへ丸められることの確認
TEST_F(promptEditTest, sanitize_boundary_clamps_to_len)
{
    // Arrange
    const char text[] = "abc"; // [状態] - 長さ 3 の文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_sanitize_boundary(
        text, 3u,
        10u); // [手順] - 長さを超える位置 10 を指定して cplat_internal_prompt_edit_utf8_sanitize_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(
        3u,
        pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_sanitize_boundary() の戻り値が 3 であること。(長さへ丸められたことの確認)
}

// 文字の途中を指す位置が先頭へ戻されることの確認
TEST_F(promptEditTest, sanitize_boundary_moves_back_to_character_head)
{
    // Arrange
    const char text[] = "\xE3\x81\x82"
                        "a"; // [状態] - 3 バイトの日本語 1 文字と ASCII 1 文字を含む文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_sanitize_boundary(
        text, 4u,
        2u); // [手順] - 日本語文字の途中を指す位置 2 を指定して cplat_internal_prompt_edit_utf8_sanitize_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(
        0u,
        pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_sanitize_boundary() の戻り値が 0 であること。(文字の先頭へ戻されたことの確認)
}

// 境界上の位置が変化しないことの確認
TEST_F(promptEditTest, sanitize_boundary_keeps_valid_position)
{
    // Arrange
    const char text[] = "\xE3\x81\x82"
                        "a"; // [状態] - 3 バイトの日本語 1 文字と ASCII 1 文字を含む文字列を用意する。

    // Pre-Assert

    // Act
    size_t pos = cplat_internal_prompt_edit_utf8_sanitize_boundary(
        text, 4u,
        3u); // [手順] - 境界上の位置 3 を指定して cplat_internal_prompt_edit_utf8_sanitize_boundary() を呼び出す。

    // Assert
    EXPECT_EQ(
        3u,
        pos); // [確認_正常系] - cplat_internal_prompt_edit_utf8_sanitize_boundary() の戻り値が 3 であること。(境界上の位置が変化しないことの確認)
}

/*
 * cplat_internal_prompt_edit_ensure_capacity
 */

// 既存容量で足りる場合に再確保しないことの確認
TEST_F(promptEditTest, ensure_capacity_keeps_buffer_when_enough)
{
    // Arrange
    char *buf = static_cast<char *>(std::malloc(16u));
    size_t cap = 16u; // [状態] - 16 byte を確保済みのバッファーを用意する。

    ASSERT_NE(nullptr, buf); // [状態確認] - std::malloc() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_ensure_capacity(
        &buf, &cap, 64u, 8u); // [手順] - 必要量 8 を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_internal_prompt_edit_ensure_capacity() の戻り値が 0 であること。
    EXPECT_EQ(16u, cap);      // [確認_正常系] - cap が 16 であること。(容量が変化しないことの確認)

    // Cleanup
    std::free(buf);
}

// 容量が 2 倍ずつ拡張されることの確認
TEST_F(promptEditTest, ensure_capacity_grows_by_doubling)
{
    // Arrange
    char *buf = static_cast<char *>(std::malloc(4u));
    size_t cap = 4u; // [状態] - 4 byte を確保済みのバッファーを用意する。

    ASSERT_NE(nullptr, buf); // [状態確認] - std::malloc() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_ensure_capacity(
        &buf, &cap, 64u,
        17u); // [手順] - 必要量 17 を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_internal_prompt_edit_ensure_capacity() の戻り値が 0 であること。
    EXPECT_EQ(32u, cap);      // [確認_正常系] - cap が 32 であること。(4 から 2 倍ずつ拡張されたことの確認)

    // Cleanup
    std::free(buf);
}

// 上限で頭打ちになることの確認
TEST_F(promptEditTest, ensure_capacity_caps_at_max_bytes)
{
    // Arrange
    char *buf = static_cast<char *>(std::malloc(4u));
    size_t cap = 4u; // [状態] - 4 byte を確保済みのバッファーを用意する。

    ASSERT_NE(nullptr, buf); // [状態確認] - std::malloc() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_ensure_capacity(
        &buf, &cap, 20u,
        20u); // [手順] - 上限 20、必要量 20 を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_internal_prompt_edit_ensure_capacity() の戻り値が 0 であること。
    EXPECT_EQ(20u,
              cap); // [確認_正常系] - cap が 20 であること。(2 倍では上限を超えるため上限で頭打ちになったことの確認)

    // Cleanup
    std::free(buf);
}

// 必要量が上限を超える場合に拒否されることの確認
TEST_F(promptEditTest, ensure_capacity_rejects_required_over_max)
{
    // Arrange
    char *buf = static_cast<char *>(std::malloc(4u));
    size_t cap = 4u; // [状態] - 4 byte を確保済みのバッファーを用意する。

    ASSERT_NE(nullptr, buf); // [状態確認] - std::malloc() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_ensure_capacity(
        &buf, &cap, 16u,
        17u); // [手順] - 上限 16 を超える必要量 17 を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。

    // Assert
    EXPECT_EQ(-1,
              actual_ret); // [確認_異常系] - cplat_internal_prompt_edit_ensure_capacity() の戻り値が -1 であること。
    EXPECT_EQ(4u, cap);    // [確認_異常系] - cap が 4 であること。(容量が変化しないことの確認)

    // Cleanup
    std::free(buf);
}

// buf と cap に NULL を渡した場合に拒否されることの確認
TEST_F(promptEditTest, ensure_capacity_rejects_null_arguments)
{
    // Arrange
    char *buf = nullptr;
    size_t cap = 0u;

    // Pre-Assert

    // Act
    int actual_ret_null_buf = cplat_internal_prompt_edit_ensure_capacity(
        nullptr, &cap, 16u,
        8u); // [手順] - buf に nullptr を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。
    int actual_ret_null_cap = cplat_internal_prompt_edit_ensure_capacity(
        &buf, nullptr, 16u,
        8u); // [手順] - cap に nullptr を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret_null_buf); // [確認_異常系] - buf が nullptr のときの cplat_internal_prompt_edit_ensure_capacity() の戻り値が -1 であること。
    EXPECT_EQ(
        -1,
        actual_ret_null_cap); // [確認_異常系] - cap が nullptr のときの cplat_internal_prompt_edit_ensure_capacity() の戻り値が -1 であること。
}

/*
 * cplat_internal_prompt_edit_resolve_options
 */

// 0 を指定した項目に既定値が入ることの確認
TEST_F(promptEditTest, resolve_options_applies_defaults_for_zero)
{
    // Arrange
    size_t history_max = 0u;
    size_t initial_capacity = 0u;
    size_t max_bytes = 0u; // [状態] - 解決結果の格納先を用意する。

    // Pre-Assert

    // Act
    cplat_internal_prompt_edit_resolve_options(
        0u, 0u, 0u, 128u, &history_max, &initial_capacity,
        &max_bytes); // [手順] - 要求値をすべて 0、既定初期容量 128 を指定して cplat_internal_prompt_edit_resolve_options() を呼び出す。

    // Assert
    EXPECT_EQ((size_t)CPLAT_PROMPT_HISTORY_DEFAULT,
              history_max); // [確認_正常系] - history_max が CPLAT_PROMPT_HISTORY_DEFAULT であること。
    EXPECT_EQ((size_t)CPLAT_PROMPT_INPUT_BYTES_DEFAULT,
              max_bytes);              // [確認_正常系] - max_bytes が CPLAT_PROMPT_INPUT_BYTES_DEFAULT であること。
    EXPECT_EQ(128u, initial_capacity); // [確認_正常系] - initial_capacity が 128 であること。
}

// 下限未満の指定が 2 へ引き上げられることの確認
TEST_F(promptEditTest, resolve_options_raises_values_below_minimum)
{
    // Arrange
    size_t history_max = 0u;
    size_t initial_capacity = 0u;
    size_t max_bytes = 0u; // [状態] - 解決結果の格納先を用意する。

    // Pre-Assert

    // Act
    cplat_internal_prompt_edit_resolve_options(
        4u, 1u, 1u, 128u, &history_max, &initial_capacity,
        &max_bytes); // [手順] - 初期容量と入力上限に 1 を指定して cplat_internal_prompt_edit_resolve_options() を呼び出す。

    // Assert
    EXPECT_EQ(4u, history_max); // [確認_正常系] - history_max が 4 であること。
    EXPECT_EQ(2u, max_bytes);   // [確認_正常系] - max_bytes が 2 であること。(下限へ引き上げられたことの確認)
    EXPECT_EQ(2u,
              initial_capacity); // [確認_正常系] - initial_capacity が 2 であること。(下限へ引き上げられたことの確認)
}

// 初期容量が入力上限へ丸められることの確認
TEST_F(promptEditTest, resolve_options_clamps_initial_capacity_to_max_bytes)
{
    // Arrange
    size_t history_max = 0u;
    size_t initial_capacity = 0u;
    size_t max_bytes = 0u; // [状態] - 解決結果の格納先を用意する。

    // Pre-Assert

    // Act
    cplat_internal_prompt_edit_resolve_options(
        4u, 64u, 16u, 128u, &history_max, &initial_capacity,
        &max_bytes); // [手順] - 初期容量 64、入力上限 16 を指定して cplat_internal_prompt_edit_resolve_options() を呼び出す。

    // Assert
    EXPECT_EQ(16u, max_bytes); // [確認_正常系] - max_bytes が 16 であること。
    EXPECT_EQ(16u,
              initial_capacity); // [確認_正常系] - initial_capacity が 16 であること。(入力上限へ丸められたことの確認)
}

// 出力先に NULL を渡してもクラッシュしないことの確認
TEST_F(promptEditTest, resolve_options_accepts_null_outputs)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_internal_prompt_edit_resolve_options(
        4u, 8u, 16u, 128u, nullptr, nullptr,
        nullptr); // [手順] - 出力先をすべて nullptr にして cplat_internal_prompt_edit_resolve_options() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(出力先がすべて nullptr の解決処理がクラッシュしないことの確認)
}

// 再確保に失敗した場合に拒否されることの確認
TEST_F(promptEditTest, ensure_capacity_returns_minus1_when_realloc_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char *buf = static_cast<char *>(std::malloc(4u));
    size_t cap = 4u; // [状態] - 4 byte を確保済みのバッファーを用意する。

    ASSERT_NE(nullptr, buf); // [状態確認] - std::malloc() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(buf, 32u, 1u))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc() が buf、32、1 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_realloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_internal_prompt_edit_ensure_capacity(
        &buf, &cap, 64u,
        17u); // [手順] - 必要量 17 を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。

    // Assert
    EXPECT_EQ(-1,
              actual_ret); // [確認_異常系] - cplat_internal_prompt_edit_ensure_capacity() の戻り値が -1 であること。
    EXPECT_EQ(4u, cap);    // [確認_異常系] - cap が 4 であること。(容量が変化しないことの確認)

    // Cleanup
    std::free(buf);
}

// 容量の 2 倍計算がオーバーフローした場合に入力上限を使用することの確認
TEST_F(promptEditTest, ensure_capacity_caps_at_max_after_overflow)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char dummy = '\0';
    char *buf = &dummy;
    const size_t max_size = (std::numeric_limits<size_t>::max)();
    size_t cap = (max_size / 2u) + 1u; // [状態] - 2 倍すると size_t の上限を超える容量を指定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(buf, max_size, 1u))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc() が buf、max_size、1 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_realloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_internal_prompt_edit_ensure_capacity(
        &buf, &cap, max_size,
        cap +
            1u); // [手順] - 現在容量より 1 byte 大きい必要量を指定して cplat_internal_prompt_edit_ensure_capacity() を呼び出す。

    // Assert
    EXPECT_EQ(-1,
              actual_ret); // [確認_異常系] - cplat_internal_prompt_edit_ensure_capacity() の戻り値が -1 であること。
    EXPECT_EQ(
        (max_size / 2u) + 1u,
        cap); // [確認_異常系] - cap が (max_size / 2u) + 1u と一致すること。(再確保失敗後も容量が変化しないことの確認)
}

/*
 * cplat_internal_prompt_edit_validate_initial_text
 */

// NULL の初期値を、長さ 0 として受け入れることの確認
TEST_F(promptEditTest, validate_initial_text_accepts_null_as_empty)
{
    // Arrange
    size_t length = 99u; // [状態] - 長さの格納先に 0 以外を入れておく。

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_validate_initial_text(
        nullptr, 16u,
        &length); // [手順] - nullptr の初期値を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0u, length); // [確認_正常系] - length が 0 であること。
}

// ASCII と UTF-8 の多バイト文字を含む初期値を受け入れることの確認
TEST_F(promptEditTest, validate_initial_text_accepts_ascii_and_utf8)
{
    // Arrange
    const char text[] = "edit 1 \xE3\x81\x82"; // [状態] - ASCII 7 バイトと 3 バイトの日本語 1 文字を用意する。
    size_t length = 0u;

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_validate_initial_text(
        text, 16u,
        &length); // [手順] - 初期値を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(10u, length); // [確認_正常系] - length が 10 であること。(NUL を除くバイト数であることの確認)
}

// 改行、タブ、DEL を含む初期値を拒否することの確認
TEST_F(promptEditTest, validate_initial_text_rejects_control_characters)
{
    // Arrange
    size_t length = 0u;

    // Pre-Assert

    // Act
    int actual_ret_newline = cplat_internal_prompt_edit_validate_initial_text(
        "a\nb", 16u,
        &length); // [手順] - 改行を含む初期値を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。
    int actual_ret_tab = cplat_internal_prompt_edit_validate_initial_text(
        "a\tb", 16u,
        &length); // [手順] - タブを含む初期値を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。
    int actual_ret_delete = cplat_internal_prompt_edit_validate_initial_text(
        "a\x7F", 16u,
        &length); // [手順] - DEL を含む初期値を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_newline); // [確認_異常系] - 改行を含む初期値に対する cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_tab); // [確認_異常系] - タブを含む初期値に対する cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_delete); // [確認_異常系] - DEL を含む初期値に対する cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(0u, length);  // [確認_異常系] - length が 0 であること。
}

// 上限ちょうどの初期値を受け入れ、1 バイト超える初期値を拒否することの確認
TEST_F(promptEditTest, validate_initial_text_checks_max_bytes_boundary)
{
    // Arrange
    size_t length_fit = 0u;
    size_t length_over = 99u;

    // Pre-Assert

    // Act
    int actual_ret_fit = cplat_internal_prompt_edit_validate_initial_text(
        "abc", 4u,
        &length_fit); // [手順] - NUL を含めて 4 バイトの初期値と上限 4 を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。
    int actual_ret_over = cplat_internal_prompt_edit_validate_initial_text(
        "abcd", 4u,
        &length_over); // [手順] - NUL を含めて 5 バイトの初期値と上限 4 を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_fit); // [確認_正常系] - 上限ちょうどの初期値に対する cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(3u, length_fit); // [確認_正常系] - length_fit が 3 であること。
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret_over); // [確認_異常系] - 上限を超える初期値に対する cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(0u, length_over); // [確認_異常系] - length_over が 0 であること。
}

// 上限 0 では空文字列も受け入れないことの確認
TEST_F(promptEditTest, validate_initial_text_rejects_zero_max_bytes)
{
    // Arrange
    size_t length = 0u;

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_validate_initial_text(
        "", 0u,
        &length); // [手順] - 上限 0 と空文字列を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

// 長さの格納先が NULL の場合を拒否することの確認
TEST_F(promptEditTest, validate_initial_text_rejects_null_length_out)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_prompt_edit_validate_initial_text(
        "abc", 16u,
        nullptr); // [手順] - 長さの格納先に nullptr を指定して cplat_internal_prompt_edit_validate_initial_text() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 固定プロンプトの文字列長ヘルパーが NULL を 0 として扱うことの確認
TEST_F(promptEditTest, static_text_helpers_cover_empty_and_utf8_cases)
{
    // Arrange
    const char combining[] = "\xCC\x81";
    const char cjk[] = "\xE6\x97\xA5";
    const char hiragana[] = "\xE3\x81\x82";
    const char katakana[] = "\xE3\x82\xA2";
    const char compatibility_ideograph[] = "\xE3\x90\x80";
    const char emoji[] = "\xF0\x9F\x98\x80";
    const char sgr[] = "\x1B[31mX";
    const char invalid_sgr[] = "\x1B[X";

    // Pre-Assert

    // Act
    size_t null_len = cplat_internal_prompt_edit_display_width(
        nullptr); // [手順] - nullptr を指定して cplat_internal_prompt_edit_display_width() を呼び出す。
    size_t ascii_width = cplat_internal_prompt_edit_char_display_width(
        "A", 1U, 0U); // [手順] - ASCII 文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t combining_width = cplat_internal_prompt_edit_char_display_width(
        combining, sizeof(combining) - 1U,
        0U); // [手順] - 結合文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t cjk_width = cplat_internal_prompt_edit_char_display_width(
        cjk, sizeof(cjk) - 1U,
        0U); // [手順] - CJK 文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t hiragana_width = cplat_internal_prompt_edit_char_display_width(
        hiragana, sizeof(hiragana) - 1U,
        0U); // [手順] - ひらがなを指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t katakana_width = cplat_internal_prompt_edit_char_display_width(
        katakana, sizeof(katakana) - 1U,
        0U); // [手順] - カタカナを指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t compatibility_width = cplat_internal_prompt_edit_char_display_width(
        compatibility_ideograph, sizeof(compatibility_ideograph) - 1U,
        0U); // [手順] - CJK 拡張文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t emoji_width = cplat_internal_prompt_edit_char_display_width(
        emoji, sizeof(emoji) - 1U,
        0U); // [手順] - 4 バイト文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t invalid_width = cplat_internal_prompt_edit_char_display_width(
        "\xC2", 1U,
        0U); // [手順] - 不完全な UTF-8 を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t sgr_len = cplat_internal_prompt_edit_sgr_sequence_len(
        sgr, sizeof(sgr) - 1U,
        0U); // [手順] - ANSI SGR を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t invalid_sgr_len = cplat_internal_prompt_edit_sgr_sequence_len(
        invalid_sgr, sizeof(invalid_sgr) - 1U,
        0U); // [手順] - 不正な SGR を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。

    // Assert
    EXPECT_EQ(
        0U,
        null_len); // [確認_正常系] - nullptr を指定した cplat_internal_prompt_edit_display_width() の戻り値が 0 であること。
    EXPECT_EQ(
        1U,
        ascii_width); // [確認_正常系] - "A" を指定した cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        0U,
        combining_width); // [確認_正常系] - 結合文字を指定した cplat_internal_prompt_edit_char_display_width() の戻り値が 0 であること。
    EXPECT_EQ(
        2U,
        cjk_width); // [確認_正常系] - CJK 文字を指定した cplat_internal_prompt_edit_char_display_width() の戻り値が 2 であること。
    EXPECT_EQ(
        2U,
        hiragana_width); // [確認_正常系] - ひらがなを指定した cplat_internal_prompt_edit_char_display_width() の戻り値が 2 であること。
    EXPECT_EQ(
        2U,
        katakana_width); // [確認_正常系] - カタカナを指定した cplat_internal_prompt_edit_char_display_width() の戻り値が 2 であること。
    EXPECT_EQ(
        2U,
        compatibility_width); // [確認_正常系] - CJK 拡張文字を指定した cplat_internal_prompt_edit_char_display_width() の戻り値が 2 であること。
    EXPECT_EQ(2U, emoji_width); // [確認_正常系] - 絵文字の表示幅が 2 桁であること。
    EXPECT_EQ(
        1U,
        invalid_width); // [確認_正常系] - 不完全な UTF-8 を指定した cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        5U,
        sgr_len); // [確認_正常系] - ANSI SGR を指定した cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 5 であること。
    EXPECT_EQ(
        0U,
        invalid_sgr_len); // [確認_正常系] - 不正な ANSI シーケンスを指定した cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 0 であること。
}

// 固定プロンプトの表示幅計算が ANSI と全角文字を処理することの確認
TEST_F(promptEditTest, static_display_helpers_skip_ansi_and_limit_columns)
{
    // Arrange
    const char text[] = "\x1B[31m日A";
    const size_t text_len = sizeof(text) - 1U;

    // Pre-Assert

    // Act
    size_t visible_one_col = cplat_internal_prompt_edit_visible_bytes_from(
        text, text_len, 0U,
        1U); // [手順] - 表示列数 1 を指定して cplat_internal_prompt_edit_visible_bytes_from() を呼び出す。
    size_t visible_three_cols = cplat_internal_prompt_edit_visible_bytes_from(
        text, text_len, 0U,
        3U); // [手順] - 表示列数 3 を指定して cplat_internal_prompt_edit_visible_bytes_from() を呼び出す。
    size_t display_width = cplat_internal_prompt_edit_display_width_between(
        text, text_len, 0U,
        text_len); // [手順] - ANSI を含む全範囲を指定して cplat_internal_prompt_edit_display_width_between() を呼び出す。
    size_t clipped_width = cplat_internal_prompt_edit_display_width_between(
        text, text_len, 0U,
        2U); // [手順] - ANSI 途中までの範囲を指定して cplat_internal_prompt_edit_display_width_between() を呼び出す。

    // Assert
    EXPECT_EQ(
        5U,
        visible_one_col); // [確認_正常系] - 1 列を指定した cplat_internal_prompt_edit_visible_bytes_from() の戻り値が 5 であること。(SGR と全角文字が 1 列に収まることの確認)
    EXPECT_EQ(
        text_len,
        visible_three_cols); // [確認_正常系] - 3 列を指定した cplat_internal_prompt_edit_visible_bytes_from() の戻り値が text_len と一致すること。(3 列制限で全内容が収まることの確認)
    EXPECT_EQ(
        3U,
        display_width); // [確認_正常系] - 全範囲を指定した cplat_internal_prompt_edit_display_width_between() の戻り値が 3 であること。(全角文字と ASCII の幅合計が 3 であることの確認)
    EXPECT_EQ(
        2U,
        clipped_width); // [確認_正常系] - 2 バイトまでを指定した cplat_internal_prompt_edit_display_width_between() の戻り値が 2 であること。(ANSI 途中までの範囲が通常文字 2 列として計算されることの確認)
}

// 固定プロンプトの文字幅と ANSI 解析が境界値を処理することの確認
TEST_F(promptEditTest, static_text_helpers_cover_boundary_sequences)
{
    // Arrange
    const char two_byte[] = "\xC3\xA9";
    const char three_byte[] = "\xE2\x82\xAC";
    const char astral_wide[] = "\xF0\xB0\x80\x80";
    const char truncated_three[] = "\xE3\x81";
    const char truncated_four[] = "\xF0\x9F\x98";
    const char valid_sgr[] = "\x1B[1;31mX";
    const char invalid_sgr[] = "\x1B[1x";
    const char incomplete_sgr[] = "\x1B[1";

    // Pre-Assert

    // Act
    size_t at_end = cplat_internal_prompt_edit_char_display_width(
        "A", 1U,
        1U); // [手順] - 文字列末尾の位置を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t two_byte_width = cplat_internal_prompt_edit_char_display_width(
        two_byte, sizeof(two_byte) - 1U,
        0U); // [手順] - 2 バイト文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t three_byte_width = cplat_internal_prompt_edit_char_display_width(
        three_byte, sizeof(three_byte) - 1U,
        0U); // [手順] - CJK 以外の 3 バイト文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t astral_wide_width = cplat_internal_prompt_edit_char_display_width(
        astral_wide, sizeof(astral_wide) - 1U,
        0U); // [手順] - 2FFFF を超える 4 バイト文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t truncated_three_width = cplat_internal_prompt_edit_char_display_width(
        truncated_three, sizeof(truncated_three) - 1U,
        0U); // [手順] - 不完全な 3 バイト文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t truncated_four_width = cplat_internal_prompt_edit_char_display_width(
        truncated_four, sizeof(truncated_four) - 1U,
        0U); // [手順] - 不完全な 4 バイト文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t valid_sgr_len = cplat_internal_prompt_edit_sgr_sequence_len(
        valid_sgr, sizeof(valid_sgr) - 1U,
        0U); // [手順] - パラメーター付き SGR を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t invalid_sgr_len = cplat_internal_prompt_edit_sgr_sequence_len(
        invalid_sgr, sizeof(invalid_sgr) - 1U,
        0U); // [手順] - 不正終端の SGR を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t incomplete_sgr_len = cplat_internal_prompt_edit_sgr_sequence_len(
        incomplete_sgr, sizeof(incomplete_sgr) - 1U,
        0U); // [手順] - 未完了の SGR を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t zero_visible = cplat_internal_prompt_edit_visible_bytes_from(
        "A", 1U, 0U, 0U); // [手順] - 表示列数 0 を指定して cplat_internal_prompt_edit_visible_bytes_from() を呼び出す。
    size_t clipped_display = cplat_internal_prompt_edit_display_width_between(
        "ABC", 3U, 1U,
        8U); // [手順] - 終端を超える範囲を指定して cplat_internal_prompt_edit_display_width_between() を呼び出す。

    // Assert
    EXPECT_EQ(
        0U,
        at_end); // [確認_正常系] - 末尾位置の cplat_internal_prompt_edit_char_display_width() の戻り値が 0 であること。
    EXPECT_EQ(
        1U,
        two_byte_width); // [確認_正常系] - 2 バイト文字の cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        three_byte_width); // [確認_正常系] - CJK 以外の 3 バイト文字の cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        astral_wide_width); // [確認_正常系] - 2FFFF を超える 4 バイト文字の cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        truncated_three_width); // [確認_異常系] - 不完全な 3 バイト文字の cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        truncated_four_width); // [確認_異常系] - 不完全な 4 バイト文字の cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        7U,
        valid_sgr_len); // [確認_正常系] - パラメーター付き SGR の cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 7 であること。
    EXPECT_EQ(
        0U,
        invalid_sgr_len); // [確認_異常系] - 不正終端 SGR の cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 0 であること。
    EXPECT_EQ(
        0U,
        incomplete_sgr_len); // [確認_異常系] - 未完了 SGR の cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 0 であること。
    EXPECT_EQ(
        0U,
        zero_visible); // [確認_正常系] - 表示列数 0 の cplat_internal_prompt_edit_visible_bytes_from() の戻り値が 0 であること。
    EXPECT_EQ(
        2U,
        clipped_display); // [確認_正常系] - 終端を超える範囲の cplat_internal_prompt_edit_display_width_between() の戻り値が 2 であること。(範囲を終端で切った表示幅が 2 であることの確認)
}

// 候補の検証、重複、一意性、置換結果の上限を判定することの確認
TEST_F(promptEditTest, completion_candidates_validate_and_deduplicate)
{
    // Arrange
    cplat_prompt_completions candidates = {};
    cplat_internal_prompt_edit_completions_reset(&candidates, "go st", 5U, 12U);
    // Pre-Assert
    // Act
    int null_sink = cplat_prompt_completions_add(nullptr, 0U, "x");         // [手順] - NULL の受け取り先へ追加する。
    int null_text = cplat_prompt_completions_add(&candidates, 3U, nullptr); // [手順] - NULL の候補を追加する。
    int past_end = cplat_prompt_completions_add(&candidates, 6U, "x");      // [手順] - 行末より後を指定する。
    int control = cplat_prompt_completions_add(&candidates, 3U, "st\n");    // [手順] - 制御文字のある候補を追加する。
    int mismatch = cplat_prompt_completions_add(&candidates, 3U, "other");  // [手順] - 先頭不一致の候補を追加する。
    int short_text = cplat_prompt_completions_add(&candidates, 3U, "s");    // [手順] - 入力より短い候補を追加する。
    int too_long =
        cplat_prompt_completions_add(&candidates, 3U, "start1234"); // [手順] - 置換後に上限を超える候補を追加する。
    int whole_long = cplat_prompt_completions_add(&candidates, 0U,
                                                  "go start12345"); // [手順] - 候補自体が上限を超える候補を追加する。
    int accepted = cplat_prompt_completions_add(&candidates, 3U, "start ");  // [手順] - 正常な候補を追加する。
    int duplicate = cplat_prompt_completions_add(&candidates, 3U, "start "); // [手順] - 同じ候補を追加する。
    const char *suffix = cplat_internal_prompt_edit_completion_suffix(&candidates);
    size_t unique_count = candidates.count;
    int second = cplat_prompt_completions_add(&candidates, 3U, "stop "); // [手順] - 異なる候補を追加する。
    const char *ambiguous = cplat_internal_prompt_edit_completion_suffix(&candidates);
    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, null_sink);  // [確認_異常系] - NULL の受け取り先を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, null_text);  // [確認_異常系] - NULL の候補を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, past_end);   // [確認_異常系] - 行末より後を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, control);    // [確認_異常系] - 制御文字を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, mismatch);   // [確認_異常系] - 先頭不一致を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, short_text); // [確認_異常系] - 短い候補を拒否すること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, too_long);   // [確認_異常系] - 置換後の上限超過を拒否すること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, whole_long); // [確認_異常系] - 候補の上限超過を拒否すること。
    EXPECT_EQ(CPLAT_OK, accepted);                     // [確認_正常系] - 正常な候補を受け付けること。
    EXPECT_EQ(CPLAT_OK, duplicate);                    // [確認_正常系] - 重複候補も成功を返すこと。
    EXPECT_EQ(1U, unique_count);                       // [確認_正常系] - 重複候補を 1 件として扱うこと。
    EXPECT_STREQ("art ", suffix);                      // [確認_正常系] - 入力との差分を返すこと。
    EXPECT_EQ(CPLAT_OK, second);                       // [確認_正常系] - 異なる候補を受け付けること。
    EXPECT_EQ(nullptr, ambiguous);                     // [確認_正常系] - 複数候補では続きを返さないこと。
    // Cleanup
    cplat_internal_prompt_edit_completions_dispose(&candidates);
}

// UTF-8 境界、同じ長さの候補、置換開始位置が異なる候補を判定することの確認
TEST_F(promptEditTest, completion_boundaries_and_same_length)
{
    // Arrange
    cplat_prompt_completions candidates = {};
    cplat_internal_prompt_edit_completions_reset(&candidates, "\xE3\x81\x82", 3U, 16U);
    // Pre-Assert
    // Act
    int mid = cplat_prompt_completions_add(&candidates, 1U, "x");             // [手順] - UTF-8 文字の途中を指定する。
    int same = cplat_prompt_completions_add(&candidates, 0U, "\xE3\x81\x82"); // [手順] - 入力と同じ候補を追加する。
    const char *same_suffix = cplat_internal_prompt_edit_completion_suffix(&candidates);
    int different_start =
        cplat_prompt_completions_add(&candidates, 3U, "x"); // [手順] - 異なる開始位置の候補を追加する。
    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, mid); // [確認_異常系] - 文字途中を拒否すること。
    EXPECT_EQ(CPLAT_OK, same);                  // [確認_正常系] - 同じ候補を受け付けること。
    EXPECT_EQ(nullptr, same_suffix);            // [確認_正常系] - 同じ長さの候補は表示しないこと。
    EXPECT_EQ(CPLAT_OK, different_start);       // [確認_正常系] - 異なる開始位置を受け付けること。
    EXPECT_EQ(2U, candidates.count);            // [確認_正常系] - 異なる開始位置の候補は別件として扱うこと。
    // Cleanup
    cplat_internal_prompt_edit_completions_dispose(&candidates);
}

// プロンプトの表示幅と横スクロールが UTF-8 と SGR に従うことの確認
TEST_F(promptEditTest, display_width_and_view_start_are_shared)
{
    // Arrange
    const char text[] = "a\xE3\x81\x82"
                        "bc";
    // Pre-Assert
    // Act
    size_t width = cplat_internal_prompt_edit_display_width(
        "\033[31m\xE3\x81\x82> \033[0m"); // [手順] - 色付き日本語の幅を求める。
    size_t view =
        cplat_internal_prompt_edit_adjust_view_start(text, 6U, 6U, 0U, 2U); // [手順] - 行末を表示する範囲を求める。
    size_t left =
        cplat_internal_prompt_edit_adjust_view_start(text, 6U, 0U, view, 2U); // [手順] - 行頭へ戻る範囲を求める。
    // Assert
    EXPECT_EQ(4U, width); // [確認_正常系] - 全角を 2 桁、SGR を 0 桁と数えること。
    EXPECT_EQ(4U, view);  // [確認_正常系] - 全角文字の後から表示すること。
    EXPECT_EQ(0U, left);  // [確認_正常系] - 行頭が見えること。
}

// UTF-8 の末尾が完成した場合だけ補完可能と判定することの確認
TEST_F(promptEditTest, utf8_completion_waits_for_last_byte)
{
    // Arrange
    const char *texts[] = {"",
                           "a",
                           "\xC2",
                           "\xC2\xA2",
                           "\xE3",
                           "\xE3\x81",
                           "\xE3\x81\x82",
                           "\xF0",
                           "\xF0\x9F\x98",
                           "\xF0\x9F\x98\x80",
                           "\x80",
                           "\xFF"};
    const int expected[] = {1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0};
    int actual[12] = {};
    // Pre-Assert
    // Act
    for (size_t i = 0U; i < 12U; i++)
    {
        actual[i] = cplat_internal_prompt_edit_utf8_complete(texts[i], strlen(texts[i]));
        // [手順] - 各バイト列の末尾の完成を判定する。(12 回)
    }
    // Assert
    for (size_t i = 0U; i < 12U; i++)
    {
        EXPECT_EQ(expected[i], actual[i]);
        // [確認_正常系] - 判定結果が期待値と一致すること。(12 件)
    }
}

// 候補の確保失敗と確定時の拡張失敗を分類することの確認
TEST_F(promptEditTest, completion_allocation_and_acceptance)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_prompt_completions candidates = {};
    cplat_internal_prompt_edit_completions_reset(&candidates, "s", 1U, 16U);
    char *buffer = static_cast<char *>(cplat_malloc(2U));
    ASSERT_NE(nullptr, buffer); // [状態確認] - バッファーを確保できること。
    buffer[0] = 's';
    buffer[1] = '\0';
    size_t cap = 2U;
    size_t len = 1U;
    size_t cursor = 1U;
    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, 1U))
        .WillOnce(Return(nullptr))
        .WillOnce(Invoke(delegate_real_cplat_realloc))
        .WillOnce(Return(nullptr))
        .WillOnce(Invoke(delegate_real_cplat_realloc));
    // [Pre-Assert確認_正常系] - 候補と入力の容量を計 4 回拡張すること。
    // [Pre-Assert手順] - 候補の初回と入力の初回の拡張を失敗させる。
    // Act
    int failed = cplat_prompt_completions_add(&candidates, 0U, "start "); // [手順] - 候補の確保を失敗させる。
    int absent = cplat_internal_prompt_edit_completion_accept(&candidates, &buffer, &cap, 16U, &len, &cursor);
    int added = cplat_prompt_completions_add(&candidates, 0U, "start "); // [手順] - 候補の確保を再試行する。
    int accept_failed = cplat_internal_prompt_edit_completion_accept(&candidates, &buffer, &cap, 16U, &len, &cursor);
    int accepted = cplat_internal_prompt_edit_completion_accept(&candidates, &buffer, &cap, 16U, &len, &cursor);
    // [手順] - 候補なし、拡張失敗、正常の各状態で確定する。
    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, failed); // [確認_異常系] - メモリ不足を返すこと。
    EXPECT_EQ(0, absent);                       // [確認_正常系] - 候補なしでは確定しないこと。
    EXPECT_EQ(CPLAT_OK, added);                 // [確認_正常系] - 再試行で候補を保持すること。
    EXPECT_EQ(0, accept_failed);                // [確認_異常系] - 拡張に失敗した候補を確定しないこと。
    EXPECT_EQ(1, accepted);                     // [確認_正常系] - 再試行で確定すること。
    EXPECT_STREQ("start ", buffer);             // [確認_正常系] - 候補全体を入力へ入れること。
    EXPECT_EQ(6U, len);                         // [確認_正常系] - 入力長を更新すること。
    EXPECT_EQ(6U, cursor);                      // [確認_正常系] - カーソルを行末へ置くこと。
    // Cleanup
    cplat_internal_prompt_edit_completions_dispose(&candidates);
    cplat_free(buffer);
}

namespace
{
void count_completion(const char *, size_t, cplat_prompt_completions *, void *user_data)
{
    (*static_cast<int *>(user_data))++;
}
} // namespace

// 共通補完処理が登録、空入力、行末、UTF-8 の条件を確認することの確認
TEST_F(promptEditTest, completion_callback_requires_complete_nonempty_end)
{
    // Arrange
    cplat_prompt_completions candidates = {};
    int calls = 0;
    // Pre-Assert
    // Act
    cplat_internal_prompt_edit_complete(&candidates, nullptr, &calls, "s", 1U, 1U, 16U);
    cplat_internal_prompt_edit_complete(&candidates, count_completion, &calls, "", 0U, 0U, 16U);
    cplat_internal_prompt_edit_complete(&candidates, count_completion, &calls, "ab", 2U, 1U, 16U);
    cplat_internal_prompt_edit_complete(&candidates, count_completion, &calls, "\xE3", 1U, 1U, 16U);
    cplat_internal_prompt_edit_complete(&candidates, count_completion, &calls, "s", 1U, 1U, 16U);
    // [手順] - 未登録、空入力、行中、未完成 UTF-8、正常入力の 5 条件で候補を求める。
    // Assert
    EXPECT_EQ(1, calls); // [確認_正常系] - 正常入力だけで呼び出すこと。
}

// UTF-8 と ANSI の境界値を分類することの確認
TEST_F(promptEditTest, text_helpers_cover_range_and_ansi_boundaries)
{
    // Arrange
    const char combining_upper_next[] = "\xCD\xB0";
    const char cjk_upper_next[] = "\xEA\x80\x80";
    const char cjk_extension_upper_next[] = "\xE4\xB7\x80";
    const char supplementary_cjk[] = "\xF0\xA0\x80\x80";
    const char not_csi[] = "\x1BXX";
    const char parameter_lower[] = "\x1B[0m";
    const char intermediate_upper[] = "\x1B[/m";
    const char invalid_control[] = "\x1B[\x1F";
    const char invalid_final[] = "\x1B[@";

    // Pre-Assert

    // Act
    size_t combining_upper_next_width = cplat_internal_prompt_edit_char_display_width(
        combining_upper_next, sizeof(combining_upper_next) - 1U,
        0U); // [手順] - 結合文字範囲の直後にある文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t cjk_upper_next_width = cplat_internal_prompt_edit_char_display_width(
        cjk_upper_next, sizeof(cjk_upper_next) - 1U,
        0U); // [手順] - CJK 統合漢字範囲の直後にある文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t cjk_extension_upper_next_width = cplat_internal_prompt_edit_char_display_width(
        cjk_extension_upper_next, sizeof(cjk_extension_upper_next) - 1U,
        0U); // [手順] - CJK 拡張 A 範囲の直後にある文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t supplementary_cjk_width = cplat_internal_prompt_edit_char_display_width(
        supplementary_cjk, sizeof(supplementary_cjk) - 1U,
        0U); // [手順] - 補助漢字面の文字を指定して cplat_internal_prompt_edit_char_display_width() を呼び出す。
    size_t not_csi_len = cplat_internal_prompt_edit_sgr_sequence_len(
        not_csi, sizeof(not_csi) - 1U,
        0U); // [手順] - CSI ではない ESC 文字列を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t parameter_lower_len = cplat_internal_prompt_edit_sgr_sequence_len(
        parameter_lower, sizeof(parameter_lower) - 1U,
        0U); // [手順] - ANSI パラメーター範囲の下限を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t intermediate_upper_len = cplat_internal_prompt_edit_sgr_sequence_len(
        intermediate_upper, sizeof(intermediate_upper) - 1U,
        0U); // [手順] - ANSI 中間バイト範囲の上限を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t invalid_control_len = cplat_internal_prompt_edit_sgr_sequence_len(
        invalid_control, sizeof(invalid_control) - 1U,
        0U); // [手順] - ANSI 中間バイト範囲未満の値を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。
    size_t invalid_final_len = cplat_internal_prompt_edit_sgr_sequence_len(
        invalid_final, sizeof(invalid_final) - 1U,
        0U); // [手順] - ANSI パラメーター範囲を超える値を指定して cplat_internal_prompt_edit_sgr_sequence_len() を呼び出す。

    // Assert
    EXPECT_EQ(
        1U,
        combining_upper_next_width); // [確認_正常系] - 結合文字範囲の直後に対する cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        cjk_upper_next_width); // [確認_正常系] - CJK 統合漢字範囲の直後に対する cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        cjk_extension_upper_next_width); // [確認_正常系] - CJK 拡張 A 範囲の直後に対する cplat_internal_prompt_edit_char_display_width() の戻り値が 1 であること。
    EXPECT_EQ(
        2U,
        supplementary_cjk_width); // [確認_正常系] - 補助漢字面に対する cplat_internal_prompt_edit_char_display_width() の戻り値が 2 であること。
    EXPECT_EQ(
        0U,
        not_csi_len); // [確認_異常系] - CSI ではない ESC 文字列に対する cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 0 であること。
    EXPECT_EQ(
        4U,
        parameter_lower_len); // [確認_正常系] - ANSI パラメーター下限を含む SGR に対する cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 4 であること。
    EXPECT_EQ(
        4U,
        intermediate_upper_len); // [確認_正常系] - ANSI 中間バイト上限を含む SGR に対する cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 4 であること。
    EXPECT_EQ(
        0U,
        invalid_control_len); // [確認_異常系] - ANSI 中間バイト範囲未満の値に対する cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 0 であること。(範囲外の値が拒否されたことの確認)
    EXPECT_EQ(
        0U,
        invalid_final_len); // [確認_異常系] - ANSI パラメーター範囲を超える値に対する cplat_internal_prompt_edit_sgr_sequence_len() の戻り値が 0 であること。(範囲外の値が拒否されたことの確認)
}

// 内部状態の容量境界でも置換結果が上限を超えないことの確認
TEST_F(promptEditTest, completion_start_must_leave_room_for_terminator)
{
    // Arrange
    cplat_prompt_completions candidates = {};
    cplat_internal_prompt_edit_completions_reset(&candidates, "a", 1U, 1U);
    // Pre-Assert
    // Act
    int result = cplat_prompt_completions_add(&candidates, 1U, ""); // [手順] - NUL の余地がない開始位置を指定する。
    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, result); // [確認_異常系] - 上限内へ NUL を置けない候補を拒否すること。
    EXPECT_EQ(0U, candidates.count);               // [確認_異常系] - 拒否した候補を件数に含めないこと。
}

struct promptUtf16Case
{
    std::vector<unsigned int> units;
    std::string expected;
    size_t high_surrogate;
};

// U+FFFF など XML に記載できない期待値を、パラメーターの説明へ出力しません。
static void PrintTo(const promptUtf16Case &value, std::ostream *stream)
{
    *stream << testing::PrintToString(value.units);
}

class promptUtf16Test : public testing::TestWithParam<promptUtf16Case>
{
};

// UTF-16 を文字単位で UTF-8 に変換し、不正なペアの後続も保持することの確認
TEST_P(promptUtf16Test, decodes_units_without_losing_following_input)
{
    // Arrange
    cplat_internal_prompt_utf16_input input = {};
    std::string actual;
    // Pre-Assert
    // Act
    for (unsigned int unit : GetParam().units)
    {
        cplat_internal_prompt_edit_utf16_feed(&input, unit); // [手順] - コード単位を入力する。
        while (true)
        {
            while (input.offset < input.length)
                actual.push_back(static_cast<char>(input.bytes[input.offset++]));
            if (input.has_pending_unit == 0U)
                break;
            unit = input.pending_unit;
            input.has_pending_unit = 0U;
            cplat_internal_prompt_edit_utf16_feed(&input, unit); // [手順] - 不正なペアの後続を再処理する。
        }
    }
    // Assert
    EXPECT_EQ(GetParam().expected, actual); // [確認_正常系] - UTF-8 のバイト列が期待値と一致すること。
    EXPECT_EQ(GetParam().high_surrogate,
              input.high_surrogate); // [確認_正常系] - 未完了の上位サロゲートだけを保留すること。
}

INSTANTIATE_TEST_SUITE_P(
    encoding, promptUtf16Test,
    testing::Values(promptUtf16Case{std::vector<unsigned int>{0U}, std::string(1U, '\0'), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0x7FU}, std::string("\x7F"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0x80U}, std::string("\xC2\x80"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0x7FFU}, std::string("\xDF\xBF"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0x800U}, std::string("\xE0\xA0\x80"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0x30C8U}, std::string("\xE3\x83\x88"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xD7FFU}, std::string("\xED\x9F\xBF"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xE000U}, std::string("\xEE\x80\x80"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xFFFFU}, std::string("\xEF\xBF\xBF"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xD800U, 0xDC00U}, std::string("\xF0\x90\x80\x80"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xDBFFU, 0xDFFFU}, std::string("\xF4\x8F\xBF\xBF"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xD83DU, 0xDE00U}, std::string("\xF0\x9F\x98\x80"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xDC00U}, std::string("\xEF\xBF\xBD"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xDFFFU}, std::string("\xEF\xBF\xBD"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xD800U, 0x41U},
                                    std::string("\xEF\xBF\xBD"
                                                "A"),
                                    0U},
                    promptUtf16Case{std::vector<unsigned int>{0xD800U, 0xE000U},
                                    std::string("\xEF\xBF\xBD\xEE\x80\x80"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xD800U, 0xDBFFU, 0xDFFFU},
                                    std::string("\xEF\xBF\xBD\xF4\x8F\xBF\xBF"), 0U},
                    promptUtf16Case{std::vector<unsigned int>{0xD800U}, std::string(), 0xD800U}));

class promptEmojiWidthTest : public testing::TestWithParam<std::tuple<const char *, size_t>>
{
};
// 補助平面の絵文字を 2 桁、幅が狭い記号を 1 桁として扱うことの確認
TEST_P(promptEmojiWidthTest, counts_terminal_columns)
{
    // Arrange
    const char *text = std::get<0>(GetParam());
    // Pre-Assert
    // Act
    size_t actual = cplat_internal_prompt_edit_display_width(text); // [手順] - 表示幅を求める。
    // Assert
    EXPECT_EQ(std::get<1>(GetParam()), actual); // [確認_正常系] - 表示幅が期待値と一致すること。
}
INSTANTIATE_TEST_SUITE_P(
    symbols, promptEmojiWidthTest,
    testing::Values(std::make_tuple("\xF0\x9F\x98\x80", 2U), std::make_tuple("\xF0\x9F\x9A\x80", 2U),
                    std::make_tuple("\xF0\x9F\x8E\x89", 2U), std::make_tuple("\xF0\x9F\x80\x84", 2U),
                    std::make_tuple("\xF0\x9D\x84\x9E", 1U), std::make_tuple("\xF0\x9F\xBF\xBF", 1U)));