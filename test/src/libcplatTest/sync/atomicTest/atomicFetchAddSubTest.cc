#include <testfw.h>

#include "atomicTestHelper.h"

#include <cstdint>

// 符号付き 32 ビットの fetch_add が、あらゆるメモリ順序で加算前の値を返し、加算後の値を格納することの確認
TEST(atomicFetchAddSubTest, i32_fetch_add_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i32 atomic = CPLAT_ATOMIC_INIT(0); // [状態] - 0 で初期化したアトミック変数を用意する。
    int32_t previous_values[kAllMemoryOrderCount] = {0};
    int32_t expected_previous[kAllMemoryOrderCount] = {0};
    int32_t current = 0;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const int32_t operand = (int32_t)(10 + (int32_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_add_i32(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で加算する。
        current = (int32_t)(current + operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 加算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_i32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての加算が反映されていること。
}

// 符号付き 32 ビットの fetch_sub が、あらゆるメモリ順序で減算前の値を返し、減算後の値を格納することの確認
TEST(atomicFetchAddSubTest, i32_fetch_sub_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i32 atomic = CPLAT_ATOMIC_INIT(100000); // [状態] - 十分大きい値で初期化したアトミック変数を用意する。
    int32_t previous_values[kAllMemoryOrderCount] = {0};
    int32_t expected_previous[kAllMemoryOrderCount] = {0};
    int32_t current = 100000;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const int32_t operand = (int32_t)(10 + (int32_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_sub_i32(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で減算する。
        current = (int32_t)(current - operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 減算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_i32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての減算が反映されていること。
}

// 符号なし 32 ビットの fetch_add が、あらゆるメモリ順序で加算前の値を返し、加算後の値を格納することの確認
TEST(atomicFetchAddSubTest, u32_fetch_add_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u32 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    uint32_t previous_values[kAllMemoryOrderCount] = {0U};
    uint32_t expected_previous[kAllMemoryOrderCount] = {0U};
    uint32_t current = 0U;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint32_t operand = (uint32_t)(10U + (uint32_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_add_u32(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で加算する。
        current = (uint32_t)(current + operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 加算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_u32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての加算が反映されていること。
}

// 符号なし 32 ビットの fetch_sub が、あらゆるメモリ順序で減算前の値を返し、減算後の値を格納することの確認
TEST(atomicFetchAddSubTest, u32_fetch_sub_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u32 atomic = CPLAT_ATOMIC_INIT(100000U); // [状態] - 十分大きい値で初期化したアトミック変数を用意する。
    uint32_t previous_values[kAllMemoryOrderCount] = {0U};
    uint32_t expected_previous[kAllMemoryOrderCount] = {0U};
    uint32_t current = 100000U;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint32_t operand = (uint32_t)(10U + (uint32_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_sub_u32(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で減算する。
        current = (uint32_t)(current - operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 減算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_u32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての減算が反映されていること。
}

// 符号付き 64 ビットの fetch_add が、あらゆるメモリ順序で加算前の値を返し、加算後の値を格納することの確認
TEST(atomicFetchAddSubTest, i64_fetch_add_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i64 atomic = CPLAT_ATOMIC_INIT(0); // [状態] - 0 で初期化したアトミック変数を用意する。
    int64_t previous_values[kAllMemoryOrderCount] = {0};
    int64_t expected_previous[kAllMemoryOrderCount] = {0};
    int64_t current = 0;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const int64_t operand = (int64_t)(10000000000LL + (int64_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_add_i64(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で加算する。
        current = (int64_t)(current + operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 加算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_i64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての加算が反映されていること。
}

// 符号付き 64 ビットの fetch_sub が、あらゆるメモリ順序で減算前の値を返し、減算後の値を格納することの確認
TEST(atomicFetchAddSubTest, i64_fetch_sub_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i64 atomic = CPLAT_ATOMIC_INIT(100000000000LL); // [状態] - 十分大きい値で初期化したアトミック変数を用意する。
    int64_t previous_values[kAllMemoryOrderCount] = {0};
    int64_t expected_previous[kAllMemoryOrderCount] = {0};
    int64_t current = 100000000000LL;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const int64_t operand = (int64_t)(10000000000LL + (int64_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_sub_i64(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で減算する。
        current = (int64_t)(current - operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 減算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_i64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての減算が反映されていること。
}

// 符号なし 64 ビットの fetch_add が、あらゆるメモリ順序で加算前の値を返し、加算後の値を格納することの確認
TEST(atomicFetchAddSubTest, u64_fetch_add_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u64 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    uint64_t previous_values[kAllMemoryOrderCount] = {0U};
    uint64_t expected_previous[kAllMemoryOrderCount] = {0U};
    uint64_t current = 0U;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint64_t operand = (uint64_t)(10000000000ULL + (uint64_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_add_u64(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で加算する。
        current = (uint64_t)(current + operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 加算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_u64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての加算が反映されていること。
}

// 符号なし 64 ビットの fetch_sub が、あらゆるメモリ順序で減算前の値を返し、減算後の値を格納することの確認
TEST(atomicFetchAddSubTest, u64_fetch_sub_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u64 atomic = CPLAT_ATOMIC_INIT(100000000000ULL); // [状態] - 十分大きい値で初期化したアトミック変数を用意する。
    uint64_t previous_values[kAllMemoryOrderCount] = {0U};
    uint64_t expected_previous[kAllMemoryOrderCount] = {0U};
    uint64_t current = 100000000000ULL;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint64_t operand = (uint64_t)(10000000000ULL + (uint64_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_fetch_sub_u64(&atomic, operand, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で減算する。
        current = (uint64_t)(current - operand);
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 減算前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_u64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - すべての減算が反映されていること。
}
