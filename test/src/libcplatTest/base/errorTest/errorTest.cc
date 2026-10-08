#include <testfw.h>
#include <cplat/base/error.h>
#include <cplat/base/error_internal.h>
#include <cplat/base/result.h>

#include <errno.h>

#if defined(PLATFORM_LINUX)
    #include <netdb.h>
#endif

#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

/* 値はライブラリの ABI の一部であり、既存の値を変更してはならない */
static_assert(std::is_trivially_copyable<cplat_error>::value, "cplat: cplat_error must be trivially copyable");
static_assert(CPLAT_ERROR_DOMAIN_NONE == 0, "cplat: domain values are part of the ABI");
static_assert(CPLAT_ERROR_DOMAIN_ERRNO == 1, "cplat: domain values are part of the ABI");
static_assert(CPLAT_ERROR_DOMAIN_WINDOWS == 2, "cplat: domain values are part of the ABI");
static_assert(CPLAT_CAUSE_NONE == 0, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_OTHER == 1, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_NOT_FOUND == 2, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_ALREADY_EXISTS == 3, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_ACCESS_DENIED == 4, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_SHARING_VIOLATION == 5, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_NOT_A_DIRECTORY == 6, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_IS_A_DIRECTORY == 7, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_DIRECTORY_NOT_EMPTY == 8, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_NAME_TOO_LONG == 9, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_INVALID_ARGUMENT == 10, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_OUT_OF_MEMORY == 11, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_DISK_FULL == 12, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_BUSY == 13, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_TIMEOUT == 14, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_INTERRUPTED == 15, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_BROKEN_PIPE == 16, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_TOO_MANY_OPEN_FILES == 17, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_READ_ONLY == 18, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_BUFFER_TOO_SMALL == 19, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_UNSUPPORTED == 20, "cplat: cause values are part of the ABI");
static_assert(CPLAT_CAUSE_IO_ERROR == 21, "cplat: new cause values must be appended");

class errorTest : public Test
{
};

// capture_errno が domain、結果、コードを保持することの確認
TEST_F(errorTest, capture_errno_preserves_domain_result_and_code)
{
    // Arrange
    cplat_error error;

    cplat_error_clear(&error); // [状態] - 詳細エラーを空の値で初期化する。

    // Pre-Assert
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE,
              error.domain); // [状態確認] - 初期化後の domain が CPLAT_ERROR_DOMAIN_NONE であること。

    // Act
    cplat_error_capture_errno(&error, ENOENT); // [手順] - ENOENT を詳細エラーへ取り込む。

    // Assert
    EXPECT_EQ(1, cplat_error_is_set(&error)); // [確認_正常系] - cplat_error_is_set の戻り値が 1 であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_ERRNO,
              cplat_error_get_domain(&error)); // [確認_正常系] - domain が errno であること。
    EXPECT_EQ(
        ENOENT,
        cplat_error_get_errno(&error)); // [確認_正常系] - cplat_error_get_errno の戻り値が ENOENT であること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              cplat_error_to_result(&error)); // [確認_正常系] - 共通結果コードが errno から変換した値であること。
    EXPECT_EQ(CPLAT_CAUSE_NOT_FOUND,
              cplat_error_get_cause(&error)); // [確認_正常系] - ENOENT の要因が NOT_FOUND であること。
    EXPECT_EQ(1,
              cplat_error_is(&error,
                                CPLAT_CAUSE_NOT_FOUND)); // [確認_正常系] - NOT_FOUND との一致判定が 1 であること。
}

// capture_current_errno が現在の errno を保持することの確認
TEST_F(errorTest, capture_current_errno_preserves_current_value)
{
    // Arrange
    cplat_error error;

    errno = ENOENT; // [状態] - 現在の errno を ENOENT に設定する。

    // Pre-Assert

    // Act
    cplat_error_capture_current_errno(&error); // [手順] - 現在の errno を詳細エラーへ取り込む。

    // Assert
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_ERRNO,
              cplat_error_get_domain(&error)); // [確認_正常系] - error のドメインが errno であること。
    EXPECT_EQ(ENOENT, cplat_error_get_errno(
                          &error)); // [確認_正常系] - cplat_error_get_errno の戻り値が ENOENT であること。
}

// set_last がコピーし NULL でクリアすることの確認
TEST_F(errorTest, set_last_copies_saved_error_and_null_clears_it)
{
    // Arrange
    const cplat_error saved_error = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND, ENOENT};
    cplat_error copied_error;
    cplat_error cleared_error;

    cplat_error_clear_last(); // [状態] - 現在のスレッドの TLS 詳細エラーを空にする。

    // Pre-Assert

    // Act
    cplat_error_set_last(&saved_error);   // [手順] - 保存済みの詳細エラーを現在のスレッドの TLS へ設定する。
    cplat_error_get_last(&copied_error);  // [手順] - 設定後の TLS 詳細エラーを取得する。
    cplat_error_set_last(NULL);           // [手順] - NULL を指定して現在のスレッドの TLS をクリアする。
    cplat_error_get_last(&cleared_error); // [手順] - クリア後の TLS 詳細エラーを取得する。

    // Assert
    EXPECT_EQ(saved_error.domain,
              copied_error.domain); // [確認_正常系] - 設定後の TLS に保存済みの domain がコピーされること。
    EXPECT_EQ(saved_error.result,
              copied_error.result); // [確認_正常系] - 設定後の TLS に保存済みの result がコピーされること。
    EXPECT_EQ(saved_error.code,
              copied_error.code); // [確認_正常系] - 設定後の TLS に保存済みの code がコピーされること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE,
              cleared_error.domain); // [確認_正常系] - NULL 指定後の TLS の domain が空であること。
    EXPECT_EQ(CPLAT_OK,
              cleared_error.result);    // [確認_正常系] - NULL 指定後の TLS の result が CPLAT_OK であること。
    EXPECT_EQ(0UL, cleared_error.code); // [確認_正常系] - NULL 指定後の TLS の code が 0 であること。
}

// アクセサが NULL、空、ドメイン不一致を拒否することの確認
TEST_F(errorTest, accessors_reject_null_empty_and_mismatched_domain)
{
    // Arrange
    cplat_error empty;
    cplat_error windows_error = {CPLAT_ERROR_DOMAIN_WINDOWS, CPLAT_ERR_UNKNOWN, 5UL};
    cplat_error invalid_error = {CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL};
    const int invalid_domain_value = 99;

    cplat_error_capture_errno(&empty, 0); // [状態] - errno 0 を取り込んで空の値を作る。
    std::memcpy(&invalid_error.domain, &invalid_domain_value,
                sizeof(invalid_error.domain)); // [状態] - 未知のドメイン値を持つ不正な詳細エラーを用意する。

    // Pre-Assert

    // Act
    cplat_error_clear(NULL);                          // [手順] - NULL の詳細エラーをクリアする。
    cplat_error_capture_errno(NULL, ENOENT);          // [手順] - NULL の格納先へ errno を取り込む。
    cplat_error_get_last(NULL);                       // [手順] - NULL の格納先へ TLS の値を取得する。
    const int null_is_set = cplat_error_is_set(NULL); // [手順] - NULL の設定状態を取得する。
    const cplat_error_domain null_domain = cplat_error_get_domain(NULL); // [手順] - NULL のドメインを取得する。
    const int null_errno = cplat_error_get_errno(NULL);                     // [手順] - NULL から errno を取得する。
    const int null_result = cplat_error_to_result(NULL); // [手順] - NULL を共通結果コードへ変換する。
    const cplat_error_cause null_cause = cplat_error_get_cause(NULL); // [手順] - NULL の要因を取得する。
    const int null_matches = cplat_error_is(NULL, CPLAT_CAUSE_NONE);  // [手順] - NULL の要因一致を判定する。
    const int empty_is_set = cplat_error_is_set(&empty); // [手順] - 空の詳細エラーの設定状態を取得する。
    const cplat_error_domain mismatched_domain =
        cplat_error_get_domain(&windows_error); // [手順] - Windows ドメインを取得する。
    const cplat_error_cause windows_cause =
        cplat_error_get_cause(&windows_error); // [手順] - Windows ドメインの要因を取得する。
    const cplat_error_domain invalid_domain =
        cplat_error_get_domain(&invalid_error); // [手順] - 未知のドメインを取得する。
    const cplat_error_cause invalid_cause =
        cplat_error_get_cause(&invalid_error); // [手順] - 未知のドメインの要因を取得する。
    const int mismatched_errno =
        cplat_error_get_errno(&windows_error); // [手順] - Windows ドメインから errno を取得する。

    // Assert
    EXPECT_EQ(0, null_is_set); // [確認_異常系] - NULL に対する設定状態が 0 であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE,
              null_domain);   // [確認_異常系] - NULL に対するドメインが CPLAT_ERROR_DOMAIN_NONE であること。
    EXPECT_EQ(0, null_errno); // [確認_異常系] - NULL から取得した errno が 0 であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_result); // [確認_異常系] - NULL に対する変換結果が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_CAUSE_NONE,
              null_cause);      // [確認_異常系] - NULL に対する要因が CPLAT_CAUSE_NONE であること。
    EXPECT_EQ(0, null_matches); // [確認_異常系] - NULL に対する要因一致が 0 であること。
    EXPECT_EQ(0, empty_is_set); // [確認_正常系] - 空の詳細エラーの設定状態が 0 であること。
    EXPECT_EQ(CPLAT_OK,
              cplat_error_to_result(&empty)); // [確認_正常系] - 空の値に対する変換結果が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_CAUSE_NONE,
              cplat_error_get_cause(&empty)); // [確認_正常系] - 空の値の要因が CPLAT_CAUSE_NONE であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_WINDOWS,
              mismatched_domain); // [確認_正常系] - Windows ドメインがそのまま取得できること。
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(CPLAT_CAUSE_OTHER,
              windows_cause); // [確認_正常系] - Linux で Windows ドメイン エラー コード 5 の要因が OTHER になること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_EQ(CPLAT_CAUSE_ACCESS_DENIED,
              windows_cause); // [確認_正常系] - Windows で Windows ドメイン エラー コード 5 の要因が ACCESS_DENIED になること。
#endif /* PLATFORM_ */
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE,
              invalid_domain); // [確認_異常系] - 未知のドメインが NONE へ正規化されること。
    EXPECT_EQ(CPLAT_CAUSE_OTHER,
              invalid_cause);       // [確認_異常系] - 未知のドメインの要因が OTHER になること。
    EXPECT_EQ(0, mismatched_errno); // [確認_異常系] - Windows ドメインから取得した errno が 0 であること。
}

// 各 errno が単一の要因へ変換されることの確認
TEST_F(errorTest, errno_values_map_to_one_cause)
{
    // Arrange
    const std::vector<std::pair<int, cplat_error_cause>> cases = {{ENOENT, CPLAT_CAUSE_NOT_FOUND},
                                                                     {EEXIST, CPLAT_CAUSE_ALREADY_EXISTS},
                                                                     {EACCES, CPLAT_CAUSE_ACCESS_DENIED},
                                                                     {ENOTDIR, CPLAT_CAUSE_NOT_A_DIRECTORY},
                                                                     {EISDIR, CPLAT_CAUSE_IS_A_DIRECTORY},
                                                                     {ENOTEMPTY, CPLAT_CAUSE_DIRECTORY_NOT_EMPTY},
                                                                     {ENAMETOOLONG, CPLAT_CAUSE_NAME_TOO_LONG},
                                                                     {EINVAL, CPLAT_CAUSE_INVALID_ARGUMENT},
                                                                     {ENOMEM, CPLAT_CAUSE_OUT_OF_MEMORY},
                                                                     {ENOSPC, CPLAT_CAUSE_DISK_FULL},
                                                                     {EBUSY, CPLAT_CAUSE_BUSY},
                                                                     {ETIMEDOUT, CPLAT_CAUSE_TIMEOUT},
                                                                     {EINTR, CPLAT_CAUSE_INTERRUPTED},
                                                                     {EPIPE, CPLAT_CAUSE_BROKEN_PIPE},
                                                                     {EMFILE, CPLAT_CAUSE_TOO_MANY_OPEN_FILES},
                                                                     {EROFS, CPLAT_CAUSE_READ_ONLY},
                                                                     {ERANGE, CPLAT_CAUSE_BUFFER_TOO_SMALL},
                                                                     {ENOTSUP, CPLAT_CAUSE_UNSUPPORTED},
                                                                     {EIO, CPLAT_CAUSE_IO_ERROR}};
    cplat_error error;
    std::vector<cplat_error_cause> actual_causes;
    std::vector<int> actual_matches;

    // Pre-Assert

    // Act
    for (const std::pair<int, cplat_error_cause> &item : cases)
    {
        cplat_error_capture_errno(&error, item.first); // [手順] - 各 errno を順番に詳細エラーへ取り込む。
        actual_causes.push_back(cplat_error_get_cause(&error));
        actual_matches.push_back(cplat_error_is(&error, item.second));
    }

    // Assert
    for (std::size_t index = 0U; index < cases.size(); ++index)
    {
        EXPECT_EQ(cases[index].second,
                  actual_causes[index]); // [確認_正常系 回数=19] - 各 errno が対応する単一の要因へ変換されること。
        EXPECT_EQ(1,
                  actual_matches[index]); // [確認_正常系 回数=19] - 対応する要因との一致判定が 1 であること。
    }
}

// 未知の errno が OTHER へ変換されることの確認
TEST_F(errorTest, unknown_errno_maps_to_other)
{
    // Arrange
    cplat_error error;

    // Pre-Assert

    // Act
    cplat_error_capture_errno(&error, EDOM); // [手順] - 対応表にない EDOM を詳細エラーへ取り込む。

    // Assert
    EXPECT_EQ(CPLAT_CAUSE_OTHER,
              cplat_error_get_cause(&error)); // [確認_正常系] - 対応表にない errno の要因が OTHER であること。
}

#if defined(ENOSYS)
// ENOSYS が UNSUPPORTED の要因へ変換されることの確認
TEST_F(errorTest, enosys_maps_to_unsupported)
{
    // Arrange
    cplat_error error;

    // Pre-Assert

    // Act
    cplat_error_capture_errno(&error, ENOSYS); // [手順] - ENOSYS を詳細エラーへ取り込む。

    // Assert
    EXPECT_EQ(CPLAT_CAUSE_UNSUPPORTED,
              cplat_error_get_cause(&error)); // [確認_正常系] - ENOSYS の要因が UNSUPPORTED であること。
}
#endif

// errno の成功値と明示結果コードが詳細エラーへ記録されることの確認
TEST_F(errorTest, report_errno_records_success_and_explicit_result)
{
    // Arrange
    cplat_error detail;
    cplat_error last_error;

    // Pre-Assert

    // Act
    int success_result = cplat_internal_error_report_errno(&detail, 0); // [手順] - errno 0 を成功として記録する。
    int mapped_result =
        cplat_internal_error_report_errno(&detail, ENOENT); // [手順] - ENOENT を対応する結果コードへ変換して記録する。
    int explicit_result = cplat_internal_error_report_errno_as(&detail, EIO, CPLAT_ERR_BUSY);
    // [手順] - EIO に対して明示した CPLAT_ERR_BUSY を記録する。
    cplat_error_get_last(&last_error); // [手順] - 最後に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        success_result); // [確認_正常系] - errno 0 を指定した cplat_internal_error_report_errno の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        mapped_result); // [確認_正常系] - ENOENT を指定した cplat_internal_error_report_errno の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(
        CPLAT_ERR_BUSY,
        explicit_result); // [確認_正常系] - 明示結果を指定した cplat_internal_error_report_errno_as の戻り値が CPLAT_ERR_BUSY であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_ERRNO,
              detail.domain); // [確認_正常系] - 明示結果の詳細エラーが errno ドメインであること。
    EXPECT_EQ(EIO, cplat_error_get_errno(&detail)); // [確認_正常系] - 詳細エラーへ EIO が記録されること。
    EXPECT_EQ(
        CPLAT_ERR_BUSY,
        cplat_error_to_result(&last_error)); // [確認_正常系] - TLS の結果コードが CPLAT_ERR_BUSY であること。
}

// 成功報告が詳細エラーと TLS の双方をクリアすることの確認
TEST_F(errorTest, report_success_clears_detail_and_last_error)
{
    // Arrange
    cplat_error detail = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_UNKNOWN, EIO};
    cplat_error last_error;

    // Pre-Assert

    // Act
    int result = cplat_internal_error_report_success(&detail); // [手順] - 詳細エラーを成功状態へ更新する。
    cplat_error_get_last(&last_error);                // [手順] - 更新後の TLS 詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_internal_error_report_success の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE,
              detail.domain); // [確認_正常系] - 出力詳細エラーのドメインが空であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE,
              last_error.domain); // [確認_正常系] - TLS 詳細エラーのドメインが空であること。
}

// ソケット errno が待機要因と通常要因を区別して記録されることの確認
TEST_F(errorTest, report_socket_errno_uses_socket_domain_and_would_block_cause)
{
    // Arrange
    cplat_error error;
    cplat_error last_error;

    // Pre-Assert

    // Act
    int blocked_result = cplat_internal_error_report_socket_errno(&error, EAGAIN); // [手順] - EAGAIN をソケット errno として記録する。
    const cplat_error_domain blocked_domain = cplat_error_get_domain(&error); // [手順] - EAGAIN の記録ドメインを取得する。
    const cplat_error_cause blocked_cause = cplat_error_get_cause(&error); // [手順] - ソケット EAGAIN の要因を取得する。
    int success_result = cplat_internal_error_report_socket_errno(&error, 0); // [手順] - errno 0 をソケット成功として記録する。
    cplat_error_get_last(&last_error); // [手順] - ソケット成功後の TLS 詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUSY, blocked_result); // [確認_正常系] - ソケット EAGAIN の戻り値が CPLAT_ERR_BUSY であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, blocked_domain); // [確認_正常系] - EAGAIN の記録ドメインが SOCKET_ERRNO であること。
    EXPECT_EQ(CPLAT_CAUSE_WOULD_BLOCK, blocked_cause); // [確認_正常系] - ソケット EAGAIN の要因が WOULD_BLOCK であること。
    EXPECT_EQ(CPLAT_OK, success_result); // [確認_正常系] - ソケット errno 0 の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE, last_error.domain); // [確認_正常系] - ソケット成功後の TLS ドメインが NONE であること。
}

// errno 0 と非成功結果をソケット エラーとして記録するとドメインが保持されることの確認
TEST_F(errorTest, report_socket_errno_as_keeps_domain_when_result_is_not_success)
{
    // Arrange
    cplat_error error;

    // Pre-Assert

    // Act
    int result = cplat_internal_error_report_socket_errno_as(
        &error, 0, CPLAT_ERR_UNKNOWN); // [手順] - errno 0 と非成功結果をソケット エラーとして記録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_正常系] - 明示した非成功結果がそのまま返ること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_SOCKET_ERRNO,
              error.domain); // [確認_正常系] - 非成功結果のドメインが SOCKET_ERRNO であること。
}

// getaddrinfo のエラー コードが要因と結果へ分類されることの確認
TEST_F(errorTest, report_gai_error_maps_standard_codes_and_unknown_code)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    cplat_error error;
#endif

    // Pre-Assert

    // Act
#if defined(PLATFORM_LINUX)
    int not_found_result = cplat_internal_error_report_gai_error(&error, EAI_NONAME); // [手順] - EAI_NONAME を記録する。
    const cplat_error_cause not_found_cause = cplat_error_get_cause(&error); // [手順] - EAI_NONAME の要因を取得する。
    int again_result = cplat_internal_error_report_gai_error(&error, EAI_AGAIN); // [手順] - EAI_AGAIN を記録する。
    const cplat_error_cause again_cause = cplat_error_get_cause(&error); // [手順] - EAI_AGAIN の要因を取得する。
    int memory_result = cplat_internal_error_report_gai_error(&error, EAI_MEMORY); // [手順] - EAI_MEMORY を記録する。
    const cplat_error_cause memory_cause = cplat_error_get_cause(&error); // [手順] - EAI_MEMORY の要因を取得する。
    int family_result = cplat_internal_error_report_gai_error(&error, EAI_FAMILY); // [手順] - EAI_FAMILY を記録する。
    const cplat_error_cause family_cause = cplat_error_get_cause(&error); // [手順] - EAI_FAMILY の要因を取得する。
    int flags_result = cplat_internal_error_report_gai_error(&error, EAI_BADFLAGS); // [手順] - EAI_BADFLAGS を記録する。
    const cplat_error_cause flags_cause = cplat_error_get_cause(&error); // [手順] - EAI_BADFLAGS の要因を取得する。
    int unknown_result = cplat_internal_error_report_gai_error(&error, -9999); // [手順] - 未知の EAI 値を記録する。
    const cplat_error_cause unknown_cause = cplat_error_get_cause(&error); // [手順] - 未知の EAI 値の要因を取得する。
    int success_result = cplat_internal_error_report_gai_error(&error, 0); // [手順] - EAI 0 を成功として記録する。
#else
    int success_result = CPLAT_OK;
#endif

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, not_found_result); // [確認_正常系] - EAI_NONAME の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(CPLAT_CAUSE_NOT_FOUND, not_found_cause); // [確認_正常系] - EAI_NONAME の要因が NOT_FOUND であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, again_result); // [確認_正常系] - EAI_AGAIN の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_CAUSE_BUSY, again_cause); // [確認_正常系] - EAI_AGAIN の要因が BUSY であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, memory_result); // [確認_正常系] - EAI_MEMORY の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_CAUSE_OUT_OF_MEMORY, memory_cause); // [確認_正常系] - EAI_MEMORY の要因が OUT_OF_MEMORY であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, family_result); // [確認_正常系] - EAI_FAMILY の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_CAUSE_UNSUPPORTED, family_cause); // [確認_正常系] - EAI_FAMILY の要因が UNSUPPORTED であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, flags_result); // [確認_正常系] - EAI_BADFLAGS の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_CAUSE_INVALID_ARGUMENT, flags_cause); // [確認_正常系] - EAI_BADFLAGS の要因が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unknown_result); // [確認_異常系] - 未知の EAI 値の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_CAUSE_OTHER, unknown_cause); // [確認_異常系] - 未知の EAI 値の要因が OTHER であること。
#endif
    EXPECT_EQ(CPLAT_OK, success_result); // [確認_正常系] - EAI 0 の戻り値が CPLAT_OK であること。
}

#if defined(PLATFORM_LINUX)
// getaddrinfo の EAI_SYSTEM が errno に基づいて分類されることの確認
TEST_F(errorTest, report_gai_error_classifies_system_error_by_errno)
{
    // Arrange
    cplat_error error;

    // Pre-Assert

    // Act
    errno = ENOMEM;                                                       // [手順] - errno に ENOMEM を設定する。
    int memory_result = cplat_internal_error_report_gai_error(&error, EAI_SYSTEM); // [手順] - EAI_SYSTEM を記録する。
    const cplat_error_cause memory_cause =
        cplat_error_get_cause(&error); // [手順] - errno が ENOMEM の EAI_SYSTEM の要因を取得する。
    const unsigned long memory_code =
        (unsigned long)error.code; // [手順] - errno が ENOMEM の EAI_SYSTEM の生値を取得する。
    errno = EINTR;                                                          // [手順] - errno に EINTR を設定する。
    cplat_internal_error_report_gai_error(&error, EAI_SYSTEM);                     // [手順] - EAI_SYSTEM を記録する。
    const cplat_error_cause interrupted_cause =
        cplat_error_get_cause(&error); // [手順] - errno が EINTR の EAI_SYSTEM の要因を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              memory_result); // [確認_異常系] - EAI_SYSTEM の cplat_internal_error_report_gai_error の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_CAUSE_OUT_OF_MEMORY,
              memory_cause); // [確認_異常系] - errno が ENOMEM の EAI_SYSTEM の要因が OUT_OF_MEMORY であること。
    EXPECT_EQ(static_cast<unsigned long>(EAI_SYSTEM),
              memory_code); // [確認_異常系] - 生値として EAI_SYSTEM が保持されること。
    EXPECT_EQ(CPLAT_CAUSE_INTERRUPTED,
              interrupted_cause); // [確認_異常系] - errno が EINTR の EAI_SYSTEM の要因が INTERRUPTED であること。
}
#endif /* PLATFORM_LINUX */

// 詳細エラーの全ドメインと errno の追加分類が取得できることの確認
TEST_F(errorTest, accessors_cover_socket_gai_and_extended_errno_causes)
{
    // Arrange
    const std::vector<std::pair<int, cplat_error_cause>> cases = {
#if defined(EINPROGRESS)
        {EINPROGRESS, CPLAT_CAUSE_IN_PROGRESS},
#endif
#if defined(ECONNREFUSED)
        {ECONNREFUSED, CPLAT_CAUSE_CONNECTION_REFUSED},
#endif
#if defined(ECONNRESET)
        {ECONNRESET, CPLAT_CAUSE_CONNECTION_RESET},
#endif
#if defined(ECONNABORTED)
        {ECONNABORTED, CPLAT_CAUSE_CONNECTION_ABORTED},
#endif
#if defined(ENOTCONN)
        {ENOTCONN, CPLAT_CAUSE_NOT_CONNECTED},
#endif
#if defined(EISCONN)
        {EISCONN, CPLAT_CAUSE_ALREADY_CONNECTED},
#endif
#if defined(EADDRINUSE)
        {EADDRINUSE, CPLAT_CAUSE_ADDRESS_IN_USE},
#endif
#if defined(EADDRNOTAVAIL)
        {EADDRNOTAVAIL, CPLAT_CAUSE_ADDRESS_NOT_AVAILABLE},
#endif
#if defined(ENETDOWN)
        {ENETDOWN, CPLAT_CAUSE_NETWORK_DOWN},
#endif
#if defined(ENETUNREACH)
        {ENETUNREACH, CPLAT_CAUSE_NETWORK_UNREACHABLE},
#endif
#if defined(EHOSTUNREACH)
        {EHOSTUNREACH, CPLAT_CAUSE_HOST_UNREACHABLE},
#endif
#if defined(EMSGSIZE)
        {EMSGSIZE, CPLAT_CAUSE_MESSAGE_SIZE},
#endif
#if defined(ESHUTDOWN)
        {ESHUTDOWN, CPLAT_CAUSE_SHUTDOWN},
#endif
        {EAGAIN, CPLAT_CAUSE_BUSY},
        {EPERM, CPLAT_CAUSE_ACCESS_DENIED}};
    cplat_error error;
    cplat_error winsock_error = {CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, 1UL};
    cplat_error gai_error = {CPLAT_ERROR_DOMAIN_GAI, CPLAT_ERR_UNKNOWN, 1UL};

    // Pre-Assert

    // Act
    cplat_internal_error_report_socket_errno(&error, EIO); // [手順] - EIO をソケット errno として記録する。
    const cplat_error_cause socket_io_cause = cplat_error_get_cause(&error); // [手順] - ソケット EIO の要因を取得する。
    cplat_internal_error_report_gai_error(&error, 0); // [手順] - GAI 0 を記録する。
    const cplat_error_domain gai_domain = cplat_error_get_domain(&error); // [手順] - GAI 成功値のドメインを取得する。
    const cplat_error_domain winsock_domain =
        cplat_error_get_domain(&winsock_error); // [手順] - WINSOCK ドメインを取得する。
    const cplat_error_domain explicit_gai_domain =
        cplat_error_get_domain(&gai_error); // [手順] - GAI ドメインを取得する。
    const cplat_error_cause winsock_cause = cplat_error_get_cause(&winsock_error); // [手順] - 非 Windows の WINSOCK ドメイン要因を取得する。
    std::vector<cplat_error_cause> causes;
    for (const std::pair<int, cplat_error_cause> &item : cases)
    {
        cplat_error_capture_errno(&error, item.first); // [手順] - 追加 errno を順番に取り込む。
        causes.push_back(cplat_error_get_cause(&error));
    }

    // Assert
    EXPECT_EQ(CPLAT_CAUSE_IO_ERROR, socket_io_cause); // [確認_正常系] - ソケット EIO の要因が IO_ERROR であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE, gai_domain); // [確認_正常系] - GAI 成功値のドメインが NONE であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_WINSOCK,
              winsock_domain); // [確認_正常系] - WINSOCK ドメインがそのまま取得できること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_GAI, explicit_gai_domain); // [確認_正常系] - GAI ドメインがそのまま取得できること。
    EXPECT_EQ(CPLAT_CAUSE_OTHER, winsock_cause); // [確認_正常系] - Linux の WINSOCK ドメイン要因が OTHER であること。
    std::vector<cplat_error_cause> expected_causes;
    for (const std::pair<int, cplat_error_cause> &item : cases)
    {
        expected_causes.push_back(item.second);
    }
    EXPECT_EQ(expected_causes, causes); // [確認_正常系] - 追加 errno の要因列が期待値と一致すること。
}

// 詳細エラーの要因が一致しない場合に不一致となることの確認
TEST_F(errorTest, error_is_rejects_nonmatching_cause)
{
    // Arrange
    const cplat_error error = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND, ENOENT};

    // Pre-Assert

    // Act
    int matches = cplat_error_is(
        &error, CPLAT_CAUSE_ACCESS_DENIED); // [手順] - NOT_FOUND の詳細エラーを ACCESS_DENIED と比較する。

    // Assert
    EXPECT_EQ(0, matches); // [確認_異常系] - 異なる要因の比較結果が 0 であること。
}

// errno 0 と非成功結果の組合せがエラー ドメインとして保持されることの確認
TEST_F(errorTest, report_errno_as_keeps_domain_when_result_is_not_success)
{
    // Arrange
    cplat_error error;

    // Pre-Assert

    // Act
    int result = cplat_internal_error_report_errno_as(&error, 0, CPLAT_ERR_UNKNOWN); // [手順] - errno 0 と非成功結果を明示して記録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_正常系] - 明示した非成功結果がそのまま返ること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_ERRNO, error.domain); // [確認_正常系] - 非成功結果のドメインが ERRNO であること。
}
