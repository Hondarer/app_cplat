/**
 *******************************************************************************
 *  @file           utf8.h
 *  @brief          UTF-8 文字列の境界処理を行う内部 API を提供します。
 *
 *  切り詰め時に多バイト文字の途中で切れない位置を求める内部共有関数を提供します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_CRT_UTF8_INTERNAL_H
#define CPLAT_CRT_UTF8_INTERNAL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          UTF-8 の文字境界で切り詰めた長さを返します。
     *  @param[in]      text  対象の UTF-8 バイト列です。NULL を渡した場合は 0 を返します。
     *  @param[in]      len   検査対象の長さ [byte] です。
     *  @return         末尾の不完全な UTF-8 多バイト文字を除いた長さ [byte] を返します。
     *
     *  長さ @p len の末尾から最大 3 [byte] の後続バイト (`10xxxxxx`) をさかのぼり、
     *  直前の先頭バイトが示す文字バイト長 (2、3、または 4 [byte]) に満たない場合に、
     *  その先頭バイトの直前までの長さを返します。\n
     *  先頭バイトがない孤立した後続バイトの並びなど、UTF-8 として不正な並びは変更せず @p len を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    size_t cplat_internal_utf8_trimmed_length(const char *text, size_t len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_CRT_UTF8_INTERNAL_H */
