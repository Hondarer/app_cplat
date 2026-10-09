#ifndef FILE_TEST_COMMON_H
#define FILE_TEST_COMMON_H

#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/crt/file.h>
#include <mock_cplat.h>

#include <errno.h>

#if defined(PLATFORM_LINUX)
    #include <fcntl.h>
    #include <mock_fcntl.h>
    #include <mock_unistd.h>
    #include <sys/mock_stat.h>
    #include <sys/stat.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::DoDefault;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

namespace
{

const char kPath[] = "file.dat";

#if defined(PLATFORM_LINUX)
const int kFakeFd = 7;

    #if defined(O_DSYNC)
const int kWriteThroughFlag = O_DSYNC;
    #else
const int kWriteThroughFlag = O_SYNC;
    #endif /* O_DSYNC */

void fill_stat(struct stat *st, off_t size, dev_t volume, ino_t index)
{
    *st = {};
    st->st_size = size;
    st->st_dev = volume;
    st->st_ino = index;
}

#endif /* PLATFORM_LINUX */

} // namespace

#if defined(PLATFORM_LINUX)

class fileTestFixture : public testing::Test
{
  protected:
    NiceMock<Mock_fcntl> mock_fcntl_;
    NiceMock<Mock_unistd> mock_unistd_;
    NiceMock<Mock_sys_stat> mock_sys_stat_;

    // [サブ手順 名前=fileTestFixture.SetUp]
    void SetUp() override
    {
        ON_CALL(mock_fcntl_, open(_, _, _, _, _, _)).WillByDefault(Return(kFakeFd));
        // [状態] - `open` の既定動作を設定する。
        ON_CALL(mock_unistd_, close(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - `close` の既定動作を設定する。
        ON_CALL(mock_unistd_, fsync(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - `fsync` の既定動作を設定する。
        ON_CALL(mock_unistd_, ftruncate(_, _, _, _, _)).WillByDefault(Return(0));
        // [状態] - `ftruncate` の既定動作を設定する。
        ON_CALL(mock_unistd_, write(_, _, _, _, _, _))
            .WillByDefault([](const char *, int, const char *, int, const void *, size_t count)
                           { return static_cast<ssize_t>(count); });
        // [状態] - `write` の既定動作を設定する。
        ON_CALL(mock_unistd_, read(_, _, _, _, _, _)).WillByDefault(Return(0));
        // [状態] - `read` の既定動作を設定する。
        ON_CALL(mock_sys_stat_, fstat(_, _, _, _, _))
            .WillByDefault(
                [](const char *, int, const char *, int, struct stat *st)
                {
                    fill_stat(st, 0, 1, 1);
                    return 0;
                });
        // [状態] - `fstat` の既定動作を設定する。
    }
    // [サブ手順終了]
};

#else /* PLATFORM_LINUX */

class fileTestFixture : public testing::Test
{
};

#endif /* PLATFORM_LINUX */

#endif /* FILE_TEST_COMMON_H */
