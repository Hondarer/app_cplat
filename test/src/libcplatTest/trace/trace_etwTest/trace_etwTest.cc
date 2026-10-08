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
    EXPECT_NE((cplat_etw_provider *)NULL, handle); // [確認_正常系] - ハンドルが NULL でないこと。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// INFO レベルで書き込みが成功することの確認
TEST_F(trace_etwTest, test_write_returns_zero)
{
    // Arrange
    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle);  // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int result = cplat_etw_provider_write(handle, 4, NULL,
                                          "test message"); // [手順] - INFO レベル (4) で "test message" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_etw_provider_write の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 全レベルで書き込みが成功することの確認
TEST_F(trace_etwTest, test_write_all_levels)
{
    // Arrange
    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle);  // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    // [手順] - CRITICAL、ERROR、WARNING、INFO、VERBOSE の各レベルで書き込む。
    int actual_ret_critical = cplat_etw_provider_write(handle, 1, NULL, "critical");
    int actual_ret_error = cplat_etw_provider_write(handle, 2, NULL, "error");
    int actual_ret_warning = cplat_etw_provider_write(handle, 3, NULL, "warning");
    int actual_ret_info = cplat_etw_provider_write(handle, 4, NULL, "info");
    int actual_ret_verbose = cplat_etw_provider_write(handle, 5, NULL, "verbose");

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_critical); // [確認_正常系] - CRITICAL レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_error);    // [確認_正常系] - ERROR レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_warning);  // [確認_正常系] - WARNING レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_info);     // [確認_正常系] - INFO レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_verbose);  // [確認_正常系] - VERBOSE レベルで書き込めること。

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
    cplat_etw_provider_dispose(NULL); // [手順] - NULL ハンドルで dispose を呼び出す。

    // Assert
    EXPECT_EQ(
        (cplat_etw_provider *)NULL,
        cplat_etw_provider_create(
            NULL)); // [確認_異常系] - cplat_etw_provider_create の戻り値から、NULL provider_ref で create が失敗したと判断できること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_etw_provider_write(
            NULL, 4, NULL,
            "test message")); // [確認_異常系] - NULL ハンドルに対する cplat_etw_provider_write の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_etw_provider_write(
            handle, 4, NULL,
            NULL)); // [確認_異常系] - NULL message に対する cplat_etw_provider_write の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

#endif /* PLATFORM_ */
