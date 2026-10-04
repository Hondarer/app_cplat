/**
 *******************************************************************************
 *  @file           ui_language.c
 *  @brief          実行環境が示す表示言語を取得する API を実装します。
 *
 *  環境変数、OS の設定、ニュートラルの順に候補を評価し、最初に決定した言語タグを返します。\n
 *  プロセスのロケール設定は変更しません。`setlocale` は呼び出し元の状態を書き換えるうえ、
 *  指定されたロケールが導入されていない環境では失敗するためです。
 *
 *******************************************************************************
 */

#include <cplat/locale/ui_language.h>

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/locale/ui_language_internal.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/wchar_conv.h>
#endif /* PLATFORM_WINDOWS */

/**
 *  表示言語の候補として評価する環境変数です。
 *
 *  メッセージの言語は `LC_MESSAGES` が決定し、`LC_ALL` が設定されている場合はそれが最優先となります。
 *  see: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html
 */
static const char *const s_environment_names[] = {"LC_ALL", "LC_MESSAGES", "LANG"};

/** `s_environment_names` の要素数です。 */
#define ENVIRONMENT_NAME_COUNT ((size_t)(sizeof(s_environment_names) / sizeof(s_environment_names[0])))

/**
 *  @brief          環境変数から表示言語を決定します。
 *  @param[out]     tag_out       言語タグの格納先。NULL を渡してはなりません。
 *  @param[in]      tag_size      @p tag_out のサイズ (バイト)。0 を渡してはなりません。
 *  @param[out]     decided_out   表示言語を決定した場合は 1、決定していない場合は 0 を格納します。
 *                                NULL を渡してはなりません。
 *  @retval         CPLAT_OK                    評価を完了しました。
 *  @retval         CPLAT_ERR_BUFFER_TOO_SMALL  @p tag_out の容量が不足しています。
 *
 *  ニュートラルを指定する環境変数を検出した場合は、空文字列を格納して決定済みとします。\n
 *  解釈できない指定は使用せず、次の候補の評価を続けます。
 */
static int decide_from_environment(char *const tag_out, const size_t tag_size, int *const decided_out)
{
    char value[CPLAT_UI_LANGUAGE_TAG_MAX];
    size_t index;

    *decided_out = 0;

    for (index = 0U; index < ENVIRONMENT_NAME_COUNT; index++)
    {
        int exists = 0;
        int ret;

        /* 言語タグに収まらない長さの指定は、言語タグとして解釈できないため次の候補へ進みます。 */
        ret = cplat_getenv(s_environment_names[index], value, sizeof(value), &exists, NULL);
        if ((ret != CPLAT_OK) || (exists == 0) || (value[0] == '\0'))
        {
            continue;
        }

        ret = cplat_internal_ui_language_normalize(value, tag_out, tag_size);
        if (ret == CPLAT_OK)
        {
            *decided_out = 1;
            return CPLAT_OK;
        }
        if (ret == CPLAT_ERR_BUFFER_TOO_SMALL)
        {
            return ret;
        }
    }

    return CPLAT_OK;
}

#if defined(PLATFORM_WINDOWS)

/**
 *  @brief          Windows の利用者設定から表示言語を決定します。
 *  @param[out]     tag_out       言語タグの格納先。NULL を渡してはなりません。
 *  @param[in]      tag_size      @p tag_out のサイズ (バイト)。0 を渡してはなりません。
 *  @param[out]     decided_out   表示言語を決定した場合は 1、決定していない場合は 0 を格納します。
 *                                NULL を渡してはなりません。
 *  @retval         CPLAT_OK                    評価を完了しました。
 *  @retval         CPLAT_ERR_BUFFER_TOO_SMALL  @p tag_out の容量が不足しています。
 *
 *  表示言語の優先順位の先頭を使用し、取得できない場合は地域設定の名前を使用します。
 */
static int decide_from_windows(char *const tag_out, const size_t tag_size, int *const decided_out)
{
    ULONG language_count = 0;
    ULONG buffer_length = 0;
    char value[CPLAT_UI_LANGUAGE_TAG_MAX];
    int ret = CPLAT_OK;
    int converted = 0;

    *decided_out = 0;

    /* 地域設定を返す GetUserDefaultLocaleName は、表示言語と別に設定できるため第一の候補としません。
       表示言語の優先順位は NUL 区切りの一覧で返り、先頭要素が最優先の表示言語となります。
       see: https://learn.microsoft.com/en-us/windows/win32/api/winnls/nf-winnls-getuserpreferreduilanguages */
    if ((GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &language_count, NULL, &buffer_length) != 0) &&
        (buffer_length > 0U))
    {
        wchar_t *const names = (wchar_t *)cplat_malloc((size_t)buffer_length * sizeof(wchar_t));

        if (names != NULL)
        {
            if ((GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &language_count, names, &buffer_length) != 0) &&
                (language_count > 0U))
            {
                /* 一覧の先頭要素は NUL 終端されているため、そのまま 1 つの文字列として扱えます。 */
                if (cplat_wstr_to_utf8(value, sizeof(value), names) > 0)
                {
                    converted = 1;
                }
                else
                {
                    converted = 0;
                }
            }
            cplat_free(names);
        }
    }

    if (converted == 0)
    {
        wchar_t name[LOCALE_NAME_MAX_LENGTH];

        if (GetUserDefaultLocaleName(name, (int)(sizeof(name) / sizeof(name[0]))) > 0)
        {
            if (cplat_wstr_to_utf8(value, sizeof(value), name) > 0)
            {
                converted = 1;
            }
            else
            {
                converted = 0;
            }
        }
    }

    if (converted == 0)
    {
        return CPLAT_OK;
    }

    ret = cplat_internal_ui_language_normalize(value, tag_out, tag_size);
    if (ret == CPLAT_OK)
    {
        *decided_out = 1;
        return CPLAT_OK;
    }
    if (ret == CPLAT_ERR_BUFFER_TOO_SMALL)
    {
        return ret;
    }

    /* 解釈できない名前は使用せず、ニュートラルとして扱う */
    return CPLAT_OK;
}

#endif /* PLATFORM_WINDOWS */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ui_language_get_tag(char *const tag_out, const size_t tag_size)
{
    int decided = 0;
    int ret;

    if ((tag_out == NULL) || (tag_size == 0U))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    tag_out[0] = '\0';

    ret = decide_from_environment(tag_out, tag_size, &decided);
    if (ret != CPLAT_OK)
    {
        tag_out[0] = '\0';
        return ret;
    }
    if (decided != 0)
    {
        return CPLAT_OK;
    }

#if defined(PLATFORM_WINDOWS)
    ret = decide_from_windows(tag_out, tag_size, &decided);
    if (ret != CPLAT_OK)
    {
        tag_out[0] = '\0';
        return ret;
    }
    if (decided != 0)
    {
        return CPLAT_OK;
    }
#endif /* PLATFORM_WINDOWS */

    /* Linux では、システムの設定がログイン時に環境変数へ反映されるため、環境変数以外の候補は評価しません。 */
    tag_out[0] = '\0';

    return CPLAT_OK;
}
