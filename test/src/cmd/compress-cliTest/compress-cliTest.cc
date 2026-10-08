#include <testfw.h>
#include <mock_stdio.h>
#include <mock_cplat.h>
#include "compress-cli.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

using testing::_;
using testing::DoAll;
using testing::DoDefault;
using testing::HasSubstr;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrEq;

namespace
{

const uint8_t kPlainInput[] = {'a', 'b', 'c', 'd', 'e', 'f'};
const uint8_t kCompressedPayload[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x78, 0x9c, 0x4b};
const uint8_t kDecompressedOutput[] = {'A', 'B', 'C', 'D', 'E', 'F'};
const int64_t kMaxUncompressedSize = (int64_t)UINT32_MAX;

static int return_full_path(char *path_out, size_t path_size, cplat_error *detail_out, const char *text)
{
    size_t len;

    cplat_error_clear(detail_out);

    if (path_out == nullptr || text == nullptr)
    {
        return -1;
    }

    len = std::strlen(text);
    if (len + 1u > path_size)
    {
        return -1;
    }

    std::memcpy(path_out, text, len + 1u);
    return 0;
}

} // namespace

class compress_cliTest : public Test
{
  protected:
    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;
};

// 未対応オプション指定時に main() が失敗終了することの確認
TEST_F(compress_cliTest, main_rejects_unknown_option)
{
    // Arrange
    const char *argv[] = {
        "compress-cli", "--unknown", "input.bin",
        "output.bin"}; // [状態] - main() に与える引数を未対応オプション "--unknown" を含む 4 つとする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。

    // Act
    int rc = __real_main(4, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// help オプション指定時に usage を表示して正常終了することの確認
TEST_F(compress_cliTest, main_prints_usage_on_help)
{
    // Arrange
    const char *argv[] = {"compress-cli", "--help"}; // [状態] - main() に与える引数を "--help" のみとする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。

    // Act
    int rc = __real_main(2, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, rc); // [確認_正常系] - 必須位置引数がない場合も main() の戻り値が EXIT_SUCCESS であること。
}

// 正規化後に同一となる入出力パスが拒否されることの確認
TEST_F(compress_cliTest, main_rejects_same_normalized_path)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {
        "compress-cli", "--compress", "./data.bin",
        "subdir/../data.bin"}; // [状態] - 入出力パスを正規化すると同一になる "./data.bin" と "subdir/../data.bin" とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("./data.bin"), StrEq("subdir/../data.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(1),
                  Return(CPLAT_OK))); // [Pre-Assert確認_異常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 1 (同一) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 同一パス検出時は cplat_path_get_full が呼び出されないこと。
    EXPECT_CALL(mock_cplat_, cplat_fopen(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 同一パス検出時は cplat_fopen が呼び出されないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("同じパス")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "同じパス" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// 圧縮入力が非圧縮データの上限を超える場合に内容を読み込まず失敗終了することの確認
TEST_F(compress_cliTest, main_rejects_compress_input_over_size_limit_before_reading)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--compress", "input.bin",
                          "output.bin"}; // [状態] - 圧縮モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x1000;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return(kMaxUncompressedSize +
                         1)); // [Pre-Assert手順] - 入力ファイルのサイズとして上限より 1 byte 大きい値を返却する。
    // [Pre-Assert確認_異常系] - mock_cplat_ の cplat_ftell(input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .Times(0); // [Pre-Assert確認_異常系] - 上限超過時は入力ファイルの先頭へ戻す処理が呼び出されないこと。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 上限超過時は入力ファイルの内容が読み込まれないこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 上限超過時に入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_compress(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 上限超過時は cplat_compress が呼び出されないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("上限サイズ")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "上限サイズ" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// 圧縮モードで入力が読み込まれ圧縮結果が出力ファイルへ書き込まれることの確認
TEST_F(compress_cliTest, main_compresses_input_and_writes_output)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--compress", "input.bin",
                          "output.bin"}; // [状態] - 圧縮モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x1010;
    FILE *output_file = (FILE *)(uintptr_t)0x2020;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    // [Pre-Assert確認_正常系] - mock_cplat_ の cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    // [Pre-Assert確認_正常系] - mock_cplat_ の cplat_path_get_full(_, _, _, StrEq("input.bin")) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    // [Pre-Assert確認_正常系] - mock_cplat_ の cplat_path_get_full(_, _, _, StrEq("output.bin")) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return(
            (int64_t)sizeof(kPlainInput))); // [Pre-Assert確認_正常系] - cplat_ftell が入力ファイルのサイズを返すこと。
                                            // [Pre-Assert手順] - 入力ファイルのサイズとして 6 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの先頭へ戻すこと。
                              // [Pre-Assert手順] - 入力ファイル先頭への移動として 0 を返却する。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, 1u, sizeof(kPlainInput), input_file))
        .WillOnce(
            [](const char *, const int, const char *, void *ptr, size_t, size_t count, FILE *)
            {
                std::memcpy(ptr, kPlainInput, sizeof(kPlainInput));
                return count;
            }); // [Pre-Assert手順] - 入力ファイルの読み込みで平文データ 6 byte を返却する。
    // [Pre-Assert確認_正常系] - mock_stdio_ の fread(_, _, _, _, 1u, sizeof(kPlainInput), input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_compress(_, _, _, sizeof(kPlainInput)))
        .WillOnce(
            [](uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t)
            {
                EXPECT_EQ(
                    0,
                    std::memcmp(
                        src, kPlainInput,
                        sizeof(
                            kPlainInput))); // [Pre-Assert確認_正常系] - 圧縮 API に読み込んだ平文データがそのまま渡ること。
                EXPECT_GE(*dst_len, sizeof(kCompressedPayload));
                // [確認_正常系] - `*dst_len` が `sizeof(kCompressedPayload)` 以上であること。
                std::memcpy(dst, kCompressedPayload, sizeof(kCompressedPayload));
                *dst_len = sizeof(kCompressedPayload);
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_compress が入力サイズ 6 byte で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_compress から圧縮済みデータ 7 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/output.bin"), StrEq("wb"), _))
        .WillOnce(Return(output_file)); // [Pre-Assert確認_正常系] - 出力ファイルがモード "wb" で開かれること。
    EXPECT_CALL(mock_stdio_, fwrite(_, _, _, _, 1u, sizeof(kCompressedPayload), output_file))
        .WillOnce(
            [](const char *, const int, const char *, const void *ptr, size_t, size_t count, FILE *)
            {
                EXPECT_EQ(
                    0,
                    std::memcmp(
                        ptr, kCompressedPayload,
                        sizeof(
                            kCompressedPayload))); // [Pre-Assert確認_正常系] - 出力ファイルへ圧縮済みデータが書き込まれること。
                return count;
            });
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, output_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 出力ファイルが fclose されること。
                              // [Pre-Assert手順] - 出力ファイルの fclose から 0 を返却する。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, rc); // [確認_正常系] - main() の戻り値が EXIT_SUCCESS であること。
}

// 展開時にヘッダーの元サイズが 0 の入力が拒否されることの確認
TEST_F(compress_cliTest, main_rejects_decompress_input_when_original_size_is_zero)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--decompress", "input.bin",
                          "output.bin"}; // [状態] - 展開モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x3030;
    const uint8_t invalid_input[] = {
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x10}; // [状態] - ヘッダーの元サイズが 0 byte の不正入力 9 byte を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return((int64_t)sizeof(
            invalid_input))); // [Pre-Assert確認_正常系] - cplat_ftell が入力ファイルのサイズを返すこと。
                              // [Pre-Assert手順] - 入力ファイルのサイズとして 9 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの先頭へ戻すこと。
                              // [Pre-Assert手順] - 入力ファイル先頭への移動として 0 を返却する。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, 1u, sizeof(invalid_input), input_file))
        .WillOnce(
            [&](const char *, const int, const char *, void *ptr, size_t, size_t count, FILE *)
            {
                std::memcpy(ptr, invalid_input, sizeof(invalid_input));
                return count;
            }); // [Pre-Assert手順] - 入力ファイルの読み込みで不正入力 9 byte を返却する。
    // [Pre-Assert確認_異常系] - mock_stdio_ の fread(_, _, _, _, 1u, sizeof(invalid_input), input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - ヘッダー不正時に入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_decompress(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ヘッダー不正時は cplat_decompress が呼び出されないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("ヘッダーの元サイズ")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "ヘッダーの元サイズ" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// 展開時にヘッダーの元サイズが上限を超える入力がバッファー確保前に拒否されることの確認
TEST_F(compress_cliTest, main_rejects_decompress_input_when_original_size_exceeds_limit)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--decompress", "input.bin",
                          "output.bin"}; // [状態] - 展開モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x3034;
    const uint8_t invalid_input[] = {
        0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x10}; // [状態] - ヘッダーの元サイズが 4 GiB の不正入力 9 byte を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return((int64_t)sizeof(
            invalid_input))); // [Pre-Assert確認_正常系] - cplat_ftell が入力ファイルのサイズを返すこと。
                              // [Pre-Assert手順] - 入力ファイルのサイズとして 9 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの先頭へ戻すこと。
                              // [Pre-Assert手順] - 入力ファイル先頭への移動として 0 を返却する。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, 1u, sizeof(invalid_input), input_file))
        .WillOnce(
            [&](const char *, const int, const char *, void *ptr, size_t, size_t count, FILE *)
            {
                std::memcpy(ptr, invalid_input, sizeof(invalid_input));
                return count;
            }); // [Pre-Assert手順] - 入力ファイルの読み込みで元サイズが上限を超える入力 9 byte を返却する。
    // [Pre-Assert確認_異常系] - mock_stdio_ の fread(_, _, _, _, 1u, sizeof(invalid_input), input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 元サイズの上限超過時に入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_decompress(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 元サイズの上限超過時は cplat_decompress が呼び出されないこと。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/output.bin"), StrEq("wb"), _))
        .Times(0); // [Pre-Assert確認_異常系] - 元サイズの上限超過時は出力ファイルが開かれないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("上限を超えています")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "上限を超えています" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// 展開時にヘッダーの元サイズが上限ちょうどの入力がバッファー確保へ進み失敗することの確認
TEST_F(compress_cliTest, main_rejects_decompress_when_max_original_size_allocation_fails)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--decompress", "input.bin",
                          "output.bin"}; // [状態] - 展開モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x3031;
    const uint8_t max_size_input[] = {
        0x00, 0x00, 0x00, 0x00, 0xFF,
        0xFF, 0xFF, 0xFF, 0x10}; // [状態] - ヘッダーの元サイズが UINT32_MAX の入力 9 byte を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return((int64_t)sizeof(
            max_size_input))); // [Pre-Assert確認_正常系] - cplat_ftell が入力ファイルのサイズを返すこと。
                               // [Pre-Assert手順] - 入力ファイルのサイズとして 9 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの先頭へ戻すこと。
                              // [Pre-Assert手順] - 入力ファイル先頭への移動として 0 を返却する。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, 1u, sizeof(max_size_input), input_file))
        .WillOnce(
            [&](const char *, const int, const char *, void *ptr, size_t, size_t count, FILE *)
            {
                std::memcpy(ptr, max_size_input, sizeof(max_size_input));
                return count;
            }); // [Pre-Assert手順] - 入力ファイルの読み込みで元サイズが UINT32_MAX の入力 9 byte を返却する。
    // [Pre-Assert確認_異常系] - mock_stdio_ の fread(_, _, _, _, 1u, sizeof(max_size_input), input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - バッファー確保失敗時に入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(
            [](size_t size)
            {
                EXPECT_EQ((size_t)UINT32_MAX,
                          size); // [Pre-Assert確認_異常系] - 展開バッファー確保が UINT32_MAX byte で呼び出されること。
                return nullptr;
            }); // [Pre-Assert手順] - 1 回目の cplat_malloc は入力バッファー用に実体へ委譲し、2 回目は NULL を返却する。
    EXPECT_CALL(mock_cplat_, cplat_decompress(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - バッファー確保失敗時は cplat_decompress が呼び出されないこと。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/output.bin"), StrEq("wb"), _))
        .Times(0); // [Pre-Assert確認_異常系] - バッファー確保失敗時は出力ファイルが開かれないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("展開バッファの確保に失敗")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "展開バッファの確保に失敗" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// 展開入力の生ファイルが上限サイズを超える場合に内容を読み込まず失敗終了することの確認
TEST_F(compress_cliTest, main_rejects_decompress_input_when_raw_file_exceeds_size_limit_before_reading)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--decompress", "input.bin",
                          "output.bin"}; // [状態] - 展開モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x1000;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return(kMaxUncompressedSize +
                         1)); // [Pre-Assert手順] - 入力ファイルのサイズとして上限より 1 byte 大きい値を返却する。
    // [Pre-Assert確認_異常系] - mock_cplat_ の cplat_ftell(input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .Times(0); // [Pre-Assert確認_異常系] - 上限超過時は入力ファイルの先頭へ戻す処理が呼び出されないこと。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 上限超過時は入力ファイルの内容が読み込まれないこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 上限超過時に入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_decompress(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 上限超過時は cplat_decompress が呼び出されないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("上限サイズ")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "上限サイズ" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// 展開時にヘッダーの元サイズが 1 byte の入力を展開して出力できることの確認
TEST_F(compress_cliTest, main_decompresses_one_byte_input)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--decompress", "input.bin",
                          "output.bin"}; // [状態] - 展開モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x3032;
    FILE *output_file = (FILE *)(uintptr_t)0x3033;
    const uint8_t compressed_input[] = {
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x10}; // [状態] - ヘッダーの元サイズが 1 byte の圧縮入力 9 byte を用意する。
    const uint8_t decompressed_output[] = {'A'};

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return((int64_t)sizeof(
            compressed_input))); // [Pre-Assert確認_正常系] - cplat_ftell が入力ファイルのサイズを返すこと。
                                 // [Pre-Assert手順] - 入力ファイルのサイズとして 9 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの先頭へ戻すこと。
                              // [Pre-Assert手順] - 入力ファイル先頭への移動として 0 を返却する。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, 1u, sizeof(compressed_input), input_file))
        .WillOnce(
            [&](const char *, const int, const char *, void *ptr, size_t, size_t count, FILE *)
            {
                std::memcpy(ptr, compressed_input, sizeof(compressed_input));
                return count;
            }); // [Pre-Assert手順] - 入力ファイルの読み込みで元サイズが 1 byte の圧縮入力を返却する。
    // [Pre-Assert確認_正常系] - mock_stdio_ の fread(_, _, _, _, 1u, sizeof(compressed_input), input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_decompress(_, _, _, sizeof(compressed_input)))
        .WillOnce(
            [&](uint8_t *dst, size_t *dst_len, const uint8_t *, size_t)
            {
                EXPECT_EQ(
                    (size_t)1u,
                    *dst_len); // [Pre-Assert確認_正常系] - 展開 API にヘッダー値 1 byte が出力サイズとして渡ること。
                std::memcpy(dst, decompressed_output, sizeof(decompressed_output));
                *dst_len = sizeof(decompressed_output);
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_decompress が入力サイズ 9 byte で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_decompress から展開済みデータ 1 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/output.bin"), StrEq("wb"), _))
        .WillOnce(
            Return(output_file)); // [Pre-Assert確認_正常系] - 展開成功時は出力ファイルがモード "wb" で開かれること。
    EXPECT_CALL(mock_stdio_, fwrite(_, _, _, _, 1u, sizeof(decompressed_output), output_file))
        .WillOnce(
            [&](const char *, const int, const char *, const void *ptr, size_t, size_t count, FILE *)
            {
                EXPECT_EQ(
                    0,
                    std::memcmp(
                        ptr, decompressed_output,
                        sizeof(
                            decompressed_output))); // [Pre-Assert確認_正常系] - 展開済みデータ 1 byte が出力されること。
                return count;
            });
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, output_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 出力ファイルが fclose されること。
                              // [Pre-Assert手順] - 出力ファイルの fclose から 0 を返却する。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, rc); // [確認_正常系] - main() の戻り値が EXIT_SUCCESS であること。
}

// 展開後サイズがヘッダー値と一致しない場合に出力せず失敗終了することの確認
TEST_F(compress_cliTest, main_rejects_decompress_output_when_size_mismatches_header)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--decompress", "input.bin",
                          "output.bin"}; // [状態] - 展開モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x4040;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return((int64_t)sizeof(
            kCompressedPayload))); // [Pre-Assert確認_正常系] - cplat_ftell が入力ファイルのサイズを返すこと。
                                   // [Pre-Assert手順] - 入力ファイルのサイズとして 7 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの先頭へ戻すこと。
                              // [Pre-Assert手順] - 入力ファイル先頭への移動として 0 を返却する。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, 1u, sizeof(kCompressedPayload), input_file))
        .WillOnce(
            [](const char *, const int, const char *, void *ptr, size_t, size_t count, FILE *)
            {
                std::memcpy(ptr, kCompressedPayload, sizeof(kCompressedPayload));
                return count;
            }); // [Pre-Assert手順] - 入力ファイルの読み込みで圧縮済みデータ 7 byte を返却する。
    // [Pre-Assert確認_異常系] - mock_stdio_ の fread(_, _, _, _, 1u, sizeof(kCompressedPayload), input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - サイズ不一致時に入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_decompress(_, _, _, sizeof(kCompressedPayload)))
        .WillOnce(
            [](uint8_t *dst, size_t *dst_len, const uint8_t *, size_t)
            {
                EXPECT_EQ(
                    (size_t)6u,
                    *dst_len); // [Pre-Assert確認_正常系] - 展開 API にヘッダー値 6 byte が出力サイズとして渡ること。
                std::memcpy(dst, kDecompressedOutput, sizeof(kDecompressedOutput));
                *dst_len = sizeof(kDecompressedOutput) - 1u;
                return 0;
            }); // [Pre-Assert確認_異常系] - cplat_decompress が 1 回呼び出されること。
                // [Pre-Assert手順] - 展開後サイズをヘッダー値より 1 byte 少ない 5 byte に設定して返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/output.bin"), StrEq("wb"), _))
        .Times(0); // [Pre-Assert確認_異常系] - サイズ不一致時は出力ファイルが開かれないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("展開後サイズがヘッダー値と一致")))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - "展開後サイズがヘッダー値と一致" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// 出力の書き込みが失敗した場合に部分出力ファイルが削除されることの確認
TEST_F(compress_cliTest, main_removes_partial_output_when_write_fails)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--decompress", "input.bin",
                          "output.bin"}; // [状態] - 展開モードで入力 "input.bin"、出力 "output.bin" とする。
    FILE *input_file = (FILE *)(uintptr_t)0x5050;
    FILE *output_file = (FILE *)(uintptr_t)0x6060;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(
            DoAll(SetArgPointee<2>(0),
                  Return(CPLAT_OK))); // [Pre-Assert確認_正常系] - cplat_path_equal で入出力パスの比較が行われること。
                                      // [Pre-Assert手順] - equal_out に 0 (不一致) を設定して CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/input.bin");
            }); // [Pre-Assert確認_正常系] - 入力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 入力パスの正規化結果として "/tmp/input.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                return return_full_path(path_out, path_size, detail_out, "/tmp/output.bin");
            }); // [Pre-Assert確認_正常系] - 出力パスの cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 出力パスの正規化結果として "/tmp/output.bin" を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/input.bin"), StrEq("rb"), _))
        .WillOnce(Return(input_file)); // [Pre-Assert確認_正常系] - 入力ファイルがモード "rb" で開かれること。
                                       // [Pre-Assert手順] - 入力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_END))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの末尾へ移動すること。
                              // [Pre-Assert手順] - 入力ファイル末尾への移動として 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_ftell(input_file))
        .WillOnce(Return((int64_t)sizeof(
            kCompressedPayload))); // [Pre-Assert確認_正常系] - cplat_ftell が入力ファイルのサイズを返すこと。
                                   // [Pre-Assert手順] - 入力ファイルのサイズとして 7 byte を返却する。
    EXPECT_CALL(mock_cplat_, cplat_fseek(input_file, 0, SEEK_SET))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_fseek が入力ファイルの先頭へ戻すこと。
                              // [Pre-Assert手順] - 入力ファイル先頭への移動として 0 を返却する。
    EXPECT_CALL(mock_stdio_, fread(_, _, _, _, 1u, sizeof(kCompressedPayload), input_file))
        .WillOnce(
            [](const char *, const int, const char *, void *ptr, size_t, size_t count, FILE *)
            {
                std::memcpy(ptr, kCompressedPayload, sizeof(kCompressedPayload));
                return count;
            }); // [Pre-Assert手順] - 入力ファイルの読み込みで圧縮済みデータ 7 byte を返却する。
    // [Pre-Assert確認_異常系] - mock_stdio_ の fread(_, _, _, _, 1u, sizeof(kCompressedPayload), input_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, input_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 入力ファイルが fclose されること。
                              // [Pre-Assert手順] - 入力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_decompress(_, _, _, sizeof(kCompressedPayload)))
        .WillOnce(
            [](uint8_t *dst, size_t *dst_len, const uint8_t *, size_t)
            {
                std::memcpy(dst, kDecompressedOutput, sizeof(kDecompressedOutput));
                *dst_len = sizeof(kDecompressedOutput);
                return 0;
            }); // [Pre-Assert手順] - cplat_decompress から展開済みデータ 6 byte を返却する。
    // [Pre-Assert確認_異常系] - mock_cplat_ の cplat_decompress(_, _, _, sizeof(kCompressedPayload)) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("/tmp/output.bin"), StrEq("wb"), _))
        .WillOnce(Return(output_file)); // [Pre-Assert確認_正常系] - 出力ファイルがモード "wb" で開かれること。
                                        // [Pre-Assert手順] - 出力ファイルのハンドルを返却する。
    EXPECT_CALL(mock_stdio_, fwrite(_, _, _, _, 1u, sizeof(kDecompressedOutput), output_file))
        .WillOnce(Return(
            sizeof(kDecompressedOutput) -
            1u)); // [Pre-Assert手順] - 出力ファイルの書き込みで要求より 1 byte 少ない 5 byte を返却し、書き込み失敗を発生させる。
    // [Pre-Assert確認_異常系] - mock_stdio_ の fwrite(_, _, _, _, 1u, sizeof(kDecompressedOutput), output_file) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, output_file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 書き込み失敗時に出力ファイルが fclose されること。
                              // [Pre-Assert手順] - 出力ファイルの fclose から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_remove(StrEq("/tmp/output.bin"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 部分出力ファイル "/tmp/output.bin" が削除されること。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("書き込みに失敗")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "書き込みに失敗" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}

// パス比較 API が失敗した場合にファイルを開かず失敗終了することの確認
TEST_F(compress_cliTest, main_fails_when_path_comparison_fails)
{
    // Arrange
    int argc = 4;
    const char *argv[] = {"compress-cli", "--compress", "input.bin",
                          "output.bin"}; // [状態] - 圧縮モードで入力 "input.bin"、出力 "output.bin" とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_path_equal(StrEq("input.bin"), StrEq("output.bin"), _, _))
        .WillOnce(DoAll(
            SetArgPointee<3>(cplat_error{CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_UNKNOWN, EIO}),
            Return(
                CPLAT_ERR_UNKNOWN))); // [Pre-Assert確認_異常系] - cplat_path_equal で入出力パスの比較が行われること。
    // [Pre-Assert手順] - detail_out に EIO を設定して CPLAT_ERR_UNKNOWN を返却する。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 比較失敗時は cplat_path_get_full が呼び出されないこと。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("input.bin")))
        .Times(0); // [Pre-Assert確認_異常系] - 比較失敗時は入力パスの cplat_path_get_full が呼び出されないこと。
    EXPECT_CALL(mock_cplat_, cplat_path_get_full(_, _, _, StrEq("output.bin")))
        .Times(0); // [Pre-Assert確認_異常系] - 比較失敗時は出力パスの cplat_path_get_full が呼び出されないこと。
    EXPECT_CALL(mock_cplat_, cplat_fopen(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 比較失敗時は cplat_fopen が呼び出されないこと。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("比較に失敗")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - "比較に失敗" を含むエラーが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - main() の戻り値が EXIT_SUCCESS 以外であること。
}
