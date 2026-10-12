#include <testfw.h>

#include <cplat/sync/atomic.h>

#include <cstdint>

// CPLAT_ATOMIC_INIT で静的初期化した各型が、初期値を保持することの確認
TEST(atomicInitLayoutTest, static_init_holds_initial_value_for_every_type)
{
    // Arrange
    static cplat_atomic_u8 s_u8 = CPLAT_ATOMIC_INIT(200U);             // [状態] - 静的記憶域の u8 を初期化する。
    static cplat_atomic_i32 s_i32 = CPLAT_ATOMIC_INIT(-7);             // [状態] - 静的記憶域の i32 を初期化する。
    static cplat_atomic_u32 s_u32 = CPLAT_ATOMIC_INIT(7U);             // [状態] - 静的記憶域の u32 を初期化する。
    static cplat_atomic_i64 s_i64 = CPLAT_ATOMIC_INIT(-70000000000LL); // [状態] - 静的記憶域の i64 を初期化する。
    static cplat_atomic_u64 s_u64 = CPLAT_ATOMIC_INIT(70000000000ULL); // [状態] - 静的記憶域の u64 を初期化する。
    static int s_marker;
    static cplat_atomic_ptr s_ptr = CPLAT_ATOMIC_INIT(&s_marker); // [状態] - 静的記憶域の ptr を初期化する。

    // Pre-Assert

    // Act
    const uint8_t loaded_u8 = cplat_atomic_load_u8(&s_u8, CPLAT_MEMORY_ORDER_SEQ_CST);     // [手順] - u8 を読み取る。
    const int32_t loaded_i32 = cplat_atomic_load_i32(&s_i32, CPLAT_MEMORY_ORDER_SEQ_CST);  // [手順] - i32 を読み取る。
    const uint32_t loaded_u32 = cplat_atomic_load_u32(&s_u32, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - u32 を読み取る。
    const int64_t loaded_i64 = cplat_atomic_load_i64(&s_i64, CPLAT_MEMORY_ORDER_SEQ_CST);  // [手順] - i64 を読み取る。
    const uint64_t loaded_u64 = cplat_atomic_load_u64(&s_u64, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - u64 を読み取る。
    void *const loaded_ptr = cplat_atomic_load_ptr(&s_ptr, CPLAT_MEMORY_ORDER_SEQ_CST);    // [手順] - ptr を読み取る。

    // Assert
    EXPECT_EQ(200U, loaded_u8); // [確認_正常系] - cplat_atomic_load_u8() の戻り値が 200U であること。
    EXPECT_EQ(-7, loaded_i32);  // [確認_正常系] - cplat_atomic_load_i32() の戻り値が -7 であること。
    EXPECT_EQ(7U, loaded_u32);  // [確認_正常系] - cplat_atomic_load_u32() の戻り値が 7U であること。
    EXPECT_EQ(-70000000000LL,
              loaded_i64); // [確認_正常系] - cplat_atomic_load_i64() の戻り値が -70000000000LL であること。
    EXPECT_EQ(70000000000ULL,
              loaded_u64); // [確認_正常系] - cplat_atomic_load_u64() の戻り値が 70000000000ULL であること。
    EXPECT_EQ((void *)&s_marker,
              loaded_ptr); // [確認_正常系] - cplat_atomic_load_ptr() の戻り値が (void *)&s_marker と一致すること。
}

// 各アトミック型の大きさと配置が、格納する整数型・ポインター型と同じであることの確認
TEST(atomicInitLayoutTest, size_and_alignment_match_stored_type_for_every_type)
{
    // Arrange

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(sizeof(uint8_t),
              sizeof(cplat_atomic_u8)); // [確認_正常系] - sizeof(cplat_atomic_u8) が sizeof(uint8_t) と一致すること。
    EXPECT_EQ(
        alignof(uint8_t),
        alignof(cplat_atomic_u8)); // [確認_正常系] - alignof(cplat_atomic_u8) が alignof(uint8_t) と一致すること。
    EXPECT_EQ(sizeof(int32_t),
              sizeof(cplat_atomic_i32)); // [確認_正常系] - sizeof(cplat_atomic_i32) が sizeof(int32_t) と一致すること。
    EXPECT_EQ(
        alignof(int32_t),
        alignof(cplat_atomic_i32)); // [確認_正常系] - alignof(cplat_atomic_i32) が alignof(int32_t) と一致すること。
    EXPECT_EQ(
        sizeof(uint32_t),
        sizeof(cplat_atomic_u32)); // [確認_正常系] - sizeof(cplat_atomic_u32) が sizeof(uint32_t) と一致すること。
    EXPECT_EQ(
        alignof(uint32_t),
        alignof(cplat_atomic_u32)); // [確認_正常系] - alignof(cplat_atomic_u32) が alignof(uint32_t) と一致すること。
    EXPECT_EQ(sizeof(int64_t),
              sizeof(cplat_atomic_i64)); // [確認_正常系] - sizeof(cplat_atomic_i64) が sizeof(int64_t) と一致すること。
    EXPECT_EQ(
        alignof(int64_t),
        alignof(cplat_atomic_i64)); // [確認_正常系] - alignof(cplat_atomic_i64) が alignof(int64_t) と一致すること。
    EXPECT_EQ(
        sizeof(uint64_t),
        sizeof(cplat_atomic_u64)); // [確認_正常系] - sizeof(cplat_atomic_u64) が sizeof(uint64_t) と一致すること。
    EXPECT_EQ(
        alignof(uint64_t),
        alignof(cplat_atomic_u64)); // [確認_正常系] - alignof(cplat_atomic_u64) が alignof(uint64_t) と一致すること。
    EXPECT_EQ(sizeof(void *),
              sizeof(cplat_atomic_ptr)); // [確認_正常系] - sizeof(cplat_atomic_ptr) が sizeof(void *) と一致すること。
    EXPECT_EQ(
        alignof(void *),
        alignof(cplat_atomic_ptr)); // [確認_正常系] - alignof(cplat_atomic_ptr) が alignof(void *) と一致すること。
}
