#include <testfw.h>
#include <mock_cplat.h>
#include <mock_stdio.h>

#include <cplat/base/platform.h>
#include <cplat/crt/stdio.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#if defined(PLATFORM_LINUX)
    #include <sys/types.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

// fopen の OS エラーを詳細エラーへ記録することの確認
TEST(stdioFailureInjectionTest, fopen_reports_mocked_os_failure)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_stdio, fopen(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const char *, const char *)
            {
                errno = EACCES;
                return nullptr;
            })); // [Pre-Assert確認_異常系] - fopen() が 1 回呼び出されること。
                 // [Pre-Assert手順] - fopen() から、errno に EACCES を設定して nullptr を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _wfsopen(_, _, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const wchar_t *, const wchar_t *, int)
            {
                errno = EACCES;
                return nullptr;
            })); // [Pre-Assert確認_異常系] - _wfsopen() が 1 回呼び出されること。
                 // [Pre-Assert手順] - _wfsopen() から、errno に EACCES を設定して nullptr を返す。
#endif /* PLATFORM_ */

    // Act
    FILE *stream = cplat_fopen("mocked-failure.txt", "rb",
                               &detail); // [手順] - fopen() が EACCES で失敗する状態で cplat_fopen() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              stream); // [確認_異常系] - cplat_fopen() の戻り値が nullptr であること。
    EXPECT_EQ(EACCES,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EACCES であること。
}

// fopen の NULL 引数が不正引数として分類されることの確認
TEST(stdioFailureInjectionTest, fopen_rejects_null_arguments)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert

    // Act
    FILE *null_path =
        cplat_fopen(nullptr, "rb", &detail); // [手順] - path に nullptr を指定して cplat_fopen() を呼び出す。
    FILE *null_modes =
        cplat_fopen("file", nullptr, &detail); // [手順] - modes に nullptr を指定して cplat_fopen() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              null_path); // [確認_異常系] - path に nullptr を指定した cplat_fopen() の戻り値が nullptr であること。
    EXPECT_EQ(nullptr,
              null_modes); // [確認_異常系] - modes に nullptr を指定した cplat_fopen() の戻り値が nullptr であること。
    EXPECT_EQ(
        CPLAT_CAUSE_INVALID_ARGUMENT,
        cplat_error_get_cause(
            &detail)); // [確認_異常系] - cplat_error_get_cause() の戻り値が CPLAT_CAUSE_INVALID_ARGUMENT であること。
}

// fopen の成功時に詳細エラーがクリアされることの確認
TEST(stdioFailureInjectionTest, fopen_reports_success_and_clears_detail)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    FILE *opened = reinterpret_cast<FILE *>(static_cast<uintptr_t>(19));
    cplat_error detail = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND,
                          ENOENT}; // [状態] - 詳細エラーへあらかじめ ENOENT を設定する。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_stdio, fopen(_, _, _, StrEq("opened.txt"), StrEq("rb")))
        .WillOnce(Return(
            opened)); // [Pre-Assert確認_正常系] - fopen() が "opened.txt" と等しい文字列、"rb" と等しい文字列を指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - fopen() から、センチネル ストリームを返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _wfsopen(_, _, _, _, _, _))
        .WillOnce(Return(opened)); // [Pre-Assert確認_正常系] - _wfsopen() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - _wfsopen() から、センチネル ストリームを返す。
#endif /* PLATFORM_ */

    // Act
    FILE *stream =
        cplat_fopen("opened.txt", "rb", &detail); // [手順] - fopen() が成功する状態で cplat_fopen() を呼び出す。

    // Assert
    EXPECT_EQ(opened, stream); // [確認_正常系] - cplat_fopen() の戻り値が opened と一致すること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_error_to_result(
            &detail)); // [確認_正常系] - cplat_error_to_result() の戻り値が CPLAT_OK であること。(成功時に詳細エラーがクリアされたことの確認)
}

// fclose の失敗時に EIO を補完して記録することの確認
TEST(stdioFailureInjectionTest, fclose_reports_eio_when_errno_is_empty)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_error detail = {};
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(1));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fclose(_, _, _, stream))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fclose() が stream を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fclose() から EOF を返す。

    // Act
    int result = cplat_fclose(stream, &detail); // [手順] - fclose() が EOF を返す状態で cplat_fclose() を呼び出す。

    // Assert
    EXPECT_EQ(EOF, result); // [確認_異常系] - cplat_fclose() の戻り値が EOF であること。
    EXPECT_EQ(
        EIO,
        cplat_error_get_errno(
            &detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EIO であること。(errno が空の失敗では EIO が記録されたことの確認)
}

// fclose の NULL 引数が不正引数として分類されることの確認
TEST(stdioFailureInjectionTest, fclose_rejects_null_stream)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert

    // Act
    int result = cplat_fclose(nullptr, &detail); // [手順] - stream に nullptr を指定して cplat_fclose() を呼び出す。

    // Assert
    EXPECT_EQ(EOF, result); // [確認_異常系] - stream に nullptr を指定した cplat_fclose() の戻り値が EOF であること。
    EXPECT_EQ(
        CPLAT_CAUSE_INVALID_ARGUMENT,
        cplat_error_get_cause(
            &detail)); // [確認_異常系] - cplat_error_get_cause() の戻り値が CPLAT_CAUSE_INVALID_ARGUMENT であること。
}

// fclose の成功時に詳細エラーがクリアされることの確認
TEST(stdioFailureInjectionTest, fclose_reports_success_and_clears_detail)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(20));
    cplat_error detail = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND,
                          ENOENT}; // [状態] - 詳細エラーへあらかじめ ENOENT を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fclose(_, _, _, stream))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - fclose() が stream を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - fclose() から 0 を返す。

    // Act
    int result = cplat_fclose(stream, &detail); // [手順] - fclose() が成功する状態で cplat_fclose() を呼び出す。

    // Assert
    EXPECT_EQ(0, result); // [確認_正常系] - cplat_fclose() の戻り値が 0 であること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_error_to_result(
            &detail)); // [確認_正常系] - cplat_error_to_result() の戻り値が CPLAT_OK であること。(成功時に詳細エラーがクリアされたことの確認)
}

// fflush の失敗時に EIO を補完して記録することの確認
TEST(stdioFailureInjectionTest, fflush_reports_eio_when_errno_is_empty)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_error detail = {};
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(2));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fflush(_, _, _, stream))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stream を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    int result = cplat_fflush(stream, &detail); // [手順] - fflush() が EOF を返す状態で cplat_fflush() を呼び出す。

    // Assert
    EXPECT_EQ(EOF, result); // [確認_異常系] - cplat_fflush() の戻り値が EOF であること。
    EXPECT_EQ(
        EIO,
        cplat_error_get_errno(
            &detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EIO であること。(errno が空の失敗では EIO が記録されたことの確認)
}

// fclose が errno を保持している失敗を詳細エラーへ記録することの確認
TEST(stdioFailureInjectionTest, fclose_preserves_nonzero_errno)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_error detail = {};
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(4));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fclose(_, _, _, stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, FILE *)
            {
                errno = EACCES;
                return EOF;
            })); // [Pre-Assert確認_異常系] - fclose() が stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - fclose() から、errno に EACCES を設定して EOF を返す。

    // Act
    int result = cplat_fclose(
        stream, &detail); // [手順] - fclose() が errno に EACCES を設定して失敗する状態で cplat_fclose() を呼び出す。

    // Assert
    EXPECT_EQ(EOF, result); // [確認_異常系] - cplat_fclose() の戻り値が EOF であること。
    EXPECT_EQ(EACCES,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EACCES であること。
}

// fflush が errno を保持している失敗を詳細エラーへ記録することの確認
TEST(stdioFailureInjectionTest, fflush_preserves_nonzero_errno)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_error detail = {};
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(5));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fflush(_, _, _, stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, FILE *)
            {
                errno = EACCES;
                return EOF;
            })); // [Pre-Assert確認_異常系] - fflush() が stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - fflush() から、errno に EACCES を設定して EOF を返す。

    // Act
    int result = cplat_fflush(
        stream, &detail); // [手順] - fflush() が errno に EACCES を設定して失敗する状態で cplat_fflush() を呼び出す。

    // Assert
    EXPECT_EQ(EOF, result); // [確認_異常系] - cplat_fflush() の戻り値が EOF であること。
    EXPECT_EQ(EACCES,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EACCES であること。
}

// fflush の成功時に詳細エラーがクリアされることの確認
TEST(stdioFailureInjectionTest, fflush_reports_success_for_valid_stream)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(13));
    cplat_error detail = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND,
                          ENOENT}; // [状態] - 詳細エラーへあらかじめ ENOENT を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fflush(_, _, _, stream))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - fflush() が stream を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - fflush() から 0 を返す。

    // Act
    int result = cplat_fflush(stream, &detail); // [手順] - センチネル ストリームを指定して cplat_fflush() を呼び出す。

    // Assert
    EXPECT_EQ(0, result); // [確認_正常系] - cplat_fflush() の戻り値が 0 であること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_error_to_result(
            &detail)); // [確認_正常系] - cplat_error_to_result() の戻り値が CPLAT_OK であること。(成功時に詳細エラーがクリアされたことの確認)
}

// freopen の OS 失敗が詳細エラーへ記録されることの確認
TEST(stdioFailureInjectionTest, freopen_reports_os_failure)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(14));
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_stdio, freopen(_, _, _, StrEq("/cplat/path/does/not/exist"), StrEq("rb"), stream))
        .WillOnce(
            [](const char *, int, const char *, const char *, const char *, FILE *)
            {
                errno = ENOENT;
                return nullptr;
            }); // [Pre-Assert確認_異常系] - freopen() が "/cplat/path/does/not/exist" と等しい文字列、"rb" と等しい文字列、stream を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - freopen() から、errno に ENOENT を設定して nullptr を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _wfsopen(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, const wchar_t *, const wchar_t *, int)
            {
                errno = ENOENT;
                return nullptr;
            }); // [Pre-Assert確認_異常系] - _wfsopen() が 1 回呼び出されること。
                // [Pre-Assert手順] - _wfsopen() から、errno に ENOENT を設定して nullptr を返す。
#endif /* PLATFORM_ */

    // Act
    FILE *reopened = cplat_freopen("/cplat/path/does/not/exist", "rb", stream,
                                   &detail); // [手順] - 存在しないパスを指定して cplat_freopen() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              reopened); // [確認_異常系] - cplat_freopen() の戻り値が nullptr であること。
    EXPECT_NE(
        CPLAT_OK,
        cplat_error_to_result(&detail)); // [確認_異常系] - cplat_error_to_result() の戻り値が CPLAT_OK 以外であること。
}

// fread と fwrite が引数、全量、短い入出力を分類することの確認
TEST(stdioFailureInjectionTest, fread_and_fwrite_classify_arguments_and_counts)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(15));
    char data[2] = {};
    cplat_error read_detail = {};
    cplat_error write_detail = {};
    ON_CALL(mock_stdio, ferror(_, _, _, stream))
        .WillByDefault(Return(0)); // [状態] - ferror() から 0 を返すように既定動作を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fread(_, _, _, _, 0u, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - サイズに 0u を指定して fread() が呼び出されないこと。
    EXPECT_CALL(mock_stdio, fread(_, _, _, _, 1u, 1u, stream))
        .WillOnce(Return(1u))
        .WillOnce(Return(0u)); // [Pre-Assert確認_正常系] - fread() が 1u、1u、stream を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - fread() から 1 回目は全量の 1 件、2 回目は短い読み込みの 0 件を返す。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, 0u, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - サイズに 0u を指定して fwrite() が呼び出されないこと。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, 1u, 1u, stream))
        .WillOnce(Return(1u))
        .WillOnce(Return(0u)); // [Pre-Assert確認_正常系] - fwrite() が 1u、1u、stream を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - fwrite() から 1 回目は全量の 1 件、2 回目は短い書き込みの 0 件を返す。
    EXPECT_CALL(mock_stdio, fread(_, _, _, _, _, 0u, _))
        .Times(0); // [Pre-Assert確認_正常系] - 要素数に 0u を指定して fread() が呼び出されないこと。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, _, 0u, _))
        .Times(0); // [Pre-Assert確認_正常系] - 要素数に 0u を指定して fwrite() が呼び出されないこと。

    // Act
    size_t read_null_buffer = cplat_fread(
        nullptr, 1u, 1u, stream, &read_detail); // [手順] - nullptr の読み込み先を指定して cplat_fread() を呼び出す。
    size_t read_null_stream = cplat_fread(
        data, 1u, 1u, nullptr, &read_detail); // [手順] - nullptr のストリームを指定して cplat_fread() を呼び出す。
    size_t read_zero_size =
        cplat_fread(nullptr, 0u, 1u, stream, &read_detail); // [手順] - サイズ 0 を指定して cplat_fread() を呼び出す。
    size_t read_zero_count =
        cplat_fread(nullptr, 1u, 0u, stream, &read_detail); // [手順] - 要素数 0 を指定して cplat_fread() を呼び出す。
    size_t read_full =
        cplat_fread(data, 1u, 1u, stream, &read_detail); // [手順] - 全量読み込みとなる状態で cplat_fread() を呼び出す。
    size_t read_short =
        cplat_fread(data, 1u, 1u, stream, &read_detail); // [手順] - 短い読み込みとなる状態で cplat_fread() を呼び出す。
    size_t write_null_buffer = cplat_fwrite(
        nullptr, 1u, 1u, stream, &write_detail); // [手順] - nullptr の書き込み元を指定して cplat_fwrite() を呼び出す。
    size_t write_null_stream = cplat_fwrite(
        data, 1u, 1u, nullptr, &write_detail); // [手順] - nullptr のストリームを指定して cplat_fwrite() を呼び出す。
    size_t write_zero_size = cplat_fwrite(nullptr, 0u, 1u, stream,
                                          &write_detail); // [手順] - サイズ 0 を指定して cplat_fwrite() を呼び出す。
    size_t write_zero_count = cplat_fwrite(nullptr, 1u, 0u, stream,
                                           &write_detail); // [手順] - 要素数 0 を指定して cplat_fwrite() を呼び出す。
    size_t write_full = cplat_fwrite(data, 1u, 1u, stream,
                                     &write_detail); // [手順] - 全量書き込みとなる状態で cplat_fwrite() を呼び出す。
    size_t write_short = cplat_fwrite(data, 1u, 1u, stream,
                                      &write_detail); // [手順] - 短い書き込みとなる状態で cplat_fwrite() を呼び出す。

    // Assert
    EXPECT_EQ(
        0u,
        read_null_buffer); // [確認_異常系] - 読み込み先に nullptr を指定した cplat_fread() の戻り値が 0u であること。
    EXPECT_EQ(
        0u,
        read_null_stream); // [確認_異常系] - ストリームに nullptr を指定した cplat_fread() の戻り値が 0u であること。
    EXPECT_EQ(0u, read_zero_size);  // [確認_正常系] - サイズに 0u を指定した cplat_fread() の戻り値が 0u であること。
    EXPECT_EQ(0u, read_zero_count); // [確認_正常系] - 要素数に 0u を指定した cplat_fread() の戻り値が 0u であること。
    EXPECT_EQ(1u, read_full);       // [確認_正常系] - 全量を読み取った cplat_fread() の戻り値が 1u であること。
    EXPECT_EQ(
        0u,
        read_short); // [確認_正常系] - 短い読み込みの cplat_fread() の戻り値が 0u であること。(エラー フラグがないことの確認)
    EXPECT_EQ(
        0u,
        write_null_buffer); // [確認_異常系] - 書き込み元に nullptr を指定した cplat_fwrite() の戻り値が 0u であること。
    EXPECT_EQ(
        0u,
        write_null_stream); // [確認_異常系] - ストリームに nullptr を指定した cplat_fwrite() の戻り値が 0u であること。
    EXPECT_EQ(0u, write_zero_size);  // [確認_正常系] - サイズに 0u を指定した cplat_fwrite() の戻り値が 0u であること。
    EXPECT_EQ(0u, write_zero_count); // [確認_正常系] - 要素数に 0u を指定した cplat_fwrite() の戻り値が 0u であること。
    EXPECT_EQ(1u, write_full);       // [確認_正常系] - 全量を書き込んだ cplat_fwrite() の戻り値が 1u であること。
    EXPECT_EQ(0u, write_short);      // [確認_異常系] - 短い書き込みの cplat_fwrite() の戻り値が 0u であること。
}

// 読み込みエラーが詳細エラーへ記録されることの確認
TEST(stdioFailureInjectionTest, fread_and_fgets_report_stream_errors)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char data[2] = {};
    cplat_error fread_detail = {};
    cplat_error fgets_detail = {};
    FILE *fread_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(16));
    FILE *fgets_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(17));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fread(_, _, _, data, 1U, 1U, fread_stream))
        .WillOnce(Return(
            0U)); // [Pre-Assert確認_異常系] - fread() が data、1U、1U、fread_stream を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - fread() から 0 件を返す。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, fread_stream))
        .WillOnce(Return(
            1)); // [Pre-Assert確認_異常系] - fread() の後に ferror() が fread_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - ferror() からエラーありを示す値を返す。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, data, static_cast<int>(sizeof(data)), fgets_stream))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - fgets() が data、sizeof(data)、fgets_stream を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fgets() から nullptr を返す。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, fgets_stream))
        .WillOnce(Return(
            1)); // [Pre-Assert確認_異常系] - fgets() の後に ferror() が fgets_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - ferror() からエラーありを示す値を返す。

    // Act
    size_t read_count =
        cplat_fread(data, 1U, 1U, fread_stream,
                    &fread_detail); // [手順] - エラー状態のストリームを指定して cplat_fread() を呼び出す。
    int fgets_result =
        cplat_fgets(data, sizeof(data), fgets_stream,
                    &fgets_detail); // [手順] - エラー状態のストリームを指定して cplat_fgets() を呼び出す。

    // Assert
    EXPECT_EQ(0U, read_count); // [確認_異常系] - cplat_fread() の戻り値が 0U であること。
    EXPECT_NE(
        CPLAT_OK,
        cplat_error_to_result(
            &fread_detail)); // [確認_異常系] - fread_detail を渡した cplat_error_to_result() の戻り値が CPLAT_OK 以外であること。
    EXPECT_NE(CPLAT_OK, fgets_result); // [確認_異常系] - cplat_fgets() の戻り値が CPLAT_OK 以外であること。
    EXPECT_NE(
        CPLAT_OK,
        cplat_error_to_result(
            &fgets_detail)); // [確認_異常系] - fgets_detail を渡した cplat_error_to_result() の戻り値が CPLAT_OK 以外であること。
}

// remove と rename の成功・失敗が結果コードへ反映されることの確認
TEST(stdioFailureInjectionTest, remove_and_rename_classify_file_operations)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_stdio> mock_stdio;
#endif /* PLATFORM_LINUX */
    const char *old_path = "cplat_stdio_old.txt";
    const char *new_path = "cplat_stdio_new.txt";
    cplat_error detail = {}; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_stdio, rename(_, _, _, StrEq(old_path), StrEq(new_path)))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - rename() が old_path と等しい文字列、new_path と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - rename() から 0 を返す。
    EXPECT_CALL(mock_stdio, remove(_, _, _, StrEq(new_path)))
        .WillOnce(Return(0))
        .WillOnce(
            [](const char *, int, const char *, const char *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - remove() が new_path と等しい文字列を指定して 2 回呼び出されること。
                // [Pre-Assert手順] - remove() から 1 回目は 0、2 回目は errno に ENOENT を設定して -1 を返す。
#endif          /* PLATFORM_LINUX */

    // Act
#if defined(PLATFORM_LINUX)
    int rename_result = cplat_rename(
        old_path, new_path, &detail); // [手順] - 既存ファイルと新しいパスを指定して cplat_rename() を呼び出す。
    int remove_result =
        cplat_remove(new_path, &detail); // [手順] - rename() 後のファイルを指定して cplat_remove() を呼び出す。
    int missing_result =
        cplat_remove(new_path, &detail); // [手順] - 存在しないファイルを指定して cplat_remove() を呼び出す。
#endif                                                              /* PLATFORM_LINUX */
    int null_rename_result =
        cplat_rename(nullptr, new_path, &detail); // [手順] - oldpath に nullptr を指定して cplat_rename() を呼び出す。
    int null_newpath_result =
        cplat_rename(old_path, nullptr, &detail); // [手順] - newpath に nullptr を指定して cplat_rename() を呼び出す。
    int null_remove_result =
        cplat_remove(nullptr, &detail); // [手順] - path に nullptr を指定して cplat_remove() を呼び出す。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(0, rename_result); // [確認_正常系] - cplat_rename() の戻り値が 0 であること。
    EXPECT_EQ(0, remove_result); // [確認_正常系] - cplat_remove() の戻り値が 0 であること。
    EXPECT_NE(
        0, missing_result); // [確認_異常系] - 存在しないファイルを指定した cplat_remove() の戻り値が 0 以外であること。
#endif                                  /* PLATFORM_LINUX */
    EXPECT_EQ(
        -1,
        null_rename_result); // [確認_異常系] - oldpath に nullptr を指定した cplat_rename() の戻り値が -1 であること。
    EXPECT_EQ(
        -1,
        null_newpath_result); // [確認_異常系] - newpath に nullptr を指定した cplat_rename() の戻り値が -1 であること。
    EXPECT_EQ(
        -1,
        null_remove_result); // [確認_異常系] - path に nullptr を指定した cplat_remove() の戻り値が -1 であること。
}

#if defined(PLATFORM_LINUX)
// rename の OS 失敗が詳細エラーへ記録されることの確認
// Windows の cplat_rename は MoveFileExW を呼ぶため、その mock が揃うまで Linux のみで検証する
TEST(stdioFailureInjectionTest, rename_reports_os_failure)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_error detail = {};

    // Pre-Assert
    EXPECT_CALL(mock_stdio, rename(_, _, _, StrEq("/cplat/path/does/not/exist"), StrEq("/tmp/cplat_stdio_target.txt")))
        .WillOnce(
            [](const char *, int, const char *, const char *, const char *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_異常系] - rename() が "/cplat/path/does/not/exist" と等しい文字列、"/tmp/cplat_stdio_target.txt" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - rename() から、errno に ENOENT を設定して -1 を返す。

    // Act
    int result = cplat_rename("/cplat/path/does/not/exist", "/tmp/cplat_stdio_target.txt",
                              &detail); // [手順] - 存在しないパスを指定して cplat_rename() を呼び出す。

    // Assert
    EXPECT_NE(0, result); // [確認_異常系] - cplat_rename() の戻り値が 0 以外であること。
    EXPECT_NE(
        CPLAT_OK,
        cplat_error_to_result(&detail)); // [確認_異常系] - cplat_error_to_result() の戻り値が CPLAT_OK 以外であること。
}
#endif /* PLATFORM_LINUX */

// fprintf、fseek、ftell のラッパーが標準 I/O を通過させることの確認
TEST(stdioFailureInjectionTest, formatted_output_and_file_position_wrappers_work)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(18));

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_stdio, vfprintf(_, _, _, stream, StrEq("abc")))
        .WillOnce(Return(
            3)); // [Pre-Assert確認_正常系] - vfprintf() が stream、"abc" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - vfprintf() から出力文字数 3 を返す。
    EXPECT_CALL(mock_stdio, fseeko(_, _, _, stream, static_cast<off_t>(0), SEEK_SET))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - fseeko() が stream、0、SEEK_SET を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - fseeko() から 0 を返す。
    EXPECT_CALL(mock_stdio, ftello(_, _, _, stream))
        .WillOnce(Return(
            static_cast<off_t>(0))); // [Pre-Assert確認_正常系] - ftello() が stream を指定して 1 回呼び出されること。
                                     // [Pre-Assert手順] - ftello() から位置 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _fseeki64(_, _, _, stream, static_cast<__int64>(0), SEEK_SET))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - _fseeki64() が stream、0、SEEK_SET を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - _fseeki64() から 0 を返す。
    EXPECT_CALL(mock_stdio, _ftelli64(_, _, _, stream))
        .WillOnce(Return(static_cast<__int64>(
            0))); // [Pre-Assert確認_正常系] - _ftelli64() が stream を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - _ftelli64() から位置 0 を返す。
#endif /* PLATFORM_ */

    // Act
#if defined(PLATFORM_LINUX)
    int print_result = cplat_fprintf(
        stream, "%s", "abc"); // [手順] - センチネル ストリームと文字列を指定して cplat_fprintf() を呼び出す。
#endif                                                     /* PLATFORM_LINUX */
    int seek_result =
        cplat_fseek(stream, 0, SEEK_SET);   // [手順] - 先頭 (SEEK_SET) を指定して cplat_fseek() を呼び出す。
    int64_t position = cplat_ftell(stream); // [手順] - cplat_ftell() を呼び出す。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(
        3, print_result); // [確認_正常系] - cplat_fprintf() の戻り値が 3 であること。(出力文字数が 3 であることの確認)
#endif                          /* PLATFORM_LINUX */
    EXPECT_EQ(0, seek_result);  // [確認_正常系] - cplat_fseek() の戻り値が 0 であること。
    EXPECT_EQ(
        0, position); // [確認_正常系] - cplat_ftell() の戻り値が 0 であること。(先頭へ移動した後の位置であることの確認)
}

// fwrite の短い書き込みを未知エラーとして報告することの確認
TEST(stdioFailureInjectionTest, fwrite_reports_short_write)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_error detail = {};
    const char data[] = "xy";
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(3));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, 1u, 2u, stream))
        .WillOnce(Return(1u)); // [Pre-Assert確認_異常系] - fwrite() が 1u、2u、stream を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - fwrite() から 1 要素を返す。

    // Act
    size_t result = cplat_fwrite(data, 1u, 2u, stream,
                                 &detail); // [手順] - 2 要素中 1 要素だけ書き込む状態で cplat_fwrite() を呼び出す。

    // Assert
    EXPECT_EQ(1u, result); // [確認_異常系] - cplat_fwrite() の戻り値が 1u であること。
    EXPECT_EQ(
        EIO,
        cplat_error_get_errno(
            &detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EIO であること。(errno が空の短い書き込みでは EIO が記録されたことの確認)
}

// 標準入出力関数が設定した errno を短い入出力の詳細へ記録することの確認
TEST(stdioFailureInjectionTest, short_io_preserves_nonzero_errno)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char data[2] = {};
    cplat_error read_detail = {};
    cplat_error write_detail = {};
    cplat_error line_detail = {};
    FILE *read_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(4));
    FILE *write_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(5));
    FILE *line_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(6));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fread(_, _, _, data, 1u, 1u, read_stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, void *, size_t, size_t, FILE *)
            {
                errno = EACCES;
                return 0u;
            })); // [Pre-Assert確認_異常系] - fread() が data、1u、1u、read_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - fread() から、errno に EACCES を設定して 0 を返す。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, read_stream))
        .WillOnce(Return(
            1)); // [Pre-Assert確認_異常系] - fread() の後に ferror() が read_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - ferror() からエラーありを示す値を返す。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, data, 1u, 1u, write_stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const void *, size_t, size_t, FILE *)
            {
                errno = ENOSPC;
                return 0u;
            })); // [Pre-Assert確認_異常系] - fwrite() が data、1u、1u、write_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - fwrite() から、errno に ENOSPC を設定して 0 を返す。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, data, static_cast<int>(sizeof(data)), line_stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, char *, int, FILE *) -> char *
            {
                errno = EIO;
                return nullptr;
            })); // [Pre-Assert確認_異常系] - fgets() が data、sizeof(data)、line_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - fgets() から、errno に EIO を設定して nullptr を返す。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, line_stream))
        .WillOnce(Return(
            1)); // [Pre-Assert確認_異常系] - fgets() の後に ferror() が line_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - ferror() からエラーありを示す値を返す。

    // Act
    size_t read_result =
        cplat_fread(data, 1u, 1u, read_stream,
                    &read_detail); // [手順] - fread() が EACCES で失敗する状態で cplat_fread() を呼び出す。
    size_t write_result =
        cplat_fwrite(data, 1u, 1u, write_stream,
                     &write_detail); // [手順] - fwrite() が ENOSPC で失敗する状態で cplat_fwrite() を呼び出す。
    int line_result = cplat_fgets(data, sizeof(data), line_stream,
                                  &line_detail); // [手順] - fgets() が EIO で失敗する状態で cplat_fgets() を呼び出す。

    // Assert
    EXPECT_EQ(0u, read_result); // [確認_異常系] - EACCES が発生した cplat_fread() の戻り値が 0u であること。
    EXPECT_EQ(
        EACCES,
        cplat_error_get_errno(
            &read_detail)); // [確認_異常系] - read_detail を渡した cplat_error_get_errno() の戻り値が EACCES であること。
    EXPECT_EQ(0u, write_result); // [確認_異常系] - ENOSPC が発生した cplat_fwrite() の戻り値が 0u であること。
    EXPECT_EQ(
        ENOSPC,
        cplat_error_get_errno(
            &write_detail)); // [確認_異常系] - write_detail を渡した cplat_error_get_errno() の戻り値が ENOSPC であること。
    EXPECT_NE(CPLAT_OK,
              line_result); // [確認_異常系] - EIO が発生した cplat_fgets() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        EIO,
        cplat_error_get_errno(
            &line_detail)); // [確認_異常系] - line_detail を渡した cplat_error_get_errno() の戻り値が EIO であること。
}

// errno が設定されない読み込みエラーを EIO として報告することの確認
TEST(stdioFailureInjectionTest, read_errors_without_errno_default_to_eio)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char read_data[2] = {};
    char line_data[2] = {};
    cplat_error read_detail = {};
    cplat_error line_detail = {};
    FILE *read_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(10));
    FILE *line_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(11));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fread(_, _, _, read_data, 1u, 1u, read_stream))
        .WillOnce(Return(
            0u)); // [Pre-Assert確認_異常系] - fread() が read_data、1u、1u、read_stream を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - fread() から、errno を設定せずに 0 を返す。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, read_stream))
        .WillOnce(Return(
            1)); // [Pre-Assert確認_異常系] - fread() の後に ferror() が read_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - ferror() からエラーありを示す値を返す。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, line_data, static_cast<int>(sizeof(line_data)), line_stream))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - fgets() が line_data、sizeof(line_data)、line_stream を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fgets() から、errno を設定せずに nullptr を返す。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, line_stream))
        .WillOnce(Return(
            1)); // [Pre-Assert確認_異常系] - fgets() の後に ferror() が line_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - ferror() からエラーありを示す値を返す。

    // Act
    size_t read_result =
        cplat_fread(read_data, 1u, 1u, read_stream,
                    &read_detail); // [手順] - fread() が errno を設定せず失敗する状態で cplat_fread() を呼び出す。
    int line_result =
        cplat_fgets(line_data, sizeof(line_data), line_stream,
                    &line_detail); // [手順] - fgets() が errno を設定せず失敗する状態で cplat_fgets() を呼び出す。

    // Assert
    EXPECT_EQ(0u, read_result); // [確認_異常系] - errno がないときの cplat_fread() の戻り値が 0u であること。
    EXPECT_EQ(
        EIO,
        cplat_error_get_errno(
            &read_detail)); // [確認_異常系] - read_detail を渡した cplat_error_get_errno() の戻り値が EIO であること。(errno がない読み込みエラーでは EIO が記録されたことの確認)
    EXPECT_NE(CPLAT_OK,
              line_result); // [確認_異常系] - errno がないときの cplat_fgets() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        EIO,
        cplat_error_get_errno(
            &line_detail)); // [確認_異常系] - line_detail を渡した cplat_error_get_errno() の戻り値が EIO であること。(errno がない読み込みエラーでは EIO が記録されたことの確認)
}

// fgets が空文字列とバッファー末尾の改行を分類することの確認
TEST(stdioFailureInjectionTest, fgets_classifies_empty_text_and_exact_newline)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char empty_text[2] = {};
    char newline_text[2] = {};
    FILE *empty_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(7));
    FILE *newline_stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(8));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, empty_text, 2, empty_stream))
        .WillOnce(Return(
            empty_text)); // [Pre-Assert確認_正常系] - fgets() が empty_text、2、empty_stream を指定して 1 回呼び出されること。
                          // [Pre-Assert手順] - fgets() から、空文字列のバッファーを返す。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, newline_text, 2, newline_stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, char *dest, int, FILE *) -> char *
            {
                dest[0] = '\n';
                dest[1] = '\0';
                return dest;
            })); // [Pre-Assert確認_正常系] - fgets() が newline_text、2、newline_stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - fgets() から、改行だけを格納したバッファーを返す。

    // Act
    int empty_result = cplat_fgets(empty_text, sizeof(empty_text), empty_stream,
                                   nullptr); // [手順] - fgets() が空文字列を返す状態で cplat_fgets() を呼び出す。
    int newline_result = cplat_fgets(newline_text, sizeof(newline_text), newline_stream,
                                     nullptr); // [手順] - バッファー末尾が改行となる状態で cplat_fgets() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              empty_result);      // [確認_正常系] - 空文字列を返した cplat_fgets() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", empty_text); // [確認_正常系] - empty_text が "" であること。
    EXPECT_EQ(CPLAT_OK,
              newline_result);      // [確認_正常系] - 改行を返した cplat_fgets() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", newline_text); // [確認_正常系] - newline_text が "" であること。(末尾の改行が除去されたことの確認)
}

// バッファーを使い切る最終行が切り詰めとして扱われないことの確認
TEST(stdioFailureInjectionTest, fgets_accepts_exact_final_line)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char data[2] = {};
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(12));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, data, 2, stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, char *dest, int, FILE *) -> char *
            {
                dest[0] = 'x';
                dest[1] = '\0';
                return dest;
            })); // [Pre-Assert確認_正常系] - fgets() が data、2、stream を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - fgets() から、バッファーを使い切る 1 文字の最終行を返す。
    EXPECT_CALL(mock_stdio, feof(_, _, _, stream))
        .WillOnce(
            Return(1)); // [Pre-Assert確認_正常系] - 行の終端確認で feof() が stream を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - feof() から EOF 到達済みを示す値を返す。

    // Act
    int result = cplat_fgets(data, sizeof(data), stream,
                             nullptr); // [手順] - バッファーを使い切る最終行を読み取る状態で cplat_fgets() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);       // [確認_正常系] - 最終行を読み取った cplat_fgets() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("x", data); // [確認_正常系] - data が "x" であること。
}

// fgets が 0 と INT_MAX 超のバッファー サイズを拒否することの確認
TEST(stdioFailureInjectionTest, fgets_rejects_invalid_buffer_sizes)
{
    // Arrange
    char data[2] = {};
    FILE *stream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(9));

    // Pre-Assert

    // Act
    int zero_result =
        cplat_fgets(data, 0u, stream, nullptr); // [手順] - dest_size に 0 を指定して cplat_fgets() を呼び出す。
    int oversized_result =
        cplat_fgets(data, static_cast<size_t>(INT_MAX) + 1u, stream,
                    nullptr); // [手順] - dest_size に INT_MAX を 1 超える値を指定して cplat_fgets() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_result); // [確認_異常系] - dest_size に 0u を指定した cplat_fgets() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        oversized_result); // [確認_異常系] - dest_size に INT_MAX を超える値を指定した cplat_fgets() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// vsnprintf の失敗時に出力を空文字列へ変更することの確認
TEST(stdioFailureInjectionTest, snprintf_reports_formatting_failure)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char buffer[8] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vsnprintf(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - vsnprintf() が 1 回呼び出されること。
                               // [Pre-Assert手順] - vsnprintf() から -1 を返す。

    // Act
    int result = cplat_snprintf(buffer, sizeof(buffer), "%s",
                                "value"); // [手順] - vsnprintf() が -1 を返す状態で cplat_snprintf() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_snprintf() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ("",
                 buffer); // [確認_異常系] - buffer が "" であること。(フォーマット失敗時に出力が空になったことの確認)
}
