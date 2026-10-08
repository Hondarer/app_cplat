/**
 *******************************************************************************
 *  @file           ui_language_internal.h
 *  @brief          ロケールの指定を言語タグへ正規化する内部 API を提供します。
 *
 *  環境変数の値と OS が返すロケール名を、同じ規則で言語タグへ正規化します。
 *  正規化の規則を 1 か所に集約し、候補ごとの評価から共通に呼び出します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは複数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_LOCALE_UI_LANGUAGE_INTERNAL_H
#define CPLAT_LOCALE_UI_LANGUAGE_INTERNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          ロケールの指定を言語タグへ正規化します。
     *  @param[in]      value     正規化するロケールの指定 (null 終端文字列)。NULL を渡してはなりません。
     *  @param[out]     tag_out   言語タグの格納先。NULL を渡してはなりません。
     *  @param[in]      tag_size  @p tag_out のサイズ (バイト)。0 を渡してはなりません。
     *  @retval         CPLAT_OK                    言語タグを格納しました。\n
     *                                              ニュートラルの指定では空文字列を格納します。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  引数が不正、または @p value を言語タグとして解釈できません。
     *  @retval         CPLAT_ERR_BUFFER_TOO_SMALL  @p tag_out の容量が不足しています。
     *
     *  `.` 以降の文字コードと `@` 以降の修飾子を取り除き、残りを `_` または `-` で区切って解釈します。\n
     *  残りが `C` または `POSIX` の場合はニュートラルとし、空文字列を格納して @ref CPLAT_OK を返します。
     *
     *  先頭の区別を言語として小文字へ、英字 4 文字の区別を表記体系として先頭だけ大文字へ、
     *  英字 2 文字または数字 3 桁の区別を地域として大文字へそろえます。\n
     *  いずれにも当てはまらない構成は解釈できないものとし、@ref CPLAT_ERR_INVALID_ARGUMENT を返します。\n
     *  表記体系と地域より後ろの区別は、表示する文言の選択に使用しないため取り込みません。\n
     *  文字コードと修飾子を除いた部分が @ref CPLAT_UI_LANGUAGE_TAG_MAX に収まらない指定も、解釈できないものとして扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    int cplat_internal_ui_language_normalize(const char *value, char *tag_out, size_t tag_size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_LOCALE_UI_LANGUAGE_INTERNAL_H */
