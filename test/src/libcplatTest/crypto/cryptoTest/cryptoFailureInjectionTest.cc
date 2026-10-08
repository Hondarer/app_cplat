#include <testfw.h>
#include <cplat/base/platform.h>
#if defined(PLATFORM_LINUX)

    #include <mock_openssl.h>
    #include <cplat/base/result.h>
    #include <cplat/crypto/crypto.h>

    #include <cstring>
    #include <vector>

using testing::_;
using testing::DoDefault;
using testing::NiceMock;
using testing::Return;

class cryptoFailureInjectionTest : public Test
{
  protected:
    uint8_t key_[CPLAT_CRYPTO_KEY_SIZE];
    uint8_t nonce_[CPLAT_CRYPTO_NONCE_SIZE];
    uint8_t plain_[16];
    uint8_t pad_[4]; // cipher_ の 8 バイト境界を明示する。
    std::vector<uint8_t> cipher_;
    size_t cipher_len_ = 0u;

    // [サブ手順 名前=cryptoFailureInjectionTest.SetUp]
    void SetUp() override
    {
        std::memset(key_, 0x11, sizeof(key_));
        std::memset(nonce_, 0x22, sizeof(nonce_));
        std::memset(plain_, 0x33, sizeof(plain_));
        cipher_.assign(sizeof(plain_) + CPLAT_CRYPTO_TAG_SIZE, 0u);
        cipher_len_ = cipher_.size();
    }
    // [サブ手順終了]

    int encrypt()
    {
        cipher_len_ = cipher_.size();
        return cplat_crypto_encrypt(cipher_.data(), &cipher_len_, plain_, sizeof(plain_), key_, nonce_, NULL, 0u);
    }
};

// 暗号化コンテキストの確保に失敗した場合にメモリ不足が返ることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_out_of_memory_when_context_allocation_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_new(_, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_CIPHER_CTX_new が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = encrypt(); // [手順] - cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_crypto_encrypt の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}

// 暗号化の初期化に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_unknown_when_init_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_EncryptInit_ex(_, _, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_EncryptInit_ex が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = encrypt(); // [手順] - cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_crypto_encrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 暗号化の鍵・ノンス設定に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_unknown_when_key_setting_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_ctrl(_, _, _, _, _, _, _))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - 鍵設定へ進むため、ノンス長設定が成功すること。
    EXPECT_CALL(mock_openssl, EVP_EncryptInit_ex(_, _, _, _, _, _, _, _))
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_EncryptInit_ex が 2 回呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は成功を示す 1、2 回目は失敗を示す 0 を返却する。

    // Act
    int actual_ret = encrypt(); // [手順] - cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_crypto_encrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// ノンス長の設定に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_unknown_when_nonce_length_setting_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_ctrl(_, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_CIPHER_CTX_ctrl が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = encrypt(); // [手順] - cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_crypto_encrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 暗号文の生成に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_unknown_when_update_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_EncryptUpdate(_, _, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_EncryptUpdate が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = encrypt(); // [手順] - cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_crypto_encrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// AAD の入力に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_unknown_when_aad_update_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;
    const uint8_t aad[] = "header"; // [状態] - 暗号化に渡す AAD を用意する。
    cipher_len_ = cipher_.size();

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_EncryptUpdate(_, _, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - AAD の入力で EVP_EncryptUpdate が呼び出されること。
                                      // [Pre-Assert手順] - AAD の入力で失敗を示す 0 を返却する。

    // Act
    int actual_ret =
        cplat_crypto_encrypt(cipher_.data(), &cipher_len_, plain_, sizeof(plain_), key_, nonce_, aad,
                             sizeof(aad) - 1u); // [手順] - AAD を指定して cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - AAD の入力失敗時に cplat_crypto_encrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 暗号化の終端処理に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_unknown_when_final_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_EncryptFinal_ex(_, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_EncryptFinal_ex が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = encrypt(); // [手順] - cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_crypto_encrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 認証タグの取得に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, encrypt_returns_unknown_when_tag_get_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_ctrl(_, _, _, _, _, _, _))
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - EVP_CIPHER_CTX_ctrl がノンス長設定とタグ取得で呼び出されること。
                          // [Pre-Assert手順] - ノンス長設定は成功させ、タグ取得で失敗を示す 0 を返却する。

    // Act
    int actual_ret = encrypt(); // [手順] - cplat_crypto_encrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - タグ取得失敗時に cplat_crypto_encrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 復号コンテキストの確保に失敗した場合にメモリ不足が返ることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_out_of_memory_when_context_allocation_fails)
{
    // Arrange
    std::vector<uint8_t> restored(sizeof(plain_));
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, encrypt()); // [状態] - 復号対象の暗号文を用意する。
                                    // [状態確認] - encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_new(_, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_CIPHER_CTX_new が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher_.data(), cipher_len_, key_, nonce_, NULL,
                             0u); // [手順] - cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_crypto_decrypt の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}

// 復号の初期化に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_unknown_when_init_fails)
{
    // Arrange
    std::vector<uint8_t> restored(sizeof(plain_));
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, encrypt()); // [状態] - 復号対象の暗号文を用意する。
                                    // [状態確認] - encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_DecryptInit_ex(_, _, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_DecryptInit_ex が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却する。

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher_.data(), cipher_len_, key_, nonce_, NULL,
                             0u); // [手順] - cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_crypto_decrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 復号のノンス長設定に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_unknown_when_nonce_length_setting_fails)
{
    // Arrange
    std::vector<uint8_t> restored(sizeof(plain_));
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, encrypt()); // [状態] - 復号対象の暗号文を用意する。
                                    // [状態確認] - encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_ctrl(_, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - 復号のノンス長設定で EVP_CIPHER_CTX_ctrl が呼び出されること。
                          // [Pre-Assert手順] - ノンス長設定で失敗を示す 0 を返却する。

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher_.data(), cipher_len_, key_, nonce_, NULL,
                             0u); // [手順] - cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - ノンス長設定失敗時に cplat_crypto_decrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 復号の鍵・ノンス設定に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_unknown_when_key_setting_fails)
{
    // Arrange
    std::vector<uint8_t> restored(sizeof(plain_));
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, encrypt()); // [状態] - 復号対象の暗号文を用意する。
                                    // [状態確認] - encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_ctrl(_, _, _, _, _, _, _))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - 鍵設定へ進むため、ノンス長設定が成功すること。
    EXPECT_CALL(mock_openssl, EVP_DecryptInit_ex(_, _, _, _, _, _, _, _))
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_DecryptInit_ex が 2 回呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は成功を示す 1、2 回目は失敗を示す 0 を返却する。

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher_.data(), cipher_len_, key_, nonce_, NULL,
                             0u); // [手順] - cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 鍵・ノンス設定失敗時に cplat_crypto_decrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// AAD の入力に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_unknown_when_aad_update_fails)
{
    // Arrange
    const uint8_t aad[] = "header";
    const uint8_t plain[] = "payload";
    const size_t plain_len = sizeof(plain) - 1u;
    const size_t aad_len = sizeof(aad) - 1u;
    std::vector<uint8_t> cipher(plain_len + CPLAT_CRYPTO_TAG_SIZE);
    size_t cipher_len = cipher.size();
    std::vector<uint8_t> restored(plain_len);
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, cplat_crypto_encrypt(cipher.data(), &cipher_len, plain, plain_len, key_, nonce_, aad,
                                             aad_len)); // [状態] - AAD 付きの復号対象を用意する。
    // [状態確認] - cplat_crypto_encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_DecryptUpdate(_, _, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - AAD の入力で EVP_DecryptUpdate が呼び出されること。
                                      // [Pre-Assert手順] - AAD の入力で失敗を示す 0 を返却する。

    // Act
    int actual_ret = cplat_crypto_decrypt(restored.data(), &restored_len, cipher.data(), cipher_len, key_, nonce_, aad,
                                          aad_len); // [手順] - AAD を指定して cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - AAD の入力失敗時に cplat_crypto_decrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 平文の復号に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_unknown_when_update_fails)
{
    // Arrange
    std::vector<uint8_t> restored(sizeof(plain_));
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, encrypt()); // [状態] - 復号対象の暗号文を用意する。
                                    // [状態確認] - encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_DecryptUpdate(_, _, _, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - 平文の復号で EVP_DecryptUpdate が呼び出されること。
                                      // [Pre-Assert手順] - 平文の復号で失敗を示す 0 を返却する。

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher_.data(), cipher_len_, key_, nonce_, NULL,
                             0u); // [手順] - cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 平文の復号失敗時に cplat_crypto_decrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 認証タグの設定に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_unknown_when_tag_setting_fails)
{
    // Arrange
    std::vector<uint8_t> restored(sizeof(plain_));
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, encrypt()); // [状態] - 復号対象の暗号文を用意する。
                                    // [状態確認] - encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_CIPHER_CTX_ctrl(_, _, _, _, _, _, _))
        .WillOnce(Return(1))
        .WillOnce(Return(0))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - 認証タグ設定で EVP_CIPHER_CTX_ctrl が呼び出されること。
                          // [Pre-Assert手順] - 認証タグ設定で失敗を示す 0 を返却する。

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher_.data(), cipher_len_, key_, nonce_, NULL,
                             0u); // [手順] - cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 認証タグ設定失敗時に cplat_crypto_decrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 認証の終端処理に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, decrypt_returns_unknown_when_final_fails)
{
    // Arrange
    std::vector<uint8_t> restored(sizeof(plain_));
    size_t restored_len = restored.size();

    ASSERT_EQ(CPLAT_OK, encrypt()); // [状態] - 復号対象の暗号文を用意する。
                                    // [状態確認] - encrypt の戻り値が CPLAT_OK であること。

    NiceMock<Mock_openssl> mock_openssl;

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_DecryptFinal_ex(_, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_DecryptFinal_ex が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却する。

    // Act
    int actual_ret =
        cplat_crypto_decrypt(restored.data(), &restored_len, cipher_.data(), cipher_len_, key_, nonce_, NULL,
                             0u); // [手順] - cplat_crypto_decrypt を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 認証終端処理失敗時に cplat_crypto_decrypt の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// ダイジェスト コンテキストの確保に失敗した場合にメモリ不足が返ることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, passphrase_to_key_returns_out_of_memory_when_context_allocation_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;
    uint8_t derived[CPLAT_CRYPTO_KEY_SIZE]; // [状態] - 導出結果の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_MD_CTX_new(_, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_MD_CTX_new が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_passphrase_to_key(derived, (const uint8_t *)"secret",
                                             6u); // [手順] - cplat_passphrase_to_key を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_passphrase_to_key の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}

// ダイジェストの計算に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, passphrase_to_key_returns_unknown_when_digest_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;
    uint8_t derived[CPLAT_CRYPTO_KEY_SIZE]; // [状態] - 導出結果の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_DigestInit_ex(_, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_DigestInit_ex が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_passphrase_to_key(derived, (const uint8_t *)"secret",
                                             6u); // [手順] - cplat_passphrase_to_key を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_passphrase_to_key の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// ダイジェスト更新に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, passphrase_to_key_returns_unknown_when_digest_update_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;
    uint8_t derived[CPLAT_CRYPTO_KEY_SIZE]; // [状態] - 導出結果の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_DigestUpdate(_, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_DigestUpdate が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却する。

    // Act
    int actual_ret = cplat_passphrase_to_key(derived, (const uint8_t *)"secret",
                                             6u); // [手順] - cplat_passphrase_to_key を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - ダイジェスト更新失敗時に cplat_passphrase_to_key の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// ダイジェスト終端処理に失敗した場合に通知されることの確認
// [サブ手順参照 名前=cryptoFailureInjectionTest.SetUp]
TEST_F(cryptoFailureInjectionTest, passphrase_to_key_returns_unknown_when_digest_final_fails)
{
    // Arrange
    NiceMock<Mock_openssl> mock_openssl;
    uint8_t derived[CPLAT_CRYPTO_KEY_SIZE]; // [状態] - 導出結果の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_openssl, EVP_DigestFinal_ex(_, _, _, _, _, _))
        .WillOnce(Return(0))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - EVP_DigestFinal_ex が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は失敗を示す 0 を返却する。

    // Act
    int actual_ret = cplat_passphrase_to_key(derived, (const uint8_t *)"secret",
                                             6u); // [手順] - cplat_passphrase_to_key を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - ダイジェスト終端処理失敗時に cplat_passphrase_to_key の戻り値が CPLAT_ERR_UNKNOWN であること。
}

#endif /* PLATFORM_LINUX */
