#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/platform.h>
#include <cplat/runtime/sym_loader.h>

#if defined(PLATFORM_LINUX)
    #include <mock_dlfcn.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::NiceMock;
using testing::Return;

class symLoaderDisposeTest : public Test
{
};

namespace
{
CPLAT_MODULE_HANDLE const kFakeHandle = reinterpret_cast<CPLAT_MODULE_HANDLE>(static_cast<uintptr_t>(0x51));
void *const kFakeFunc = reinterpret_cast<void *>(static_cast<uintptr_t>(0x52));
} // namespace

// 解決済みエントリのハンドルと関数ポインターが解放されることの確認
#if defined(PLATFORM_LINUX)
TEST_F(symLoaderDisposeTest, releases_handle_and_func_ptr_of_resolved_entry)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("test_key", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry};
    entry.handle = kFakeHandle;
    cplat_atomic_store_ptr(&entry.func_ptr, kFakeFunc,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - ハンドルと関数ポインターが入ったエントリを用意する。
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
    EXPECT_EQ(nullptr, entry.handle); // [確認_正常系] - entry.handle が nullptr であること。
    EXPECT_EQ(
        nullptr,
        cplat_atomic_load_ptr(
            &entry.func_ptr,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry.func_ptr を読み込んだ cplat_atomic_load_ptr() の戻り値が nullptr であること。
}
#endif

// ハンドルを持たないエントリが読み飛ばされることの確認
TEST_F(symLoaderDisposeTest, skips_entry_without_handle)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("test_key", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 未解決のままハンドルを持たないエントリを用意する。
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
        entries, 1u); // [手順] - ハンドルを持たないエントリを指定して cplat_sym_loader_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, entry.handle); // [確認_正常系] - entry.handle が nullptr であること。
}

// 要素数 0 の指定で何も起こらないことの確認
TEST_F(symLoaderDisposeTest, accepts_zero_length)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("test_key", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry};
    entry.handle = kFakeHandle;
    cplat_atomic_store_ptr(&entry.func_ptr, kFakeFunc,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - ハンドル入りのエントリを用意する。
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
    EXPECT_EQ(
        kFakeHandle,
        entry
            .handle); // [確認_正常系] - entry.handle が kFakeHandle と一致すること。(要素数 0 ではハンドルが保持されることの確認)
}

// 複数エントリがまとめて解放されることの確認
#if defined(PLATFORM_LINUX)
TEST_F(symLoaderDisposeTest, releases_multiple_entries)
{
    // Arrange
    cplat_sym_loader_entry first = CPLAT_SYM_LOADER_ENTRY_INIT("first", void (*)(void));
    cplat_sym_loader_entry second = CPLAT_SYM_LOADER_ENTRY_INIT("second", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&first, &second};
    first.handle = kFakeHandle;
    cplat_atomic_store_ptr(&first.func_ptr, kFakeFunc, CPLAT_MEMORY_ORDER_RELAXED);
    second.handle = reinterpret_cast<CPLAT_MODULE_HANDLE>(static_cast<uintptr_t>(0x53));
    cplat_atomic_store_ptr(&second.func_ptr, reinterpret_cast<void *>(static_cast<uintptr_t>(0x54)),
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - ハンドル入りのエントリを 2 件用意する。
    #if defined(PLATFORM_LINUX)
    NiceMock<Mock_dlfcn> mock_dlfcn;
    #endif /* PLATFORM_LINUX */

    // Pre-Assert
    #if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, first.handle)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - dlclose() が first.handle を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - dlclose() から 0 を返す。
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, second.handle)).WillOnce(Return(0));
        // [Pre-Assert確認_正常系] - dlclose() が second.handle を指定して 1 回呼び出されること。
        // [Pre-Assert手順] - dlclose() から 0 を返す。
    #endif /* PLATFORM_LINUX */
    // 2 件のハンドルがそれぞれ閉じられること。

    // Act
    cplat_sym_loader_dispose(entries,
                             2u); // [手順] - 解決済みのエントリ 2 件を指定して cplat_sym_loader_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, first.handle);  // [確認_正常系] - first.handle が nullptr であること。
    EXPECT_EQ(nullptr, second.handle); // [確認_正常系] - second.handle が nullptr であること。
}
#endif

// NULL 要素があれば配列全体を解放前に拒否することの確認
TEST_F(symLoaderDisposeTest, rejects_invalid_array_before_releasing_resources)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("key", void (*)(void));
    entry.handle = kFakeHandle;
    cplat_sym_loader_entry *entries[] = {&entry, nullptr}; // [状態] - 有効要素の後に NULL を置く。

    // Pre-Assert

    // Act
    int actual = cplat_sym_loader_dispose(entries, 2); // [手順] - 配列全体を検査する。
    int empty_ret = cplat_sym_loader_dispose(nullptr, 0);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual); // [確認_異常系] - 不正配列を拒否すること。
    EXPECT_EQ(kFakeHandle, entry.handle);          // [確認_異常系] - 前方の要素も未解放であること。
    EXPECT_EQ(CPLAT_OK, empty_ret);                // [確認_正常系] - 空配列を許可すること。
}

#if defined(PLATFORM_LINUX)
// 解放失敗の所有権を保持し、残りの解放を続けることの確認
TEST_F(symLoaderDisposeTest, retains_failed_resource_and_continues_other_entries)
{
    // Arrange
    NiceMock<Mock_dlfcn> mock;
    cplat_sym_loader_entry first = CPLAT_SYM_LOADER_ENTRY_INIT("first", void (*)(void));
    cplat_sym_loader_entry second = CPLAT_SYM_LOADER_ENTRY_INIT("second", void (*)(void));
    first.handle = kFakeHandle;
    second.handle = reinterpret_cast<CPLAT_MODULE_HANDLE>(static_cast<uintptr_t>(0x53));
    cplat_sym_loader_entry *entries[] = {&first, &second}; // [状態] - 解放対象を 2 件用意する。

    // Pre-Assert
    EXPECT_CALL(mock, dlclose(_, _, _, first.handle))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 先頭の解放を 1 回試すこと。
    // [Pre-Assert手順] - 解放失敗を返す。
    EXPECT_CALL(mock, dlclose(_, _, _, second.handle))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 後続の解放を 1 回試すこと。
    // [Pre-Assert手順] - 解放成功を返す。

    // Act
    int actual = cplat_sym_loader_dispose(entries, 2); // [手順] - 解放を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual); // [確認_異常系] - 解放失敗を返すこと。
    EXPECT_EQ(kFakeHandle, first.handle); // [確認_異常系] - 失敗した参照を保持すること。
    EXPECT_EQ(nullptr, second.handle);    // [確認_正常系] - 後続を解放すること。
}
#endif

#if defined(PLATFORM_WINDOWS)
// Windows の実モジュール参照と所有ロックを解放することの確認
TEST_F(symLoaderDisposeTest, releases_real_windows_module_and_owned_lock)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("key", void (*)(void));
    entry.handle = LoadLibraryW(L"kernel32.dll");
    ASSERT_NE(nullptr, entry.handle); // [状態確認] - 解放できる実モジュール参照を取得すること。
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&entry.lock)); // [状態確認] - 所有ロックを生成できること。
    cplat_atomic_store_i32(&entry.lock_state, 2, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_atomic_store_i32(&entry.resolved, 1, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 解決済み状態の資源を用意する。

    // Pre-Assert

    // Act
    int actual = cplat_sym_loader_dispose(entries, 1); // [手順] - 実資源を解放する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual); // [確認_正常系] - 解放に成功すること。
    EXPECT_EQ(nullptr, entry.handle);
    EXPECT_EQ(nullptr, entry.lock);
    EXPECT_EQ(0, cplat_atomic_load_i32(&entry.resolved, CPLAT_MEMORY_ORDER_RELAXED));
    EXPECT_EQ(0, cplat_atomic_load_i32(&entry.lock_state,
                                       CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - 未解決状態に戻ること。
}
#endif
