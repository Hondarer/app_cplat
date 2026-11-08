#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/sym_loader.h>

#include <string>

#include "sym_loader_internal.h"
#include "sym_loader_resolve.inject.h"

#if defined(PLATFORM_LINUX)
    #include <mock_dlfcn.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{
// [サブ手順 名前=symLoaderResolveTest.count]
size_t test_sym_loader_count(cplat_sym_loader_entry *entry, int expected = CPLAT_OK)
{
    size_t value = 0u;
    EXPECT_EQ(expected, cplat_sym_loader_count(entry, &value)); // [確認_正常系] - 件数取得の結果コードが expected と一致すること。
    return value;
}
// [サブ手順終了]

// [サブ手順 名前=symLoaderResolveTest.reject_count]
size_t test_sym_loader_reject_count(cplat_sym_loader_entry *entry)
{
    size_t value = 0u;
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_reject_count(entry, &value)); // [確認_正常系] - 診断件数の取得に成功すること。
    return value;
}
// [サブ手順終了]

// [サブ手順 名前=symLoaderResolveTest.get]
void *test_sym_loader_get(cplat_sym_loader_entry *entry, size_t index, int expected = CPLAT_OK)
{
    void *value = nullptr;
    EXPECT_EQ(expected, cplat_sym_loader_get(entry, index, &value)); // [確認_正常系] - アドレス取得の結果コードが expected と一致すること。
    return value;
}
// [サブ手順終了]

// [サブ手順 名前=symLoaderResolveTest.is_default]
int test_sym_loader_is_default(cplat_sym_loader_entry *entry)
{
    int value = 0;
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_is_default(entry, &value)); // [確認_正常系] - 明示的既定値の取得に成功すること。
    return value;
}
// [サブ手順終了]

// [サブ手順 名前=symLoaderResolveTest.is_failed]
int test_sym_loader_is_failed(cplat_sym_loader_entry *entry)
{
    int value = 0;
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_is_failed(entry, &value)); // [確認_正常系] - 失敗状態の取得に成功すること。
    return value;
}
// [サブ手順終了]

void complete_entry_lock_initialization(cplat_sym_loader_entry *entry)
{
    cplat_atomic_store_i32(&entry->lock_state, 2, CPLAT_MEMORY_ORDER_RELAXED);
}
} // namespace

class symLoaderResolveTest : public Test
{
  protected:
    cplat_sym_loader_entry entry_ = CPLAT_SYM_LOADER_ENTRY_INIT("test_key");

    // [サブ手順 名前=symLoaderResolveTest.set_names]
    void set_names(const char *lib_name, const char *func_name)
    {
        cplat_sym_loader_name_pair *grown;
        char *lib_copy;
        char *func_copy;

        if (entry_.impl == nullptr)
        {
            entry_.impl =
                static_cast<cplat_sym_loader_entry_impl *>(cplat_calloc(1u, sizeof(*entry_.impl)));
            ASSERT_NE(nullptr, entry_.impl);
            // [状態確認] - 管理構造体を確保する cplat_calloc() の戻り値が nullptr 以外であること。
        }

        grown = static_cast<cplat_sym_loader_name_pair *>(
            cplat_realloc(entry_.impl->pending, entry_.impl->pending_count + 1u, sizeof(*grown)));
        ASSERT_NE(nullptr, grown);
        // [状態確認] - pending を伸ばす cplat_realloc() の戻り値が nullptr 以外であること。
        entry_.impl->pending = grown;
        lib_copy = cplat_strdup(lib_name);
        func_copy = cplat_strdup(func_name);
        ASSERT_NE(nullptr, lib_copy);
        // [状態確認] - lib_name を複製する cplat_strdup() の戻り値が nullptr 以外であること。
        ASSERT_NE(nullptr, func_copy);
        // [状態確認] - func_name を複製する cplat_strdup() の戻り値が nullptr 以外であること。
        grown[entry_.impl->pending_count].lib_name = lib_copy;
        grown[entry_.impl->pending_count].func_name = func_copy;
        entry_.impl->pending_count += 1u;
    }
    // [サブ手順終了]

    // [サブ手順 名前=symLoaderResolveTest.TearDown]
    void TearDown() override
    {
        cplat_sym_loader_entry *entries[] = {&entry_};

        cplat_sym_loader_dispose(entries, 1u);
    }
    // [サブ手順終了]
};

// 不正な入力と範囲外添字を拒否して出力を維持することの確認
TEST_F(symLoaderResolveTest, rejects_invalid_arguments_without_changing_outputs)
{
    size_t count = 42u;
    int value = 42;
    int reason = 42;
    void *address = reinterpret_cast<void *>(1);
    const char *lib_name = "kept";
    const char *func_name = "kept";

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_count(nullptr, &count)); // [確認_異常系] - NULL エントリを拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_count(&entry_, nullptr)); // [確認_異常系] - NULL 件数出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_get(nullptr, 0u, &address)); // [確認_異常系] - NULL エントリを拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_get(&entry_, 0u, nullptr)); // [確認_異常系] - NULL アドレス出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_reject_count(nullptr, &count)); // [確認_異常系] - NULL エントリを拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_reject_count(&entry_, nullptr)); // [確認_異常系] - NULL 件数出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_is_default(nullptr, &value)); // [確認_異常系] - NULL エントリを拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_is_default(&entry_, nullptr)); // [確認_異常系] - NULL 状態出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_is_failed(nullptr, &value)); // [確認_異常系] - NULL エントリを拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_is_failed(&entry_, nullptr)); // [確認_異常系] - NULL 状態出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_sym_loader_reject_get(nullptr, 0u, &lib_name, &func_name, &reason)); // [確認_異常系] - NULL エントリを拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_sym_loader_reject_get(&entry_, 0u, nullptr, &func_name, &reason)); // [確認_異常系] - NULL ライブラリ名出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, nullptr, &reason)); // [確認_異常系] - NULL 関数名出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name, nullptr)); // [確認_異常系] - NULL 診断理由出力を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, cplat_sym_loader_get(&entry_, 0u, &address)); // [確認_異常系] - 範囲外を拒否する。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name, &reason)); // [確認_異常系] - 診断の範囲外を拒否する。
    EXPECT_EQ(42u, count); // [確認_異常系] - 件数を変更しない。
    EXPECT_EQ(42, value); // [確認_異常系] - 状態値を変更しない。
    EXPECT_EQ(42, reason); // [確認_異常系] - 診断理由を変更しない。
    EXPECT_EQ(reinterpret_cast<void *>(1), address); // [確認_異常系] - アドレスを変更しない。
    EXPECT_STREQ("kept", lib_name); // [確認_異常系] - ライブラリ名を変更しない。
    EXPECT_STREQ("kept", func_name); // [確認_異常系] - 関数名を変更しない。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 解放待ちの間は取得を拒否し、失敗状態を維持することの確認
TEST_F(symLoaderResolveTest, refuses_access_while_release_is_pending)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("default", "default");
    ASSERT_NE(nullptr, entry_.impl);
    entry_.impl->release_pending = 1;
    cplat_atomic_store_i32(&entry_.failed, 1, CPLAT_MEMORY_ORDER_RELAXED);
    size_t count = 42u;
    int value = 42;
    int reason = 42;
    void *address = reinterpret_cast<void *>(1);
    const char *lib_name = "kept";
    const char *func_name = "kept";
    cplat_sym_loader_entry *entries[] = {&entry_};

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_count(&entry_, &count)); // [確認_異常系] - 件数の取得を拒否する。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_get(&entry_, 0u, &address)); // [確認_異常系] - アドレスの取得を拒否する。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_reject_count(&entry_, &count)); // [確認_異常系] - 診断件数の取得を拒否する。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name, &reason)); // [確認_異常系] - 診断の取得を拒否する。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_is_default(&entry_, &value)); // [確認_異常系] - 既定値の取得を拒否する。
    EXPECT_EQ(CPLAT_ERR_BUSY, cplat_sym_loader_init(entries, 1u, nullptr)); // [確認_異常系] - 設定の再適用を拒否する。
    EXPECT_EQ(42u, count); // [確認_異常系] - 件数を変更しない。
    EXPECT_EQ(reinterpret_cast<void *>(1), address); // [確認_異常系] - アドレスを変更しない。
    EXPECT_EQ(42, value); // [確認_異常系] - 状態値を変更しない。
    EXPECT_EQ(42, reason); // [確認_異常系] - 診断理由を変更しない。
    EXPECT_STREQ("kept", lib_name); // [確認_異常系] - ライブラリ名を変更しない。
    EXPECT_STREQ("kept", func_name); // [確認_異常系] - 関数名を変更しない。
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_is_failed(&entry_, &value)); // [確認_正常系] - 失敗状態を取得する。
    EXPECT_EQ(1, value); // [確認_正常系] - 失敗状態は 1 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 実在するライブラリとシンボルが解決されることの確認
TEST_F(symLoaderResolveTest, resolves_existing_symbol)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat",
              "cplat_path_basename"); // [状態] - 実在するライブラリ名と関数名を設定したエントリを用意する。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t count = test_sym_loader_count(&entry_); // [手順] - test_sym_loader_count() を呼び出す。
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    void *func_ptr = test_sym_loader_get(&entry_, 0u); // [手順] - 添字 0 を指定して test_sym_loader_get() を呼び出す。

    // Assert
    EXPECT_EQ(1u, count); // [確認_正常系] - test_sym_loader_count() の戻り値が 1u であること。
    EXPECT_NE(nullptr, func_ptr); // [確認_正常系] - test_sym_loader_get() の戻り値が nullptr 以外であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決済みになることの確認)
    // [サブ手順参照 名前=symLoaderResolveTest.is_default]
    EXPECT_EQ(0, test_sym_loader_is_default(&entry_)); // [確認_正常系] - test_sym_loader_is_default() の戻り値が 0 であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_failed]
    EXPECT_EQ(0, test_sym_loader_is_failed(&entry_)); // [確認_正常系] - test_sym_loader_is_failed() の戻り値が 0 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 2 回目の呼び出しが解決済みの結果をそのまま返すことの確認
TEST_F(symLoaderResolveTest, second_call_returns_cached_result)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename");
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    void *first = test_sym_loader_get(&entry_, 0u); // [状態] - 1 回目の解決を済ませておく。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    void *second = test_sym_loader_get(
        &entry_, 0u); // [手順] - 2 回目として同じエントリの添字 0 を指定して test_sym_loader_get() を呼び出す。

    // Assert
    EXPECT_EQ(
        first,
        second); // [確認_正常系] - 2 回目の test_sym_loader_get() の戻り値が 1 回目の test_sym_loader_get() の戻り値と一致すること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決状態が変化しないことの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// lib と func がともに default の 1 件が明示的既定値になることの確認
TEST_F(symLoaderResolveTest, marks_explicit_default_when_both_names_are_default)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("default", "default"); // [状態] - lib と func の双方に "default" を設定する。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t count = test_sym_loader_count(&entry_); // [手順] - test_sym_loader_count() を呼び出す。
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    void *func_ptr = test_sym_loader_get(&entry_, 0u, CPLAT_ERR_INVALID_ARGUMENT); // [手順] - 添字 0 を指定する。

    // Assert
    EXPECT_EQ(0u, count); // [確認_正常系] - test_sym_loader_count() の戻り値が 0u であること。
    EXPECT_EQ(nullptr, func_ptr); // [確認_正常系] - test_sym_loader_get() の戻り値が nullptr であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_default]
    EXPECT_EQ(1, test_sym_loader_is_default(&entry_)); // [確認_正常系] - test_sym_loader_is_default() の戻り値が 1 であること。
    // [サブ手順参照 名前=symLoaderResolveTest.reject_count]
    EXPECT_EQ(0u, test_sym_loader_reject_count(&entry_)); // [確認_正常系] - test_sym_loader_reject_count() の戻り値が 0u であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決済みになることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// lib が空の先が診断理由 -1 になることの確認
TEST_F(symLoaderResolveTest, marks_undefined_when_lib_name_is_empty)
{
    const char *lib_name = nullptr;
    const char *func_name = nullptr;
    int reason = 0;

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("", "cplat_path_basename"); // [状態] - lib を空文字列にしたエントリを用意する。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t count = test_sym_loader_count(&entry_); // [手順] - test_sym_loader_count() を呼び出す。
    int reject_result = cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name,
                                                    &reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(0u, count); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
    EXPECT_EQ(CPLAT_OK, reject_result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_EMPTY_NAME, reason); // [確認_異常系] - reason が CPLAT_SYM_LOADER_REJECT_EMPTY_NAME であること。
    EXPECT_STREQ("", lib_name); // [確認_異常系] - lib_name が "" であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決済みになることの確認)
    // [サブ手順参照 名前=symLoaderResolveTest.is_default]
    EXPECT_EQ(0, test_sym_loader_is_default(&entry_)); // [確認_異常系] - test_sym_loader_is_default() の戻り値が 0 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// func が空の先が診断理由 -1 になることの確認
TEST_F(symLoaderResolveTest, marks_undefined_when_func_name_is_empty)
{
    const char *lib_name = nullptr;
    const char *func_name = nullptr;
    int reason = 0;

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", ""); // [状態] - func を空文字列にしたエントリを用意する。

    // Pre-Assert

    // Act
    int reject_result = cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name,
                                                    &reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, reject_result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_EMPTY_NAME, reason); // [確認_異常系] - reason が CPLAT_SYM_LOADER_REJECT_EMPTY_NAME であること。
    EXPECT_STREQ("", func_name); // [確認_異常系] - func_name が "" であること。
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    EXPECT_EQ(0u, test_sym_loader_count(&entry_)); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 終端を除く 255 バイトの名前はオープンを試みて、ライブラリが無いときは -3 になることの確認
TEST_F(symLoaderResolveTest, rejects_open_when_configured_name_is_255_bytes)
{
    const char *lib_name = nullptr;
    const char *func_name = nullptr;
    int reason = 0;
    std::string long_name(255u, 'a');

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names(long_name.c_str(),
              "cplat_path_basename"); // [状態] - 終端を除いて 255 バイトの lib を設定する。

    // Pre-Assert

    // Act
    int reject_result = cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name,
                                                    &reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, reject_result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_OPEN_FAILED, reason); // [確認_異常系] - reason が CPLAT_SYM_LOADER_REJECT_OPEN_FAILED であること。
    EXPECT_STREQ(long_name.c_str(), lib_name); // [確認_異常系] - lib_name が設定した 255 バイトの名前と一致すること。
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    EXPECT_EQ(0u, test_sym_loader_count(&entry_)); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 終端を除く 256 バイト以上の名前が診断理由 -2 になることの確認
TEST_F(symLoaderResolveTest, rejects_name_longer_than_255_bytes)
{
    const char *lib_name = nullptr;
    const char *func_name = nullptr;
    int reason = 0;
    std::string long_name(256u, 'a');

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names(long_name.c_str(), "cplat_path_basename"); // [状態] - 終端を除いて 256 バイトの lib を設定する。

    // Pre-Assert

    // Act
    int reject_result = cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name,
                                                    &reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, reject_result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_NAME_TOO_LONG, reason); // [確認_異常系] - reason が CPLAT_SYM_LOADER_REJECT_NAME_TOO_LONG であること。
    EXPECT_STREQ(long_name.c_str(), lib_name); // [確認_異常系] - lib_name が切り詰められていない 256 バイトの名前と一致すること。
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    EXPECT_EQ(0u, test_sym_loader_count(&entry_)); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 実在しないライブラリのオープン失敗が診断理由 -3 になることの確認
TEST_F(symLoaderResolveTest, marks_open_error_when_library_is_missing)
{
    const char *lib_name = nullptr;
    const char *func_name = nullptr;
    int reason = 0;

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat_missing_for_test",
              "cplat_path_basename"); // [状態] - 実在しないライブラリ名を設定したエントリを用意する。

    // Pre-Assert

    // Act
    int reject_result = cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name,
                                                    &reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, reject_result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_OPEN_FAILED, reason); // [確認_異常系] - reason が CPLAT_SYM_LOADER_REJECT_OPEN_FAILED であること。
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    EXPECT_EQ(0u, test_sym_loader_count(&entry_)); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    EXPECT_EQ(nullptr, test_sym_loader_get(&entry_, 0u, CPLAT_ERR_INVALID_ARGUMENT)); // [確認_異常系] - 範囲外を拒否する。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 実在しないシンボルが診断理由 -4 になり、利用側の列に残らないことの確認
TEST_F(symLoaderResolveTest, releases_handle_when_symbol_is_missing)
{
    const char *lib_name = nullptr;
    const char *func_name = nullptr;
    int reason = 0;

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat",
              "cplat_symbol_that_does_not_exist"); // [状態] - 実在するライブラリと実在しない関数名を設定する。

    // Pre-Assert

    // Act
    int reject_result = cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name,
                                                    &reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, reject_result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_SYMBOL_NOT_FOUND, reason); // [確認_異常系] - reason が CPLAT_SYM_LOADER_REJECT_SYMBOL_NOT_FOUND であること。
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    EXPECT_EQ(0u, test_sym_loader_count(&entry_)); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決済みになることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 片方だけが default の先は開かず、同じキーの使えた先は残ることの確認
TEST_F(symLoaderResolveTest, rejects_partial_default_and_keeps_usable_target)
{
    const char *lib_name = nullptr;
    const char *func_name = nullptr;
    int reason = 0;

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("default", "not_default"); // [状態] - ライブラリ名だけが default の先を 1 件目に設定する。
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename"); // [状態] - 実在する先を 2 件目に設定する。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t count = test_sym_loader_count(&entry_); // [手順] - test_sym_loader_count() を呼び出す。
    int reject_result = cplat_sym_loader_reject_get(&entry_, 0u, &lib_name, &func_name,
                                                    &reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    void *func_ptr = test_sym_loader_get(&entry_, 0u); // [手順] - 添字 0 を指定して test_sym_loader_get() を呼び出す。

    // Assert
    EXPECT_EQ(1u, count); // [確認_正常系] - test_sym_loader_count() の戻り値が 1u であること。
    EXPECT_NE(nullptr, func_ptr); // [確認_正常系] - test_sym_loader_get() の戻り値が nullptr 以外であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_default]
    EXPECT_EQ(0, test_sym_loader_is_default(&entry_)); // [確認_正常系] - test_sym_loader_is_default() の戻り値が 0 であること。
    EXPECT_EQ(CPLAT_OK, reject_result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_PARTIAL_DEFAULT, reason); // [確認_異常系] - reason が CPLAT_SYM_LOADER_REJECT_PARTIAL_DEFAULT であること。
    // [サブ手順参照 名前=symLoaderResolveTest.reject_count]
    EXPECT_EQ(1u, test_sym_loader_reject_count(&entry_)); // [確認_異常系] - test_sym_loader_reject_count() の戻り値が 1u であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 2 件以上のうち両方 default の先があると、キー全体が診断理由 -5 になることの確認
TEST_F(symLoaderResolveTest, rejects_mixed_default_for_every_adopted_target)
{
    const char *first_lib = nullptr;
    const char *first_func = nullptr;
    const char *second_lib = nullptr;
    const char *second_func = nullptr;
    int first_reason = 0;
    int second_reason = 0;

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("default", "default"); // [状態] - 両方 default の先を 1 件目に設定する。
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename"); // [状態] - 実在する先を 2 件目に設定する。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t count = test_sym_loader_count(&entry_); // [手順] - test_sym_loader_count() を呼び出す。
    int first_result = cplat_sym_loader_reject_get(&entry_, 0u, &first_lib, &first_func,
                                                   &first_reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。
    int second_result = cplat_sym_loader_reject_get(&entry_, 1u, &second_lib, &second_func,
                                                    &second_reason); // [手順] - 添字 1 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(0u, count); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_default]
    EXPECT_EQ(0, test_sym_loader_is_default(&entry_)); // [確認_異常系] - test_sym_loader_is_default() の戻り値が 0 であること。
    EXPECT_EQ(CPLAT_OK, first_result); // [確認_異常系] - 添字 0 の cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, second_result); // [確認_異常系] - 添字 1 の cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT, first_reason); // [確認_異常系] - 1 件目の reason が CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT, second_reason); // [確認_異常系] - 2 件目の reason が CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT であること。
    EXPECT_STREQ("default", first_lib); // [確認_異常系] - 1 件目の lib_name が "default" であること。
    EXPECT_STREQ("libcplat", second_lib); // [確認_異常系] - 2 件目の lib_name が "libcplat" であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 置き換えて捨てた先の診断が、採用した値の診断より前に -7 で並ぶことの確認
TEST_F(symLoaderResolveTest, places_duplicate_key_rejects_before_adopted_rejects)
{
    const char *first_lib = nullptr;
    const char *first_func = nullptr;
    const char *second_lib = nullptr;
    const char *second_func = nullptr;
    int first_reason = 0;
    int second_reason = 0;
    cplat_sym_loader_reject_item *grown;

    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat_missing_for_test", "cplat_path_basename"); // [状態] - 採用する先として実在しないライブラリを設定する。
    grown = static_cast<cplat_sym_loader_reject_item *>(
        cplat_realloc(entry_.impl->duplicates, 1u, sizeof(*grown)));
    ASSERT_NE(nullptr, grown); // [状態確認] - duplicates を確保する cplat_realloc() の戻り値が nullptr 以外であること。
    entry_.impl->duplicates = grown;
    grown[0].lib_name = cplat_strdup("libfirst");
    grown[0].func_name = cplat_strdup("first_calc");
    ASSERT_NE(nullptr, grown[0].lib_name); // [状態確認] - 捨てた先の lib を複製する cplat_strdup() の戻り値が nullptr 以外であること。
    ASSERT_NE(nullptr, grown[0].func_name); // [状態確認] - 捨てた先の func を複製する cplat_strdup() の戻り値が nullptr 以外であること。
    grown[0].reason = CPLAT_SYM_LOADER_REJECT_DUPLICATE_KEY;
    entry_.impl->duplicate_count = 1u; // [状態] - 置き換えて捨てた先を診断理由 -7 で 1 件用意する。

    // Pre-Assert

    // Act
    int first_result = cplat_sym_loader_reject_get(&entry_, 0u, &first_lib, &first_func,
                                                   &first_reason); // [手順] - 添字 0 を指定して cplat_sym_loader_reject_get() を呼び出す。
    int second_result = cplat_sym_loader_reject_get(&entry_, 1u, &second_lib, &second_func,
                                                    &second_reason); // [手順] - 添字 1 を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    // [サブ手順参照 名前=symLoaderResolveTest.reject_count]
    EXPECT_EQ(2u, test_sym_loader_reject_count(&entry_)); // [確認_異常系] - test_sym_loader_reject_count() の戻り値が 2u であること。
    EXPECT_EQ(CPLAT_OK, first_result); // [確認_異常系] - 添字 0 の cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, second_result); // [確認_異常系] - 添字 1 の cplat_sym_loader_reject_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_DUPLICATE_KEY, first_reason); // [確認_異常系] - 1 件目の reason が CPLAT_SYM_LOADER_REJECT_DUPLICATE_KEY であること。
    EXPECT_STREQ("libfirst", first_lib); // [確認_異常系] - 1 件目の lib_name が "libfirst" であること。
    EXPECT_STREQ("first_calc", first_func); // [確認_異常系] - 1 件目の func_name が "first_calc" であること。
    EXPECT_EQ(CPLAT_SYM_LOADER_REJECT_OPEN_FAILED, second_reason); // [確認_異常系] - 2 件目の reason が CPLAT_SYM_LOADER_REJECT_OPEN_FAILED であること。
    EXPECT_STREQ("libcplat_missing_for_test", second_lib); // [確認_異常系] - 2 件目の lib_name が "libcplat_missing_for_test" であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 診断取得の出力ポインターが NULL のとき、書き込まずに引数不正を返すことの確認
TEST_F(symLoaderResolveTest, rejects_null_output_pointers)
{
    const char *func_name = "kept";
    int reason = 99;

    // Arrange

    // Pre-Assert

    // Act
    int result = cplat_sym_loader_reject_get(
        &entry_, 0u, nullptr, &func_name,
        &reason); // [手順] - lib_name に nullptr を指定して cplat_sym_loader_reject_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_sym_loader_reject_get() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("kept", func_name); // [確認_異常系] - func_name が "kept" であること。(出力へ書き込まないことの確認)
    EXPECT_EQ(99, reason); // [確認_異常系] - reason が 99 であること。(出力へ書き込まないことの確認)
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。(解決を始めないことの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 解決用の複製に失敗したあと、次の取得で再試行して解決できることの確認
TEST_F(symLoaderResolveTest, retries_after_resolve_memory_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename"); // [状態] - 実在するライブラリ名と関数名を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_strdup(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_strdup() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_strdup() から 1 回目は nullptr を返し、以降は製品 (prod) の処理へ委譲する。

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t failed_count = test_sym_loader_count(&entry_, CPLAT_ERR_UNKNOWN); // [手順] - 複製失敗を通知する。
    // [サブ手順参照 名前=symLoaderResolveTest.is_failed]
    int failed = test_sym_loader_is_failed(&entry_); // [手順] - test_sym_loader_is_failed() を呼び出す。

    // Assert
    EXPECT_EQ(0u, failed_count); // [確認_異常系] - 失敗中の test_sym_loader_count() の戻り値が 0u であること。
    EXPECT_EQ(1, failed); // [確認_異常系] - test_sym_loader_is_failed() の戻り値が 1 であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.failed,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.failed を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(既存の失敗を解消しないことの確認)
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。

    // Act_2
    // [サブ手順参照 名前=symLoaderResolveTest.is_default]
    int is_default = test_sym_loader_is_default(&entry_); // [手順] - 再試行として test_sym_loader_is_default() を呼び出す。
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t retried_count = test_sym_loader_count(&entry_); // [手順] - 再試行のあと test_sym_loader_count() を呼び出す。

    // Assert_2
    EXPECT_EQ(0, is_default); // [確認_正常系] - 再試行後の test_sym_loader_is_default() の戻り値が 0 であること。
    // [サブ手順参照 名前=symLoaderResolveTest.reject_count]
    EXPECT_EQ(0u, test_sym_loader_reject_count(&entry_)); // [確認_正常系] - 再試行後の test_sym_loader_reject_count() の戻り値が 0u であること。
    EXPECT_EQ(1u, retried_count); // [確認_正常系] - 再試行後の test_sym_loader_count() の戻り値が 1u であること。
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    EXPECT_NE(nullptr, test_sym_loader_get(&entry_, 0u)); // [確認_正常系] - 再試行後の test_sym_loader_get() の戻り値が nullptr 以外であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_failed]
    EXPECT_EQ(0, test_sym_loader_is_failed(&entry_)); // [確認_正常系] - 再試行後の test_sym_loader_is_failed() の戻り値が 0 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

#if defined(PLATFORM_LINUX)

// 確保失敗後の解放にも失敗した場合、ハンドルを保持することの確認
TEST_F(symLoaderResolveTest, retains_handle_when_allocation_and_close_fail)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_dlfcn> mock_dlfcn;
    void *handle = reinterpret_cast<void *>(static_cast<uintptr_t>(0x51));
    void *symbol = reinterpret_cast<void *>(static_cast<uintptr_t>(0x52));
    size_t count = 42u;

    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("test_module", "test_symbol");
    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dlopen(_, _, _, _, _)).WillOnce(Return(handle));
    // [Pre-Assert確認_正常系] - モジュールを 1 回開こうとすること。
    // [Pre-Assert手順] - テスト用ハンドルを返す。
    EXPECT_CALL(mock_dlfcn, dlsym(_, _, _, handle, _)).WillOnce(Return(symbol));
    // [Pre-Assert確認_正常系] - ハンドルからシンボルを 1 回取得すること。
    // [Pre-Assert手順] - テスト用シンボルを返す。
    EXPECT_CALL(mock_cplat, cplat_strdup(_)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_正常系] - 名前を 1 回複製しようとすること。
    // [Pre-Assert手順] - 名前の確保を失敗させる。
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, handle)).WillOnce(Return(-1)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - ハンドルの解放を 2 回試みること。
    // [Pre-Assert手順] - 最初の解放は失敗し、再試行は成功する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_count(&entry_, &count)); // [確認_異常系] - 解決失敗を通知する。
    EXPECT_EQ(42u, count); // [確認_異常系] - 件数を変更しない。
    ASSERT_NE(nullptr, entry_.impl); // [確認_異常系] - 管理構造体が残ること。
    ASSERT_NE(nullptr, entry_.impl->usable); // [確認_異常系] - ハンドルの管理領域が残ること。
    EXPECT_EQ(handle, entry_.impl->usable[0].handle); // [確認_異常系] - 開いたハンドルを保持する。
    EXPECT_EQ(1, entry_.impl->release_pending); // [確認_異常系] - 解放待ちになる。
    cplat_sym_loader_entry *entries[] = {&entry_};
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_dispose(entries, 1u)); // [確認_正常系] - 解放を再試行できる。
    EXPECT_EQ(nullptr, entry_.impl); // [確認_正常系] - 再試行後に管理構造体が解放される。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// シンボル不在時の解放失敗を保持し、後続の解放で再試行することの確認
TEST_F(symLoaderResolveTest, retains_handle_when_missing_symbol_close_fails)
{
    NiceMock<Mock_dlfcn> mock_dlfcn;
    void *handle = reinterpret_cast<void *>(static_cast<uintptr_t>(0x51));
    size_t count = 42u;
    cplat_sym_loader_entry *entries[] = {&entry_};

    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("test_module", "missing_symbol");
    EXPECT_CALL(mock_dlfcn, dlopen(_, _, _, _, _)).WillOnce(Return(handle));
    // [Pre-Assert確認_正常系] - モジュールを 1 回開こうとすること。
    // [Pre-Assert手順] - テスト用ハンドルを返す。
    EXPECT_CALL(mock_dlfcn, dlsym(_, _, _, handle, _)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_正常系] - シンボルを 1 回取得しようとすること。
    // [Pre-Assert手順] - シンボルが存在しないことを返す。
    EXPECT_CALL(mock_dlfcn, dlclose(_, _, _, handle)).WillOnce(Return(-1)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - ハンドルの解放を 2 回試みること。
    // [Pre-Assert手順] - 最初の解放は失敗し、再試行は成功する。

    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_sym_loader_count(&entry_, &count)); // [確認_異常系] - 解放失敗を通知すること。
    EXPECT_EQ(42u, count); // [確認_異常系] - 件数を変更しないこと。
    ASSERT_NE(nullptr, entry_.impl); // [確認_異常系] - 管理構造体が残ること。
    ASSERT_NE(nullptr, entry_.impl->usable); // [確認_異常系] - ハンドルの保持領域が残ること。
    EXPECT_EQ(handle, entry_.impl->usable[0].handle); // [確認_異常系] - ハンドルを保持すること。
    EXPECT_EQ(1, entry_.impl->release_pending); // [確認_異常系] - 解放待ちになること。
    EXPECT_EQ(CPLAT_OK, cplat_sym_loader_dispose(entries, 1u)); // [確認_正常系] - 後続の解放で再試行すること。
    EXPECT_EQ(nullptr, entry_.impl); // [確認_正常系] - 再試行後に管理構造体を解放すること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ロックの生成に失敗したあと、次の取得で再試行して解決できることの確認
TEST_F(symLoaderResolveTest, returns_null_when_lock_creation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename"); // [状態] - 実在するライブラリ名と関数名を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_create(_))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_lock_create() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_create() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は製品 (prod) の処理へ委譲する。

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t count = test_sym_loader_count(&entry_, CPLAT_ERR_UNKNOWN); // [手順] - ロック生成失敗を通知する。

    // Assert
    EXPECT_EQ(0u, count); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_failed]
    EXPECT_EQ(1, test_sym_loader_is_failed(&entry_)); // [確認_異常系] - test_sym_loader_is_failed() の戻り値が 1 であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.failed,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.failed を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.lock_state,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.lock_state を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。(ロック生成の失敗後に再試行できることの確認)

    // Act_2
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t retried_count = test_sym_loader_count(&entry_); // [手順] - 再試行として test_sym_loader_count() を呼び出す。

    // Assert_2
    EXPECT_EQ(1u, retried_count); // [確認_正常系] - 再試行後の test_sym_loader_count() の戻り値が 1u であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_failed]
    EXPECT_EQ(0, test_sym_loader_is_failed(&entry_)); // [確認_正常系] - 再試行後の test_sym_loader_is_failed() の戻り値が 0 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ロックの取得に失敗したあと、次の取得で再試行して解決できることの確認
TEST_F(symLoaderResolveTest, returns_null_when_lock_acquisition_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename"); // [状態] - 実在するライブラリ名と関数名を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_lock() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は製品 (prod) の処理へ委譲する。

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t count = test_sym_loader_count(&entry_, CPLAT_ERR_UNKNOWN); // [手順] - ロック取得失敗を通知する。

    // Assert
    EXPECT_EQ(0u, count); // [確認_異常系] - test_sym_loader_count() の戻り値が 0u であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。
    // [サブ手順参照 名前=symLoaderResolveTest.is_failed]
    EXPECT_EQ(1, test_sym_loader_is_failed(&entry_)); // [確認_異常系] - test_sym_loader_is_failed() の戻り値が 1 であること。

    // Act_2
    // [サブ手順参照 名前=symLoaderResolveTest.count]
    size_t retried_count = test_sym_loader_count(&entry_); // [手順] - 再試行として test_sym_loader_count() を呼び出す。

    // Assert_2
    EXPECT_EQ(1u, retried_count); // [確認_正常系] - 再試行後の test_sym_loader_count() の戻り値が 1u であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ほかのスレッドがロック初期化を完了した状態を再利用することの確認
TEST_F(symLoaderResolveTest, reuses_lock_initialized_by_another_thread)
{
    // Arrange
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&entry_.lock)); // [状態] - エントリのロックを生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    cplat_atomic_store_i32(&entry_.lock_state, 1, CPLAT_MEMORY_ORDER_RELAXED);
    test_sym_loader_set_entry_lock_wait_hook(complete_entry_lock_initialization);

    // Pre-Assert

    // Act
    int result = test_sym_loader_ensure_entry_lock_initialized(
        &entry_); // [手順] - 他スレッドの初期化中を表すエントリを指定して test_sym_loader_ensure_entry_lock_initialized() を呼び出す。

    // Assert
    EXPECT_EQ(0, result); // [確認_正常系] - test_sym_loader_ensure_entry_lock_initialized() の戻り値が 0 であること。
    EXPECT_EQ(
        2,
        cplat_atomic_load_i32(
            &entry_.lock_state,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.lock_state を読み込んだ cplat_atomic_load_i32() の戻り値が 2 であること。(ロック初期化が完了することの確認)

    // Cleanup
    test_sym_loader_set_entry_lock_wait_hook(nullptr);
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ロック初期化待機の既定処理が呼び出し元へ制御を戻すことの確認
TEST_F(symLoaderResolveTest, default_lock_wait_yields_execution)
{
    // Arrange

    // Pre-Assert

    // Act
    test_sym_loader_default_entry_lock_wait(
        &entry_); // [手順] - エントリを指定して test_sym_loader_default_entry_lock_wait() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(test_sym_loader_default_entry_lock_wait() が呼び出し元へ制御を戻すことの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ほかのスレッドによるロック初期化失敗を通知することの確認
TEST_F(symLoaderResolveTest, reports_lock_initialization_failure_from_another_thread)
{
    // Arrange
    cplat_atomic_store_i32(&entry_.lock_state, -1, CPLAT_MEMORY_ORDER_RELAXED);

    // Pre-Assert

    // Act
    int result = test_sym_loader_ensure_entry_lock_initialized(
        &entry_); // [手順] - 他スレッドの初期化失敗を表すエントリを指定して test_sym_loader_ensure_entry_lock_initialized() を呼び出す。

    // Assert
    EXPECT_EQ(-1, result); // [確認_異常系] - test_sym_loader_ensure_entry_lock_initialized() の戻り値が -1 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ロック取得中に解決済みとなった結果を再利用することの確認
TEST_F(symLoaderResolveTest, returns_result_resolved_while_waiting_for_lock)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, _))
        .WillOnce(Invoke(
            [this](cplat_local_lock *, int)
            {
                EXPECT_EQ(0, test_sym_loader_publish_resolved_pointer(&entry_, reinterpret_cast<void *>(1)));
                return CPLAT_OK;
            })); // [Pre-Assert確認_正常系] - cplat_local_lock_lock() が 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_local_lock_lock() から、解決済みの関数ポインターを公開して CPLAT_OK を返す。

    // Act
    // [サブ手順参照 名前=symLoaderResolveTest.get]
    void *result = test_sym_loader_get(
        &entry_, 0u); // [手順] - ロック取得中に解決済みとなるエントリを指定して test_sym_loader_get() を呼び出す。

    // Assert
    EXPECT_EQ(
        reinterpret_cast<void *>(1),
        result); // [確認_正常系] - test_sym_loader_get() の戻り値が reinterpret_cast<void *>(1) と一致すること。(別スレッドの解決結果を再利用することの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

#endif /* PLATFORM_LINUX */
