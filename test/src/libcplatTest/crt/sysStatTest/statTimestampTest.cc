#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>
#include <cplat/crt/file.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/sys/stat.h>

namespace
{

const char kPath[] = "stat_timestamp.dat";

cplat_timespec make_timestamp(time_t tv_sec, int64_t tv_nsec)
{
    cplat_timespec timestamp;

    timestamp.tv_sec = tv_sec;
    timestamp.tv_nsec = tv_nsec;
    return timestamp;
}

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

} // namespace

class statTimestampTest : public Test
{
  protected:
    void SetUp() override
    {
        (void)cplat_remove(kPath, NULL);
        create_file(kPath);
    }

    void TearDown() override
    {
        (void)cplat_remove(kPath, NULL);
    }
};

/*
 *  _wstat64 が成功したあと GetFileAttributesExW が失敗する経路は、対象パスが
 *  二つの呼び出しの間に消える競合など一時的な状態であり、安定して注入できない。
 *  失敗時は _wstat64 の時刻欄を残して CPLAT_OK を返す。
 */

// 設定した最終更新日時の秒部が cplat_stat の st_mtime と一致することの確認
TEST_F(statTimestampTest, seconds_agree_with_set_modified_timestamp)
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
