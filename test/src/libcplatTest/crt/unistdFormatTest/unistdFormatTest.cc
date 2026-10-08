#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/base/result.h>
#include <cplat/crt/unistd.h>

#include <errno.h>
#include <stdarg.h>

using namespace testing;

class unistdFormatTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
};

static int call_vaccess_fmt(int mode, cplat_error *detail_out, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int actual_ret = cplat_vaccess_fmt(mode, detail_out, format, args);
    va_end(args);

    return actual_ret;
}

// 書式で組み立てたパスが cplat_open へ渡されることの確認
TEST_F(unistdFormatTest, passes_formatted_path_to_open)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_access(StrEq("/tmp/sample_42.txt"), CPLAT_ACCESS_FMT_F_OK, &detail))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_open が展開後のパス "/tmp/sample_42.txt" を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_open から成功を示す 0 を返却する。

    // Act
    int actual_ret = cplat_access_fmt(CPLAT_ACCESS_FMT_F_OK, &detail, "/tmp/sample_%d.txt",
                                      42); // [手順] - 書式引数 42 を指定して cplat_access_fmt を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_access_fmt の戻り値が cplat_open の戻り値 7 であること。
}

// va_list 版が同じ経路を通ることの確認
TEST_F(unistdFormatTest, vaccess_fmt_passes_formatted_path_to_open)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_access(StrEq("/tmp/sample_7.txt"), CPLAT_ACCESS_FMT_F_OK, &detail))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_open が展開後のパス "/tmp/sample_7.txt" を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_open から成功を示す 0 を返却する。

    // Act
    int actual_ret = call_vaccess_fmt(CPLAT_ACCESS_FMT_F_OK, &detail, "/tmp/sample_%d.txt",
                                      7); // [手順] - 書式引数 7 を指定して cplat_vaccess_fmt を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_vaccess_fmt の戻り値が cplat_open の戻り値 3 であること。
}

// 書式展開に失敗した場合に cplat_open を呼ばずに -1 を返すことの確認
TEST_F(unistdFormatTest, returns_minus1_without_open_when_format_fails)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_access(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_open が呼び出されないこと。

    // Act
    int actual_ret = cplat_access_fmt(CPLAT_ACCESS_FMT_F_OK, &detail,
                                      NULL); // [手順] - 書式文字列に NULL を指定して cplat_access_fmt を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - cplat_access_fmt の戻り値が -1 であること。
    EXPECT_EQ(EINVAL,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno の戻り値が EINVAL であること。
}
