/**
 *******************************************************************************
 *  @file           temp_dir_internal.h
 *  @brief          Windows の一時ディレクトリ取得を行う内部 API を宣言します。
 *
 *  GetTempPath2W / GetTempPathW の実行時解決と動的バッファー確保による取得を提供します。
 *  このヘッダーはモジュール内部でのみ使用します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは複数のモジュールから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_CRT_TEMP_DIR_INTERNAL_H
#define CPLAT_CRT_TEMP_DIR_INTERNAL_H

#include <cplat/base/error.h>
#include <cplat/base/platform.h>

#include <stddef.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if defined(PLATFORM_WINDOWS)

    /** 一時ディレクトリ取得関数 (`GetTempPath2W` / `GetTempPathW`) の関数ポインター型です。 */
    typedef DWORD(WINAPI *cplat_internal_temp_path_w_fn)(DWORD buffer_length, LPWSTR buffer);

    /**
     *  @brief          一時ディレクトリのパスを、確保した領域へ取得します (Windows だけ)。
     *  @param[out]     dir_out    取得したパス (末尾の '\\' を含む、NUL 終端) の格納先。
     *                             成功時は cplat_free で解放します。失敗時は NULL を格納します。
     *  @param[out]     length_out 取得したパスの長さ (NUL を含まない要素数) の格納先。NULL 可。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         CPLAT_OK、CPLAT_ERR_INVALID_ARGUMENT、CPLAT_ERR_OUT_OF_MEMORY、CPLAT_ERR_BUFFER_TOO_SMALL、
     *                  OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  GetTempPath2W がある Windows ではそれを使い、ない Windows では GetTempPathW を使います。
     *  パスが存在するかどうかは確かめません。
     */
    int cplat_internal_get_temp_dir_w(wchar_t **dir_out, size_t *length_out, cplat_error *detail_out);

    /**
     *  @brief          一時ディレクトリ取得の解決済み関数をテスト用に差し替えます (Windows だけ)。
     *  @param[in]      resolver 差し替える取得関数。NULL を渡すとテスト用の差し替えを解除し、
     *                           次回呼び出し時に通常の解決へ戻します。
     *
     *  単体テストで GetTempPath2W による SystemTemp などの選択や取得関数の解決状態を切り替えるための内部 API です。
     *  製品コードからは呼び出さないでください。
     */
    void cplat_internal_temp_dir_set_resolver_for_test(cplat_internal_temp_path_w_fn resolver);

#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_CRT_TEMP_DIR_INTERNAL_H */
