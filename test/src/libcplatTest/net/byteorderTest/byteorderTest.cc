#include <testfw.h>

#include <cplat/net/byteorder.h>

#include <cstring>

namespace
{

/**
 *  @brief          値をバイト列として取り出します。
 *  @param[in]      value   取り出す値。
 *  @param[out]     bytes   バイト列の格納先。
 */
template <typename T> void to_bytes(const T value, uint8_t *bytes)
{
    std::memcpy(bytes, &value, sizeof(T));
}

} // namespace

// 16 bit のネットワーク バイト オーダー変換がビッグ エンディアンのバイト列になることの確認
TEST(byteorderTest, hton16_places_most_significant_byte_first)
{
    // Arrange
    uint8_t bytes[2] = {0U, 0U};

    // Pre-Assert

    // Act
    uint16_t converted = cplat_hton16((uint16_t)0x1234U); // [手順] - 0x1234 をネットワーク バイト オーダーへ変換する。

    // Assert
    to_bytes(converted, bytes);
    EXPECT_EQ(0x12U, bytes[0]); // [確認_正常系] - cplat_hton16 の結果の先頭バイトが上位バイトの 0x12 であること。
    EXPECT_EQ(0x34U, bytes[1]); // [確認_正常系] - cplat_hton16 の結果の 2 バイト目が下位バイトの 0x34 であること。
}

// 16 bit のホスト バイト オーダー変換がビッグ エンディアンのバイト列を解釈することの確認
TEST(byteorderTest, ntoh16_reads_most_significant_byte_first)
{
    // Arrange
    const uint8_t bytes[2] = {0x12U, 0x34U};
    uint16_t network = 0U;

    std::memcpy(&network, bytes, sizeof(network)); // [状態] - ビッグ エンディアンのバイト列を用意する。

    // Pre-Assert

    // Act
    uint16_t converted = cplat_ntoh16(network); // [手順] - バイト列をホスト バイト オーダーへ変換する。

    // Assert
    EXPECT_EQ((uint16_t)0x1234U,
              converted); // [確認_正常系] - cplat_ntoh16 の戻り値が 0x1234 であること。
}

// 32 bit のネットワーク バイト オーダー変換がビッグ エンディアンのバイト列になることの確認
TEST(byteorderTest, hton32_places_most_significant_byte_first)
{
    // Arrange
    uint8_t bytes[4] = {0U, 0U, 0U, 0U};

    // Pre-Assert

    // Act
    uint32_t converted = cplat_hton32(0x12345678U); // [手順] - 0x12345678 をネットワーク バイト オーダーへ変換する。

    // Assert
    to_bytes(converted, bytes);
    EXPECT_EQ(0x12U, bytes[0]); // [確認_正常系] - cplat_hton32 の結果の 1 バイト目が 0x12 であること。
    EXPECT_EQ(0x34U, bytes[1]); // [確認_正常系] - cplat_hton32 の結果の 2 バイト目が 0x34 であること。
    EXPECT_EQ(0x56U, bytes[2]); // [確認_正常系] - cplat_hton32 の結果の 3 バイト目が 0x56 であること。
    EXPECT_EQ(0x78U, bytes[3]); // [確認_正常系] - cplat_hton32 の結果の 4 バイト目が 0x78 であること。
}

// 32 bit のホスト バイト オーダー変換がビッグ エンディアンのバイト列を解釈することの確認
TEST(byteorderTest, ntoh32_reads_most_significant_byte_first)
{
    // Arrange
    const uint8_t bytes[4] = {0x12U, 0x34U, 0x56U, 0x78U};
    uint32_t network = 0U;

    std::memcpy(&network, bytes, sizeof(network)); // [状態] - ビッグ エンディアンのバイト列を用意する。

    // Pre-Assert

    // Act
    uint32_t converted = cplat_ntoh32(network); // [手順] - バイト列をホスト バイト オーダーへ変換する。

    // Assert
    EXPECT_EQ(0x12345678U,
              converted); // [確認_正常系] - cplat_ntoh32 の戻り値が 0x12345678 であること。
}

// 16 bit の変換が往復で元の値へ戻ることの確認
TEST(byteorderTest, hton16_and_ntoh16_round_trip)
{
    // Arrange
    const uint16_t values[] = {0x0000U, 0x0001U, 0x00FFU, 0x0100U, 0x1234U, 0xFFFEU, 0xFFFFU};

    // Pre-Assert

    // Act

    // Assert
    for (const uint16_t value : values)
    {
        EXPECT_EQ(value, cplat_ntoh16(cplat_hton16(value)));
        // [確認_正常系 回数=7] - 境界値を含む各値について cplat_hton16 と cplat_ntoh16 の往復結果が元の値と一致すること。
    }
}

// 32 bit の変換が往復で元の値へ戻ることの確認
TEST(byteorderTest, hton32_and_ntoh32_round_trip)
{
    // Arrange
    const uint32_t values[] = {0x00000000U, 0x00000001U, 0x000000FFU, 0x0000FF00U,
                               0x00FF0000U, 0xFF000000U, 0x12345678U, 0xFFFFFFFFU};

    // Pre-Assert

    // Act

    // Assert
    for (const uint32_t value : values)
    {
        EXPECT_EQ(value, cplat_ntoh32(cplat_hton32(value)));
        // [確認_正常系 回数=8] - 境界値を含む各値について cplat_hton32 と cplat_ntoh32 の往復結果が元の値と一致すること。
    }
}

// 16 bit の変換が上位と下位のバイトを入れ替えることの確認
TEST(byteorderTest, hton16_swaps_boundary_values)
{
    // Arrange
    uint8_t low_only[2] = {0U, 0U};
    uint8_t high_only[2] = {0U, 0U};

    // Pre-Assert

    // Act
    uint16_t converted_low = cplat_hton16((uint16_t)0x00FFU);  // [手順] - 下位バイトのみの値を変換する。
    uint16_t converted_high = cplat_hton16((uint16_t)0xFF00U); // [手順] - 上位バイトのみの値を変換する。

    // Assert
    to_bytes(converted_low, low_only);
    to_bytes(converted_high, high_only);
    EXPECT_EQ(0x00U, low_only[0]);  // [確認_正常系] - 0x00FF を変換した結果の先頭バイトが 0x00 であること。
    EXPECT_EQ(0xFFU, low_only[1]);  // [確認_正常系] - 0x00FF を変換した結果の 2 バイト目が 0xFF であること。
    EXPECT_EQ(0xFFU, high_only[0]); // [確認_正常系] - 0xFF00 を変換した結果の先頭バイトが 0xFF であること。
    EXPECT_EQ(0x00U, high_only[1]); // [確認_正常系] - 0xFF00 を変換した結果の 2 バイト目が 0x00 であること。
}

// 64 bit のネットワーク バイト オーダー変換がビッグ エンディアンのバイト列になることの確認
TEST(byteorderTest, hton64_places_most_significant_byte_first)
{
    // Arrange
    uint8_t bytes[8] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};

    // Pre-Assert

    // Act
    uint64_t converted = cplat_hton64(
        UINT64_C(0x123456789ABCDEF0)); // [手順] - 0x123456789ABCDEF0 をネットワーク バイト オーダーへ変換する。

    // Assert
    to_bytes(converted, bytes);
    EXPECT_EQ(0x12U, bytes[0]); // [確認_正常系] - cplat_hton64 の結果の 1 バイト目が 0x12 であること。
    EXPECT_EQ(0x34U, bytes[1]); // [確認_正常系] - cplat_hton64 の結果の 2 バイト目が 0x34 であること。
    EXPECT_EQ(0x56U, bytes[2]); // [確認_正常系] - cplat_hton64 の結果の 3 バイト目が 0x56 であること。
    EXPECT_EQ(0x78U, bytes[3]); // [確認_正常系] - cplat_hton64 の結果の 4 バイト目が 0x78 であること。
    EXPECT_EQ(0x9AU, bytes[4]); // [確認_正常系] - cplat_hton64 の結果の 5 バイト目が 0x9A であること。
    EXPECT_EQ(0xBCU, bytes[5]); // [確認_正常系] - cplat_hton64 の結果の 6 バイト目が 0xBC であること。
    EXPECT_EQ(0xDEU, bytes[6]); // [確認_正常系] - cplat_hton64 の結果の 7 バイト目が 0xDE であること。
    EXPECT_EQ(0xF0U, bytes[7]); // [確認_正常系] - cplat_hton64 の結果の 8 バイト目が 0xF0 であること。
}

// 64 bit のホスト バイト オーダー変換がビッグ エンディアンのバイト列を解釈することの確認
TEST(byteorderTest, ntoh64_reads_most_significant_byte_first)
{
    // Arrange
    const uint8_t bytes[8] = {0x12U, 0x34U, 0x56U, 0x78U, 0x9AU, 0xBCU, 0xDEU, 0xF0U};
    uint64_t network = 0U;

    std::memcpy(&network, bytes, sizeof(network)); // [状態] - ビッグ エンディアンのバイト列を用意する。

    // Pre-Assert

    // Act
    uint64_t converted = cplat_ntoh64(network); // [手順] - バイト列をホスト バイト オーダーへ変換する。

    // Assert
    EXPECT_EQ(UINT64_C(0x123456789ABCDEF0),
              converted); // [確認_正常系] - cplat_ntoh64 の戻り値が 0x123456789ABCDEF0 であること。
}

// 64 bit の変換が往復で元の値へ戻ることの確認
TEST(byteorderTest, hton64_and_ntoh64_round_trip)
{
    // Arrange
    const uint64_t values[] = {UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000001), UINT64_C(0x00000000000000FF),
                               UINT64_C(0x00000000FF000000), UINT64_C(0x0000000100000000), UINT64_C(0xFF00000000000000),
                               UINT64_C(0x123456789ABCDEF0), UINT64_C(0xFFFFFFFFFFFFFFFF)};

    // Pre-Assert

    // Act

    // Assert
    for (const uint64_t value : values)
    {
        EXPECT_EQ(value, cplat_ntoh64(cplat_hton64(value)));
        // [確認_正常系 回数=8] - 境界値を含む各値について cplat_hton64 と cplat_ntoh64 の往復結果が元の値と一致すること。
    }
}
