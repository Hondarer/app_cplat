#include <cplat/test/syslog_test.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_LINUX)

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_syslog_test_fd_write(const char *buf, const size_t nbytes)
{
    const char *fd_str = getenv("SYSLOG_TEST_FD");
    int test_fd;

    if (fd_str == NULL)
    {
        return 0;
    }

    /*
     * 戻り値 1 はテスト FD への書き込み成功ではなく、実 /dev/log への送信を抑止する指示である。
     * FD 番号の解析や write() が失敗しても実 syslog へ流さない契約のため、結果を返却値に反映しない。
     */
    if (cplat_parse_int(&test_fd, fd_str, 10) == CPLAT_OK && test_fd >= 0)
    {
        (void)write(test_fd, buf, nbytes);
    }

    return 1;
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif /* PLATFORM_LINUX */
