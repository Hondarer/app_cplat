#include <testfw.h>
#include <cplat/crt/stdlib.h>
#include <cstdint>
#include <cstring>

class allocTest : public Test
{
};

// 指定したバイト数の領域が確保され、書き込めることの確認
TEST_F(allocTest, malloc_allocates_requested_size)
{
    // Arrange
    uint8_t *buf = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。

    // Pre-Assert

    // Act
    buf = (uint8_t *)cplat_malloc(16U); // [手順] - 16 バイトを指定して cplat_malloc() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, buf); // [確認_正常系] - cplat_malloc() の戻り値が nullptr 以外であること。

    // Act_2
    memset(buf, 0xAB, 16U); // [手順] - 確保した 16 バイトを 0xAB で埋める。

    // Assert_2
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(確保した 16 バイト全体へ書き込めたことの確認)
    EXPECT_EQ((uint8_t)0xAB, buf[0]);
    // [確認_正常系] - buf[0] が 0xAB であること。
    EXPECT_EQ((uint8_t)0xAB, buf[15]);
    // [確認_正常系] - buf[15] が 0xAB であること。

    // Cleanup
    cplat_free(buf);
}

// バイト数 0 の指定で確保が行われないことの確認
TEST_F(allocTest, malloc_rejects_zero_size)
{
    // Arrange
    void *ptr = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。

    // Pre-Assert

    // Act
    ptr = cplat_malloc(0U); // [手順] - 0 を指定して cplat_malloc() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, ptr); // [確認_異常系] - cplat_malloc() の戻り値が nullptr であること。
}

// 確保できない大きさの要求が NULL を返すことの確認
TEST_F(allocTest, malloc_returns_null_on_failure)
{
    // Arrange
    void *ptr = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。

    // Pre-Assert

    // Act
    ptr = cplat_malloc(SIZE_MAX /
                       2U); // [手順] - 確保できない大きさ (SIZE_MAX / 2) を指定して cplat_malloc() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, ptr); // [確認_異常系] - cplat_malloc() の戻り値が nullptr であること。
}

// 確保した領域全体がゼロ初期化されることの確認
TEST_F(allocTest, malloc_zerofill_clears_whole_area)
{
    // Arrange
    uint8_t *buf = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。
    int nonzero_count = 0;
    size_t i;

    // Pre-Assert

    // Act
    // [手順] - 64 バイトを指定して cplat_malloc_zerofill() を呼び出す。
    buf = (uint8_t *)cplat_malloc_zerofill(64U);

    // Assert
    ASSERT_NE(nullptr,
              buf); // [確認_正常系] - cplat_malloc_zerofill() の戻り値が nullptr 以外であること。
    for (i = 0U; i < 64U; i++)
    {
        if (buf[i] != 0U)
        {
            nonzero_count++;
        }
    }
    EXPECT_EQ(
        0,
        nonzero_count); // [確認_正常系] - nonzero_count が 0 であること。(確保した 64 バイトすべてがゼロ初期化されたことの確認)

    // Cleanup
    cplat_free(buf);
}

// バイト数 0 の指定で確保が行われないことの確認 (ゼロ初期化版)
TEST_F(allocTest, malloc_zerofill_rejects_zero_size)
{
    // Arrange
    void *ptr = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。

    // Pre-Assert

    // Act
    ptr = cplat_malloc_zerofill(0U); // [手順] - 0 を指定して cplat_malloc_zerofill() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, ptr); // [確認_異常系] - cplat_malloc_zerofill() の戻り値が nullptr であること。
}

// 要素数とサイズの指定で領域が確保され、ゼロ初期化されることの確認
TEST_F(allocTest, calloc_allocates_and_clears)
{
    // Arrange
    int32_t *values = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。
    int nonzero_count = 0;
    size_t i;

    // Pre-Assert

    // Act
    // [手順] - 要素数 8、要素サイズ 4 を指定して cplat_calloc() を呼び出す。
    values = (int32_t *)cplat_calloc(8U, sizeof(*values));

    // Assert
    ASSERT_NE(nullptr, values); // [確認_正常系] - cplat_calloc() の戻り値が nullptr 以外であること。
    for (i = 0U; i < 8U; i++)
    {
        if (values[i] != 0)
        {
            nonzero_count++;
        }
    }
    EXPECT_EQ(
        0,
        nonzero_count); // [確認_正常系] - nonzero_count が 0 であること。(確保した 8 要素すべてがゼロ初期化されたことの確認)

    // Cleanup
    cplat_free(values);
}

// 要素数 0 の指定で確保が行われないことの確認
TEST_F(allocTest, calloc_rejects_zero_count)
{
    // Arrange
    void *ptr = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。

    // Pre-Assert

    // Act
    ptr = cplat_calloc(0U, 4U); // [手順] - 要素数 0、要素サイズ 4 を指定して cplat_calloc() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              ptr); // [確認_異常系] - 要素数 0 を渡した cplat_calloc() の戻り値が nullptr であること。
}

// 要素サイズ 0 の指定で確保が行われないことの確認
TEST_F(allocTest, calloc_rejects_zero_size)
{
    // Arrange
    void *ptr = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。

    // Pre-Assert

    // Act
    ptr = cplat_calloc(4U, 0U); // [手順] - 要素数 4、要素サイズ 0 を指定して cplat_calloc() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              ptr); // [確認_異常系] - 要素サイズ 0 を渡した cplat_calloc() の戻り値が nullptr であること。
}

// 要素数と要素サイズの乗算が回り込む指定で確保が行われないことの確認
TEST_F(allocTest, calloc_rejects_size_overflow)
{
    // Arrange
    void *ptr = nullptr;                      // [状態] - 確保結果の格納先を nullptr で初期化する。
    const size_t huge = (SIZE_MAX / 8U) + 1U; // [状態] - 8 倍すると size_t を回り込む要素数を用意する。

    // Pre-Assert

    // Act
    ptr = cplat_calloc(huge, 8U); // [手順] - 乗算が回り込む要素数と要素サイズ 8 を指定して cplat_calloc() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              ptr); // [確認_異常系] - 乗算が回り込む指定の cplat_calloc() の戻り値が nullptr であること。
}

// 伸長後も元の内容が保持されることの確認
TEST_F(allocTest, realloc_preserves_existing_content)
{
    // Arrange
    int32_t *values = nullptr;     // [状態] - 確保結果の格納先を nullptr で初期化する。
    int32_t *new_values = nullptr; // [状態] - 再確保結果の格納先を nullptr で初期化する。

    values = (int32_t *)cplat_calloc(4U, sizeof(*values)); // [状態] - 4 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    values[0] = 11;
    values[3] = 44;

    EXPECT_EQ(11, values[0]); // [状態確認] - 再確保前の values[0] が 11 であること。
    EXPECT_EQ(44, values[3]); // [状態確認] - 再確保前の values[3] が 44 であること。

    // Pre-Assert

    // Act
    new_values = (int32_t *)cplat_realloc(
        values, 16U, sizeof(*new_values)); // [手順] - 要素数 16 への伸長を指定して cplat_realloc() を呼び出す。

    // Assert
    ASSERT_NE(nullptr,
              new_values);        // [確認_正常系] - cplat_realloc() の戻り値が nullptr 以外であること。
    EXPECT_EQ(11, new_values[0]); // [確認_正常系] - 伸長後の new_values[0] が 11 であること。
    EXPECT_EQ(44, new_values[3]); // [確認_正常系] - 伸長後の new_values[3] が 44 であること。

    // Cleanup
    cplat_free(new_values);
}

// NULL を渡した再確保が新規確保として動作することの確認
TEST_F(allocTest, realloc_allocates_when_ptr_is_null)
{
    // Arrange
    int32_t *values = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。

    // Pre-Assert

    // Act
    // [手順] - ptr に nullptr を指定して cplat_realloc() を呼び出す。
    values = (int32_t *)cplat_realloc(nullptr, 4U, sizeof(*values));

    // Assert
    // ptr に NULL を渡した cplat_realloc の戻り値が NULL でないこと。
    ASSERT_NE(nullptr, values);
    // [確認_異常系] - ptr に nullptr を渡した cplat_realloc() の戻り値が nullptr 以外であること。

    // Cleanup
    cplat_free(values);
}

// 要素数 0 の再確保が元の領域を解放せずに NULL を返すことの確認
TEST_F(allocTest, realloc_rejects_zero_count_without_free)
{
    // Arrange
    int32_t *values = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。
    void *ptr = nullptr;       // [状態] - 再確保結果の格納先を nullptr で初期化する。

    values = (int32_t *)cplat_calloc(4U, sizeof(*values)); // [状態] - 4 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    values[0] = 55;

    EXPECT_EQ(55, values[0]); // [状態確認] - 再確保前の values[0] が 55 であること。

    // Pre-Assert

    // Act
    ptr = cplat_realloc(values, 0U, sizeof(*values)); // [手順] - 要素数 0 を指定して cplat_realloc() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              ptr); // [確認_異常系] - 要素数 0 を渡した cplat_realloc() の戻り値が nullptr であること。
    EXPECT_EQ(55,
              values[0]); // [確認_異常系] - values[0] が 55 であること。(元の領域が解放されずに保持されたことの確認)

    // Cleanup
    cplat_free(values);
}

// 乗算が回り込む再確保が元の領域を解放せずに NULL を返すことの確認
TEST_F(allocTest, realloc_rejects_size_overflow_without_free)
{
    // Arrange
    int32_t *values = nullptr;                // [状態] - 確保結果の格納先を nullptr で初期化する。
    void *ptr = nullptr;                      // [状態] - 再確保結果の格納先を nullptr で初期化する。
    const size_t huge = (SIZE_MAX / 8U) + 1U; // [状態] - 8 倍すると size_t を回り込む要素数を用意する。

    values = (int32_t *)cplat_calloc(4U, sizeof(*values)); // [状態] - 4 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    values[0] = 66;

    EXPECT_EQ(66, values[0]); // [状態確認] - 再確保前の values[0] が 66 であること。

    // Pre-Assert

    // Act
    // [手順] - 乗算が回り込む要素数と要素サイズ 8 を指定して cplat_realloc() を呼び出す。
    ptr = cplat_realloc(values, huge, 8U);

    // Assert
    EXPECT_EQ(nullptr,
              ptr); // [確認_異常系] - 乗算が回り込む指定の cplat_realloc() の戻り値が nullptr であること。
    EXPECT_EQ(66,
              values[0]); // [確認_異常系] - values[0] が 66 であること。(元の領域が解放されずに保持されたことの確認)

    // Cleanup
    cplat_free(values);
}

// 拡張した範囲だけがゼロ初期化され、既存の範囲が保持されることの確認
TEST_F(allocTest, realloc_zerofill_clears_extended_range_only)
{
    // Arrange
    int32_t *values = nullptr;     // [状態] - 確保結果の格納先を nullptr で初期化する。
    int32_t *new_values = nullptr; // [状態] - 再確保結果の格納先を nullptr で初期化する。
    int nonzero_count = 0;
    size_t i;

    values = (int32_t *)cplat_calloc(4U, sizeof(*values)); // [状態] - 4 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    for (i = 0U; i < 4U; i++)
    {
        values[i] = 7;
    }

    EXPECT_EQ(7, values[0]); // [状態確認] - 再確保前の values[0] が 7 であること。
    EXPECT_EQ(7, values[3]); // [状態確認] - 再確保前の values[3] が 7 であること。

    // Pre-Assert

    // Act
    // [手順] - 旧要素数 4、新要素数 16 を指定して cplat_realloc_zerofill() を呼び出す。
    new_values = (int32_t *)cplat_realloc_zerofill(values, 4U, 16U, sizeof(*new_values));

    // Assert
    ASSERT_NE(nullptr,
              new_values); // [確認_正常系] - cplat_realloc_zerofill() の戻り値が nullptr 以外であること。
    for (i = 0U; i < 4U; i++)
    {
        if (new_values[i] != 7)
        {
            nonzero_count++;
        }
    }
    EXPECT_EQ(
        0,
        nonzero_count); // [確認_正常系] - 旧範囲を調べた nonzero_count が 0 であること。(旧範囲の 4 要素すべてが 7 のまま保持されたことの確認)

    nonzero_count = 0;
    for (i = 4U; i < 16U; i++)
    {
        if (new_values[i] != 0)
        {
            nonzero_count++;
        }
    }
    EXPECT_EQ(
        0,
        nonzero_count); // [確認_正常系] - 拡張範囲を調べた nonzero_count が 0 であること。(拡張範囲の 12 要素すべてがゼロ初期化されたことの確認)

    // Cleanup
    cplat_free(new_values);
}

// 旧要素数が新要素数以上の場合にゼロ初期化が行われないことの確認
TEST_F(allocTest, realloc_zerofill_skips_when_not_extended)
{
    // Arrange
    int32_t *values = nullptr;     // [状態] - 確保結果の格納先を nullptr で初期化する。
    int32_t *new_values = nullptr; // [状態] - 再確保結果の格納先を nullptr で初期化する。

    values = (int32_t *)cplat_calloc(8U, sizeof(*values)); // [状態] - 8 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    values[0] = 99;
    values[1] = 98;

    EXPECT_EQ(99, values[0]); // [状態確認] - 再確保前の values[0] が 99 であること。

    // Pre-Assert

    // Act
    // [手順] - 旧要素数 8、新要素数 2 を指定して cplat_realloc_zerofill() を呼び出す (縮小)。
    new_values = (int32_t *)cplat_realloc_zerofill(values, 8U, 2U, sizeof(*new_values));

    // Assert
    // 縮小指定の cplat_realloc_zerofill の戻り値が NULL でないこと。
    ASSERT_NE(nullptr, new_values);
    // [確認_正常系] - 縮小指定の cplat_realloc_zerofill() の戻り値が nullptr 以外であること。
    EXPECT_EQ(99, new_values[0]); // [確認_正常系] - 縮小後の new_values[0] が 99 であること。
    EXPECT_EQ(98, new_values[1]); // [確認_正常系] - 縮小後の new_values[1] が 98 であること。

    // Cleanup
    cplat_free(new_values);
}

// NULL の解放が何も行わないことの確認
TEST_F(allocTest, free_accepts_null)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_free(nullptr); // [手順] - nullptr を指定して cplat_free() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(nullptr を渡した cplat_free() が異常終了せずに戻ったことの確認)
}

// 確保できない大きさの再確保が元の領域を解放せずに NULL を返すことの確認
TEST_F(allocTest, realloc_returns_null_on_failure_without_free)
{
    // Arrange
    int32_t *values = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。
    void *ptr = nullptr;       // [状態] - 再確保結果の格納先を nullptr で初期化する。

    values = (int32_t *)cplat_calloc(4U, sizeof(*values)); // [状態] - 4 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    values[0] = 77;

    EXPECT_EQ(77, values[0]); // [状態確認] - 再確保前の values[0] が 77 であること。

    // Pre-Assert

    // Act
    // [手順] - 確保できない要素数 (SIZE_MAX / 4) と要素サイズ 4 を指定して cplat_realloc() を呼び出す。
    ptr = cplat_realloc(values, SIZE_MAX / 4U, 4U);

    // Assert
    EXPECT_EQ(nullptr, ptr); // [確認_異常系] - 確保に失敗した cplat_realloc() の戻り値が nullptr であること。
    EXPECT_EQ(77,
              values[0]); // [確認_異常系] - values[0] が 77 であること。(元の領域が解放されずに保持されたことの確認)

    // Cleanup
    cplat_free(values);
}

// 要素数 0 のゼロ初期化付き再確保が元の領域を解放せずに NULL を返すことの確認
TEST_F(allocTest, realloc_zerofill_rejects_zero_count_without_free)
{
    // Arrange
    int32_t *values = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。
    void *ptr = nullptr;       // [状態] - 再確保結果の格納先を nullptr で初期化する。

    values = (int32_t *)cplat_calloc(4U, sizeof(*values)); // [状態] - 4 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    values[0] = 88;

    EXPECT_EQ(88, values[0]); // [状態確認] - 再確保前の values[0] が 88 であること。

    // Pre-Assert

    // Act
    // [手順] - 旧要素数 4、新要素数 0 を指定して cplat_realloc_zerofill() を呼び出す。
    ptr = cplat_realloc_zerofill(values, 4U, 0U, sizeof(*values));

    // Assert
    // 要素数 0 を渡した cplat_realloc_zerofill の戻り値が NULL であること。
    EXPECT_EQ(nullptr, ptr);
    // [確認_異常系] - 要素数 0 を渡した cplat_realloc_zerofill() の戻り値が nullptr であること。
    EXPECT_EQ(88,
              values[0]); // [確認_異常系] - values[0] が 88 であること。(元の領域が解放されずに保持されたことの確認)

    // Cleanup
    cplat_free(values);
}

// 確保できない大きさのゼロ初期化付き再確保が元の領域を解放せずに NULL を返すことの確認
TEST_F(allocTest, realloc_zerofill_returns_null_on_failure_without_free)
{
    // Arrange
    int32_t *values = nullptr; // [状態] - 確保結果の格納先を nullptr で初期化する。
    void *ptr = nullptr;       // [状態] - 再確保結果の格納先を nullptr で初期化する。

    values = (int32_t *)cplat_calloc(4U, sizeof(*values)); // [状態] - 4 要素の領域を確保する。
    ASSERT_NE(nullptr, values); // [状態確認] - cplat_calloc() の戻り値が nullptr 以外であること。
    values[0] = 89;

    EXPECT_EQ(89, values[0]); // [状態確認] - 再確保前の values[0] が 89 であること。

    // Pre-Assert

    // Act
    // [手順] - 旧要素数 4、確保できない新要素数 (SIZE_MAX / 4) と要素サイズ 4 を指定して cplat_realloc_zerofill() を呼び出す。
    ptr = cplat_realloc_zerofill(values, 4U, SIZE_MAX / 4U, 4U);

    // Assert
    // 確保に失敗した cplat_realloc_zerofill の戻り値が NULL であること。
    EXPECT_EQ(nullptr, ptr);
    // [確認_異常系] - 確保に失敗した cplat_realloc_zerofill() の戻り値が nullptr であること。
    EXPECT_EQ(89,
              values[0]); // [確認_異常系] - values[0] が 89 であること。(元の領域が解放されずに保持されたことの確認)

    // Cleanup
    cplat_free(values);
}
