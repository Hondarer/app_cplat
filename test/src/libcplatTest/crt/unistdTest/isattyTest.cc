#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/crt/unistd.h>

#if defined(PLATFORM_LINUX)
    #include <mock_unistd.h>
    #include <unistd.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::NiceMock;
using testing::Return;

#if defined(PLATFORM_LINUX)

class isattyTest : public testing::Test
{
  protected:
    NiceMock<Mock_unistd> mock_unistd_;
};

#else /* PLATFORM_LINUX */

class isattyTest : public testing::Test
{
};

#endif /* PLATFORM_LINUX */

// 定義外のストリーム指定で 0 が返ることの確認
TEST_F(isattyTest, invalid_stream_returns_zero)
{
    // Arrange
    // 列挙範囲外の不正値を意図的に渡す (定数キャストは -Wconversion になるため変数経由)
    int invalid_stream_value = 99;
    cplat_stream invalid_stream = (cplat_stream)invalid_stream_value; // [状態] - 定義外の enum 値 99 とする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_isatty(invalid_stream); // [手順] - 定義外の enum 値を渡して cplat_isatty を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_異常系] - cplat_isatty の戻り値が 0 であること。
}

// stdin の判定が STDIN_FILENO の isatty 結果になることの確認
TEST_F(isattyTest, stdin_returns_int)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, isatty(_, _, _, STDIN_FILENO))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - isatty が STDIN_FILENO で 1 回呼び出されること。
                              // [Pre-Assert手順] - 1 を返却する。
#endif                        /* PLATFORM_LINUX */

    // Act
    int actual_ret = cplat_isatty(CPLAT_STREAM_STDIN); // [手順] - CPLAT_STREAM_STDIN を渡して cplat_isatty を呼び出す。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(1, actual_ret); // [確認_正常系] - cplat_isatty の戻り値が 1 であること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_TRUE(actual_ret == 0 ||
                actual_ret == 1); // [確認_正常系] - cplat_isatty の戻り値が 0 または 1 であり、クラッシュしないこと。
#endif /* PLATFORM_ */
}

// stdout の判定が STDOUT_FILENO の isatty 結果になることの確認
TEST_F(isattyTest, stdout_returns_int)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, isatty(_, _, _, STDOUT_FILENO))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - isatty が STDOUT_FILENO で 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
#endif                        /* PLATFORM_LINUX */

    // Act
    int actual_ret =
        cplat_isatty(CPLAT_STREAM_STDOUT); // [手順] - CPLAT_STREAM_STDOUT を渡して cplat_isatty を呼び出す。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_isatty の戻り値が 0 であること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_TRUE(actual_ret == 0 ||
                actual_ret == 1); // [確認_正常系] - cplat_isatty の戻り値が 0 または 1 であり、クラッシュしないこと。
#endif /* PLATFORM_ */
}

// stderr の判定が STDERR_FILENO の isatty 結果になることの確認
TEST_F(isattyTest, stderr_returns_int)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, isatty(_, _, _, STDERR_FILENO))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - isatty が STDERR_FILENO で 1 回呼び出されること。
                              // [Pre-Assert手順] - 1 を返却する。
#endif                        /* PLATFORM_LINUX */

    // Act
    int actual_ret =
        cplat_isatty(CPLAT_STREAM_STDERR); // [手順] - CPLAT_STREAM_STDERR を渡して cplat_isatty を呼び出す。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(1, actual_ret); // [確認_正常系] - cplat_isatty の戻り値が 1 であること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_TRUE(actual_ret == 0 ||
                actual_ret == 1); // [確認_正常系] - cplat_isatty の戻り値が 0 または 1 であり、クラッシュしないこと。
#endif /* PLATFORM_ */
}
