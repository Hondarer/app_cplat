#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/crt/stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

static FILE *call_vfopen_fmt(const char *modes, cplat_error *detail_out, const char *format, ...)
{
    FILE *fp;
    va_list args;

    va_start(args, format);
    fp = cplat_vfopen_fmt(modes, detail_out, format, args);
    va_end(args);

    return fp;
}

class stdioFormatTest : public Test
{
};

// modes が NULL の場合に cplat_fopen を呼び出さず NULL を返すことの確認
TEST_F(stdioFormatTest, test_null_modes)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_error last_error;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_fopen が呼び出されないこと。

    // Act
    FILE *fp = cplat_fopen_fmt(NULL, NULL, "test_%d.txt",
                               1);     // [手順] - modes に NULL を渡して cplat_fopen_fmt を呼び出す。
    cplat_error_get_last(&last_error); // [手順] - TLS に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ((FILE *)NULL, fp);                   // [確認_異常系] - cplat_fopen_fmt から NULL が返されること。
    EXPECT_EQ(1, cplat_error_is_set(&last_error)); // [確認_異常系] - TLS に詳細エラーが記録されること。
}

// va_list 版で modes が NULL の場合に TLS へ詳細エラーが記録されることの確認
TEST_F(stdioFormatTest, vfopen_fmt_records_error_for_null_modes)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_error last_error;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_fopen が呼び出されないこと。

    // Act
    FILE *fp = call_vfopen_fmt(NULL, NULL, "%s", "test.txt"); // [手順] - modes に NULL を指定して va_list 版を呼ぶ。
    cplat_error_get_last(&last_error);                        // [手順] - TLS に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ((FILE *)NULL, fp); // [確認_異常系] - cplat_vfopen_fmt の戻り値が NULL であること。
    EXPECT_EQ(1, cplat_error_is(&last_error,
                                CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - TLS の要因が EINVAL であること。
}

// format が NULL の場合に cplat_fopen を呼び出さず NULL を返すことの確認
TEST_F(stdioFormatTest, test_null_format)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_fopen が呼び出されないこと。

    // Act
    FILE *fp = cplat_fopen_fmt("r", NULL, NULL); // [手順] - format に NULL を渡して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)NULL, fp); // [確認_異常系] - cplat_fopen_fmt から NULL が返されること。
}

// フォーマット結果がバッファー サイズを超える場合に cplat_fopen を呼び出さず NULL を返すことの確認
TEST_F(stdioFormatTest, test_buffer_overflow)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char long_string[5000];
    memset(long_string, 'a', sizeof(long_string) - 1);
    long_string[sizeof(long_string) - 1] = '\0'; // [状態] - バッファー サイズを超える 4999 文字のファイル名を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_fopen が呼び出されないこと。

    // Act
    FILE *fp = cplat_fopen_fmt(
        "w", NULL, "%s.txt",
        long_string); // [手順] - バッファー サイズを超えるファイル名を指定して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)NULL, fp); // [確認_異常系] - cplat_fopen_fmt から NULL が返されること。
}

// フォーマット文字列を展開したファイル名で cplat_fopen が呼び出されることの確認
TEST_F(stdioFormatTest, test_successful_call_with_format)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    FILE *expected_fp =
        (FILE *)(uintptr_t)0x12345678; // [状態] - cplat_fopen が返すファイル ポインターの期待値を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("test_123.txt"), StrEq("r"), _))
        .WillOnce(Return(
            expected_fp)); // [Pre-Assert確認_正常系] - cplat_fopen が展開後のファイル名 "test_123.txt" とモード "r" で 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fopen から expected_fp を返却する。

    // Act
    FILE *fp = cplat_fopen_fmt(
        "r", NULL, "test_%d.txt",
        123); // [手順] - フォーマット文字列 "test_%d.txt" と引数 123 を指定して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ(expected_fp, fp); // [確認_正常系] - cplat_fopen_fmt から expected_fp が返されること。
}

// 複数のフォーマット パラメーターを展開したファイル名で cplat_fopen が呼び出されることの確認
TEST_F(stdioFormatTest, test_successful_call_with_multiple_parameters)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    FILE *expected_fp =
        (FILE *)(uintptr_t)0x87654321; // [状態] - cplat_fopen が返すファイル ポインターの期待値を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("output_1_2_3.txt"), StrEq("w"), _))
        .WillOnce(Return(
            expected_fp)); // [Pre-Assert確認_正常系] - cplat_fopen が展開後のファイル名 "output_1_2_3.txt" とモード "w" で 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_fopen から expected_fp を返却する。

    // Act
    FILE *fp = cplat_fopen_fmt(
        "w", NULL, "output_%d_%d_%d.txt", 1, 2,
        3); // [手順] - フォーマット文字列 "output_%d_%d_%d.txt" と引数 1, 2, 3 を指定して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ(expected_fp, fp); // [確認_正常系] - cplat_fopen_fmt から expected_fp が返されること。
}

// cplat_fopen が NULL を返した場合に NULL を返すことの確認
TEST_F(stdioFormatTest, test_fopen_returns_null)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("nonexistent.txt"), StrEq("r"), _))
        .WillOnce(Return(
            (FILE *)
                NULL)); // [Pre-Assert確認_異常系] - cplat_fopen がファイル名 "nonexistent.txt" とモード "r" で 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fopen から NULL を返却する。

    // Act
    FILE *fp = cplat_fopen_fmt(
        "r", NULL,
        "nonexistent.txt"); // [手順] - 存在しないファイル名 "nonexistent.txt" を指定して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)NULL, fp); // [確認_異常系] - cplat_fopen_fmt から NULL が返されること。
}

#if defined(PLATFORM_WINDOWS)
// 書き込み保護されたファイルで cplat_fopen が NULL を返した場合に NULL を返すことの確認
TEST_F(stdioFormatTest, test_fopen_s_access_denied)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("protected.txt"), StrEq("w"), _))
        .WillOnce(Return(
            (FILE *)
                NULL)); // [Pre-Assert確認_異常系] - cplat_fopen がファイル名 "protected.txt" とモード "w" で 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fopen から NULL を返却する。

    // Act
    FILE *fp = cplat_fopen_fmt(
        "w", NULL,
        "protected.txt"); // [手順] - 書き込み保護されたファイル名 "protected.txt" を指定して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)NULL, fp); // [確認_異常系] - cplat_fopen_fmt から NULL が返されること。
}
#endif /* PLATFORM_WINDOWS */

// cplat_fopen が失敗した場合にエラー コードが取得できることの確認
TEST_F(stdioFormatTest, test_fopen_returns_null_with_errno)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_error error = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND, ENOENT};
    cplat_error error_code; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("nonexistent.txt"), StrEq("r"), _))
        .WillOnce(DoAll(
            SetArgPointee<2>(error),
            Return(
                (FILE *)
                    NULL))); // [Pre-Assert確認_異常系] - cplat_fopen がファイル名 "nonexistent.txt" とモード "r" で 1 回呼び出されること。
                             // [Pre-Assert手順] - cplat_fopen から NULL を返却し、detail_out に ENOENT を設定する。

    // Act
    FILE *fp =
        cplat_fopen_fmt("r", &error_code,
                        "nonexistent.txt"); // [手順] - error_code の受け取り先を指定して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)NULL, fp); // [確認_異常系] - cplat_fopen_fmt から NULL が返されること。
    EXPECT_EQ(1, cplat_error_is(&error_code,
                                CPLAT_CAUSE_NOT_FOUND)); // [確認_異常系] - error_code の要因が NOT_FOUND であること。
}

// cplat_fopen が成功した場合に詳細エラーが空になることの確認
TEST_F(stdioFormatTest, test_fopen_success_clears_error)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    FILE *expected_fp =
        (FILE *)(uintptr_t)0x12345678; // [状態] - cplat_fopen が返すファイル ポインターの期待値を用意する。
    cplat_error empty_error = {CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL};
    cplat_error error = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND, ENOENT};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_fopen(StrEq("success.txt"), StrEq("r"), _))
        .WillOnce(DoAll(
            SetArgPointee<2>(empty_error),
            Return(
                expected_fp))); // [Pre-Assert確認_正常系] - cplat_fopen がファイル名 "success.txt" とモード "r" で 1 回呼び出されること。
                                // [Pre-Assert手順] - detail_out を空にして expected_fp を返却する。

    // Act
    FILE *fp = cplat_fopen_fmt("r", &error,
                               "success.txt"); // [手順] - 詳細エラーの受け取り先を指定して cplat_fopen_fmt を呼び出す。

    // Assert
    EXPECT_EQ(expected_fp, fp);               // [確認_正常系] - cplat_fopen_fmt から expected_fp が返されること。
    EXPECT_EQ(0, cplat_error_is_set(&error)); // [確認_正常系] - 成功時は詳細エラーが空であること。
}

// format が NULL の場合に cplat_remove を呼び出さず -1 を返すことの確認
TEST_F(stdioFormatTest, remove_fmt_rejects_null_format)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_remove(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_remove が呼び出されないこと。

    // Act
    const int result = cplat_remove_fmt(NULL, NULL); // [手順] - format に NULL を指定して cplat_remove_fmt を呼び出す。

    // Assert
    EXPECT_EQ(-1,
              result); // [確認_異常系] - cplat_remove_fmt の戻り値が -1 であること。
}

// フォーマット文字列を展開したファイル名で cplat_remove が呼び出されることの確認
TEST_F(stdioFormatTest, remove_fmt_passes_formatted_path)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("temporary_42.txt"), &detail))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - 展開後のパスで cplat_remove が呼び出されること。

    // Act
    const int result = cplat_remove_fmt(&detail, "temporary_%d.txt",
                                        42); // [手順] - 書式引数 42 を指定して cplat_remove_fmt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_remove_fmt の戻り値が CPLAT_OK であること。
}
