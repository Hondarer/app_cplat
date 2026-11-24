#include <testfw.h>
#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <cplat/crt/sys/stat.h>
#include <cplat/crt/wchar_conv.h>
#include <mock_cplat.h>
    #include <mock_direct.h>
    #include <sys/mock_stat.h>

#include <cstdarg>
#include <cerrno>

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::StrEq;

static int call_vstat_fmt(cplat_file_stat_t *stat_out, cplat_error *detail_out, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vstat_fmt(stat_out, detail_out, format, args);
    va_end(args);
    return result;
}

static int call_vmkdir_fmt(cplat_error *detail_out, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vmkdir_fmt(detail_out, format, args);
    va_end(args);
    return result;
}

TEST(pathConversionTest, sys_stat_path_apis_report_invalid_encoding_and_eilseq)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_direct> mock_direct;
    NiceMock<Mock_sys_stat> mock_sys_stat;
    const char invalid_path[] = "\x80";
    cplat_file_stat_t stat_out = {};
    cplat_error details[8] = {};
    int results[8] = {};
    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_utf8_to_wpath(_, _, _, StrEq(invalid_path)))
        .Times(8)
        .WillRepeatedly(Invoke([](wchar_t *dest, size_t dest_size, size_t *count_out, const char *)
                               {
                                   if (dest != nullptr && dest_size > 0U)
                                   {
                                       dest[0] = L'\0';
                                   }
                                   if (count_out != nullptr)
                                   {
                                       *count_out = 0U;
                                   }
                                   return CPLAT_ERR_INVALID_ENCODING;
                               })); // [Pre-Assert確認_正常系] - 8 個の公開 API が不正パスを 1 回ずつ変換すること。
                                     // [Pre-Assert手順] - cplat_utf8_to_wpath() から CPLAT_ERR_INVALID_ENCODING を返す。
    EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 不正な UTF-8 のとき _wstat64() が呼び出されないこと。
    EXPECT_CALL(mock_direct, wmkdir(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 不正な UTF-8 のとき _wmkdir() が呼び出されないこと。
    EXPECT_CALL(mock_direct, wrmdir(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 不正な UTF-8 のとき _wrmdir() が呼び出されないこと。

    // Act
    results[0] = cplat_stat(&stat_out, &details[0], invalid_path);
    // [手順] - 不正な UTF-8 パスで cplat_stat() を呼び出す。
    results[1] = cplat_stat_fmt(&stat_out, &details[1], "%s", invalid_path);
    // [手順] - 不正な UTF-8 パスで cplat_stat_fmt() を呼び出す。
    results[2] = call_vstat_fmt(&stat_out, &details[2], "%s", invalid_path);
    // [手順] - 不正な UTF-8 パスで cplat_vstat_fmt() を呼び出す。
    results[3] = cplat_rmdir(invalid_path, &details[3]);
    // [手順] - 不正な UTF-8 パスで cplat_rmdir() を呼び出す。
    results[4] = cplat_mkdir(invalid_path, &details[4]);
    // [手順] - 不正な UTF-8 パスで cplat_mkdir() を呼び出す。
    results[5] = cplat_mkdir_fmt(&details[5], "%s", invalid_path);
    // [手順] - 不正な UTF-8 パスで cplat_mkdir_fmt() を呼び出す。
    results[6] = call_vmkdir_fmt(&details[6], "%s", invalid_path);
    // [手順] - 不正な UTF-8 パスで cplat_vmkdir_fmt() を呼び出す。
    results[7] = cplat_makedirs(invalid_path, &details[7]);
    // [手順] - 不正な UTF-8 パスで cplat_makedirs() を呼び出す。

    // Assert
    for (size_t index = 0U; index < 8U; index++)
    {
        EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, results[index]); // [確認_異常系] - 8 個の対象関数が CPLAT_ERR_INVALID_ENCODING を返すこと。
        EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, cplat_error_to_result(&details[index])); // [確認_異常系] - 詳細エラーの結果コードが CPLAT_ERR_INVALID_ENCODING であること。
        EXPECT_EQ(CPLAT_ERROR_DOMAIN_ERRNO, cplat_error_get_domain(&details[index])); // [確認_異常系] - 詳細エラーの domain が errno であること。
        EXPECT_EQ(EILSEQ, cplat_error_get_errno(&details[index])); // [確認_異常系] - 詳細 errno が EILSEQ であること。
        EXPECT_EQ(CPLAT_CAUSE_OTHER, cplat_error_get_cause(&details[index])); // [確認_異常系] - 詳細エラーの要因が CPLAT_CAUSE_OTHER であること。
    }
}

// cplat_makedirs の中間階層に不正な UTF-8 を含む場合も OS API を呼ばず CPLAT_ERR_INVALID_ENCODING を返すことの確認
TEST(pathConversionTest, makedirs_intermediate_invalid_encoding_skips_os_calls)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_direct> mock_direct;
    NiceMock<Mock_sys_stat> mock_sys_stat;
    const char invalid_nested[] = "valid/\x80/leaf";
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_utf8_to_wpath(_, _, _, StrEq(invalid_nested)))
        .WillOnce(Invoke([](wchar_t *dest, size_t dest_size, size_t *count_out, const char *)
                         {
                             if (dest != nullptr && dest_size > 0U)
                             {
                                 dest[0] = L'\0';
                             }
                             if (count_out != nullptr)
                             {
                                 *count_out = 0U;
                             }
                             return CPLAT_ERR_INVALID_ENCODING;
                         }));
    EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, _, _)).Times(0);
    EXPECT_CALL(mock_direct, wmkdir(_, _, _, _)).Times(0);

    // Act
    int rc = cplat_makedirs(invalid_nested, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, rc);
    EXPECT_EQ(EILSEQ, cplat_error_get_errno(&detail));
    EXPECT_EQ(CPLAT_CAUSE_OTHER, cplat_error_get_cause(&detail));
}

#endif /* PLATFORM_WINDOWS */
