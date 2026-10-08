/**
 *******************************************************************************
 *  @file           argparser.c
 *  @brief          汎用コマンド ライン オプション パーサーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/11
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/argparser/argparser.h>

#include <cplat/base/platform.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/sync/sync.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 登録配列の初期容量 */
#define ARGPARSER_INITIAL_CAPACITY (8)

/* usage で説明文の表示を開始する列 (行頭から) */
#define ARGPARSER_USAGE_DESC_COLUMN (28)

/* print_error_messages のエラー メッセージ組み立て用スタック バッファー サイズ */
#define ARGPARSER_ERROR_MESSAGE_BYTES (512)

/* value_name 未指定時の既定表示名 */
static const char s_default_value_name[] = "VALUE";

/* program_name 未解決時の既定表示名 */
static const char s_default_program_name[] = "{program}";

/* 登録項目の種別 */
typedef enum
{
    ARGPARSER_SPEC_FLAG = 0,
    ARGPARSER_SPEC_OPTION_INT = 1,
    ARGPARSER_SPEC_OPTION_STRING = 2,
    ARGPARSER_SPEC_OPTION_INT_ARRAY = 3,
    ARGPARSER_SPEC_OPTION_STRING_ARRAY = 4,
    ARGPARSER_SPEC_POSITIONAL_INT = 5,
    ARGPARSER_SPEC_POSITIONAL_STRING = 6,
    ARGPARSER_SPEC_POSITIONAL_INT_ARRAY = 7,
    ARGPARSER_SPEC_POSITIONAL_STRING_ARRAY = 8
} argparser_spec_kind;

/* 登録項目 */
typedef struct argparser_spec
{
    argparser_spec_kind kind;
    unsigned int flags;
    char *short_name;  /* "-x" 形式。位置引数では NULL */
    char *long_name;   /* "--xxx" 形式。位置引数では名前を保持する */
    char *value_name;  /* 値付きオプションの値表示名。他は NULL */
    char *description; /* usage 用説明文。NULL 可 */
    int *int_storage;
    const char **string_storage;
    size_t capacity; /* 配列オプションの要素数。他は 0 */
    size_t *count;   /* 配列オプションの出現数出力先。他は NULL */
    size_t found;    /* 解析中の出現回数 */
} argparser_spec;

/* register 系呼び出しで発生した 1 件のエラー */
typedef struct argparser_register_error
{
    int result;       /* 発生した結果コード (OK 以外) */
    unsigned int pad; /* 明示的アラインメント */
    char *target;     /* 対象名 (複製)。対象がない場合は NULL */
} argparser_register_error;

struct cplat_argparser
{
    argparser_spec *specs;
    size_t spec_count;
    size_t spec_capacity;
    char *program_name;          /* 生成オプション指定のプログラム名。NULL 可 */
    char *program_description;   /* 生成オプション指定の説明文。NULL 可 */
    char *resolved_program_name; /* 初期化時に argv[0] から求めたベース名。NULL 可 */
    char *last_error_target;
    char *const *argv; /* 初期化時に受け取ったコマンド ライン引数。複製しない。NULL 可 */
    int argc;          /* argv の要素数。未設定は 0 */
    int last_error;
    int last_error_index;
    int library_owned;
    argparser_register_error *register_errors; /* register 系エラーを発生順に積み上げる配列。NULL 可 */
    size_t register_error_count;
    size_t register_error_capacity;
};

/* ================================================================
 * 内部ヘルパー
 * ================================================================ */

/**
 *  @brief          文字列を複製します。
 *  @param[in]      text  複製する文字列。NULL の場合は NULL を返します。
 *  @return         複製した文字列。メモリを確保できない場合は NULL を返します。
 *
 *  strdup は Windows (MSVC) では _strdup となるため、malloc ベースで自前実装します。
 */
static char *argparser_strdup(const char *text)
{
    if (text == NULL)
    {
        return NULL;
    }

    size_t size = strlen(text) + 1;
    char *copied = cplat_malloc(size);
    if (copied == NULL)
    {
        return NULL;
    }
    memcpy(copied, text, size);

    return copied;
}

/**
 *  @brief          短いオプション名 ("-x" 形式) かどうかを検証します。
 *  @param[in]      name  検証する名前。
 *  @return         有効な場合は 1、無効な場合は 0 を返します。
 */
static int argparser_is_valid_short_name(const char *name)
{
    if (name == NULL)
    {
        return 0;
    }
    if (strlen(name) != 2)
    {
        return 0;
    }
    if (name[0] != '-')
    {
        return 0;
    }
    if (name[1] == '-')
    {
        return 0;
    }

    return 1;
}

/**
 *  @brief          長いオプション名 ("--xxx" 形式) かどうかを検証します。
 *  @param[in]      name  検証する名前。
 *  @return         有効な場合は 1、無効な場合は 0 を返します。
 */
static int argparser_is_valid_long_name(const char *name)
{
    if (name == NULL)
    {
        return 0;
    }
    if (strlen(name) < 3)
    {
        return 0;
    }
    if (strncmp(name, "--", 2) != 0)
    {
        return 0;
    }
    if (strchr(name, '=') != NULL)
    {
        return 0;
    }

    return 1;
}

/**
 *  @brief          登録項目が位置引数かどうかを判定します。
 *  @param[in]      spec  判定する登録項目。
 *  @return         位置引数の場合は 1、それ以外の場合は 0 を返します。
 */
static int argparser_spec_is_positional(const argparser_spec *spec)
{
    if (spec->kind == ARGPARSER_SPEC_POSITIONAL_INT || spec->kind == ARGPARSER_SPEC_POSITIONAL_STRING ||
        spec->kind == ARGPARSER_SPEC_POSITIONAL_INT_ARRAY || spec->kind == ARGPARSER_SPEC_POSITIONAL_STRING_ARRAY)
    {
        return 1;
    }

    return 0;
}

/**
 *  @brief          登録項目が可変長位置引数かどうかを判定します。
 *  @param[in]      spec  判定する登録項目。
 *  @return         可変長位置引数の場合は 1、それ以外の場合は 0 を返します。
 */
static int argparser_spec_is_positional_array(const argparser_spec *spec)
{
    if (spec->kind == ARGPARSER_SPEC_POSITIONAL_INT_ARRAY || spec->kind == ARGPARSER_SPEC_POSITIONAL_STRING_ARRAY)
    {
        return 1;
    }

    return 0;
}

/**
 *  @brief          登録項目が値付きオプションかどうかを判定します。
 *  @param[in]      spec  判定する登録項目。
 *  @return         値付きオプションの場合は 1、それ以外の場合は 0 を返します。
 */
static int argparser_spec_takes_value(const argparser_spec *spec)
{
    if (spec->kind == ARGPARSER_SPEC_OPTION_INT || spec->kind == ARGPARSER_SPEC_OPTION_STRING ||
        spec->kind == ARGPARSER_SPEC_OPTION_INT_ARRAY || spec->kind == ARGPARSER_SPEC_OPTION_STRING_ARRAY)
    {
        return 1;
    }

    return 0;
}

/**
 *  @brief          登録項目の代表名 (long 優先) を取得します。
 *  @param[in]      spec  対象の登録項目。
 *  @return         代表名を返します。
 */
static const char *argparser_spec_display_name(const argparser_spec *spec)
{
    if (spec->long_name != NULL)
    {
        return spec->long_name;
    }

    return spec->short_name;
}

/**
 *  @brief          登録項目が保持する文字列を解放します。
 *  @param[in,out]  spec  解放する登録項目。
 */
static void argparser_free_spec(argparser_spec *spec)
{
    cplat_free(spec->short_name);
    cplat_free(spec->long_name);
    cplat_free(spec->value_name);
    cplat_free(spec->description);
    spec->short_name = NULL;
    spec->long_name = NULL;
    spec->value_name = NULL;
    spec->description = NULL;
}

/**
 *  @brief          エラー状態をクリアします。
 *  @param[in,out]  parser  対象のハンドル。
 */
static void argparser_clear_error(cplat_argparser *parser)
{
    parser->last_error = CPLAT_OK;
    cplat_free(parser->last_error_target);
    parser->last_error_target = NULL;
    parser->last_error_index = -1;
}

/**
 *  @brief          解析エラーの詳細をハンドルに記録します。
 *  @param[in,out]  parser  対象のハンドル。
 *  @param[in]      error   エラー種別。
 *  @param[in]      target  エラー対象名。NULL 可。複製して保持します。
 *  @param[in]      index   エラーを起こした argv インデックス。該当なしは -1。
 *  @return         @p error をそのまま返します。呼び出し元は解析関数の戻り値として伝播します。
 */
static int argparser_set_error(cplat_argparser *parser, int error, const char *target, const int index)
{
    argparser_clear_error(parser);
    parser->last_error = error;
    /* 複製に失敗した場合は対象名なし (NULL) として記録を継続する */
    parser->last_error_target = argparser_strdup(target);
    parser->last_error_index = index;

    return error;
}

/**
 *  @brief          register 系呼び出しのエラーをハンドルへ積み上げます。
 *  @param[in,out]  parser              対象のハンドル。NULL の場合は何もしません。
 *  @param[in]      result              register 呼び出しの結果。@ref CPLAT_OK の場合は何もしません。
 *  @param[in]      short_name_or_name  短いオプション名、または位置引数名。NULL 可。
 *  @param[in]      long_name           長いオプション名。位置引数では使用しません。NULL 可。
 *
 *  失敗するたびに 1 件ずつ配列へ追記します。記録用メモリを確保できない場合は記録を諦めます
 *  (呼び出し元へは result をそのまま返しているため、呼び出し元の動作に影響しません)。
 */
static void argparser_record_register_result(cplat_argparser *parser, int result, const char *short_name_or_name,
                                             const char *long_name)
{
    if (parser == NULL || result == CPLAT_OK)
    {
        return;
    }

    if (parser->register_error_count == parser->register_error_capacity)
    {
        size_t new_capacity = parser->register_error_capacity * 2;
        if (new_capacity == 0)
        {
            new_capacity = ARGPARSER_INITIAL_CAPACITY;
        }
        argparser_register_error *new_errors =
            cplat_realloc(parser->register_errors, new_capacity, sizeof(argparser_register_error));
        if (new_errors == NULL)
        {
            return;
        }
        parser->register_errors = new_errors;
        parser->register_error_capacity = new_capacity;
    }

    argparser_register_error *entry = &parser->register_errors[parser->register_error_count];
    entry->result = result;
    if (long_name != NULL)
    {
        entry->target = argparser_strdup(long_name);
    }
    else
    {
        entry->target = argparser_strdup(short_name_or_name);
    }
    parser->register_error_count++;
}

/**
 *  @brief          登録名の重複を検査し、登録配列に空きを確保します。
 *  @param[in,out]  parser      対象のハンドル。
 *  @param[in]      short_name  検査する短い名前。NULL 可。
 *  @param[in]      long_name   検査する長い名前。NULL 可。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_DUPLICATE_DEFINITION 、
 *                  @ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
 */
static int argparser_prepare_spec_slot(cplat_argparser *parser, const char *short_name, const char *long_name)
{
    for (size_t i = 0; i < parser->spec_count; i++)
    {
        const argparser_spec *spec = &parser->specs[i];
        if (short_name != NULL && spec->short_name != NULL && strcmp(spec->short_name, short_name) == 0)
        {
            return CPLAT_ERR_DUPLICATE_DEFINITION;
        }
        if (long_name != NULL && spec->long_name != NULL && strcmp(spec->long_name, long_name) == 0)
        {
            return CPLAT_ERR_DUPLICATE_DEFINITION;
        }
    }

    if (parser->spec_count == parser->spec_capacity)
    {
        size_t new_capacity = parser->spec_capacity * 2;
        if (new_capacity == 0)
        {
            new_capacity = ARGPARSER_INITIAL_CAPACITY;
        }
        argparser_spec *new_specs = cplat_realloc(parser->specs, new_capacity, sizeof(argparser_spec));
        if (new_specs == NULL)
        {
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        parser->specs = new_specs;
        parser->spec_capacity = new_capacity;
    }

    return CPLAT_OK;
}

/**
 *  @brief          登録項目の名前・説明文を複製して格納します。
 *  @param[in,out]  spec         格納先の登録項目。
 *  @param[in]      short_name   短い名前。NULL 可。
 *  @param[in]      long_name    長い名前 (位置引数では名前)。NULL 可。
 *  @param[in]      value_name   値表示名。NULL 可。
 *  @param[in]      description  説明文。NULL 可。
 *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_OUT_OF_MEMORY を返します。
 *
 *  失敗した場合は複製済みの文字列を解放してから返します。
 */
static int argparser_copy_spec_strings(argparser_spec *spec, const char *short_name, const char *long_name,
                                       const char *value_name, const char *description)
{
    int failed = 0;

    spec->short_name = argparser_strdup(short_name);
    if (short_name != NULL && spec->short_name == NULL)
    {
        failed = 1;
    }
    spec->long_name = argparser_strdup(long_name);
    if (long_name != NULL && spec->long_name == NULL)
    {
        failed = 1;
    }
    spec->value_name = argparser_strdup(value_name);
    if (value_name != NULL && spec->value_name == NULL)
    {
        failed = 1;
    }
    spec->description = argparser_strdup(description);
    if (description != NULL && spec->description == NULL)
    {
        failed = 1;
    }

    if (failed != 0)
    {
        argparser_free_spec(spec);
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    return CPLAT_OK;
}

/**
 *  @brief          オプション (フラグ・値付き) の共通登録処理です。
 *  @param[in,out]  parser       対象のハンドル。
 *  @param[in]      kind         登録項目の種別。
 *  @param[in]      short_name   短いオプション名。NULL 可。
 *  @param[in]      long_name    長いオプション名。NULL 可。
 *  @param[in]      value_name   値表示名。値付きオプションで NULL の場合は既定名を使用します。
 *  @param[in]      description  説明文。NULL 可。
 *  @param[in]      flags        登録フラグ。
 *  @param[out]     registered   登録した項目の格納先。
 *  @return         登録結果を返します。
 */
static int argparser_register_option_core(cplat_argparser *parser, argparser_spec_kind kind, const char *short_name,
                                          const char *long_name, const char *value_name, const char *description,
                                          const unsigned int flags, argparser_spec **registered)
{
    if (parser == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    int result = CPLAT_OK;

    if (short_name == NULL && long_name == NULL)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if (short_name != NULL && argparser_is_valid_short_name(short_name) == 0)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if (long_name != NULL && argparser_is_valid_long_name(long_name) == 0)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if ((flags & ~CPLAT_ARGPARSER_REQUIRED) != 0u)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else
    {
        result = argparser_prepare_spec_slot(parser, short_name, long_name);
        if (result == CPLAT_OK)
        {
            argparser_spec *spec = &parser->specs[parser->spec_count];
            memset(spec, 0, sizeof(*spec));
            spec->kind = kind;
            spec->flags = flags;

            result = argparser_copy_spec_strings(spec, short_name, long_name, value_name, description);
            if (result == CPLAT_OK)
            {
                parser->spec_count++;
                *registered = spec;
            }
        }
    }

    argparser_record_register_result(parser, result, short_name, long_name);

    return result;
}

/**
 *  @brief          位置引数の共通登録処理です。
 *  @param[in,out]  parser       対象のハンドル。
 *  @param[in]      kind         登録項目の種別。
 *  @param[in]      name         位置引数の名前。
 *  @param[in]      description  説明文。NULL 可。
 *  @param[in]      flags        登録フラグ。
 *  @param[out]     registered   登録した項目の格納先。
 *  @return         登録結果を返します。
 */
static int argparser_register_positional_core(cplat_argparser *parser, argparser_spec_kind kind, const char *name,
                                              const char *description, const unsigned int flags,
                                              argparser_spec **registered)
{
    if (parser == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    int result = CPLAT_OK;

    if (name == NULL)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if ((flags & ~CPLAT_ARGPARSER_REQUIRED) != 0u)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else
    {
        /* 可変長位置引数は位置引数列の末尾に 1 件だけ登録できる */
        for (size_t i = 0; i < parser->spec_count; i++)
        {
            if (argparser_spec_is_positional_array(&parser->specs[i]) != 0)
            {
                result = CPLAT_ERR_INVALID_ARGUMENT;
                break;
            }
        }

        /* 任意の位置引数の後に必須の位置引数は登録できない (割り当てが曖昧になるため) */
        if (result == CPLAT_OK && (flags & CPLAT_ARGPARSER_REQUIRED) != 0u)
        {
            for (size_t i = 0; i < parser->spec_count; i++)
            {
                const argparser_spec *spec = &parser->specs[i];
                if (argparser_spec_is_positional(spec) != 0 && (spec->flags & CPLAT_ARGPARSER_REQUIRED) == 0u)
                {
                    result = CPLAT_ERR_INVALID_ARGUMENT;
                    break;
                }
            }
        }

        if (result == CPLAT_OK)
        {
            result = argparser_prepare_spec_slot(parser, NULL, NULL);
            if (result == CPLAT_OK)
            {
                argparser_spec *spec = &parser->specs[parser->spec_count];
                memset(spec, 0, sizeof(*spec));
                spec->kind = kind;
                spec->flags = flags;

                result = argparser_copy_spec_strings(spec, NULL, name, NULL, description);
                if (result == CPLAT_OK)
                {
                    parser->spec_count++;
                    *registered = spec;
                }
            }
        }
    }

    argparser_record_register_result(parser, result, name, NULL);

    return result;
}

/**
 *  @brief          長いオプション名でトークンに一致する登録項目を検索します。
 *  @param[in]      parser    対象のハンドル。
 *  @param[in]      name      トークンの名前部分 (先頭)。
 *  @param[in]      name_len  名前部分の長さ。
 *  @return         一致した登録項目。見つからない場合は NULL を返します。
 */
static argparser_spec *argparser_find_long(const cplat_argparser *parser, const char *name, const size_t name_len)
{
    for (size_t i = 0; i < parser->spec_count; i++)
    {
        argparser_spec *spec = &parser->specs[i];
        if (argparser_spec_is_positional(spec) != 0 || spec->long_name == NULL)
        {
            continue;
        }
        if (strlen(spec->long_name) == name_len && strncmp(spec->long_name, name, name_len) == 0)
        {
            return spec;
        }
    }

    return NULL;
}

/**
 *  @brief          短いオプション名でトークンに一致する登録項目を検索します。
 *  @param[in]      parser      対象のハンドル。
 *  @param[in]      token       トークン ("-x" 形式の名前部分を含む文字列)。
 *  @param[in]      name_len    トークンのうちオプション名として扱う長さ。
 *  @return         一致した登録項目。見つからない場合は NULL を返します。
 */
static argparser_spec *argparser_find_short(const cplat_argparser *parser, const char *token, size_t name_len)
{
    for (size_t i = 0; i < parser->spec_count; i++)
    {
        argparser_spec *spec = &parser->specs[i];
        if (argparser_spec_is_positional(spec) != 0 || spec->short_name == NULL)
        {
            continue;
        }
        if (strlen(spec->short_name) == name_len && strncmp(spec->short_name, token, name_len) == 0)
        {
            return spec;
        }
    }

    return NULL;
}

/**
 *  @brief          値付きオプションの値を登録項目の格納先へ書き込みます。
 *  @param[in,out]  parser  対象のハンドル。
 *  @param[in,out]  spec    対象の登録項目。
 *  @param[in]      value   書き込む値の文字列。
 *  @param[in]      index   値を取得した argv インデックス。
 *  @return         成功時は @ref CPLAT_OK 、
 *                  失敗時は解析エラーの種別に対応する結果コードを返します。
 */
static int argparser_store_option_value(cplat_argparser *parser, argparser_spec *spec, const char *value,
                                        const int index)
{
    const char *display_name = argparser_spec_display_name(spec);

    if (spec->kind == ARGPARSER_SPEC_OPTION_INT || spec->kind == ARGPARSER_SPEC_OPTION_STRING)
    {
        if (spec->found > 0)
        {
            return argparser_set_error(parser, CPLAT_ERR_DUPLICATE_OPTION, display_name, index);
        }
        if (spec->kind == ARGPARSER_SPEC_OPTION_INT)
        {
            int int_error = cplat_parse_int(spec->int_storage, value, 10);
            if (int_error != CPLAT_OK)
            {
                return argparser_set_error(parser, int_error, display_name, index);
            }
        }
        else
        {
            *spec->string_storage = value;
        }
    }
    else
    {
        if (*spec->count >= spec->capacity)
        {
            return argparser_set_error(parser, CPLAT_ERR_TOO_MANY_OCCURRENCES, display_name, index);
        }
        if (spec->kind == ARGPARSER_SPEC_OPTION_INT_ARRAY)
        {
            int int_error = cplat_parse_int(&spec->int_storage[*spec->count], value, 10);
            if (int_error != CPLAT_OK)
            {
                return argparser_set_error(parser, int_error, display_name, index);
            }
        }
        else
        {
            spec->string_storage[*spec->count] = value;
        }
        (*spec->count)++;
    }

    spec->found++;

    return CPLAT_OK;
}

/**
 *  @brief          位置引数トークンを次の未割当の位置引数へ書き込みます。
 *  @param[in,out]  parser            対象のハンドル。
 *  @param[in]      token             位置引数トークン。
 *  @param[in]      index             トークンの argv インデックス。
 *  @param[in,out]  positional_index  次に割り当てる位置引数の検索開始位置。
 *  @return         成功時は @ref CPLAT_OK 、
 *                  失敗時は解析エラーの種別に対応する結果コードを返します。
 */
static int argparser_store_positional(cplat_argparser *parser, const char *token, const int index,
                                      size_t *positional_index)
{
    while (*positional_index < parser->spec_count)
    {
        argparser_spec *spec = &parser->specs[*positional_index];
        if (argparser_spec_is_positional(spec) == 0)
        {
            (*positional_index)++;
            continue;
        }

        if (argparser_spec_is_positional_array(spec) != 0)
        {
            if (*spec->count >= spec->capacity)
            {
                return argparser_set_error(parser, CPLAT_ERR_TOO_MANY_ARGUMENTS, token, index);
            }
            if (spec->kind == ARGPARSER_SPEC_POSITIONAL_INT_ARRAY)
            {
                int int_error = cplat_parse_int(&spec->int_storage[*spec->count], token, 10);
                if (int_error != CPLAT_OK)
                {
                    return argparser_set_error(parser, int_error, spec->long_name, index);
                }
            }
            else
            {
                spec->string_storage[*spec->count] = token;
            }
            (*spec->count)++;
            spec->found++;

            return CPLAT_OK;
        }

        (*positional_index)++;

        if (spec->kind == ARGPARSER_SPEC_POSITIONAL_INT)
        {
            int int_error = cplat_parse_int(spec->int_storage, token, 10);
            if (int_error != CPLAT_OK)
            {
                return argparser_set_error(parser, int_error, spec->long_name, index);
            }
        }
        else
        {
            *spec->string_storage = token;
        }
        spec->found++;

        return CPLAT_OK;
    }

    return argparser_set_error(parser, CPLAT_ERR_TOO_MANY_ARGUMENTS, token, index);
}

/**
 *  @brief          argv[0] からベース名を求めてハンドルに保持します。
 *  @param[in,out]  parser  対象のハンドル。
 *  @param[in]      argv0   argv[0] の文字列。NULL 可。
 *
 *  複製に失敗した場合は前回値を維持します (usage 表示のみに影響するため)。
 */
static void argparser_resolve_program_name(cplat_argparser *parser, const char *argv0)
{
    if (argv0 == NULL)
    {
        return;
    }

    const char *base = cplat_path_basename(argv0);

    char *copied = argparser_strdup(base);
    if (copied == NULL)
    {
        return;
    }
    cplat_free(parser->resolved_program_name);
    parser->resolved_program_name = copied;
}

/* ================================================================
 * usage 組み立て
 * ================================================================ */

/* usage 文字列の書き込み状態 */
typedef struct argparser_usage_writer
{
    char *buffer;       /* 書き込み先。NULL の場合はサイズ計測のみ */
    size_t buffer_size; /* buffer のバイト数 */
    size_t needed;      /* NUL 終端を除く必要バイト数 (書き込み済みとは限らない) */
} argparser_usage_writer;

/**
 *  @brief          usage 文字列に文字列を追記します。
 *  @param[in,out]  writer  書き込み状態。
 *  @param[in]      text    追記する文字列。
 *
 *  バッファーに収まらない部分は破棄し、必要バイト数の計測のみ継続します。
 */
static void argparser_usage_write(argparser_usage_writer *writer, const char *text)
{
    size_t length = strlen(text);

    if (writer->buffer != NULL && writer->needed < (writer->buffer_size - 1))
    {
        size_t writable = writer->buffer_size - 1 - writer->needed;
        if (writable > length)
        {
            writable = length;
        }
        memcpy(&writer->buffer[writer->needed], text, writable);
    }
    writer->needed += length;
}

/**
 *  @brief          usage 文字列に空白を追記します。
 *  @param[in,out]  writer  書き込み状態。
 *  @param[in]      count   追記する空白の数。
 */
static void argparser_usage_write_spaces(argparser_usage_writer *writer, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        argparser_usage_write(writer, " ");
    }
}

/**
 *  @brief          登録項目の値表示名を取得します。
 *  @param[in]      spec  対象の登録項目。
 *  @return         値表示名を返します。未指定の場合は既定名を返します。
 */
static const char *argparser_spec_value_name(const argparser_spec *spec)
{
    if (spec->value_name != NULL)
    {
        return spec->value_name;
    }

    return s_default_value_name;
}

/**
 *  @brief          usage の 1 項目分 (左列ラベルと説明文) を追記します。
 *  @param[in,out]  writer       書き込み状態。
 *  @param[in]      label_start  左列ラベルの書き込み開始時点の必要バイト数。
 *  @param[in]      spec         対象の登録項目。
 */
static void argparser_usage_write_description(argparser_usage_writer *writer, const size_t label_start,
                                              const argparser_spec *spec)
{
    size_t label_length = writer->needed - label_start;

    if (label_length < ARGPARSER_USAGE_DESC_COLUMN)
    {
        argparser_usage_write_spaces(writer, ARGPARSER_USAGE_DESC_COLUMN - label_length);
    }
    else
    {
        argparser_usage_write(writer, "\n");
        argparser_usage_write_spaces(writer, ARGPARSER_USAGE_DESC_COLUMN);
    }

    if (spec->description != NULL)
    {
        argparser_usage_write(writer, spec->description);
    }
    if ((spec->flags & CPLAT_ARGPARSER_REQUIRED) != 0u)
    {
        if (spec->description != NULL)
        {
            argparser_usage_write(writer, " ");
        }
        argparser_usage_write(writer, "(required)");
    }
    argparser_usage_write(writer, "\n");
}

/**
 *  @brief          usage 文字列全体を組み立てます。
 *  @param[in]      parser  対象のハンドル。
 *  @param[in,out]  writer  書き込み状態。
 */
static void argparser_usage_build(const cplat_argparser *parser, argparser_usage_writer *writer)
{
    int has_option = 0;
    int has_positional = 0;

    for (size_t i = 0; i < parser->spec_count; i++)
    {
        if (argparser_spec_is_positional(&parser->specs[i]) != 0)
        {
            has_positional = 1;
        }
        else
        {
            has_option = 1;
        }
    }

    if (parser->program_description != NULL)
    {
        argparser_usage_write(writer, parser->program_description);
        argparser_usage_write(writer, "\n\n");
    }

    const char *program_name = parser->program_name;
    if (program_name == NULL)
    {
        program_name = parser->resolved_program_name;
    }
    if (program_name == NULL)
    {
        program_name = s_default_program_name;
    }

    argparser_usage_write(writer, "Usage: ");
    argparser_usage_write(writer, program_name);
    if (has_option != 0)
    {
        argparser_usage_write(writer, " [OPTIONS]");
    }
    for (size_t i = 0; i < parser->spec_count; i++)
    {
        const argparser_spec *spec = &parser->specs[i];
        if (argparser_spec_is_positional(spec) == 0)
        {
            continue;
        }
        if ((spec->flags & CPLAT_ARGPARSER_REQUIRED) != 0u)
        {
            argparser_usage_write(writer, " <");
            argparser_usage_write(writer, spec->long_name);
            argparser_usage_write(writer, ">");
            if (argparser_spec_is_positional_array(spec) != 0)
            {
                argparser_usage_write(writer, "...");
            }
        }
        else
        {
            argparser_usage_write(writer, " [");
            argparser_usage_write(writer, spec->long_name);
            if (argparser_spec_is_positional_array(spec) != 0)
            {
                argparser_usage_write(writer, "...");
            }
            argparser_usage_write(writer, "]");
        }
    }
    argparser_usage_write(writer, "\n");

    if (has_positional != 0)
    {
        argparser_usage_write(writer, "\nPositional arguments:\n");
        for (size_t i = 0; i < parser->spec_count; i++)
        {
            const argparser_spec *spec = &parser->specs[i];
            if (argparser_spec_is_positional(spec) == 0)
            {
                continue;
            }
            size_t label_start = writer->needed;
            argparser_usage_write(writer, "  ");
            argparser_usage_write(writer, spec->long_name);
            argparser_usage_write_description(writer, label_start, spec);
        }
    }

    if (has_option != 0)
    {
        argparser_usage_write(writer, "\nOptions:\n");
        for (size_t i = 0; i < parser->spec_count; i++)
        {
            const argparser_spec *spec = &parser->specs[i];
            if (argparser_spec_is_positional(spec) != 0)
            {
                continue;
            }
            size_t label_start = writer->needed;
            argparser_usage_write(writer, "  ");
            if (spec->short_name != NULL && spec->long_name != NULL)
            {
                argparser_usage_write(writer, spec->short_name);
                argparser_usage_write(writer, ", ");
                argparser_usage_write(writer, spec->long_name);
            }
            else if (spec->short_name != NULL)
            {
                argparser_usage_write(writer, spec->short_name);
            }
            else
            {
                /* short 名の桁 ("-x, ") を空白で揃える */
                argparser_usage_write_spaces(writer, 4);
                argparser_usage_write(writer, spec->long_name);
            }
            if (argparser_spec_takes_value(spec) != 0)
            {
                argparser_usage_write(writer, " ");
                argparser_usage_write(writer, argparser_spec_value_name(spec));
            }
            argparser_usage_write_description(writer, label_start, spec);
        }
    }
}

/* ================================================================
 * 公開 API
 * ================================================================ */

/**
 *  @brief          パーサー ハンドルが所有する領域を解放します。
 *  @param[in]      parser  対象のパーサー ハンドル。NULL を渡してはなりません。
 *
 *  ハンドル自身は解放しません。メンバーの値も初期化しません。\n
 *  呼び出し側が、破棄なら @ref argparser_dispose_core 、
 *  再初期化なら @ref argparser_reset_core として後続を行います。
 */
static void argparser_release_members(cplat_argparser *parser)
{
    for (size_t i = 0; i < parser->spec_count; i++)
    {
        argparser_free_spec(&parser->specs[i]);
    }
    cplat_free(parser->specs);
    cplat_free(parser->program_name);
    cplat_free(parser->program_description);
    cplat_free(parser->resolved_program_name);
    cplat_free(parser->last_error_target);
    for (size_t i = 0; i < parser->register_error_count; i++)
    {
        cplat_free(parser->register_errors[i].target);
    }
    cplat_free(parser->register_errors);
}

/**
 *  @brief          パーサー ハンドルが保持するリソースを解放します。
 *  @param[in]      parser  解放するパーサー ハンドル。
 */
static void argparser_dispose_core(cplat_argparser *parser)
{
    argparser_release_members(parser);
    cplat_free(parser);
}

/**
 *  @brief          パーサー ハンドルを生成直後と同じ状態へ戻します。
 *  @param[in,out]  parser  対象のパーサー ハンドル。NULL を渡してはなりません。
 *
 *  登録済みのオプション、解析結果、エラー情報をすべて捨てます。\n
 *  ハンドルは作り直さないため、既に取得済みのポインターは有効なままです。\n
 *  所有区分 (library_owned) は引き継ぎます。
 */
static void argparser_reset_core(cplat_argparser *parser)
{
    int library_owned = parser->library_owned;

    argparser_release_members(parser);
    /* 宣言済みハンドルの再初期化のため、集成体初期化子ではなく memset を使う。 */
    memset(parser, 0, sizeof(*parser));
    parser->last_error_index = -1;
    parser->library_owned = library_owned;
}

/**
 *  @brief          生成オプションをパーサー ハンドルへ適用します。
 *  @param[in,out]  parser   対象のパーサー ハンドル。NULL を渡してはなりません。
 *  @param[in]      options  生成オプション。NULL のときは何もしません。
 *  @return         成功なら 0、複製に失敗したら -1 です。
 *
 *  呼び出し前に、対象メンバーが NULL であること (生成直後または
 *  @ref argparser_reset_core 直後) を前提とします。
 */
static int argparser_apply_options(cplat_argparser *parser, const cplat_argparser_options *options)
{
    if (options == NULL)
    {
        return 0;
    }

    parser->program_name = argparser_strdup(options->program_name);
    if ((options->program_name != NULL) && (parser->program_name == NULL))
    {
        return -1;
    }
    parser->program_description = argparser_strdup(options->program_description);
    if ((options->program_description != NULL) && (parser->program_description == NULL))
    {
        return -1;
    }
    return 0;
}

/**
 *  @brief          解析対象のコマンド ライン引数をパーサー ハンドルへ適用します。
 *  @param[in,out]  parser  対象のパーサー ハンドル。NULL を渡してはなりません。
 *  @param[in]      argc    argv の要素数。1 未満も指定できます。
 *  @param[in]      argv    コマンド ライン引数の配列。NULL も指定できます。
 *
 *  @p argv は複製せず、ポインターをそのまま保持します。\n
 *  @p argc と @p argv が解析できる組み合わせの場合に限り、
 *  usage 用のプログラム名を argv[0] から求めます。\n
 *  不正な組み合わせはここでは通知せず、
 *  cplat_argparser_handle_parse() が CPLAT_ERR_INVALID_ARGUMENT を返します。
 */
static void argparser_apply_args(cplat_argparser *parser, int argc, char *const *argv)
{
    parser->argc = argc;
    parser->argv = argv;

    if ((argc < 1) || (argv == NULL))
    {
        return;
    }
    argparser_resolve_program_name(parser, argv[0]);
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_argparser *cplat_argparser_handle_create(const int argc, char *const *argv,
                                               const cplat_argparser_options *options)
{
    cplat_argparser *parser = cplat_calloc(1, sizeof(cplat_argparser));
    if (parser == NULL)
    {
        return NULL;
    }
    parser->last_error_index = -1;

    if (argparser_apply_options(parser, options) != 0)
    {
        cplat_argparser_handle_dispose(parser);
        return NULL;
    }
    argparser_apply_args(parser, argc, argv);

    return parser;
}

void cplat_argparser_handle_dispose(cplat_argparser *parser)
{
    if (parser == NULL || parser->library_owned != 0)
    {
        return;
    }

    argparser_dispose_core(parser);
}

/* プロセス共有の既定パーサー ハンドル */
static cplat_argparser *s_default_parser = NULL;
/* 破棄と並行取得の競合を避けるため、初期化後はプロセス終了まで同じロックを使用する */
static cplat_local_lock *s_default_lock = NULL;
static cplat_once_flag s_default_initialize_once = {0};

static void argparser_default_dispose_on_shutdown(const cplat_shutdown_event *event, void *context)
{
    (void)context;
    if (event == NULL || event->reason != CPLAT_SHUTDOWN_REASON_NORMAL_EXIT)
    {
        return;
    }
    cplat_local_lock *lock = s_default_lock;
    if (lock == NULL)
    {
        return;
    }

    if (cplat_local_lock_lock(lock, CPLAT_SYNC_WAIT_FOREVER) != CPLAT_OK)
    {
        return;
    }
    cplat_argparser *parser = s_default_parser;
    s_default_parser = NULL;
    (void)cplat_local_lock_unlock(lock);

    if (parser != NULL)
    {
        argparser_dispose_core(parser);
    }
}

static void argparser_default_initialize(void)
{
    if (cplat_local_lock_create(&s_default_lock) != CPLAT_OK)
    {
        s_default_lock = NULL;
        return;
    }
    if (cplat_shutdown_register(argparser_default_dispose_on_shutdown, NULL) != CPLAT_OK)
    {
        cplat_local_lock_dispose(s_default_lock);
        s_default_lock = NULL;
    }
}

/**
 *  @brief          プロセス共有の既定パーサーを取得します。
 *  @param[in]      argc            解析対象の argv の要素数。適用しない場合は 0 を指定します。
 *  @param[in]      argv            解析対象のコマンド ライン引数。適用しない場合は NULL を指定します。
 *  @param[in]      options         生成オプション。NULL も指定できます。
 *  @param[in]      reset_existing  0 以外なら、既存インスタンスを生成直後の状態へ戻して
 *                                  @p argc 、@p argv 、@p options を適用し直します。
 *  @return         既定パーサー。生成に失敗した場合は NULL です。
 *
 *  生成と再初期化を同じロック区間で行い、取得と初期化の競合を避けます。
 */
static cplat_argparser *argparser_default_acquire(const int argc, char *const *argv,
                                                  const cplat_argparser_options *options, int reset_existing)
{
    cplat_call_once(&s_default_initialize_once, argparser_default_initialize);
    cplat_local_lock *lock = s_default_lock;
    if (lock == NULL)
    {
        return NULL;
    }

    if (cplat_local_lock_lock(lock, CPLAT_SYNC_WAIT_FOREVER) != CPLAT_OK)
    {
        return NULL;
    }
    if (s_default_parser == NULL)
    {
        s_default_parser = cplat_argparser_handle_create(argc, argv, options);
        if (s_default_parser != NULL)
        {
            s_default_parser->library_owned = 1;
        }
    }
    else if (reset_existing != 0)
    {
        argparser_reset_core(s_default_parser);
        (void)argparser_apply_options(s_default_parser, options);
        argparser_apply_args(s_default_parser, argc, argv);
    }
    cplat_argparser *parser = s_default_parser;
    (void)cplat_local_lock_unlock(lock);

    return parser;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_argparser_init(const int argc, char *const *argv, const char *description)
{
    cplat_argparser_options options = {0};
    options.program_description = description;
    (void)argparser_default_acquire(argc, argv, &options, 1);
}

int cplat_argparser_handle_register_flag(cplat_argparser *parser, const char *short_name, const char *long_name,
                                         const char *description, int *storage)
{
    if (storage == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, short_name, long_name);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result = argparser_register_option_core(parser, ARGPARSER_SPEC_FLAG, short_name, long_name, NULL, description,
                                                0u, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->int_storage = storage;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_flag(const char *short_name, const char *long_name, const char *description, int *storage)
{
    return cplat_argparser_handle_register_flag(argparser_default_acquire(0, NULL, NULL, 0), short_name, long_name,
                                                description, storage);
}

int cplat_argparser_handle_register_option_int(cplat_argparser *parser, const char *short_name, const char *long_name,
                                               const char *value_name, const char *description,
                                               const unsigned int flags, int *storage)
{
    if (storage == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, short_name, long_name);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result = argparser_register_option_core(parser, ARGPARSER_SPEC_OPTION_INT, short_name, long_name, value_name,
                                                description, flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->int_storage = storage;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_option_int(const char *short_name, const char *long_name, const char *value_name,
                                        const char *description, const unsigned int flags, int *storage)
{
    return cplat_argparser_handle_register_option_int(argparser_default_acquire(0, NULL, NULL, 0), short_name,
                                                      long_name, value_name, description, flags, storage);
}

int cplat_argparser_handle_register_option_string(cplat_argparser *parser, const char *short_name,
                                                  const char *long_name, const char *value_name,
                                                  const char *description, const unsigned int flags,
                                                  const char **storage)
{
    if (storage == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, short_name, long_name);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result = argparser_register_option_core(parser, ARGPARSER_SPEC_OPTION_STRING, short_name, long_name, value_name,
                                                description, flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->string_storage = storage;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_option_string(const char *short_name, const char *long_name, const char *value_name,
                                           const char *description, const unsigned int flags, const char **storage)
{
    return cplat_argparser_handle_register_option_string(argparser_default_acquire(0, NULL, NULL, 0), short_name,
                                                         long_name, value_name, description, flags, storage);
}

int cplat_argparser_handle_register_option_int_array(cplat_argparser *parser, const char *short_name,
                                                     const char *long_name, const char *value_name,
                                                     const char *description, const unsigned int flags, int *storage,
                                                     const size_t capacity, size_t *count)
{
    if (storage == NULL || capacity == 0 || count == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, short_name, long_name);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result = argparser_register_option_core(parser, ARGPARSER_SPEC_OPTION_INT_ARRAY, short_name, long_name,
                                                value_name, description, flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->int_storage = storage;
    spec->capacity = capacity;
    spec->count = count;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_option_int_array(const char *short_name, const char *long_name, const char *value_name,
                                              const char *description, const unsigned int flags, int *storage,
                                              const size_t capacity, size_t *count)
{
    return cplat_argparser_handle_register_option_int_array(argparser_default_acquire(0, NULL, NULL, 0), short_name,
                                                            long_name, value_name, description, flags, storage,
                                                            capacity, count);
}

int cplat_argparser_handle_register_option_string_array(cplat_argparser *parser, const char *short_name,
                                                        const char *long_name, const char *value_name,
                                                        const char *description, const unsigned int flags,
                                                        const char **storage, const size_t capacity, size_t *count)
{
    if (storage == NULL || capacity == 0 || count == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, short_name, long_name);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result = argparser_register_option_core(parser, ARGPARSER_SPEC_OPTION_STRING_ARRAY, short_name, long_name,
                                                value_name, description, flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->string_storage = storage;
    spec->capacity = capacity;
    spec->count = count;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_option_string_array(const char *short_name, const char *long_name, const char *value_name,
                                                 const char *description, const unsigned int flags,
                                                 const char **storage, const size_t capacity, size_t *count)
{
    return cplat_argparser_handle_register_option_string_array(argparser_default_acquire(0, NULL, NULL, 0), short_name,
                                                               long_name, value_name, description, flags, storage,
                                                               capacity, count);
}

int cplat_argparser_handle_register_positional_int(cplat_argparser *parser, const char *name, const char *description,
                                                   const unsigned int flags, int *storage)
{
    if (storage == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, name, NULL);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result =
        argparser_register_positional_core(parser, ARGPARSER_SPEC_POSITIONAL_INT, name, description, flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->int_storage = storage;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_positional_int(const char *name, const char *description, const unsigned int flags,
                                            int *storage)
{
    return cplat_argparser_handle_register_positional_int(argparser_default_acquire(0, NULL, NULL, 0), name,
                                                          description, flags, storage);
}

int cplat_argparser_handle_register_positional_string(cplat_argparser *parser, const char *name,
                                                      const char *description, const unsigned int flags,
                                                      const char **storage)
{
    if (storage == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, name, NULL);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result =
        argparser_register_positional_core(parser, ARGPARSER_SPEC_POSITIONAL_STRING, name, description, flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->string_storage = storage;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_positional_string(const char *name, const char *description, const unsigned int flags,
                                               const char **storage)
{
    return cplat_argparser_handle_register_positional_string(argparser_default_acquire(0, NULL, NULL, 0), name,
                                                             description, flags, storage);
}

int cplat_argparser_handle_register_positional_int_array(cplat_argparser *parser, const char *name,
                                                         const char *description, const unsigned int flags,
                                                         int *storage, const size_t capacity, size_t *count)
{
    if (storage == NULL || capacity == 0 || count == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, name, NULL);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result = argparser_register_positional_core(parser, ARGPARSER_SPEC_POSITIONAL_INT_ARRAY, name, description,
                                                    flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->int_storage = storage;
    spec->capacity = capacity;
    spec->count = count;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_positional_int_array(const char *name, const char *description, const unsigned int flags,
                                                  int *storage, const size_t capacity, size_t *count)
{
    return cplat_argparser_handle_register_positional_int_array(argparser_default_acquire(0, NULL, NULL, 0), name,
                                                                description, flags, storage, capacity, count);
}

int cplat_argparser_handle_register_positional_string_array(cplat_argparser *parser, const char *name,
                                                            const char *description, const unsigned int flags,
                                                            const char **storage, const size_t capacity, size_t *count)
{
    if (storage == NULL || capacity == 0 || count == NULL)
    {
        argparser_record_register_result(parser, CPLAT_ERR_INVALID_ARGUMENT, name, NULL);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_spec *spec = NULL;
    int result = argparser_register_positional_core(parser, ARGPARSER_SPEC_POSITIONAL_STRING_ARRAY, name, description,
                                                    flags, &spec);
    if (result != CPLAT_OK)
    {
        return result;
    }
    spec->string_storage = storage;
    spec->capacity = capacity;
    spec->count = count;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_register_positional_string_array(const char *name, const char *description,
                                                     const unsigned int flags, const char **storage,
                                                     const size_t capacity, size_t *count)
{
    return cplat_argparser_handle_register_positional_string_array(argparser_default_acquire(0, NULL, NULL, 0), name,
                                                                   description, flags, storage, capacity, count);
}

int cplat_argparser_handle_parse(cplat_argparser *parser)
{
    if (parser == NULL || parser->argc < 1 || parser->argv == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    const int argc = parser->argc;
    char *const *argv = parser->argv;

    argparser_clear_error(parser);

    /* 再実行に備えて出現状態を初期化する */
    for (size_t i = 0; i < parser->spec_count; i++)
    {
        argparser_spec *spec = &parser->specs[i];
        spec->found = 0;
        if (spec->kind == ARGPARSER_SPEC_FLAG)
        {
            *spec->int_storage = 0;
        }
        if (spec->count != NULL)
        {
            *spec->count = 0;
        }
    }

    size_t positional_index = 0;

    for (int i = 1; i < argc; i++)
    {
        const char *token = argv[i];
        if (token == NULL)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }

        if (strncmp(token, "--", 2) == 0 && token[2] != '\0')
        {
            /* 長いオプション ("--xxx" / "--xxx=value") */
            const char *equal = strchr(token, '=');
            size_t name_len = strlen(token);
            if (equal != NULL)
            {
                name_len = (size_t)(equal - token);
            }

            argparser_spec *spec = argparser_find_long(parser, token, name_len);
            if (spec == NULL)
            {
                return argparser_set_error(parser, CPLAT_ERR_UNKNOWN_OPTION, token, i);
            }

            if (spec->kind == ARGPARSER_SPEC_FLAG)
            {
                if (equal != NULL)
                {
                    return argparser_set_error(parser, CPLAT_ERR_UNEXPECTED_VALUE, argparser_spec_display_name(spec),
                                               i);
                }
                (*spec->int_storage)++;
                spec->found++;
                continue;
            }

            const char *value = NULL;
            int value_index = i;
            if (equal != NULL)
            {
                value = equal + 1;
            }
            else
            {
                if ((i + 1) >= argc)
                {
                    return argparser_set_error(parser, CPLAT_ERR_MISSING_VALUE, argparser_spec_display_name(spec), i);
                }
                i++;
                value = argv[i];
                value_index = i;
                if (value == NULL)
                {
                    return CPLAT_ERR_INVALID_ARGUMENT;
                }
            }

            int result = argparser_store_option_value(parser, spec, value, value_index);
            if (result != CPLAT_OK)
            {
                return result;
            }
            continue;
        }

        if (token[0] == '-' && token[1] != '\0')
        {
            /* 短いオプション ("-x" / "-x=value")。連結 ("-abc") と "--" は未サポートのため未知扱い */
            const char *equal = strchr(token, '=');
            size_t name_len = strlen(token);
            if (equal != NULL)
            {
                name_len = (size_t)(equal - token);
            }

            argparser_spec *spec = NULL;
            if (name_len == 2)
            {
                spec = argparser_find_short(parser, token, name_len);
            }
            if (spec == NULL)
            {
                /* 次の位置引数が整数の場合、負数は短いオプションではなく位置引数として扱う */
                if (token[1] >= '0' && token[1] <= '9')
                {
                    size_t next_positional_index = positional_index;
                    while (next_positional_index < parser->spec_count)
                    {
                        const argparser_spec *next_spec = &parser->specs[next_positional_index];
                        if (argparser_spec_is_positional(next_spec) != 0)
                        {
                            if (next_spec->kind == ARGPARSER_SPEC_POSITIONAL_INT ||
                                next_spec->kind == ARGPARSER_SPEC_POSITIONAL_INT_ARRAY)
                            {
                                int result = argparser_store_positional(parser, token, i, &positional_index);
                                if (result != CPLAT_OK)
                                {
                                    return result;
                                }
                                break;
                            }
                            return argparser_set_error(parser, CPLAT_ERR_UNKNOWN_OPTION, token, i);
                        }
                        next_positional_index++;
                    }
                    if (next_positional_index < parser->spec_count)
                    {
                        continue;
                    }
                }
                return argparser_set_error(parser, CPLAT_ERR_UNKNOWN_OPTION, token, i);
            }

            if (spec->kind == ARGPARSER_SPEC_FLAG)
            {
                if (equal != NULL)
                {
                    return argparser_set_error(parser, CPLAT_ERR_UNEXPECTED_VALUE, argparser_spec_display_name(spec),
                                               i);
                }
                (*spec->int_storage)++;
                spec->found++;
                continue;
            }

            const char *value = NULL;
            int value_index = i;
            if (equal != NULL)
            {
                value = equal + 1;
            }
            else
            {
                if ((i + 1) >= argc)
                {
                    return argparser_set_error(parser, CPLAT_ERR_MISSING_VALUE, argparser_spec_display_name(spec), i);
                }
                i++;
                value = argv[i];
                value_index = i;
                if (value == NULL)
                {
                    return CPLAT_ERR_INVALID_ARGUMENT;
                }
            }

            int result = argparser_store_option_value(parser, spec, value, value_index);
            if (result != CPLAT_OK)
            {
                return result;
            }
            continue;
        }

        /* 位置引数 ("-" 単独を含む) */
        int result = argparser_store_positional(parser, token, i, &positional_index);
        if (result != CPLAT_OK)
        {
            return result;
        }
    }

    /* 必須チェック */
    for (size_t i = 0; i < parser->spec_count; i++)
    {
        const argparser_spec *spec = &parser->specs[i];
        if ((spec->flags & CPLAT_ARGPARSER_REQUIRED) != 0u && spec->found == 0)
        {
            const char *display_name = spec->long_name;
            if (argparser_spec_is_positional(spec) == 0)
            {
                display_name = argparser_spec_display_name(spec);
            }
            return argparser_set_error(parser, CPLAT_ERR_MISSING_REQUIRED, display_name, -1);
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_parse(void)
{
    return cplat_argparser_handle_parse(argparser_default_acquire(0, NULL, NULL, 0));
}

int cplat_argparser_handle_get_error(const cplat_argparser *parser)
{
    if (parser == NULL)
    {
        return CPLAT_OK;
    }

    return parser->last_error;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_get_error(void)
{
    return cplat_argparser_handle_get_error(argparser_default_acquire(0, NULL, NULL, 0));
}

const char *cplat_argparser_handle_get_error_target(const cplat_argparser *parser)
{
    if (parser == NULL)
    {
        return NULL;
    }

    return parser->last_error_target;
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_argparser_get_error_target(void)
{
    return cplat_argparser_handle_get_error_target(argparser_default_acquire(0, NULL, NULL, 0));
}

int cplat_argparser_handle_get_error_index(const cplat_argparser *parser)
{
    if (parser == NULL)
    {
        return -1;
    }

    return parser->last_error_index;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_get_error_index(void)
{
    return cplat_argparser_handle_get_error_index(argparser_default_acquire(0, NULL, NULL, 0));
}

int cplat_argparser_handle_get_error_message(const cplat_argparser *parser, char *buffer, const size_t buffer_size)
{
    if (parser == NULL || buffer == NULL || buffer_size == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    const char *target = parser->last_error_target;
    if (target == NULL)
    {
        target = "?";
    }

    /* -Wformat-nonliteral を避けるため、エラー種別ごとにリテラル書式で組み立てる */
    /* 切り詰め時も途中までを残す (呼び出し側が部分メッセージを表示する)。 */
    int written = 0;
    switch (parser->last_error)
    {
    case CPLAT_ERR_UNKNOWN_OPTION:
        written = snprintf(buffer, buffer_size, "unknown option '%s'", target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_MISSING_VALUE:
        written =
            snprintf(buffer, buffer_size, "option '%s' requires a value", target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_INVALID_INTEGER:
        written = snprintf(buffer, buffer_size, "value of '%s' must be an integer",
                           target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_OUT_OF_RANGE:
        written =
            snprintf(buffer, buffer_size, "value of '%s' is out of range", target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_MISSING_REQUIRED:
        written = snprintf(buffer, buffer_size, "'%s' is required", target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_DUPLICATE_OPTION:
        written = snprintf(buffer, buffer_size, "option '%s' is specified more than once",
                           target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_TOO_MANY_ARGUMENTS:
        written = snprintf(buffer, buffer_size, "too many arguments: '%s'", target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_TOO_MANY_OCCURRENCES:
        written = snprintf(buffer, buffer_size, "option '%s' is specified too many times",
                           target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_UNEXPECTED_VALUE:
        written = snprintf(buffer, buffer_size, "option '%s' does not take a value",
                           target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_OK:
    default:
        written = snprintf(buffer, buffer_size, "no error"); /* 置換対象外: 意図的な切り詰め */
        break;
    }

    if (written < 0)
    {
        buffer[0] = '\0';
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((size_t)written >= buffer_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_get_error_message(char *buffer, const size_t buffer_size)
{
    return cplat_argparser_handle_get_error_message(argparser_default_acquire(0, NULL, NULL, 0), buffer, buffer_size);
}

int cplat_argparser_handle_get_usage(const cplat_argparser *parser, char *buffer, const size_t buffer_size,
                                     size_t *required_size)
{
    if (parser == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (buffer == NULL && required_size == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (buffer != NULL && buffer_size == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    argparser_usage_writer writer;
    writer.buffer = buffer;
    writer.buffer_size = buffer_size;
    writer.needed = 0;

    argparser_usage_build(parser, &writer);

    if (required_size != NULL)
    {
        *required_size = writer.needed + 1;
    }

    if (buffer != NULL)
    {
        size_t nul_position = writer.needed;
        if (nul_position > (buffer_size - 1))
        {
            nul_position = buffer_size - 1;
        }
        buffer[nul_position] = '\0';

        if ((writer.needed + 1) > buffer_size)
        {
            return CPLAT_ERR_BUFFER_TOO_SMALL;
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_get_usage(char *buffer, const size_t buffer_size, size_t *required_size)
{
    return cplat_argparser_handle_get_usage(argparser_default_acquire(0, NULL, NULL, 0), buffer, buffer_size,
                                            required_size);
}

int cplat_argparser_handle_print_usage(const cplat_argparser *parser, FILE *stream)
{
    if (parser == NULL || stream == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    size_t required_size = 0;
    cplat_argparser_handle_get_usage(parser, NULL, 0, &required_size);

    char *buffer = cplat_malloc(required_size);
    if (buffer == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    int result = cplat_argparser_handle_get_usage(parser, buffer, required_size, NULL);
    if (result == CPLAT_OK)
    {
        fprintf(stream, "%s", buffer);
    }

    cplat_free(buffer);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_print_usage(FILE *stream)
{
    return cplat_argparser_handle_print_usage(argparser_default_acquire(0, NULL, NULL, 0), stream);
}

int cplat_argparser_handle_print_error_messages(const cplat_argparser *parser, FILE *stream)
{
    if (parser == NULL || stream == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (parser->last_error == CPLAT_OK)
    {
        return CPLAT_OK;
    }

    char message[ARGPARSER_ERROR_MESSAGE_BYTES];
    int result = cplat_argparser_handle_get_error_message(parser, message, sizeof(message));
    if (result == CPLAT_OK || result == CPLAT_ERR_BUFFER_TOO_SMALL)
    {
        fprintf(stream, "error: %s\n", message);
    }
    fprintf(stream, "\n");

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_print_error_messages(FILE *stream)
{
    return cplat_argparser_handle_print_error_messages(argparser_default_acquire(0, NULL, NULL, 0), stream);
}

size_t cplat_argparser_handle_get_register_error_count(const cplat_argparser *parser)
{
    if (parser == NULL)
    {
        return 0;
    }

    return parser->register_error_count;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_argparser_get_register_error_count(void)
{
    return cplat_argparser_handle_get_register_error_count(argparser_default_acquire(0, NULL, NULL, 0));
}

int cplat_argparser_handle_get_register_error(const cplat_argparser *parser, const size_t index)
{
    if (parser == NULL || index >= parser->register_error_count)
    {
        return CPLAT_OK;
    }

    return parser->register_errors[index].result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_get_register_error(size_t index)
{
    return cplat_argparser_handle_get_register_error(argparser_default_acquire(0, NULL, NULL, 0), index);
}

const char *cplat_argparser_handle_get_register_error_target(const cplat_argparser *parser, const size_t index)
{
    if (parser == NULL || index >= parser->register_error_count)
    {
        return NULL;
    }

    return parser->register_errors[index].target;
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_argparser_get_register_error_target(size_t index)
{
    return cplat_argparser_handle_get_register_error_target(argparser_default_acquire(0, NULL, NULL, 0), index);
}

int cplat_argparser_handle_get_register_error_message(const cplat_argparser *parser, const size_t index, char *buffer,
                                                      const size_t buffer_size)
{
    if (parser == NULL || buffer == NULL || buffer_size == 0 || index >= parser->register_error_count)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    const char *target = parser->register_errors[index].target;
    if (target == NULL)
    {
        target = "?";
    }

    /* -Wformat-nonliteral を避けるため、結果コードごとにリテラル書式で組み立てる */
    /* 切り詰め時も途中までを残す (呼び出し側が部分メッセージを表示する)。 */
    int written = 0;
    switch (parser->register_errors[index].result)
    {
    case CPLAT_ERR_INVALID_ARGUMENT:
        written = snprintf(buffer, buffer_size, "failed to register '%s': invalid argument",
                           target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_OUT_OF_MEMORY:
        written = snprintf(buffer, buffer_size, "failed to register '%s': out of memory",
                           target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_ERR_DUPLICATE_DEFINITION:
        written = snprintf(buffer, buffer_size, "failed to register '%s': duplicate definition",
                           target); /* 置換対象外: 意図的な切り詰め */
        break;
    case CPLAT_OK:
    case CPLAT_ERR_BUFFER_TOO_SMALL:
    default:
        written = snprintf(buffer, buffer_size, "failed to register '%s'", target); /* 置換対象外: 意図的な切り詰め */
        break;
    }

    if (written < 0)
    {
        buffer[0] = '\0';
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((size_t)written >= buffer_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_get_register_error_message(const size_t index, char *buffer, const size_t buffer_size)
{
    return cplat_argparser_handle_get_register_error_message(argparser_default_acquire(0, NULL, NULL, 0), index, buffer,
                                                             buffer_size);
}

int cplat_argparser_handle_print_register_error_messages(const cplat_argparser *parser, FILE *stream)
{
    if (parser == NULL || stream == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    int result = CPLAT_OK;

    for (size_t i = 0; i < parser->register_error_count; i++)
    {
        char message[ARGPARSER_ERROR_MESSAGE_BYTES];
        result = cplat_argparser_handle_get_register_error_message(parser, i, message, sizeof(message));
        if (result == CPLAT_OK || result == CPLAT_ERR_BUFFER_TOO_SMALL)
        {
            fprintf(stream, "error: %s\n", message);
        }
    }
    if (parser->register_error_count > 0)
    {
        fprintf(stream, "\n");
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_argparser_print_register_error_messages(FILE *stream)
{
    return cplat_argparser_handle_print_register_error_messages(argparser_default_acquire(0, NULL, NULL, 0), stream);
}
