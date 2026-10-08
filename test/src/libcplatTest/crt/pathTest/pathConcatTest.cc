#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <cerrno>
#include <cstring>

class pathConcatTest : public Test
{
};

// パス断片が指定順に連結されることの確認
TEST_F(pathConcatTest, concatenates_path_fragments)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {}; // [状態] - 出力バッファーを用意する。
    cplat_error err;

    // Pre-Assert

    // Act
    int actual_ret_path_concat =
        cplat_path_concat(path, sizeof(path), &err, "tmp", PLATFORM_PATH_SEP,
                          "libbase_extdef.json"); // [手順] - "tmp"、セパレータ、"libbase_extdef.json" を連結する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_path_concat);   // [確認_正常系] - cplat_path_concat の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("tmp/libbase_extdef.json", path); // [確認_正常系] - 断片が指定順に連結されること。
}

// 空文字列の断片がそのまま扱えることの確認
TEST_F(pathConcatTest, keeps_empty_fragment)
{
    // Arrange
    char path[32] = {}; // [状態] - 出力バッファーを用意する。
    cplat_error err;

    // Pre-Assert

    // Act
    int actual_ret_path_concat =
        cplat_path_concat(path, sizeof(path), &err, "", "abc"); // [手順] - 空文字列と "abc" を連結する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_path_concat); // [確認_正常系] - cplat_path_concat の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", path);                   // [確認_正常系] - 空文字断片もそのまま扱え "abc" になること。
}

// サポート上限の 16 断片を連結できることの確認
TEST_F(pathConcatTest, accepts_sixteen_fragments)
{
    // Arrange
    char path[32] = {}; // [状態] - 出力バッファーを用意する。
    cplat_error err;

    // Pre-Assert

    // Act
    int actual_ret_path_concat =
        cplat_path_concat(path, sizeof(path), &err, "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                          "n", "o", "p"); // [手順] - "a" から "p" までの 16 断片を連結する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_path_concat); // [確認_正常系] - cplat_path_concat の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abcdefghijklmnop", path);      // [確認_正常系] - サポート上限の 16 断片が連結されること。
}

// 断片数 0 の場合に EINVAL で失敗することの確認
TEST_F(pathConcatTest, returns_einval_for_zero_part_count)
{
    // Arrange
    char path[8] = {}; // [状態] - 出力バッファーを用意する。
    cplat_error err;
    cplat_error last_error;

    // Pre-Assert

    // Act
    int actual_ret_path_concat_n = cplat_path_concat_n(
        path, sizeof(path), &err, 0u); // [手順] - part_count に 0 を渡して cplat_path_concat_n を呼び出す。
    cplat_error_get_last(&last_error); // [手順] - TLS に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_path_concat_n); // [確認_異常系] - cplat_path_concat_n の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因が返ること。
    EXPECT_EQ(1, cplat_error_is_set(&last_error)); // [確認_異常系] - TLS に詳細エラーが記録されること。
}

// 連結先が NULL またはサイズ 0 の場合に EINVAL で失敗することの確認
TEST_F(pathConcatTest, returns_einval_for_invalid_output_buffer)
{
    // Arrange
    char path[8] = {'x'}; // [状態] - サイズ 0 の呼び出しで変更されないことを確認するため 'x' で初期化する。
    cplat_error null_detail;
    cplat_error zero_detail;

    // Pre-Assert

    // Act
    int null_result = cplat_path_concat_n(NULL, sizeof(path), &null_detail, 1u,
                                          "a"); // [手順] - 連結先に NULL を指定して cplat_path_concat_n を呼び出す。
    int zero_result = cplat_path_concat_n(path, 0u, &zero_detail, 1u,
                                          "a"); // [手順] - 連結先サイズに 0 を指定して cplat_path_concat_n を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_result); // [確認_異常系] - 連結先が NULL の cplat_path_concat_n の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_result); // [確認_異常系] - 連結先サイズが 0 の cplat_path_concat_n の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('x', path[0]); // [確認_異常系] - サイズ 0 の連結先が変更されないこと。
}

// NULL 断片が含まれる場合に EINVAL で失敗することの確認
TEST_F(pathConcatTest, returns_einval_for_null_fragment)
{
    // Arrange
    char path[8] = {}; // [状態] - 出力バッファーを用意する。
    cplat_error err;

    // Pre-Assert

    // Act
    int actual_ret_path_concat_n = cplat_path_concat_n(
        path, sizeof(path), &err, 2u, "ab",
        (const char *)NULL); // [手順] - 2 断片のうち 1 つに NULL を渡して cplat_path_concat_n を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_path_concat_n); // [確認_異常系] - cplat_path_concat_n の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因が返ること。
}

// 連結結果がバッファーに収まらない場合に ENAMETOOLONG で失敗することの確認
TEST_F(pathConcatTest, returns_enametoolong_when_result_does_not_fit)
{
    // Arrange
    char path[5];
    cplat_error err;

    std::memset(path, 'x',
                sizeof(path)); // [状態] - 5 バイトの出力バッファーを 'x' で埋め、クリアを検出できるようにする。

    // Pre-Assert

    // Act
    int actual_ret_path_concat =
        cplat_path_concat(path, sizeof(path), &err, "ab", "cd",
                          "e"); // [手順] - 連結結果が 5 文字 (+NUL) となる断片を渡して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret_path_concat); // [確認_異常系] - cplat_path_concat の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因が返ること。
    EXPECT_EQ('\0', path[0]); // [確認_異常系] - 失敗時は空文字列に初期化されること。
}

// 一時ディレクトリの格納先が NULL の場合に TLS へ詳細エラーが記録されることの確認
TEST_F(pathConcatTest, get_temp_dir_records_error_for_null_output)
{
    // Arrange
    cplat_error last_error;

    // Pre-Assert

    // Act
    const int result = cplat_path_get_temp_dir(NULL, 0U, NULL); // [手順] - 格納先と詳細エラー出力に NULL を指定する。
    cplat_error_get_last(&last_error);                          // [手順] - TLS に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_path_get_temp_dir の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&last_error,
                                CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - TLS の要因が EINVAL であること。
}

// 一時ディレクトリの格納先サイズが 0 の場合に引数エラーになることの確認
TEST_F(pathConcatTest, get_temp_dir_rejects_zero_output_size)
{
    // Arrange
    char output = 'x'; // [状態] - サイズ 0 の呼び出しで変更されないことを確認するため 'x' で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    int result = cplat_path_get_temp_dir(
        &output, 0u, &detail); // [手順] - 格納先サイズに 0 を指定して cplat_path_get_temp_dir を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result); // [確認_異常系] - 格納先サイズが 0 の cplat_path_get_temp_dir の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('x', output); // [確認_異常系] - サイズ 0 の格納先が変更されないこと。
}

#if defined(PLATFORM_WINDOWS)
// Windows の GetTempPathW で取得した一時ディレクトリが公開 API のパス形式になることの確認
TEST_F(pathConcatTest, get_temp_dir_returns_normalized_windows_path)
{
    // Arrange
    char output[PLATFORM_PATH_MAX] = {};
    cplat_error err;

    // Pre-Assert

    // Act
    int result =
        cplat_path_get_temp_dir(output, sizeof(output), &err); // [手順] - Windows の一時ディレクトリを取得する。

    // Assert
    ASSERT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_path_get_temp_dir の戻り値が CPLAT_OK であること。
    EXPECT_NE('\0', output[0]);  // [確認_正常系] - 一時ディレクトリの絶対パスが返ること。
    EXPECT_EQ(nullptr, std::strchr(output, '\\')); // [確認_正常系] - 出力に Windows 固有の区切り文字が残らないこと。
    EXPECT_NE(PLATFORM_PATH_SEP_CHR,
              output[std::strlen(output) - 1u]); // [確認_正常系] - 末尾の区切り文字が除去されること。
}
#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_LINUX)
// TMPDIR 未設定時に標準の一時ディレクトリが返ることの確認
TEST_F(pathConcatTest, get_temp_dir_uses_default_when_tmpdir_is_empty)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char output[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_getenv(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, char *buffer, size_t, int *, cplat_error *)
            {
                buffer[0] = '\0';
                return CPLAT_OK;
            })); // [Pre-Assert確認_正常系] - TMPDIR の取得が 1 回呼び出されること。
                 // [Pre-Assert手順] - TMPDIR として空文字列を返却する。

    // Act
    int result = cplat_path_get_temp_dir(output, sizeof(output),
                                         NULL); // [手順] - TMPDIR が空の状態で一時ディレクトリを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);  // [確認_正常系] - TMPDIR 未設定時の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/tmp", output); // [確認_正常系] - 標準の一時ディレクトリ /tmp が返ること。
}

// TMPDIR 末尾のセパレーターが除去されることの確認
TEST_F(pathConcatTest, get_temp_dir_removes_trailing_separators)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char output[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_getenv(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, char *buffer, size_t, int *, cplat_error *)
            {
                std::memcpy(buffer, "/var/tmp///", sizeof("/var/tmp///"));
                return CPLAT_OK;
            })); // [Pre-Assert確認_正常系] - TMPDIR の取得が 1 回呼び出されること。
                 // [Pre-Assert手順] - TMPDIR として末尾セパレーター付きの "/var/tmp///" を返却する。

    // Act
    int result =
        cplat_path_get_temp_dir(output, sizeof(output), NULL); // [手順] - 末尾セパレーターを含む TMPDIR を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);      // [確認_正常系] - TMPDIR 末尾セパレーターの戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/var/tmp", output); // [確認_正常系] - 末尾セパレーターを除いたパスが返ること。
}

// ルートを示す TMPDIR のセパレーターが保持されることの確認
TEST_F(pathConcatTest, get_temp_dir_preserves_root_directory)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char output[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_getenv(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, char *buffer, size_t, int *, cplat_error *)
            {
                std::memcpy(buffer, "/", sizeof("/"));
                return CPLAT_OK;
            })); // [Pre-Assert確認_正常系] - TMPDIR の取得が 1 回呼び出されること。
                 // [Pre-Assert手順] - TMPDIR としてルートディレクトリ "/" を返却する。

    // Act
    int result = cplat_path_get_temp_dir(
        output, sizeof(output), NULL); // [手順] - TMPDIR がルートディレクトリの状態で一時ディレクトリを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - ルート TMPDIR に対する cplat_path_get_temp_dir の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/", output); // [確認_正常系] - ルートディレクトリのセパレーターが保持されること。
}

// TMPDIR が取得バッファーへ収まらない場合に失敗することの確認
TEST_F(pathConcatTest, get_temp_dir_rejects_overlong_tmpdir)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char output[PLATFORM_PATH_MAX] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_getenv(_, _, _, _, _))
        .WillOnce(
            Return(CPLAT_ERR_BUFFER_TOO_SMALL)); // [Pre-Assert確認_異常系] - TMPDIR の取得が 1 回呼び出されること。
                                                 // [Pre-Assert手順] - CPLAT_ERR_BUFFER_TOO_SMALL を返却する。

    // Act
    int result =
        cplat_path_get_temp_dir(output, sizeof(output), NULL); // [手順] - 長過ぎる TMPDIR の取得結果を処理する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, result); // [確認_異常系] - 長過ぎる TMPDIR が BUFFER_TOO_SMALL になること。
    EXPECT_STREQ("", output);                      // [確認_異常系] - 失敗時の出力が空文字列になること。
}

// 一時ディレクトリの出力整形失敗がエラーになることの確認
TEST_F(pathConcatTest, get_temp_dir_reports_formatting_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char output[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_getenv(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, char *buffer, size_t, int *, cplat_error *)
            {
                std::memcpy(buffer, "/tmp", sizeof("/tmp"));
                return CPLAT_OK;
            })); // [Pre-Assert確認_正常系] - TMPDIR の取得が 1 回呼び出されること。
                 // [Pre-Assert手順] - TMPDIR として "/tmp" を返却する。
    EXPECT_CALL(mock_cplat, cplat_snprintf(_, _, _))
        .WillOnce(Return(
            CPLAT_ERR_BUFFER_TOO_SMALL)); // [Pre-Assert確認_異常系] - 一時ディレクトリの出力整形が 1 回呼び出されること。
                                          // [Pre-Assert手順] - CPLAT_ERR_BUFFER_TOO_SMALL を返却する。

    // Act
    int result =
        cplat_path_get_temp_dir(output, sizeof(output), NULL); // [手順] - 一時ディレクトリの出力整形失敗を注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, result); // [確認_異常系] - 出力整形失敗が BUFFER_TOO_SMALL になること。
}
#endif /* PLATFORM_LINUX */
