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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 圧縮 API の処理上限に合わせ、展開後長さの上限は 4 GiB - 1 byte とする。 */
#define COMPRESS_CLI_MAX_UNCOMPRESSED_SIZE ((size_t)CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE)

/* エラー メッセージの格納に使用するバッファーのバイト数。 */
#define COMPRESS_CLI_ERROR_MESSAGE_SIZE 256

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

static uint64_t compress_cli_read_u64_be(const uint8_t *data)
{
    uint64_t orig_len_nbo;

    memcpy(&orig_len_nbo, data, sizeof(orig_len_nbo));
    return cplat_ntoh64(orig_len_nbo);
}

static int compress_cli_read_file_fail(FILE *file, uint8_t *data)
{
    if (file != NULL)
    {
        (void)fclose(file);
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

    file = cplat_fopen(path, "rb", &open_error);
    if (file == NULL)
    {
        fprintf(stderr, "入力ファイルを開けません: %s (%s)\n", path,
                compress_cli_error_text(&open_error, message, sizeof(message)));
        return -1;
    }

    if (cplat_fseek(file, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "入力ファイルのサイズ取得に失敗しました: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    file_size_i64 = cplat_ftell(file);
    if (file_size_i64 < 0)
    {
        fprintf(stderr, "入力ファイルのサイズ取得に失敗しました: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    if (file_size_i64 > (int64_t)max_size)
    {
        fprintf(stderr, "入力ファイルが上限サイズを超えています: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    file_size = (size_t)file_size_i64;

    if (cplat_fseek(file, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "入力ファイルの先頭へ戻せません: %s\n", path);
        return compress_cli_read_file_fail(file, data);
    }

    if (file_size > 0u)
    {
        data = (uint8_t *)cplat_malloc(file_size);
        if (data == NULL)
        {
            fprintf(stderr, "入力バッファの確保に失敗しました。\n");
            return compress_cli_read_file_fail(file, data);
        }

        read_count = fread(data, 1u, file_size, file);
        if (read_count != file_size)
        {
            fprintf(stderr, "入力ファイルの読み込みに失敗しました: %s\n", path);
            return compress_cli_read_file_fail(file, data);
        }
    }

    if (fclose(file) != 0)
    {
        file = NULL;
        fprintf(stderr, "入力ファイルのクローズに失敗しました: %s\n", path);
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

    file = cplat_fopen(path, "wb", &open_error);
    if (file == NULL)
    {
        fprintf(stderr, "出力ファイルを開けません: %s (%s)\n", path,
                compress_cli_error_text(&open_error, message, sizeof(message)));
        return -1;
    }

    if (size > 0u)
    {
        written = fwrite(data, 1u, size, file);
        if (written != size)
        {
            fprintf(stderr, "出力ファイルの書き込みに失敗しました: %s\n", path);
            close_rc = fclose(file);
            if (close_rc != 0)
            {
                fprintf(stderr, "出力ファイルのクローズに失敗しました: %s\n", path);
            }
            (void)cplat_remove(path, NULL);
            return -1;
        }
    }

    if (fclose(file) != 0)
    {
        fprintf(stderr, "出力ファイルのクローズに失敗しました: %s\n", path);
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

    if (cplat_path_equal(options->input_path, options->output_path, &path_equal, &error) != CPLAT_OK)
    {
        fprintf(stderr, "入力パスと出力パスの比較に失敗しました (%s)\n",
                compress_cli_error_text(&error, message, sizeof(message)));
        return -1;
    }

    if (path_equal != 0)
    {
        fprintf(stderr, "入力ファイルと出力ファイルに同じパスは指定できません。\n");
        return -1;
    }

    if (cplat_path_get_full(input_full, input_full_size, &error, options->input_path) != CPLAT_OK)
    {
        fprintf(stderr, "入力パスの正規化に失敗しました: %s (%s)\n", options->input_path,
                compress_cli_error_text(&error, message, sizeof(message)));
        return -1;
    }

    if (cplat_path_get_full(output_full, output_full_size, &error, options->output_path) != CPLAT_OK)
    {
        fprintf(stderr, "出力パスの正規化に失敗しました: %s (%s)\n", options->output_path,
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

    if (input_size == 0u)
    {
        fprintf(stderr, "空ファイルは圧縮できません: %s\n", input_path);
        return compress_cli_run_compress_return(input_data, compressed_data, rc);
    }

    compressed_capacity = input_size * 2u + CPLAT_COMPRESS_HEADER_SIZE;
    if (compressed_capacity < 256u)
    {
        compressed_capacity = 256u;
    }

    compressed_data = (uint8_t *)cplat_malloc(compressed_capacity);
    if (compressed_data == NULL)
    {
        fprintf(stderr, "圧縮バッファの確保に失敗しました。\n");
        return compress_cli_run_compress_return(input_data, compressed_data, rc);
    }

    compressed_size = compressed_capacity;
    if (cplat_compress(compressed_data, &compressed_size, input_data, input_size) != CPLAT_OK)
    {
        fprintf(stderr, "圧縮に失敗しました: %s\n", input_path);
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

    if (input_size <= CPLAT_COMPRESS_HEADER_SIZE)
    {
        fprintf(stderr, "展開入力が不正です。ヘッダーのみ、または空です: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    expected_size = compress_cli_read_u64_be(input_data);
    if (expected_size == 0U)
    {
        fprintf(stderr, "展開入力が不正です。ヘッダーの元サイズが小さすぎます: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    if (expected_size > (uint64_t)COMPRESS_CLI_MAX_UNCOMPRESSED_SIZE)
    {
        fprintf(stderr, "展開入力が不正です。ヘッダーの元サイズが上限を超えています: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    decompressed_data = (uint8_t *)cplat_malloc((size_t)expected_size);
    if (decompressed_data == NULL)
    {
        fprintf(stderr, "展開バッファの確保に失敗しました。\n");
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    decompressed_size = (size_t)expected_size;
    if (cplat_decompress(decompressed_data, &decompressed_size, input_data, input_size) != CPLAT_OK)
    {
        fprintf(stderr, "展開に失敗しました: %s\n", input_path);
        return compress_cli_run_decompress_return(input_data, decompressed_data, rc);
    }

    if (decompressed_size != (size_t)expected_size)
    {
        fprintf(stderr, "展開後サイズがヘッダー値と一致しません: %s\n", input_path);
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
    char input_full[PLATFORM_PATH_MAX];
    char output_full[PLATFORM_PATH_MAX];

    cplat_console_init();

    compress_cli_options_init(&options);

    int compress_count = 0;
    int decompress_count = 0;

    cplat_argparser_init(argc, argv, "ファイルを圧縮または展開します。");
    cplat_argparser_register_flag("-h", "--help", "ヘルプを表示します。", &options.need_help);
    cplat_argparser_register_flag(NULL, "--compress", "入力ファイルを圧縮します。", &compress_count);
    cplat_argparser_register_flag(NULL, "--decompress", "入力ファイルを展開します。", &decompress_count);
    cplat_argparser_register_positional_string("input", "入力ファイル。", CPLAT_ARGPARSER_REQUIRED,
                                               &options.input_path);
    cplat_argparser_register_positional_string("output", "出力ファイル。", CPLAT_ARGPARSER_REQUIRED,
                                               &options.output_path);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        cplat_argparser_print_register_error_messages(stderr);
        cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    if (options.need_help != 0)
    {
        cplat_argparser_print_usage(stdout);
        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        cplat_argparser_print_error_messages(stderr);
        cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if ((compress_count + decompress_count) != 1)
    {
        cplat_argparser_print_error_messages(stderr);
        cplat_argparser_print_usage(stderr);
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
        cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (compress_cli_resolve_paths(&options, input_full, sizeof(input_full), output_full, sizeof(output_full)) != 0)
    {
        return EXIT_FAILURE;
    }

    int exit_code = EXIT_SUCCESS;
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
