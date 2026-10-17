#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crypto/crypto.h>

#include <cstring>
#include <vector>

class cryptoTest : public Test
{
  protected:
    uint8_t key_[CPLAT_CRYPTO_KEY_SIZE];
    uint8_t nonce_[CPLAT_CRYPTO_NONCE_SIZE];
    unsigned int pad_ = 0; /* 明示的アラインメント */

    // [サブ手順 名前=cryptoTest.SetUp]
    void SetUp() override
    {
        std::memset(key_, 0x11, sizeof(key_));
        std::memset(nonce_, 0x22, sizeof(nonce_));
    }
    // [サブ手順終了]
};

// 暗号化した結果を復号すると元のデータへ戻ることの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, round_trip_restores_original_bytes)
{
    // Arrange
    const uint8_t plain[] = "cplat crypto round trip";
    const size_t plain_len = sizeof(plain) - 1u;
    std::vector<uint8_t> cipher(plain_len + CPLAT_CRYPTO_TAG_SIZE);
    std::vector<uint8_t> restored(plain_len);
    size_t cipher_len = cipher.size();
    size_t restored_len = restored.size(); // [状態] - 平文と、タグ込みの暗号文バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_encrypt = cplat_crypto_encrypt(cipher.data(), &cipher_len, plain, plain_len, key_, nonce_, nullptr,
                                                  0u); // [手順] - AAD なしで cplat_crypto_encrypt() を呼び出す。
    int actual_ret_decrypt =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher.data(), cipher_len, key_, nonce_, nullptr,
                             0u); // [手順] - 同じ鍵とノンスで cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_encrypt); // [確認_正常系] - cplat_crypto_encrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_decrypt); // [確認_正常系] - cplat_crypto_decrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(plain_len + CPLAT_CRYPTO_TAG_SIZE,
              cipher_len); // [確認_正常系] - cipher_len が plain_len + CPLAT_CRYPTO_TAG_SIZE と一致すること。
    EXPECT_EQ(plain_len, restored_len); // [確認_正常系] - restored_len が plain_len と一致すること。
    EXPECT_EQ(
        0, memcmp(plain, restored.data(),
                  plain_len)); // [確認_正常系] - memcmp() の戻り値が 0 であること。(復号結果が平文と一致することの確認)
}

// AAD 付きの往復が成功することの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, round_trip_with_aad)
{
    // Arrange
    const uint8_t plain[] = "payload";
    const uint8_t aad[] = "header";
    const size_t plain_len = sizeof(plain) - 1u;
    const size_t aad_len = sizeof(aad) - 1u;
    std::vector<uint8_t> cipher(plain_len + CPLAT_CRYPTO_TAG_SIZE);
    std::vector<uint8_t> restored(plain_len);
    size_t cipher_len = cipher.size();
    size_t restored_len = restored.size(); // [状態] - 平文と AAD を用意する。

    // Pre-Assert

    // Act
    int actual_ret_encrypt =
        cplat_crypto_encrypt(cipher.data(), &cipher_len, plain, plain_len, key_, nonce_, aad,
                             aad_len); // [手順] - AAD を指定して cplat_crypto_encrypt() を呼び出す。
    int actual_ret_decrypt =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher.data(), cipher_len, key_, nonce_, aad,
                             aad_len); // [手順] - 同じ AAD を指定して cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_encrypt); // [確認_正常系] - cplat_crypto_encrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_decrypt); // [確認_正常系] - cplat_crypto_decrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0, memcmp(plain, restored.data(),
                  plain_len)); // [確認_正常系] - memcmp() の戻り値が 0 であること。(復号結果が平文と一致することの確認)
}

// AAD のポインターが非 NULL でも長さ 0 の場合に往復が成功することの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, round_trip_with_nonnull_zero_length_aad)
{
    // Arrange
    const uint8_t plain[] = "payload";
    const uint8_t aad[] = "ignored";
    const size_t plain_len = sizeof(plain) - 1u;
    const size_t aad_len = 0u;
    std::vector<uint8_t> cipher(plain_len + CPLAT_CRYPTO_TAG_SIZE);
    std::vector<uint8_t> restored(plain_len);
    size_t cipher_len = cipher.size();
    size_t restored_len = restored.size(); // [状態] - 平文と、nullptr 以外で長さ 0 の AAD を用意する。

    // Pre-Assert

    // Act
    int actual_ret_encrypt = cplat_crypto_encrypt(
        cipher.data(), &cipher_len, plain, plain_len, key_, nonce_, aad,
        aad_len); // [手順] - nullptr 以外で長さ 0 の AAD を指定して cplat_crypto_encrypt() を呼び出す。
    int actual_ret_decrypt = cplat_crypto_decrypt(
        restored.data(), &restored_len, cipher.data(), cipher_len, key_, nonce_, aad,
        aad_len); // [手順] - nullptr 以外で長さ 0 の AAD を指定して cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_encrypt); // [確認_正常系] - cplat_crypto_encrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_decrypt); // [確認_正常系] - cplat_crypto_decrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        plain_len,
        restored_len); // [確認_正常系] - nullptr 以外で長さ 0 の AAD を指定した restored_len が plain_len と一致すること。
    EXPECT_EQ(
        0, memcmp(plain, restored.data(),
                  plain_len)); // [確認_正常系] - memcmp() の戻り値が 0 であること。(復号結果が平文と一致することの確認)
}

// 改ざんされた暗号文の復号が認証に失敗することの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, decrypt_rejects_tampered_cipher_text)
{
    // Arrange
    const uint8_t plain[] = "payload";
    const size_t plain_len = sizeof(plain) - 1u;
    std::vector<uint8_t> cipher(plain_len + CPLAT_CRYPTO_TAG_SIZE);
    std::vector<uint8_t> restored(plain_len);
    size_t cipher_len = cipher.size();
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, cplat_crypto_encrypt(cipher.data(), &cipher_len, plain, plain_len, key_, nonce_, nullptr,
                                             0u)); // [状態] - 復号対象の暗号文を用意する。
    // [状態確認] - cplat_crypto_encrypt() の戻り値が CPLAT_OK であること。
    cipher[0] = static_cast<uint8_t>(cipher[0] ^ 0xFFu); // [状態] - 暗号文の先頭 1 byte を反転して改ざんする。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher.data(), cipher_len, key_, nonce_, nullptr,
                             0u); // [手順] - 改ざんした暗号文で cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 改ざんした暗号文に対する cplat_crypto_decrypt() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 異なる鍵での復号が認証に失敗することの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, decrypt_rejects_wrong_key)
{
    // Arrange
    const uint8_t plain[] = "payload";
    const size_t plain_len = sizeof(plain) - 1u;
    std::vector<uint8_t> cipher(plain_len + CPLAT_CRYPTO_TAG_SIZE);
    std::vector<uint8_t> restored(plain_len);
    size_t cipher_len = cipher.size();
    size_t restored_len = restored.size();
    uint8_t other_key[CPLAT_CRYPTO_KEY_SIZE];

    ASSERT_EQ(CPLAT_OK, cplat_crypto_encrypt(cipher.data(), &cipher_len, plain, plain_len, key_, nonce_, nullptr,
                                             0u)); // [状態] - 復号対象の暗号文を用意する。
    // [状態確認] - cplat_crypto_encrypt() の戻り値が CPLAT_OK であること。
    std::memset(other_key, 0x33, sizeof(other_key)); // [状態] - 暗号化に使用したものと異なる鍵を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher.data(), cipher_len, other_key, nonce_, nullptr,
                             0u); // [手順] - 異なる鍵で cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 異なる鍵を指定した cplat_crypto_decrypt() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 空の平文が暗号化・復号できることの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, round_trip_of_empty_plain_text)
{
    // Arrange
    uint8_t cipher[CPLAT_CRYPTO_TAG_SIZE];
    uint8_t restored[1];
    size_t cipher_len = sizeof(cipher);
    size_t restored_len = sizeof(restored); // [状態] - 平文長 0、タグのみが入る暗号文バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_encrypt = cplat_crypto_encrypt(cipher, &cipher_len, nullptr, 0u, key_, nonce_, nullptr,
                                                  0u); // [手順] - 長さ 0 の平文で cplat_crypto_encrypt() を呼び出す。
    int actual_ret_decrypt =
        cplat_crypto_decrypt(restored, &restored_len, cipher, cipher_len, key_, nonce_, nullptr,
                             0u); // [手順] - タグのみの暗号文で cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_encrypt); // [確認_正常系] - cplat_crypto_encrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_decrypt); // [確認_正常系] - cplat_crypto_decrypt() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_CRYPTO_TAG_SIZE, cipher_len); // [確認_正常系] - cipher_len が CPLAT_CRYPTO_TAG_SIZE であること。
    EXPECT_EQ(0u, restored_len);                  // [確認_正常系] - restored_len が 0 であること。
}

// cplat_crypto_encrypt が不正な引数を拒否することの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, encrypt_rejects_invalid_arguments)
{
    // Arrange
    const uint8_t plain[] = "payload";
    const size_t plain_len = sizeof(plain) - 1u;
    uint8_t cipher[64];
    size_t cipher_len = sizeof(cipher); // [状態] - 十分な大きさの暗号文バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_dst = cplat_crypto_encrypt(nullptr, &cipher_len, plain, plain_len, key_, nonce_, nullptr, 0u);
    int actual_ret_null_dst_len = cplat_crypto_encrypt(cipher, nullptr, plain, plain_len, key_, nonce_, nullptr, 0u);
    int actual_ret_null_src = cplat_crypto_encrypt(cipher, &cipher_len, nullptr, plain_len, key_, nonce_, nullptr, 0u);
    int actual_ret_null_key = cplat_crypto_encrypt(cipher, &cipher_len, plain, plain_len, nullptr, nonce_, nullptr, 0u);
    int actual_ret_null_nonce = cplat_crypto_encrypt(
        cipher, &cipher_len, plain, plain_len, key_, nullptr, nullptr,
        0u); // [手順] - dst、dst_len、src、key、nonce に順に nullptr を指定して cplat_crypto_encrypt() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst); // [確認_異常系] - dst が nullptr の cplat_crypto_encrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst_len); // [確認_異常系] - dst_len が nullptr の cplat_crypto_encrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_src); // [確認_異常系] - src が nullptr かつ src_len が 0 より大きい cplat_crypto_encrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_key); // [確認_異常系] - key が nullptr の cplat_crypto_encrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_nonce); // [確認_異常系] - nonce が nullptr の cplat_crypto_encrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 暗号文とタグが収まらない出力バッファーが拒否されることの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, encrypt_returns_buffer_too_small)
{
    // Arrange
    const uint8_t plain[] = "payload";
    const size_t plain_len = sizeof(plain) - 1u;
    uint8_t cipher[8];
    size_t cipher_len = sizeof(cipher); // [状態] - 平文長 + タグ長に満たない出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_crypto_encrypt(cipher, &cipher_len, plain, plain_len, key_, nonce_, nullptr,
                             0u); // [手順] - 不足する出力バッファーを指定して cplat_crypto_encrypt() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_crypto_encrypt() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

// cplat_crypto_decrypt が不正な引数を拒否することの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, decrypt_rejects_invalid_arguments)
{
    // Arrange
    uint8_t cipher[CPLAT_CRYPTO_TAG_SIZE + 8u] = {0};
    uint8_t restored[64];
    size_t restored_len = sizeof(restored); // [状態] - タグ長を超える暗号文と出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_dst =
        cplat_crypto_decrypt(nullptr, &restored_len, cipher, sizeof(cipher), key_, nonce_, nullptr, 0u);
    int actual_ret_null_dst_len =
        cplat_crypto_decrypt(restored, nullptr, cipher, sizeof(cipher), key_, nonce_, nullptr, 0u);
    int actual_ret_null_src =
        cplat_crypto_decrypt(restored, &restored_len, nullptr, sizeof(cipher), key_, nonce_, nullptr, 0u);
    int actual_ret_short_src =
        cplat_crypto_decrypt(restored, &restored_len, cipher, CPLAT_CRYPTO_TAG_SIZE - 1u, key_, nonce_, nullptr, 0u);
    int actual_ret_null_key =
        cplat_crypto_decrypt(restored, &restored_len, cipher, sizeof(cipher), nullptr, nonce_, nullptr, 0u);
    int actual_ret_null_nonce = cplat_crypto_decrypt(
        restored, &restored_len, cipher, sizeof(cipher), key_, nullptr, nullptr,
        0u); // [手順] - dst、dst_len、src、src_len、key、nonce に順に不正値を指定して cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst); // [確認_異常系] - dst が nullptr の cplat_crypto_decrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_dst_len); // [確認_異常系] - dst_len が nullptr の cplat_crypto_decrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_src); // [確認_異常系] - src が nullptr の cplat_crypto_decrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_short_src); // [確認_異常系] - src_len がタグ長未満の cplat_crypto_decrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_key); // [確認_異常系] - key が nullptr の cplat_crypto_decrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_nonce); // [確認_異常系] - nonce が nullptr の cplat_crypto_decrypt() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 平文が収まらない出力バッファーが拒否されることの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, decrypt_returns_buffer_too_small)
{
    // Arrange
    uint8_t cipher[CPLAT_CRYPTO_TAG_SIZE + 8u] = {0};
    uint8_t restored[4];
    size_t restored_len = sizeof(restored); // [状態] - 平文長に満たない出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored, &restored_len, cipher, sizeof(cipher), key_, nonce_, nullptr,
                             0u); // [手順] - 不足する出力バッファーを指定して cplat_crypto_decrypt() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_crypto_decrypt() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

// パスフレーズから鍵が導出されることの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, passphrase_to_key_derives_deterministic_key)
{
    // Arrange
    const uint8_t passphrase[] = "secret";
    uint8_t first[CPLAT_CRYPTO_KEY_SIZE] = {0};
    uint8_t second[CPLAT_CRYPTO_KEY_SIZE] = {0}; // [状態] - 導出結果の格納先を 2 つ用意する。

    // Pre-Assert

    // Act
    int actual_ret_first = cplat_passphrase_to_key(first, passphrase, sizeof(passphrase) - 1u);
    int actual_ret_second = cplat_passphrase_to_key(
        second, passphrase,
        sizeof(passphrase) - 1u); // [手順] - 同じパスフレーズを指定して 2 回 cplat_passphrase_to_key() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_first); // [確認_正常系] - 1 回目の cplat_passphrase_to_key() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_second); // [確認_正常系] - 2 回目の cplat_passphrase_to_key() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0,
        memcmp(
            first, second,
            sizeof(
                first))); // [確認_正常系] - memcmp() の戻り値が 0 であること。(同じパスフレーズから同じ鍵が導出されることの確認)
}

// 長さ 0 のパスフレーズが受け付けられることの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, passphrase_to_key_accepts_empty_passphrase)
{
    // Arrange
    uint8_t key[CPLAT_CRYPTO_KEY_SIZE] = {0}; // [状態] - 導出結果の格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_passphrase_to_key(
        key, nullptr,
        0u); // [手順] - passphrase に nullptr、長さに 0 を指定して cplat_passphrase_to_key() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_passphrase_to_key() の戻り値が CPLAT_OK であること。
}

// cplat_passphrase_to_key が不正な引数を拒否することの確認
// [サブ手順参照 名前=cryptoTest.SetUp]
TEST_F(cryptoTest, passphrase_to_key_rejects_invalid_arguments)
{
    // Arrange
    uint8_t key[CPLAT_CRYPTO_KEY_SIZE];

    // Pre-Assert

    // Act
    int actual_ret_null_key = cplat_passphrase_to_key(nullptr, (const uint8_t *)"secret", 6u);
    int actual_ret_null_passphrase = cplat_passphrase_to_key(
        key, nullptr,
        6u); // [手順] - key に nullptr、passphrase に nullptr を指定して cplat_passphrase_to_key() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_key); // [確認_異常系] - key が nullptr の cplat_passphrase_to_key() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_passphrase); // [確認_異常系] - passphrase が nullptr かつ長さが 0 より大きい cplat_passphrase_to_key() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
