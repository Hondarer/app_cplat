#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crypto/random.h>
#if defined(PLATFORM_LINUX)
    #include <mock_openssl.h>
#endif /* PLATFORM_LINUX */

#include <cstring>

class randomTest : public Test
{
};

// 要求したバイト数がすべて満たされることの確認
TEST_F(randomTest, fills_requested_size)
{
    // Arrange
    unsigned char buf[32];
    unsigned char sentinel[32];

    memset(buf, 0xCD, sizeof(buf));           // [状態] - バッファー 32 byte を 0xCD で埋める。
    memset(sentinel, 0xCD, sizeof(sentinel)); // [状態] - 比較用に同じ内容の領域を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_random_bytes(buf, sizeof(buf)); // [手順] - 32 byte を要求して cplat_random_bytes を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_random_bytes の戻り値が CPLAT_OK であること。
    EXPECT_NE(0, memcmp(buf, sentinel,
                        sizeof(buf))); // [確認_正常系] - バッファーが初期値 0xCD のままではなく書き換わること。
}

// 続けて取得した乱数が一致しないことの確認
TEST_F(randomTest, successive_calls_differ)
{
    // Arrange
    unsigned char first[32] = {0};  // [状態] - 1 回目の格納先を 0 で初期化する。
    unsigned char second[32] = {0}; // [状態] - 2 回目の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_first = cplat_random_bytes(first, sizeof(first));    // [手順] - 1 回目の取得を行う。
    int actual_ret_second = cplat_random_bytes(second, sizeof(second)); // [手順] - 2 回目の取得を行う。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_first); // [確認_正常系] - 1 回目の cplat_random_bytes の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_second); // [確認_正常系] - 2 回目の cplat_random_bytes の戻り値が CPLAT_OK であること。
    EXPECT_NE(0, memcmp(first, second, sizeof(first))); // [確認_正常系] - 2 回の取得結果が一致しないこと。
}

// サイズ 0 を成功として扱うことの確認
TEST_F(randomTest, zero_size_succeeds)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_random_bytes(NULL, 0U); // [手順] - バッファーに NULL、サイズに 0 を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - サイズ 0 の場合に cplat_random_bytes の戻り値が CPLAT_OK であること。
}

// バッファーが NULL の場合に引数不正を返すことの確認
TEST_F(randomTest, null_buffer_returns_invalid_argument)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_random_bytes(NULL, 16U); // [手順] - バッファーに NULL、サイズに 16 を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_random_bytes の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 要求バイト数が共通最大サイズを超える場合に拒否されることの確認
// OS ごとの乱数 API に渡す前に、クロスプラットフォームの最大サイズを検査する
TEST_F(randomTest, size_over_int_max_returns_invalid_argument)
{
    // Arrange
    unsigned char buf[1]; // [状態] - 実際には書き込まれない 1 byte のバッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_random_bytes(
        buf, CPLAT_CRYPTO_RANDOM_MAX_BYTES +
                 1U); // [手順] - 共通最大サイズを 1 byte 超えるサイズで cplat_random_bytes を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_random_bytes の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

#if defined(PLATFORM_LINUX)

// 乱数の生成に失敗した場合に通知されることの確認
// Windows は BCryptGenRandom を使うため、この失敗経路は Linux のみに存在する
TEST_F(randomTest, returns_unknown_when_rand_bytes_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;
    unsigned char buf[16]; // [状態] - 16 byte のバッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_openssl, RAND_bytes(_, _, _, buf, 16))
        .WillOnce(Return(0))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - RAND_bytes がバッファーと 16 byte を指定して 1 回目に呼び出されること。
                          // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_random_bytes(buf, sizeof(buf)); // [手順] - cplat_random_bytes を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_random_bytes の戻り値が CPLAT_ERR_UNKNOWN であること。
}

#endif /* PLATFORM_LINUX */
