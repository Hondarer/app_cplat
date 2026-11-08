#include <cplat/test/syslog_test.h>

#if defined(PLATFORM_LINUX)

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_syslog_test_fd_write(const char *buf, size_t nbytes)
{
    const char *fd_str = getenv("SYSLOG_TEST_FD");
    int test_fd;

    if (fd_str == NULL)
    {
        return 0;
    }

    test_fd = atoi(fd_str);
    if (test_fd >= 0)
    {
        (void)write(test_fd, buf, nbytes);
    }

    return 1;
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif /* PLATFORM_LINUX */
