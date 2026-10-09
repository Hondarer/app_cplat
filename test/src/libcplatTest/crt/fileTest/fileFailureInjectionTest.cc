#include "fileTestCommon.h"

#include <cplat/base/result.h>
#include <cplat/crt/file.h>
#include <mock_unistd.h>
#include <sys/mock_stat.h>

#include <errno.h>
#include <limits>

#if defined(PLATFORM_LINUX)

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::DoDefault;
using testing::NiceMock;
using testing::Return;

class fileFailureInjectionTest : public Test
{
  protected:
    cplat_file file_ = {};

    // [サブ手順 名前=fileFailureInjectionTest.SetUp]
    void SetUp() override
    {
        cplat_file_init(&file_);
        file_.handle = kFakeFd;
        file_.writable = 1;
    }
    // [サブ手順終了]

    // [サブ手順 名前=fileFailureInjectionTest.TearDown]
    void TearDown() override
    {
        file_.handle = -1;
    }
    // [サブ手順終了]
};

// サイズ変更に失敗した場合に errno が通知されることの確認
// Windows の cplat_file_set_size は SetEndOfFile を使うため、この失敗経路は Linux のみに存在する
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, set_size_reports_errno_when_ftruncate_fails)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, ftruncate(_, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EIO), Return(-1)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - ftruncate が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - errno に EIO を設定し、1 回目は -1 を返却する。

    // Act
    int actual_ret = cplat_file_set_size(&file_, 16u, &detail); // [手順] - cplat_file_set_size を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_file_set_size の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        EIO,
        cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno の戻り値が EIO であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// サイズ取得に失敗した場合に errno が通知されることの確認
// Windows の cplat_file_get_size は GetFileSizeEx を使うため、この失敗経路は Linux のみに存在する
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, get_size_reports_errno_when_fstat_fails)
{
    // Arrange
    NiceMock<Mock_sys_stat> mock_sys_stat;
    size_t size = 0u;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat, fstat(_, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - fstat が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - errno に EBADF を設定し、1 回目は -1 を返却する。

    // Act
    int actual_ret = cplat_file_get_size(&file_, &size, &detail); // [手順] - cplat_file_get_size を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_file_get_size の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        EBADF,
        cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno の戻り値が EBADF であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// ファイル識別の取得に失敗した場合に errno が通知されることの確認
// Windows の cplat_file_get_id は GetFileInformationByHandle を使うため、この失敗経路は Linux のみに存在する
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, get_id_reports_errno_when_fstat_fails)
{
    // Arrange
    NiceMock<Mock_sys_stat> mock_sys_stat;
    cplat_file_id id = {};
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat, fstat(_, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - fstat が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - errno に EBADF を設定し、1 回目は -1 を返却する。

    // Act
    int actual_ret = cplat_file_get_id(&file_, &id, &detail); // [手順] - cplat_file_get_id を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_file_get_id の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        EBADF,
        cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno の戻り値が EBADF であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// NULL のファイル ハンドル初期化が安全に完了することの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, init_accepts_null)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_file_init(NULL); // [手順] - NULL を指定して cplat_file_init を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - クラッシュせずに完了すること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 既存ハンドルのクローズに失敗した場合にオープン処理を中断することの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, open_reports_close_failure_before_opening_new_path)
{
    // Arrange
    cplat_file file;
    cplat_error detail;
    cplat_file_init(&file);
    file.handle = std::numeric_limits<int>::max(); // [状態] - クローズできない無効なファイル記述子を設定する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_READ,
                                 &detail); // [手順] - 既存ハンドルを持つ状態で別のファイルを開く。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - 既存ハンドルのクローズ失敗が返ること。
    EXPECT_EQ(EBADF,
              cplat_error_get_errno(&detail)); // [確認_異常系] - クローズ失敗の errno が EBADF であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 書き込み長 0 の場合に OS API を呼ばず成功することの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, write_succeeds_for_zero_length)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_write(&file_, NULL, 0u,
                                  &detail); // [手順] - NULL バッファーと長さ 0 を指定して書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 長さ 0 の書き込みが成功すること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// オープン済みファイルへの NULL バッファー付き書き込みが拒否されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, write_rejects_null_buffer_for_positive_length)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_file_write(&file_, NULL, 1u,
                            &detail); // [手順] - オープン済みファイルへ NULL バッファーと長さ 1 を指定して書き込む。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - NULL バッファー付き書き込みが CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(
                          &detail)); // [確認_異常系] - NULL バッファー付き書き込みの errno が EINVAL であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 読み取り長 0 の場合に OS API を呼ばず成功することの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, read_succeeds_for_zero_length)
{
    // Arrange
    char buf[1] = {};
    size_t read = 99u;
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_read(&file_, buf, 0u, &read,
                                 &detail); // [手順] - 長さ 0 を指定して読み込む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 長さ 0 の読み取りが成功すること。
    EXPECT_EQ(0u, read);         // [確認_正常系] - 読み取ったバイト数が 0 であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// オープン済みファイルからの NULL バッファー付き読み取りが拒否されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, read_rejects_null_buffer_when_open)
{
    // Arrange
    size_t read = 0u;
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_file_read(&file_, NULL, 1u, &read,
                           &detail); // [手順] - オープン済みファイルへ NULL バッファーと長さ 1 を指定して読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - NULL バッファー付き読み取りが CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(
                          &detail)); // [確認_異常系] - NULL バッファー付き読み取りの errno が EINVAL であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// オープン済みファイルからの NULL 出力先付き読み取りが拒否されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, read_rejects_null_output_when_open)
{
    // Arrange
    char buffer[1] = {};
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_read(
        &file_, buffer, 1u, NULL,
        &detail); // [手順] - オープン済みファイルへ NULL の読み取りバイト数出力先を指定して読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - NULL 出力先付き読み取りが CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(
                          &detail)); // [確認_異常系] - NULL 出力先付き読み取りの errno が EINVAL であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// オープン済みファイルのサイズ取得で NULL 出力先が拒否されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, get_size_rejects_null_output_when_open)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_get_size(
        &file_, NULL,
        &detail); // [手順] - オープン済みファイルへ NULL のサイズ出力先を指定してサイズを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - NULL 出力先付きサイズ取得が CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(
                          &detail)); // [確認_異常系] - NULL 出力先付きサイズ取得の errno が EINVAL であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// オープン済みファイルの ID 取得で NULL 出力先が拒否されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, get_id_rejects_null_output_when_open)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_file_get_id(&file_, NULL,
                             &detail); // [手順] - オープン済みファイルへ NULL の ID 出力先を指定して ID を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - NULL 出力先付き ID 取得が CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(
                          &detail)); // [確認_異常系] - NULL 出力先付き ID 取得の errno が EINVAL であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 未オープンのファイルに対する flush が拒否されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, flush_rejects_unopened_file)
{
    // Arrange
    cplat_file file;
    cplat_error detail;
    cplat_file_init(&file);

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_flush(&file,
                                  &detail); // [手順] - 未オープンのファイルを flush する。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - 未オープンのファイルが拒否されること。
    EXPECT_EQ(EINVAL,
              cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細 errno が EINVAL であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// NULL のファイル クローズが拒否されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, close_rejects_null_file)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_close(NULL,
                                  &detail); // [手順] - NULL を指定して cplat_file_close を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - NULL のファイルが拒否されること。
    EXPECT_EQ(EINVAL,
              cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細 errno が EINVAL であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 無効なファイル記述子への書き込み失敗が通知されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, write_reports_os_failure)
{
    // Arrange
    cplat_file file;
    cplat_error detail;
    const char byte = 'x';
    cplat_file_init(&file);
    file.handle = std::numeric_limits<int>::max();
    file.writable = 1; // [状態] - 無効な記述子を書き込み可能状態で用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_write(&file, &byte, sizeof(byte),
                                  &detail); // [手順] - 無効な記述子へ 1 バイトを書き込む。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - OS の書き込み失敗が通知されること。
    EXPECT_EQ(EBADF,
              cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細 errno が EBADF であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 無効なファイル記述子からの読み取り失敗が通知されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, read_reports_os_failure)
{
    // Arrange
    cplat_file file;
    char buf[1] = {};
    size_t read = 0u;
    cplat_error detail;
    cplat_file_init(&file);
    file.handle = std::numeric_limits<int>::max(); // [状態] - 無効なファイル記述子を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_read(&file, buf, sizeof(buf), &read,
                                 &detail); // [手順] - 無効な記述子から 1 バイトを読み取る。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - OS の読み取り失敗が通知されること。
    EXPECT_EQ(EBADF,
              cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細 errno が EBADF であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 無効なファイル記述子の flush 失敗が通知されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, flush_reports_os_failure)
{
    // Arrange
    cplat_file file;
    cplat_error detail;
    cplat_file_init(&file);
    file.handle = std::numeric_limits<int>::max(); // [状態] - 無効なファイル記述子を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_flush(&file,
                                  &detail); // [手順] - 無効な記述子を flush する。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - OS の flush 失敗が通知されること。
    EXPECT_EQ(EBADF,
              cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細 errno が EBADF であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 読み取りがシグナルで中断された場合に再試行されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, read_retries_after_interrupt)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    unsigned char buffer[4] = {0u, 0u, 0u, 0u};
    size_t read_bytes = 0u;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, read(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(4)); // [Pre-Assert確認_正常系] - read が 2 回呼び出されること。
                              // [Pre-Assert手順] - errno に EINTR を設定した -1 ののち 4 を返却する。

    // Act
    int actual_ret = cplat_file_read(&file_, buffer, sizeof(buffer), &read_bytes,
                                 &detail); // [手順] - cplat_file_read を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 中断後に再試行した cplat_file_read の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)4,
              read_bytes); // [確認_正常系] - 再試行後の読み取りバイト数が 4 であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 書き込みがシグナルで中断された場合に再試行されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, write_retries_after_interrupt)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    const unsigned char buffer[4] = {1u, 2u, 3u, 4u};
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(4)); // [Pre-Assert確認_正常系] - write が 2 回呼び出されること。
                              // [Pre-Assert手順] - errno に EINTR を設定した -1 ののち 4 を返却する。

    // Act
    int actual_ret = cplat_file_write(&file_, buffer, sizeof(buffer),
                                  &detail); // [手順] - cplat_file_write を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 中断後に再試行した cplat_file_write の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

// 無効なファイル記述子のクローズ失敗が通知されることの確認
// [サブ手順参照 名前=fileFailureInjectionTest.SetUp]
TEST_F(fileFailureInjectionTest, close_reports_os_failure)
{
    // Arrange
    cplat_file file;
    cplat_error detail;
    cplat_file_init(&file);
    file.handle = std::numeric_limits<int>::max(); // [状態] - 無効なファイル記述子を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_close(&file,
                                  &detail); // [手順] - 無効な記述子をクローズする。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - OS のクローズ失敗が通知されること。
    EXPECT_EQ(EBADF,
              cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細 errno が EBADF であること。
}
// [サブ手順参照 名前=fileFailureInjectionTest.TearDown]

#endif /* PLATFORM_LINUX */
