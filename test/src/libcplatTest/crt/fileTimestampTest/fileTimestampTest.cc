#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/base/error.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>
#include <cplat/crt/file.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/sys/stat.h>

#include <string>

namespace
{

const char kPath[] = "file_timestamp.dat";
const char kMissingPath[] = "file_timestamp_missing.dat";

/*
 *  検証に使う固定の時刻です。
 *  ナノ秒部は 100 の倍数にしています。Windows の FILETIME の分解能が 100 ナノ秒であり、
 *  それ未満は切り捨てられるためです。
 */
cplat_timespec make_timestamp(time_t tv_sec, int64_t tv_nsec)
{
    cplat_timespec timestamp;

    timestamp.tv_sec = tv_sec;
    timestamp.tv_nsec = tv_nsec;
    return timestamp;
}

// [サブ手順 名前=fileTimestampTest.create_file]
void create_file(const char *path)
{
    FILE *stream = cplat_fopen(path, "wb", NULL);

    ASSERT_NE(nullptr, stream);
    // [状態確認] - `nullptr` と `stream` が異なること。
    ASSERT_EQ(1U, cplat_fwrite("x", 1U, 1U, stream, NULL));
    // [状態確認] - `cplat_fwrite("x", 1U, 1U, stream, NULL)` の戻り値が `1U` であること。
    ASSERT_EQ(CPLAT_OK, cplat_fclose(stream, NULL));
    // [状態確認] - `cplat_fclose(stream, NULL)` の戻り値が `CPLAT_OK` であること。
}
// [サブ手順終了]

} // namespace

class fileTimestampTest : public Test
{
  protected:
    // [サブ手順 名前=fileTimestampTest.SetUp]
    void SetUp() override
    {
        (void)cplat_remove(kPath, NULL);
        (void)cplat_remove(kMissingPath, NULL);
        // [サブ手順参照 名前=fileTimestampTest.create_file]
        create_file(kPath);
    }
    // [サブ手順終了]

    // [サブ手順 名前=fileTimestampTest.TearDown]
    void TearDown() override
    {
        (void)cplat_remove(kPath, NULL);
    }
    // [サブ手順終了]

    /* 書き込みアクセスで対象ファイルを開きます。 */
    // [サブ手順 名前=fileTimestampTest.open_writable]
    void open_writable(cplat_file *file)
    {
        cplat_file_init(file);
        ASSERT_EQ(CPLAT_OK,
                  cplat_file_open(file, kPath, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, NULL));
        // [状態確認] - `cplat_file_open(file, kPath, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, NULL)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]
};

// パス版で設定した最終更新日時が、パス版で取得し直すと一致することの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, path_set_then_path_get_round_trips)
{
    // Arrange
    const cplat_timespec expected = make_timestamp(1600000000, 123456700); // [状態] - サブ秒を含む時刻を用意する。
    cplat_timespec actual = make_timestamp(0, 0);

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_file_set_path_modified_timestamp(kPath, &expected,
                                                                   NULL); // [手順] - パス版で日時を設定する。
    int actual_ret_get = cplat_file_get_path_modified_timestamp(kPath, &actual,
                                                                   NULL); // [手順] - パス版で日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_set);    // [確認_正常系] - 設定が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_get);    // [確認_正常系] - 取得が CPLAT_OK であること。
    EXPECT_EQ(expected.tv_sec, actual.tv_sec); // [確認_正常系] - 秒部が一致すること。
    EXPECT_EQ(expected.tv_nsec,
              actual.tv_nsec); // [確認_正常系] - ナノ秒部が一致すること (サブ秒を保持するファイル システムが前提)。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// ハンドル版で設定した最終更新日時が、ハンドル版で取得し直すと一致することの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, handle_set_then_handle_get_round_trips)
{
    // Arrange
    const cplat_timespec expected = make_timestamp(1500000000, 987654300); // [状態] - サブ秒を含む時刻を用意する。
    cplat_timespec actual = make_timestamp(0, 0);
    cplat_file file;

    // [サブ手順参照 名前=fileTimestampTest.open_writable]
    open_writable(&file); // [状態] - 書き込みアクセスでファイルを開く。

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_file_set_modified_timestamp(&file, &expected,
                                                              NULL); // [手順] - ハンドル版で日時を設定する。
    int actual_ret_get = cplat_file_get_modified_timestamp(&file, &actual,
                                                              NULL); // [手順] - ハンドル版で日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_set);      // [確認_正常系] - 設定が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_get);      // [確認_正常系] - 取得が CPLAT_OK であること。
    EXPECT_EQ(expected.tv_sec, actual.tv_sec);   // [確認_正常系] - 秒部が一致すること。
    EXPECT_EQ(expected.tv_nsec, actual.tv_nsec); // [確認_正常系] - ナノ秒部が一致すること。

    EXPECT_EQ(CPLAT_OK, cplat_file_close(&file, NULL));
    // [確認_正常系] - `cplat_file_close(&file, NULL)` の戻り値が `CPLAT_OK` であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// パス版で設定した最終更新日時を、ハンドル版で取得しても一致することの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, path_set_is_visible_from_handle_get)
{
    // Arrange
    const cplat_timespec expected = make_timestamp(1400000000, 500000000); // [状態] - サブ秒を含む時刻を用意する。
    cplat_timespec actual = make_timestamp(0, 0);
    cplat_file file;

    ASSERT_EQ(CPLAT_OK, cplat_file_set_path_modified_timestamp(kPath, &expected, NULL));
    // [状態確認] - `cplat_file_set_path_modified_timestamp(kPath, &expected, NULL)` の戻り値が `CPLAT_OK` であること。
    // [サブ手順参照 名前=fileTimestampTest.open_writable]
    open_writable(&file); // [状態] - 設定後にファイルを開く。

    // Pre-Assert

    // Act
    int actual_ret_get = cplat_file_get_modified_timestamp(&file, &actual,
                                                              NULL); // [手順] - ハンドル版で日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_get);      // [確認_正常系] - 取得が CPLAT_OK であること。
    EXPECT_EQ(expected.tv_sec, actual.tv_sec);   // [確認_正常系] - 秒部がパス版の設定値と一致すること。
    EXPECT_EQ(expected.tv_nsec, actual.tv_nsec); // [確認_正常系] - ナノ秒部がパス版の設定値と一致すること。

    EXPECT_EQ(CPLAT_OK, cplat_file_close(&file, NULL));
    // [確認_正常系] - `cplat_file_close(&file, NULL)` の戻り値が `CPLAT_OK` であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// 取得した最終更新日時の秒部が cplat_stat の st_mtime と一致することの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, seconds_agree_with_cplat_stat)
{
    // Arrange
    const cplat_timespec expected = make_timestamp(1300000000, 250000000); // [状態] - サブ秒を含む時刻を用意する。
    cplat_file_stat_t file_stat;

    ASSERT_EQ(CPLAT_OK, cplat_file_set_path_modified_timestamp(kPath, &expected, NULL));
    // [状態確認] - `cplat_file_set_path_modified_timestamp(kPath, &expected, NULL)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    int actual_ret_stat = cplat_stat(&file_stat, NULL, kPath); // [手順] - cplat_stat でファイル情報を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_stat); // [確認_正常系] - cplat_stat が CPLAT_OK であること。
    // st_mtime が設定した秒部と一致すること。
    EXPECT_EQ(expected.tv_sec, static_cast<time_t>(file_stat.st_mtime));
    // [確認_正常系] - `file_stat.st_mtime` を `time_t` に変換した値が、設定した秒数 `expected.tv_sec` と一致すること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// 過去と未来のいずれの日時も往復することの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, past_and_future_timestamps_round_trip)
{
    // Arrange
    const cplat_timespec past = make_timestamp(1, 0);            // [状態] - Unix epoch の直後を用意する。
    const cplat_timespec future = make_timestamp(4000000000, 0); // [状態] - 2038 年より後を用意する。
    cplat_timespec actual_past = make_timestamp(0, 0);
    cplat_timespec actual_future = make_timestamp(0, 0);

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_file_set_path_modified_timestamp(kPath, &past, NULL));
    // [確認_正常系] - `cplat_file_set_path_modified_timestamp(kPath, &past, NULL)` の戻り値が `CPLAT_OK` であること。
    int actual_ret_get_past =
        cplat_file_get_path_modified_timestamp(kPath, &actual_past, NULL); // [手順] - 過去の日時を往復させる。
    ASSERT_EQ(CPLAT_OK, cplat_file_set_path_modified_timestamp(kPath, &future, NULL));
    // [確認_正常系] - `cplat_file_set_path_modified_timestamp(kPath, &future, NULL)` の戻り値が `CPLAT_OK` であること。
    int actual_ret_get_future =
        cplat_file_get_path_modified_timestamp(kPath, &actual_future, NULL); // [手順] - 未来の日時を往復させる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_get_past);    // [確認_正常系] - 過去の取得が CPLAT_OK であること。
    EXPECT_EQ(past.tv_sec, actual_past.tv_sec);     // [確認_正常系] - 過去の秒部が一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_get_future);  // [確認_正常系] - 未来の取得が CPLAT_OK であること。
    EXPECT_EQ(future.tv_sec, actual_future.tv_sec); // [確認_正常系] - 未来の秒部が一致すること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// 最終更新日時の設定が最終アクセス日時を変更しないことの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, set_does_not_change_access_time)
{
    // Arrange
    const cplat_timespec expected = make_timestamp(1200000000, 0); // [状態] - 設定する時刻を用意する。
    cplat_file_stat_t before;
    cplat_file_stat_t after;

    ASSERT_EQ(CPLAT_OK, cplat_stat(&before, NULL, kPath)); // [状態] - 設定前の最終アクセス日時を控える。
    // [状態確認] - `cplat_stat(&before, NULL, kPath)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_file_set_path_modified_timestamp(kPath, &expected,
                                                                   NULL); // [手順] - 最終更新日時を設定する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_set); // [確認_正常系] - 設定が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_stat(&after, NULL, kPath));
    // [確認_正常系] - `cplat_stat(&after, NULL, kPath)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(before.st_atime, after.st_atime); // [確認_正常系] - 最終アクセス日時が変化しないこと。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// 読み取り専用で開いたハンドルへの設定が権限エラーになることの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, set_on_read_only_handle_is_permission_denied)
{
    // Arrange
    const cplat_timespec timestamp = make_timestamp(1100000000, 0);
    cplat_error detail;
    cplat_file file;

    cplat_error_clear(&detail);
    cplat_file_init(&file);
    ASSERT_EQ(CPLAT_OK,
              cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_READ, NULL)); // [状態] - 読み取り専用で開く。
    // [状態確認] - `cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_READ, NULL)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_file_set_modified_timestamp(&file, &timestamp,
                                                              &detail); // [手順] - 最終更新日時の設定を試みる。

    // Assert
    // 戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, actual_ret_set);
    // [確認_異常系] - 詳細エラーの要因がアクセス拒否であること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_ACCESS_DENIED));
    // [確認_正常系] - `cplat_error_is(&detail, CPLAT_CAUSE_ACCESS_DENIED)` の戻り値が `1` であること。

    EXPECT_EQ(CPLAT_OK, cplat_file_close(&file, NULL));
    // [確認_正常系] - `cplat_file_close(&file, NULL)` の戻り値が `CPLAT_OK` であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// 存在しないパスに対する取得が対象なしの要因になることの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, get_on_missing_path_reports_not_found)
{
    // Arrange
    cplat_timespec actual = make_timestamp(0, 0);
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert

    // Act
    int actual_ret_get = cplat_file_get_path_modified_timestamp(kMissingPath, &actual,
                                                                   &detail); // [手順] - 存在しないパスを指定する。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret_get); // [確認_異常系] - 戻り値が CPLAT_OK 以外であること。
    // 詳細エラーの要因が対象なしであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_NOT_FOUND));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_NOT_FOUND)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// 存在しないパスに対する設定が対象なしの要因になることの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, set_on_missing_path_reports_not_found)
{
    // Arrange
    const cplat_timespec timestamp = make_timestamp(1000000000, 0);
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_file_set_path_modified_timestamp(kMissingPath, &timestamp,
                                                                   &detail); // [手順] - 存在しないパスを指定する。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret_set); // [確認_異常系] - 戻り値が CPLAT_OK 以外であること。
    // 詳細エラーの要因が対象なしであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_NOT_FOUND));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_NOT_FOUND)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// 引数に NULL を指定した場合に引数不正を返すことの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, null_arguments_are_rejected)
{
    // Arrange
    const cplat_timespec timestamp = make_timestamp(900000000, 0);
    cplat_timespec actual = make_timestamp(0, 0);
    cplat_file file;
    cplat_file open_file;

    cplat_file_init(&file); // [状態] - 無効なハンドルを用意する。
    // [サブ手順参照 名前=fileTimestampTest.open_writable]
    open_writable(&open_file); // [状態] - 有効なハンドルを用意する。

    // Pre-Assert

    // Act
    int actual_ret_get_path = cplat_file_get_path_modified_timestamp(NULL, &actual, NULL);
    int actual_ret_get_path_out = cplat_file_get_path_modified_timestamp(kPath, NULL, NULL);
    int actual_ret_set_path = cplat_file_set_path_modified_timestamp(NULL, &timestamp, NULL);
    int actual_ret_set_path_value = cplat_file_set_path_modified_timestamp(kPath, NULL, NULL);
    int actual_ret_get_handle = cplat_file_get_modified_timestamp(NULL, &actual, NULL);
    int actual_ret_get_handle_closed = cplat_file_get_modified_timestamp(&file, &actual, NULL);
    int actual_ret_set_handle = cplat_file_set_modified_timestamp(NULL, &timestamp, NULL);
    int actual_ret_get_handle_out = cplat_file_get_modified_timestamp(&open_file, NULL, NULL);
    int actual_ret_set_handle_value = cplat_file_set_modified_timestamp(&open_file, NULL, NULL);
    // [手順] - 各関数へ NULL または無効なハンドルを指定する。
    // 有効なハンドルと NULL の出力引数を組み合わせ、短絡評価の両側を通す。

    // Assert
    // いずれも CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_get_path);
    // [確認_異常系] - `cplat_file_get_path_modified_timestamp(NULL, &actual, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_get_path_out);
    // [確認_異常系] - `cplat_file_get_path_modified_timestamp(kPath, NULL, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_set_path);
    // [確認_異常系] - `cplat_file_set_path_modified_timestamp(NULL, &timestamp, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_set_path_value);
    // [確認_異常系] - `cplat_file_set_path_modified_timestamp(kPath, NULL, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_get_handle);
    // [確認_異常系] - `cplat_file_get_modified_timestamp(NULL, &actual, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_get_handle_closed);
    // [確認_異常系] - `cplat_file_get_modified_timestamp(&file, &actual, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_set_handle);
    // [確認_異常系] - `cplat_file_set_modified_timestamp(NULL, &timestamp, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_get_handle_out);
    // [確認_異常系] - `cplat_file_get_modified_timestamp(&open_file, NULL, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_set_handle_value);
    // [確認_異常系] - `cplat_file_set_modified_timestamp(&open_file, NULL, NULL)` の戻り値が `CPLAT_ERR_INVALID_ARGUMENT` であること。

    EXPECT_EQ(CPLAT_OK, cplat_file_close(&open_file, NULL));
    // [確認_正常系] - `cplat_file_close(&open_file, NULL)` の戻り値が `CPLAT_OK` であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

#if defined(PLATFORM_WINDOWS)

// Windows でパスがワイド文字へ変換できない場合に取得が名称長超過になることの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, get_path_reports_name_too_long_when_path_exceeds_wide_buffer)
{
    // Arrange
    const std::string long_path(PLATFORM_PATH_MAX + 1u,
                                'a'); // [状態] - PLATFORM_PATH_MAX を 1 文字超えるパスを用意する。
    cplat_timespec actual = make_timestamp(0, 0);
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert

    // Act
    int actual_ret_get = cplat_file_get_path_modified_timestamp(long_path.c_str(), &actual,
                                                                   &detail); // [手順] - 長過ぎるパスで日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, actual_ret_get); // [確認_異常系] - 戻り値が BUFFER_TOO_SMALL であること。
    EXPECT_EQ(1, cplat_error_is(&detail,
                                   CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

// Windows でパスがワイド文字へ変換できない場合に設定が名称長超過になることの確認
// [サブ手順参照 名前=fileTimestampTest.SetUp]
TEST_F(fileTimestampTest, set_path_reports_name_too_long_when_path_exceeds_wide_buffer)
{
    // Arrange
    const std::string long_path(PLATFORM_PATH_MAX + 1u,
                                'a'); // [状態] - PLATFORM_PATH_MAX を 1 文字超えるパスを用意する。
    const cplat_timespec timestamp = make_timestamp(1000000000, 0);
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert

    // Act
    int actual_ret_set = cplat_file_set_path_modified_timestamp(long_path.c_str(), &timestamp,
                                                                   &detail); // [手順] - 長過ぎるパスで日時を設定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, actual_ret_set); // [確認_異常系] - 戻り値が BUFFER_TOO_SMALL であること。
    EXPECT_EQ(1, cplat_error_is(&detail,
                                   CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因であること。
}
// [サブ手順参照 名前=fileTimestampTest.TearDown]

#endif /* PLATFORM_WINDOWS */
