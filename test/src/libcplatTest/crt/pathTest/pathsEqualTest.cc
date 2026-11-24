#include <testfw.h>
#include <cplat/crt/path.h>
#include <mock_cplat.h>
#if defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#elif defined(PLATFORM_LINUX)
    #include <mock_stdlib.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif /* PLATFORM_ */
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

using namespace testing;

namespace
{

// [サブ手順 名前=pathsEqualTest.assert_path_get_full_success]
static void assert_path_get_full_success(char *path_out, size_t path_size, const char *path)
{
    cplat_error err;
    ASSERT_EQ(0, cplat_path_get_full(path_out, path_size, &err, path));
    // [状態確認] - cplat_path_get_full() の戻り値が 0 であること。
}
// [サブ手順終了]

// [サブ手順 名前=pathsEqualTest.build_path]
static void build_path(char *path_out, size_t path_size, const char *lhs, const char *rhs)
{
    int written = std::snprintf(path_out, path_size, "%s/%s", lhs, rhs);
    ASSERT_GE(written, 0);
    // [状態確認] - written が 0 以上であること。
    ASSERT_LT((size_t)written, path_size);
    // [状態確認] - written が path_size より小さいこと。
}
// [サブ手順終了]

// [サブ手順 名前=pathsEqualTest.build_three_part_path]
static void build_three_part_path(char *path_out, size_t path_size, const char *lhs, const char *middle,
                                  const char *rhs)
{
    int written = std::snprintf(path_out, path_size, "%s/%s/%s", lhs, middle, rhs);
    ASSERT_GE(written, 0);
    // [状態確認] - written が 0 以上であること。
    ASSERT_LT((size_t)written, path_size);
    // [状態確認] - written が path_size より小さいこと。
}
// [サブ手順終了]

} // namespace

class pathsEqualTest : public Test
{
};

// 左辺パスが NULL の場合に EINVAL で失敗することの確認
TEST_F(pathsEqualTest, returns_einval_for_null_lhs)
{
    // Arrange
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。
    cplat_error last_error;
    int equal = 0; // [状態] - equal_out の受け取り先を 0 で初期化する。

    // Pre-Assert

    // Act
    int rc = cplat_path_equal(nullptr, ".", &equal,
                              &err); // [手順] - 左辺パスに nullptr を渡して cplat_path_equal() を呼び出す。
    cplat_error_get_last(
        &last_error); // [手順] - TLS に記録された詳細エラーの格納先として last_error を指定して cplat_error_get_last() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              rc); // [確認_異常系] - cplat_path_equal() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
    EXPECT_EQ(
        1,
        cplat_error_is_set(
            &last_error)); // [確認_異常系] - cplat_error_is_set() の戻り値が 1 であること。(TLS に詳細エラーが記録されたことの確認)
}

// 右辺パスの取得に失敗した場合にそのエラーを返すことの確認
TEST_F(pathsEqualTest, returns_error_when_rhs_path_cannot_be_resolved)
{
    // Arrange
    char lhs[PLATFORM_PATH_MAX] = {};
    cplat_error err;
    int equal = 1;

    ASSERT_EQ(CPLAT_OK, cplat_path_get_full(lhs, sizeof(lhs), nullptr,
                                            ".")); // [状態] - カレント ディレクトリから左辺パスを取得する。
    // [状態確認] - cplat_path_get_full() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_path_equal(lhs, nullptr, &equal,
                                  &err); // [手順] - 右辺に nullptr を渡して cplat_path_equal() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_path_equal() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
    EXPECT_EQ(1, equal); // [確認_異常系] - equal が 1 であること。(右辺取得に失敗して比較結果が変更されないことの確認)
}

// 比較結果の出力先が NULL の場合に不正引数で失敗することの確認
TEST_F(pathsEqualTest, returns_invalid_argument_for_null_equal_out)
{
    // Arrange
    cplat_error err;

    // Pre-Assert

    // Act
    int result = cplat_path_equal(".", ".", nullptr,
                                  &err); // [手順] - equal_out に nullptr を渡して cplat_path_equal() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_path_equal() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
}

// 相対パスと絶対パスが同じ実体なら一致と判定されることの確認
TEST_F(pathsEqualTest, compares_relative_and_absolute_current_directory_as_equal)
{
    // Arrange
    char absolute_current_dir[PLATFORM_PATH_MAX] = {};
    cplat_error err;
    int equal = 0;

    // [サブ手順参照 名前=pathsEqualTest.assert_path_get_full_success]
    assert_path_get_full_success(absolute_current_dir, sizeof(absolute_current_dir),
                                 "."); // [状態] - カレント ディレクトリの絶対パスを取得する。

    // Pre-Assert

    // Act
    int rc = cplat_path_equal(".", absolute_current_dir, &equal,
                              &err); // [手順] - 相対パス "." と絶対パスを指定して cplat_path_equal() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - cplat_path_equal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, equal);     // [確認_正常系] - equal が 1 であること。(一致であることの確認)
}

// ".." やバックスラッシュを含むパスが正規化されて比較されることの確認
TEST_F(pathsEqualTest, normalizes_dotdot_and_backslash_segments_before_comparing)
{
    // Arrange
    char base[PLATFORM_PATH_MAX] = {};
    char lhs[PLATFORM_PATH_MAX] = {};
    char rhs[PLATFORM_PATH_MAX] = {};
    cplat_error err;
    int equal = 0;

    // [サブ手順参照 名前=pathsEqualTest.assert_path_get_full_success]
    assert_path_get_full_success(base, sizeof(base), ".");
    // [サブ手順参照 名前=pathsEqualTest.build_three_part_path]
    build_three_part_path(lhs, sizeof(lhs), base, "alpha\\..",
                          "beta.txt"); // [状態] - 左辺を "alpha\\.." を挟んだ表記ゆれパスとする。
    // [サブ手順参照 名前=pathsEqualTest.build_path]
    build_path(rhs, sizeof(rhs), base, "beta.txt"); // [状態] - 右辺を正規化済みの同一実体パスとする。

    // Pre-Assert

    // Act
    int rc = cplat_path_equal(lhs, rhs, &equal,
                              &err); // [手順] - 表記ゆれのある 2 つのパスを指定して cplat_path_equal() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - cplat_path_equal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, equal);     // [確認_正常系] - equal が 1 であること。(正規化後に一致であることの確認)
}

// 異なるパスが不一致と判定されることの確認
TEST_F(pathsEqualTest, returns_zero_for_different_paths)
{
    // Arrange
    char base[PLATFORM_PATH_MAX] = {};
    char lhs[PLATFORM_PATH_MAX] = {};
    char rhs[PLATFORM_PATH_MAX] = {};
    cplat_error err;
    int equal = 1;

    // [サブ手順参照 名前=pathsEqualTest.assert_path_get_full_success]
    assert_path_get_full_success(base, sizeof(base), ".");
    // [サブ手順参照 名前=pathsEqualTest.build_path]
    build_path(lhs, sizeof(lhs), base, "alpha.bin"); // [状態] - 左辺を "alpha.bin" とする。
    // [サブ手順参照 名前=pathsEqualTest.build_path]
    build_path(rhs, sizeof(rhs), base, "beta.bin"); // [状態] - 右辺を "beta.bin" とする。

    // Pre-Assert

    // Act
    int rc = cplat_path_equal(lhs, rhs, &equal,
                              &err); // [手順] - 異なる 2 つのパスを指定して cplat_path_equal() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - cplat_path_equal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, equal);     // [確認_正常系] - equal が 0 であること。(不一致であることの確認)
}

#if defined(PLATFORM_LINUX)
// 左辺パスの正規化用メモリを確保できない場合に ENOMEM で失敗することの確認
TEST_F(pathsEqualTest, returns_enomem_when_lhs_normalization_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。
    int equal = 0;   // [状態] - equal_out の受け取り先を 0 で初期化する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(PLATFORM_PATH_MAX, sizeof(size_t)))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc() が PLATFORM_PATH_MAX、sizeof を指定して 1 回呼び出されること。(左辺パスの正規化用メモリを確保する呼び出しであることの確認)
                       // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    int rc = cplat_path_equal("/lhs", "/rhs", &equal,
                              &err); // [手順] - 2 つの絶対パスを指定して cplat_path_equal() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              rc); // [確認_異常系] - cplat_path_equal() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_OUT_OF_MEMORY)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_OUT_OF_MEMORY であることの確認)
}

// Linux では大小文字を区別して比較されることの確認
TEST_F(pathsEqualTest, keeps_case_sensitive_comparison_on_linux)
{
    // Arrange
    char base[PLATFORM_PATH_MAX] = {};
    char lhs[PLATFORM_PATH_MAX] = {};
    char rhs[PLATFORM_PATH_MAX] = {};
    cplat_error err;
    int equal = 1;

    // [サブ手順参照 名前=pathsEqualTest.assert_path_get_full_success]
    assert_path_get_full_success(base, sizeof(base), ".");
    // [サブ手順参照 名前=pathsEqualTest.build_path]
    build_path(lhs, sizeof(lhs), base, "Case.bin"); // [状態] - 左辺を "Case.bin" とする。
    // [サブ手順参照 名前=pathsEqualTest.build_path]
    build_path(rhs, sizeof(rhs), base, "case.bin"); // [状態] - 右辺を大小文字だけが異なる "case.bin" とする。

    // Pre-Assert

    // Act
    int rc = cplat_path_equal(lhs, rhs, &equal,
                              &err); // [手順] - 大小文字だけが異なるパスを指定して cplat_path_equal() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - cplat_path_equal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, equal);     // [確認_正常系] - equal が 0 であること。(大小文字を区別して不一致であることの確認)
}

// 非 ASCII の大文字小文字だけが違うパスが Linux で不一致と判定されることの確認
TEST_F(pathsEqualTest, distinguishes_non_ascii_case_differences_on_linux)
{
    // Arrange
    cplat_error err;
    int equal = 1;

    // Pre-Assert

    // Act
    int rc = cplat_path_equal("/tmp/\xC3\x84.txt", "/tmp/\xC3\xA4.txt", &equal,
                              &err); // [手順] - "Ä.txt" と "ä.txt" で cplat_path_equal() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, equal);     // [確認_正常系] - 非 ASCII の大小文字が区別されること。
}

// シンボリック リンクとリンク先は一致し、別名のハード リンクは不一致であることの確認 (Linux)
TEST_F(pathsEqualTest, resolves_symlinks_but_not_hard_links_on_linux)
{
    // Arrange
    std::string dir = "/tmp/cplat_paths_equal_link_test";
    std::string target = dir + "/target.txt";
    std::string sym = dir + "/sym.txt";
    std::string hard = dir + "/hard.txt";
    std::error_code ec;
    (void)std::filesystem::remove_all(dir, ec);
    ASSERT_TRUE(std::filesystem::create_directories(dir, ec));
    {
        int fd = ::open(target.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        ASSERT_GE(fd, 0);
        (void)::close(fd);
    }
    ASSERT_EQ(0, ::symlink(target.c_str(), sym.c_str()));
    ASSERT_EQ(0, ::link(target.c_str(), hard.c_str()));

    cplat_error err;
    int sym_equal = 0;
    int hard_equal = 1;

    // Pre-Assert

    // Act
    int sym_rc = cplat_path_equal(target.c_str(), sym.c_str(), &sym_equal, &err);
    int hard_rc = cplat_path_equal(target.c_str(), hard.c_str(), &hard_equal, &err);

    // Assert
    EXPECT_EQ(CPLAT_OK, sym_rc); // [確認_正常系] - シンボリック リンクの比較が成功すること。
    EXPECT_EQ(1, sym_equal);     // [確認_正常系] - リンク先と一致と判定されること。
    EXPECT_EQ(CPLAT_OK, hard_rc); // [確認_正常系] - ハード リンクの比較が成功すること。
    EXPECT_EQ(0, hard_equal);     // [確認_正常系] - 別名のハード リンクは不一致と判定されること。

    // Cleanup
    (void)std::filesystem::remove_all(dir, ec);
}

// 存在しないパスでも正規化後の絶対パスで一致・不一致を判定し、realpath 失敗時も比較できることの確認 (Linux)
TEST_F(pathsEqualTest, compares_nonexistent_and_realpath_failing_paths_on_linux)
{
    // Arrange
    NiceMock<Mock_stdlib> mock_stdlib;
    cplat_error err;
    int nonexist_equal = 0;
    int nonexist_diff = 1;
    int fallback_equal = 0;

    // Pre-Assert
    EXPECT_CALL(mock_stdlib, realpath(_, _, _, _, _))
        .WillRepeatedly(Invoke(
            [](const char *, int, const char *, const char *, char *) -> char *
            {
                errno = ENOENT;
                return nullptr;
            })); // [Pre-Assert確認_正常系] - realpath() が失敗しても正規化候補で比較が継続すること。

    // Act
    int rc_eq = cplat_path_equal("/nonexistent_cplat/dir/../a.txt", "/nonexistent_cplat/a.txt", &nonexist_equal, &err);
    int rc_ne = cplat_path_equal("/nonexistent_cplat/a.txt", "/nonexistent_cplat/b.txt", &nonexist_diff, &err);
    int rc_fb = cplat_path_equal("/tmp/sub/../x.txt", "/tmp/x.txt", &fallback_equal, &err);

    // Assert
    EXPECT_EQ(CPLAT_OK, rc_eq);        // [確認_正常系] - 存在しない同一正規化パスの比較が成功すること。
    EXPECT_EQ(1, nonexist_equal);      // [確認_正常系] - 一致と判定されること。
    EXPECT_EQ(CPLAT_OK, rc_ne);        // [確認_正常系] - 存在しない異なるパスの比較が成功すること。
    EXPECT_EQ(0, nonexist_diff);       // [確認_正常系] - 不一致と判定されること。
    EXPECT_EQ(CPLAT_OK, rc_fb);        // [確認_正常系] - realpath() 失敗時の比較が成功すること。
    EXPECT_EQ(1, fallback_equal);      // [確認_正常系] - 正規化済み候補で一致すること。
}
#elif defined(PLATFORM_WINDOWS)
// Windows では大小文字差を無視して比較されることの確認
TEST_F(pathsEqualTest, ignores_case_differences_on_windows)
{
    // Arrange
    char base[PLATFORM_PATH_MAX] = {};
    char lhs[PLATFORM_PATH_MAX] = {};
    char rhs[PLATFORM_PATH_MAX] = {};
    cplat_error err;
    int equal = 0;

    // [サブ手順参照 名前=pathsEqualTest.assert_path_get_full_success]
    assert_path_get_full_success(base, sizeof(base), ".");
    // [サブ手順参照 名前=pathsEqualTest.build_path]
    build_path(lhs, sizeof(lhs), base, "Case.bin"); // [状態] - 左辺を "Case.bin" とする。
    // [サブ手順参照 名前=pathsEqualTest.build_path]
    build_path(rhs, sizeof(rhs), base, "case.bin"); // [状態] - 右辺を大小文字だけが異なる "case.bin" とする。

    // Pre-Assert

    // Act
    int rc = cplat_path_equal(lhs, rhs, &equal,
                              &err); // [手順] - 大小文字だけが異なるパスを指定して cplat_path_equal() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - cplat_path_equal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, equal);     // [確認_正常系] - equal が 1 であること。(大小文字差を無視して一致であることの確認)
}

// 260 バイト超の同一非 ASCII パスが一致と判定されることの確認 (Windows)
TEST_F(pathsEqualTest, compares_non_ascii_paths_over_260_bytes_windows)
{
    // Arrange
    std::string suffix;
    for (int i = 0; i < 100; ++i)
    {
        suffix += "\xE3\x81\x82";
    }
    std::string lhs = "C:/" + suffix;
    std::string rhs =
        "C:\\" + suffix; // [状態] - 260 バイト超の同一非 ASCII パスを '/' 区切りと '\\' 区切りで用意する。
    cplat_error err;
    int equal = 0;

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_equal(
        lhs.c_str(), rhs.c_str(), &equal,
        &err); // [手順] - '/' 区切りと '\\' 区切りの非 ASCII パスを指定して cplat_path_equal() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_equal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, equal);             // [確認_正常系] - equal が 1 であること。
}

// Windows で ASCII 外の Unicode 大文字と小文字を同一視し、異なる文字や長さ違いを不一致とすることの確認
TEST_F(pathsEqualTest, compares_unicode_case_equivalences_and_differences_on_windows)
{
    // Arrange
    cplat_error err;
    int umlaut_equal = 0;
    int greek_equal = 0;
    int diff_equal = 1;
    int length_equal = 1;
    int long_unicode_equal = 0;

    std::string long_upper = "C:/";
    std::string long_lower = "C:/";
    for (int i = 0; i < 240; ++i)
    {
        long_upper += "\xC3\x84";
        long_lower += "\xC3\xA4";
    }

    // Pre-Assert

    // Act
    int rc_umlaut = cplat_path_equal("C:/tmp/\xC3\x84.txt", "c:/TMP/\xC3\xA4.txt", &umlaut_equal, &err);
    int rc_greek = cplat_path_equal("C:/tmp/\xCE\xA9.txt", "C:/tmp/\xCF\x89.txt", &greek_equal, &err);
    int rc_diff = cplat_path_equal("C:/tmp/a.txt", "C:/tmp/b.txt", &diff_equal, &err);
    int rc_len = cplat_path_equal("C:/tmp/a.txt", "C:/tmp/ab.txt", &length_equal, &err);
    int rc_long = cplat_path_equal(long_upper.c_str(), long_lower.c_str(), &long_unicode_equal, &err);

    // Assert
    EXPECT_EQ(CPLAT_OK, rc_umlaut);      // [確認_正常系] - Ä と ä の比較が成功すること。
    EXPECT_EQ(1, umlaut_equal);          // [確認_正常系] - Ä と ä が一致と判定されること。
    EXPECT_EQ(CPLAT_OK, rc_greek);       // [確認_正常系] - Ω と ω の比較が成功すること。
    EXPECT_EQ(1, greek_equal);           // [確認_正常系] - Ω と ω が一致と判定されること。
    EXPECT_EQ(CPLAT_OK, rc_diff);        // [確認_正常系] - a.txt と b.txt の比較が成功すること。
    EXPECT_EQ(0, diff_equal);            // [確認_正常系] - a.txt と b.txt が不一致であること。
    EXPECT_EQ(CPLAT_OK, rc_len);         // [確認_正常系] - 長さ違いの比較が成功すること。
    EXPECT_EQ(0, length_equal);          // [確認_正常系] - 長さ違いが不一致であること。
    EXPECT_EQ(CPLAT_OK, rc_long);        // [確認_正常系] - PLATFORM_PATH_MAX 付近の非 ASCII 比較が成功すること。
    EXPECT_EQ(1, long_unicode_equal);    // [確認_正常系] - 長い非 ASCII パスも大小文字同一視で一致すること。
}

// Windows で UTF-8 変換失敗および CompareStringOrdinal 失敗時に equal_out を変更しないことの確認
TEST_F(pathsEqualTest, preserves_equal_out_when_conversion_or_compare_fails_on_windows)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_error err_conv;
    cplat_error err_cmp;
    int equal_conv = 77;
    int equal_cmp = 88;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CompareStringOrdinal(_, _, _, _, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - CompareStringOrdinal() が 0 を返すこと。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_INVALID_PARAMETER)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼ばれること。

    // Act
    int rc_conv = cplat_path_equal("C:/tmp/\x80.txt", "C:/tmp/a.txt", &equal_conv, &err_conv);
    int rc_cmp = cplat_path_equal("C:/tmp/a.txt", "C:/tmp/a.txt", &equal_cmp, &err_cmp);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, rc_conv); // [確認_異常系] - 不正な UTF-8 は CPLAT_ERR_INVALID_ENCODING を返すこと。
    EXPECT_EQ(77, equal_conv); // [確認_異常系] - equal_conv が変更されないこと。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, rc_cmp); // [確認_異常系] - CompareStringOrdinal 失敗が変換されること。
    EXPECT_EQ(88, equal_cmp); // [確認_異常系] - equal_cmp が変更されないこと。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_WINDOWS, cplat_error_get_domain(&err_cmp)); // [確認_異常系] - Win32 詳細エラーが記録されること。
}
#endif
