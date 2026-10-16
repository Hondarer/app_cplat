#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <TraceLoggingProvider.h>
    #include <testfw.h>
    #include <mock_cplat.h>
    #include <cplat/trace/etw.h>

CPLAT_ETW_DEFINE_PROVIDER(s_test_provider, "TraceEtwTest",
                          (0x62ab1ccc, 0x5fc6, 0x4e1e, 0x82, 0x60, 0x9e, 0xa2, 0x77, 0x2a, 0xfe, 0x5e));

class trace_etwTest : public Test
{
};

// プロバイダーを登録し、有効なハンドルが返されることの確認
TEST_F(trace_etwTest, test_init_and_dispose)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_etw_provider *handle = cplat_etw_provider_create(s_test_provider); // [手順] - ETW provider を登録する。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// INFO レベルで書き込みが成功することの確認
TEST_F(trace_etwTest, test_write_returns_zero)
{
    // Arrange
    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    int result = cplat_etw_provider_write(handle, 4, nullptr,
                                          "test message"); // [手順] - INFO レベル (4) で "test message" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_etw_provider_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 全レベルで書き込みが成功することの確認
TEST_F(trace_etwTest, test_write_all_levels)
{
    // Arrange
    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    // [手順] - CRITICAL、ERROR、WARNING、INFO、VERBOSE の各レベルで書き込む。
    int actual_ret_critical = cplat_etw_provider_write(handle, 1, nullptr, "critical");
    int actual_ret_error = cplat_etw_provider_write(handle, 2, nullptr, "error");
    int actual_ret_warning = cplat_etw_provider_write(handle, 3, nullptr, "warning");
    int actual_ret_info = cplat_etw_provider_write(handle, 4, nullptr, "info");
    int actual_ret_verbose = cplat_etw_provider_write(handle, 5, nullptr, "verbose");

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_critical); // [確認_正常系] - CRITICAL レベルでの cplat_etw_provider_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_error); // [確認_正常系] - ERROR レベルでの cplat_etw_provider_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_warning); // [確認_正常系] - WARNING レベルでの cplat_etw_provider_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_info); // [確認_正常系] - INFO レベルでの cplat_etw_provider_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_verbose); // [確認_正常系] - VERBOSE レベルでの cplat_etw_provider_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// NULL 引数が安全に無視されることの確認
TEST_F(trace_etwTest, test_null_arguments_are_safe)
{
    // Arrange
    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。

    // Pre-Assert

    // Act
    cplat_etw_provider_dispose(nullptr); // [手順] - NULL ハンドルで dispose() を呼び出す。

    cplat_etw_provider *actual_ret_create =
        cplat_etw_provider_create(nullptr); // [手順] - NULL provider_ref で create() を呼び出す。
    int actual_ret_null_handle =
        cplat_etw_provider_write(nullptr, 4, nullptr, "test message"); // [手順] - NULL ハンドルで write() を呼び出す。
    int actual_ret_null_message =
        cplat_etw_provider_write(handle, 4, nullptr, nullptr); // [手順] - NULL message で write() を呼び出す。

    // Assert
    EXPECT_EQ(
        nullptr,
        actual_ret_create); // [確認_異常系] - NULL provider_ref に対する cplat_etw_provider_create() の戻り値が nullptr であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_null_handle); // [確認_異常系] - NULL ハンドルに対する cplat_etw_provider_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_null_message); // [確認_異常系] - NULL message に対する cplat_etw_provider_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

#endif /* PLATFORM_ */
