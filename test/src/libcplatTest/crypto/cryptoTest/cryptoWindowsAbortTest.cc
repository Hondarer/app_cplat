#include <testfw.h>
#include <cplat/base/platform.h>
#if defined(PLATFORM_WINDOWS)

    #include <mock_bcrypt.h>
    #include <mock_cplat.h>
    #include <cplat/base/result.h>
    #include <cplat/crypto/crypto.h>

    #include <cstdio>
    #include <cstdlib>
    #include <cstring>

namespace
{

// BCrypt の解放関数が無効なハンドルとして失敗したことを表す値 (STATUS_INVALID_HANDLE)。
const NTSTATUS k_status_invalid_handle = (NTSTATUS)0xC0000008L;

// cplat_abort_at() の呼び出しを、呼び出し元の関数名を標準エラー出力へ出して終了する動作へ置き換える。
void report_abort_and_exit(cplat_abort_kind, const char *, const char *, int, const char *func)
{
    std::fprintf(stderr, "cplat_abort_at called from %s\n", func);
    std::fflush(stderr);
    std::_Exit(3);
}

} // namespace

class cryptoWindowsAbortTest : public Test
{
  protected:
    uint8_t key_[CPLAT_CRYPTO_KEY_SIZE];
    uint8_t nonce_[CPLAT_CRYPTO_NONCE_SIZE];
    uint8_t plain_[16];
    uint8_t cipher_[16 + CPLAT_CRYPTO_TAG_SIZE];
    NiceMock<Mock_bcrypt> mock_bcrypt_;
    NiceMock<Mock_cplat> mock_cplat_;

    // [サブ手順 名前=cryptoWindowsAbortTest.SetUp]
    void SetUp() override
    {
        std::memset(key_, 0x11, sizeof(key_));
        std::memset(nonce_, 0x22, sizeof(nonce_));
        std::memset(plain_, 0x33, sizeof(plain_));
        std::memset(cipher_, 0, sizeof(cipher_));
        // [状態] - 鍵、ノンス、平文、暗号文のバッファーを用意する。
        ON_CALL(mock_cplat_, cplat_abort_at(_, _, _, _, _)).WillByDefault(Invoke(report_abort_and_exit));
        // [状態] - cplat_abort_at() から、呼び出し元の関数名を標準エラー出力へ出して終了するように既定動作を設定する。
    }
    // [サブ手順終了]
};

// BCryptDestroyKey() が失敗した場合に cplat_abort_at() を呼び出すことの確認
// [サブ手順参照 名前=cryptoWindowsAbortTest.SetUp]
TEST_F(cryptoWindowsAbortTest, encrypt_aborts_when_destroy_key_fails)
{
    // Arrange
    size_t cipher_len = sizeof(cipher_);

    // Pre-Assert

    // Act / Assert
    EXPECT_DEATH(
        {
            ON_CALL(mock_bcrypt_, BCryptDestroyKey(_, _, _, _)).WillByDefault(Return(k_status_invalid_handle));
            (void)cplat_crypto_encrypt(cipher_, &cipher_len, plain_, sizeof(plain_), key_, nonce_, nullptr, 0U);
        },
        "cplat_abort_at called from destroy_key_or_abort"); // [確認_異常系] - BCryptDestroyKey() が失敗した場合に、destroy_key_or_abort() から cplat_abort_at() が呼ばれてプロセスが終了すること。
}

// BCryptDestroyHash() が失敗した場合に cplat_abort_at() を呼び出すことの確認
// [サブ手順参照 名前=cryptoWindowsAbortTest.SetUp]
TEST_F(cryptoWindowsAbortTest, passphrase_to_key_aborts_when_destroy_hash_fails)
{
    // Arrange
    uint8_t key_out[CPLAT_CRYPTO_KEY_SIZE] = {};
    const uint8_t passphrase[] = {'p', 'a', 's', 's'};

    // Pre-Assert

    // Act / Assert
    EXPECT_DEATH(
        {
            ON_CALL(mock_bcrypt_, BCryptDestroyHash(_, _, _, _)).WillByDefault(Return(k_status_invalid_handle));
            (void)cplat_passphrase_to_key(key_out, passphrase, sizeof(passphrase));
        },
        "cplat_abort_at called from destroy_hash_or_abort"); // [確認_異常系] - BCryptDestroyHash() が失敗した場合に、destroy_hash_or_abort() から cplat_abort_at() が呼ばれてプロセスが終了すること。
}

// BCryptCloseAlgorithmProvider() が失敗した場合に cplat_abort_at() を呼び出すことの確認
// [サブ手順参照 名前=cryptoWindowsAbortTest.SetUp]
TEST_F(cryptoWindowsAbortTest, passphrase_to_key_aborts_when_close_algorithm_fails)
{
    // Arrange
    uint8_t key_out[CPLAT_CRYPTO_KEY_SIZE] = {};
    const uint8_t passphrase[] = {'p', 'a', 's', 's'};

    // Pre-Assert

    // Act / Assert
    EXPECT_DEATH(
        {
            ON_CALL(mock_bcrypt_, BCryptCloseAlgorithmProvider(_, _, _, _, _))
                .WillByDefault(Return(k_status_invalid_handle));
            (void)cplat_passphrase_to_key(key_out, passphrase, sizeof(passphrase));
        },
        "cplat_abort_at called from close_algorithm_or_abort"); // [確認_異常系] - BCryptCloseAlgorithmProvider() が失敗した場合に、close_algorithm_or_abort() から cplat_abort_at() が呼ばれてプロセスが終了すること。
}

#endif /* PLATFORM_WINDOWS */
