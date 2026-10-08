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
    (void)cplat_vopen_fmt(0, 0, NULL, format, args);
    va_end(args);
}

static void invoke_vfopen_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vfopen_fmt(NULL, NULL, format, args);
    va_end(args);
}

static void invoke_vremove_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vremove_fmt(NULL, format, args);
    va_end(args);
}

static void invoke_vstat_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vstat_fmt(NULL, NULL, format, args);
    va_end(args);
}

static void invoke_vmkdir_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vmkdir_fmt(NULL, format, args);
    va_end(args);
}

static void invoke_vaccess_fmt_with_null_detail(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)cplat_vaccess_fmt(0, NULL, format, args);
    va_end(args);
}

static void record_thread_local_error(void *arg)
{
    tls_thread_case *test_case = static_cast<tls_thread_case *>(arg);

    if (test_case->requested_cause == CPLAT_CAUSE_NOT_FOUND)
    {
        (void)cplat_fopen("cplat_error_tls_missing_file", "rb", NULL);
    }
    else
    {
        (void)cplat_path_get_full(NULL, 0U, NULL, NULL);
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
        {"cplat_open", []() { (void)cplat_open(NULL, 0, 0, NULL); }, 1, 0U},
        {"cplat_open_fmt", []() { (void)cplat_open_fmt(0, 0, NULL, NULL); }, 1, 0U},
        {"cplat_vopen_fmt", []() { invoke_vopen_fmt_with_null_detail(NULL); }, 1, 0U},
        {"cplat_file_open", []() { (void)cplat_file_open(NULL, NULL, 0, NULL); }, 1, 0U},
        {"cplat_file_write", []() { (void)cplat_file_write(NULL, NULL, 1U, NULL); }, 1, 0U},
        {"cplat_file_read", []() { (void)cplat_file_read(NULL, NULL, 1U, NULL, NULL); }, 1, 0U},
        {"cplat_file_get_size", []() { (void)cplat_file_get_size(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_file_set_size", []() { (void)cplat_file_set_size(NULL, 0U, NULL); }, 1, 0U},
        {"cplat_file_get_id", []() { (void)cplat_file_get_id(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_file_get_path_id", []() { (void)cplat_file_get_path_id(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_file_get_modified_timestamp", []() { (void)cplat_file_get_modified_timestamp(NULL, NULL, NULL); }, 1,
         0U},
        {"cplat_file_set_modified_timestamp", []() { (void)cplat_file_set_modified_timestamp(NULL, NULL, NULL); }, 1,
         0U},
        {"cplat_file_get_path_modified_timestamp",
         []() { (void)cplat_file_get_path_modified_timestamp(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_file_set_path_modified_timestamp",
         []() { (void)cplat_file_set_path_modified_timestamp(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_file_flush", []() { (void)cplat_file_flush(NULL, NULL); }, 1, 0U},
        {"cplat_file_close", []() { (void)cplat_file_close(NULL, NULL); }, 1, 0U},
        {"cplat_path_get_full", []() { (void)cplat_path_get_full(NULL, 0U, NULL, NULL); }, 1, 0U},
        {"cplat_path_equal", []() { (void)cplat_path_equal(NULL, NULL, NULL, NULL); }, 1, 0U},
        {"cplat_path_get_temp_dir", []() { (void)cplat_path_get_temp_dir(NULL, 0U, NULL); }, 1, 0U},
        {"cplat_path_concat_n", []() { (void)cplat_path_concat_n(NULL, 0U, NULL, 1U, "x"); }, 1, 0U},
        {"cplat_path_concat", []() { (void)cplat_path_concat(NULL, 0U, NULL, "x"); }, 1, 0U},
        {"cplat_path_dirname", []() { (void)cplat_path_dirname(NULL, 0U, NULL, NULL); }, 1, 0U},
        {"cplat_path_strip_extension", []() { (void)cplat_path_strip_extension(NULL, 0U, NULL, NULL); }, 1, 0U},
        {"cplat_path_join_n", []() { (void)cplat_path_join_n(NULL, 0U, NULL, 1U, "x"); }, 1, 0U},
        {"cplat_path_join", []() { (void)cplat_path_join(NULL, 0U, NULL, "x"); }, 1, 0U},
        {"cplat_fopen", []() { (void)cplat_fopen(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_freopen", []() { (void)cplat_freopen(NULL, NULL, NULL, NULL); }, 1, 0U},
        {"cplat_fclose", []() { (void)cplat_fclose(NULL, NULL); }, 1, 0U},
        {"cplat_fflush", []() { (void)cplat_fflush(NULL, NULL); }, -1, 0U},
        {"cplat_fread", []() { (void)cplat_fread(NULL, 1U, 1U, NULL, NULL); }, 1, 0U},
        {"cplat_fwrite", []() { (void)cplat_fwrite(NULL, 1U, 1U, NULL, NULL); }, 1, 0U},
        {"cplat_remove", []() { (void)cplat_remove(NULL, NULL); }, 1, 0U},
        {"cplat_rename", []() { (void)cplat_rename(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_fopen_fmt", []() { (void)cplat_fopen_fmt(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_vfopen_fmt", []() { invoke_vfopen_fmt_with_null_detail(NULL); }, 1, 0U},
        {"cplat_remove_fmt", []() { (void)cplat_remove_fmt(NULL, NULL); }, 1, 0U},
        {"cplat_vremove_fmt", []() { invoke_vremove_fmt_with_null_detail(NULL); }, 1, 0U},
        {"cplat_fopen_temp", []() { (void)cplat_fopen_temp(NULL, NULL, NULL, 0U, NULL); }, 1, 0U},
        {"cplat_getenv", []() { (void)cplat_getenv(NULL, NULL, 0U, NULL, NULL); }, 1, 0U},
        {"cplat_setenv", []() { (void)cplat_setenv(NULL, NULL, 0, NULL); }, 1, 0U},
        {"cplat_unsetenv", []() { (void)cplat_unsetenv(NULL, NULL); }, 1, 0U},
        {"cplat_lseek", []() { (void)cplat_lseek(-1, 0, 0, NULL); }, 1, 0U},
        {"cplat_close", []() { (void)cplat_close(-1, NULL); }, 1, 0U},
        {"cplat_dup", []() { (void)cplat_dup(-1, NULL); }, 1, 0U},
        {"cplat_dup2", []() { (void)cplat_dup2(-1, -1, NULL); }, 1, 0U},
        {"cplat_read", []() { (void)cplat_read(-1, NULL, 1U, NULL); }, 1, 0U},
        {"cplat_write", []() { (void)cplat_write(-1, NULL, 1U, NULL); }, 1, 0U},
        {"cplat_access", []() { (void)cplat_access(NULL, 0, NULL); }, 1, 0U},
        {"cplat_access_fmt", []() { (void)cplat_access_fmt(0, NULL, NULL); }, 1, 0U},
        {"cplat_vaccess_fmt", []() { invoke_vaccess_fmt_with_null_detail(NULL); }, 1, 0U},
        {"cplat_stat", []() { (void)cplat_stat(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_mkdir", []() { (void)cplat_mkdir(NULL, NULL); }, 1, 0U},
        {"cplat_makedirs", []() { (void)cplat_makedirs(NULL, NULL); }, 1, 0U},
        {"cplat_rmdir", []() { (void)cplat_rmdir(NULL, NULL); }, 1, 0U},
        {"cplat_stat_fmt", []() { (void)cplat_stat_fmt(NULL, NULL, NULL); }, 1, 0U},
        {"cplat_vstat_fmt", []() { invoke_vstat_fmt_with_null_detail(NULL); }, 1, 0U},
        {"cplat_mkdir_fmt", []() { (void)cplat_mkdir_fmt(NULL, NULL); }, 1, 0U},
        {"cplat_vmkdir_fmt", []() { invoke_vmkdir_fmt_with_null_detail(NULL); }, 1, 0U},
        {"cplat_mmap_attach", []() { (void)cplat_mmap_attach(NULL, CPLAT_MMAP_ACCESS_READ_ONLY, 0U, NULL, NULL); }, 1,
         0U},
        {"cplat_mmap_flush", []() { (void)cplat_mmap_flush(NULL, NULL, 0U, NULL); }, 1, 0U},
        {"cplat_mmap_detach", []() { (void)cplat_mmap_detach(NULL, NULL); }, 0, 0U},
    }; // [状態] - detail_out に NULL を指定する公開関数 59 件と公開マクロ 2 件を用意する。
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
    } // [手順] - 全公開 API の detail_out に NULL を指定し、呼び出し後の TLS を取得する。

    // Assert
    EXPECT_EQ(cases.size(), tls_set_results.size()); // [確認_正常系] - 全 61 件の公開 API 呼び出しが完了したこと。
    for (std::size_t index = 0U; index < cases.size(); ++index)
    {
        if (cases[index].expected_tls_set >= 0)
        {
            EXPECT_EQ(cases[index].expected_tls_set, tls_set_results[index]) << cases[index].name;
            // [確認_正常系 回数=60] - 各公開 API が NULL の detail_out を参照せず、TLS を規約どおり更新すること。
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
                       NULL); // [状態] - 失敗対象のファイルが存在しない状態にする。

    // Pre-Assert

    // Act
    FILE *missing =
        cplat_fopen("cplat_error_tls_missing_file", "rb", NULL); // [手順] - 詳細エラー出力なしで失敗させる。
    cplat_error_get_last(&failure_error);                        // [手順] - 失敗直後の TLS 詳細エラーを取得する。
    FILE *existing = cplat_fopen_temp("err", "w+b", temp_path, sizeof(temp_path),
                                      NULL); // [手順] - 次の詳細エラー記録対象 API を成功させる。
    cplat_error_get_last(&success_error);    // [手順] - 成功直後の TLS 詳細エラーを取得する。

    // Assert
    EXPECT_EQ((FILE *)NULL, missing); // [確認_異常系] - 存在しないファイルに対する cplat_fopen が NULL を返すこと。
    EXPECT_EQ(1, cplat_error_is(&failure_error,
                                CPLAT_CAUSE_NOT_FOUND)); // [確認_異常系] - 失敗直後の TLS 要因が NOT_FOUND であること。
    ASSERT_NE((FILE *)NULL, existing); // [確認_正常系] - 存在するファイルに対する cplat_fopen が成功すること。
    EXPECT_EQ(0, cplat_error_is_set(&success_error)); // [確認_正常系] - 成功直後の TLS 詳細エラーが空であること。

    // Cleanup
    (void)fclose(existing);
    (void)cplat_remove(temp_path, NULL);
}

// ネストした成功が直前の失敗をクリアすることの確認
TEST_F(errorContractTest, nested_paths_equal_success_clears_previous_failure)
{
    // Arrange
    int equal = 0;
    cplat_error last_error;

    (void)cplat_fopen("cplat_error_tls_missing_file", "rb", NULL); // [状態] - TLS へ失敗を記録する。

    // Pre-Assert

    // Act
    const int result = cplat_path_equal(".", ".", &equal, NULL); // [手順] - 内部で 2 回絶対パス化する比較を成功させる。
    cplat_error_get_last(&last_error);                           // [手順] - 比較成功直後の TLS 詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);   // [確認_正常系] - cplat_path_equal の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, equal); // [確認_正常系] - 同じパスの比較結果が一致であること。
    EXPECT_EQ(0,
              cplat_error_is_set(&last_error)); // [確認_正常系] - ネストした成功後の TLS 詳細エラーが空であること。
}

// スレッド間で last error が独立することの確認
TEST_F(errorContractTest, last_error_is_isolated_between_threads)
{
    // Arrange
    tls_thread_case not_found_case = {{CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL}, CPLAT_CAUSE_NOT_FOUND, 0};
    tls_thread_case invalid_case = {{CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL}, CPLAT_CAUSE_INVALID_ARGUMENT, 0};
    cplat_thread *not_found_thread = NULL;
    cplat_thread *invalid_thread = NULL;
    cplat_error main_error;
    int join_not_found_result = CPLAT_ERR_UNKNOWN;
    int join_invalid_result = CPLAT_ERR_UNKNOWN;

    (void)cplat_remove("cplat_error_tls_missing_file",
                       NULL); // [状態] - NOT_FOUND 用のファイルが存在しない状態にする。
    cplat_error_clear_last(); // [状態] - メイン スレッドの TLS 詳細エラーを空にする。

    // Pre-Assert

    // Act
    const int create_not_found_result =
        cplat_thread_create(&not_found_thread, record_thread_local_error,
                            &not_found_case); // [手順] - NOT_FOUND を記録するスレッドを起動する。
    const int create_invalid_result =
        cplat_thread_create(&invalid_thread, record_thread_local_error,
                            &invalid_case); // [手順] - INVALID_ARGUMENT を記録するスレッドを起動する。
    if (create_not_found_result == CPLAT_OK)
    {
        join_not_found_result =
            cplat_thread_join(not_found_thread, CPLAT_SYNC_WAIT_FOREVER); // [手順] - NOT_FOUND スレッドを待機する。
    }
    if (create_invalid_result == CPLAT_OK)
    {
        join_invalid_result = cplat_thread_join(
            invalid_thread, CPLAT_SYNC_WAIT_FOREVER); // [手順] - INVALID_ARGUMENT スレッドを待機する。
    }
    cplat_error_get_last(&main_error); // [手順] - 子スレッド終了後にメイン スレッドの TLS 詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              create_not_found_result); // [確認_正常系] - 1 つ目の cplat_thread_create の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              create_invalid_result); // [確認_正常系] - 2 つ目の cplat_thread_create の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              join_not_found_result); // [確認_正常系] - 1 つ目の cplat_thread_join の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              join_invalid_result); // [確認_正常系] - 2 つ目の cplat_thread_join の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1,
              not_found_case.call_completed); // [確認_正常系] - 1 つ目のスレッドが詳細エラーの取得を完了したこと。
    EXPECT_EQ(1,
              invalid_case.call_completed); // [確認_正常系] - 2 つ目のスレッドが詳細エラーの取得を完了したこと。
    EXPECT_EQ(
        1, cplat_error_is(&not_found_case.observed_error,
                          CPLAT_CAUSE_NOT_FOUND)); // [確認_正常系] - 1 つ目のスレッドが NOT_FOUND だけを取得すること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &invalid_case.observed_error,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_正常系] - 2 つ目のスレッドが INVALID_ARGUMENT だけを取得すること。
    EXPECT_EQ(
        0, cplat_error_is_set(&main_error)); // [確認_正常系] - メイン スレッドの TLS 詳細エラーが空のままであること。
}
