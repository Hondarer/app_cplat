#include <testfw.h>
#include "mmapTestCommon.h"

#include <errno.h>

using testing::Assign;
using testing::DoAll;

class mmapFailureInjectionTest : public mmapTestFixture
{
};

// 読み取り専用ファイルのサイズ取得に失敗した場合にファイルを閉じて失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_returns_error_when_read_only_size_lookup_fails)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - 読み取り専用アタッチ用のパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_read_only(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_read_only() の戻り値を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_open() から、センチネル ハンドルを設定して CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat_, cplat_file_get_size(_, _, _))
        .WillOnce(
            Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_get_size() が 1 回呼び出されること。
                                        // [Pre-Assert手順] - cplat_file_get_size() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat_, cplat_file_close(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回呼び出されること。

    // Act
    int actual_ret = cplat_mmap_attach(
        kPath, CPLAT_MMAP_ACCESS_READ_ONLY, 0u, &map,
        nullptr); // [手順] - cplat_file_get_size() が失敗する状態で、読み取り専用を指定して cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - サイズ取得失敗時の cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        nullptr,
        map); // [確認_異常系] - map が nullptr であること。(サイズ取得失敗時にマップ ハンドルが設定されないことの確認)
}

// 新規ファイルのサイズ設定に失敗した場合にファイルを削除して失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_returns_error_when_new_file_size_setting_fails)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - 新規作成するマップのパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_set_size(_, _, _))
        .WillOnce(
            Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_set_size() が 1 回呼び出されること。
                                        // [Pre-Assert手順] - cplat_file_set_size() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat_, cplat_file_close(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_remove(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - cplat_remove() が 1 回呼び出されること。

    // Act
    int actual_ret = cplat_mmap_attach(
        kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
        nullptr); // [手順] - cplat_file_set_size() が失敗する状態で、新規ファイルのパスを指定して cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - サイズ設定失敗時の cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        nullptr,
        map); // [確認_異常系] - map が nullptr であること。(サイズ設定失敗時にマップ ハンドルが設定されないことの確認)
}

// 既存ファイルの再オープンに失敗した場合にアタッチが失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_returns_error_when_existing_file_reopen_fails)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - 既存ファイル再オープン失敗用のパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_create_new() の戻り値を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - cplat_file_open() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_existing_rw(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_existing_rw() の戻り値を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - cplat_file_open() から、再オープンでも CPLAT_ERR_UNKNOWN を返す。

    // Act
    int actual_ret = cplat_mmap_attach(
        kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
        nullptr); // [手順] - cplat_file_open() が再オープンで失敗する状態で、既存ファイルを指定して cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - 再オープン失敗時の cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        nullptr,
        map); // [確認_異常系] - map が nullptr であること。(再オープン失敗時にマップ ハンドルが設定されないことの確認)
}

// 既存ファイルのサイズ取得に失敗した場合にファイルを閉じて失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_returns_error_when_existing_size_lookup_fails)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - 既存ファイルのサイズ取得失敗用のパス mmap.dat を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_create_new(), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_create_new() の戻り値を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - cplat_file_open() から 1 回目は CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat_, cplat_file_open(_, _, flags_existing_rw(), _))
        .WillOnce(
            [](cplat_file *file, const char *, int flags, cplat_error *)
            {
                fill_open_file(file, flags);
                return CPLAT_OK;
            }); // [Pre-Assert確認_異常系] - cplat_file_open() が flags_existing_rw() の戻り値を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_file_open() から、再オープンとしてセンチネル ハンドルを設定して CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat_, cplat_file_get_size(_, _, _))
        .WillOnce(
            Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_get_size() が 1 回呼び出されること。
                                        // [Pre-Assert手順] - cplat_file_get_size() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat_, cplat_file_close(_, _))
        .Times(1); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回呼び出されること。

    // Act
    int actual_ret = cplat_mmap_attach(
        kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
        nullptr); // [手順] - 既存ファイルのサイズ取得が失敗する状態で cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - 既存ファイルのサイズ取得失敗時の cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        nullptr,
        map); // [確認_異常系] - map が nullptr であること。(既存ファイルのサイズ取得失敗時にマップ ハンドルが設定されないことの確認)
}

// ファイルのクローズに失敗した場合に detach が失敗を返すことの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, detach_reports_error_when_file_close_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    cplat_error detail;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map); // [状態] - detach 前のマップと詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_file_close(_, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - cplat_file_close() から CPLAT_ERR_UNKNOWN を返す。

    // Act
    int actual_ret =
        cplat_mmap_detach(map,
                          &detail); // [手順] - cplat_file_close() が失敗する状態で cplat_mmap_detach() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - ファイル クローズ失敗時の cplat_mmap_detach() の戻り値が CPLAT_OK 以外であること。
}

// マップ ハンドルの確保に失敗した場合にアタッチが失敗することの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_returns_out_of_memory_when_handle_allocation_fails)
{
    // Arrange
    cplat_mmap *map = nullptr; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_calloc(_, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_calloc() が 1 回以上呼び出されること。(ハンドル確保で呼び出されることの確認)
                          // [Pre-Assert手順] - cplat_calloc() から、ハンドル確保で nullptr を返す。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                       nullptr); // [手順] - cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - ハンドル確保失敗時の cplat_mmap_attach() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(nullptr,
              map); // [確認_異常系] - map が nullptr であること。(マップ ハンドルが設定されないことの確認)
}

#if defined(PLATFORM_LINUX)
// メモリ マップの作成に失敗した場合に errno が通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_reports_errno_when_mmap_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_mman_, mmap(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, ENOMEM),
                        Return(MAP_FAILED))); // [Pre-Assert確認_異常系] - mmap() が 1 回呼び出されること。
    // [Pre-Assert手順] - mmap() から、errno に ENOMEM を設定して MAP_FAILED を返す。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                       &detail); // [手順] - cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(nullptr,
              map); // [確認_異常系] - map が nullptr であること。(マップ ハンドルが設定されないことの確認)
    EXPECT_EQ(ENOMEM,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が ENOMEM であること。
}

// 書き戻しに失敗した場合に errno が通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, flush_reports_errno_when_msync_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map);
    cplat_error detail; // [状態] - アタッチ済みのメモリ マップと詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_mman_, msync(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EIO),
                        Return(-1))); // [Pre-Assert確認_異常系] - msync() が 1 回呼び出されること。
                                      // [Pre-Assert手順] - msync() から、errno に EIO を設定して -1 を返す。

    // Act
    int actual_ret = cplat_mmap_flush(map, nullptr, 0u, &detail); // [手順] - cplat_mmap_flush() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_mmap_flush() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(EIO,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EIO であること。

    // Cleanup
    (void)cplat_mmap_detach(map, nullptr);
}

// マップ解除に失敗した場合に errno が通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, detach_reports_errno_when_munmap_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    cplat_error detail;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map); // [状態] - アタッチ済みのメモリ マップを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_mman_, munmap(_, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EIO),
                        Return(-1))); // [Pre-Assert確認_異常系] - munmap() が 1 回呼び出されること。
                                      // [Pre-Assert手順] - munmap() から、errno に EIO を設定して -1 を返す。

    // Act
    int actual_ret = cplat_mmap_detach(map, &detail); // [手順] - cplat_mmap_detach() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - munmap() の失敗時の cplat_mmap_detach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(EIO,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EIO であること。
}

#elif defined(PLATFORM_WINDOWS)
// CreateFileMapping に失敗した場合に Windows エラーが通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_reports_error_when_create_file_mapping_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, CreateFileMappingA(_, _, _, _, _, _, _, _, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - CreateFileMappingA() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - CreateFileMappingA() から nullptr を返す。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(
            Return(ERROR_NOT_ENOUGH_MEMORY)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - GetLastError() から ERROR_NOT_ENOUGH_MEMORY を返す。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                       &detail); // [手順] - cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(nullptr,
              map); // [確認_異常系] - map が nullptr であること。(マップ ハンドルが設定されないことの確認)
    EXPECT_EQ(
        static_cast<unsigned long>(ERROR_NOT_ENOUGH_MEMORY),
        cplat_error_get_windows_error(
            &detail)); // [確認_異常系] - cplat_error_get_windows_error() の戻り値が ERROR_NOT_ENOUGH_MEMORY と一致すること。
}

// MapViewOfFile に失敗した場合に Windows エラーが通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, attach_reports_error_when_map_view_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, MapViewOfFile(_, _, _, _, _, _, _, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - MapViewOfFile() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - MapViewOfFile() から nullptr を返す。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(
            Return(ERROR_NOT_ENOUGH_MEMORY)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - GetLastError() から ERROR_NOT_ENOUGH_MEMORY を返す。
    EXPECT_CALL(mock_windows_, CloseHandle(_, _, _, kFakeMappingHandle))
        .Times(1); // [Pre-Assert確認_異常系] - CloseHandle() が kFakeMappingHandle を指定して 1 回呼び出されること。

    // Act
    int actual_ret = cplat_mmap_attach(kPath, CPLAT_MMAP_ACCESS_READ_WRITE, kMapSize, &map,
                                       &detail); // [手順] - cplat_mmap_attach() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_mmap_attach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(nullptr,
              map); // [確認_異常系] - map が nullptr であること。(マップ ハンドルが設定されないことの確認)
    EXPECT_EQ(
        static_cast<unsigned long>(ERROR_NOT_ENOUGH_MEMORY),
        cplat_error_get_windows_error(
            &detail)); // [確認_異常系] - cplat_error_get_windows_error() の戻り値が ERROR_NOT_ENOUGH_MEMORY と一致すること。
}

// FlushViewOfFile に失敗した場合に Windows エラーが通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, flush_reports_error_when_flush_view_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map);
    cplat_error detail; // [状態] - アタッチ済みのメモリ マップと詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, FlushViewOfFile(_, _, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - FlushViewOfFile() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FlushViewOfFile() から FALSE を返す。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_LOCK_VIOLATION)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                                 // [Pre-Assert手順] - GetLastError() から ERROR_LOCK_VIOLATION を返す。

    // Act
    int actual_ret = cplat_mmap_flush(map, nullptr, 0u, &detail); // [手順] - cplat_mmap_flush() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_mmap_flush() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        static_cast<unsigned long>(ERROR_LOCK_VIOLATION),
        cplat_error_get_windows_error(
            &detail)); // [確認_異常系] - cplat_error_get_windows_error() の戻り値が ERROR_LOCK_VIOLATION と一致すること。

    // Cleanup
    (void)cplat_mmap_detach(map, nullptr);
}

// FlushFileBuffers に失敗した場合に Windows エラーが通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, flush_reports_error_when_flush_file_buffers_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map);
    cplat_error detail; // [状態] - アタッチ済みのメモリ マップと詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, FlushViewOfFile(_, _, _, _, _))
        .WillOnce(Return(TRUE)); // [Pre-Assert確認_異常系] - FlushViewOfFile() が 1 回呼び出されること。
                                 // [Pre-Assert手順] - FlushViewOfFile() から TRUE を返す。
    EXPECT_CALL(mock_windows_, FlushFileBuffers(_, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - FlushFileBuffers() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FlushFileBuffers() から FALSE を返す。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_LOCK_VIOLATION)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                                 // [Pre-Assert手順] - GetLastError() から ERROR_LOCK_VIOLATION を返す。

    // Act
    int actual_ret = cplat_mmap_flush(map, nullptr, 0u, &detail); // [手順] - cplat_mmap_flush() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_mmap_flush() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        static_cast<unsigned long>(ERROR_LOCK_VIOLATION),
        cplat_error_get_windows_error(
            &detail)); // [確認_異常系] - cplat_error_get_windows_error() の戻り値が ERROR_LOCK_VIOLATION と一致すること。

    // Cleanup
    (void)cplat_mmap_detach(map, nullptr);
}

// UnmapViewOfFile に失敗した場合に Windows エラーが通知されることの確認
// [サブ手順参照 名前=mmapTestFixture.SetUp]
TEST_F(mmapFailureInjectionTest, detach_reports_error_when_unmap_view_fails)
{
    // Arrange
    cplat_mmap *map = nullptr;
    cplat_error detail;
    // [サブ手順参照 名前=mmapTestFixture.attachNewFile]
    attachNewFile(&map); // [状態] - アタッチ済みのメモリ マップを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, UnmapViewOfFile(_, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - UnmapViewOfFile() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - UnmapViewOfFile() から FALSE を返す。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(
            Return(ERROR_INVALID_PARAMETER)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - GetLastError() から ERROR_INVALID_PARAMETER を返す。

    // Act
    int actual_ret = cplat_mmap_detach(map, &detail); // [手順] - cplat_mmap_detach() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret); // [確認_異常系] - UnmapViewOfFile() の失敗時の cplat_mmap_detach() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(
        static_cast<unsigned long>(ERROR_INVALID_PARAMETER),
        cplat_error_get_windows_error(
            &detail)); // [確認_異常系] - cplat_error_get_windows_error() の戻り値が ERROR_INVALID_PARAMETER と一致すること。
}

#endif /* PLATFORM_ */
