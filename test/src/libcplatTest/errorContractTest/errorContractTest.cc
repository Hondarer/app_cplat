#include <testfw.h>
#include <cplat/base/error.h>
#include <cplat/base/error_message.h>
#include <cplat/base/result.h>
#include <cplat/crt/fcntl.h>
#include <cplat/crt/file.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/sys/stat.h>
#include <cplat/crt/unistd.h>
#include <cplat/mmap/mmap.h>
#include <cplat/sync/sync.h>

#include <errno.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

typedef struct tls_thread_case
{
    cplat_error observed_error;
    cplat_error_cause requested_cause;
    int call_completed;
} tls_thread_case;

typedef struct detail_out_null_case
{
    const char *name;
    void (*invoke)(void);
    int expected_tls_set;
    unsigned int pad; /* 明示的アラインメント */
} detail_out_null_case;

static void invoke_vopen_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vopen_fmt(0, 0, nullptr, format, args);
    va_end(args);
}

static void invoke_vfopen_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vfopen_fmt(nullptr, nullptr, format, args);
    va_end(args);
}

static void invoke_vremove_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vremove_fmt(nullptr, format, args);
    va_end(args);
}

static void invoke_vstat_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vstat_fmt(nullptr, nullptr, format, args);
    va_end(args);
}

static void invoke_vmkdir_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vmkdir_fmt(nullptr, format, args);
    va_end(args);
}

static void invoke_vaccess_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vaccess_fmt(0, nullptr, format, args);
    va_end(args);
}

static void record_thread_local_error(void *arg)
{
    tls_thread_case *test_case = static_cast<tls_thread_case *>(arg);

    if (test_case->requested_cause == CPLAT_CAUSE_NOT_FOUND)
    {
        (void)cplat_fopen("cplat_error_tls_missing_file", "rb", nullptr);
    }
    else
    {
        (void)cplat_path_get_full(nullptr, 0U, nullptr, nullptr);
    }

    cplat_error_get_last(&test_case->observed_error);
    test_case->call_completed = 1;
}

class errorContractTest : public Test
{
};

// 全公開 API が detail_out の NULL を受け付けることの確認
TEST_F(errorContractTest, all_detail_out_apis_accept_null)
{
    // Arrange
    const std::vector<detail_out_null_case> cases = {
        {"cplat_open", []() { (void)cplat_open(nullptr, 0, 0, nullptr); }, 1, 0U},
        {"cplat_open_fmt", []() { (void)cplat_open_fmt(0, 0, nullptr, nullptr); }, 1, 0U},
        {"cplat_vopen_fmt", []() { invoke_vopen_fmt_with_null_detail(nullptr); }, 1, 0U},
        {"cplat_file_open", []() { (void)cplat_file_open(nullptr, nullptr, 0, nullptr); }, 1, 0U},
        {"cplat_file_write", []() { (void)cplat_file_write(nullptr, nullptr, 1U, nullptr); }, 1, 0U},
        {"cplat_file_read", []() { (void)cplat_file_read(nullptr, nullptr, 1U, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_get_size", []() { (void)cplat_file_get_size(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_set_size", []() { (void)cplat_file_set_size(nullptr, 0U, nullptr); }, 1, 0U},
        {"cplat_file_get_id", []() { (void)cplat_file_get_id(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_get_path_id", []() { (void)cplat_file_get_path_id(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_get_modified_timestamp",
         []() { (void)cplat_file_get_modified_timestamp(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_set_modified_timestamp",
         []() { (void)cplat_file_set_modified_timestamp(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_get_path_modified_timestamp",
         []() { (void)cplat_file_get_path_modified_timestamp(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_set_path_modified_timestamp",
         []() { (void)cplat_file_set_path_modified_timestamp(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_file_flush", []() { (void)cplat_file_flush(nullptr, nullptr); }, 1, 0U},
        {"cplat_file_close", []() { (void)cplat_file_close(nullptr, nullptr); }, 1, 0U},
        {"cplat_path_get_full", []() { (void)cplat_path_get_full(nullptr, 0U, nullptr, nullptr); }, 1, 0U},
        {"cplat_path_equal", []() { (void)cplat_path_equal(nullptr, nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_path_get_temp_dir", []() { (void)cplat_path_get_temp_dir(nullptr, 0U, nullptr); }, 1, 0U},
        {"cplat_path_concat_n", []() { (void)cplat_path_concat_n(nullptr, 0U, nullptr, 1U, "x"); }, 1, 0U},
        {"cplat_path_concat", []() { (void)cplat_path_concat(nullptr, 0U, nullptr, "x"); }, 1, 0U},
        {"cplat_path_dirname", []() { (void)cplat_path_dirname(nullptr, 0U, nullptr, nullptr); }, 1, 0U},
        {"cplat_path_strip_extension", []() { (void)cplat_path_strip_extension(nullptr, 0U, nullptr, nullptr); }, 1,
         0U},
        {"cplat_path_join_n", []() { (void)cplat_path_join_n(nullptr, 0U, nullptr, 1U, "x"); }, 1, 0U},
        {"cplat_path_join", []() { (void)cplat_path_join(nullptr, 0U, nullptr, "x"); }, 1, 0U},
        {"cplat_fopen", []() { (void)cplat_fopen(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_freopen", []() { (void)cplat_freopen(nullptr, nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_fclose", []() { (void)cplat_fclose(nullptr, nullptr); }, 1, 0U},
        {"cplat_fflush", []() { (void)cplat_fflush(nullptr, nullptr); }, -1, 0U},
        {"cplat_fread", []() { (void)cplat_fread(nullptr, 1U, 1U, nullptr, nullptr); }, 1, 0U},
        {"cplat_fwrite", []() { (void)cplat_fwrite(nullptr, 1U, 1U, nullptr, nullptr); }, 1, 0U},
        {"cplat_remove", []() { (void)cplat_remove(nullptr, nullptr); }, 1, 0U},
        {"cplat_rename", []() { (void)cplat_rename(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_fopen_fmt", []() { (void)cplat_fopen_fmt(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_vfopen_fmt", []() { invoke_vfopen_fmt_with_null_detail(nullptr); }, 1, 0U},
        {"cplat_remove_fmt", []() { (void)cplat_remove_fmt(nullptr, nullptr); }, 1, 0U},
        {"cplat_vremove_fmt", []() { invoke_vremove_fmt_with_null_detail(nullptr); }, 1, 0U},
        {"cplat_fopen_temp", []() { (void)cplat_fopen_temp(nullptr, nullptr, nullptr, 0U, nullptr); }, 1, 0U},
        {"cplat_getenv", []() { (void)cplat_getenv(nullptr, nullptr, 0U, nullptr, nullptr); }, 1, 0U},
        {"cplat_setenv", []() { (void)cplat_setenv(nullptr, nullptr, 0, nullptr); }, 1, 0U},
        {"cplat_unsetenv", []() { (void)cplat_unsetenv(nullptr, nullptr); }, 1, 0U},
        {"cplat_lseek", []() { (void)cplat_lseek(-1, 0, 0, nullptr); }, 1, 0U},
        {"cplat_close", []() { (void)cplat_close(-1, nullptr); }, 1, 0U},
        {"cplat_dup", []() { (void)cplat_dup(-1, nullptr); }, 1, 0U},
        {"cplat_dup2", []() { (void)cplat_dup2(-1, -1, nullptr); }, 1, 0U},
        {"cplat_read", []() { (void)cplat_read(-1, nullptr, 1U, nullptr); }, 1, 0U},
        {"cplat_write", []() { (void)cplat_write(-1, nullptr, 1U, nullptr); }, 1, 0U},
        {"cplat_access", []() { (void)cplat_access(nullptr, 0, nullptr); }, 1, 0U},
        {"cplat_access_fmt", []() { (void)cplat_access_fmt(0, nullptr, nullptr); }, 1, 0U},
        {"cplat_vaccess_fmt", []() { invoke_vaccess_fmt_with_null_detail(nullptr); }, 1, 0U},
        {"cplat_stat", []() { (void)cplat_stat(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_mkdir", []() { (void)cplat_mkdir(nullptr, nullptr); }, 1, 0U},
        {"cplat_makedirs", []() { (void)cplat_makedirs(nullptr, nullptr); }, 1, 0U},
        {"cplat_rmdir", []() { (void)cplat_rmdir(nullptr, nullptr); }, 1, 0U},
        {"cplat_stat_fmt", []() { (void)cplat_stat_fmt(nullptr, nullptr, nullptr); }, 1, 0U},
        {"cplat_vstat_fmt", []() { invoke_vstat_fmt_with_null_detail(nullptr); }, 1, 0U},
        {"cplat_mkdir_fmt", []() { (void)cplat_mkdir_fmt(nullptr, nullptr); }, 1, 0U},
        {"cplat_vmkdir_fmt", []() { invoke_vmkdir_fmt_with_null_detail(nullptr); }, 1, 0U},
        {"cplat_mmap_attach",
         []() { (void)cplat_mmap_attach(nullptr, CPLAT_MMAP_ACCESS_READ_ONLY, 0U, nullptr, nullptr); }, 1, 0U},
        {"cplat_mmap_flush", []() { (void)cplat_mmap_flush(nullptr, nullptr, 0U, nullptr); }, 1, 0U},
        {"cplat_mmap_detach", []() { (void)cplat_mmap_detach(nullptr, nullptr); }, 0, 0U},
    }; // [状態] - detail_out に nullptr を指定する公開関数 59 件と公開マクロ 2 件を用意する。
    std::vector<int> tls_set_results;

    // Pre-Assert

    // Act
    for (const detail_out_null_case &item : cases)
    {
        cplat_error last_error;

        cplat_error_clear_last();
        item.invoke();
        cplat_error_get_last(&last_error);
        tls_set_results.push_back(cplat_error_is_set(&last_error));
    } // [手順] - 全公開 API の detail_out に nullptr を指定し、呼び出し後の TLS を取得する。

    // Assert
    EXPECT_EQ(cases.size(),
              tls_set_results
                  .size()); // [確認_正常系] - tls_set_results.size() の戻り値が cases.size() の戻り値と一致すること。
    for (std::size_t index = 0U; index < cases.size(); ++index)
    {
        if (cases[index].expected_tls_set >= 0)
        {
            EXPECT_EQ(cases[index].expected_tls_set, tls_set_results[index]) << cases[index].name;
            // [確認_正常系 回数=60] - tls_set_results[index] が cases[index].expected_tls_set と一致すること。(各公開 API が nullptr の detail_out を参照せず、TLS を規約どおり更新したことの確認)
        }
    }
}

// 実 API が失敗を記録し成功でクリアすることの確認
TEST_F(errorContractTest, real_api_records_failure_and_clears_it_on_success)
{
    // Arrange
    char temp_path[PLATFORM_PATH_MAX] = {};
    cplat_error failure_error;
    cplat_error success_error;

    (void)cplat_remove("cplat_error_tls_missing_file",
                       nullptr); // [状態] - 失敗対象のファイルが存在しない状態にする。

    // Pre-Assert

    // Act
    FILE *missing =
        cplat_fopen("cplat_error_tls_missing_file", "rb",
                    nullptr); // [手順] - 詳細エラー出力なしで存在しないファイルを指定して cplat_fopen() を呼び出す。
    cplat_error_get_last(&failure_error); // [手順] - 失敗直後の状態で cplat_error_get_last() を呼び出す。
    FILE *existing = cplat_fopen_temp("err", "w+b", temp_path, sizeof(temp_path),
                                      nullptr); // [手順] - 詳細エラー出力なしで cplat_fopen_temp() を呼び出す。
    cplat_error_get_last(&success_error);       // [手順] - 成功直後の状態で cplat_error_get_last() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              missing); // [確認_異常系] - 存在しないファイルに対する cplat_fopen() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &failure_error,
            CPLAT_CAUSE_NOT_FOUND)); // [確認_異常系] - CPLAT_CAUSE_NOT_FOUND に対する失敗直後の cplat_error_is() の戻り値が 1 であること。
    ASSERT_NE(nullptr, existing);    // [確認_正常系] - cplat_fopen_temp() の戻り値が nullptr 以外であること。
    EXPECT_EQ(
        0,
        cplat_error_is_set(
            &success_error)); // [確認_正常系] - 成功直後の cplat_error_is_set() の戻り値が 0 であること。(TLS 詳細エラーが空であることの確認)

    // Cleanup
    (void)fclose(existing);
    (void)cplat_remove(temp_path, nullptr);
}

// ネストした成功が直前の失敗をクリアすることの確認
TEST_F(errorContractTest, nested_paths_equal_success_clears_previous_failure)
{
    // Arrange
    int equal = 0;
    cplat_error last_error;

    (void)cplat_fopen("cplat_error_tls_missing_file", "rb", nullptr); // [状態] - TLS へ失敗を記録する。

    // Pre-Assert

    // Act
    const int result = cplat_path_equal(
        ".", ".", &equal, nullptr);    // [手順] - 相対パス "." どうしを指定して cplat_path_equal() を呼び出す。
    cplat_error_get_last(&last_error); // [手順] - 比較成功直後の状態で cplat_error_get_last() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);   // [確認_正常系] - cplat_path_equal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, equal); // [確認_正常系] - equal が 1 であること。(同じパスの比較結果が一致であることの確認)
    EXPECT_EQ(
        0,
        cplat_error_is_set(
            &last_error)); // [確認_正常系] - ネストした成功後の cplat_error_is_set() の戻り値が 0 であること。(TLS 詳細エラーが空であることの確認)
}

// スレッド間で last error が独立することの確認
TEST_F(errorContractTest, last_error_is_isolated_between_threads)
{
    // Arrange
    tls_thread_case not_found_case = {{CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL}, CPLAT_CAUSE_NOT_FOUND, 0};
    tls_thread_case invalid_case = {{CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL}, CPLAT_CAUSE_INVALID_ARGUMENT, 0};
    cplat_thread *not_found_thread = nullptr;
    cplat_thread *invalid_thread = nullptr;
    cplat_error main_error;
    int join_not_found_result = CPLAT_ERR_UNKNOWN;
    int join_invalid_result = CPLAT_ERR_UNKNOWN;

    (void)cplat_remove("cplat_error_tls_missing_file",
                       nullptr); // [状態] - NOT_FOUND 用のファイルが存在しない状態にする。
    cplat_error_clear_last(); // [状態] - メイン スレッドの TLS 詳細エラーを空にする。

    // Pre-Assert

    // Act
    const int create_not_found_result = cplat_thread_create(
        &not_found_thread, record_thread_local_error,
        &not_found_case); // [手順] - NOT_FOUND を記録する関数を指定して cplat_thread_create() を呼び出す。
    const int create_invalid_result = cplat_thread_create(
        &invalid_thread, record_thread_local_error,
        &invalid_case); // [手順] - INVALID_ARGUMENT を記録する関数を指定して cplat_thread_create() を呼び出す。
    if (create_not_found_result == CPLAT_OK)
    {
        join_not_found_result = cplat_thread_join(
            not_found_thread,
            CPLAT_SYNC_WAIT_FOREVER); // [手順] - NOT_FOUND スレッドを指定して cplat_thread_join() を呼び出す。
    }
    if (create_invalid_result == CPLAT_OK)
    {
        join_invalid_result = cplat_thread_join(
            invalid_thread,
            CPLAT_SYNC_WAIT_FOREVER); // [手順] - INVALID_ARGUMENT スレッドを指定して cplat_thread_join() を呼び出す。
    }
    cplat_error_get_last(&main_error); // [手順] - 子スレッド終了後の状態で cplat_error_get_last() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        create_not_found_result); // [確認_正常系] - NOT_FOUND スレッドを起動した cplat_thread_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        create_invalid_result); // [確認_正常系] - INVALID_ARGUMENT スレッドを起動した cplat_thread_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        join_not_found_result); // [確認_正常系] - NOT_FOUND スレッドを待機した cplat_thread_join() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        join_invalid_result); // [確認_正常系] - INVALID_ARGUMENT スレッドを待機した cplat_thread_join() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1,
        not_found_case
            .call_completed); // [確認_正常系] - not_found_case.call_completed が 1 であること。(NOT_FOUND スレッドが詳細エラーの取得を完了したことの確認)
    EXPECT_EQ(
        1,
        invalid_case
            .call_completed); // [確認_正常系] - invalid_case.call_completed が 1 であること。(INVALID_ARGUMENT スレッドが詳細エラーの取得を完了したことの確認)
    EXPECT_EQ(
        1,
        cplat_error_is(
            &not_found_case.observed_error,
            CPLAT_CAUSE_NOT_FOUND)); // [確認_正常系] - NOT_FOUND スレッドの CPLAT_CAUSE_NOT_FOUND に対する cplat_error_is() の戻り値が 1 であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &invalid_case.observed_error,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_正常系] - INVALID_ARGUMENT スレッドの CPLAT_CAUSE_INVALID_ARGUMENT に対する cplat_error_is() の戻り値が 1 であること。
    EXPECT_EQ(
        0,
        cplat_error_is_set(
            &main_error)); // [確認_正常系] - 子スレッド終了後のメイン スレッドの cplat_error_is_set() の戻り値が 0 であること。(TLS 詳細エラーが空のままであることの確認)
}
