#include <testfw.h>
#include <cplat/prompt/prompt.h>
#include <cplat/prompt/prompt_edit.h>
#include <mock_cplat.h>

#include <cstdlib>
#include <cstring>
#include <limits>

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
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(text, 0u); // [手順] - 位置 0 を指定して呼び出す。

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
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(text, 3u); // [手順] - 終端の位置 3 を指定して呼び出す。

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
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(text, 4u); // [手順] - 終端の位置 4 を指定して呼び出す。

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
    size_t pos = cplat_internal_prompt_edit_utf8_prev_boundary(text, 1u); // [手順] - 位置 1 を指定して呼び出す。

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
    size_t pos =
        cplat_internal_prompt_edit_utf8_next_boundary(text, 3u, 3u); // [手順] - 終端の位置 3 を指定して呼び出す。

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
    size_t pos =
        cplat_internal_prompt_edit_utf8_next_boundary(text, 3u, 0u); // [手順] - 先頭の位置 0 を指定して呼び出す。

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
    size_t pos =
        cplat_internal_prompt_edit_utf8_next_boundary(text, 4u, 0u); // [手順] - 先頭の位置 0 を指定して呼び出す。

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
    size_t pos = cplat_internal_prompt_edit_utf8_next_boundary(text, 1u, 0u); // [手順] - 位置 0 を指定して呼び出す。

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
    size_t pos =
        cplat_internal_prompt_edit_utf8_sanitize_boundary(text, 3u, 10u); // [手順] - 長さを超える位置 10 を指定する。

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
    size_t pos =
        cplat_internal_prompt_edit_utf8_sanitize_boundary(text, 4u,
                                                          2u); // [手順] - 日本語文字の途中を指す位置 2 を指定する。

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
    size_t pos =
        cplat_internal_prompt_edit_utf8_sanitize_boundary(text, 4u, 3u); // [手順] - 境界上の位置 3 を指定する。

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
    int actual_ret =
        cplat_internal_prompt_edit_ensure_capacity(&buf, &cap, 64u, 8u); // [手順] - 必要量 8 を指定して呼び出す。

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
    int actual_ret =
        cplat_internal_prompt_edit_ensure_capacity(&buf, &cap, 64u, 17u); // [手順] - 必要量 17 を指定して呼び出す。

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
    int actual_ret =
        cplat_internal_prompt_edit_ensure_capacity(&buf, &cap, 20u,
                                                   20u); // [手順] - 上限 20、必要量 20 を指定して呼び出す。

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
    int actual_ret =
        cplat_internal_prompt_edit_ensure_capacity(&buf, &cap, 16u,
                                                   17u); // [手順] - 上限 16 を超える必要量 17 を指定して呼び出す。

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
    char *buf = NULL;
    size_t cap = 0u;

    // Pre-Assert

    // Act
    int actual_ret_null_buf =
        cplat_internal_prompt_edit_ensure_capacity(NULL, &cap, 16u, 8u); // [手順] - buf に NULL を指定する。
    int actual_ret_null_cap =
        cplat_internal_prompt_edit_ensure_capacity(&buf, NULL, 16u, 8u); // [手順] - cap に NULL を指定する。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret_null_buf); // [確認_異常系] - buf が NULL のときの cplat_internal_prompt_edit_ensure_capacity() の戻り値が -1 であること。
    EXPECT_EQ(
        -1,
        actual_ret_null_cap); // [確認_異常系] - cap が NULL のときの cplat_internal_prompt_edit_ensure_capacity() の戻り値が -1 であること。
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
        &max_bytes); // [手順] - 要求値をすべて 0、既定初期容量を 128 として呼び出す。

    // Assert
    EXPECT_EQ((size_t)CPLAT_PROMPT_HISTORY_DEFAULT,
              history_max); // [確認_正常系] - history_max が (size_t)CPLAT_PROMPT_HISTORY_DEFAULT であること。
    EXPECT_EQ((size_t)CPLAT_PROMPT_INPUT_BYTES_DEFAULT,
              max_bytes); // [確認_正常系] - max_bytes が (size_t)CPLAT_PROMPT_INPUT_BYTES_DEFAULT であること。
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
    cplat_internal_prompt_edit_resolve_options(4u, 1u, 1u, 128u, &history_max, &initial_capacity,
                                               &max_bytes); // [手順] - 初期容量と入力上限に 1 を指定して呼び出す。

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
    cplat_internal_prompt_edit_resolve_options(4u, 64u, 16u, 128u, &history_max, &initial_capacity,
                                               &max_bytes); // [手順] - 初期容量 64、入力上限 16 を指定して呼び出す。

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
    cplat_internal_prompt_edit_resolve_options(4u, 8u, 16u, 128u, NULL, NULL,
                                               NULL); // [手順] - 出力先をすべて NULL にして呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(出力先がすべて NULL の解決処理がクラッシュしないことの確認)
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
                       // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    int actual_ret =
        cplat_internal_prompt_edit_ensure_capacity(&buf, &cap, 64u, 17u); // [手順] - 必要量 17 を指定して呼び出す。

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
                       // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    int actual_ret =
        cplat_internal_prompt_edit_ensure_capacity(&buf, &cap, max_size,
                                                   cap + 1u); // [手順] - 現在容量より 1 byte 大きい必要量を指定する。

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
    int actual_ret =
        cplat_internal_prompt_edit_validate_initial_text(NULL, 16u, &length); // [手順] - NULL の初期値を検証する。

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
    int actual_ret =
        cplat_internal_prompt_edit_validate_initial_text(text, 16u, &length); // [手順] - 初期値を検証する。

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
    int actual_ret_newline =
        cplat_internal_prompt_edit_validate_initial_text("a\nb", 16u, &length); // [手順] - 改行を含む初期値を検証する。
    int actual_ret_tab =
        cplat_internal_prompt_edit_validate_initial_text("a\tb", 16u, &length); // [手順] - タブを含む初期値を検証する。
    int actual_ret_delete = cplat_internal_prompt_edit_validate_initial_text(
        "a\x7F", 16u, &length); // [手順] - DEL を含む初期値を検証する。

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
        "abc", 4u, &length_fit); // [手順] - NUL を含めて 4 バイトの初期値を上限 4 で検証する。
    int actual_ret_over = cplat_internal_prompt_edit_validate_initial_text(
        "abcd", 4u, &length_over); // [手順] - NUL を含めて 5 バイトの初期値を上限 4 で検証する。

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
    int actual_ret =
        cplat_internal_prompt_edit_validate_initial_text("", 0u, &length); // [手順] - 上限 0 で空文字列を検証する。

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
    int actual_ret =
        cplat_internal_prompt_edit_validate_initial_text("abc", 16u, NULL); // [手順] - 長さの格納先に NULL を渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_internal_prompt_edit_validate_initial_text() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
