// syncTest の各スイートで共有するテスト ヘルパー
#ifndef SYNC_TEST_HELPER_H
#define SYNC_TEST_HELPER_H

#include <cplat/base/platform.h>
#include <cplat/sync/sync.h>
#include <mock_cplat.h>

#include <stdio.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <errno.h>
    #include <fcntl.h>
    #include <mock_fcntl.h>
    #include <mock_unistd.h>
    #include <sys/mock_file.h>
    #include <time.h>

using testing::_;
using testing::Return;

const int kFakeFd = 7;
const int kFakeFd2 = 8;
const char kLockIdentity[] = "sync.lock";
const char kRwlockIdentity[] = "sync.rwlock";

// open / close だけをセンチネル記述子へ差し替える。flock は各テストが個別に設定する。
struct InterprocessOpenMocks
{
    testing::NiceMock<Mock_fcntl> fcntl;
    testing::NiceMock<Mock_unistd> unistd;

    InterprocessOpenMocks()
    {
        ON_CALL(fcntl, open(_, _, _, _, _, _)).WillByDefault(Return(kFakeFd));
        ON_CALL(unistd, close(_, _, _, _)).WillByDefault(Return(0));
    }
};

// open / close / flock を既定で成功させる。失敗経路は EXPECT_CALL で上書きする。
struct InterprocessOsMocks : InterprocessOpenMocks
{
    testing::NiceMock<Mock_sys_file> sys_file;

    InterprocessOsMocks()
    {
        ON_CALL(sys_file, flock(_, _, _, _, _)).WillByDefault(Return(0));
    }
};

static inline uint64_t test_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}
#elif defined(PLATFORM_WINDOWS)
    #include <process.h>
    #include <io.h>
    #include <cplat/base/windows_sdk.h>
static inline uint64_t test_monotonic_ms(void)
{
    return (uint64_t)GetTickCount64();
}
static inline void make_test_interprocess_path(char *buf, size_t size, const char *tag)
{
    char tmp_dir[256];
    DWORD len = GetTempPathA((DWORD)sizeof(tmp_dir), tmp_dir);
    if (len == 0)
    {
        tmp_dir[0] = '.';
        tmp_dir[1] = '\0';
        len = 1;
    }
    while (len > 0 && (tmp_dir[len - 1] == '\\' || tmp_dir[len - 1] == '/'))
    {
        tmp_dir[--len] = '\0';
    }
    snprintf(buf, size, "%s/cplat_%s_%ld.lock", tmp_dir, tag, (long)_getpid());
}
    #define TEST_INTERPROCESS_UNLINK(path) _unlink(path)
#endif

#endif /* SYNC_TEST_HELPER_H */
