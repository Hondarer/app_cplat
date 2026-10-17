#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>

class consoleAttachTest : public Test
{
};

// 親コンソールへの接続指定がない場合に出力フラグが 0 になることの確認
TEST_F(consoleAttachTest, reports_not_attached_without_takeover_option)
{
    // Arrange
    int argc = 1;
    char program[] = "consoleAttachTest";
    char *argv[] = {program, nullptr};
    int attached = 1; // [状態] - 未接続への書き換えを確認するため 1 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_console_attach_parent(
        &argc, argv, &attached); // [手順] - 引き継ぎ指定のない引数を指定して cplat_console_attach_parent() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_attach_parent() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, attached);          // [確認_正常系] - attached が 0 であること。
    EXPECT_EQ(1, argc);              // [確認_正常系] - argc が 1 であること。(引数の個数が変化しないことの確認)
}

// attached_out に NULL を渡してもクラッシュしないことの確認
TEST_F(consoleAttachTest, accepts_null_attached_out)
{
    // Arrange
    int argc = 1;
    char program[] = "consoleAttachTest";
    char *argv[] = {program, nullptr};

    // Pre-Assert

    // Act
    int actual_ret = cplat_console_attach_parent(
        &argc, argv,
        nullptr); // [手順] - attached_out に nullptr を指定して cplat_console_attach_parent() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_attach_parent() の戻り値が CPLAT_OK であること。
}
