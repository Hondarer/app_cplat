/**
 *******************************************************************************
 *  @file           module.c
 *  @brief          共有ライブラリ自身 (.so/.dll) の絶対パスと basename
 *                  (パスなし・拡張子なし) を取得します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  Linux(GCC) では dladdr() で共有オブジェクトを特定し、realpath() で可能な限り正規化 (絶対化・symlink 解決) します。
 *  Windows(MSVC) では GetModuleHandleEx() で DLL の HMODULE を得て、GetModuleFileNameW() でパスを取得します。
 *
 *  @note           - 「絶対パス」は OS/ローダーの情報とファイル システム状態に依存します。
 *                    Linux でロード後にファイルが移動/削除される等により realpath()
 *                    が失敗する場合、可能な範囲で絶対化した文字列を返します。
 *                  - Windows は基本的に完全なパスが得られますが、古い環境では MAX_PATH 制約が残る場合があります。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/runtime/module.h>
#include <cplat/crt/wchar_conv.h>
#include <cplat/crt/path.h>
#include <cplat/crt/string.h>
#include <errno.h>
#if defined(PLATFORM_LINUX)
    #include <dlfcn.h>
    #include <stddef.h>
    #include <stdint.h>
    #include <string.h>
#elif defined(PLATFORM_WINDOWS)
    #include <stddef.h>
    #include <stdint.h>
    #include <string.h>
#endif /* PLATFORM_ */

/**
 *  @brief          共有ライブラリ特有の拡張子 (Linux の .so 系) の切り出し位置を求めます。
 *
 *  - ".so." が含まれる場合はその位置 (例: libx.so.1.2.3 -> "libx" の直後)
 *  - 末尾が ".so" の場合はその位置 (例: libx.so -> "libx" の直後)
 *  - 末尾が ".dylib" の場合はその位置
 *  - いずれにも該当しない場合は NULL (汎用の cplat_path_strip_extension() に委譲する)
 *
 *  @param[in]      s 対象文字列 (basename 済み、NULL 終端)。
 *  @return         切り出し位置 (@p s 内を指す) または NULL。
 */
static const char *find_shared_lib_extension_cut(const char *s)
{
#if defined(PLATFORM_LINUX)
    const char *so_ver = strstr(s, ".so.");
    size_t len;

    if (so_ver != NULL)
    {
        return so_ver;
    }

    len = strlen(s);
    if (len >= 3 && strcmp(s + (len - 3), ".so") == 0)
    {
        return s + (len - 3);
    }
    if (len >= 6 && strcmp(s + (len - 6), ".dylib") == 0)
    {
        return s + (len - 6);
    }
#else
    (void)s;
#endif /* PLATFORM_LINUX */

    return NULL;
}

#if defined(PLATFORM_LINUX)

/**
 *  @brief          .so 自身の絶対パスを取得します (Linux/Unix)。
 *
 *  dladdr() に指定された関数アドレスを渡して所属共有オブジェクトを取得し、\n
 *  realpath() で可能な限り絶対化・正規化します。
 *
 *  @param[out]     path_out    出力 (UTF-8、NULL 終端)。
 *  @param[in]      path_size   出力バッファーのサイズ (バイト)。
 *  @param[in]      func_addr   所属モジュールを特定するための関数アドレス。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれか。
 */
static int get_self_path_posix(char *path_out, size_t path_size, const void *func_addr)
{
    Dl_info info = {0};
    const char *p;
    cplat_error err;

    if (dladdr(func_addr, &info) == 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    if (info.dli_fname != NULL)
    {
        p = info.dli_fname;
    }
    else
    {
        p = "";
    }
    if (p[0] == '\0')
        return CPLAT_ERR_UNKNOWN;

    if (cplat_path_get_full(path_out, path_size, &err, p) == CPLAT_OK)
    {
        return CPLAT_OK;
    }

    if (cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG))
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    else
    {
        return CPLAT_ERR_UNKNOWN;
    }
}

#elif defined(PLATFORM_WINDOWS)

/**
 *  @brief          DLL 自身の絶対パス (ワイド文字列) を取得します。
 *
 *  @param[out]     wpath_out   出力 (UTF-16、NULL 終端)。
 *  @param[in]      wpath_size  出力バッファーのサイズ (wchar_t 個数)。
 *  @param[in]      func_addr   所属モジュールを特定するための関数アドレス。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれか。
 */
static int get_self_path_w(wchar_t *wpath_out, size_t wpath_size, const void *func_addr)
{
    HMODULE hm = NULL;
    wchar_t buf[PLATFORM_PATH_MAX];
    DWORD n;

    if (wpath_out == NULL || wpath_size == 0 || func_addr == NULL)
        return CPLAT_ERR_INVALID_ARGUMENT;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)func_addr, &hm))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /* 本関数はワイド文字列を出力するため、UTF-8 を返す GetModuleFileNameU は使えない */
    n = GetModuleFileNameW(hm, buf, (DWORD)(sizeof(buf) / sizeof(buf[0])));
    if (n == 0 || n >= (DWORD)(sizeof(buf) / sizeof(buf[0])))
    {
        return CPLAT_ERR_UNKNOWN;
    }
    buf[n] = L'\0';
    if (wcslen(buf) + 1 > wpath_size)
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    (void)cplat_wcscpy(wpath_out, wpath_size, buf);
    return CPLAT_OK;
}

#endif /* PLATFORM_ */

/**
 *  @brief          パスの basename から共有ライブラリ拡張子または通常拡張子を除去します。
 *  @param[out]     basename_out   basename の出力先。
 *  @param[in]      basename_size  出力先のサイズ。
 *  @param[in]      path           basename を取得するパス。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int get_basename_from_path(char *basename_out, size_t basename_size, const char *path)
{
    const char *fname = cplat_path_basename(path);
    const char *shared_lib_cut;

    if (fname[0] == '\0')
    {
        basename_out[0] = '\0';
        return CPLAT_ERR_UNKNOWN;
    }

    shared_lib_cut = find_shared_lib_extension_cut(fname);
    if (shared_lib_cut != NULL)
    {
        size_t len = (size_t)(shared_lib_cut - fname);

        if (len + 1u > basename_size)
        {
            basename_out[0] = '\0';
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
        memcpy(basename_out, fname, len);
        basename_out[len] = '\0';
        return CPLAT_OK;
    }

    {
        cplat_error path_error;

        if (cplat_path_strip_extension(basename_out, basename_size, &path_error, fname) != CPLAT_OK)
        {
            basename_out[0] = '\0';
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_module_get_path(char *path_out, const size_t path_size, const void *func_addr)
{
    if (path_out == NULL || path_size == 0u || func_addr == NULL)
    {
        if (path_out != NULL && path_size > 0u)
        {
            path_out[0] = '\0';
        }
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_LINUX)
    return get_self_path_posix(path_out, path_size, func_addr);
#elif defined(PLATFORM_WINDOWS)
    wchar_t wpath[PLATFORM_PATH_MAX];
    char utf8_path[PLATFORM_PATH_MAX];
    cplat_error err;
    int st = get_self_path_w(wpath, (size_t)(sizeof(wpath) / sizeof(wpath[0])), func_addr);
    if (st != CPLAT_OK)
    {
        if (path_out && path_size)
            path_out[0] = '\0';
        return st;
    }
    if (cplat_wpath_to_utf8(utf8_path, sizeof(utf8_path), wpath) < 0)
    {
        if (path_out && path_size)
            path_out[0] = '\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    if (cplat_path_get_full(path_out, path_size, &err, utf8_path) != CPLAT_OK)
    {
        if (path_out && path_size)
            path_out[0] = '\0';
        if (cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG))
        {
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
        else
        {
            return CPLAT_ERR_UNKNOWN;
        }
    }
    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_module_get_basename(char *basename_out, const size_t basename_size, const void *func_addr)
{
    int st;
    char path_buf[4096];

    if (basename_out == NULL || basename_size == 0)
        return CPLAT_ERR_INVALID_ARGUMENT;

    st = cplat_module_get_path(path_buf, sizeof(path_buf), func_addr);
    if (st != CPLAT_OK)
    {
        basename_out[0] = '\0';
        return st;
    }

    return get_basename_from_path(basename_out, basename_size, path_buf);
}
