/**
 *******************************************************************************
 *  @file           string_catalog.h
 *  @brief          文字列カタログの公開 API を宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  文字列キーを指定して、UTF-8 の文字列を組み立てます。\n
 *  文字列キーは処理から項目を参照する識別子で、項目の `key` に格納します。\n
 *  項目の `id` は処理では意味を持たない補足の文字列で、ライブラリは解釈しません。\n
 *  引数の型と文字列表現は文字列キー側の定義が決め、言語別リソースは語順だけを決めます。
 *
 *  カタログはライブラリ側では保持しません。\n
 *  利用側で @ref cplat_string_catalog へ配列とインデックス表をまとめ、API の呼び出しごとに渡します。\n
 *  1 つのプロセスで複数のカタログを扱えます。カタログ同士は互いに独立しています。\n
 *  カタログ指定を省略する簡易関数は、利用側の生成物で提供します。
 *
 *  出力する言語はプロセスで 1 つとし、@ref cplat_string_catalog_set_language で設定します。\n
 *  設定していないプロセスは、実行環境が示す表示言語に対応する言語を使用します。
 *
 *  書式中の位置指定は `{0}` から `{49}` までです。書式指定は書けません。\n
 *  `{` と `}` そのものを出力する場合は `{{` と `}}` を使用します。\n
 *  `{0}` を複数回参照することも、`{1}` を `{0}` より前に置くこともできます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_STRING_CATALOG_STRING_CATALOG_H
#define CPLAT_STRING_CATALOG_STRING_CATALOG_H

#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <cplat/string_catalog/argument.h>
#include <cplat/string_catalog/catalog.h>
#include <cplat/string_catalog/language.h>
#include <stdarg.h>
#include <stddef.h>

/**
 *  @ingroup        CPLAT_STRING_CATALOG
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          プロセスが文字列を出力する言語を設定します。
     *  @param[in]      language 設定する言語。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p language が範囲外の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返し、
     *                  設定を変更しません。
     *
     *  設定はプロセス全体で 1 つです。文字列を組み立てるたびに言語を指定する必要はありません。\n
     *  本関数を呼び出していないプロセスは、実行環境が示す表示言語に対応する言語を使用します。\n
     *  本関数による設定は実行環境よりも優先し、@ref CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL の
     *  明示的な設定も実行環境の表示言語で上書きしません。
     *
     *  @ref CPLAT_STRING_CATALOG_LANGUAGE_COUNT は言語ではないため、指定できません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセスの初期化時に設定し、文字列を組み立てている間は変更しないでください。\n
     *  設定を変更しない限り、文字列の組み立ては複数のスレッドから同時に行えます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_set_language(cplat_string_catalog_language language);

    /**
     *  @brief          プロセスが文字列を出力する言語を返します。
     *  @return         現在の言語を返します。
     *
     *  @ref cplat_string_catalog_set_language を呼び出していない場合は、最初の呼び出しの時点で
     *  @ref cplat_ui_language_get_tag が返す言語タグに対応する言語を決定し、以後は同じ言語を返します。\n
     *  対応する言語が存在しない場合と、表示言語がニュートラルの場合は
     *  @ref CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\n
     *  cplat_string_catalog_set_language() で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT cplat_string_catalog_language CPLAT_API cplat_string_catalog_get_language(void);

    /**
     *  @brief          言語タグに対応する、文字列を出力する言語を返します。
     *  @param[in]      tag           言語タグ (null 終端文字列)。NULL を渡してはなりません。\n
     *                                空文字列はニュートラル言語の指定として扱います。
     *  @param[out]     language_out  対応する言語の格納先。NULL を渡してはなりません。
     *  @retval         CPLAT_OK                    対応する言語を格納しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  @p tag または @p language_out が NULL です。
     *  @retval         CPLAT_ERR_NOT_FOUND         対応する言語が存在しないため、
     *                                              @ref CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL を格納しました。
     *
     *  対応付けには言語タグの言語だけを使用し、表記体系と地域は使用しません。\n
     *  @ref cplat_ui_language_get_tag が返す言語タグを、出力言語の設定へ渡す用途を想定しています。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_language_from_tag(const char *tag,
                                                                      cplat_string_catalog_language *language_out);

    /**
     *  @brief          文字列キーと可変長引数から、現在の言語の文字列を組み立てます。
     *  @param[in]      catalog    使用するカタログ。NULL を渡してはなりません。
     *  @param[out]     dest       文字列の格納先。NULL を渡してはなりません。常に NUL 終端します。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上を指定してください。
     *  @param[in]      string_key 組み立てる文字列のキー。利用者の列挙の値を指定します。
     *  @param[in]      ...        文字列キーの引数スキーマが定める順序と型の値。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p catalog が NULL の場合、@p catalog の内容が不正な場合、
     *                  @p dest が NULL の場合、または @p dest_size が 0 の場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @p string_key がカタログに存在しない場合は @ref CPLAT_ERR_NOT_FOUND を返します。
     *  @return         カタログの書式が不正な場合は @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
     *  @return         結果が @p dest に収まらない場合は、切り詰めたうえで
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *
     *  出力する言語は @ref cplat_string_catalog_get_language が返す現在の言語です。
     *
     *  可変長引数は、文字列キーの引数スキーマが定める順序でそのまま並べます。\n
     *  書式中で `{1}` が `{0}` より前に現れる言語であっても、呼び出し側の引数順序は変わりません。
     *
     *  各引数へ渡す型は @ref cplat_string_catalog_argument_kind の表に従ってください。\n
     *  スキーマと異なる型を渡した場合の動作は未定義です。
     *
     *  @par            使用例
        @code{.c}
        static const cplat_string_catalog catalog = {entries, key_index, entry_count, key_index_count};
        char text[CPLAT_STRING_CATALOG_TEXT_MAX];
        cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE);
        int ret = cplat_string_catalog_format(&catalog, text, sizeof(text), SAMPLE_MESSAGES_KEY_FILE_OPEN_FAILED,
                                         "config.json", 2);
        if (ret == CPLAT_OK)
        {
            puts(text);  // ファイル config.json を開けませんでした。エラー コード=2 (0x00000002)
        }
        @endcode
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\n
     *  cplat_string_catalog_set_language() で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_format(const cplat_string_catalog *catalog, char *dest,
                                                           size_t dest_size, int string_key, ...);

    /**
     *  @brief          文字列キーと @c va_list から、現在の言語の文字列を組み立てます。
     *  @param[in]      catalog    使用するカタログ。NULL を渡してはなりません。
     *  @param[out]     dest       文字列の格納先。NULL を渡してはなりません。常に NUL 終端します。
     *  @param[in]      dest_size  @p dest のバイト数。1 以上を指定してください。
     *  @param[in]      string_key 組み立てる文字列のキー。利用者の列挙の値を指定します。
     *  @param[in]      args       文字列キーの引数スキーマが定める順序と型の値を保持する引数リスト。
     *  @return         戻り値は @ref cplat_string_catalog_format と同じです。
     *
     *  本関数は @p args を最初に一度だけ走査し、引数スキーマに従って値の配列へ取り出します。\n
     *  そのあとで書式を展開するため、位置指定の並べ替えと繰り返し参照を行えます。
     *
     *  呼び出し後の @p args は、@c va_arg で読み進めた状態になります。\n
     *  呼び出し側は @c va_end を実行してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\n
     *  cplat_string_catalog_set_language() で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_vformat(const cplat_string_catalog *catalog, char *dest,
                                                            size_t dest_size, int string_key, va_list args);

    /**
     *  @brief          カタログのすべての定義が、必要なメタデータと引数スキーマに適合することを確認します。
     *  @param[in]      catalog        確認するカタログ。NULL を渡してはなりません。
     *  @param[out]     string_key_out 不正を検出した文字列のキー。不要な場合は NULL を指定できます。
     *  @param[out]     language_out   不正を検出した言語。不要な場合は NULL を指定できます。
     *  @return         すべての書式が正しい場合は @ref CPLAT_OK を返します。
     *  @return         @p catalog が NULL の場合、@ref cplat_string_catalog::entries が NULL の場合、
     *                  または要素数が負の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         書式の構文が不正な場合、位置指定が引数個数を超える場合、
     *                  引数個数が @ref CPLAT_STRING_CATALOG_ARGUMENT_MAX を超える場合、
     *                  引数の種別、名前、説明、短い説明のいずれかが未定義の場合、
     *                  または文字列キーが重複している場合、
     *                  インデックス表から文字列へ到達できない場合、
     *                  またはニュートラル言語の書式や備考が未定義の場合は
     *                  @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
     *
     *  現在の言語だけでなく、すべての言語のリソースを確認します。\n
     *  ニュートラル言語以外のリソースは、未定義であればニュートラル言語を代替として使用するため、
     *  未定義であること自体は不正ではありません。
     *
     *  出力引数の値は、戻り値が @ref CPLAT_ERR_MALFORMED_DEFINITION の場合だけ有効です。\n
     *  最初に検出した 1 件を報告し、その時点で走査を打ち切ります。\n
     *  引数個数やインデックス表の不正は言語に依存しないため、@p language_out には言語ではない
     *  @ref CPLAT_STRING_CATALOG_LANGUAGE_COUNT を格納します。
     *
     *  カタログは生成物であるため、通常はビルド時または起動時に 1 度検証すれば十分です。\n
     *  文字列を組み立てるたびに実行する必要はありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_verify(const cplat_string_catalog *catalog, int *string_key_out,
                                                           cplat_string_catalog_language *language_out);

    /**
     *  @brief          文字列キーに対応するカタログ項目を返します。
     *  @param[in]      catalog    参照するカタログ。NULL を渡した場合は NULL を返します。
     *  @param[in]      string_key 参照する文字列のキー。利用者の列挙の値を指定します。
     *  @return         カタログ項目への読み取り専用ポインターを返します。
     *  @return         カタログに存在しない文字列キーでは NULL を返します。
     *
     *  返すポインターは、呼び出し側が用意したカタログ配列の要素を指します。\n
     *  カタログと配列が有効な間だけ参照でき、呼び出し側で解放してはなりません。
     *  項目には文字列キー、補足の ID、引数の種別・名前・説明、分類値、短い説明、詳細説明、補足説明、書式、備考が含まれます。
     *  ID、詳細説明、補足説明は、定義で省略されている場合に NULL です。
     *  項目の識別には @ref cplat_string_catalog_entry::key を使用します。
     *  @ref cplat_string_catalog_entry::id は処理では意味を持たない補足の文字列です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT const cplat_string_catalog_entry *CPLAT_API
    cplat_string_catalog_get_entry(const cplat_string_catalog *catalog, int string_key);

    /**
     *  @brief          文字列の分類値を返します。
     *  @param[in]      catalog    参照するカタログ。NULL を渡した場合は 0 を返します。
     *  @param[in]      string_key 参照する文字列のキー。利用者の列挙の値を指定します。
     *  @return         カタログが保持する分類値を返します。
     *  @return         カタログに存在しない文字列キーでは 0 を返します。
     *
     *  分類値は、ライブラリが解釈しない補足情報です。\n
     *  重大度、用途、出力先など、値の意味と有効な範囲は利用者が決めます。\n
     *  ライブラリは値を検査せず、保持して返すだけです。
     *
     *  0 は分類なしを表します。\n
     *  利用者が 0 を意味のある分類値として登録することもできますが、
     *  その場合はカタログに存在しない文字列キーと区別できません。\n
     *  区別が必要な場合は、先に @ref cplat_string_catalog_get_entry で存在を確認してください。
     *
     *  分類値は言語に依存せず、文字列キーごとに固定です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_string_catalog_get_category(const cplat_string_catalog *catalog, int string_key);

    /**
     *  @brief          文字列定義の ID を返します。
     *  @param[in]      catalog    参照するカタログ。NULL を渡した場合は NULL を返します。
     *  @param[in]      string_key 参照する文字列のキー。利用者の列挙の値を指定します。
     *  @return         ID (例: `SAMPLE_MESSAGES_ID_0001`) を返します。
     *  @return         ID が未設定の項目、またはカタログに存在しない文字列キーでは NULL を返します。
     *
     *  ID は、処理では意味を持たない補足の文字列です。\n
     *  ライブラリは値を解釈せず、未設定や重複も検査しません。保持して返すだけです。\n
     *  返す文字列は言語に依存せず、カタログの生成物が保持する静的領域を指します。\n
     *  呼び出し側で解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_string_catalog_get_id(const cplat_string_catalog *catalog, int string_key);

    /**
     *  @brief          現在の言語で文字列の備考を返します。
     *  @param[in]      catalog    参照するカタログ。NULL を渡した場合は NULL を返します。
     *  @param[in]      string_key 参照する文字列のキー。利用者の列挙の値を指定します。
     *  @return         備考を返します。備考が未設定の文字列では空文字列を返します。
     *  @return         カタログに存在しない文字列キーでは NULL を返します。
     *
     *  備考は、引数の単位や出力条件など、書式には含めない補足です。\n
     *  現在の言語の備考が未設定の場合は、ニュートラル言語の備考を返します。\n
     *  返す文字列は、カタログの生成物が保持する静的領域を指します。\n
     *  呼び出し側で解放してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\n
     *  cplat_string_catalog_set_language() で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_string_catalog_get_note(const cplat_string_catalog *catalog,
                                                                     int string_key);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_STRING_CATALOG_STRING_CATALOG_H */
