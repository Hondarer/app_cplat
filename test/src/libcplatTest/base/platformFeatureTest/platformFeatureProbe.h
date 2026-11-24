#ifndef PLATFORM_FEATURE_PROBE_H
#define PLATFORM_FEATURE_PROBE_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief  C の翻訳単位で <cplat/base/platform.h> 先頭取り込み時の _GNU_SOURCE 有効性を検証します。
     *  @return Linux の場合は 1、非 Linux の場合は 0 を返します。
     */
    int platform_feature_probe_has_gnu_dlfcn(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PLATFORM_FEATURE_PROBE_H */
