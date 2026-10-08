#include <testfw.h>

#include "atomicTestHelper.h"

#include <cstdint>

// 符号なし 8 ビットの compare_exchange が、期待値が一致する限りあらゆるメモリ順序で成功することの確認
TEST(atomicCompareExchangeTest, u8_compare_exchange_succeeds_and_updates_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u8 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    uint8_t expected_before[kAllMemoryOrderCount] = {0U};
    uint8_t expected_after[kAllMemoryOrderCount] = {0U};
    uint8_t desired_values[kAllMemoryOrderCount] = {0U};
    uint8_t loaded_after[kAllMemoryOrderCount] = {0U};
    uint8_t current = 0U;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        uint8_t expected = current; // 現在値と一致させ、交換を成功させる

        desired_values[index] = (uint8_t)(30U + (uint8_t)index);
        expected_before[index] = expected;
        exchanged[index] =
            cplat_atomic_compare_exchange_u8(&atomic, &expected, desired_values[index],
                                             kAllMemoryOrders[index]); // [手順] - 期待値を現在値に一致させて交換する。
        expected_after[index] = expected;
        loaded_after[index] =
            cplat_atomic_load_u8(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 交換後の値を読み取る。
        current = desired_values[index];
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_NE(0, exchanged[index]); // [確認_正常系] - 一致した期待値での交換が成功と判定されること。
        EXPECT_EQ(expected_before[index],
                  expected_after[index]); // [確認_正常系] - 成功時に expected が変化しないこと。
        EXPECT_EQ(desired_values[index],
                  loaded_after[index]); // [確認_正常系] - 交換後に desired が格納されていること。
    }
}

// 符号なし 8 ビットの compare_exchange が、期待値が不一致のときあらゆるメモリ順序で失敗し、現在値を報告することの確認
TEST(atomicCompareExchangeTest, u8_compare_exchange_fails_and_reports_current_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u8 atomic = CPLAT_ATOMIC_INIT(200U); // [状態] - 200 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    uint8_t expected_after[kAllMemoryOrderCount] = {0U};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        uint8_t expected = (uint8_t)(5U + (uint8_t)index); // 現在値 200 と一致しない期待値

        exchanged[index] =
            cplat_atomic_compare_exchange_u8(&atomic, &expected, (uint8_t)(1U + (uint8_t)index),
                                             kAllMemoryOrders[index]); // [手順] - 一致しない期待値で交換を試行する。
        expected_after[index] = expected;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(0, exchanged[index]);         // [確認_異常系] - 不一致の期待値では交換が失敗と判定されること。
        EXPECT_EQ(200U, expected_after[index]); // [確認_異常系] - 失敗時に expected へ現在値 200 が格納されること。
    }
    EXPECT_EQ(200U, cplat_atomic_load_u8(
                        &atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_異常系] - 失敗した交換で値が変化しないこと。
}

// 符号付き 32 ビットの compare_exchange が、期待値が一致する限りあらゆるメモリ順序で成功することの確認
TEST(atomicCompareExchangeTest, i32_compare_exchange_succeeds_and_updates_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i32 atomic = CPLAT_ATOMIC_INIT(0); // [状態] - 0 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    int32_t expected_before[kAllMemoryOrderCount] = {0};
    int32_t expected_after[kAllMemoryOrderCount] = {0};
    int32_t desired_values[kAllMemoryOrderCount] = {0};
    int32_t loaded_after[kAllMemoryOrderCount] = {0};
    int32_t current = 0;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        int32_t expected = current; // 現在値と一致させ、交換を成功させる

        desired_values[index] = (int32_t)(2000 + (int32_t)index);
        expected_before[index] = expected;
        exchanged[index] =
            cplat_atomic_compare_exchange_i32(&atomic, &expected, desired_values[index],
                                              kAllMemoryOrders[index]); // [手順] - 期待値を現在値に一致させて交換する。
        expected_after[index] = expected;
        loaded_after[index] =
            cplat_atomic_load_i32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 交換後の値を読み取る。
        current = desired_values[index];
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_NE(0, exchanged[index]); // [確認_正常系] - 一致した期待値での交換が成功と判定されること。
        EXPECT_EQ(expected_before[index],
                  expected_after[index]); // [確認_正常系] - 成功時に expected が変化しないこと。
        EXPECT_EQ(desired_values[index],
                  loaded_after[index]); // [確認_正常系] - 交換後に desired が格納されていること。
    }
}

// 符号付き 32 ビットの compare_exchange が、期待値が不一致のときあらゆるメモリ順序で失敗し、現在値を報告することの確認
TEST(atomicCompareExchangeTest, i32_compare_exchange_fails_and_reports_current_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i32 atomic = CPLAT_ATOMIC_INIT(777); // [状態] - 777 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    int32_t expected_after[kAllMemoryOrderCount] = {0};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        int32_t expected = (int32_t)(999 + (int32_t)index); // 現在値 777 と一致しない期待値

        exchanged[index] =
            cplat_atomic_compare_exchange_i32(&atomic, &expected, (int32_t)(1 + (int32_t)index),
                                              kAllMemoryOrders[index]); // [手順] - 一致しない期待値で交換を試行する。
        expected_after[index] = expected;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(0, exchanged[index]);        // [確認_異常系] - 不一致の期待値では交換が失敗と判定されること。
        EXPECT_EQ(777, expected_after[index]); // [確認_異常系] - 失敗時に expected へ現在値 777 が格納されること。
    }
    EXPECT_EQ(777, cplat_atomic_load_i32(
                       &atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_異常系] - 失敗した交換で値が変化しないこと。
}

// 符号なし 32 ビットの compare_exchange が、期待値が一致する限りあらゆるメモリ順序で成功することの確認
TEST(atomicCompareExchangeTest, u32_compare_exchange_succeeds_and_updates_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u32 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    uint32_t expected_before[kAllMemoryOrderCount] = {0U};
    uint32_t expected_after[kAllMemoryOrderCount] = {0U};
    uint32_t desired_values[kAllMemoryOrderCount] = {0U};
    uint32_t loaded_after[kAllMemoryOrderCount] = {0U};
    uint32_t current = 0U;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        uint32_t expected = current; // 現在値と一致させ、交換を成功させる

        desired_values[index] = (uint32_t)(3000U + (uint32_t)index);
        expected_before[index] = expected;
        exchanged[index] =
            cplat_atomic_compare_exchange_u32(&atomic, &expected, desired_values[index],
                                              kAllMemoryOrders[index]); // [手順] - 期待値を現在値に一致させて交換する。
        expected_after[index] = expected;
        loaded_after[index] =
            cplat_atomic_load_u32(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 交換後の値を読み取る。
        current = desired_values[index];
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_NE(0, exchanged[index]); // [確認_正常系] - 一致した期待値での交換が成功と判定されること。
        EXPECT_EQ(expected_before[index],
                  expected_after[index]); // [確認_正常系] - 成功時に expected が変化しないこと。
        EXPECT_EQ(desired_values[index],
                  loaded_after[index]); // [確認_正常系] - 交換後に desired が格納されていること。
    }
}

// 符号なし 32 ビットの compare_exchange が、期待値が不一致のときあらゆるメモリ順序で失敗し、現在値を報告することの確認
TEST(atomicCompareExchangeTest, u32_compare_exchange_fails_and_reports_current_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u32 atomic = CPLAT_ATOMIC_INIT(777U); // [状態] - 777 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    uint32_t expected_after[kAllMemoryOrderCount] = {0U};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        uint32_t expected = (uint32_t)(999U + (uint32_t)index); // 現在値 777 と一致しない期待値

        exchanged[index] =
            cplat_atomic_compare_exchange_u32(&atomic, &expected, (uint32_t)(1U + (uint32_t)index),
                                              kAllMemoryOrders[index]); // [手順] - 一致しない期待値で交換を試行する。
        expected_after[index] = expected;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(0, exchanged[index]);         // [確認_異常系] - 不一致の期待値では交換が失敗と判定されること。
        EXPECT_EQ(777U, expected_after[index]); // [確認_異常系] - 失敗時に expected へ現在値 777 が格納されること。
    }
    EXPECT_EQ(777U, cplat_atomic_load_u32(
                        &atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_異常系] - 失敗した交換で値が変化しないこと。
}

// 符号付き 64 ビットの compare_exchange が、期待値が一致する限りあらゆるメモリ順序で成功することの確認
TEST(atomicCompareExchangeTest, i64_compare_exchange_succeeds_and_updates_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i64 atomic = CPLAT_ATOMIC_INIT(0); // [状態] - 0 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    int64_t expected_before[kAllMemoryOrderCount] = {0};
    int64_t expected_after[kAllMemoryOrderCount] = {0};
    int64_t desired_values[kAllMemoryOrderCount] = {0};
    int64_t loaded_after[kAllMemoryOrderCount] = {0};
    int64_t current = 0;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        int64_t expected = current; // 現在値と一致させ、交換を成功させる

        desired_values[index] = (int64_t)(70000000000LL + (int64_t)index);
        expected_before[index] = expected;
        exchanged[index] =
            cplat_atomic_compare_exchange_i64(&atomic, &expected, desired_values[index],
                                              kAllMemoryOrders[index]); // [手順] - 期待値を現在値に一致させて交換する。
        expected_after[index] = expected;
        loaded_after[index] =
            cplat_atomic_load_i64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 交換後の値を読み取る。
        current = desired_values[index];
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_NE(0, exchanged[index]); // [確認_正常系] - 一致した期待値での交換が成功と判定されること。
        EXPECT_EQ(expected_before[index],
                  expected_after[index]); // [確認_正常系] - 成功時に expected が変化しないこと。
        EXPECT_EQ(desired_values[index],
                  loaded_after[index]); // [確認_正常系] - 交換後に desired が格納されていること。
    }
}

// 符号付き 64 ビットの compare_exchange が、期待値が不一致のときあらゆるメモリ順序で失敗し、現在値を報告することの確認
TEST(atomicCompareExchangeTest, i64_compare_exchange_fails_and_reports_current_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_i64 atomic = CPLAT_ATOMIC_INIT(777); // [状態] - 777 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    int64_t expected_after[kAllMemoryOrderCount] = {0};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        int64_t expected = (int64_t)(999 + (int64_t)index); // 現在値 777 と一致しない期待値

        exchanged[index] =
            cplat_atomic_compare_exchange_i64(&atomic, &expected, (int64_t)(1 + (int64_t)index),
                                              kAllMemoryOrders[index]); // [手順] - 一致しない期待値で交換を試行する。
        expected_after[index] = expected;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(0, exchanged[index]);        // [確認_異常系] - 不一致の期待値では交換が失敗と判定されること。
        EXPECT_EQ(777, expected_after[index]); // [確認_異常系] - 失敗時に expected へ現在値 777 が格納されること。
    }
    EXPECT_EQ(777, cplat_atomic_load_i64(
                       &atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_異常系] - 失敗した交換で値が変化しないこと。
}

// 符号なし 64 ビットの compare_exchange が、期待値が一致する限りあらゆるメモリ順序で成功することの確認
TEST(atomicCompareExchangeTest, u64_compare_exchange_succeeds_and_updates_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u64 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    uint64_t expected_before[kAllMemoryOrderCount] = {0U};
    uint64_t expected_after[kAllMemoryOrderCount] = {0U};
    uint64_t desired_values[kAllMemoryOrderCount] = {0U};
    uint64_t loaded_after[kAllMemoryOrderCount] = {0U};
    uint64_t current = 0U;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        uint64_t expected = current; // 現在値と一致させ、交換を成功させる

        desired_values[index] = (uint64_t)(80000000000ULL + (uint64_t)index);
        expected_before[index] = expected;
        exchanged[index] =
            cplat_atomic_compare_exchange_u64(&atomic, &expected, desired_values[index],
                                              kAllMemoryOrders[index]); // [手順] - 期待値を現在値に一致させて交換する。
        expected_after[index] = expected;
        loaded_after[index] =
            cplat_atomic_load_u64(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 交換後の値を読み取る。
        current = desired_values[index];
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_NE(0, exchanged[index]); // [確認_正常系] - 一致した期待値での交換が成功と判定されること。
        EXPECT_EQ(expected_before[index],
                  expected_after[index]); // [確認_正常系] - 成功時に expected が変化しないこと。
        EXPECT_EQ(desired_values[index],
                  loaded_after[index]); // [確認_正常系] - 交換後に desired が格納されていること。
    }
}

// 符号なし 64 ビットの compare_exchange が、期待値が不一致のときあらゆるメモリ順序で失敗し、現在値を報告することの確認
TEST(atomicCompareExchangeTest, u64_compare_exchange_fails_and_reports_current_value_for_every_memory_order)
{
    // Arrange
    cplat_atomic_u64 atomic = CPLAT_ATOMIC_INIT(777U); // [状態] - 777 で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    uint64_t expected_after[kAllMemoryOrderCount] = {0U};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        uint64_t expected = (uint64_t)(999U + (uint64_t)index); // 現在値 777 と一致しない期待値

        exchanged[index] =
            cplat_atomic_compare_exchange_u64(&atomic, &expected, (uint64_t)(1U + (uint64_t)index),
                                              kAllMemoryOrders[index]); // [手順] - 一致しない期待値で交換を試行する。
        expected_after[index] = expected;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(0, exchanged[index]);         // [確認_異常系] - 不一致の期待値では交換が失敗と判定されること。
        EXPECT_EQ(777U, expected_after[index]); // [確認_異常系] - 失敗時に expected へ現在値 777 が格納されること。
    }
    EXPECT_EQ(777U, cplat_atomic_load_u64(
                        &atomic, CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_異常系] - 失敗した交換で値が変化しないこと。
}

// ポインターの compare_exchange が、期待値が一致する限りあらゆるメモリ順序で成功することの確認
TEST(atomicCompareExchangeTest, ptr_compare_exchange_succeeds_and_updates_value_for_every_memory_order)
{
    // Arrange
    int markers[kAllMemoryOrderCount] = {0};
    cplat_atomic_ptr atomic = CPLAT_ATOMIC_INIT(nullptr); // [状態] - NULL で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    void *expected_before[kAllMemoryOrderCount] = {nullptr};
    void *expected_after[kAllMemoryOrderCount] = {nullptr};
    void *desired_values[kAllMemoryOrderCount] = {nullptr};
    void *loaded_after[kAllMemoryOrderCount] = {nullptr};
    void *current = nullptr;

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        void *expected = current; // 現在値と一致させ、交換を成功させる

        markers[index] = (int)index;
        desired_values[index] = &markers[index];
        expected_before[index] = expected;
        exchanged[index] =
            cplat_atomic_compare_exchange_ptr(&atomic, &expected, desired_values[index],
                                              kAllMemoryOrders[index]); // [手順] - 期待値を現在値に一致させて交換する。
        expected_after[index] = expected;
        loaded_after[index] =
            cplat_atomic_load_ptr(&atomic, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 交換後の値を読み取る。
        current = desired_values[index];
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_NE(0, exchanged[index]); // [確認_正常系] - 一致した期待値での交換が成功と判定されること。
        EXPECT_EQ(expected_before[index],
                  expected_after[index]); // [確認_正常系] - 成功時に expected が変化しないこと。
        EXPECT_EQ(desired_values[index],
                  loaded_after[index]); // [確認_正常系] - 交換後に desired が格納されていること。
    }
}

// ポインターの compare_exchange が、期待値が不一致のときあらゆるメモリ順序で失敗し、現在値を報告することの確認
TEST(atomicCompareExchangeTest, ptr_compare_exchange_fails_and_reports_current_value_for_every_memory_order)
{
    // Arrange
    int current_marker = 0;
    int other_marker = 0;
    cplat_atomic_ptr atomic =
        CPLAT_ATOMIC_INIT(&current_marker); // [状態] - &current_marker で初期化したアトミック変数を用意する。
    int exchanged[kAllMemoryOrderCount] = {0};
    void *expected_after[kAllMemoryOrderCount] = {nullptr};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        void *expected = &other_marker; // 現在値 &current_marker と一致しない期待値

        exchanged[index] =
            cplat_atomic_compare_exchange_ptr(&atomic, &expected, &other_marker,
                                              kAllMemoryOrders[index]); // [手順] - 一致しない期待値で交換を試行する。
        expected_after[index] = expected;
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ(0, exchanged[index]); // [確認_異常系] - 不一致の期待値では交換が失敗と判定されること。
        EXPECT_EQ(
            (void *)&current_marker,
            expected_after[index]); // [確認_異常系] - 失敗時に expected へ現在値 &current_marker が格納されること。
    }
    EXPECT_EQ((void *)&current_marker,
              cplat_atomic_load_ptr(&atomic,
                                    CPLAT_MEMORY_ORDER_SEQ_CST)); // [確認_異常系] - 失敗した交換で値が変化しないこと。
}
