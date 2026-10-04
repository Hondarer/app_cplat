#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/base/result_internal.h>

#include <errno.h>
#include <set>
#include <utility>
#include <vector>

/* result.h の値は ABI として凍結する。値を変更した場合、以下の静的検査が失敗する。 */
static_assert(CPLAT_OK == 0, "cplat: CPLAT_OK の ABI 値を変更してはなりません。");
static_assert(CPLAT_SKIPPED == 1, "cplat: CPLAT_SKIPPED の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_UNKNOWN == -1, "cplat: CPLAT_ERR_UNKNOWN の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_INVALID_ARGUMENT == -2, "cplat: CPLAT_ERR_INVALID_ARGUMENT の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_UNSUPPORTED == -3, "cplat: CPLAT_ERR_UNSUPPORTED の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_PERMISSION_DENIED == -4, "cplat: CPLAT_ERR_PERMISSION_DENIED の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_NOT_FOUND == -6, "cplat: CPLAT_ERR_NOT_FOUND の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_DUPLICATE_DEFINITION == -5,
              "cplat: CPLAT_ERR_DUPLICATE_DEFINITION の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_DUPLICATE_KEY == -7, "cplat: CPLAT_ERR_DUPLICATE_KEY の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_IDENTITY_MISMATCH == -8,
              "cplat: CPLAT_ERR_IDENTITY_MISMATCH の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_VERSION_MISMATCH == -9, "cplat: CPLAT_ERR_VERSION_MISMATCH の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_OUT_OF_MEMORY == -10, "cplat: CPLAT_ERR_OUT_OF_MEMORY の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_BUSY == -11, "cplat: CPLAT_ERR_BUSY の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_TIMEOUT == -12, "cplat: CPLAT_ERR_TIMEOUT の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_LIMIT_EXCEEDED == -13, "cplat: CPLAT_ERR_LIMIT_EXCEEDED の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_BUFFER_TOO_SMALL == -14, "cplat: CPLAT_ERR_BUFFER_TOO_SMALL の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_CORRUPT_DESCRIPTOR == -15,
              "cplat: CPLAT_ERR_CORRUPT_DESCRIPTOR の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_STORAGE_FULL == -16, "cplat: CPLAT_ERR_STORAGE_FULL の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_UNKNOWN_OPTION == -20, "cplat: CPLAT_ERR_UNKNOWN_OPTION の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_MISSING_VALUE == -21, "cplat: CPLAT_ERR_MISSING_VALUE の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_UNEXPECTED_VALUE == -22, "cplat: CPLAT_ERR_UNEXPECTED_VALUE の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_INVALID_INTEGER == -23, "cplat: CPLAT_ERR_INVALID_INTEGER の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_OUT_OF_RANGE == -24, "cplat: CPLAT_ERR_OUT_OF_RANGE の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_MISSING_REQUIRED == -25, "cplat: CPLAT_ERR_MISSING_REQUIRED の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_DUPLICATE_OPTION == -26, "cplat: CPLAT_ERR_DUPLICATE_OPTION の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_TOO_MANY_ARGUMENTS == -27,
              "cplat: CPLAT_ERR_TOO_MANY_ARGUMENTS の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_TOO_MANY_OCCURRENCES == -28,
              "cplat: CPLAT_ERR_TOO_MANY_OCCURRENCES の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_INVALID_PATTERN == -29, "cplat: CPLAT_ERR_INVALID_PATTERN の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_INVALID_ENCODING == -30, "cplat: CPLAT_ERR_INVALID_ENCODING の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_EOF == -40, "cplat: CPLAT_ERR_EOF の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_CANCELED == -41, "cplat: CPLAT_ERR_CANCELED の ABI 値を変更してはなりません。");
static_assert(CPLAT_ERR_IN_PROGRESS == -42, "cplat: CPLAT_ERR_IN_PROGRESS の ABI 値を変更してはなりません。");

/* result.h が定義するエラー コードの一覧 (CPLAT_OK を除く)。 */
static std::vector<int> all_error_codes()
{
    return std::vector<int>{CPLAT_ERR_UNKNOWN,
                            CPLAT_ERR_INVALID_ARGUMENT,
                            CPLAT_ERR_UNSUPPORTED,
                            CPLAT_ERR_PERMISSION_DENIED,
                            CPLAT_ERR_DUPLICATE_DEFINITION,
                            CPLAT_ERR_DUPLICATE_KEY,
                            CPLAT_ERR_IDENTITY_MISMATCH,
                            CPLAT_ERR_VERSION_MISMATCH,
                            CPLAT_ERR_OUT_OF_MEMORY,
                            CPLAT_ERR_BUSY,
                            CPLAT_ERR_TIMEOUT,
                            CPLAT_ERR_LIMIT_EXCEEDED,
                            CPLAT_ERR_BUFFER_TOO_SMALL,
                            CPLAT_ERR_CORRUPT_DESCRIPTOR,
                            CPLAT_ERR_STORAGE_FULL,
                            CPLAT_ERR_UNKNOWN_OPTION,
                            CPLAT_ERR_MISSING_VALUE,
                            CPLAT_ERR_UNEXPECTED_VALUE,
                            CPLAT_ERR_INVALID_INTEGER,
                            CPLAT_ERR_OUT_OF_RANGE,
                            CPLAT_ERR_MISSING_REQUIRED,
                            CPLAT_ERR_DUPLICATE_OPTION,
                            CPLAT_ERR_TOO_MANY_ARGUMENTS,
                            CPLAT_ERR_TOO_MANY_OCCURRENCES,
                            CPLAT_ERR_INVALID_PATTERN,
                            CPLAT_ERR_INVALID_ENCODING,
                            CPLAT_ERR_EOF,
                            CPLAT_ERR_CANCELED,
                            CPLAT_ERR_IN_PROGRESS};
}

class resultTest : public Test
{
};

// すべての結果コードが相異なる値であることの確認
TEST_F(resultTest, all_codes_are_distinct)
{
    // Arrange
    std::vector<int> codes = all_error_codes(); // [状態] - result.h が定義する全エラー コードを列挙する。
    std::set<int> unique_codes;

    codes.push_back(CPLAT_OK);      // [状態] - 比較対象に CPLAT_OK を加える。
    codes.push_back(CPLAT_SKIPPED); // [状態] - 比較対象に CPLAT_SKIPPED を加える。

    // Pre-Assert

    // Act
    unique_codes.insert(codes.begin(), codes.end()); // [手順] - 全コードを集合へ挿入して重複を排除する。

    // Assert
    EXPECT_EQ(codes.size(),
              unique_codes.size()); // [確認_正常系] - 重複がなく、集合の要素数が列挙したコード数と一致すること。
}

// CPLAT_OK のみが 0 で、エラー コードがすべて負値であることの確認
TEST_F(resultTest, only_ok_is_zero_and_all_errors_are_negative)
{
    // Arrange
    const std::vector<int> error_codes = all_error_codes(); // [状態] - CPLAT_OK を除く全エラー コードを列挙する。
    size_t non_negative_count = 0U;

    // Pre-Assert

    // Act
    for (int code : error_codes)
    {
        if (code >= 0)
        {
            non_negative_count++; // [手順] - 0 以上の値を持つエラー コードを数える。
        }
    }

    // Assert
    EXPECT_EQ(0, CPLAT_OK);         // [確認_正常系] - CPLAT_OK の値が 0 であること。
    EXPECT_EQ(1, CPLAT_SKIPPED);    // [確認_正常系] - CPLAT_SKIPPED の値が 1 であること。
    EXPECT_EQ(0U, non_negative_count); // [確認_正常系] - 0 以上の値を持つエラー コードが存在しないこと。
    EXPECT_FALSE(error_codes.empty()); // [確認_正常系] - 検証対象のエラー コードが 1 つ以上列挙されていること。
}

// パスまたはバッファーの長さ超過を共通結果コードへ変換できることの確認
TEST_F(resultTest, length_errors_map_to_buffer_too_small)
{
    // Arrange

    // Pre-Assert

    // Act
    const int name_too_long_result =
        cplat_internal_result_from_errno(ENAMETOOLONG);                // [手順] - ENAMETOOLONG を共通結果コードへ変換する。
    const int range_result = cplat_internal_result_from_errno(ERANGE); // [手順] - ERANGE を共通結果コードへ変換する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        name_too_long_result); // [確認_正常系] - ENAMETOOLONG の変換結果が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              range_result); // [確認_正常系] - ERANGE の変換結果が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

// 代表的な errno が対応する共通結果コードへ変換されることの確認
TEST_F(resultTest, errno_values_map_to_expected_results)
{
    // Arrange
    const std::vector<std::pair<int, int>> cases = {
        {EINVAL, CPLAT_ERR_INVALID_ARGUMENT},
        {EACCES, CPLAT_ERR_PERMISSION_DENIED},
        {EPERM, CPLAT_ERR_PERMISSION_DENIED},
        {ETIMEDOUT, CPLAT_ERR_TIMEOUT},
        {EBUSY, CPLAT_ERR_BUSY},
        {EAGAIN, CPLAT_ERR_BUSY},
        {ENOMEM, CPLAT_ERR_OUT_OF_MEMORY},
        {ENOENT, CPLAT_ERR_NOT_FOUND},
        {ENAMETOOLONG, CPLAT_ERR_BUFFER_TOO_SMALL},
        {ERANGE, CPLAT_ERR_BUFFER_TOO_SMALL},
        {EIO, CPLAT_ERR_UNKNOWN}}; // [状態] - errno と期待する共通結果コードの対応表を用意する。
    std::vector<int> actual;

    // Pre-Assert

    // Act
    for (const std::pair<int, int> &item : cases)
    {
        actual.push_back(cplat_internal_result_from_errno(item.first)); // [手順] - 対応表の errno を順番に変換する。
    }

    // Assert
    ASSERT_EQ(cases.size(), actual.size()); // [確認_正常系] - 全ての errno に変換結果が得られること。
    for (std::size_t index = 0U; index < cases.size(); ++index)
    {
        EXPECT_EQ(cases[index].second,
                  actual[index]); // [確認_正常系] - errno ごとの共通結果コードが期待値と一致すること。
    }
}
