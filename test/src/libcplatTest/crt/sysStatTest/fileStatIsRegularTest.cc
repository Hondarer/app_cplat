#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/sys/stat.h>

namespace
{

const char kFilePath[] = "is_regular.dat";
const char kDirPath[] = "is_regular_dir";

} // namespace

class fileStatIsRegularTest : public testing::Test
{
  protected:
    // [サブ手順 名前=fileStatIsRegularTest.SetUp]
    void SetUp() override
    {
        FILE *stream = nullptr;

        (void)cplat_remove(kFilePath, nullptr);
        (void)cplat_rmdir(kDirPath, nullptr);

        stream = cplat_fopen(kFilePath, "wb", nullptr);
        ASSERT_NE(nullptr, stream);
        // [状態確認] - cplat_fopen() の戻り値が nullptr 以外であること。
        ASSERT_EQ(CPLAT_OK, cplat_fclose(stream, nullptr));
        // [状態確認] - cplat_fclose() の戻り値が CPLAT_OK であること。
        ASSERT_EQ(CPLAT_OK, cplat_mkdir(kDirPath, nullptr));
        // [状態確認] - cplat_mkdir() の戻り値が CPLAT_OK であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=fileStatIsRegularTest.TearDown]
    void TearDown() override
    {
        (void)cplat_remove(kFilePath, nullptr);
        (void)cplat_rmdir(kDirPath, nullptr);
    }
    // [サブ手順終了]
};

// cplat_file_stat_is_regular が通常ファイルに対して 1 を返すことの確認
// [サブ手順参照 名前=fileStatIsRegularTest.SetUp]
TEST_F(fileStatIsRegularTest, returns_one_for_regular_file)
{
    // Arrange
    cplat_file_stat_t file_stat;

    ASSERT_EQ(CPLAT_OK, cplat_stat(&file_stat, nullptr, kFilePath)); // [状態] - 通常ファイルの情報を取得する。
    // [状態確認] - 通常ファイルに対する cplat_stat() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_is_regular = cplat_file_stat_is_regular(&file_stat); // [手順] - 種別を判定する。

    // Assert
    EXPECT_EQ(
        1,
        actual_ret_is_regular); // [確認_正常系] - 通常ファイルに対する cplat_file_stat_is_regular() の戻り値が 1 であること。
}
// [サブ手順参照 名前=fileStatIsRegularTest.TearDown]

// cplat_file_stat_is_regular がディレクトリに対して 0 を返すことの確認
// [サブ手順参照 名前=fileStatIsRegularTest.SetUp]
TEST_F(fileStatIsRegularTest, returns_zero_for_directory)
{
    // Arrange
    cplat_file_stat_t file_stat;

    ASSERT_EQ(CPLAT_OK, cplat_stat(&file_stat, nullptr, kDirPath)); // [状態] - ディレクトリの情報を取得する。
    // [状態確認] - ディレクトリに対する cplat_stat() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_is_regular = cplat_file_stat_is_regular(&file_stat); // [手順] - 種別を判定する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret_is_regular); // [確認_異常系] - ディレクトリに対する cplat_file_stat_is_regular() の戻り値が 0 であること。
}
// [サブ手順参照 名前=fileStatIsRegularTest.TearDown]

// cplat_file_stat_is_regular が NULL に対して 0 を返すことの確認
// [サブ手順参照 名前=fileStatIsRegularTest.SetUp]
TEST_F(fileStatIsRegularTest, returns_zero_for_null)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_is_regular = cplat_file_stat_is_regular(nullptr); // [手順] - NULL を指定する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret_is_regular); // [確認_異常系] - NULL に対する cplat_file_stat_is_regular() の戻り値が 0 であること。
}
// [サブ手順参照 名前=fileStatIsRegularTest.TearDown]
