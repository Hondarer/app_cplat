/**
 *******************************************************************************
 *  @file           temp_file_internal.h
 *  @brief          Windows の一時ファイルを作成する内部 API を宣言します。
 *
 *  一時ディレクトリに一意な名前の空ファイルを作成し、その完全なパスを返す関数を提供します。
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

#ifndef CPLAT_CRT_TEMP_FILE_INTERNAL_H
#define CPLAT_CRT_TEMP_FILE_INTERNAL_H

#include <cplat/base/error.h>
#include <cplat/base/platform.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#if defined(PLATFORM_WINDOWS)

    /** 一時ファイルの名前の接頭辞として採用する最大の文字数 [wchar_t] です。 */
    #define CPLAT_INTERNAL_TEMP_FILE_PREFIX_MAX 3U

    /** 名前の衝突で作成をやり直す最大の回数 (最初の試行を含む) です。 */
    #define CPLAT_INTERNAL_TEMP_FILE_ATTEMPT_MAX 16

    /**
     *  @brief          一時ディレクトリに一意な名前の空ファイルを作成し、その完全なパスを返します (Windows だけ)。
     *  @param[in]      prefix     ファイル名の先頭に付ける接頭辞。NULL 可 (接頭辞なし)。
     *                             `CPLAT_INTERNAL_TEMP_FILE_PREFIX_MAX` 文字を超える場合は先頭の
     *                             `CPLAT_INTERNAL_TEMP_FILE_PREFIX_MAX` 文字を採用します。
     *  @param[out]     path_out   作成したファイルの完全なパス (NUL 終端) の格納先。
     *                             成功時は cplat_free で解放します。失敗時は NULL を格納します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         CPLAT_OK、CPLAT_ERR_INVALID_ARGUMENT、CPLAT_ERR_OUT_OF_MEMORY、
     *                  cplat_random_bytes の失敗の結果コード、OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  一時ディレクトリは cplat_internal_get_temp_dir_w で取得します。\n
     *  ファイル名は、接頭辞、暗号論的乱数の 8 バイトの 16 進表記 (小文字 16 文字)、`.tmp` の順に連結します。\n
     *  `CreateFileW` を `CREATE_NEW` 指定で呼び出して作成し、すぐに閉じます。同じ名前のファイルが
     *  すでにある場合は別の名前でやり直し、`CPLAT_INTERNAL_TEMP_FILE_ATTEMPT_MAX` 回の試行で
     *  作成できなければ、最後の衝突のエラーで失敗を返します。\n
     *  パスの区切りは `\\` へそろえます。完全なパスの長さ (NUL を含まない) が `MAX_PATH` 以上になる場合だけ、
     *  拡張パスの接頭辞を付けます。ドライブのパスには `\\?\` を前置し、UNC のパス (`\\server\share\...`) は
     *  `\\?\UNC\server\share\...` にします。一時ディレクトリがすでに `\\?\` または `\\.\` で始まる場合は付けません。
     *
     *  呼び出し元は、不要になったファイルを削除する責任があります。
     */
    int cplat_internal_create_temp_file_w(const wchar_t *prefix, wchar_t **path_out, cplat_error *detail_out);

#endif /* PLATFORM_WINDOWS */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_CRT_TEMP_FILE_INTERNAL_H */
