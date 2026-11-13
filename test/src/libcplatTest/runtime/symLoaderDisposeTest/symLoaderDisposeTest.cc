#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/sym_loader.h>

#include "sym_loader_internal.h"

#if defined(PLATFORM_LINUX)
    #include <mock_dlfcn.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::NiceMock;
using testing::Return;

class symLoaderDisposeTest : public Test
{
  protected:
    // [サブ手順 名前=symLoaderDisposeTest.plant_usable]
    void plant_usable(cplat_sym_loader_entry *entry, CPLAT_MODULE_HANDLE handle)
    {
        entry->impl = static_cast<cplat_sym_loader_entry_impl *>(cplat_calloc(1u, sizeof(*entry->impl)));
        ASSERT_NE(nullptr, entry->impl);
        // [状態確認] - 管理構造体を確保する cplat_calloc() の戻り値が nullptr 以外であること。
        entry->impl->usable = static_cast<cplat_sym_loader_usable *>(cplat_calloc(1u, sizeof(*entry->impl->usable)));
        ASSERT_NE(nullptr, entry->impl->usable);
        // [状態確認] - 利用側の列を確保する cplat_calloc() の戻り値が nullptr 以外であること。
#if defined(PLATFORM_WINDOWS)
        (void)handle;
        entry->impl->usable[0].handle = LoadLibraryW(L"kernel32.dll");
        ASSERT_NE(nullptr, entry->impl->usable[0].handle);
        // [状態確認] - テスト用モジュールのロードに成功すること。
#else
        entry->impl->usable[0].handle = handle;
#endif /* PLATFORM_WINDOWS */
        entry->impl->usable[0].lib_name = cplat_strdup("lib");
        entry->impl->usable[0].func_name = cplat_strdup("func");
        entry->impl->usable[0].func_ptr = kFakeFunc;
        ASSERT_NE(nullptr, entry->impl->usable[0].lib_name);
        // [状態確認] - lib を複製する cplat_strdup() の戻り値が nullptr 以外であること。
        ASSERT_NE(nullptr, entry->impl->usable[0].func_name);
        // [状態確認] - func を複製する cplat_strdup() の戻り値が nullptr 以外であること。
        entry->impl->usable_count = 1u;
        cplat_atomic_store_i32(&entry->resolved, 1, CPLAT_MEMORY_ORDER_RELAXED);
    }
    // [サブ手順終了]

    void *const kFakeFunc = reinterpret_cast<void *>(static_cast<uintptr_t>(0x52));
};

namespace
{
CPLAT_MODULE_HANDLE const kFakeHandle = reinterpret_cast<CPLAT_MODULE_HANDLE>(static_cast<uintptr_t>(0x51));
} // namespace

// 解放前に配列を検査し、不正な配列ではエントリを変更しないことの確認
TEST_F(symLoaderDisposeTest, validates_array_before_disposing)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("test_key");
    cplat_sym_loader_entry *entries[] = {&entry, nullptr};
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&entry.lock));
    // [状態確認] - エントリのロックを生成できること。
    cplat_atomic_store_i32(&entry.lock_state, 2, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_local_lock *lock = entry.lock;

    // Act / Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_sym_loader_dispose(nullptr, 1u)); // [確認_異常系] - NULL 配列を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_sym_loader_dispose(entries, 2u));           // [確認_異常系] - NULL 要素を拒否すること。
    EXPECT_EQ(lock, entry.lock);                                // [確認_異常系] - 不正な配列ではロックを保持すること。
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_dispose(nullptr, 0u)); // [確認_正常系] - 0 件の NULL 配列を受け付けること。
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_dispose(entries, 1u)); // [確認_正常系] - 正しい配列を解放すること。
    EXPECT_EQ(nullptr, entry.lock);                             // [確認_正常系] - ロックが解放されること。
    EXPECT_EQ(0, cplat_atomic_load_i32(&entry.lock_state,
                                       CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - ロック状態が初期値に戻ること。
}

// 解決済みエントリのモジュールが 1 回閉じられ、管理構造体が解放されることの確認
TEST_F(symLoaderDisposeTest, releases_handle_and_func_ptr_of_resolved_entry)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("test_key");
    cplat_sym_loader_entry *entries[] = {&entry};
    // [サブ手順参照 名前=symLoaderDisposeTest.plant_usable]
    plant_usable(&entry, kFakeHandle); // [状態] - モジュールを開いた利用側の列を 1 件用意する。
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_dlfcn> mock_dlfcn;
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, kFakeHandle)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - dlclose() が kFakeHandle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - dlclose() から 0 を返す。
#endif /* PLATFORM_LINUX */
    // 解放 API がエントリのハンドルを 1 回閉じること。

    // Act
    cplat_sym_loader_dispose(entries,
                             1u); // [手順] - 解決済みのエントリ 1 件を指定して cplat_sym_loader_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, entry.impl); // [確認_正常系] - entry.impl が nullptr であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry.failed,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry.failed を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。
}

// 管理構造体を持たないエントリが読み飛ばされることの確認
TEST_F(symLoaderDisposeTest, skips_entry_without_handle)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("test_key");
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 管理構造体を持たないエントリを用意する。
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_dlfcn> mock_dlfcn;
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, _)).Times(0);
    // [Pre-Assert確認_正常系] - dlclose() が呼び出されないこと。
#endif /* PLATFORM_LINUX */
    // ハンドルが無いとき解放 API が呼び出されないこと。

    // Act
    cplat_sym_loader_dispose(
        entries, 1u); // [手順] - 管理構造体を持たないエントリを指定して cplat_sym_loader_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, entry.impl); // [確認_正常系] - entry.impl が nullptr であること。
}

// 要素数 0 の指定で開いたモジュールが保持されることの確認
TEST_F(symLoaderDisposeTest, accepts_zero_length)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("test_key");
    cplat_sym_loader_entry *entries[] = {&entry};
    // [サブ手順参照 名前=symLoaderDisposeTest.plant_usable]
    plant_usable(&entry, kFakeHandle); // [状態] - モジュールを開いた利用側の列を 1 件用意する。
    CPLAT_MODULE_HANDLE original_handle = entry.impl->usable[0].handle;
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_dlfcn> mock_dlfcn;
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, _)).Times(0);
    // [Pre-Assert確認_正常系] - dlclose() が呼び出されないこと。
#endif /* PLATFORM_LINUX */
    // 要素数 0 のとき解放 API が呼び出されないこと。

    // Act
    cplat_sym_loader_dispose(entries, 0u); // [手順] - 要素数に 0 を指定して cplat_sym_loader_dispose() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, entry.impl); // [確認_正常系] - entry.impl が nullptr 以外であること。
    EXPECT_EQ(
        original_handle,
        entry.impl->usable[0]
            .handle); // [確認_正常系] - 利用側のハンドルが kFakeHandle と一致すること。(要素数 0 ではハンドルが保持されることの確認)

    // Cleanup
#if defined(PLATFORM_LINUX)
    testing::Mock::VerifyAndClearExpectations(&mock_dlfcn);
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, kFakeHandle)).WillOnce(Return(0));
#endif /* PLATFORM_LINUX */
    cplat_sym_loader_dispose(entries, 1u);
}

// 複数エントリがまとめて解放されることの確認
TEST_F(symLoaderDisposeTest, releases_multiple_entries)
{
    // Arrange
    cplat_sym_loader_entry first = CPLAT_SYM_LOADER_ENTRY_INIT("first");
    cplat_sym_loader_entry second = CPLAT_SYM_LOADER_ENTRY_INIT("second");
    cplat_sym_loader_entry *entries[] = {&first, &second};
    CPLAT_MODULE_HANDLE second_handle = reinterpret_cast<CPLAT_MODULE_HANDLE>(static_cast<uintptr_t>(0x53));
    // [サブ手順参照 名前=symLoaderDisposeTest.plant_usable]
    plant_usable(&first, kFakeHandle);
    // [サブ手順参照 名前=symLoaderDisposeTest.plant_usable]
    plant_usable(&second, second_handle); // [状態] - モジュールを開いたエントリを 2 件用意する。
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_dlfcn> mock_dlfcn;
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, kFakeHandle)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - dlclose() が kFakeHandle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - dlclose() から 0 を返す。
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, second_handle)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - dlclose() が second_handle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - dlclose() から 0 を返す。
#endif /* PLATFORM_LINUX */
    // 2 件のハンドルがそれぞれ閉じられること。

    // Act
    cplat_sym_loader_dispose(entries,
                             2u); // [手順] - 解決済みのエントリ 2 件を指定して cplat_sym_loader_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, first.impl);  // [確認_正常系] - first.impl が nullptr であること。
    EXPECT_EQ(nullptr, second.impl); // [確認_正常系] - second.impl が nullptr であること。
}

#if defined(PLATFORM_LINUX)
// 解放失敗時に残ったハンドルだけを保持し、次の解放で再試行することの確認
TEST_F(symLoaderDisposeTest, retries_only_failed_handles)
{
    // Arrange
    cplat_sym_loader_entry first = CPLAT_SYM_LOADER_ENTRY_INIT("first");
    cplat_sym_loader_entry second = CPLAT_SYM_LOADER_ENTRY_INIT("second");
    cplat_sym_loader_entry *entries[] = {&first, &second, &first};
    CPLAT_MODULE_HANDLE second_handle = reinterpret_cast<CPLAT_MODULE_HANDLE>(static_cast<uintptr_t>(0x53));
    NiceMock<Mock_dlfcn> mock_dlfcn;
    // [サブ手順参照 名前=symLoaderDisposeTest.plant_usable]
    plant_usable(&first, kFakeHandle);
    // [サブ手順参照 名前=symLoaderDisposeTest.plant_usable]
    plant_usable(&second, second_handle);

    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, kFakeHandle)).WillOnce(Return(-1)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - 最初のハンドルの解放が 2 回呼び出されること。
    // [Pre-Assert手順] - 最初は失敗し、再試行では成功する。
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, second_handle)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - 2 番目のハンドルの解放が 1 回呼び出されること。
    // [Pre-Assert手順] - 解放に成功する。

    // Act / Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_dispose(entries, 3u)); // [確認_異常系] - 解放失敗を返すこと。
    ASSERT_NE(nullptr, first.impl);                       // [確認_異常系] - 解放失敗した管理構造体が残ること。
    EXPECT_EQ(kFakeHandle, first.impl->usable[0].handle); // [確認_異常系] - 失敗したハンドルが残ること。
    EXPECT_EQ(1, first.impl->release_pending);            // [確認_異常系] - 解放待ちになること。
    EXPECT_EQ(nullptr, second.impl);                      // [確認_正常系] - 他のエントリが解放されること。
    EXPECT_EQ(1,
              cplat_atomic_load_i32(&first.failed, CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - 失敗状態になること。
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_dispose(entries, 3u)); // [確認_正常系] - 再試行に成功すること。
    EXPECT_EQ(nullptr, first.impl); // [確認_正常系] - 再試行後に管理構造体が解放されること。
}
#endif /* PLATFORM_LINUX */
