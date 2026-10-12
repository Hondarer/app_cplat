#include <testfw.h>

#include "atomicTestHelper.h"

// cplat_atomic_thread_fence が、あらゆるメモリ順序で呼び出せることの確認
TEST(atomicFenceTest, thread_fence_can_be_called_with_every_memory_order)
{
    // Arrange

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < kAllMemoryOrderCount; index++)
    {
        cplat_atomic_thread_fence(kAllMemoryOrders[index]); // [手順] - 各メモリ順序でフェンスを呼び出す。
    }

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(すべてのメモリ順序で cplat_atomic_thread_fence() がクラッシュせず完了することの確認)
}
