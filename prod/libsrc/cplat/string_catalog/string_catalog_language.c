/**
 *******************************************************************************
 *  @file           string_catalog_language.c
 *  @brief          プロセスが文字列を出力する言語の設定を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  出力言語はプロセス全体で一元管理され、文字列組み立ての都度指定する必要はありません。\n
 *  利用側が設定していないプロセスでは、最初の参照時に実行環境の表示言語から決定します。\n
 *  決定は 1 回だけ行い、同じプロセスの記録が途中で別の言語へ切り替わらないようにします。
 *
 *  言語設定はプロセス共有の状態であり、アトミック変数で保持します。\n
 *  プロセスの初期化時に設定し、文字列組み立ての実行中は変更しない運用を前提とします。\n
 *  実行環境からの決定は複数のスレッドで重複して実行される場合がありますが、
 *  比較交換で最初に決まった言語だけを採用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/string_catalog/string_catalog.h>

#include <cplat/base/result.h>
#include <cplat/crt/string.h>
#include <cplat/locale/ui_language.h>
#include <cplat/string_catalog/language_internal.h>
#include <cplat/sync/atomic.h>

#include <string.h>

/** 出力する言語が未決定であることを示す値です。言語の列挙値とは重なりません。 */
#define LANGUAGE_UNDECIDED ((int32_t)-1)

/**
 *  プロセスが文字列を出力する言語です。
 *
 *  未決定の場合は `LANGUAGE_UNDECIDED` を保持し、最初の参照で実行環境から決定します。
 *  言語と決定済みかどうかを 1 つの値で表し、両者が食い違った状態を他スレッドから観測させません。
 */
static cplat_atomic_i32 s_language = CPLAT_ATOMIC_INIT(LANGUAGE_UNDECIDED);

/**
 *  言語タグの言語と、文字列を出力する言語の対応表です。
 *
 *  言語は英字 3 文字までのため、ポインターではなく固定長の配列で保持します。
 *  ポインターで保持すると、要素の末尾にパディングが生じます。
 */
static const struct
{
    char tag_language[4];                   /**< 言語タグの言語 (NUL 終端込み)。 */
    cplat_string_catalog_language language; /**< 対応する言語。 */
} s_language_map[] = {
    {"ja", CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE},
    {"en", CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH},
};

/** `s_language_map` の要素数です。 */
#define LANGUAGE_MAP_COUNT ((size_t)(sizeof(s_language_map) / sizeof(s_language_map[0])))

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_set_language(const cplat_string_catalog_language language)
{
    /* 列挙の基底型は処理系定義のため、符号なし整数へキャストして上限値のみを判定します。 */
    if ((unsigned int)language >= (unsigned int)CPLAT_STRING_CATALOG_LANGUAGE_COUNT)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 明示的な設定を実行環境よりも優先します。ニュートラル言語の設定も上書きしません。 */
    cplat_atomic_store_i32(&s_language, (int32_t)language, CPLAT_MEMORY_ORDER_RELEASE);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_string_catalog_language cplat_string_catalog_get_language(void)
{
    int32_t current = cplat_atomic_load_i32(&s_language, CPLAT_MEMORY_ORDER_ACQUIRE);

    if (current == LANGUAGE_UNDECIDED)
    {
        char tag[CPLAT_UI_LANGUAGE_TAG_MAX];
        cplat_string_catalog_language language = CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL;

        if (cplat_ui_language_get_tag(tag, sizeof(tag)) == CPLAT_OK)
        {
            /* 対応する言語が存在しない場合はニュートラル言語のままとします。 */
            (void)cplat_string_catalog_language_from_tag(tag, &language);
        }

        /* 他スレッドが先に決定または設定した場合は、その言語を採用します。
           交換しなかった場合は、current に読み取った現在の言語が入ります。 */
        if (cplat_atomic_compare_exchange_i32(&s_language, &current, (int32_t)language, CPLAT_MEMORY_ORDER_ACQ_REL))
        {
            current = (int32_t)language;
        }
    }

    return (cplat_string_catalog_language)current;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_language_from_tag(const char *const tag, cplat_string_catalog_language *const language_out)
{
    size_t length;
    size_t index;

    if ((tag == NULL) || (language_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *language_out = CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL;

    /* 空文字列はニュートラル言語の指定とみなします。 */
    if (tag[0] == '\0')
    {
        return CPLAT_OK;
    }

    /* 表記体系と地域は言語の後ろに続きます。対応付けには言語のみを使用します。 */
    length = strcspn(tag, "-");

    for (index = 0U; index < LANGUAGE_MAP_COUNT; index++)
    {
        const char *const tag_language = s_language_map[index].tag_language;

        if ((strlen(tag_language) == length) && (cplat_strncasecmp(tag, tag_language, length) == 0))
        {
            *language_out = s_language_map[index].language;
            return CPLAT_OK;
        }
    }

    return CPLAT_ERR_NOT_FOUND;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_string_catalog_language_reset_for_test(void)
{
    cplat_atomic_store_i32(&s_language, LANGUAGE_UNDECIDED, CPLAT_MEMORY_ORDER_RELEASE);
}
