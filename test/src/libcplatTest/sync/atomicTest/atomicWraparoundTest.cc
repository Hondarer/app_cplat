#include <testfw.h>

#include <cplat/sync/atomic.h>

#include <cstdint>

// 符号付き 32 ビットの fetch_add が、INT32_MAX への加算で INT32_MIN へ折り返すことの確認
TEST(atomicWraparoundTest, i32_fetch_add_wraps_from_int32_max_to_int32_min)
{
    // Arrange
    cplat_atomic_i32 atomic = CPLAT_ATOMIC_INIT(INT32_MAX); // [状態] - INT32_MAX で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const int32_t previous =
        cplat_atomic_fetch_add_i32(&atomic, 1, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - INT32_MAX に 1 を加算する。
    const int32_t actual =
        cplat_atomic_load_i32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 加算後の値を読み取る。

    // Assert
    EXPECT_EQ(INT32_MAX, previous); // [確認_正常系] - 加算前の値が INT32_MAX であること。
    EXPECT_EQ(INT32_MIN, actual);   // [確認_正常系] - 加算後の値が INT32_MIN へ折り返すこと。
}

// 符号付き 32 ビットの fetch_sub が、INT32_MIN からの減算で INT32_MAX へ折り返すことの確認
TEST(atomicWraparoundTest, i32_fetch_sub_wraps_from_int32_min_to_int32_max)
{
    // Arrange
    cplat_atomic_i32 atomic = CPLAT_ATOMIC_INIT(INT32_MIN); // [状態] - INT32_MIN で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const int32_t previous =
        cplat_atomic_fetch_sub_i32(&atomic, 1, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - INT32_MIN から 1 を減算する。
    const int32_t actual =
        cplat_atomic_load_i32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 減算後の値を読み取る。

    // Assert
    EXPECT_EQ(INT32_MIN, previous); // [確認_正常系] - 減算前の値が INT32_MIN であること。
    EXPECT_EQ(INT32_MAX, actual);   // [確認_正常系] - 減算後の値が INT32_MAX へ折り返すこと。
}

// 符号なし 32 ビットの fetch_add が、UINT32_MAX への加算で 0 へ折り返すことの確認
TEST(atomicWraparoundTest, u32_fetch_add_wraps_from_uint32_max_to_zero)
{
    // Arrange
    cplat_atomic_u32 atomic =
        CPLAT_ATOMIC_INIT(UINT32_MAX); // [状態] - UINT32_MAX で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const uint32_t previous =
        cplat_atomic_fetch_add_u32(&atomic, 1U, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - UINT32_MAX に 1 を加算する。
    const uint32_t actual =
        cplat_atomic_load_u32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 加算後の値を読み取る。

    // Assert
    EXPECT_EQ(UINT32_MAX, previous); // [確認_正常系] - 加算前の値が UINT32_MAX であること。
    EXPECT_EQ(0U, actual);           // [確認_正常系] - 加算後の値が 0 へ折り返すこと。
}

// 符号なし 32 ビットの fetch_sub が、0 からの減算で UINT32_MAX へ折り返すことの確認
TEST(atomicWraparoundTest, u32_fetch_sub_wraps_from_zero_to_uint32_max)
{
    // Arrange
    cplat_atomic_u32 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const uint32_t previous =
        cplat_atomic_fetch_sub_u32(&atomic, 1U, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 0 から 1 を減算する。
    const uint32_t actual =
        cplat_atomic_load_u32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 減算後の値を読み取る。

    // Assert
    EXPECT_EQ(0U, previous);       // [確認_正常系] - 減算前の値が 0 であること。
    EXPECT_EQ(UINT32_MAX, actual); // [確認_正常系] - 減算後の値が UINT32_MAX へ折り返すこと。
}

// 符号付き 64 ビットの fetch_add が、INT64_MAX への加算で INT64_MIN へ折り返すことの確認
TEST(atomicWraparoundTest, i64_fetch_add_wraps_from_int64_max_to_int64_min)
{
    // Arrange
    cplat_atomic_i64 atomic = CPLAT_ATOMIC_INIT(INT64_MAX); // [状態] - INT64_MAX で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const int64_t previous =
        cplat_atomic_fetch_add_i64(&atomic, 1, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - INT64_MAX に 1 を加算する。
    const int64_t actual =
        cplat_atomic_load_i64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 加算後の値を読み取る。

    // Assert
    EXPECT_EQ(INT64_MAX, previous); // [確認_正常系] - 加算前の値が INT64_MAX であること。
    EXPECT_EQ(INT64_MIN, actual);   // [確認_正常系] - 加算後の値が INT64_MIN へ折り返すこと。
}

// 符号付き 64 ビットの fetch_sub が、INT64_MIN からの減算で INT64_MAX へ折り返すことの確認
TEST(atomicWraparoundTest, i64_fetch_sub_wraps_from_int64_min_to_int64_max)
{
    // Arrange
    cplat_atomic_i64 atomic = CPLAT_ATOMIC_INIT(INT64_MIN); // [状態] - INT64_MIN で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const int64_t previous =
        cplat_atomic_fetch_sub_i64(&atomic, 1, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - INT64_MIN から 1 を減算する。
    const int64_t actual =
        cplat_atomic_load_i64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 減算後の値を読み取る。

    // Assert
    EXPECT_EQ(INT64_MIN, previous); // [確認_正常系] - 減算前の値が INT64_MIN であること。
    EXPECT_EQ(INT64_MAX, actual);   // [確認_正常系] - 減算後の値が INT64_MAX へ折り返すこと。
}

// 符号なし 64 ビットの fetch_add が、UINT64_MAX への加算で 0 へ折り返すことの確認
TEST(atomicWraparoundTest, u64_fetch_add_wraps_from_uint64_max_to_zero)
{
    // Arrange
    cplat_atomic_u64 atomic =
        CPLAT_ATOMIC_INIT(UINT64_MAX); // [状態] - UINT64_MAX で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const uint64_t previous =
        cplat_atomic_fetch_add_u64(&atomic, 1U, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - UINT64_MAX に 1 を加算する。
    const uint64_t actual =
        cplat_atomic_load_u64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 加算後の値を読み取る。

    // Assert
    EXPECT_EQ(UINT64_MAX, previous); // [確認_正常系] - 加算前の値が UINT64_MAX であること。
    EXPECT_EQ(0U, actual);           // [確認_正常系] - 加算後の値が 0 へ折り返すこと。
}

// 符号なし 64 ビットの fetch_sub が、0 からの減算で UINT64_MAX へ折り返すことの確認
TEST(atomicWraparoundTest, u64_fetch_sub_wraps_from_zero_to_uint64_max)
{
    // Arrange
    cplat_atomic_u64 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。

    // Pre-Assert

    // Act
    const uint64_t previous =
        cplat_atomic_fetch_sub_u64(&atomic, 1U, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 0 から 1 を減算する。
    const uint64_t actual =
        cplat_atomic_load_u64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 減算後の値を読み取る。

    // Assert
    EXPECT_EQ(0U, previous);       // [確認_正常系] - 減算前の値が 0 であること。
    EXPECT_EQ(UINT64_MAX, actual); // [確認_正常系] - 減算後の値が UINT64_MAX へ折り返すこと。
}
