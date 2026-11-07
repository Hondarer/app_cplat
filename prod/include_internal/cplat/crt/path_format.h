/**
 *******************************************************************************
 *  @file           path_format.h
 *  @brief          書式指定された UTF-8 パスを生成する内部 API を提供します。
 *
 *  CRT 抽象 API の書式付きパス関数で共有するバッファー検証と書式処理を提供します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_CRT_PATH_FORMAT_INTERNAL_H
#define CPLAT_CRT_PATH_FORMAT_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     *  @brief          可変長引数リストから UTF-8 パス文字列を生成します。
     *  @param[out]     path       生成した NUL 終端文字列の格納先です。
     *  @param[in]      path_size  @p path のバイト数です。
     *  @param[in]      format     printf 形式の書式文字列です。
     *  @param[in]      args       @p format に対応する可変長引数リストです。
     *  @param[out]     error_out  失敗理由の errno 値を格納します。NULL も指定できます。
     *  @return         成功時は 0 を返します。引数不正、書式処理失敗、またはバッファー不足の場合は
     *                  -1 を返します。失敗時の @p path の内容は使用できません。
     */
    int cplat_internal_vformat_path(char *path, size_t path_size, const char *format, va_list args, int *error_out);

#ifdef __cplusplus
}
#endif

#endif /* CPLAT_CRT_PATH_FORMAT_INTERNAL_H */
