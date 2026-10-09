#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/base/error.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>
#include <cplat/crt/file.h>
#include <cplat/crt/stdio.h>

#if defined(PLATFORM_LINUX)
    #include <sys/mock_stat.h>
    #include <cerrno>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif /* PLATFORM_ */

#if defined(PLATFORM_LINUX)

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SetErrnoAndReturn;

namespace
{

const char kPath[] = "file_timestamp_failure.dat";

} // namespace

class fileTimestampFailureInjectionTest : public testing::Test
{
  protected:
    // [サブ手順 名前=fileTimestampFailureInjectionTest.SetUp_1]
    void SetUp() override
    {
        FILE *stream = NULL;

        (void)cplat_remove(kPath, NULL);
        stream = cplat_fopen(kPath, "wb", NULL);
        ASSERT_NE(nullptr, stream);
        // [状態確認] - `nullptr` と `stream` が異なること。
        ASSERT_EQ(CPLAT_OK, cplat_fclose(stream, NULL));
        // [状態確認] - `cplat_fclose(stream, NULL)` の戻り値が `CPLAT_OK` であること。

        cplat_file_init(&file_);
        ASSERT_EQ(CPLAT_OK,
                  cplat_file_open(&file_, kPath, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, NULL));
        // [状態確認] - `cplat_file_open(&file_, kPath, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, NULL)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=fileTimestampFailureInjectionTest.TearDown_1]
    void TearDown() override
    {
        (void)cplat_file_close(&file_, NULL);
        (void)cplat_remove(kPath, NULL);
    }
    // [サブ手順終了]

    NiceMock<Mock_sys_stat> mock_sys_stat_;
    cplat_file file_;
};

// fstat の失敗が cplat_file_get_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_1]
TEST_F(fileTimestampFailureInjectionTest, get_reports_fstat_failure)
{
    // Arrange
    cplat_timespec actual = {0, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, _, _))
        .WillOnce(SetErrnoAndReturn(EIO, -1)); // [Pre-Assert確認_異常系] - fstat が 1 回呼び出されること。
                                               // [Pre-Assert手順] - errno に EIO を設定して -1 を返却する。

    // Act
    int actual_ret_get =
        cplat_file_get_modified_timestamp(&file_, &actual, &detail); // [手順] - 最終更新日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_get); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_1]

// futimens の失敗が cplat_file_set_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_1]
TEST_F(fileTimestampFailureInjectionTest, set_reports_futimens_failure)
{
    // Arrange
    const cplat_timespec timestamp = {1600000000, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, futimens(_, _, _, _, _))
        .WillOnce(SetErrnoAndReturn(EIO, -1)); // [Pre-Assert確認_異常系] - futimens が 1 回呼び出されること。
                                               // [Pre-Assert手順] - errno に EIO を設定して -1 を返却する。

    // Act
    int actual_ret_set =
        cplat_file_set_modified_timestamp(&file_, &timestamp, &detail); // [手順] - 最終更新日時を設定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_set); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_1]

// utimensat の失敗が cplat_file_set_path_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_1]
TEST_F(fileTimestampFailureInjectionTest, set_path_reports_utimensat_failure)
{
    // Arrange
    const cplat_timespec timestamp = {1600000000, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, utimensat(_, _, _, _, _, _, _))
        .WillOnce(SetErrnoAndReturn(EIO, -1)); // [Pre-Assert確認_異常系] - utimensat が 1 回呼び出されること。
                                               // [Pre-Assert手順] - errno に EIO を設定して -1 を返却する。

    // Act
    int actual_ret_set =
        cplat_file_set_path_modified_timestamp(kPath, &timestamp, &detail); // [手順] - 最終更新日時を設定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_set); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_1]

// stat の失敗が cplat_file_get_path_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_1]
TEST_F(fileTimestampFailureInjectionTest, get_path_reports_stat_failure)
{
    // Arrange
    cplat_timespec actual = {0, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, _, _))
        .WillOnce(SetErrnoAndReturn(EIO, -1)); // [Pre-Assert確認_異常系] - stat が 1 回呼び出されること。
                                               // [Pre-Assert手順] - errno に EIO を設定して -1 を返却する。

    // Act
    int actual_ret_get =
        cplat_file_get_path_modified_timestamp(kPath, &actual, &detail); // [手順] - 最終更新日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_get); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_1]

#elif defined(PLATFORM_WINDOWS)

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{

const char kPath[] = "file_timestamp_failure.dat";

} // namespace

class fileTimestampFailureInjectionTest : public testing::Test
{
  protected:
    // [サブ手順 名前=fileTimestampFailureInjectionTest.SetUp_2]
    void SetUp() override
    {
        FILE *stream = NULL;

        (void)cplat_remove(kPath, NULL);
        stream = cplat_fopen(kPath, "wb", NULL);
        ASSERT_NE(nullptr, stream);
        // [状態確認] - `nullptr` と `stream` が異なること。
        ASSERT_EQ(CPLAT_OK, cplat_fclose(stream, NULL));
        // [状態確認] - `cplat_fclose(stream, NULL)` の戻り値が `CPLAT_OK` であること。

        cplat_file_init(&file_);
        ASSERT_EQ(CPLAT_OK,
                  cplat_file_open(&file_, kPath, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, NULL));
        // [状態確認] - `cplat_file_open(&file_, kPath, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, NULL)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=fileTimestampFailureInjectionTest.TearDown_2]
    void TearDown() override
    {
        (void)cplat_file_close(&file_, NULL);
        (void)cplat_remove(kPath, NULL);
    }
    // [サブ手順終了]

    NiceMock<Mock_windows> mock_windows_;
    cplat_file file_;
};

// GetFileTime の失敗が cplat_file_get_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_2]
TEST_F(fileTimestampFailureInjectionTest, get_reports_GetFileTime_failure)
{
    // Arrange
    cplat_timespec actual = {0, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetFileTime(_, _, _, _, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - GetFileTime が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_IO_DEVICE)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                            // [Pre-Assert手順] - ERROR_IO_DEVICE を返却する。

    // Act
    int actual_ret_get =
        cplat_file_get_modified_timestamp(&file_, &actual, &detail); // [手順] - 最終更新日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_get); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_2]

// SetFileTime の失敗が cplat_file_set_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_2]
TEST_F(fileTimestampFailureInjectionTest, set_reports_SetFileTime_failure)
{
    // Arrange
    const cplat_timespec timestamp = {1600000000, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_windows_, SetFileTime(_, _, _, _, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetFileTime が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_IO_DEVICE)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                            // [Pre-Assert手順] - ERROR_IO_DEVICE を返却する。

    // Act
    int actual_ret_set =
        cplat_file_set_modified_timestamp(&file_, &timestamp, &detail); // [手順] - 最終更新日時を設定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_set); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_2]

// パス版の GetFileTime の失敗が cplat_file_get_path_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_2]
TEST_F(fileTimestampFailureInjectionTest, get_path_reports_GetFileTime_failure)
{
    // Arrange
    cplat_timespec actual = {0, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetFileTime(_, _, _, _, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - GetFileTime が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_IO_DEVICE)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                            // [Pre-Assert手順] - ERROR_IO_DEVICE を返却する。

    // Act
    int actual_ret_get =
        cplat_file_get_path_modified_timestamp(kPath, &actual, &detail); // [手順] - 最終更新日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_get); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_2]

// パス版の SetFileTime の失敗が cplat_file_set_path_modified_timestamp から伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_2]
TEST_F(fileTimestampFailureInjectionTest, set_path_reports_SetFileTime_failure)
{
    // Arrange
    const cplat_timespec timestamp = {1600000000, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_windows_, SetFileTime(_, _, _, _, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetFileTime が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_IO_DEVICE)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                            // [Pre-Assert手順] - ERROR_IO_DEVICE を返却する。

    // Act
    int actual_ret_set =
        cplat_file_set_path_modified_timestamp(kPath, &timestamp, &detail); // [手順] - 最終更新日時を設定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_set); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_2]

// パス版の取得で CloseHandle の失敗が伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_2]
TEST_F(fileTimestampFailureInjectionTest, get_path_reports_CloseHandle_failure)
{
    // Arrange
    cplat_timespec actual = {0, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_windows_, CloseHandle(_, _, _, _))
        .WillOnce(DoAll(Invoke(delegate_real_CloseHandle),
                        Return(FALSE))); // [Pre-Assert確認_異常系] - CloseHandle が 1 回呼び出されること。
                                         // [Pre-Assert手順] - 実ハンドルを閉じたうえで FALSE を返却する。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_IO_DEVICE)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                            // [Pre-Assert手順] - ERROR_IO_DEVICE を返却する。

    // Act
    int actual_ret_get =
        cplat_file_get_path_modified_timestamp(kPath, &actual, &detail); // [手順] - 最終更新日時を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_get); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_2]

// パス版の設定で CloseHandle の失敗が伝播することの確認
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.SetUp_2]
TEST_F(fileTimestampFailureInjectionTest, set_path_reports_CloseHandle_failure)
{
    // Arrange
    const cplat_timespec timestamp = {1600000000, 0};
    cplat_error detail;

    cplat_error_clear(&detail);

    // Pre-Assert
    EXPECT_CALL(mock_windows_, CloseHandle(_, _, _, _))
        .WillOnce(DoAll(Invoke(delegate_real_CloseHandle),
                        Return(FALSE))); // [Pre-Assert確認_異常系] - CloseHandle が 1 回呼び出されること。
                                         // [Pre-Assert手順] - 実ハンドルを閉じたうえで FALSE を返却する。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_IO_DEVICE)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                            // [Pre-Assert手順] - ERROR_IO_DEVICE を返却する。

    // Act
    int actual_ret_set =
        cplat_file_set_path_modified_timestamp(kPath, &timestamp, &detail); // [手順] - 最終更新日時を設定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_set); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーの要因が入出力エラーであること。
    EXPECT_EQ(1, cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR));
    // [確認_異常系] - `cplat_error_is(&detail, CPLAT_CAUSE_IO_ERROR)` の戻り値が `1` であること。
}
// [サブ手順参照 名前=fileTimestampFailureInjectionTest.TearDown_2]

#endif /* PLATFORM_ */
