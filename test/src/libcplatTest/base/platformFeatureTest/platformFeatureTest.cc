#include <testfw.h>
#include <cplat/base/platform.h>
#include "platformFeatureProbe.h"

class platformFeatureTest : public Test
{
};

// C の翻訳単位で <cplat/base/platform.h> を先頭に取り込んでも、Linux の _GNU_SOURCE 宣言と Windows の非定義が維持されることの確認
TEST_F(platformFeatureTest, probe_matches_platform_feature_macro_contract)
{
    // Arrange

    // Pre-Assert

    // Act
    int probe_result = platform_feature_probe_has_gnu_dlfcn(); // [手順] - platformFeatureProbe.c のプローブ関数を呼び出す。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(1, probe_result); // [確認_正常系] - Linux で _GNU_SOURCE による Dl_info と RTLD_DEFAULT が有効であること。
#else
    EXPECT_EQ(0, probe_result); // [確認_正常系] - 非 Linux で _GNU_SOURCE が定義されずプローブが 0 を返すこと。
#endif /* PLATFORM_LINUX */
}
