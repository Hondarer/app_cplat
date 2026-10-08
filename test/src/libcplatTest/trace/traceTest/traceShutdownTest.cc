#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/trace/tracer.h>
#include <cplat/trace/tracer_internal.h>

#include "tracer.inject.h"
#include "traceSyncMock.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace
{

class traceShutdownTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
    cplat_shutdown_fn shutdown_callback_ = nullptr;
    void *shutdown_context_ = nullptr;

#if defined(PLATFORM_LINUX)
    cplat_syslog_sink *os_handle_ = reinterpret_cast<cplat_syslog_sink *>(static_cast<uintptr_t>(0x1100));
#elif defined(PLATFORM_WINDOWS)
    cplat_etw_provider *os_handle_ = reinterpret_cast<cplat_etw_provider *>(static_cast<uintptr_t>(0x1100));
#endif

    // [サブ手順 名前=traceShutdownTest.SetUp]
    void SetUp() override
    {
        set_trace_sync_mock_defaults(mock_cplat);
        test_trace_registry_reset_shutdown_state();
        ON_CALL(mock_cplat, cplat_shutdown_register(_, _))
            .WillByDefault(
                [this](cplat_shutdown_fn callback, void *context)
                {
                    shutdown_callback_ = callback;
                    shutdown_context_ = context;
                    return CPLAT_OK;
                });
        // [状態] - `cplat_shutdown_register` の既定動作を設定する。

#if defined(PLATFORM_LINUX)
        ON_CALL(mock_cplat, cplat_syslog_sink_create(_, _)).WillByDefault(Return(os_handle_));
        // [状態] - `cplat_syslog_sink_create` の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_syslog_sink_dispose(_)).WillByDefault(Return());
        // [状態] - `cplat_syslog_sink_dispose` の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_syslog_sink_rename(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - `cplat_syslog_sink_rename` の既定動作を設定する。
#elif defined(PLATFORM_WINDOWS)
        ON_CALL(mock_cplat, cplat_etw_provider_create(_)).WillByDefault(Return(os_handle_));
        // [状態] - `cplat_etw_provider_create` の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_etw_provider_dispose(_)).WillByDefault(Return());
        // [状態] - `cplat_etw_provider_dispose` の既定動作を設定する。
#endif
    }
    // [サブ手順終了]

    // [サブ手順 名前=traceShutdownTest.TearDown]
    void TearDown() override
    {
        // shutdown_started は本来戻らないため、同一バイナリの他テストが tracer を生成できるよう初期状態へ戻す。
        test_trace_registry_reset_shutdown_state();
    }
    // [サブ手順終了]
};

} // namespace

// 共通 shutdown で registry が破棄され、以後の tracer 生成が拒否されることの確認
// [サブ手順参照 名前=traceShutdownTest.SetUp]
TEST_F(traceShutdownTest, shutdown_disposes_registry_and_rejects_new_create)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - tracer を 1 件生成し registry に登録された状態とする。
    ASSERT_NE((cplat_tracer *)NULL, handle);      // [状態確認] - ハンドルが非 NULL であること。
    EXPECT_EQ((size_t)1, cplat_internal_trace_registry_count()); // [状態確認] - registry の登録件数が 1 件であること。

    cplat_shutdown_event event = {CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                                  0}; // [状態] - 通常終了 (NORMAL_EXIT) の shutdown イベントを用意する。

    // Pre-Assert
    ASSERT_NE(nullptr, shutdown_callback_); // [Pre-Assert確認_正常系] - tracer の shutdown callback が登録されること。

    // Act
    shutdown_callback_(&event, shutdown_context_); // [手順] - 登録された tracer shutdown callback を直接呼び出す。
    cplat_tracer *created_after_shutdown = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - shutdown 後に新しい tracer の生成を試みる。

    // Assert
    EXPECT_EQ((size_t)0,
              cplat_internal_trace_registry_count()); // [確認_正常系] - shutdown 後に registry が空になること。
    EXPECT_EQ(
        (cplat_tracer *)NULL,
        created_after_shutdown); // [確認_正常系] - cplat_tracer_create の戻り値として、shutdown 開始後は新規 tracer 作成が拒否され NULL が返ること。
}
// [サブ手順参照 名前=traceShutdownTest.TearDown]
