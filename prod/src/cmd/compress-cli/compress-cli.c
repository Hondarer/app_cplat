/**
 *******************************************************************************
 *  @file           compress-cli.c
 *  @brief          データを圧縮および展開するコマンドを実装します。
 *
 *  コマンド ライン引数を検証し、ファイル入出力と圧縮 API の結果を終了コードへ変換します。
 *
 *******************************************************************************
 */

#include "compress-cli.h"

#include <cplat/argparser/argparser.h>
#include <cplat/base/error.h>
#include <cplat/base/error_message.h>
#include <cplat/base/platform.h>
#include <cplat/compress/compress.h>
#include <cplat/console/console.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/net/byteorder.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 圧縮 API の処理上限に合わせ、展開後長さの上限は 4 [GiB] - 1 [byte] とする。 */
#define COMPRESS_CLI_MAX_UNCOMPRESSED_SIZE ((size_t)CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE)

/* エラー メッセージの格納に使用するバッファーのバイト数。 */
#define COMPRESS_CLI_ERROR_MESSAGE_SIZE 256

/*
 * 非圧縮時の膨張を見込む出力容量の倍率。
 * 制約: 2 以上とすること (deflateBound() の上界以上を保証するため)。変更時は確保容量だけが変わる。
 */
#define COMPRESS_CLI_CAPACITY_FACTOR 2U

/*
 * 小さい入力に対する圧縮用バッファーの最小容量 [byte]。
 * 制約: 256 以上とすること。変更時は小入力の確保容量だけが変わる。
 */
#define COMPRESS_CLI_MIN_CAPACITY 256U

static_assert(COMPRESS_CLI_CAPACITY_FACTOR >= 2U, "capacity factor must be at least 2");
static_assert(COMPRESS_CLI_MIN_CAPACITY >= 256U, "minimum capacity must be at least 256");

/**
 *  @brief          詳細エラーを人間可読の文字列へ変換します。
 *  @param[in]      error    変換する詳細エラー。
 *  @param[out]     buf      メッセージの格納先。
 *  @param[in]      buf_size @p buf のバイト数。
 *  @return         @p buf を返します。変換できない場合は代替の静的文字列を返します。
 */
static const char *compress_cli_error_text(const cplat_error *error, char *buf, size_t buf_size)
{
    if (cplat_error_message(buf, buf_size, error) != CPLAT_OK)
    {
        return "unknown error";
    }

    return buf;
}

/*
 * ビッグ エンディアンの 64 ビット符号なし整数を読み取り、ホストのバイト オーダーで返す。
 */
static uint64_t compress_cli_read_u64_be(const uint8_t *data)
{
    uint64_t orig_len_nbo;

    memcpy(&orig_len_nbo, data, sizeof(orig_len_nbo));

    return cplat_ntoh64(orig_len_nbo);
}

/*
 * ファイル読み込み失敗時に確保済み資源を解放し、エラー コード -1 を返す。
 */
static int compress_cli_read_file_fail(FILE *file, uint8_t *data)
{
    if (file != NULL)
    {
        (void)fclose(file); /* 読み取り専用であり、すでに -1 を返す経路のため、戻り値は明示的に破棄する */
    }

    cplat_free(data);

    return -1;
}

static int compress_cli_read_file(const char *path, size_t max_size, uint8_t **data_out, size_t *size_out)
{
    FILE *file = NULL;
    uint8_t *data = NULL;
    cplat_error open_error;
    char message[COMPRESS_CLI_ERROR_MESSAGE_SIZE];
    int64_t file_size_i64;
    size_t file_size;
    size_t read_count = 0u;

    if (path == NULL || data_out == NULL || size_out == NULL)
    {
        return -1;
    }

    *data_out = NULL;
    *size_out = 0u;

    /* 診断の出力先がほかにないため、この関数の fprintf の戻り値は明示的に破棄する。 */
    file = cplat_fopen(path, "rb", &open_error);
    if (file == NULL)
    {
        (void)fprintf(stderr, "入力ファイルを開けません: %s (%s)\n", path,
                      compress_cli_error_text(&open_error, message, sizeof(message)));
        return -1;
    }

    /* ファイル末尾へ移動してサイズを取得し、上限以内であることを検証する。 */
    if (cplat_fseek(file, 0, SEEK_END) != 0)
    {
        (void)fprintf(stderr, "入力ファイルのサイズ取得に失敗しました: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    file_size_i64 = cplat_ftell(file);
    if (file_size_i64 < 0)
    {
        (void)fprintf(stderr, "入力ファイルのサイズ取得に失敗しました: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    if (file_size_i64 > (int64_t)max_size)
    {
        (void)fprintf(stderr, "入力ファイルが上限サイズを超えています: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    file_size = (size_t)file_size_i64;

    if (cplat_fseek(file, 0, SEEK_SET) != 0)
    {
        (void)fprintf(stderr, "入力ファイルの先頭へ戻せません: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    /* 空ファイルでない場合は入力バッファーを確保し、全データを読み込む。 */
    if (file_size > 0u)
    {
        data = (uint8_t *)cplat_malloc(file_size);
        if (data == NULL)
        {
            (void)fprintf(stderr, "入力バッファの確保に失敗しました。\n");
            return compress_cli_read_file_fail(file, data);
        }

        read_count = fread(data, 1u, file_size, file);
        if (read_count != file_size)
        {
            (void)fprintf(stderr, "入力ファイルの読み込みに失敗しました: %s\n", path);
            return compress_cli_read_file_fail(file, data);
        }
    }

    if (fclose(file) != 0)
    {
        file = NULL;
        (void)fprintf(stderr, "入力ファイルのクローズに失敗しました: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    *data_out = data;
    *size_out = file_size;

    return 0;
}

static int compress_cli_write_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *file = NULL;
    cplat_error open_error;
    char message[COMPRESS_CLI_ERROR_MESSAGE_SIZE];
    size_t written = 0u;
    int close_rc = 0;

    /* 診断の出力先がほかにないため、この関数の fprintf の戻り値は明示的に破棄する。 */
    file = cplat_fopen(path, "wb", &open_error);
    if (file == NULL)
    {
        (void)fprintf(stderr, "出力ファイルを開けません: %s (%s)\n", path,
                      compress_cli_error_text(&open_error, message, sizeof(message)));
        return -1;
    }

    if (size > 0u)
    {
        written = fwrite(data, 1u, size, file);
        if (written != size)
        {
            (void)fprintf(stderr, "出力ファイルの書き込みに失敗しました: %s\n", path);
            close_rc = fclose(file);
            if (close_rc != 0)
            {
                (void)fprintf(stderr, "出力ファイルのクローズに失敗しました: %s\n", path);
            }

            /*
             * 不完全な出力ファイルを残さないため削除する。
             * 書き込みの失敗をすでに診断して -1 を返す経路であり、削除に失敗しても結果は変わらないため、
             * 戻り値は明示的に破棄する。
             */
            (void)cplat_remove(path, NULL);
            return -1;
        }
    }

    if (fclose(file) != 0)
    {
        (void)fprintf(stderr, "出力ファイルのクローズに失敗しました: %s\n", path);

        /*
         * クローズ失敗時は不完全な可能性があるため削除する。
         * クローズの失敗をすでに診断して -1 を返す経路であり、削除に失敗しても結果は変わらないため、
         * 戻り値は明示的に破棄する。
         */
        (void)cplat_remove(path, NULL);
        return -1;
    }

    return 0;
}

static int compress_cli_resolve_paths(const compress_cli_options *options, char *input_full, size_t input_full_size,
                                      char *output_full, size_t output_full_size)
{
    cplat_error error;
    char message[COMPRESS_CLI_ERROR_MESSAGE_SIZE];
    int path_equal = 0;

    /* 診断の出力先がほかにないため、この関数の fprintf の戻り値は明示的に破棄する。 */

    /* 入力パスと出力パスが同一ファイルを指していないか検証する。 */
    if (cplat_path_equal(options->input_path, options->output_path, &path_equal, &error) != CPLAT_OK)
    {
        (void)fprintf(stderr, "入力パスと出力パスの比較に失敗しました (%s)\n",
                      compress_cli_error_text(&error, message, sizeof(message)));
        return -1;
    }

    if (path_equal != 0)
    {
        (void)fprintf(stderr, "入力ファイルと出力ファイルに同じパスは指定できません。\n");
        return -1;
    }

    /* 入力パスと出力パスを正規化する。 */
    if (cplat_path_get_full(input_full, input_full_size, &error, options->input_path) != CPLAT_OK)
    {
        (void)fprintf(stderr, "入力パスの正規化に失敗しました: %s (%s)\n", options->input_path,
                      compress_cli_error_text(&error, message, sizeof(message)));
        return -1;
    }

    if (cplat_path_get_full(output_full, output_full_size, &error, options->output_path) != CPLAT_OK)
    {
        (void)fprintf(stderr, "出力パスの正規化に失敗しました: %s (%s)\n", options->output_path,
                      compress_cli_error_text(&error, message, sizeof(message)));
        return -1;
    }

    return 0;
}

static int compress_cli_run_compress_return(uint8_t *input_data, uint8_t *compressed_data, int rc)
{
    cplat_free(compressed_data);
    cplat_free(input_data);

    return rc;
}

static int compress_cli_run_compress(const char *input_path, const char *output_path)
{
    uint8_t *input_data = NULL;
    uint8_t *compressed_data = NULL;
    size_t input_size = 0u;
    size_t compressed_capacity;
    size_t compressed_size;
    int rc = -1;

    if (compress_cli_read_file(input_path, COMPRESS_CLI_MAX_UNCOMPRESSED_SIZE, &input_data, &input_size) != 0)
    {
        return -1;
    }

    /* 診断の出力先がほかにないため、この関数の fprintf の戻り値は明示的に破棄する。 */
    if (input_size == 0u)
    {
        (void)fprintf(stderr, "空ファイルは圧縮できません: %s\n", input_path);
        return compress_cli_run_compress_return(input_data, compressed_data, rc);
    }

    /*
     * 出力の容量は、入力長の COMPRESS_CLI_CAPACITY_FACTOR 倍とヘッダー長の和とし、
     * COMPRESS_CLI_MIN_CAPACITY [byte] を下限とする。
     * cplat_compress() と同じ設定 (raw DEFLATE、windowBits 15、memLevel 8) での deflateBound() の上界は、
     * 入力長 n に対して n + (n >> 12) + (n >> 14) + (n >> 25) + 7 [byte] である。
     * ヘッダーを除いた容量は、n が 7 以上では 2n 以上でこの上界以上になり、
     * n が 6 以下では 248 [byte] で上界 (最大 13 [byte]) 以上になる。
     * see: https://zlib.net/manual.html
     */
    compressed_capacity = input_size * COMPRESS_CLI_CAPACITY_FACTOR + CPLAT_COMPRESS_HEADER_SIZE;
    if (compressed_capacity < COMPRESS_CLI_MIN_CAPACITY)
    {
        compressed_capacity = COMPRESS_CLI_MIN_CAPACITY;
    }

    compressed_data = (uint8_t *)cplat_malloc(compressed_capacity);
    if (compressed_data == NULL)
    {
        (void)fprintf(stderr, "圧縮バッファの確保に失敗しました。\n");
        return compress_cli_run_compress_return(input_data, compressed_data, rc);
    }

    compressed_size = compressed_capacity;
    if (cplat_compress(compressed_data, &compressed_size, input_data, input_size) != CPLAT_OK)
    {
        (void)fprintf(stderr, "圧縮に失敗しました: %s\n", input_path);
        return compress_cli_run_compress_return(input_data, compressed_data, rc);
    }

    if (compress_cli_write_file(output_path, compressed_data, compressed_size) != 0)
    {
        return compress_cli_run_compress_return(input_data, compressed_data, rc);
    }

    return compress_cli_run_compress_return(input_data, compressed_data, 0);
}

static int compress_cli_run_decompress_return(uint8_t *input_data, uint8_t *decompressed_data, int rc)
{
    cplat_free(decompressed_data);
    cplat_free(input_data);

    return rc;
}

static int compress_cli_run_decompress(const char *input_path, const char *output_path)
{
    uint8_t *input_data = NULL;
    uint8_t *decompressed_data = NULL;
    size_t input_size = 0u;
    size_t decompressed_size;
    uint64_t expected_size;
    int rc = -1;

    if (compress_cli_read_file(input_path, COMPRESS_CLI_MAX_UNCOMPRESSED_SIZE, &input_data, &input_size) != 0)
    {
        return -1;
    }

    /* 診断の出力先がほかにないため、この関数の fprintf の戻り値は明示的に破棄する。 */

    /* ヘッダー長以下のデータは展開対象として不正なため除外する。 */
    if (input_size <= CPLAT_COMPRESS_HEADER_SIZE)
    {
        (void)fprintf(stderr, "展開入力が不正です。ヘッダーのみ、または空です: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    /* ヘッダーから展開後サイズを読み取り、上限以内であることを検証する。 */
    expected_size = compress_cli_read_u64_be(input_data);
    if (expected_size == 0U)
    {
        (void)fprintf(stderr, "展開入力が不正です。ヘッダーの元サイズが小さすぎます: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    if (expected_size > (uint64_t)COMPRESS_CLI_MAX_UNCOMPRESSED_SIZE)
    {
        (void)fprintf(stderr, "展開入力が不正です。ヘッダーの元サイズが上限を超えています: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    decompressed_data = (uint8_t *)cplat_malloc((size_t)expected_size);
    if (decompressed_data == NULL)
    {
        (void)fprintf(stderr, "展開バッファの確保に失敗しました。\n");
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    decompressed_size = (size_t)expected_size;
    if (cplat_decompress(decompressed_data, &decompressed_size, input_data, input_size) != CPLAT_OK)
    {
        (void)fprintf(stderr, "展開に失敗しました: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    if (decompressed_size != (size_t)expected_size)
    {
        (void)fprintf(stderr, "展開後サイズがヘッダー値と一致しません: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    if (compress_cli_write_file(output_path, decompressed_data, decompressed_size) != 0)
    {
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    return compress_cli_run_decompress_return(input_data, decompressed_data, 0);
}

void compress_cli_options_init(compress_cli_options *options)
{
    if (options == NULL)
    {
        return;
    }

    options->mode = COMPRESS_CLI_MODE_NONE;
    options->need_help = 0;
    options->input_path = NULL;
    options->output_path = NULL;
}

int main(int argc, char *argv[])
{
    compress_cli_options options;
    char input_full[PLATFORM_PATH_UTF8_MAX];
    char output_full[PLATFORM_PATH_UTF8_MAX];

    cplat_console_init();

    compress_cli_options_init(&options);

    int compress_count = 0;
    int decompress_count = 0;

    cplat_argparser_init(argc, argv, "ファイルを圧縮または展開します。");
    /*
     * 登録の失敗はパーサーに記録され、直後の cplat_argparser_get_register_error_count() でまとめて検査するため、
     * 登録の関数の戻り値は明示的に破棄する。
     */
    (void)cplat_argparser_register_flag("-h", "--help", "ヘルプを表示します。", &options.need_help);
    (void)cplat_argparser_register_flag(NULL, "--compress", "入力ファイルを圧縮します。", &compress_count);
    (void)cplat_argparser_register_flag(NULL, "--decompress", "入力ファイルを展開します。", &decompress_count);
    (void)cplat_argparser_register_positional_string("input", "入力ファイル。", CPLAT_ARGPARSER_REQUIRED,
                                                     &options.input_path);
    (void)cplat_argparser_register_positional_string("output", "出力ファイル。", CPLAT_ARGPARSER_REQUIRED,
                                                     &options.output_path);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_register_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    /* ヘルプ要求時は使用法を表示して終了する。 */
    if (options.need_help != 0)
    {
        if (cplat_argparser_print_usage(stdout) != CPLAT_OK || fflush(stdout) != 0 || ferror(stdout) != 0)
        {
            (void)fprintf(stderr, "標準出力へ書き込めませんでした。\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    /* 圧縮モードと展開モードのどちらか一方のみが指定されていることを検証する。 */
    if ((compress_count + decompress_count) != 1)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (compress_count == 1 && decompress_count == 0)
    {
        options.mode = COMPRESS_CLI_MODE_COMPRESS;
    }
    else if (decompress_count == 1 && compress_count == 0)
    {
        options.mode = COMPRESS_CLI_MODE_DECOMPRESS;
    }
    else
    {
        /* 終了コードはすでに失敗に決まっているため、戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (compress_cli_resolve_paths(&options, input_full, sizeof(input_full), output_full, sizeof(output_full)) != 0)
    {
        return EXIT_FAILURE;
    }

    int exit_code = EXIT_SUCCESS;

    /* 指定されたモードに応じて圧縮または展開を実行する。 */
    if (options.mode == COMPRESS_CLI_MODE_COMPRESS)
    {
        if (compress_cli_run_compress(input_full, output_full) != 0)
        {
            exit_code = EXIT_FAILURE;
        }
    }

    if (options.mode == COMPRESS_CLI_MODE_DECOMPRESS)
    {
        if (compress_cli_run_decompress(input_full, output_full) != 0)
        {
            exit_code = EXIT_FAILURE;
        }
    }

    return exit_code;
}
