#include <testfw.h>

#include "atomicTestHelper.h"

#include <cstdint>

// 符号なし 8 ビットの store と load が、あらゆるメモリ順序で書き込んだ値を読めることの確認
TEST(atomicLoadStoreTest, u8_store_and_load_are_consistent_across_memory_orders)
{
    // Arrange
    cplat_atomic_u8 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    uint8_t loaded[kAllMemoryOrderCount] = {0U};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint8_t value = (uint8_t)(10U + (uint8_t)index);
        cplat_atomic_store_u8(&atomic, value, kAllMemoryOrders[index]);         // [手順] - 各メモリ順序で異なる値を書き込む。
        loaded[index] = cplat_atomic_load_u8(&atomic, kAllMemoryOrders[index]); // [手順] - 直後に同じメモリ順序で読み取る。
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ((uint8_t)(10U + (uint8_t)index),
                  loaded[index]); // [確認_正常系 回数=5] - メモリ順序によらず直前に書き込んだ値を読めること。
    }
}

// 符号付き 32 ビットの store と load が、同じメモリ順序を渡す限りどの順序でも書き込んだ値を読めることの確認
TEST(atomicLoadStoreTest, i32_store_and_load_are_consistent_across_memory_orders)
{
    // Arrange
    cplat_atomic_i32 atomic = CPLAT_ATOMIC_INIT(0); // [状態] - 0 で初期化したアトミック変数を用意する。
    int32_t loaded[kAllMemoryOrderCount] = {0};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const int32_t value = (int32_t)(1000 + (int32_t)index);
        cplat_atomic_store_i32(&atomic, value, kAllMemoryOrders[index]);         // [手順] - 各メモリ順序で異なる値を書き込む。
        loaded[index] = cplat_atomic_load_i32(&atomic, kAllMemoryOrders[index]); // [手順] - 直後に同じメモリ順序で読み取る。
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ((int32_t)(1000 + (int32_t)index),
                  loaded[index]); // [確認_正常系 回数=5] - メモリ順序によらず直前に書き込んだ値を読めること。
    }
}

// 符号なし 32 ビットの store と load が、あらゆるメモリ順序で書き込んだ値を読めることの確認
TEST(atomicLoadStoreTest, u32_store_and_load_are_consistent_across_memory_orders)
{
    // Arrange
    cplat_atomic_u32 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    uint32_t loaded[kAllMemoryOrderCount] = {0U};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint32_t value = (uint32_t)(2000U + (uint32_t)index);
        cplat_atomic_store_u32(&atomic, value, kAllMemoryOrders[index]);         // [手順] - 各メモリ順序で異なる値を書き込む。
        loaded[index] = cplat_atomic_load_u32(&atomic, kAllMemoryOrders[index]); // [手順] - 直後に同じメモリ順序で読み取る。
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ((uint32_t)(2000U + (uint32_t)index),
                  loaded[index]); // [確認_正常系 回数=5] - メモリ順序によらず直前に書き込んだ値を読めること。
    }
}

// 符号付き 64 ビットの store と load が、あらゆるメモリ順序で書き込んだ値を読めることの確認
TEST(atomicLoadStoreTest, i64_store_and_load_are_consistent_across_memory_orders)
{
    // Arrange
    cplat_atomic_i64 atomic = CPLAT_ATOMIC_INIT(0); // [状態] - 0 で初期化したアトミック変数を用意する。
    int64_t loaded[kAllMemoryOrderCount] = {0};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const int64_t value = (int64_t)(30000000000LL + (int64_t)index);
        cplat_atomic_store_i64(&atomic, value, kAllMemoryOrders[index]);         // [手順] - 各メモリ順序で異なる値を書き込む。
        loaded[index] = cplat_atomic_load_i64(&atomic, kAllMemoryOrders[index]); // [手順] - 直後に同じメモリ順序で読み取る。
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ((int64_t)(30000000000LL + (int64_t)index),
                  loaded[index]); // [確認_正常系 回数=5] - メモリ順序によらず直前に書き込んだ値を読めること。
    }
}

// 符号なし 64 ビットの store と load が、あらゆるメモリ順序で書き込んだ値を読めることの確認
TEST(atomicLoadStoreTest, u64_store_and_load_are_consistent_across_memory_orders)
{
    // Arrange
    cplat_atomic_u64 atomic = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化したアトミック変数を用意する。
    uint64_t loaded[kAllMemoryOrderCount] = {0U};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        const uint64_t value = (uint64_t)(40000000000ULL + (uint64_t)index);
        cplat_atomic_store_u64(&atomic, value, kAllMemoryOrders[index]);         // [手順] - 各メモリ順序で異なる値を書き込む。
        loaded[index] = cplat_atomic_load_u64(&atomic, kAllMemoryOrders[index]); // [手順] - 直後に同じメモリ順序で読み取る。
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ((uint64_t)(40000000000ULL + (uint64_t)index),
                  loaded[index]); // [確認_正常系 回数=5] - メモリ順序によらず直前に書き込んだ値を読めること。
    }
}

// ポインターの store と load が、あらゆるメモリ順序で書き込んだ値を読めることの確認
TEST(atomicLoadStoreTest, ptr_store_and_load_are_consistent_across_memory_orders)
{
    // Arrange
    cplat_atomic_ptr atomic = CPLAT_ATOMIC_INIT(nullptr); // [状態] - NULL で初期化したアトミック変数を用意する。
    int markers[kAllMemoryOrderCount] = {0};
    void *loaded[kAllMemoryOrderCount] = {nullptr};

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        markers[index] = (int)index;
        cplat_atomic_store_ptr(&atomic, &markers[index], kAllMemoryOrders[index]); // [手順] - 各メモリ順序で異なるポインターを書き込む。
        loaded[index] = cplat_atomic_load_ptr(&atomic, kAllMemoryOrders[index]);   // [手順] - 直後に同じメモリ順序で読み取る。
    }

    // Assert
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        EXPECT_EQ((void *)&markers[index],
                  loaded[index]); // [確認_正常系 回数=5] - メモリ順序によらず直前に書き込んだポインターを読めること。
    }
}

// 同じ変数を 2 つのポインター経由で操作しても、書き込んだ値が一致することの確認 (共有メモリ配置を模擬)
TEST(atomicLoadStoreTest, value_written_through_one_pointer_is_visible_through_another_pointer)
{
    // Arrange
    cplat_atomic_i32 shared_value = CPLAT_ATOMIC_INIT(0); // [状態] - 共有変数を 1 個だけ用意する。
    cplat_atomic_i32 *writer_view = &shared_value;
    cplat_atomic_i32 *reader_view = &shared_value;

    // Pre-Assert

    // Act
    cplat_atomic_store_i32(writer_view, 4242, CPLAT_MEMORY_ORDER_SEQ_CST);   // [手順] - 一方のポインター経由で書き込む。
    const int32_t actual = cplat_atomic_load_i32(reader_view, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - もう一方のポインター経由で読み取る。

    // Assert
    EXPECT_EQ(4242, actual); // [確認_正常系] - 別ポインター経由でも書き込んだ値が読めること。
}
