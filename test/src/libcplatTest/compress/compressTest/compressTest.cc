#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/compress/compress.h>
#include <mock_zlib.h>

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

class compressTest : public Test
{
  protected:
    /* 圧縮しやすいように同じ並びを繰り返す平文 */
    static std::string make_plain_text()
    {
        std::string text;

        for (int i = 0; i < 64; ++i)
        {
            text += "cplat compress round trip ";
        }

        return text;
    }
};

// 圧縮した結果を展開すると元のデータへ戻ることの確認
TEST_F(compressTest, round_trip_restores_original_bytes)
{
    // Arrange
    std::string plain = make_plain_text(); // [状態] - 繰り返しを含む平文を用意する。
    std::vector<uint8_t> compressed(plain.size() + CPLAT_COMPRESS_HEADER_SIZE + 64u);
    std::vector<uint8_t> restored(plain.size() + 1u);
    size_t compressed_len = compressed.size();
    size_t restored_len = restored.size();

    // Pre-Assert

    // Act
    int actual_ret_compress =
        cplat_compress(compressed.data(), &compressed_len, reinterpret_cast<const uint8_t *>(plain.data()),
                       plain.size()); // [手順] - 平文を cplat_compress で圧縮する。
    int actual_ret_decompress = cplat_decompress(restored.data(), &restored_len, compressed.data(),
                                                 compressed_len); // [手順] - 圧縮結果を cplat_decompress で展開する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_compress);   // [確認_正常系] - cplat_compress() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_decompress); // [確認_正常系] - cplat_decompress() の戻り値が CPLAT_OK であること。
    EXPECT_GT(
        plain.size(),
        compressed_len); // [確認_正常系] - plain.size() の戻り値が compressed_len より大きいこと。(圧縮後のサイズが元のサイズより小さいことの確認)
    EXPECT_EQ(
        plain.size(),
        restored_len); // [確認_正常系] - restored_len が plain.size() の戻り値と一致すること。(展開後のサイズが元のサイズと一致することの確認)
    EXPECT_EQ(
        0,
        memcmp(
            plain.data(), restored.data(),
            restored_len)); // [確認_正常系] - memcmp() の戻り値が 0 であること。(展開後の内容が元のデータと一致することの確認)
}

// cplat_compress が不正な引数を拒否することの確認
TEST_F(compressTest, compress_rejects_invalid_arguments)
{
    // Arrange
    uint8_t dst[64];
    const uint8_t src[] = "data";
    size_t dst_len = sizeof(dst); // [状態] - 64 byte の出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_dst =
        cplat_compress(NULL, &dst_len, src, sizeof(src)); // [手順] - dst に NULL を指定して呼び出す。
    int actual_ret_null_dst_len =
        cplat_compress(dst, NULL, src, sizeof(src)); // [手順] - dst_len に NULL を指定して呼び出す。
    int actual_ret_null_src =
        cplat_compress(dst, &dst_len, NULL, sizeof(src));                 // [手順] - src に NULL を指定して呼び出す。
    int actual_ret_zero_src_len = cplat_compress(dst, &dst_len, src, 0u); // [手順] - src_len に 0 を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst); // [確認_異常系] - dst が NULL のとき cplat_compress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst_len); // [確認_異常系] - dst_len が NULL のとき cplat_compress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_src); // [確認_異常系] - src が NULL のとき cplat_compress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero_src_len); // [確認_異常系] - src_len が 0 のとき cplat_compress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 圧縮前サイズが 4 GiB 以上の場合に上限超過を通知することの確認
// cplat-req: id=CPLAT-COMPRESS-FUNC-003; uuid=a6d4c8e1-3b27-4f9a-8e05-1c7b9d2f4a60
TEST_F(compressTest, compress_returns_limit_exceeded_when_src_len_exceeds_max)
{
    // Arrange
    uint8_t dst[64];
    const uint8_t src[] = "data";
    size_t dst_len = sizeof(dst); // [状態] - 64 byte の出力バッファーを用意する。
    size_t too_large_src_len = (size_t)CPLAT_COMPRESS_MAX_UNCOMPRESSED_SIZE +
                               1u; // [状態] - 処理上限を 1 byte 超える入力サイズを用意する。実データは確保しない。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_compress(dst, &dst_len, src,
                       too_large_src_len); // [手順] - src_len が処理上限を超える値で cplat_compress を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_LIMIT_EXCEEDED,
        actual_ret); // [確認_異常系] - src_len が処理上限を超えるとき cplat_compress() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
}

// cplat_compress は zlib の avail_out が uInt を超えるとき deflate を繰り返す。
// その継続条件は 4 GiB 超の出力バッファーが必要なため、単体テストでは到達できない。

// ヘッダーと最低 1 byte を収められない出力バッファーが拒否されることの確認
TEST_F(compressTest, compress_returns_buffer_too_small_for_header_only_buffer)
{
    // Arrange
    uint8_t dst[CPLAT_COMPRESS_HEADER_SIZE];
    const uint8_t src[] = "data";
    size_t dst_len = sizeof(dst); // [状態] - ヘッダー分ちょうどの出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_compress(dst, &dst_len, src,
                       sizeof(src)); // [手順] - ヘッダー分しかない出力バッファーで cplat_compress を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_compress() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

// 圧縮結果が収まらない出力バッファーで圧縮が失敗することの確認
TEST_F(compressTest, compress_returns_unknown_when_output_does_not_fit)
{
    // Arrange
    std::string plain(4096u, 'x');
    // ヘッダーの最低容量を満たし、圧縮結果が収まらない容量を選ぶ。
    uint8_t dst[CPLAT_COMPRESS_HEADER_SIZE + 3u];
    size_t dst_len = sizeof(dst); // [状態] - 圧縮結果が収まらない出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_compress(dst, &dst_len, reinterpret_cast<const uint8_t *>(plain.data()),
                                    plain.size()); // [手順] - 出力が収まらないサイズで cplat_compress を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_compress() の戻り値が CPLAT_ERR_UNKNOWN であること。(deflate が完了しないことの確認)
}

// cplat_decompress が不正な引数を拒否することの確認
TEST_F(compressTest, decompress_rejects_invalid_arguments)
{
    // Arrange
    uint8_t dst[64];
    uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 64 byte の出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_dst =
        cplat_decompress(NULL, &dst_len, src, sizeof(src)); // [手順] - dst に NULL を指定して呼び出す。
    int actual_ret_null_dst_len =
        cplat_decompress(dst, NULL, src, sizeof(src)); // [手順] - dst_len に NULL を指定して呼び出す。
    int actual_ret_null_src =
        cplat_decompress(dst, &dst_len, NULL, sizeof(src)); // [手順] - src に NULL を指定して呼び出す。
    int actual_ret_header_only = cplat_decompress(
        dst, &dst_len, src, CPLAT_COMPRESS_HEADER_SIZE); // [手順] - src_len をヘッダー長ちょうどに指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst); // [確認_異常系] - dst が NULL のとき cplat_decompress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst_len); // [確認_異常系] - dst_len が NULL のとき cplat_decompress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_src); // [確認_異常系] - src が NULL のとき cplat_decompress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_header_only); // [確認_異常系] - src_len がヘッダー長以下のとき cplat_decompress() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// ヘッダーが示す元サイズに満たない出力バッファーが拒否されることの確認
TEST_F(compressTest, decompress_returns_buffer_too_small_when_dst_is_shorter_than_original)
{
    // Arrange
    std::string plain = make_plain_text();
    std::vector<uint8_t> compressed(plain.size() + CPLAT_COMPRESS_HEADER_SIZE + 64u);
    size_t compressed_len = compressed.size();
    uint8_t dst[16];
    size_t dst_len = sizeof(dst); // [状態] - 元サイズに満たない 16 byte の出力バッファーを用意する。

    ASSERT_EQ(CPLAT_OK,
              cplat_compress(compressed.data(), &compressed_len, reinterpret_cast<const uint8_t *>(plain.data()),
                             plain.size())); // [状態] - 平文を圧縮しておく。
                                             // [状態確認] - cplat_compress() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_decompress(dst, &dst_len, compressed.data(),
                                      compressed_len); // [手順] - 元サイズに満たない出力バッファーで展開を試みる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_decompress() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

// DEFLATE ストリームとして不正なデータの展開が失敗することの確認
TEST_F(compressTest, decompress_returns_unknown_for_corrupt_stream)
{
    // Arrange
    uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u];
    uint8_t dst[64];
    size_t dst_len = sizeof(dst);

    memset(src, 0xFF, sizeof(src));
    src[0] = 0x00;
    src[1] = 0x00;
    src[2] = 0x00;
    src[3] = 0x00;
    src[4] = 0x00;
    src[5] = 0x00;
    src[6] = 0x00;
    src[7] = 0x08; // [状態] - 元サイズ 8 byte を示すヘッダーと、DEFLATE として不正な本体を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_decompress(dst, &dst_len, src,
                         sizeof(src)); // [手順] - 不正な DEFLATE ストリームで cplat_decompress を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_decompress() の戻り値が CPLAT_ERR_UNKNOWN であること。(inflate が完了しないことの確認)
}

// ヘッダーの展開後長さが 4 GiB 以上の場合に上限超過を通知することの確認
// cplat-req: id=CPLAT-COMPRESS-FUNC-003; uuid=a6d4c8e1-3b27-4f9a-8e05-1c7b9d2f4a60
TEST_F(compressTest, decompress_returns_limit_exceeded_when_original_size_exceeds_max)
{
    // Arrange
    uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 1u] = {0};
    uint8_t dst[16];
    size_t dst_len = sizeof(dst);

    src[3] = 0x01; // [状態] - 展開後長さ 4 GiB を示す 8 バイトヘッダーと 1 byte の本体を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_decompress(dst, &dst_len, src,
                         sizeof(src)); // [手順] - 展開後長さが処理上限以上のヘッダーで cplat_decompress を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              actual_ret); // [確認_異常系] - cplat_decompress() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
}

// deflate の init 失敗を結果コードへ変換することの確認
TEST_F(compressTest, compress_returns_out_of_memory_when_deflate_init_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_zlib, deflateInit2_(_, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY,
                                         StrEq(ZLIB_VERSION), static_cast<int>(sizeof(z_stream))))
        .WillOnce(Return(
            Z_MEM_ERROR)); // [Pre-Assert確認_異常系] - deflateInit2_() が Z_DEFAULT_COMPRESSION、Z_DEFLATED、-15、8、Z_DEFAULT_STRATEGY、ZLIB_VERSION と等しい文字列、static_cast<int>(sizeof(z_stream)) を指定して 1 回呼び出されること。
                           // [Pre-Assert手順] - Z_MEM_ERROR を返す。
    EXPECT_CALL(mock_zlib, deflate(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - deflate() が呼び出されないこと。(初期化失敗後に処理しないことの確認)
    EXPECT_CALL(mock_zlib, deflateEnd(_))
        .Times(
            0); // [Pre-Assert確認_異常系] - deflateEnd() が呼び出されないこと。(未初期化のストリームを終了しないことの確認)

    // Act
    int actual_ret = cplat_compress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_compress() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// deflate の init 失敗を結果コードへ変換することの確認
TEST_F(compressTest, compress_returns_unknown_when_deflate_init_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_zlib, deflateInit2_(_, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY,
                                         StrEq(ZLIB_VERSION), static_cast<int>(sizeof(z_stream))))
        .WillOnce(Return(
            Z_STREAM_ERROR)); // [Pre-Assert確認_異常系] - deflateInit2_() が Z_DEFAULT_COMPRESSION、Z_DEFLATED、-15、8、Z_DEFAULT_STRATEGY、ZLIB_VERSION と等しい文字列、static_cast<int>(sizeof(z_stream)) を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - Z_STREAM_ERROR を返す。
    EXPECT_CALL(mock_zlib, deflate(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - deflate() が呼び出されないこと。(初期化失敗後に処理しないことの確認)
    EXPECT_CALL(mock_zlib, deflateEnd(_))
        .Times(
            0); // [Pre-Assert確認_異常系] - deflateEnd() が呼び出されないこと。(未初期化のストリームを終了しないことの確認)

    // Act
    int actual_ret = cplat_compress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_compress() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// inflate の init 失敗を結果コードへ変換することの確認
TEST_F(compressTest, decompress_returns_out_of_memory_when_inflate_init_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_zlib, inflateInit2_(_, -15, StrEq(ZLIB_VERSION), static_cast<int>(sizeof(z_stream))))
        .WillOnce(Return(
            Z_MEM_ERROR)); // [Pre-Assert確認_異常系] - inflateInit2_() が -15、ZLIB_VERSION と等しい文字列、static_cast<int>(sizeof(z_stream)) を指定して 1 回呼び出されること。
                           // [Pre-Assert手順] - Z_MEM_ERROR を返す。
    EXPECT_CALL(mock_zlib, inflate(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - inflate() が呼び出されないこと。(初期化失敗後に処理しないことの確認)
    EXPECT_CALL(mock_zlib, inflateEnd(_))
        .Times(
            0); // [Pre-Assert確認_異常系] - inflateEnd() が呼び出されないこと。(未初期化のストリームを終了しないことの確認)

    // Act
    int actual_ret = cplat_decompress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_decompress() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// inflate の init 失敗を結果コードへ変換することの確認
TEST_F(compressTest, decompress_returns_unknown_when_inflate_init_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_zlib, inflateInit2_(_, -15, StrEq(ZLIB_VERSION), static_cast<int>(sizeof(z_stream))))
        .WillOnce(Return(
            Z_STREAM_ERROR)); // [Pre-Assert確認_異常系] - inflateInit2_() が -15、ZLIB_VERSION と等しい文字列、static_cast<int>(sizeof(z_stream)) を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - Z_STREAM_ERROR を返す。
    EXPECT_CALL(mock_zlib, inflate(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - inflate() が呼び出されないこと。(初期化失敗後に処理しないことの確認)
    EXPECT_CALL(mock_zlib, inflateEnd(_))
        .Times(
            0); // [Pre-Assert確認_異常系] - inflateEnd() が呼び出されないこと。(未初期化のストリームを終了しないことの確認)

    // Act
    int actual_ret = cplat_decompress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_decompress() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// deflate の processing 失敗を結果コードへ変換することの確認
TEST_F(compressTest, compress_returns_out_of_memory_when_deflate_processing_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    InSequence sequence;
    EXPECT_CALL(mock_zlib, deflate(_, Z_FINISH))
        .WillOnce(Return(
            Z_MEM_ERROR)); // [Pre-Assert確認_異常系] - deflate() が Z_FINISH を指定して 1 回呼び出されること。(初期化に成功した後の処理であることの確認)
                           // [Pre-Assert手順] - Z_MEM_ERROR を返す。
    EXPECT_CALL(mock_zlib, deflateEnd(_))
        .WillOnce(
            DoDefault()); // [Pre-Assert確認_異常系] - deflateEnd() が 1 回呼び出されること。(失敗後も終了処理を呼ぶことの確認)
                          // [Pre-Assert手順] - 実関数へ委譲してストリームを解放する。

    // Act
    int actual_ret = cplat_compress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_compress() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// deflate の processing 失敗を結果コードへ変換することの確認
TEST_F(compressTest, compress_returns_unknown_when_deflate_processing_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    InSequence sequence;
    EXPECT_CALL(mock_zlib, deflate(_, Z_FINISH))
        .WillOnce(Return(
            Z_STREAM_ERROR)); // [Pre-Assert確認_異常系] - deflate() が Z_FINISH を指定して 1 回呼び出されること。(初期化に成功した後の処理であることの確認)
                              // [Pre-Assert手順] - Z_STREAM_ERROR を返す。
    EXPECT_CALL(mock_zlib, deflateEnd(_))
        .WillOnce(
            DoDefault()); // [Pre-Assert確認_異常系] - deflateEnd() が 1 回呼び出されること。(失敗後も終了処理を呼ぶことの確認)
                          // [Pre-Assert手順] - 実関数へ委譲してストリームを解放する。

    // Act
    int actual_ret = cplat_compress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_compress() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// inflate の processing 失敗を結果コードへ変換することの確認
TEST_F(compressTest, decompress_returns_out_of_memory_when_inflate_processing_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    InSequence sequence;
    EXPECT_CALL(mock_zlib, inflate(_, Z_FINISH))
        .WillOnce(Return(
            Z_MEM_ERROR)); // [Pre-Assert確認_異常系] - inflate() が Z_FINISH を指定して 1 回呼び出されること。(初期化に成功した後の処理であることの確認)
                           // [Pre-Assert手順] - Z_MEM_ERROR を返す。
    EXPECT_CALL(mock_zlib, inflateEnd(_))
        .WillOnce(
            DoDefault()); // [Pre-Assert確認_異常系] - inflateEnd() が 1 回呼び出されること。(失敗後も終了処理を呼ぶことの確認)
                          // [Pre-Assert手順] - 実関数へ委譲してストリームを解放する。

    // Act
    int actual_ret = cplat_decompress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_decompress() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// inflate の processing 失敗を結果コードへ変換することの確認
TEST_F(compressTest, decompress_returns_unknown_when_inflate_processing_fails)
{
    // Arrange
    NiceMock<Mock_zlib> mock_zlib;
    const uint8_t src[CPLAT_COMPRESS_HEADER_SIZE + 8u] = {0};
    uint8_t dst[64] = {0};
    size_t dst_len = sizeof(dst); // [状態] - 初期検査を通過する入出力を用意する。

    // Pre-Assert
    InSequence sequence;
    EXPECT_CALL(mock_zlib, inflate(_, Z_FINISH))
        .WillOnce(Return(
            Z_STREAM_ERROR)); // [Pre-Assert確認_異常系] - inflate() が Z_FINISH を指定して 1 回呼び出されること。(初期化に成功した後の処理であることの確認)
                              // [Pre-Assert手順] - Z_STREAM_ERROR を返す。
    EXPECT_CALL(mock_zlib, inflateEnd(_))
        .WillOnce(
            DoDefault()); // [Pre-Assert確認_異常系] - inflateEnd() が 1 回呼び出されること。(失敗後も終了処理を呼ぶことの確認)
                          // [Pre-Assert手順] - 実関数へ委譲してストリームを解放する。

    // Act
    int actual_ret = cplat_decompress(dst, &dst_len, src, sizeof(src)); // [手順] - 失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_decompress() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(sizeof(dst),
              dst_len); // [確認_異常系] - dst_len が sizeof(dst) と一致すること。(失敗時に出力長を更新しないことの確認)
}

// 変更前の Linux 実装が出力した圧縮データを復元することの確認
// 採取元 cplat commit: fd8e8801219a431e273a23e97c7c63739b44037c (システム libz.so.1 を使用)
TEST_F(compressTest, decompress_restores_legacy_linux_payload)
{
    // Arrange
    const uint8_t compressed[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x4b, 0x2e, 0xc8, 0x49, 0x2c, 0x51, 0xc8, 0x49,
        0x4d, 0x4f, 0x4c, 0xae, 0x54, 0x28, 0x4a, 0x2c, 0x57, 0x70, 0x71, 0x75, 0xf3, 0x71, 0x0c, 0x71,
        0x55, 0x48, 0xcb, 0xac, 0x28, 0x29, 0x2d, 0x4a, 0x65, 0xf8, 0x9f, 0x4c, 0x17, 0x15, 0x00}; // [状態] - 依存先変更前に採取したデータを使用する。
    const char expected[] = "cplat legacy raw DEFLATE fixture\0\xff"
                            "cplat legacy raw DEFLATE fixture\0\xff"
                            "cplat legacy raw DEFLATE fixture\0\xff"
                            "cplat legacy raw DEFLATE fixture\0\xff";
    uint8_t restored[sizeof(expected) - 1u] = {0};
    size_t restored_len = sizeof(restored);

    // Pre-Assert

    // Act
    int actual_ret = cplat_decompress(restored, &restored_len, compressed,
                                      sizeof(compressed)); // [手順] - 旧データを共通実装で展開する。

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_decompress() の戻り値が CPLAT_OK であること。(変更前の Linux 実装が出力したデータを展開できたことの確認)
    ASSERT_EQ(
        sizeof(expected) - 1u,
        restored_len); // [確認_正常系] - restored_len が sizeof(expected) - 1u と一致すること。(NUL を含む元の長さを復元したことの確認)
    EXPECT_EQ(
        0,
        memcmp(
            expected, restored,
            restored_len)); // [確認_正常系] - memcmp() の戻り値が 0 であること。(バイナリを含む元データと一致することの確認)
}
