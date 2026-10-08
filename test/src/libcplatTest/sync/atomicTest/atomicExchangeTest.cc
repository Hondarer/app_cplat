#include <testfw.h>

#include "atomicTestHelper.h"

#include <cstdint>

// 符号なし 8 ビットの exchange が、あらゆるメモリ順序で交換前の値を返し、最終値を格納することの確認
TEST(atomicExchangeTest, u8_exchange_returns_previous_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u8 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    uint8_t previous_values[kAllMemoryOrderCount] = {0U};
    uint8_t expected_previous[kAllMemoryOrderCount] = {0U};
    uint8_t current = 0U;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint8_t desired = (uint8_t)(20U + (uint8_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_exchange_u8(&atomic, desired, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で交換する。
        current = desired;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 交換前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_u8(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - 最後に交換した値が格納されていること。
}

// 符号付き 32 ビットの exchange が、あらゆるメモリ順序で交換前の値を返し、最終値を格納することの確認
TEST(atomicExchangeTest, i32_exchange_returns_previous_value_for_every_memory_order)
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
        const int32_t desired = (int32_t)(500 + (int32_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_exchange_i32(&atomic, desired, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で交換する。
        current = desired;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 交換前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_i32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - 最後に交換した値が格納されていること。
}

// 符号なし 32 ビットの exchange が、あらゆるメモリ順序で交換前の値を返し、最終値を格納することの確認
TEST(atomicExchangeTest, u32_exchange_returns_previous_value_for_every_memory_order)
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
        const uint32_t desired = (uint32_t)(600U + (uint32_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_exchange_u32(&atomic, desired, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で交換する。
        current = desired;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 交換前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_u32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - 最後に交換した値が格納されていること。
}

// 符号付き 64 ビットの exchange が、あらゆるメモリ順序で交換前の値を返し、最終値を格納することの確認
TEST(atomicExchangeTest, i64_exchange_returns_previous_value_for_every_memory_order)
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
        const int64_t desired = (int64_t)(50000000000LL + (int64_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_exchange_i64(&atomic, desired, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で交換する。
        current = desired;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 交換前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_i64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - 最後に交換した値が格納されていること。
}

// 符号なし 64 ビットの exchange が、あらゆるメモリ順序で交換前の値を返し、最終値を格納することの確認
TEST(atomicExchangeTest, u64_exchange_returns_previous_value_for_every_memory_order)
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
        const uint64_t desired = (uint64_t)(60000000000ULL + (uint64_t)index);

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_exchange_u64(&atomic, desired, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で交換する。
        current = desired;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 交換前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_u64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - 最後に交換した値が格納されていること。
}

// ポインターの exchange が、あらゆるメモリ順序で交換前の値を返し、最終値を格納することの確認
TEST(atomicExchangeTest, ptr_exchange_returns_previous_value_for_every_memory_order)
{
    // Arrange
    int markers[kAllMemoryOrderCount] = {0};
    cplat_atomic_ptr atomic = CPLAT_ATOMIC_INIT(nullptr); // [状態] - NULL で初期化したアトミック変数を用意する。
    void *previous_values[kAllMemoryOrderCount] = {nullptr};
    void *expected_previous[kAllMemoryOrderCount] = {nullptr};
    void *current = nullptr;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        markers[index] = (int)index;

        void *const desired = &markers[index];

        expected_previous[index] = current;
        previous_values[index] = cplat_atomic_exchange_ptr(&atomic, desired, kAllMemoryOrders[index]); // [手順] - 各メモリ順序で交換する。
        current = desired;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(expected_previous[index], previous_values[index]); // [確認_正常系 回数=5] - 交換前の値が返ること。
    }
    EXPECT_EQ(current, cplat_atomic_load_ptr(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_正常系] - 最後に交換した値が格納されていること。
}
