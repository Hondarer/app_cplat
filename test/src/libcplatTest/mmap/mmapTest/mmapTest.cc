#include <testfw.h>
#include "mmapTestCommon.h"

class mmapTest : public mmapTestFixture
{
};

// 新規ファイルを create_size で作成し、再アタッチ時は既存サイズを使うことの確認 (マルチ フェーズ テスト)
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_creates_new_file_and_ignores_create_size_on_reattach)
{
    // Arrange
    cplat_mmap *map = NULL;
    int result; // [状態] - 新規作成用のパス mmap.dat とハンドル格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - CREATE_NEW 付きの cplat_file_open が 1 回呼び出されること。
                // [Pre-Assert手順] - 番兵ハンドルを設定し、CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_file_set_size(_, kMapSize, _))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - create_size 64 で cplat_file_set_size が呼び出されること。
                           // [Pre-Assert手順] - CPLAT_OK を返却する。

    // Act
    result = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                  NULL); // [手順] - create_size 64 で新規アタッチする。

    // Assert
    ASSERT_EQ(CPLAT_OK, result);        // [確認_正常系] - attach (新規作成) の戻り値が CPLAT_OK であること。
    ASSERT_NE((cplat_mmap *)NULL, map); // [確認_正常系] - attach (新規作成) のマップが NULL でないこと。
    EXPECT_EQ(kMapSize,
              cplat_mmap_get_size(map)); // [確認_正常系] - マップ サイズが create_size (64) と一致すること。
    EXPECT_EQ(static_cast<void *>(mapped_buf_),
              cplat_mmap_get_address(map)); // [確認_正常系] - マップ済みアドレスがモックの返却バッファーであること。

    // Pre-Assert_2
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_正常系] - 再アタッチで CREATE_NEW の cplat_file_open が 1 回呼び出されること。
                                    // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返却する。
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_existing_rw(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - 再アタッチで CREATE_NEW が失敗し、既存オープンが成功すること。
                // [Pre-Assert手順] - CREATE_NEW はエラーを返し、既存オープンは番兵ハンドルを設定する。
    EXPECT_CALL(mock_cplat_, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = kMapSize;
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - 既存ファイルのサイズ取得が 1 回呼び出されること。
                // [Pre-Assert手順] - サイズ 64 を設定し、CPLAT_OK を返却する。

    // Act_2
    (void)cplat_mmap_detach(map, NULL);
    map = NULL;
    result =
        cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, 4096u, &map,
                             NULL); // [手順] - 既存ファイルに対し異なる create_size (4096) を指定して再アタッチする。

    // Assert_2
    ASSERT_EQ(CPLAT_OK,
              result); // [確認_正常系] - attach (既存ファイル) の戻り値が CPLAT_OK であること。
    EXPECT_EQ(kMapSize,
              cplat_mmap_get_size(
                  map)); // [確認_正常系] - 既存ファイルでは create_size が無視され、実サイズ 64 が報告されること。

    // Cleanup
    (void)cplat_mmap_detach(map, NULL);
}

// create_size に 0 を渡した新規作成が INVALID_ARGUMENT で失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_fails_when_create_size_is_zero_for_new_file)
{
    // Arrange
    cplat_mmap *map = NULL; // [状態] - 新規作成用のパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_close(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - create_size 0 のとき cplat_file_close が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_remove(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - create_size 0 のときファイルを閉じて削除すること。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, 0u, &map,
                                   NULL); // [手順] - create_size 0 で新規アタッチを試みる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - attach (create_size 0、新規作成) が CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ((cplat_mmap *)NULL, map); // [確認_異常系] - create_size 0 ではマップ ハンドルが設定されないこと。
}

// サイズ 0 の既存ファイルへのアタッチが失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_fails_for_empty_existing_file)
{
    // Arrange
    cplat_mmap *map = NULL; // [状態] - サイズ 0 の既存ファイルを開く前提を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - CREATE_NEW の cplat_file_open が 1 回呼び出されること。
                                    // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返却する。
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_existing_rw(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_異常系] - CREATE_NEW が失敗し、既存ファイルの再オープンが成功すること。
                // [Pre-Assert手順] - 再オープンで番兵ハンドルを設定する。
    EXPECT_CALL(mock_cplat_, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0u;
                return CPLAT_OK;
            }); // [Pre-Assert確認_異常系] - 既存ファイルのサイズ取得が 0 を返すこと。
                // [Pre-Assert手順] - サイズ 0 を設定し、CPLAT_OK を返却する。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                   NULL); // [手順] - サイズ 0 の既存ファイルへアタッチする。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 空の既存ファイルに対する cplat_mmap_attach の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ((cplat_mmap *)NULL, map); // [確認_異常系] - 空の既存ファイルではマップ ハンドルが設定されないこと。
}

// 既存ファイルを読み取り専用でマップできることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_read_only_maps_existing_file)
{
    // Arrange
    cplat_mmap *map = NULL; // [状態] - 読み取り専用で開く既存ファイルのパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_read_only(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - 読み取り専用の cplat_file_open が 1 回呼び出されること。
                // [Pre-Assert手順] - 番兵ハンドルを設定し、CPLAT_OK を返却する。
    // 読み取り専用のマップ API が呼び出されること。
    // [Pre-Assert手順] - テスト用バッファーを返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_mman_, mmap(_, _, _, _, kMapSize, PROT_READ, MAP_SHARED, kFakeFileHandle, 0))
        .WillOnce(Return(mapped_buf_));
    // [Pre-Assert確認_正常系] - mock_sys_mman_ の mmap(_, _, _, _, kMapSize, PROT_READ, MAP_SHARED, kFakeFileHandle, 0) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_windows_, CreateFileMappingA(_, _, _, kFakeFileHandle, _, PAGE_READONLY, _, _, _))
        .WillOnce(Return(kFakeMappingHandle));
    // [Pre-Assert確認_正常系] - mock_windows_ の CreateFileMappingA(_, _, _, kFakeFileHandle, _, PAGE_READONLY, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_windows_, MapViewOfFile(_, _, _, kFakeMappingHandle, FILE_MAP_READ, 0u, 0u, kMapSize))
        .WillOnce(Return(mapped_buf_));
    // [Pre-Assert確認_正常系] - mock_windows_ の MapViewOfFile(_, _, _, kFakeMappingHandle, FILE_MAP_READ, 0u, 0u, kMapSize) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_ONLY, 0u, &map,
                                   NULL); // [手順] - 既存ファイルを読み取り専用でアタッチする。

    // Assert
    ASSERT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 読み取り専用アタッチの戻り値が CPLAT_OK であること。
    ASSERT_NE((cplat_mmap *)NULL, map);
    // [確認_正常系] - `(cplat_mmap *)NULL` と `map` が異なること。
    EXPECT_EQ(kMapSize,
              cplat_mmap_get_size(map)); // [確認_正常系] - マップ サイズが既存ファイルの 64 バイトと一致すること。
    EXPECT_EQ(static_cast<void *>(mapped_buf_),
              cplat_mmap_get_address(
                  map)); // [確認_正常系] - 読み取り専用マップのアドレスがモックの返却バッファーであること。

    // Cleanup
    (void)cplat_mmap_detach(map, NULL);
}

// 読み取り専用アクセスで存在しないファイルを指定すると失敗することの確認 (新規作成しない)
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_read_only_fails_for_missing_file)
{
    // Arrange
    cplat_mmap *map = NULL; // [状態] - 存在しないファイルのパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_read_only(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - 読み取り専用の cplat_file_open が 1 回呼び出されること。
                                    // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返却する。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_ONLY, kMapSize, &map,
                                   NULL); // [手順] - 存在しないファイルを READ_ONLY でアタッチを試みる。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret);           // [確認_異常系] - attach (READ_ONLY、存在しないファイル) が失敗すること。
    EXPECT_EQ((cplat_mmap *)NULL, map); // [確認_異常系] - 欠落ファイルではマップ ハンドルが設定されないこと。
}

// 不正な引数で attach が INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_invalid_arguments_fail)
{
    // Arrange
    cplat_mmap *map = NULL;
    int invalid_access_value = 99;
    cplat_mmap_access invalid_access = (cplat_mmap_access)invalid_access_value;
    int null_path_result;
    int null_map_result;
    int empty_path_result;
    int invalid_access_result; // [状態] - 不正な access 値 99 を用意する。

    // Pre-Assert

    // Act
    null_path_result = cplat_mmap_attach(NULL, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                            NULL); // [手順] - path に NULL を指定してアタッチする。
    null_map_result = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, NULL,
                                           NULL); // [手順] - map に NULL を指定してアタッチする。
    empty_path_result = cplat_mmap_attach("", CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                             NULL); // [手順] - 空文字列の path を指定してアタッチする。
    invalid_access_result = cplat_mmap_attach(kPath, invalid_access, kMapSize, &map,
                                                 NULL); // [手順] - 不正な access 値でアタッチする。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_path_result); // [確認_異常系] - path NULL の cplat_mmap_attach の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_map_result); // [確認_異常系] - map NULL の cplat_mmap_attach の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        empty_path_result); // [確認_異常系] - 空文字列 path の cplat_mmap_attach の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_access_result); // [確認_異常系] - access 不正値の cplat_mmap_attach の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// NULL ハンドルに対する get_address/get_size/flush/detach が安全であることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, accessors_are_safe_for_null_handle)
{
    // Arrange
    void *address;
    size_t size;
    int flush_result;
    int detach_result;

    // Pre-Assert

    // Act
    address = cplat_mmap_get_address(NULL);                // [手順] - get_address に NULL を渡す。
    size = cplat_mmap_get_size(NULL);                      // [手順] - get_size に NULL を渡す。
    flush_result = cplat_mmap_flush(NULL, NULL, 0u, NULL); // [手順] - flush に NULL ハンドルを渡す。
    detach_result = cplat_mmap_detach(NULL, NULL);         // [手順] - detach に NULL ハンドルを渡す。

    // Assert
    EXPECT_EQ((void *)NULL, address); // [確認_異常系] - get_address(NULL) が NULL を返すこと。
    EXPECT_EQ((size_t)0, size);       // [確認_異常系] - get_size(NULL) が 0 を返すこと。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              flush_result);               // [確認_異常系] - flush(NULL) が CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(CPLAT_OK, detach_result); // [確認_異常系] - detach(NULL) が CPLAT_OK を返すこと。
}

// 指定したアドレス範囲の書き戻しが成功することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, flush_succeeds_for_explicit_address_range)
{
    // Arrange
    cplat_mmap *map = NULL;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map); // [状態] - 書き戻し対象のマップを用意する。

    // Pre-Assert
    // 先頭 1 byte を対象とする書き戻し API が呼び出されること。
    // [Pre-Assert手順] - 成功を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_mman_, msync(_, _, _, mapped_buf_, 1u, MS_SYNC)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - mock_sys_mman_ の msync(_, _, _, mapped_buf_, 1u, MS_SYNC) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_windows_, FlushViewOfFile(_, _, _, mapped_buf_, 1u)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - mock_windows_ の FlushViewOfFile(_, _, _, mapped_buf_, 1u) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_windows_, FlushFileBuffers(_, _, _, kFakeFileHandle)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_正常系] - mock_windows_ の FlushFileBuffers(_, _, _, kFakeFileHandle) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_mmap_flush(map, mapped_buf_, 1u,
                                  NULL); // [手順] - マップ先頭の 1 byte を指定して書き戻す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 明示したアドレス範囲に対する cplat_mmap_flush の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_mmap_detach(map, NULL);
}
