#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #ifndef _GNU_SOURCE
        #error "_GNU_SOURCE must be defined via app/cplat/makepart.mk"
    #endif /* _GNU_SOURCE */

    #include <dlfcn.h>

/*
 * app/cplat/makepart.mk の DEFINES += _GNU_SOURCE が失われた場合、platform.h で
 * <assert.h> を取り込んだ後の <dlfcn.h> で Dl_info と RTLD_DEFAULT が見えず失敗する。
 */
int platform_feature_probe_has_gnu_dlfcn(void)
{
    Dl_info info = {0};
    const void *default_handle = RTLD_DEFAULT;

    return (info.dli_fname == default_handle) ? 1 : 1;
}
#else
    #if defined(_GNU_SOURCE)
        #error "_GNU_SOURCE must not be defined on non-Linux builds"
    #endif /* _GNU_SOURCE */

/* Windows ビルドでは Linux 向け GNU 拡張プローブを空にし、C 翻訳単位としての構築のみを行う。 */
int platform_feature_probe_has_gnu_dlfcn(void)
{
    return 0;
}
#endif /* PLATFORM_LINUX */
