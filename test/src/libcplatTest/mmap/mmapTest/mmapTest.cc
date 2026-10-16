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
    cplat_mmap *map = nullptr;
    int result; // [状態] - 新規作成用のパス mmap.dat とハンドル格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_file_open() が flags_create_new() の戻り値を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - センチネル ハンドルを設定し、CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat_, cplat_file_set_size(_, kMapSize, _))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_file_set_size() が kMapSize を指定して 1 回呼び出されること。(create_size の 64 を指定したことの確認)
                        // [Pre-Assert手順] - CPLAT_OK を返却する。

    // Act
    result = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                               nullptr); // [手順] - create_size 64 で新規アタッチする。

    // Assert
    ASSERT_EQ(CPLAT_OK, result); // [確認_正常系] - 新規作成の cplat_mmap_attach() の戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr,
              map); // [確認_正常系] - 新規作成の map が nullptr 以外であること。(マップ ハンドルが設定されることの確認)
    EXPECT_EQ(
        kMapSize,
        cplat_mmap_get_size(
            map)); // [確認_正常系] - cplat_mmap_get_size() の戻り値が kMapSize であること。(新規作成時に指定した create_size と一致することの確認)
    EXPECT_EQ(
        static_cast<void *>(mapped_buf_),
        cplat_mmap_get_address(
            map)); // [確認_正常系] - cplat_mmap_get_address() の戻り値が static_cast<void *>(mapped_buf_) と一致すること。(モックの返却バッファーであることの確認)

    // Pre-Assert_2
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_正常系] - cplat_file_open() が flags_create_new() の戻り値を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返却する。
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_existing_rw(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_file_open() が flags_existing_rw() の戻り値を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - CREATE_NEW はエラーを返し、既存オープンはセンチネル ハンドルを設定する。
    EXPECT_CALL(mock_cplat_, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = kMapSize;
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - サイズ 64 を設定し、CPLAT_OK を返却する。

    // Act_2
    (void)cplat_mmap_detach(map, nullptr);
    map = nullptr;
    result =
        cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, 4096u, &map,
                          nullptr); // [手順] - 既存ファイルに対し異なる create_size (4096) を指定して再アタッチする。

    // Assert_2
    ASSERT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 既存ファイルの cplat_mmap_attach() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        kMapSize,
        cplat_mmap_get_size(
            map)); // [確認_正常系] - 再アタッチ後の cplat_mmap_get_size() の戻り値が kMapSize であること。(既存ファイルでは create_size が無視され、実サイズ 64 が報告されることの確認)

    // Cleanup
    (void)cplat_mmap_detach(map, nullptr);
}

// create_size に 0 を渡した新規作成が INVALID_ARGUMENT で失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_fails_when_create_size_is_zero_for_new_file)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - 新規作成用のパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_close(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_remove(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - cplat_remove() が 1 回呼び出されること。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, 0u, &map,
                                       nullptr); // [手順] - create_size 0 で新規アタッチを試みる。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - create_size が 0 の新規作成に対する cplat_mmap_attach() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        map); // [確認_異常系] - map が nullptr であること。(create_size が 0 ではマップ ハンドルが設定されないことの確認)
}

// サイズ 0 の既存ファイルへのアタッチが失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_fails_for_empty_existing_file)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - サイズ 0 の既存ファイルを開く前提を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_create_new() の戻り値を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返却する。
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_existing_rw(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_existing_rw() の戻り値を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - 再オープンでセンチネル ハンドルを設定する。
    EXPECT_CALL(mock_cplat_, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0u;
                return CPLAT_OK;
            }); // [Pre-Assert確認_異常系] - cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - サイズ 0 を設定し、CPLAT_OK を返却する。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                       nullptr); // [手順] - サイズ 0 の既存ファイルへアタッチする。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 空の既存ファイルに対する cplat_mmap_attach() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        map); // [確認_異常系] - map が nullptr であること。(空の既存ファイルではマップ ハンドルが設定されないことの確認)
}

// 既存ファイルを読み取り専用でマップできることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_read_only_maps_existing_file)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - 読み取り専用で開く既存ファイルのパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_read_only(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_file_open() が flags_read_only() の戻り値を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - センチネル ハンドルを設定し、CPLAT_OK を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_mman_, mmap(_, _, _, _, kMapSize, PROT_READ, MAP_SHARED, kFakeFileHandle, 0))
        .WillOnce(Return(
            mapped_buf_)); // [Pre-Assert確認_正常系] - mmap() が kMapSize、PROT_READ、MAP_SHARED、kFakeFileHandle、0 を指定して 1 回呼び出されること。
                           // [Pre-Assert手順] - テスト用バッファーを返却する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_windows_, CreateFileMappingA(_, _, _, kFakeFileHandle, _, PAGE_READONLY, _, _, _))
        .WillOnce(Return(
            kFakeMappingHandle)); // [Pre-Assert確認_正常系] - CreateFileMappingA() が kFakeFileHandle、PAGE_READONLY を指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - kFakeMappingHandle を返却する。
    EXPECT_CALL(mock_windows_, MapViewOfFile(_, _, _, kFakeMappingHandle, FILE_MAP_READ, 0u, 0u, kMapSize))
        .WillOnce(Return(
            mapped_buf_)); // [Pre-Assert確認_正常系] - MapViewOfFile() が kFakeMappingHandle、FILE_MAP_READ、0u、0u、kMapSize を指定して 1 回呼び出されること。
                           // [Pre-Assert手順] - テスト用バッファーを返却する。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_ONLY, 0u, &map,
                                       nullptr); // [手順] - 既存ファイルを読み取り専用でアタッチする。

    // Assert
    ASSERT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 読み取り専用の cplat_mmap_attach() の戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, map); // [確認_正常系] - map が nullptr 以外であること。
    EXPECT_EQ(
        kMapSize,
        cplat_mmap_get_size(
            map)); // [確認_正常系] - cplat_mmap_get_size() の戻り値が kMapSize であること。(既存ファイルの 64 バイトと一致することの確認)
    EXPECT_EQ(
        static_cast<void *>(mapped_buf_),
        cplat_mmap_get_address(
            map)); // [確認_正常系] - cplat_mmap_get_address() の戻り値が static_cast<void *>(mapped_buf_) と一致すること。(モックの返却バッファーであることの確認)

    // Cleanup
    (void)cplat_mmap_detach(map, nullptr);
}

// 読み取り専用アクセスで存在しないファイルを指定すると失敗することの確認 (新規作成しない)
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_read_only_fails_for_missing_file)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - 存在しないファイルのパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_read_only(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_read_only() の戻り値を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返却する。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_ONLY, kMapSize, &map,
                                       nullptr); // [手順] - 存在しないファイルを READ_ONLY でアタッチを試みる。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - 存在しないファイルに対する読み取り専用の cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        nullptr,
        map); // [確認_異常系] - map が nullptr であること。(欠落ファイルではマップ ハンドルが設定されないことの確認)
}

// 不正な引数で attach が INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, attach_invalid_arguments_fail)
{
    // Arrange
    cplat_mmap *map = nullptr;
    int invalid_access_value = 99;
    cplat_mmap_access invalid_access = (cplat_mmap_access)invalid_access_value;
    int null_path_result;
    int null_map_result;
    int empty_path_result;
    int invalid_access_result; // [状態] - 不正な access 値 99 を用意する。

    // Pre-Assert

    // Act
    null_path_result = cplat_mmap_attach(nullptr, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                         nullptr); // [手順] - path に NULL を指定してアタッチする。
    null_map_result = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, nullptr,
                                        nullptr); // [手順] - map に NULL を指定してアタッチする。
    empty_path_result = cplat_mmap_attach("", CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                          nullptr); // [手順] - 空文字列の path を指定してアタッチする。
    invalid_access_result = cplat_mmap_attach(kPath, invalid_access, kMapSize, &map,
                                              nullptr); // [手順] - 不正な access 値でアタッチする。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_path_result); // [確認_異常系] - path が NULL の cplat_mmap_attach() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_map_result); // [確認_異常系] - map が NULL の cplat_mmap_attach() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        empty_path_result); // [確認_異常系] - 空文字列の path に対する cplat_mmap_attach() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        invalid_access_result); // [確認_異常系] - 不正な access に対する cplat_mmap_attach() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
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
    address = cplat_mmap_get_address(nullptr); // [手順] - cplat_mmap_get_address() に NULL を渡す。
    size = cplat_mmap_get_size(nullptr);       // [手順] - cplat_mmap_get_size() に NULL を渡す。
    flush_result =
        cplat_mmap_flush(nullptr, nullptr, 0u, nullptr); // [手順] - cplat_mmap_flush() に NULL ハンドルを渡す。
    detach_result = cplat_mmap_detach(nullptr, nullptr); // [手順] - cplat_mmap_detach() に NULL ハンドルを渡す。

    // Assert
    EXPECT_EQ(nullptr,
              address); // [確認_異常系] - NULL に対する cplat_mmap_get_address() の戻り値が nullptr であること。
    EXPECT_EQ((size_t)0, size); // [確認_異常系] - NULL に対する cplat_mmap_get_size() の戻り値が 0 であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        flush_result); // [確認_異常系] - NULL に対する cplat_mmap_flush() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK,
              detach_result); // [確認_異常系] - NULL に対する cplat_mmap_detach() の戻り値が CPLAT_OK であること。
}

// 指定したアドレス範囲の書き戻しが成功することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapTest, flush_succeeds_for_explicit_address_range)
{
    // Arrange
    cplat_mmap *map = nullptr;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map); // [状態] - 書き戻し対象のマップを用意する。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_mman_, msync(_, _, _, mapped_buf_, 1u, MS_SYNC))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - msync() が mapped_buf_、1u、MS_SYNC を指定して 1 回呼び出されること。(先頭 1 byte を指定したことの確認)
                 // [Pre-Assert手順] - 成功を返却する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_windows_, FlushViewOfFile(_, _, _, mapped_buf_, 1u))
        .WillOnce(Return(
            TRUE)); // [Pre-Assert確認_正常系] - FlushViewOfFile() が mapped_buf_、1u を指定して 1 回呼び出されること。(先頭 1 byte を指定したことの確認)
                    // [Pre-Assert手順] - 成功を返却する。
    EXPECT_CALL(mock_windows_, FlushFileBuffers(_, _, _, kFakeFileHandle))
        .WillOnce(Return(
            TRUE)); // [Pre-Assert確認_正常系] - FlushFileBuffers() が kFakeFileHandle を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - 成功を返却する。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_mmap_flush(map, mapped_buf_, 1u,
                                      nullptr); // [手順] - マップ先頭の 1 byte を指定して書き戻す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 明示したアドレス範囲に対する cplat_mmap_flush() の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_mmap_detach(map, nullptr);
}
