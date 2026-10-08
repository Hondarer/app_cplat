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
                return static_cast<FILE *>(NULL);
            })); // [Pre-Assert確認_異常系] - fopen が 1 回呼び出されること。
                 // [Pre-Assert手順] - errno に EACCES を設定し、NULL を返却する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _wfsopen(_, _, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const wchar_t *, const wchar_t *, int)
            {
                errno = EACCES;
                return static_cast<FILE *>(NULL);
            })); // [Pre-Assert確認_異常系] - _wfsopen が 1 回呼び出されること。
                 // [Pre-Assert手順] - errno に EACCES を設定し、NULL を返却する。
#endif /* PLATFORM_ */

    // Act
    FILE *stream =
        cplat_fopen("mocked-failure.txt", "rb", &detail); // [手順] - fopen の EACCES 失敗を注入してファイルを開く。

    // Assert
    EXPECT_EQ(static_cast<FILE *>(NULL), stream);      // [確認_異常系] - cplat_fopen の戻り値が NULL であること。
    EXPECT_EQ(EACCES, cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細エラーへ EACCES が記録されること。
}

// fopen の NULL 引数が不正引数として分類されることの確認
TEST(stdioFailureInjectionTest, fopen_rejects_null_arguments)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert

    // Act
    FILE *null_path = cplat_fopen(NULL, "rb", &detail);    // [手順] - path に NULL を指定して fopen する。
    FILE *null_modes = cplat_fopen("file", NULL, &detail); // [手順] - modes に NULL を指定して fopen する。

    // Assert
    EXPECT_EQ(static_cast<FILE *>(NULL), null_path);  // [確認_異常系] - path NULL の fopen が NULL を返すこと。
    EXPECT_EQ(static_cast<FILE *>(NULL), null_modes); // [確認_異常系] - modes NULL の fopen が NULL を返すこと。
    EXPECT_EQ(CPLAT_CAUSE_INVALID_ARGUMENT,
              cplat_error_get_cause(&detail)); // [確認_異常系] - NULL 引数の要因が INVALID_ARGUMENT であること。
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
        .WillOnce(Return(opened)); // [Pre-Assert確認_正常系] - fopen が opened.txt で 1 回呼び出されること。
                                   // [Pre-Assert手順] - 番兵ストリームを返却する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _wfsopen(_, _, _, _, _, _))
        .WillOnce(Return(opened)); // [Pre-Assert確認_正常系] - _wfsopen が 1 回呼び出されること。
                                   // [Pre-Assert手順] - 番兵ストリームを返却する。
#endif /* PLATFORM_ */

    // Act
    FILE *stream = cplat_fopen("opened.txt", "rb", &detail); // [手順] - 成功する fopen を注入してファイルを開く。

    // Assert
    EXPECT_EQ(opened, stream); // [確認_正常系] - cplat_fopen の戻り値が番兵ストリームであること。
    EXPECT_EQ(CPLAT_OK,
              cplat_error_to_result(&detail)); // [確認_正常系] - 成功時に詳細エラーがクリアされること。
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
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fclose が番兵ストリームで 1 回呼び出されること。
                                // [Pre-Assert手順] - EOF を返却する。

    // Act
    int result = cplat_fclose(stream, &detail); // [手順] - fclose が EOF を返す失敗を注入する。

    // Assert
    EXPECT_EQ(EOF, result);                         // [確認_異常系] - cplat_fclose の戻り値が EOF であること。
    EXPECT_EQ(EIO, cplat_error_get_errno(&detail)); // [確認_異常系] - errno が空の場合に EIO が記録されること。
}

// fclose の NULL 引数が不正引数として分類されることの確認
TEST(stdioFailureInjectionTest, fclose_rejects_null_stream)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert

    // Act
    int result = cplat_fclose(NULL, &detail); // [手順] - stream に NULL を指定して fclose する。

    // Assert
    EXPECT_EQ(EOF, result); // [確認_異常系] - NULL stream の fclose が EOF を返すこと。
    EXPECT_EQ(CPLAT_CAUSE_INVALID_ARGUMENT,
              cplat_error_get_cause(&detail)); // [確認_異常系] - NULL stream が INVALID_ARGUMENT として記録されること。
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
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - fclose が番兵ストリームで 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。

    // Act
    int result = cplat_fclose(stream, &detail); // [手順] - 成功する fclose を注入して閉じる。

    // Assert
    EXPECT_EQ(0, result); // [確認_正常系] - cplat_fclose の戻り値が 0 であること。
    EXPECT_EQ(CPLAT_OK,
              cplat_error_to_result(&detail)); // [確認_正常系] - 成功時に詳細エラーがクリアされること。
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
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush が番兵ストリームで 1 回呼び出されること。
                                // [Pre-Assert手順] - EOF を返却する。

    // Act
    int result = cplat_fflush(stream, &detail); // [手順] - fflush が EOF を返す失敗を注入する。

    // Assert
    EXPECT_EQ(EOF, result);                         // [確認_異常系] - cplat_fflush の戻り値が EOF であること。
    EXPECT_EQ(EIO, cplat_error_get_errno(&detail)); // [確認_異常系] - errno が空の場合に EIO が記録されること。
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
            })); // [Pre-Assert確認_異常系] - fclose が番兵ストリームで 1 回呼び出されること。
                 // [Pre-Assert手順] - errno に EACCES を設定し、EOF を返却する。

    // Act
    int result = cplat_fclose(stream, &detail); // [手順] - errno が EACCES の fclose 失敗を注入する。

    // Assert
    EXPECT_EQ(EOF, result);                            // [確認_異常系] - fclose の戻り値が EOF であること。
    EXPECT_EQ(EACCES, cplat_error_get_errno(&detail)); // [確認_異常系] - EACCES が詳細エラーへ記録されること。
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
            })); // [Pre-Assert確認_異常系] - fflush が番兵ストリームで 1 回呼び出されること。
                 // [Pre-Assert手順] - errno に EACCES を設定し、EOF を返却する。

    // Act
    int result = cplat_fflush(stream, &detail); // [手順] - errno が EACCES の fflush 失敗を注入する。

    // Assert
    EXPECT_EQ(EOF, result);                            // [確認_異常系] - fflush の戻り値が EOF であること。
    EXPECT_EQ(EACCES, cplat_error_get_errno(&detail)); // [確認_異常系] - EACCES が詳細エラーへ記録されること。
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
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - fflush が番兵ストリームで 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。

    // Act
    int result = cplat_fflush(stream, &detail); // [手順] - 番兵ストリームへ fflush を実行する。

    // Assert
    EXPECT_EQ(0, result); // [確認_正常系] - cplat_fflush の戻り値が 0 であること。
    EXPECT_EQ(CPLAT_OK,
              cplat_error_to_result(&detail)); // [確認_正常系] - 成功時に詳細エラーがクリアされること。
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
                return static_cast<FILE *>(NULL);
            }); // [Pre-Assert確認_異常系] - freopen が存在しないパスで 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、NULL を返却する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _wfsopen(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, const wchar_t *, const wchar_t *, int)
            {
                errno = ENOENT;
                return static_cast<FILE *>(NULL);
            }); // [Pre-Assert確認_異常系] - _wfsopen が 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、NULL を返却する。
#endif /* PLATFORM_ */

    // Act
    FILE *reopened = cplat_freopen("/cplat/path/does/not/exist", "rb", stream,
                                   &detail); // [手順] - 存在しないパスへ freopen して OS 失敗を発生させる。

    // Assert
    EXPECT_EQ(static_cast<FILE *>(NULL), reopened); // [確認_異常系] - cplat_freopen の戻り値が NULL であること。
    EXPECT_NE(CPLAT_OK,
              cplat_error_to_result(&detail)); // [確認_異常系] - OS 失敗が詳細エラーへ記録されること。
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
        .WillByDefault(Return(0)); // [状態] - ferror が呼び出された際に 0 を返すようにモックを設定する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fread(_, _, _, _, 0u, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - fread がサイズ 0 で呼び出されないこと。
    EXPECT_CALL(mock_stdio, fread(_, _, _, _, 1u, 1u, stream))
        .WillOnce(Return(1u))
        .WillOnce(Return(0u)); // [Pre-Assert確認_正常系] - fread がサイズ 1 で 2 回呼び出されること。
                               // [Pre-Assert手順] - 全量 1 件ののち短い読み込み 0 件を返却する。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, 0u, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - fwrite がサイズ 0 で呼び出されないこと。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, 1u, 1u, stream))
        .WillOnce(Return(1u))
        .WillOnce(Return(0u)); // [Pre-Assert確認_正常系] - fwrite がサイズ 1 で 2 回呼び出されること。
                               // [Pre-Assert手順] - 全量 1 件ののち短い書き込み 0 件を返却する。
    EXPECT_CALL(mock_stdio, fread(_, _, _, _, _, 0u, _))
        .Times(0); // [Pre-Assert確認_正常系] - fread が要素数 0 で呼び出されないこと。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, _, _, 0u, _))
        .Times(0); // [Pre-Assert確認_正常系] - fwrite が要素数 0 で呼び出されないこと。

    // Act
    size_t read_null_buffer = cplat_fread(NULL, 1u, 1u, stream, &read_detail); // [手順] - NULL 読み込み先を指定する。
    size_t read_null_stream = cplat_fread(data, 1u, 1u, NULL, &read_detail);   // [手順] - NULL ストリームを指定する。
    size_t read_zero_size = cplat_fread(NULL, 0u, 1u, stream, &read_detail); // [手順] - サイズ 0 の読み込みを指定する。
    size_t read_zero_count =
        cplat_fread(NULL, 1u, 0u, stream, &read_detail);                 // [手順] - 要素数 0 の読み込みを指定する。
    size_t read_full = cplat_fread(data, 1u, 1u, stream, &read_detail);  // [手順] - 全量読み込みを指定する。
    size_t read_short = cplat_fread(data, 1u, 1u, stream, &read_detail); // [手順] - 短い読み込みを指定する。
    size_t write_null_buffer =
        cplat_fwrite(NULL, 1u, 1u, stream, &write_detail);                      // [手順] - NULL 書き込み元を指定する。
    size_t write_null_stream = cplat_fwrite(data, 1u, 1u, NULL, &write_detail); // [手順] - NULL ストリームへ書き込む。
    size_t write_zero_size =
        cplat_fwrite(NULL, 0u, 1u, stream, &write_detail); // [手順] - サイズ 0 の書き込みを指定する。
    size_t write_zero_count =
        cplat_fwrite(NULL, 1u, 0u, stream, &write_detail);                  // [手順] - 要素数 0 の書き込みを指定する。
    size_t write_full = cplat_fwrite(data, 1u, 1u, stream, &write_detail);  // [手順] - 全量書き込みを指定する。
    size_t write_short = cplat_fwrite(data, 1u, 1u, stream, &write_detail); // [手順] - 短い書き込みを指定する。

    // Assert
    EXPECT_EQ(0u, read_null_buffer);  // [確認_異常系] - NULL 読み込み先が 0 件になること。
    EXPECT_EQ(0u, read_null_stream);  // [確認_異常系] - NULL ストリームが 0 件になること。
    EXPECT_EQ(0u, read_zero_size);    // [確認_正常系] - サイズ 0 の読み込みが 0 件になること。
    EXPECT_EQ(0u, read_zero_count);   // [確認_正常系] - 要素数 0 の cplat_fread の戻り値が 0 件であること。
    EXPECT_EQ(1u, read_full);         // [確認_正常系] - 全量読み込みが 1 件になること。
    EXPECT_EQ(0u, read_short);        // [確認_正常系] - エラー フラグのない短い読み込みが 0 件になること。
    EXPECT_EQ(0u, write_null_buffer); // [確認_異常系] - NULL 書き込み元が 0 件になること。
    EXPECT_EQ(0u, write_null_stream); // [確認_異常系] - NULL ストリームが 0 件になること。
    EXPECT_EQ(0u, write_zero_size);   // [確認_正常系] - サイズ 0 の書き込みが 0 件になること。
    EXPECT_EQ(0u, write_zero_count);  // [確認_正常系] - 要素数 0 の cplat_fwrite の戻り値が 0 件であること。
    EXPECT_EQ(1u, write_full);        // [確認_正常系] - 全量書き込みが 1 件になること。
    EXPECT_EQ(0u, write_short);       // [確認_異常系] - 短い書き込みが 0 件になること。
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
        .WillOnce(Return(0U)); // [Pre-Assert確認_異常系] - fread が対象バッファーとストリームで 1 回呼び出されること。
                               // [Pre-Assert手順] - 0 件を返却する。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, fread_stream))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - fread 後に ferror が 1 回呼び出されること。
                              // [Pre-Assert手順] - エラーありを返却する。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, data, static_cast<int>(sizeof(data)), fgets_stream))
        .WillOnce(Return(static_cast<char *>(
            NULL))); // [Pre-Assert確認_異常系] - fgets が対象バッファーとストリームで 1 回呼び出されること。
                     // [Pre-Assert手順] - NULL を返却する。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, fgets_stream))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - fgets 後に ferror が 1 回呼び出されること。
                              // [Pre-Assert手順] - エラーありを返却する。

    // Act
    size_t read_count =
        cplat_fread(data, 1U, 1U, fread_stream, &fread_detail); // [手順] - エラー状態のストリームを fread へ渡す。
    int fgets_result = cplat_fgets(data, sizeof(data), fgets_stream,
                                   &fgets_detail); // [手順] - エラー状態のストリームを fgets へ渡す。

    // Assert
    EXPECT_EQ(0U, read_count); // [確認_異常系] - 読み込みエラー時の fread 件数が 0 であること。
    EXPECT_NE(CPLAT_OK,
              cplat_error_to_result(&fread_detail)); // [確認_異常系] - fread の詳細エラーが成功以外であること。
    EXPECT_NE(CPLAT_OK, fgets_result); // [確認_異常系] - 読み込みエラー時の fgets 結果が成功以外であること。
    EXPECT_NE(CPLAT_OK,
              cplat_error_to_result(&fgets_detail)); // [確認_異常系] - fgets の詳細エラーが成功以外であること。
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
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - rename が旧パスと新パスで 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(mock_stdio, remove(_, _, _, StrEq(new_path)))
        .WillOnce(Return(0))
        .WillOnce(
            [](const char *, int, const char *, const char *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - remove が新パスで 2 回呼び出されること。
                // [Pre-Assert手順] - 1 回目は 0、2 回目は ENOENT で -1 を返却する。
#endif          /* PLATFORM_LINUX */

    // Act
#if defined(PLATFORM_LINUX)
    int rename_result = cplat_rename(old_path, new_path, &detail);  // [手順] - 既存ファイルを新しいパスへ rename する。
    int remove_result = cplat_remove(new_path, &detail);            // [手順] - rename 後のファイルを remove する。
    int missing_result = cplat_remove(new_path, &detail);           // [手順] - 存在しないファイルを remove する。
#endif                                                              /* PLATFORM_LINUX */
    int null_rename_result = cplat_rename(NULL, new_path, &detail); // [手順] - oldpath NULL の rename を実行する。
    int null_newpath_result = cplat_rename(old_path, NULL, &detail); // [手順] - newpath NULL の rename を実行する。
    int null_remove_result = cplat_remove(NULL, &detail);            // [手順] - path NULL の remove を実行する。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(0, rename_result);        // [確認_正常系] - rename の戻り値が 0 であること。
    EXPECT_EQ(0, remove_result);        // [確認_正常系] - remove の戻り値が 0 であること。
    EXPECT_NE(0, missing_result);       // [確認_異常系] - 存在しないファイルの remove が失敗すること。
#endif                                  /* PLATFORM_LINUX */
    EXPECT_EQ(-1, null_rename_result);  // [確認_異常系] - oldpath NULL の rename が -1 を返すこと。
    EXPECT_EQ(-1, null_newpath_result); // [確認_異常系] - newpath NULL の cplat_rename の戻り値が -1 であること。
    EXPECT_EQ(-1, null_remove_result);  // [確認_異常系] - path NULL の remove が -1 を返すこと。
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
            }); // [Pre-Assert確認_異常系] - rename が存在しないパスで 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。

    // Act
    int result = cplat_rename("/cplat/path/does/not/exist", "/tmp/cplat_stdio_target.txt",
                              &detail); // [手順] - 存在しないパスを rename して OS 失敗を発生させる。

    // Assert
    EXPECT_NE(0, result); // [確認_異常系] - cplat_rename の戻り値が 0 以外であること。
    EXPECT_NE(CPLAT_OK,
              cplat_error_to_result(&detail)); // [確認_異常系] - OS 失敗が詳細エラーへ記録されること。
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
        .WillOnce(Return(3)); // [Pre-Assert確認_正常系] - vfprintf が "abc" で 1 回呼び出されること。
                              // [Pre-Assert手順] - 出力文字数 3 を返却する。
    EXPECT_CALL(mock_stdio, fseeko(_, _, _, stream, static_cast<off_t>(0), SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - fseeko が先頭へ 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(mock_stdio, ftello(_, _, _, stream))
        .WillOnce(Return(static_cast<off_t>(0))); // [Pre-Assert確認_正常系] - ftello が 1 回呼び出されること。
                                                  // [Pre-Assert手順] - 位置 0 を返却する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_stdio, _fseeki64(_, _, _, stream, static_cast<__int64>(0), SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - _fseeki64 が先頭へ 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(mock_stdio, _ftelli64(_, _, _, stream))
        .WillOnce(Return(static_cast<__int64>(0))); // [Pre-Assert確認_正常系] - _ftelli64 が 1 回呼び出されること。
                                                    // [Pre-Assert手順] - 位置 0 を返却する。
#endif /* PLATFORM_ */

    // Act
#if defined(PLATFORM_LINUX)
    int print_result = cplat_fprintf(stream, "%s", "abc"); // [手順] - 番兵ストリームへ文字列を fprintf する。
#endif                                                     /* PLATFORM_LINUX */
    int seek_result = cplat_fseek(stream, 0, SEEK_SET);    // [手順] - ファイル位置を先頭へ移動する。
    int64_t position = cplat_ftell(stream);                // [手順] - 現在のファイル位置を取得する。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(3, print_result); // [確認_正常系] - fprintf の出力文字数が 3 であること。
#endif                          /* PLATFORM_LINUX */
    EXPECT_EQ(0, seek_result);  // [確認_正常系] - fseek の戻り値が 0 であること。
    EXPECT_EQ(0, position);     // [確認_正常系] - 先頭へ移動後の ftell が 0 であること。
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
        .WillOnce(Return(1u)); // [Pre-Assert確認_異常系] - fwrite が 2 要素の書き込みで 1 回呼び出されること。
                               // [Pre-Assert手順] - 1 要素を返却する。

    // Act
    size_t result = cplat_fwrite(data, 1u, 2u, stream, &detail); // [手順] - 2 要素中 1 要素だけ書き込む失敗を注入する。

    // Assert
    EXPECT_EQ(1u, result); // [確認_異常系] - cplat_fwrite の戻り値が 1 要素であること。
    EXPECT_EQ(EIO,
              cplat_error_get_errno(&detail)); // [確認_異常系] - errno が空の短い書き込みへ EIO が記録されること。
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
            })); // [Pre-Assert確認_異常系] - fread が対象バッファーとストリームで 1 回呼び出されること。
                 // [Pre-Assert手順] - errno に EACCES を設定して fread から 0 を返却する。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, read_stream))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - fread 後に ferror が 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror からエラーありを返却する。
    EXPECT_CALL(mock_stdio, fwrite(_, _, _, data, 1u, 1u, write_stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const void *, size_t, size_t, FILE *)
            {
                errno = ENOSPC;
                return 0u;
            })); // [Pre-Assert確認_異常系] - fwrite が対象バッファーとストリームで 1 回呼び出されること。
                 // [Pre-Assert手順] - errno に ENOSPC を設定して fwrite から 0 を返却する。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, data, static_cast<int>(sizeof(data)), line_stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, char *, int, FILE *) -> char *
            {
                errno = EIO;
                return NULL;
            })); // [Pre-Assert確認_異常系] - fgets が対象バッファーとストリームで 1 回呼び出されること。
                 // [Pre-Assert手順] - errno に EIO を設定して fgets から NULL を返却する。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, line_stream))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - fgets 後に ferror が 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror からエラーありを返却する。

    // Act
    size_t read_result =
        cplat_fread(data, 1u, 1u, read_stream, &read_detail); // [手順] - EACCES で失敗する fread を実行する。
    size_t write_result =
        cplat_fwrite(data, 1u, 1u, write_stream, &write_detail); // [手順] - ENOSPC で失敗する fwrite を実行する。
    int line_result =
        cplat_fgets(data, sizeof(data), line_stream, &line_detail); // [手順] - EIO で失敗する fgets を実行する。

    // Assert
    EXPECT_EQ(0u, read_result); // [確認_異常系] - EACCES 発生時の cplat_fread の戻り値が 0 であること。
    EXPECT_EQ(EACCES, cplat_error_get_errno(
                          &read_detail)); // [確認_異常系] - cplat_fread の詳細エラーに EACCES が記録されること。
    EXPECT_EQ(0u, write_result);          // [確認_異常系] - ENOSPC 発生時の cplat_fwrite の戻り値が 0 であること。
    EXPECT_EQ(ENOSPC, cplat_error_get_errno(
                          &write_detail)); // [確認_異常系] - cplat_fwrite の詳細エラーに ENOSPC が記録されること。
    EXPECT_NE(CPLAT_OK, line_result);      // [確認_異常系] - EIO 発生時の cplat_fgets の戻り値が成功以外であること。
    EXPECT_EQ(EIO,
              cplat_error_get_errno(&line_detail)); // [確認_異常系] - cplat_fgets の詳細エラーに EIO が記録されること。
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
        .WillOnce(Return(0u)); // [Pre-Assert確認_異常系] - fread が対象バッファーとストリームで 1 回呼び出されること。
                               // [Pre-Assert手順] - errno を設定せず fread から 0 を返却する。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, read_stream))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - fread 後に ferror が 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror からエラーありを返却する。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, line_data, static_cast<int>(sizeof(line_data)), line_stream))
        .WillOnce(Return(static_cast<char *>(
            NULL))); // [Pre-Assert確認_異常系] - fgets が対象バッファーとストリームで 1 回呼び出されること。
                     // [Pre-Assert手順] - errno を設定せず fgets から NULL を返却する。
    EXPECT_CALL(mock_stdio, ferror(_, _, _, line_stream))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - fgets 後に ferror が 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror からエラーありを返却する。

    // Act
    size_t read_result = cplat_fread(read_data, 1u, 1u, read_stream,
                                     &read_detail); // [手順] - errno を設定せず失敗する fread を実行する。
    int line_result = cplat_fgets(line_data, sizeof(line_data), line_stream,
                                  &line_detail); // [手順] - errno を設定せず失敗する fgets を実行する。

    // Assert
    EXPECT_EQ(0u, read_result); // [確認_異常系] - errno がないエラー時の cplat_fread の戻り値が 0 であること。
    EXPECT_EQ(EIO, cplat_error_get_errno(
                       &read_detail)); // [確認_異常系] - errno がない fread エラーの詳細へ EIO が記録されること。
    EXPECT_NE(CPLAT_OK,
              line_result); // [確認_異常系] - errno がないエラー時の cplat_fgets の戻り値が成功以外であること。
    EXPECT_EQ(EIO, cplat_error_get_errno(
                       &line_detail)); // [確認_異常系] - errno がない fgets エラーの詳細へ EIO が記録されること。
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
        .WillOnce(Return(empty_text)); // [Pre-Assert確認_正常系] - 空文字列用の fgets が 1 回呼び出されること。
                                       // [Pre-Assert手順] - fgets から空文字列のバッファーを返却する。
    EXPECT_CALL(mock_stdio, fgets(_, _, _, newline_text, 2, newline_stream))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, char *dest, int, FILE *) -> char *
            {
                dest[0] = '\n';
                dest[1] = '\0';
                return dest;
            })); // [Pre-Assert確認_正常系] - 改行用の fgets が 1 回呼び出されること。
                 // [Pre-Assert手順] - fgets から改行だけを格納したバッファーを返却する。

    // Act
    int empty_result = cplat_fgets(empty_text, sizeof(empty_text), empty_stream,
                                   NULL); // [手順] - 空文字列を返す fgets を実行する。
    int newline_result = cplat_fgets(newline_text, sizeof(newline_text), newline_stream,
                                     NULL); // [手順] - バッファー末尾が改行の fgets を実行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              empty_result);      // [確認_正常系] - 空文字列を返した cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", empty_text); // [確認_正常系] - 空文字列が維持されること。
    EXPECT_EQ(CPLAT_OK,
              newline_result);      // [確認_正常系] - 改行を返した cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", newline_text); // [確認_正常系] - 末尾の改行が除去されること。
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
            })); // [Pre-Assert確認_正常系] - fgets が 1 回呼び出されること。
                 // [Pre-Assert手順] - fgets からバッファーを使い切る 1 文字の最終行を返却する。
    EXPECT_CALL(mock_stdio, feof(_, _, _, stream))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - 行の終端確認で feof が 1 回呼び出されること。
                              // [Pre-Assert手順] - feof から EOF 到達済みを返却する。

    // Act
    int result = cplat_fgets(data, sizeof(data), stream, NULL); // [手順] - バッファーを使い切る最終行を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);       // [確認_正常系] - 最終行を読み取った cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("x", data); // [確認_正常系] - 最終行の 1 文字が保持されること。
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
        cplat_fgets(data, 0u, stream, NULL); // [手順] - dest_size に 0 を指定して cplat_fgets を呼び出す。
    int oversized_result =
        cplat_fgets(data, static_cast<size_t>(INT_MAX) + 1u, stream,
                    NULL); // [手順] - dest_size に INT_MAX を 1 超える値を指定して cplat_fgets を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_result); // [確認_異常系] - dest_size が 0 の cplat_fgets の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        oversized_result); // [確認_異常系] - dest_size が INT_MAX を超える cplat_fgets の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// vsnprintf の失敗時に出力を空文字列へ変更することの確認
TEST(stdioFailureInjectionTest, snprintf_reports_formatting_failure)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    char buffer[8] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vsnprintf(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - vsnprintf が 1 回呼び出されること。
                               // [Pre-Assert手順] - -1 を返却する。

    // Act
    int result =
        cplat_snprintf(buffer, sizeof(buffer), "%s", "value"); // [手順] - vsnprintf が -1 を返す失敗を注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result);        // [確認_異常系] - cplat_snprintf の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ("", buffer); // [確認_異常系] - フォーマット失敗時に出力バッファーが空になること。
}
