#include <testfw.h>
#include <mock_cplat.h>
#include "prompt_platform.h"

namespace
{
int size_result;
int size_cols;
int size_rows;
int enter_result;
int leave_results[2];
int leave_count;
int enter_count;
} // namespace

extern "C" int prompt_platform_os_get_size(int *cols, int *rows)
{
    *cols = size_cols;
    *rows = size_rows;
    return size_result;
}
extern "C" int prompt_platform_os_enter_raw(prompt_platform_context *)
{
    enter_count++;
    return enter_result;
}
extern "C" int prompt_platform_os_leave_raw(prompt_platform_context *)
{
    return leave_results[leave_count++];
}

class promptPlatformTest : public Test
{
  protected:
    // [サブ手順 名前=promptPlatformTest.SetUp]
    void SetUp() override
    {
        size_result = 0;
        size_cols = 120;
        size_rows = 40;
        enter_result = 0;
        leave_results[0] = 0;
        leave_results[1] = 0;
        leave_count = 0;
        enter_count = 0;
    }
    // [サブ手順終了]
};

// 正常寸法と各寸法の不正値を独立に扱うことの確認
// [サブ手順参照 名前=promptPlatformTest.SetUp]
TEST_F(promptPlatformTest, dimensions_are_validated_independently)
{
    // Arrange
    const int values[][4] = {{120, 40, 120, 40}, {0, 40, 80, 40}, {120, 0, 120, 24}, {-1, -1, 80, 24}};
    // Pre-Assert
    // Act / Assert
    for (const auto &value : values)
    {
        size_cols = value[0];
        size_rows = value[1];
        int cols;
        int rows;
        prompt_platform_get_size(&cols, &rows); // [手順] - 表の寸法を正規化する。4 回実施。
        EXPECT_EQ(value[2], cols);              // [確認_正常系] - 幅が期待値になること。4 件。
        EXPECT_EQ(value[3], rows);              // [確認_正常系] - 高さが期待値になること。4 件。
    }
}

// OS 取得失敗時に両寸法を既定値にすることの確認
// [サブ手順参照 名前=promptPlatformTest.SetUp]
TEST_F(promptPlatformTest, failed_size_query_uses_both_defaults)
{
    // Arrange
    size_result = -1;
    int cols;
    int rows;
    // Pre-Assert
    // Act
    prompt_platform_get_size(&cols, &rows); // [手順] - 取得失敗を正規化する。
    // Assert
    EXPECT_EQ(80, cols); // [確認_異常系] - 幅が既定値になること。
    EXPECT_EQ(24, rows); // [確認_異常系] - 高さが既定値になること。
}

// 復元・移行・ロールバックの結果を分類することの確認
// [サブ手順参照 名前=promptPlatformTest.SetUp]
TEST_F(promptPlatformTest, acquisition_distinguishes_fallback_and_unrestored_state)
{
    // Arrange
    const int cases[][6] = {{0, 0, 0, 0, 1, 1}, {-1, 0, 0, -1, 0, 1}, {0, -1, 0, 1, 1, 2}, {0, -1, -1, -1, 1, 2}};
    // Pre-Assert
    // Act / Assert
    for (const auto &value : cases)
    {
        prompt_platform_context context = {};
        leave_results[0] = value[0];
        enter_result = value[1];
        leave_results[1] = value[2];
        leave_count = 0;
        enter_count = 0;
        int result = prompt_platform_enter_raw(&context); // [手順] - 各取得結果を注入する。4 回実施。
        EXPECT_EQ(value[3], result);      // [確認_正常系] - 成功、切替可能、復元未完を分類すること。4 件。
        EXPECT_EQ(value[4], enter_count); // [確認_正常系] - 復元未完なら設定を上書きしないこと。4 件。
        EXPECT_EQ(value[5], leave_count); // [確認_正常系] - 必要な復元だけを試みること。4 件。
    }
}

// 標準入出力の双方を端末判定することの確認
// [サブ手順参照 名前=promptPlatformTest.SetUp]
TEST_F(promptPlatformTest, tty_requires_both_streams)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    // Pre-Assert
    EXPECT_CALL(mock, cplat_isatty(_))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .WillOnce(Return(1))
        .WillOnce(Return(1));
    // [Pre-Assert確認_正常系] - 入力非端末、出力非端末、両方端末の順に 5 回判定すること。
    // Act
    int input_not_tty = prompt_platform_is_tty();  // [手順] - 入力が非端末の結果を取得する。
    int output_not_tty = prompt_platform_is_tty(); // [手順] - 出力が非端末の結果を取得する。
    int both = prompt_platform_is_tty();           // [手順] - 両方端末の結果を取得する。
    // Assert
    EXPECT_EQ(0, input_not_tty);  // [確認_正常系] - 入力非端末なら無効であること。
    EXPECT_EQ(0, output_not_tty); // [確認_正常系] - 出力非端末なら無効であること。
    EXPECT_EQ(1, both);           // [確認_正常系] - 両方端末なら有効であること。
}
