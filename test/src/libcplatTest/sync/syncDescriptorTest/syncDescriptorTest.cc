#include <testfw.h>

#include <cplat/crt/stdlib.h>
#include <cplat/sync/sync_descriptor.h>

#include <mock_cplat.h>

#include <cstring>

using testing::_;
using testing::DoDefault;
using testing::NiceMock;
using testing::Return;

// export / import が不正引数を拒否することの確認
TEST(syncDescriptorTest, rejects_invalid_export_and_import_arguments)
{
    // Arrange
    unsigned char descriptor[64] = {0};
    size_t descriptor_size = sizeof(descriptor);
    char *identity = NULL;

    // Pre-Assert

    // Act
    int export_identity_result = cplat_internal_interprocess_sync_descriptor_export(
        NULL, 1U, 1U, descriptor, &descriptor_size); // [手順] - NULL identity で descriptor を出力する。
    int export_size_result = cplat_internal_interprocess_sync_descriptor_export(
        "identity", 1U, 1U, descriptor, NULL); // [手順] - NULL サイズ格納先で descriptor を出力する。
    int import_descriptor_result = cplat_internal_interprocess_sync_descriptor_import(
        NULL, sizeof(descriptor), 1U, 1U, &identity); // [手順] - NULL descriptor を import する。
    int import_identity_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, sizeof(descriptor), 1U, 1U, NULL); // [手順] - NULL identity 格納先で import する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              export_identity_result); // [確認_異常系] - NULL identity が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              export_size_result); // [確認_異常系] - NULL サイズ格納先が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              import_descriptor_result); // [確認_異常系] - NULL descriptor が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              import_identity_result); // [確認_異常系] - NULL identity 格納先が INVALID_ARGUMENT になること。
}

// identity 長 0 とサイズ不一致を拒否することの確認
TEST(syncDescriptorTest, rejects_zero_and_mismatched_identity_lengths)
{
    // Arrange
    unsigned char descriptor[64] = {0};
    size_t descriptor_size = sizeof(descriptor);
    char *identity = NULL;

    // Pre-Assert
    ASSERT_EQ(CPLAT_OK,
              cplat_internal_interprocess_sync_descriptor_export("identity", 1U, 1U, descriptor, &descriptor_size));
    // [確認_正常系] - `cplat_internal_interprocess_sync_descriptor_export("identity", 1U, 1U, descriptor, &descriptor_size)` の戻り値が `CPLAT_OK` であること。

    // Act
    descriptor[8] = 0U;
    descriptor[9] = 0U;
    descriptor[10] = 0U;
    descriptor[11] = 0U;
    int zero_length_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, 20U, 1U, 1U, &identity); // [手順] - identity length 0 の descriptor を import する。
    int short_descriptor_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, 1U, 1U,
        &identity); // [手順] - identity length とサイズが一致しない descriptor を import する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              zero_length_result); // [確認_異常系] - identity length 0 が CORRUPT_DESCRIPTOR になること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              short_descriptor_result); // [確認_異常系] - サイズ不一致が CORRUPT_DESCRIPTOR になること。
}

// identity が同じ種別とバックエンドで往復することの確認
TEST(syncDescriptorTest, exports_and_imports_identity)
{
    // Arrange
    unsigned char descriptor[64] = {0};
    size_t descriptor_size = sizeof(descriptor);
    char *identity = NULL; // [状態] - 出力バッファーと identity 格納先を用意する。

    // Pre-Assert

    // Act
    int export_result = cplat_internal_interprocess_sync_descriptor_export(
        "lock-path", CPLAT_INTERPROCESS_SYNC_KIND_LOCK, 1U, descriptor,
        &descriptor_size); // [手順] - identity "lock-path" を descriptor へ出力する。
    int import_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, CPLAT_INTERPROCESS_SYNC_KIND_LOCK, 1U,
        &identity); // [手順] - 同じ種別とバックエンドで import する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        export_result); // [確認_正常系] - cplat_internal_interprocess_sync_descriptor_export の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        import_result); // [確認_正常系] - cplat_internal_interprocess_sync_descriptor_import の戻り値が CPLAT_OK であること。
    ASSERT_NE((char *)NULL, identity);   // [確認_正常系] - import した identity が NULL でないこと。
    EXPECT_STREQ("lock-path", identity); // [確認_正常系] - import した identity が "lock-path" であること。

    // Cleanup
    cplat_free(identity);
}

// identity 確保失敗が UNKNOWN になることの確認
TEST(syncDescriptorTest, reports_unknown_when_identity_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    unsigned char descriptor[64] = {0};
    size_t descriptor_size = sizeof(descriptor);
    char *identity = NULL;
    ASSERT_EQ(CPLAT_OK,
              cplat_internal_interprocess_sync_descriptor_export(
                  "identity", 1U, 1U, descriptor, &descriptor_size)); // [状態] - 正常な descriptor を生成する。
    // [状態確認] - cplat_internal_interprocess_sync_descriptor_export の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - identity 確保が 1 回目に失敗すること。

    // Act
    int result = cplat_internal_interprocess_sync_descriptor_import(descriptor, descriptor_size, 1U, 1U,
                                                                    &identity); // [手順] - descriptor を import する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result);         // [確認_異常系] - import の戻り値が CPLAT_ERR_UNKNOWN になること。
    EXPECT_EQ(NULL, identity); // [確認_異常系] - identity が設定されないこと。
}

// 省略と容量不足で必要サイズを返すことの確認
TEST(syncDescriptorTest, reports_required_size_for_absent_and_small_buffers)
{
    // Arrange
    unsigned char descriptor[64] = {0};
    size_t absent_size = 0U;
    size_t small_size = 1U;

    // Pre-Assert

    // Act
    int absent_result = cplat_internal_interprocess_sync_descriptor_export(
        "identity", 1U, 1U, NULL, &absent_size); // [手順] - descriptor を省略して必要サイズを問い合わせる。
    int small_result =
        cplat_internal_interprocess_sync_descriptor_export("identity", 1U, 1U, descriptor,
                                                           &small_size); // [手順] - 容量不足の descriptor へ出力する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              absent_result); // [確認_正常系] - descriptor 省略時の戻り値が BUFFER_TOO_SMALL であること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              small_result);            // [確認_異常系] - descriptor 容量不足時の戻り値が BUFFER_TOO_SMALL であること。
    EXPECT_EQ(absent_size, small_size); // [確認_正常系] - 両方の呼び出しが同じ必要サイズを返すこと。
}

// 壊れたヘッダー各欄を拒否することの確認
TEST(syncDescriptorTest, rejects_each_corrupt_header_field)
{
    // Arrange
    unsigned char descriptor[64] = {0};
    unsigned char original[64] = {0};
    size_t descriptor_size = sizeof(descriptor);
    char *identity = NULL;
    ASSERT_EQ(CPLAT_OK,
              cplat_internal_interprocess_sync_descriptor_export(
                  "identity", 1U, 1U, descriptor, &descriptor_size)); // [状態] - 正常な descriptor を生成する。
    // [状態確認] - cplat_internal_interprocess_sync_descriptor_export の戻り値が CPLAT_OK であること。
    memcpy(original, descriptor, descriptor_size);

    // Pre-Assert

    // Act
    int short_header_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE - 1U, 1U, 1U,
        &identity); // [手順] - ヘッダーより短い descriptor を import する。
    descriptor[0] ^= 0xffU;
    int magic_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, 1U, 1U, &identity); // [手順] - magic が異なる descriptor を import する。
    memcpy(descriptor, original, descriptor_size);
    descriptor[4]++;
    int version_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, 1U, 1U, &identity); // [手順] - version が異なる descriptor を import する。
    memcpy(descriptor, original, descriptor_size);
    descriptor[5]++;
    int kind_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, 1U, 1U, &identity); // [手順] - kind が異なる descriptor を import する。
    memcpy(descriptor, original, descriptor_size);
    descriptor[6]++;
    int backend_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, 1U, 1U, &identity); // [手順] - backend が異なる descriptor を import する。
    memcpy(descriptor, original, descriptor_size);
    descriptor[8]++;
    int length_result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, 1U, 1U,
        &identity); // [手順] - identity 長と全体サイズが異なる descriptor を import する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              short_header_result); // [確認_異常系] - 短いヘッダーが CORRUPT_DESCRIPTOR になること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              magic_result); // [確認_異常系] - magic 不一致が CORRUPT_DESCRIPTOR になること。
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH,
              version_result); // [確認_異常系] - version 不一致が VERSION_MISMATCH になること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              kind_result); // [確認_異常系] - kind 不一致が CORRUPT_DESCRIPTOR になること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              backend_result); // [確認_異常系] - backend 不一致が CORRUPT_DESCRIPTOR になること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              length_result);  // [確認_異常系] - identity 長不一致が CORRUPT_DESCRIPTOR になること。
    EXPECT_EQ(NULL, identity); // [確認_異常系] - 不正 descriptor から identity が生成されないこと。
}
