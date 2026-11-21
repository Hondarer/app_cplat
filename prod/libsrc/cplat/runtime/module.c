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
 *                  - Windows はヒープ領域を使用して OS の拡張パス上限 (32767 文字) までのパスを取得できます。
 *                    パスに `\\?\` が含まれる場合も区切り文字は `'/'` へ正規化されます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/runtime/module.h>
#include <cplat/crt/wchar_conv.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#if defined(PLATFORM_LINUX)
    #include <dlfcn.h>
    #include <stddef.h>
    #include <stdint.h>
    #include <string.h>
#elif defined(PLATFORM_WINDOWS)
    #include <stddef.h>
    #include <stdint.h>
    #include <string.h>

/*
 * Windows の拡張パス (\\?\ 接頭辞) の最大長 32767 文字と終端 NUL を合わせた文字数 [wchar_t]。
 * GetModuleFileNameW() の nSize 引数と同じ DWORD 型に合わせて符号なしとし、上限確認に使用する。
 * see: https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
 */
    #define MODULE_WIN32_EXTENDED_PATH_CAP 32768U

static_assert(MODULE_WIN32_EXTENDED_PATH_CAP == 32767U + 1U, "extended path capacity must include NUL");
static_assert(MODULE_WIN32_EXTENDED_PATH_CAP <= (size_t)INT_MAX, "must fit in int for WideCharToMultiByte");
#endif /* PLATFORM_ */

/*
 * cplat_module_get_basename() がモジュールのパス全体を受け取る作業領域の上限 [byte] (終端 NUL を含む)。
 * Windows は、拡張パスの上限の文字数を UTF-8 へ変換した場合の最大とする。
 * UTF-16 の 1 単位は、UTF-8 で最大 3 [byte] になる (サロゲート ペアは 2 単位で 4 [byte])。
 * Linux の cplat_path_get_full() は PATH_MAX を超えるパスを扱わないため、作業領域を広げても結果は変わらない。
 */
#if defined(PLATFORM_LINUX)
    #define MODULE_PATH_UTF8_CAP_MAX PLATFORM_PATH_UTF8_MAX
#elif defined(PLATFORM_WINDOWS)
    #define MODULE_PATH_UTF8_CAP_MAX (MODULE_WIN32_EXTENDED_PATH_CAP * 3U)
#endif /* PLATFORM_ */

static_assert(MODULE_PATH_UTF8_CAP_MAX >= PLATFORM_PATH_UTF8_MAX, "work area limit must cover the initial size");

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
 *  @param[in]      path_size   出力バッファーのサイズ [byte]。
 *  @param[in]      func_addr   所属モジュールを特定するための関数アドレス。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
 *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれか。
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
    {
        return CPLAT_ERR_UNKNOWN;
    }

    if (cplat_path_get_full(path_out, path_size, &err, p) == CPLAT_OK)
    {
        return CPLAT_OK;
    }

    if (cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG))
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    else if (cplat_error_is(&err, CPLAT_CAUSE_OUT_OF_MEMORY))
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    else
    {
        return CPLAT_ERR_UNKNOWN;
    }
}

#elif defined(PLATFORM_WINDOWS)

/**
 *  @brief          DLL 自身のワイド文字列パスをヒープへ取得します。
 *
 *  @param[in]      func_addr   所属モジュールを特定するための関数アドレス。
 *  @param[out]     wpath_out   ヒープ確保した UTF-16 パスの格納先。呼び出し元が cplat_free() で解放すること。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
 *                  @ref CPLAT_ERR_UNKNOWN のいずれか。
 */
static int get_self_path_w_alloc(const void *func_addr, wchar_t **wpath_out)
{
    HMODULE hm = NULL;
    wchar_t *wbuf;
    DWORD n;

    if (wpath_out == NULL || func_addr == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *wpath_out = NULL;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)func_addr, &hm))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    wbuf = (wchar_t *)cplat_malloc((size_t)MODULE_WIN32_EXTENDED_PATH_CAP * sizeof(wchar_t));
    if (wbuf == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    /* 本関数はワイド文字列を出力するため、UTF-8 を返す GetModuleFileNameU() は使えない。 */
    n = GetModuleFileNameW(hm, wbuf, (DWORD)MODULE_WIN32_EXTENDED_PATH_CAP);
    if (n == 0U || n >= (DWORD)MODULE_WIN32_EXTENDED_PATH_CAP)
    {
        cplat_free(wbuf);
        return CPLAT_ERR_UNKNOWN;
    }

    wbuf[n] = L'\0';
    *wpath_out = wbuf;

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

        if ((len + 1u) > basename_size)
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
    wchar_t *wpath = NULL;
    wchar_t *wfull = NULL;
    DWORD full_cap;
    DWORD full_len;
    int needed_utf8;
    int st;

    st = get_self_path_w_alloc(func_addr, &wpath);
    if (st != CPLAT_OK)
    {
        path_out[0] = '\0';
        return st;
    }

    /* 拡張パス長に対応するため、cplat_path_get_full() は経由せず GetFullPathNameW() で必要文字数を照会する。 */
    full_cap = GetFullPathNameW(wpath, 0U, NULL, NULL);
    if (full_cap == 0U)
    {
        cplat_free(wpath);
        path_out[0] = '\0';
        return CPLAT_ERR_UNKNOWN;
    }

    wfull = (wchar_t *)cplat_malloc((size_t)full_cap * sizeof(wchar_t));
    if (wfull == NULL)
    {
        cplat_free(wpath);
        path_out[0] = '\0';
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    full_len = GetFullPathNameW(wpath, full_cap, wfull, NULL);
    cplat_free(wpath);
    if (full_len == 0U || full_len >= full_cap)
    {
        cplat_free(wfull);
        path_out[0] = '\0';
        return CPLAT_ERR_UNKNOWN;
    }

    /*
     * 出力先を NULL、サイズを 0 にして必要な UTF-8 バイト数 (終端 NUL 含む) を求め、
     * 呼び出し元の出力バッファー不足だけを CPLAT_ERR_BUFFER_TOO_SMALL として区別する。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
     */
    needed_utf8 = WideCharToMultiByte(CP_UTF8, 0, wfull, -1, NULL, 0, NULL, NULL);
    if (needed_utf8 <= 0)
    {
        cplat_free(wfull);
        path_out[0] = '\0';
        return CPLAT_ERR_UNKNOWN;
    }

    if ((size_t)needed_utf8 > path_size)
    {
        cplat_free(wfull);
        path_out[0] = '\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    if (cplat_wpath_to_utf8(path_out, path_size, wfull) < 0)
    {
        cplat_free(wfull);
        path_out[0] = '\0';
        return CPLAT_ERR_UNKNOWN;
    }

    cplat_free(wfull);
    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_module_get_basename(char *basename_out, const size_t basename_size, const void *func_addr)
{
    int st;
    size_t path_cap;
    char *path_buf;

    if (basename_out == NULL || basename_size == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    path_cap = (size_t)PLATFORM_PATH_UTF8_MAX;
    for (;;)
    {
        path_buf = (char *)cplat_malloc(path_cap);
        if (path_buf == NULL)
        {
            basename_out[0] = '\0';
            return CPLAT_ERR_OUT_OF_MEMORY;
        }

        st = cplat_module_get_path(path_buf, path_cap, func_addr);
        if (st == CPLAT_ERR_BUFFER_TOO_SMALL && path_cap < (size_t)MODULE_PATH_UTF8_CAP_MAX)
        {
            /*
             * 作業領域が足りないため、広げて取得し直す。
             * 上限に達した後の失敗は、広げても解消しないため、そのまま返す。
             */
            cplat_free(path_buf);
            if (path_cap > (size_t)MODULE_PATH_UTF8_CAP_MAX / 2U)
            {
                path_cap = (size_t)MODULE_PATH_UTF8_CAP_MAX;
            }
            else
            {
                path_cap *= 2U;
            }
            continue;
        }

        if (st != CPLAT_OK)
        {
            cplat_free(path_buf);
            basename_out[0] = '\0';
            return st;
        }

        st = get_basename_from_path(basename_out, basename_size, path_buf);
        cplat_free(path_buf);
        return st;
    }
}
