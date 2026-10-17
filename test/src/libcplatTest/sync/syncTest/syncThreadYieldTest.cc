#include <testfw.h>
#include "syncTestHelper.h"
#if defined(PLATFORM_LINUX)
    #include <mock_unistd.h>
#endif

// 実行権の譲渡で OS の譲渡 API が 1 回呼び出されることの確認
TEST(syncThreadYieldTest, calls_native_yield_once)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    testing::NiceMock<Mock_unistd> mock_unistd;
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd, sched_yield(_, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - sched_yield() が 1 回呼び出されること。
                              // [Pre-Assert手順] - sched_yield() から 0 を返す。
#endif                        /* PLATFORM_LINUX */

    // Act
    cplat_thread_yield(); // [手順] - cplat_thread_yield() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(実行権の譲渡から戻ることの確認)
}
