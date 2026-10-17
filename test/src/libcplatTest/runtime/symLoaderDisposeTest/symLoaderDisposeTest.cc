#include <testfw.h>
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
