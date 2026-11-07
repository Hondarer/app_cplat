/**
 *******************************************************************************
 *  @file           ui_language.h
 *  @brief          実行環境が示す表示言語を取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/21
 *
 *  表示言語は、画面やメッセージの表示に使用する言語です。\n
 *  日付や数値の書式に使用する地域設定とは別に扱います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_LOCALE_UI_LANGUAGE_H
#define CPLAT_LOCALE_UI_LANGUAGE_H

#include <stddef.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_LOCALE
 *  @{
 */

/**
 *  @brief          言語タグの格納に使う配列サイズです (NUL 終端込み)。
 *
 *  `char tag[CPLAT_UI_LANGUAGE_TAG_MAX];` として @ref cplat_ui_language_get_tag へ渡せば、
 *  Linux と Windows が示す表示言語の言語タグが収まる想定です。\n
 *  Windows の `LOCALE_NAME_MAX_LENGTH` (85) に合わせています。
 */
#define CPLAT_UI_LANGUAGE_TAG_MAX 85

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          実行環境が示す表示言語を言語タグで取得します。
     *  @param[out]     tag_out   言語タグの格納先。NULL を渡してはなりません。\n
     *                            表示言語がニュートラルの場合は空文字列を格納します。
     *  @param[in]      tag_size  @p tag_out のサイズ [byte]。0 を渡してはなりません。
     *  @retval         CPLAT_OK                    表示言語を格納しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  @p tag_out が NULL、または @p tag_size が 0 です。
     *  @retval         CPLAT_ERR_BUFFER_TOO_SMALL  @p tag_out の容量が不足しています。
     *
     *  環境変数 `LC_ALL`、`LC_MESSAGES`、`LANG` をこの順に評価し、最初に見つかった空でない指定を使用します。\n
     *  指定が `C` または `POSIX` の場合はニュートラルとし、言語タグとして解釈できない指定は次の候補へ進みます。\n
     *  環境変数で決定できない場合、Windows では利用者が設定した表示言語を使用し、Linux ではニュートラルとします。
     *
     *  格納する言語タグは、言語を小文字、表記体系を先頭だけ大文字、地域を大文字にした
     *  `ja`、`ja-JP`、`zh-Hans-CN` のような表記です。\n
     *  ニュートラルは空文字列で表し、表示言語を決定できないことは失敗として扱いません。
     *
     *  取得した結果は保持しません。\n
     *  プロセスの実行中に同じ結果を使用する場合は、呼び出し側で保持してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  他スレッドが環境変数を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に環境変数を変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_ui_language_get_tag(char *tag_out, size_t tag_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_LOCALE_UI_LANGUAGE_H */
