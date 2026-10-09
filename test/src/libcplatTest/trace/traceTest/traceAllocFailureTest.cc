#include <testfw.h>
#include <mock_cplat.h>
#include "traceSyncMock.h"
#include <cplat/trace/tracer.h>
#include <cplat/trace/tracer_internal.h>

#if defined(PLATFORM_LINUX)
    #include <syslog.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::NiceMock;
using testing::Return;

class traceAllocFailureTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;

#if defined(PLATFORM_LINUX)
    cplat_syslog_sink *os_handle_ = reinterpret_cast<cplat_syslog_sink *>(static_cast<uintptr_t>(0x1100));
#elif defined(PLATFORM_WINDOWS)
    cplat_etw_provider *os_handle_ = reinterpret_cast<cplat_etw_provider *>(static_cast<uintptr_t>(0x1100));
#endif /* PLATFORM_ */

    // [サブ手順 名前=traceAllocFailureTest.SetUp]
    void SetUp() override
    {
        set_trace_sync_mock_defaults(mock_cplat);
        ON_CALL(mock_cplat, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - `cplat_shutdown_register` の既定動作を設定する。
#if defined(PLATFORM_LINUX)
        ON_CALL(mock_cplat, cplat_syslog_sink_create(_, _)).WillByDefault(Return(os_handle_));
        // [状態] - `cplat_syslog_sink_create` の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_syslog_sink_dispose(_)).WillByDefault(Return());
        // [状態] - `cplat_syslog_sink_dispose` の既定動作を設定する。
#elif defined(PLATFORM_WINDOWS)
        ON_CALL(mock_cplat, cplat_etw_provider_create(_)).WillByDefault(Return(os_handle_));
        // [状態] - `cplat_etw_provider_create` の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_etw_provider_dispose(_)).WillByDefault(Return());
        // [状態] - `cplat_etw_provider_dispose` の既定動作を設定する。
#endif /* PLATFORM_ */
    }
    // [サブ手順終了]
};

// ハンドルの確保に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=traceAllocFailureTest.SetUp]
TEST_F(traceAllocFailureTest, create_returns_null_when_handle_allocation_fails)
{
    // Arrange
    // Pre-Assert
    /* cplat_tracer は不透明型でサイズを指定できない。生成時の malloc はハンドル確保の 1 回だけである */
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc がハンドル確保のために 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc から NULL を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_cplat, cplat_syslog_sink_dispose(os_handle_))
        .Times(1); // [Pre-Assert確認_異常系] - 確保済みの syslog sink が 1 回破棄されること。
#endif             /* PLATFORM_LINUX */

    // Act
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - cplat_tracer_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_tracer *)NULL,
              handle);                            // [確認_異常系] - cplat_tracer_create の戻り値が NULL であること。
    EXPECT_EQ((size_t)0, cplat_internal_trace_registry_count()); // [確認_異常系] - registry へ登録されないこと。
}

#if defined(PLATFORM_LINUX)

// インスタンス名の複製に失敗した場合に生成が失敗することの確認
// Windows の cplat_tracer_create は _strdup を使うため、この失敗経路は Linux のみに存在する
// [サブ手順参照 名前=traceAllocFailureTest.SetUp]
TEST_F(traceAllocFailureTest, create_returns_null_when_name_duplication_fails)
{
    // Arrange
    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_strdup(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_strdup が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_strdup から NULL を返却する。
    EXPECT_CALL(mock_cplat, cplat_syslog_sink_dispose(os_handle_))
        .Times(1); // [Pre-Assert確認_異常系] - 確保済みの syslog sink が 1 回破棄されること。

    // Act
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - cplat_tracer_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_tracer *)NULL,
              handle);                            // [確認_異常系] - cplat_tracer_create の戻り値が NULL であること。
    EXPECT_EQ((size_t)0, cplat_internal_trace_registry_count()); // [確認_異常系] - registry へ登録されないこと。
}

#endif /* PLATFORM_LINUX */

// レジストリの拡張に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=traceAllocFailureTest.SetUp]
TEST_F(traceAllocFailureTest, create_returns_null_when_registry_expansion_fails)
{
    // Arrange
    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc がレジストリの拡張のために 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - cplat_tracer_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_tracer *)NULL,
              handle);                            // [確認_異常系] - cplat_tracer_create の戻り値が NULL であること。
    EXPECT_EQ((size_t)0, cplat_internal_trace_registry_count()); // [確認_異常系] - registry が空のままであること。
}

// インスタンス識別付きの名前組み立てで確保に失敗した場合に設定が失敗することの確認
// [サブ手順参照 名前=traceAllocFailureTest.SetUp]
TEST_F(traceAllocFailureTest, set_name_fails_when_effective_name_allocation_fails)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。

    ASSERT_NE((cplat_tracer *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が名前組み立て用に 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    int actual_ret =
        cplat_tracer_set_name(handle, "sample",
                                 42); // [手順] - インスタンス識別 42 を指定して cplat_tracer_set_name を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_tracer_set_name の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。

    // Cleanup
    cplat_tracer_dispose(&handle);
}
