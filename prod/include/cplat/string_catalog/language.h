/**
 *******************************************************************************
 *  @file           language.h
 *  @brief          文字列を出力する言語を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  言語はライブラリ側で規定します。カタログを注入する利用側では変更できません。\n
 *  言語を追加する場合は、本列挙とカタログの言語別リソースを合わせて更新してください。
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

#ifndef CPLAT_STRING_CATALOG_LANGUAGE_H
#define CPLAT_STRING_CATALOG_LANGUAGE_H

/**
 *  @ingroup        CPLAT_STRING_CATALOG
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          文字列を出力する言語を表します。
     *
     *  @ref CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL は、特定の自然言語に属さないニュートラル言語です。\n
     *  @ref cplat_string_catalog_set_language を呼び出していないプロセスの既定値です。\n
     *  他の言語のリソースが未設定の場合のフォールバック先でもあります。
     *
     *  @ref CPLAT_STRING_CATALOG_LANGUAGE_COUNT は言語の総数であり、言語ではありません。\n
     *  言語別リソースの配列長として使用します。
     */
    typedef enum cplat_string_catalog_language
    {
        CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL = 0,  /**< ニュートラル言語。未指定時の既定値。 */
        CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE = 1, /**< 日本語。 */
        CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH = 2,  /**< 英語。 */
        CPLAT_STRING_CATALOG_LANGUAGE_COUNT = 3     /**< 言語の総数。 */
    } cplat_string_catalog_language;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_STRING_CATALOG_LANGUAGE_H */
