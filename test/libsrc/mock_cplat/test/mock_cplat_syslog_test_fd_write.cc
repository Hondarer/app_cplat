#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)
int delegate_real_cplat_syslog_test_fd_write(const char *buf, size_t nbytes)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_syslog_test_fd_write)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_syslog_test_fd_write"));
    return real_fn(buf, nbytes);
}

MOCK_WEAK_IMPL(int, cplat_syslog_test_fd_write, const char *buf, size_t nbytes)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_syslog_test_fd_write(buf, nbytes);
    }
    else
    {
        mock_ret = delegate_real_cplat_syslog_test_fd_write(buf, nbytes);
    }

    return mock_ret;
}
#endif
