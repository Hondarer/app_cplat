#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <mock_cplat.h>
#include <mock_stdlib.h>
#include <mock_unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

#if defined(PLATFORM_LINUX)
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <direct.h>
#endif

using namespace testing;

namespace
{

#if defined(PLATFORM_LINUX)
static void stub_linux_path_resolution(Mock_unistd *mock_unistd, Mock_stdlib *mock_stdlib)
{
    ON_CALL(*mock_unistd, getcwd(_, _, _, _, _))
        .WillByDefault(
            [](const char *, int, const char *, char *buf, size_t size) -> char *
            {
                const char cwd[] = "/work";
                if (size < sizeof(cwd))
                {
                    return nullptr;
                }
                memcpy(buf, cwd, sizeof(cwd));
                return buf;
            });
    ON_CALL(*mock_stdlib, realpath(_, _, _, _, _))
        .WillByDefault(
            [](const char *, int, const char *, const char *path, char *resolved) -> char *
            {
                strcpy(resolved, path);
                return resolved;
            });
}
#endif /* PLATFORM_LINUX */

// [サブ手順 名前=pathGetFullTest.assert_path_get_full_success]
static void assert_path_get_full_success(char *path_out, size_t path_size, const char *path)
{
    cplat_error err;
    ASSERT_EQ(CPLAT_OK, cplat_path_get_full(path_out, path_size, &err, path));
    // [状態確認] - `cplat_path_get_full(path_out, path_size, &err, path)` の戻り値が `CPLAT_OK` であること。
}
// [サブ手順終了]

// [サブ手順 名前=pathGetFullTest.build_path]
static void build_path(char *path_out, size_t path_size, const char *lhs, const char *rhs)
{
    int written = std::snprintf(path_out, path_size, "%s/%s", lhs, rhs);
    ASSERT_GE(written, 0);
    // [状態確認] - `written` が `0` 以上であること。
    ASSERT_LT((size_t)written, path_size);
    // [状態確認] - `(size_t)written` が `path_size` より小さいこと。
}
// [サブ手順終了]

// [サブ手順 名前=pathGetFullTest.build_three_part_path]
static void build_three_part_path(char *path_out, size_t path_size, const char *lhs, const char *middle,
                                  const char *rhs)
{
    int written = std::snprintf(path_out, path_size, "%s/%s/%s", lhs, middle, rhs);
    ASSERT_GE(written, 0);
    // [状態確認] - `written` が `0` 以上であること。
    ASSERT_LT((size_t)written, path_size);
    // [状態確認] - `(size_t)written` が `path_size` より小さいこと。
}
// [サブ手順終了]

} // namespace

class pathGetFullTest : public Test
{
};

// パスが NULL の場合に EINVAL で失敗することの確認
TEST_F(pathGetFullTest, returns_einval_for_null_path)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {}; // [状態] - 出力バッファーを用意する。
    cplat_error err;                   // [状態] - 詳細エラーの受け取り先を用意する。
    cplat_error last_error;

    // Pre-Assert

    // Act
    int rc = cplat_path_get_full(path, sizeof(path), &err,
                                 nullptr); // [手順] - パスに NULL を渡して cplat_path_get_full を呼び出す。
    cplat_error_get_last(&last_error);     // [手順] - TLS に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              rc); // [確認_異常系] - cplat_path_get_full の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因が返ること。
    EXPECT_EQ(1, cplat_error_is_set(&last_error)); // [確認_異常系] - TLS に詳細エラーが記録されること。
    EXPECT_EQ('\0', path[0]);                      // [確認_異常系] - 出力は空文字列に初期化されること。
}

// 出力先、出力サイズ、空パスの異常入力が EINVAL になることの確認
TEST_F(pathGetFullTest, rejects_invalid_output_and_empty_path)
{
    // Arrange
    char zero_size_output = 'x'; // [状態] - サイズ 0 の呼び出しで変更されないことを確認するため 'x' で初期化する。
    char empty_path_output[8] = "stale"; // [状態] - 空パスの呼び出しで空文字列へ変更されることを確認する。
    cplat_error null_detail;
    cplat_error zero_detail;
    cplat_error empty_detail;

    // Pre-Assert

    // Act
    int null_result = cplat_path_get_full(NULL, 8u, &null_detail,
                                          "."); // [手順] - 出力先に NULL を指定して cplat_path_get_full を呼び出す。
    int zero_result = cplat_path_get_full(&zero_size_output, 0u, &zero_detail,
                                          "."); // [手順] - 出力サイズに 0 を指定して cplat_path_get_full を呼び出す。
    int empty_result = cplat_path_get_full(empty_path_output, sizeof(empty_path_output), &empty_detail,
                                           ""); // [手順] - 空パスを指定して cplat_path_get_full を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_result); // [確認_異常系] - 出力先が NULL の cplat_path_get_full の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_result); // [確認_異常系] - 出力サイズが 0 の cplat_path_get_full の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('x', zero_size_output); // [確認_異常系] - サイズ 0 の出力先が変更されないこと。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        empty_result); // [確認_異常系] - 空パスを渡した cplat_path_get_full の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("", empty_path_output); // [確認_異常系] - 空パスの失敗時に出力先が空文字列になること。
}

// カレント ディレクトリが絶対パスへ展開されることの確認
TEST_F(pathGetFullTest, expands_current_directory_to_absolute_path)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_stdlib> mock_stdlib;
    stub_linux_path_resolution(&mock_unistd,
                               &mock_stdlib); // [状態] - getcwd と realpath を既定の作業ディレクトリへ差し替える。
#endif                                        /* PLATFORM_LINUX */
    char actual[PLATFORM_PATH_MAX] = {};
    char expected[PLATFORM_PATH_MAX] = {}; // [状態] - 出力バッファーを 2 つ用意する。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=pathGetFullTest.assert_path_get_full_success]
    assert_path_get_full_success(actual, sizeof(actual), "."); // [手順] - カレント ディレクトリ "." を絶対化する。
    // [サブ手順参照 名前=pathGetFullTest.assert_path_get_full_success]
    assert_path_get_full_success(expected, sizeof(expected), actual); // [手順] - 得られた絶対パスを再度正規化する。

    // Assert
    EXPECT_STREQ(expected, actual); // [確認_正常系] - "." が絶対パスへ展開され、再正規化しても同一であること。
}

// ".." やバックスラッシュを含むセグメントが正規化されることの確認
TEST_F(pathGetFullTest, normalizes_dotdot_and_backslash_segments)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_stdlib> mock_stdlib;
    stub_linux_path_resolution(&mock_unistd,
                               &mock_stdlib); // [状態] - getcwd と realpath を既定の作業ディレクトリへ差し替える。
#endif                                        /* PLATFORM_LINUX */
    char base[PLATFORM_PATH_MAX] = {};
    char candidate[PLATFORM_PATH_MAX] = {};
    char actual[PLATFORM_PATH_MAX] = {};
    char expected[PLATFORM_PATH_MAX] = {};

    // [サブ手順参照 名前=pathGetFullTest.assert_path_get_full_success]
    assert_path_get_full_success(base, sizeof(base), ".");
    // [サブ手順参照 名前=pathGetFullTest.build_three_part_path]
    build_three_part_path(candidate, sizeof(candidate), base, "alpha\\..",
                          "beta.txt"); // [状態] - 入力を "alpha\\.." を挟んだ表記ゆれパスとする。
    // [サブ手順参照 名前=pathGetFullTest.build_path]
    build_path(expected, sizeof(expected), base, "beta.txt"); // [状態] - 期待値を正規化済みパスとする。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=pathGetFullTest.assert_path_get_full_success]
    assert_path_get_full_success(actual, sizeof(actual), candidate); // [手順] - '\\' と '..' を含むパスを正規化する。

    // Assert
    EXPECT_STREQ(expected, actual); // [確認_正常系] - セパレータと dot segment が正規化されること。
}

#if defined(PLATFORM_LINUX)
// 連続したセパレーターを含む絶対パスが正規化されることの確認
TEST_F(pathGetFullTest, normalizes_repeated_separators)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX] = {};

    // Pre-Assert

    // Act
    int result = cplat_path_get_full(actual, sizeof(actual), NULL,
                                     "/tmp//"); // [手順] - 連続したセパレーターを含む絶対パスを正規化する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);  // [確認_正常系] - cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/tmp", actual); // [確認_正常系] - 連続したセパレーターが除去されたパスになること。
}

// ルートを越える親参照と 2 文字の通常セグメントが正規化されることの確認
TEST_F(pathGetFullTest, normalizes_parent_above_root_and_two_character_segments)
{
    // Arrange
    char parent_actual[PLATFORM_PATH_MAX] = {};
    char plain_actual[PLATFORM_PATH_MAX] = {};
    char leading_dot_actual[PLATFORM_PATH_MAX] = {};
    char trailing_dot_actual[PLATFORM_PATH_MAX] = {};

    // Pre-Assert

    // Act
    int parent_result = cplat_path_get_full(parent_actual, sizeof(parent_actual), NULL,
                                            "/../../a"); // [手順] - ルートを越える親参照を含む絶対パスを正規化する。
    int plain_result = cplat_path_get_full(plain_actual, sizeof(plain_actual), NULL,
                                           "/ab"); // [手順] - 2 文字の通常セグメントを持つ絶対パスを正規化する。
    int leading_dot_result = cplat_path_get_full(leading_dot_actual, sizeof(leading_dot_actual), NULL,
                                                 "/.x"); // [手順] - 先頭だけがドットの 2 文字セグメントを正規化する。
    int trailing_dot_result = cplat_path_get_full(trailing_dot_actual, sizeof(trailing_dot_actual), NULL,
                                                  "/x."); // [手順] - 末尾だけがドットの 2 文字セグメントを正規化する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              parent_result); // [確認_正常系] - 親参照を含む cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/a", parent_actual); // [確認_正常系] - ルートを越える親参照がルートに留まり "/a" になること。
    EXPECT_EQ(
        CPLAT_OK,
        plain_result); // [確認_正常系] - 2 文字セグメントに対する cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/ab", plain_actual); // [確認_正常系] - 2 文字の通常セグメントが保持されること。
    EXPECT_EQ(
        CPLAT_OK,
        leading_dot_result); // [確認_正常系] - 先頭ドットのセグメントに対する cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/.x", leading_dot_actual); // [確認_正常系] - 先頭だけがドットのセグメントが保持されること。
    EXPECT_EQ(
        CPLAT_OK,
        trailing_dot_result); // [確認_正常系] - 末尾ドットのセグメントに対する cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/x.", trailing_dot_actual); // [確認_正常系] - 末尾だけがドットのセグメントが保持されること。
}
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)
// Windows の GetFullPathNameW で連続したセパレーターと '\\' が公開 API の形式へ正規化されることの確認
TEST_F(pathGetFullTest, normalizes_repeated_windows_separators)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX] = {};

    // Pre-Assert

    // Act
    int result = cplat_path_get_full(
        actual, sizeof(actual), NULL,
        ".\\pathGetFullTest\\\\child"); // [手順] - '\\' と連続したセパレーターを含む相対パスを絶対化する。

    // Assert
    ASSERT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_NE('\0', actual[0]);  // [確認_正常系] - 絶対パスが返ること。
    EXPECT_EQ(nullptr, std::strchr(actual, '\\')); // [確認_正常系] - '\\' が '/' に正規化されること。
    EXPECT_EQ(nullptr, std::strstr(actual, "//")); // [確認_正常系] - 連続したセパレーターが除去されること。
}
#endif /* PLATFORM_WINDOWS */

// カレント ディレクトリとの連結結果が長過ぎる場合に失敗することの確認
TEST_F(pathGetFullTest, returns_enametoolong_when_relative_path_is_too_long)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_stdlib> mock_stdlib;
    stub_linux_path_resolution(&mock_unistd,
                               &mock_stdlib); // [状態] - getcwd と realpath を既定の作業ディレクトリへ差し替える。
#endif                                        /* PLATFORM_LINUX */
    char relative[PLATFORM_PATH_MAX] = {};
    char output[PLATFORM_PATH_MAX] = {'x'};
    cplat_error err;
    std::memset(relative, 'a', sizeof(relative) - 1u);

    // Pre-Assert

    // Act
    int result =
        cplat_path_get_full(output, sizeof(output), &err,
                            relative); // [手順] - カレント ディレクトリとの連結結果が長過ぎる相対パスを指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              result); // [確認_異常系] - cplat_path_get_full の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(1, cplat_error_is(&err,
                                CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因が返ること。
    EXPECT_EQ('\0', output[0]);                              // [確認_異常系] - 失敗時に出力先が空文字列になること。
}

// 出力バッファーが小さすぎる場合に ENAMETOOLONG で失敗することの確認
TEST_F(pathGetFullTest, returns_enametoolong_when_buffer_is_too_small)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_stdlib> mock_stdlib;
    stub_linux_path_resolution(&mock_unistd,
                               &mock_stdlib); // [状態] - getcwd と realpath を既定の作業ディレクトリへ差し替える。
#endif                                        /* PLATFORM_LINUX */
    char path[4] = {};                        // [状態] - 4 バイトの小さすぎる出力バッファーを用意する。
    cplat_error err;                          // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert

    // Act
    int rc = cplat_path_get_full(path, sizeof(path), &err,
                                 "."); // [手順] - 小さすぎる出力バッファーで cplat_path_get_full を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              rc); // [確認_異常系] - cplat_path_get_full の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因が返ること。
    EXPECT_EQ('\0', path[0]);                                      // [確認_異常系] - 出力は空文字列に初期化されること。
}

// 存在しないパスでも絶対化済み文字列が返ることの確認
TEST_F(pathGetFullTest, returns_absolute_path_for_nonexistent_target)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    NiceMock<Mock_stdlib> mock_stdlib;
    stub_linux_path_resolution(&mock_unistd,
                               &mock_stdlib); // [状態] - getcwd と realpath を既定の作業ディレクトリへ差し替える。
#endif                                        /* PLATFORM_LINUX */
    char base[PLATFORM_PATH_MAX] = {};
    char candidate[PLATFORM_PATH_MAX] = {};
    char actual[PLATFORM_PATH_MAX] = {};
    char expected[PLATFORM_PATH_MAX] = {};

    // [サブ手順参照 名前=pathGetFullTest.assert_path_get_full_success]
    assert_path_get_full_success(base, sizeof(base), ".");
    // [サブ手順参照 名前=pathGetFullTest.build_three_part_path]
    build_three_part_path(candidate, sizeof(candidate), base, "ghost-dir/..",
                          "ghost.bin"); // [状態] - 入力を存在しない "ghost-dir/.." を挟んだパスとする。
    // [サブ手順参照 名前=pathGetFullTest.build_path]
    build_path(expected, sizeof(expected), base, "ghost.bin"); // [状態] - 期待値を正規化済みパスとする。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=pathGetFullTest.assert_path_get_full_success]
    assert_path_get_full_success(actual, sizeof(actual), candidate); // [手順] - 存在しないパスを絶対化する。

    // Assert
    EXPECT_STREQ(expected, actual); // [確認_正常系] - 実体解決できなくても絶対化済み文字列が返ること。
}

#if defined(PLATFORM_LINUX)
// 正規化用メモリを確保できない場合に ENOMEM で失敗することの確認
TEST_F(pathGetFullTest, returns_enomem_when_normalization_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char path[PLATFORM_PATH_MAX] = {'x'}; // [状態] - 出力バッファーを空文字列以外で初期化する。
    cplat_error err;                      // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(PLATFORM_PATH_MAX, sizeof(size_t)))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - 正規化用メモリの cplat_calloc が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_calloc から NULL を返却する。

    // Act
    int rc = cplat_path_get_full(path, sizeof(path), &err,
                                 "/missing"); // [手順] - 絶対パスを指定して cplat_path_get_full を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              rc); // [確認_異常系] - cplat_path_get_full の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_OUT_OF_MEMORY)); // [確認_異常系] - ENOMEM の要因が返ること。
    EXPECT_EQ('\0', path[0]);                                      // [確認_異常系] - 出力は空文字列に初期化されること。
}

// symlink が実体ファイルのパスへ解決されることの確認
TEST_F(pathGetFullTest, resolves_symlink_to_target_path_when_target_exists)
{
    // Arrange
    NiceMock<Mock_stdlib> mock_stdlib;
    char actual[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_stdlib, realpath(_, _, _, StrEq("/work/target-link.bin"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, char *resolved) -> char *
            {
                const char target[] = "/work/target.bin";
                memcpy(resolved, target, sizeof(target));
                return resolved;
            }); // [Pre-Assert確認_正常系] - realpath が symlink パスを指定して 1 回呼び出されること。
                // [Pre-Assert手順] - 実体ファイルのパスを書き込み、そのアドレスを返却する。

    // Act
    int result = cplat_path_get_full(actual, sizeof(actual), NULL,
                                     "/work/target-link.bin"); // [手順] - symlink のパスを絶対化する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result);              // [確認_正常系] - cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/work/target.bin", actual); // [確認_正常系] - 実体ファイルのパスへ解決されること。
}
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_LINUX)

// カレント ディレクトリの取得に失敗した場合に errno が通知されることの確認
// Windows の cplat_path_get_full は GetFullPathNameW を使うため、この失敗経路は Linux のみに存在する
TEST_F(pathGetFullTest, reports_errno_when_getcwd_fails)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error detail;             // [状態] - 詳細エラーの格納先を用意する。

    std::memset(actual, 'X', sizeof(actual));

    // Pre-Assert
    EXPECT_CALL(mock_unistd, getcwd(_, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EACCES), Return(nullptr)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - getcwd が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - errno に EACCES を設定し、1 回目は NULL を返却する。

    // Act
    int actual_ret = cplat_path_get_full(actual, sizeof(actual), &detail,
                                         "relative.txt"); // [手順] - 相対パスを指定して呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_path_get_full の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(EACCES,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno の戻り値が EACCES であること。
    EXPECT_STREQ("", actual);                  // [確認_異常系] - 出力バッファーが空文字列に初期化されること。
}

// realpath による解決に失敗しても正規化済みパスが返ることの確認
// Windows の cplat_path_get_full は GetFullPathNameW を使うため、この分岐は Linux のみに存在する
TEST_F(pathGetFullTest, falls_back_to_normalized_path_when_realpath_fails)
{
    // Arrange
    NiceMock<Mock_stdlib> mock_stdlib;
    char actual[PLATFORM_PATH_MAX] = {0}; // [状態] - 出力バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdlib, realpath(_, _, _, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - realpath が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_path_get_full(actual, sizeof(actual), NULL,
                                         "/tmp/./pathGetFullTest_fallback"); // [手順] - 絶対パスを指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_get_full の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/tmp/pathGetFullTest_fallback",
                 actual); // [確認_正常系] - realpath を使わず '.' を解消した正規化済みパスが返ること。
}

#endif /* PLATFORM_LINUX */
