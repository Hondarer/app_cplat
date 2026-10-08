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
    void SetUp() override
    {
        FILE *stream = NULL;

        (void)cplat_remove(kFilePath, NULL);
        (void)cplat_rmdir(kDirPath, NULL);

        stream = cplat_fopen(kFilePath, "wb", NULL);
        ASSERT_NE(nullptr, stream);
        // [状態確認] - `nullptr` と `stream` が異なること。
        ASSERT_EQ(CPLAT_OK, cplat_fclose(stream, NULL));
        // [状態確認] - `cplat_fclose(stream, NULL)` の戻り値が `CPLAT_OK` であること。
        ASSERT_EQ(CPLAT_OK, cplat_mkdir(kDirPath, NULL));
        // [状態確認] - `cplat_mkdir(kDirPath, NULL)` の戻り値が `CPLAT_OK` であること。
    }

    void TearDown() override
    {
        (void)cplat_remove(kFilePath, NULL);
        (void)cplat_rmdir(kDirPath, NULL);
    }
};

// cplat_file_stat_is_regular が通常ファイルに対して 1 を返すことの確認
TEST_F(fileStatIsRegularTest, returns_one_for_regular_file)
{
    // Arrange
    cplat_file_stat_t file_stat;

    ASSERT_EQ(CPLAT_OK, cplat_stat(&file_stat, NULL, kFilePath)); // [状態] - 通常ファイルの情報を取得する。
    // [状態確認] - `cplat_stat(&file_stat, NULL, kFilePath)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    int actual_ret_is_regular = cplat_file_stat_is_regular(&file_stat); // [手順] - 種別を判定する。

    // Assert
    EXPECT_EQ(1, actual_ret_is_regular); // [確認_正常系] - 戻り値が 1 であること。
}

// cplat_file_stat_is_regular がディレクトリに対して 0 を返すことの確認
TEST_F(fileStatIsRegularTest, returns_zero_for_directory)
{
    // Arrange
    cplat_file_stat_t file_stat;

    ASSERT_EQ(CPLAT_OK, cplat_stat(&file_stat, NULL, kDirPath)); // [状態] - ディレクトリの情報を取得する。
    // [状態確認] - `cplat_stat(&file_stat, NULL, kDirPath)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    int actual_ret_is_regular = cplat_file_stat_is_regular(&file_stat); // [手順] - 種別を判定する。

    // Assert
    EXPECT_EQ(0, actual_ret_is_regular); // [確認_異常系] - 戻り値が 0 であること。
}

// cplat_file_stat_is_regular が NULL に対して 0 を返すことの確認
TEST_F(fileStatIsRegularTest, returns_zero_for_null)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_is_regular = cplat_file_stat_is_regular(NULL); // [手順] - NULL を指定する。

    // Assert
    EXPECT_EQ(0, actual_ret_is_regular); // [確認_異常系] - 戻り値が 0 であること。
}
