/**
 *******************************************************************************
 *  @file           argparser.h
 *  @brief          汎用コマンド ライン オプション パーサー API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/11
 *  @version        1.0.0
 *
 *  コマンド ライン引数 (argc / argv) を解析する汎用パーサーです。\n
 *  フラグ、値付きオプション、位置引数を事前に登録し、解析結果を
 *  登録時に指定した格納先へ書き込みます。
 *
 *  以下の構文をサポートします。
 *  - フラグ : `-v` / `--verbose` (出現回数を格納)
 *  - 値付きオプション : `-o {value}` / `-o={value}` / `--option {value}` / `--option={value}`
 *  - 位置引数 : 登録順に割り当て
 *  - 可変長位置引数 : 位置引数列の末尾で複数の値を配列へ格納
 *  - 負の位置整数 : 次の位置引数が整数型の場合、`-1` などを位置引数として割り当て
 *
 *  短オプションの連結 (`-abc`) と `--` 区切りはサポートしません。
 *
 *  ダブル クオーテーションの除去とエスケープの解釈は、シェル (POSIX shell) や
 *  C ランタイム (MSVC CRT) が argv を生成する時点で行われます。\n
 *  `--option="a b"` のようにワード中間にクオートがある場合も、シェルはクオートを
 *  除去して `--option=a b` を argv に渡すため、空白を含む値は全構文で同一に取得できます。\n
 *  本 API は argv の文字列を無加工で扱い、クオートの独自解釈は行いません。\n
 *  see: https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_02 \n
 *  see: https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
 *
 *  本 API はエラーを標準出力・標準エラーに出力しません。\n
 *  解析エラーの詳細は cplat_argparser_get_error() 系 API で取得し、
 *  表示は呼び出し側で行います。
 *
 *  @par 使用例
    @code{.c}
    #include <cplat/argparser/argparser.h>
    #include <stdio.h>
    #include <stdlib.h>

    int main(int argc, char *argv[])
    {
        cplat_console_init();
        cplat_argparser_init(argc, argv, "sample program");

        int need_help = 0;
        int count = 1; // 既定値は解析前に設定する
        const char *input = NULL;

        cplat_argparser_register_flag("-h", "--help", "ヘルプを表示する", &need_help);
        cplat_argparser_register_option_int("-c", "--count", "N", "繰り返し回数", 0, &count);
        cplat_argparser_register_positional_string("input", "入力ファイル",
                                                      CPLAT_ARGPARSER_REQUIRED, &input);

        if (cplat_argparser_get_register_error_count() > 0)
        { // オプションの登録に失敗した場合 (コーディング エラーの場合)
            cplat_argparser_print_register_error_messages(stderr);
            return EXIT_FAILURE;
        }

        int parse_result = cplat_argparser_parse();

        if (need_help != 0)
        {
            // 必須引数が省略されていても -h, --help を優先する
            cplat_argparser_print_usage(stdout);
            return EXIT_SUCCESS;
        }

        if (parse_result != CPLAT_OK)
        {
            cplat_argparser_print_error_messages(stderr);
            cplat_argparser_print_usage(stderr);
            return EXIT_FAILURE;
        }

        printf("count=%d input=%s\n", count, input);

        return EXIT_SUCCESS;
    }
    @endcode
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_ARGPARSER_H
#define CPLAT_ARGPARSER_H

#include <stddef.h>
#include <stdio.h>

#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_ARGPARSER
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**
 *  @brief  登録フラグ: そのオプション/位置引数を必須とします。
 */
#define CPLAT_ARGPARSER_REQUIRED (0x00000001u)

    /**
     *  @brief  引数パーサーを操作する不透明ハンドルです。
     */
    typedef struct cplat_argparser cplat_argparser;

    /**
     *  @brief  引数パーサーの生成オプションです。
     */
    typedef struct cplat_argparser_options
    {
        /**
         *  @brief  usage に表示するプログラム名です。
         *          NULL の場合は cplat_argparser_handle_create() 時に argv[0] のベース名で補完します。
         */
        const char *program_name;

        /**
         *  @brief  usage の冒頭に表示するプログラムの説明文です。NULL も指定できます。
         */
        const char *program_description;
    } cplat_argparser_options;

    /**
     *  @brief          引数パーサー ハンドルを生成します。
     *  @param[in]      argc     解析するコマンド ライン引数の個数です。1 以上を指定してください。
     *  @param[in]      argv     解析するコマンド ライン引数の配列です。NULL を渡してはなりません。\n
     *                           argv[0] はプログラム名として扱い、usage のプログラム名を求めます。
     *  @param[in]      options  生成オプションです。NULL の場合は既定設定を使用します。
     *  @return         成功時は生成したハンドルを返します。メモリを確保できない場合は NULL を返します。
     *
     *  @p argv は配列も要素の文字列も複製しません。ハンドルはポインターを保持するだけです。\n
     *  ハンドルを使用する間、および解析結果の文字列を参照する間は、
     *  @p argv とその要素文字列を有効なまま維持してください。
     *
     *  @p argc が 1 未満、または @p argv が NULL の場合も本関数は成功します。\n
     *  この場合は cplat_argparser_handle_parse() が @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  解析する引数を差し替える場合は、本関数でハンドルを生成し直してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_argparser *CPLAT_API cplat_argparser_handle_create(int argc, char *const *argv,
                                                                          const cplat_argparser_options *options);

    /**
     *  @brief          プロセス共有のパーサーを初期化します。
     *  @param[in]      argc         解析するコマンド ライン引数の個数です。1 以上を指定してください。
     *  @param[in]      argv         解析するコマンド ライン引数の配列です。NULL を渡してはなりません。\n
     *                               argv[0] はプログラム名として扱い、usage のプログラム名を求めます。
     *  @param[in]      description  プログラムの説明文です。NULL も指定できます。
     *
     *  `cplat_console_init()` にならい、通常のコマンドで使う 1 インスタンスのみの用途では
     *  ハンドルを一切意識せずに済むようにするための入口です。\n
     *  本関数を呼ばずに無修飾 API (`cplat_argparser_register_flag()` 等、parser 引数を持たない
     *  関数群) をいきなり呼び出しても、既定のオプションで暗黙に初期化されます。\n
     *  複数ハンドルを扱う必要がある場合は cplat_argparser_handle_create() を使用してください。
     *
     *  @p argv は配列も要素の文字列も複製しません。パーサーはポインターを保持するだけです。\n
     *  パーサーを使用する間、および解析結果の文字列を参照する間は、
     *  @p argv とその要素文字列を有効なまま維持してください。
     *
     *  @p argc が 1 未満、または @p argv が NULL の場合も本関数は初期化を行います。\n
     *  この場合は cplat_argparser_parse() が @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  パーサーはプロセス共有です。\n
     *  本関数を 2 回目以降に呼び出すと、それまでに登録したオプション、解析結果、
     *  エラー情報をすべて破棄し、@p argc 、@p argv 、@p description を再適用します。\n
     *  同一プロセスで `main` 相当の処理を繰り返し実行する場合 (テストなど) は、
     *  この再初期化により登録の重複を避けられます。\n
     *  解析する引数を差し替える場合も、本関数を呼び出し直してオプションを登録し直してください。
     *
     *  @warning        パーサーを共有する複数の登録元がある場合、
     *                  あとから本関数を呼び出した側が、先に登録された内容を破棄します。\n
     *                  本関数はオプション登録より前に、単一のスレッドから呼び出してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_argparser_init(int argc, char *const *argv, const char *description);

    /**
     *  @brief          引数パーサー ハンドルを解放します。
     *  @param[in]      parser  cplat_argparser_handle_create() が返したハンドルです。NULL も指定できます。
     *
     *  cplat_argparser_handle_create() で得たハンドルは、プロセス終了時の自動解放を行いません。
     *  生成したハンドルは必ず本関数で解放してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_argparser_handle_dispose(cplat_argparser *parser);

    /**
     *  @brief          フラグ (値なしオプション) を登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      short_name   短いオプション名 (`-x` 形式) です。@p long_name を指定する場合は NULL も指定できます。
     *  @param[in]      long_name    長いオプション名 (`--xxx` 形式) です。@p short_name を指定する場合は NULL も指定できます。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[out]     storage      出現回数の格納先です。NULL を渡してはなりません。\n
     *                               cplat_argparser_handle_parse() の開始時に 0 へ初期化し、出現ごとに 1 加算します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  フラグは同一コマンド ラインで複数回指定できます (例: `-v -v` で @p storage は 2)。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_flag(cplat_argparser *parser, const char *short_name,
                                                                    const char *long_name, const char *description,
                                                                    int *storage);

    /**
     *  @brief          フラグ (値なしオプション) を、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_flag
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_flag(const char *short_name, const char *long_name,
                                                             const char *description, int *storage);

    /**
     *  @brief          int 値を取るオプションを登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      short_name   短いオプション名 (`-x` 形式) です。@p long_name を指定する場合は NULL も指定できます。
     *  @param[in]      long_name    長いオプション名 (`--xxx` 形式) です。@p short_name を指定する場合は NULL も指定できます。
     *  @param[in]      value_name   usage に表示する値の名前です。NULL の場合は "VALUE" を使用します。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます。
     *  @param[out]     storage      解析した値の格納先です。NULL を渡してはなりません。\n
     *                               オプションが出現した場合のみ書き込みます。既定値は解析前に呼び出し側で設定してください。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  同一コマンド ラインで複数回指定された場合は解析エラー
     *  (@ref CPLAT_ERR_DUPLICATE_OPTION) になります。\n
     *  複数回の指定を許可する場合は cplat_argparser_handle_register_option_int_array() を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_option_int(cplat_argparser *parser,
                                                                          const char *short_name, const char *long_name,
                                                                          const char *value_name,
                                                                          const char *description, unsigned int flags,
                                                                          int *storage);

    /**
     *  @brief          int 値を取るオプションを、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_option_int
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_option_int(const char *short_name, const char *long_name,
                                                                   const char *value_name, const char *description,
                                                                   unsigned int flags, int *storage);

    /**
     *  @brief          文字列値を取るオプションを登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      short_name   短いオプション名 (`-x` 形式) です。@p long_name を指定する場合は NULL も指定できます。
     *  @param[in]      long_name    長いオプション名 (`--xxx` 形式) です。@p short_name を指定する場合は NULL も指定できます。
     *  @param[in]      value_name   usage に表示する値の名前です。NULL の場合は "VALUE" を使用します。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます。
     *  @param[out]     storage      解析した値の格納先です。NULL を渡してはなりません。\n
     *                               オプションが出現した場合のみ書き込みます。既定値は解析前に呼び出し側で設定してください。\n
     *                               格納する文字列は argv 内の文字列を指します (コピーしません)。
     *                               寿命は argv に従います。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  同一コマンド ラインで複数回指定された場合は解析エラー
     *  (@ref CPLAT_ERR_DUPLICATE_OPTION) になります。\n
     *  複数回の指定を許可する場合は cplat_argparser_handle_register_option_string_array() を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_option_string(
        cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
        const char *description, unsigned int flags, const char **storage);

    /**
     *  @brief          文字列値を取るオプションを、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_option_string
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_option_string(const char *short_name, const char *long_name,
                                                                      const char *value_name, const char *description,
                                                                      unsigned int flags, const char **storage);

    /**
     *  @brief          複数回指定できる int 値オプションを登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      short_name   短いオプション名 (`-x` 形式) です。@p long_name を指定する場合は NULL も指定できます。
     *  @param[in]      long_name    長いオプション名 (`--xxx` 形式) です。@p short_name を指定する場合は NULL も指定できます。
     *  @param[in]      value_name   usage に表示する値の名前です。NULL の場合は "VALUE" を使用します。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます
     *                               (1 回以上の出現を必須とします)。
     *  @param[out]     storage      解析した値を出現順に格納する配列です。NULL を渡してはなりません。
     *  @param[in]      capacity     @p storage の要素数です。1 以上を指定してください。\n
     *                               出現数が @p capacity を超えた場合は解析エラー
     *                               (@ref CPLAT_ERR_TOO_MANY_OCCURRENCES) になります。
     *  @param[out]     count        出現数の格納先です。NULL を渡してはなりません。\n
     *                               cplat_argparser_handle_parse() の開始時に 0 へ初期化します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_option_int_array(
        cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
        const char *description, unsigned int flags, int *storage, size_t capacity, size_t *count);

    /**
     *  @brief          複数回指定できる int 値オプションを、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_option_int_array
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_option_int_array(const char *short_name, const char *long_name,
                                                                         const char *value_name,
                                                                         const char *description, unsigned int flags,
                                                                         int *storage, size_t capacity, size_t *count);

    /**
     *  @brief          複数回指定できる文字列値オプションを登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      short_name   短いオプション名 (`-x` 形式) です。@p long_name を指定する場合は NULL も指定できます。
     *  @param[in]      long_name    長いオプション名 (`--xxx` 形式) です。@p short_name を指定する場合は NULL も指定できます。
     *  @param[in]      value_name   usage に表示する値の名前です。NULL の場合は "VALUE" を使用します。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます
     *                               (1 回以上の出現を必須とします)。
     *  @param[out]     storage      解析した値を出現順に格納する配列です。NULL を渡してはなりません。\n
     *                               格納する文字列は argv 内の文字列を指します (コピーしません)。
     *                               寿命は argv に従います。
     *  @param[in]      capacity     @p storage の要素数です。1 以上を指定してください。\n
     *                               出現数が @p capacity を超えた場合は解析エラー
     *                               (@ref CPLAT_ERR_TOO_MANY_OCCURRENCES) になります。
     *  @param[out]     count        出現数の格納先です。NULL を渡してはなりません。\n
     *                               cplat_argparser_handle_parse() の開始時に 0 へ初期化します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_option_string_array(
        cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
        const char *description, unsigned int flags, const char **storage, size_t capacity, size_t *count);

    /**
     *  @brief          複数回指定できる文字列値オプションを、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_option_string_array
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_option_string_array(
        const char *short_name, const char *long_name, const char *value_name, const char *description,
        unsigned int flags, const char **storage, size_t capacity, size_t *count);

    /**
     *  @brief          int 値の位置引数を登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      name         usage に表示する位置引数の名前です。NULL を渡してはなりません。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます。
     *  @param[out]     storage      解析した値の格納先です。NULL を渡してはなりません。\n
     *                               位置引数が出現した場合のみ書き込みます。
     *                               既定値は解析前に呼び出し側で設定してください。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  位置引数は登録順にコマンド ラインの非オプション トークンへ割り当てます。\n
     *  任意 (REQUIRED なし) の位置引数の後に必須の位置引数を登録した場合は
     *  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_positional_int(cplat_argparser *parser, const char *name,
                                                                              const char *description,
                                                                              unsigned int flags, int *storage);

    /**
     *  @brief          int 値の位置引数を、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_positional_int
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_positional_int(const char *name, const char *description,
                                                                       unsigned int flags, int *storage);

    /**
     *  @brief          文字列値の位置引数を登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      name         usage に表示する位置引数の名前です。NULL を渡してはなりません。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます。
     *  @param[out]     storage      解析した値の格納先です。NULL を渡してはなりません。\n
     *                               位置引数が出現した場合のみ書き込みます。
     *                               既定値は解析前に呼び出し側で設定してください。\n
     *                               格納する文字列は argv 内の文字列を指します (コピーしません)。
     *                               寿命は argv に従います。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  位置引数は登録順にコマンド ラインの非オプション トークンへ割り当てます。\n
     *  任意 (REQUIRED なし) の位置引数の後に必須の位置引数を登録した場合は
     *  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_positional_string(
        cplat_argparser *parser, const char *name, const char *description, unsigned int flags, const char **storage);

    /**
     *  @brief          文字列値の位置引数を、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_positional_string
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_positional_string(const char *name, const char *description,
                                                                          unsigned int flags, const char **storage);

    /**
     *  @brief          可変長の int 値位置引数を登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      name         usage に表示する位置引数の名前です。NULL を渡してはなりません。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます
     *                               (1 個以上の位置引数を必須とします)。
     *  @param[out]     storage      解析した値を出現順に格納する配列です。NULL を渡してはなりません。
     *  @param[in]      capacity     @p storage の要素数です。1 以上を指定してください。
     *                               出現数が @p capacity を超えた場合は解析エラー
     *                               (@ref CPLAT_ERR_TOO_MANY_ARGUMENTS) になります。
     *  @param[out]     count        出現数の格納先です。NULL を渡してはなりません。
     *                               cplat_argparser_handle_parse() の開始時に 0 へ初期化します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  可変長位置引数は 1 件だけ登録でき、位置引数列の末尾に配置する必要があります。
     *  本関数の後に別の位置引数を登録した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_positional_int_array(cplat_argparser *parser,
                                                                                    const char *name,
                                                                                    const char *description,
                                                                                    unsigned int flags, int *storage,
                                                                                    size_t capacity, size_t *count);

    /**
     *  @brief          可変長の int 値位置引数を、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_positional_int_array
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_positional_int_array(const char *name, const char *description,
                                                                             unsigned int flags, int *storage,
                                                                             size_t capacity, size_t *count);

    /**
     *  @brief          可変長の文字列値位置引数を登録します。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      name         usage に表示する位置引数の名前です。NULL を渡してはなりません。
     *  @param[in]      description  usage に表示する説明文です。NULL も指定できます。
     *  @param[in]      flags        登録フラグです。@ref CPLAT_ARGPARSER_REQUIRED を指定できます
     *                               (1 個以上の位置引数を必須とします)。
     *  @param[out]     storage      解析した値を出現順に格納する配列です。NULL を渡してはなりません。
     *                               格納する文字列は argv 内の文字列を指します (コピーしません)。
     *                               寿命は argv に従います。
     *  @param[in]      capacity     @p storage の要素数です。1 以上を指定してください。
     *                               出現数が @p capacity を超えた場合は解析エラー
     *                               (@ref CPLAT_ERR_TOO_MANY_ARGUMENTS) になります。
     *  @param[out]     count        出現数の格納先です。NULL を渡してはなりません。
     *                               cplat_argparser_handle_parse() の開始時に 0 へ初期化します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  可変長位置引数は 1 件だけ登録でき、位置引数列の末尾に配置する必要があります。
     *  本関数の後に別の位置引数を登録した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_register_positional_string_array(
        cplat_argparser *parser, const char *name, const char *description, unsigned int flags, const char **storage,
        size_t capacity, size_t *count);

    /**
     *  @brief          可変長の文字列値位置引数を、プロセス共有の既定パーサーへ登録します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  登録エラーは内部にも記録され、cplat_argparser_get_register_error_count() でも確認できます。
     *  @see            cplat_argparser_handle_register_positional_string_array
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_register_positional_string_array(const char *name,
                                                                                const char *description,
                                                                                unsigned int flags,
                                                                                const char **storage, size_t capacity,
                                                                                size_t *count);

    /**
     *  @brief          コマンド ラインを解析し、登録済みの格納先に結果を書き込みます。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN_OPTION 、
     *                  @ref CPLAT_ERR_MISSING_VALUE 、
     *                  @ref CPLAT_ERR_UNEXPECTED_VALUE 、
     *                  @ref CPLAT_ERR_INVALID_INTEGER 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、
     *                  @ref CPLAT_ERR_MISSING_REQUIRED 、
     *                  @ref CPLAT_ERR_DUPLICATE_OPTION 、
     *                  @ref CPLAT_ERR_TOO_MANY_ARGUMENTS 、
     *                  @ref CPLAT_ERR_TOO_MANY_OCCURRENCES 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。\n
     *                  解析エラーの場合は、検出した種別に対応する結果コードを返します。\n
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT は、@p parser が NULL の場合、
     *                  または cplat_argparser_handle_create() に渡した argc が 1 未満か
     *                  argv が NULL の場合に返します。
     *
     *  解析対象は cplat_argparser_handle_create() で受け取った argc と argv です。\n
     *  argv[0] はプログラム名として扱い、argv[1] 以降を解析します。\n
     *  解析する引数を差し替える場合は、ハンドルを生成し直してください。
     *
     *  解析エラーの対象名と位置は cplat_argparser_handle_get_error_target()、
     *  cplat_argparser_handle_get_error_index() で取得し、表示用のメッセージは
     *  cplat_argparser_handle_get_error_message() で組み立てられます。\n
     *  種別は戻り値を保持していない場合でも cplat_argparser_handle_get_error() で再取得できます。\n
     *  解析エラー時、エラー検出より前に処理した格納先には値が書き込まれています。
     *
     *  本関数は同一ハンドルで繰り返し呼び出せます (同じ argc と argv を解析し直します)。\n
     *  呼び出しの開始時に、フラグの格納先を 0 に、複数値オプションの出現数を 0 に初期化し、
     *  前回のエラー状態をクリアします。\n
     *  値付きオプションと位置引数の格納先は出現時のみ書き込むため、
     *  既定値は毎回の解析前に呼び出し側で設定してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_parse(cplat_argparser *parser);

    /**
     *  @brief          プロセス共有の既定パーサーでコマンド ラインを解析します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN_OPTION 、
     *                  @ref CPLAT_ERR_MISSING_VALUE 、
     *                  @ref CPLAT_ERR_UNEXPECTED_VALUE 、
     *                  @ref CPLAT_ERR_INVALID_INTEGER 、
     *                  @ref CPLAT_ERR_OUT_OF_RANGE 、
     *                  @ref CPLAT_ERR_MISSING_REQUIRED 、
     *                  @ref CPLAT_ERR_DUPLICATE_OPTION 、
     *                  @ref CPLAT_ERR_TOO_MANY_ARGUMENTS 、
     *                  @ref CPLAT_ERR_TOO_MANY_OCCURRENCES 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  解析対象は cplat_argparser_init() で受け取った argc と argv です。\n
     *  解析する引数を差し替える場合は、cplat_argparser_init() から呼び出し直してください。
     *
     *  @see            cplat_argparser_handle_parse
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_parse(void);

    /**
     *  @brief          直前の cplat_argparser_handle_parse() の解析エラー種別を取得します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL の場合は
     *                          @ref CPLAT_OK を返します。
     *  @return         解析エラー種別を返します。解析が成功した場合と未解析の場合は
     *                  @ref CPLAT_OK を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_get_error(const cplat_argparser *parser);

    /**
     *  @brief          プロセス共有の既定パーサーの、直前の解析エラー種別を取得します。
     *  @see            cplat_argparser_handle_get_error
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_get_error(void);

    /**
     *  @brief          直前の解析エラーの対象名を取得します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL の場合は NULL を返します。
     *  @return         エラーの対象を示す文字列 (オプション名、位置引数名、または該当トークン) を返します。\n
     *                  エラーがない場合と対象がない場合は NULL を返します。\n
     *                  返却する文字列はハンドルが所有します。次回の cplat_argparser_handle_parse() または
     *                  cplat_argparser_handle_dispose() まで有効です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_argparser_handle_get_error_target(const cplat_argparser *parser);

    /**
     *  @brief          プロセス共有の既定パーサーの、直前の解析エラーの対象名を取得します。
     *  @see            cplat_argparser_handle_get_error_target
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_argparser_get_error_target(void);

    /**
     *  @brief          直前の解析エラーが発生した argv のインデックスを取得します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL の場合は -1 を返します。
     *  @return         エラーを起こしたトークンの argv インデックスを返します。\n
     *                  エラーがない場合と、特定のトークンに対応しないエラー
     *                  (必須引数の欠落など) の場合は -1 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_get_error_index(const cplat_argparser *parser);

    /**
     *  @brief          プロセス共有の既定パーサーの、直前の解析エラーが発生した argv のインデックスを取得します。
     *  @see            cplat_argparser_handle_get_error_index
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_get_error_index(void);

    /**
     *  @brief          直前の解析エラーの内容を人間可読の 1 行メッセージとして組み立てます。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[out]     buffer       メッセージの格納先バッファーです。NULL を渡してはなりません。\n
     *                               常に NUL 終端します。
     *  @param[in]      buffer_size  @p buffer のバイト数です。1 以上を指定してください。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。\n
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL の場合、@p buffer には
     *                  切り詰めたメッセージを格納します。
     *
     *  本 API は組み立てた文字列を返すだけで、表示は行いません。表示は呼び出し側で行ってください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_get_error_message(const cplat_argparser *parser, char *buffer,
                                                                        size_t buffer_size);

    /**
     *  @brief          プロセス共有の既定パーサーの、直前の解析エラーのメッセージを組み立てます。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *  @see            cplat_argparser_handle_get_error_message
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_get_error_message(char *buffer, size_t buffer_size);

    /**
     *  @brief          登録内容から usage 文字列を組み立てます。
     *  @param[in]      parser         引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[out]     buffer         usage 文字列の格納先バッファーです。\n
     *                                 NULL の場合は @p required_size への必要サイズの出力のみを行います
     *                                 (このとき @p required_size に NULL を渡してはなりません)。\n
     *                                 NULL でない場合は常に NUL 終端します。
     *  @param[in]      buffer_size    @p buffer のバイト数です。@p buffer が NULL の場合は無視します。
     *  @param[out]     required_size  NUL 終端を含む必要バイト数の格納先です。不要な場合は NULL も指定できます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。\n
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL の場合、@p buffer には
     *                  切り詰めた usage を格納し、@p required_size (NULL でない場合) に必要サイズを出力します。
     *
     *  本 API は組み立てた文字列を返すだけで、表示は行いません。\n
     *  解析の成否とは独立に、登録完了後であればいつでも呼び出せます。
     *  解析前のヘルプ表示にも、解析後に呼び出し側で行うバリデーションのエラー報告にも使用できます。
     *
     *  プログラム名は生成オプションの program_name、未指定の場合は初期化時に
     *  argv[0] から求めたベース名、いずれも得られない場合は "{program}" を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_get_usage(const cplat_argparser *parser, char *buffer,
                                                                size_t buffer_size, size_t *required_size);

    /**
     *  @brief          プロセス共有の既定パーサーの登録内容から usage 文字列を組み立てます。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *  @see            cplat_argparser_handle_get_usage
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_get_usage(char *buffer, size_t buffer_size, size_t *required_size);

    /**
     *  @brief          登録内容から組み立てた usage を指定ストリームへ出力します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      stream  出力先ストリームです (stdout / stderr など)。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  内部で cplat_argparser_handle_get_usage() を用いて usage 文字列を組み立ててから
     *  @p stream へ書き出します。固定長バッファーによる切り詰めは発生しません。\n
     *  解析の成否とは独立に、登録完了後であればいつでも呼び出せます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_print_usage(const cplat_argparser *parser, FILE *stream);

    /**
     *  @brief          プロセス共有の既定パーサーの usage を指定ストリームへ出力します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *  @see            cplat_argparser_handle_print_usage
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_print_usage(FILE *stream);

    /**
     *  @brief          直前の解析エラーのメッセージを指定ストリームへ出力します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      stream  出力先ストリームです (stdout / stderr など)。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。\n
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL の場合、切り詰めたメッセージを出力した上で返します。
     *
     *  内部で cplat_argparser_handle_get_error_message() を用いてエラー メッセージを組み立ててから、
     *  "error: {メッセージ}\n" の形式で @p stream へ書き出し、続けて区切りの空行を出力します。\n
     *  エラーがない場合や対象がない場合は何も出力しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_print_error_messages(const cplat_argparser *parser, FILE *stream);

    /**
     *  @brief          プロセス共有の既定パーサーの、直前の解析エラーのメッセージを出力します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *  @see            cplat_argparser_handle_print_error_messages
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_print_error_messages(FILE *stream);

    /**
     *  @brief          register 系呼び出しで発生したエラーの件数を取得します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL の場合は 0 を返します。
     *  @return         これまでに対象の @p parser へ行った register 系呼び出しのうち、
     *                  @ref CPLAT_OK 以外を返した回数を返します。
     *
     *  明示 API の各 cplat_argparser_handle_register_*() は個別に結果コードを返します。
     *  呼び出し側は戻り値を都度確認せずに
     *  すべての登録を終えた後、本関数でまとめて成否を判定できます。\n
     *  0 より大きい場合は cplat_argparser_handle_get_register_error() 系 API で詳細を取得できます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_argparser_handle_get_register_error_count(const cplat_argparser *parser);

    /**
     *  @brief          プロセス共有の既定パーサーの、register 系呼び出しで発生したエラーの件数を取得します。
     *  @see            cplat_argparser_handle_get_register_error_count
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_argparser_get_register_error_count(void);

    /**
     *  @brief          register 系呼び出しで発生した @p index 件目のエラーの結果コードを取得します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL の場合は @ref CPLAT_OK を返します。
     *  @param[in]      index   取得するエラーの番号 (0 起点、発生順)。
     *  @return         @p index 件目のエラーの結果コードを返します。\n
     *                  @p index が cplat_argparser_handle_get_register_error_count() 以上の場合は
     *                  @ref CPLAT_OK を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_get_register_error(const cplat_argparser *parser, size_t index);

    /**
     *  @brief          プロセス共有の既定パーサーの、register 系エラーの結果コードを取得します。
     *  @see            cplat_argparser_handle_get_register_error
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_get_register_error(size_t index);

    /**
     *  @brief          register 系呼び出しで発生した @p index 件目のエラーの対象名を取得します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL の場合は NULL を返します。
     *  @param[in]      index   取得するエラーの番号 (0 起点、発生順)。
     *  @return         エラーの対象を示す文字列 (オプション名または位置引数名) を返します。\n
     *                  @p index が範囲外の場合と対象がない場合は NULL を返します。\n
     *                  返却する文字列はハンドルが所有します。cplat_argparser_handle_dispose() まで有効です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_argparser_handle_get_register_error_target(const cplat_argparser *parser,
                                                                                        size_t index);

    /**
     *  @brief          プロセス共有の既定パーサーの、register 系エラーの対象名を取得します。
     *  @see            cplat_argparser_handle_get_register_error_target
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_argparser_get_register_error_target(size_t index);

    /**
     *  @brief          register 系呼び出しで発生した @p index 件目のエラーを人間可読の 1 行メッセージとして組み立てます。
     *  @param[in]      parser       引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      index       取得するエラーの番号 (0 起点、発生順)。
     *                              cplat_argparser_handle_get_register_error_count() 以上を指定してはなりません。
     *  @param[out]     buffer       メッセージの格納先バッファーです。NULL を渡してはなりません。\n
     *                               常に NUL 終端します。
     *  @param[in]      buffer_size  @p buffer のバイト数です。1 以上を指定してください。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。\n
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL の場合、@p buffer には
     *                  切り詰めたメッセージを格納します。
     *
     *  本 API は組み立てた文字列を返すだけで、表示は行いません。表示は呼び出し側で行ってください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_get_register_error_message(const cplat_argparser *parser,
                                                                                 size_t index, char *buffer,
                                                                                 size_t buffer_size);

    /**
     *  @brief          プロセス共有の既定パーサーの、register 系エラーのメッセージを組み立てます。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *  @see            cplat_argparser_handle_get_register_error_message
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_get_register_error_message(size_t index, char *buffer,
                                                                          size_t buffer_size);

    /**
     *  @brief          register 系呼び出しで発生したエラーのメッセージを指定ストリームへ全件出力します。
     *  @param[in]      parser  引数パーサー ハンドルです。NULL を渡してはなりません。
     *  @param[in]      stream  出力先ストリームです (stdout / stderr など)。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *
     *  内部で cplat_argparser_handle_get_register_error_message() を用いてエラー メッセージを組み立ててから、
     *  発生順にすべて "error: {メッセージ}\n" の形式で @p stream へ書き出し、
     *  最後に区切りの空行を出力します。\n
     *  エラーがない場合は何も出力しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p parser ハンドルに対する操作は同時に実行できます。\n
     *  同一 @p parser に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_handle_print_register_error_messages(const cplat_argparser *parser,
                                                                                    FILE *stream);

    /**
     *  @brief          プロセス共有の既定パーサーの、register 系エラーのメッセージを全件出力します。
     *
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL のいずれかを返します。
     *  @see            cplat_argparser_handle_print_register_error_messages
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス共有のパーサーに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_argparser_print_register_error_messages(FILE *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_ARGPARSER_H */
