#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/sym_loader.h>

#include "sym_loader_internal.h"

using testing::NiceMock;

class symLoaderIsDefaultTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
    cplat_sym_loader_entry entry_ = CPLAT_SYM_LOADER_ENTRY_INIT("test_key");

    // [サブ手順 名前=symLoaderIsDefaultTest.plant_pending]
    void plant_pending(const char *lib_name, const char *func_name)
    {
        entry_.impl = static_cast<cplat_sym_loader_entry_impl *>(cplat_calloc(1u, sizeof(*entry_.impl)));
        ASSERT_NE(nullptr, entry_.impl);
        // [状態確認] - 管理構造体を確保する cplat_calloc() の戻り値が nullptr 以外であること。
        entry_.impl->pending =
            static_cast<cplat_sym_loader_name_pair *>(cplat_calloc(1u, sizeof(*entry_.impl->pending)));
        ASSERT_NE(nullptr, entry_.impl->pending);
        // [状態確認] - pending を確保する cplat_calloc() の戻り値が nullptr 以外であること。
        entry_.impl->pending[0].lib_name = cplat_strdup(lib_name);
        entry_.impl->pending[0].func_name = cplat_strdup(func_name);
        ASSERT_NE(nullptr, entry_.impl->pending[0].lib_name);
        // [状態確認] - lib_name を複製する cplat_strdup() の戻り値が nullptr 以外であること。
        ASSERT_NE(nullptr, entry_.impl->pending[0].func_name);
        // [状態確認] - func_name を複製する cplat_strdup() の戻り値が nullptr 以外であること。
        entry_.impl->pending_count = 1u;
    }
    // [サブ手順終了]

    // [サブ手順 名前=symLoaderIsDefaultTest.TearDown]
    void TearDown() override
    {
        cplat_sym_loader_entry *entries[] = {&entry_};

        cplat_sym_loader_dispose(entries, 1u);
    }
    // [サブ手順終了]
};

// 未解決のエントリが解決されたうえで明示的既定値と判定されることの確認
TEST_F(symLoaderIsDefaultTest, resolves_and_reports_explicit_default)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderIsDefaultTest.plant_pending]
    plant_pending("default", "default"); // [状態] - 両方 default の未解決エントリを用意する。
    cplat_atomic_store_i32(&entry_.resolved, 0, CPLAT_MEMORY_ORDER_RELAXED);

    // Pre-Assert

    // Act
    int actual_ret = cplat_sym_loader_is_default(&entry_); // [手順] - cplat_sym_loader_is_default() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        actual_ret); // [確認_正常系] - cplat_sym_loader_is_default() の戻り値が 1 であること。(明示的既定値と判定されることの確認)
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(呼び出しの中で解決済みになることの確認)
    EXPECT_EQ(0, cplat_sym_loader_is_failed(&entry_)); // [確認_正常系] - cplat_sym_loader_is_failed() の戻り値が 0 であること。
}
// [サブ手順参照 名前=symLoaderIsDefaultTest.TearDown]

// 解決済みのエントリが明示的既定値でないと判定されることの確認
TEST_F(symLoaderIsDefaultTest, reports_not_default_for_resolved_symbol)
{
    // Arrange
    entry_.impl = static_cast<cplat_sym_loader_entry_impl *>(cplat_calloc(1u, sizeof(*entry_.impl)));
    ASSERT_NE(nullptr, entry_.impl); // [状態確認] - 管理構造体を確保する cplat_calloc() の戻り値が nullptr 以外であること。
    entry_.impl->is_default = 0;
    cplat_atomic_store_i32(&entry_.resolved, 1,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 解決済みで明示的既定値でないエントリを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_sym_loader_is_default(&entry_); // [手順] - cplat_sym_loader_is_default() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret); // [確認_正常系] - cplat_sym_loader_is_default() の戻り値が 0 であること。(明示的既定値でないと判定されることの確認)
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決状態が変化しないことの確認)
}
// [サブ手順参照 名前=symLoaderIsDefaultTest.TearDown]

// 内部失敗中のエントリが明示的既定値でないと判定され、失敗が解消されないことの確認
TEST_F(symLoaderIsDefaultTest, reports_not_default_for_unresolved_entry)
{
    // Arrange
    cplat_atomic_store_i32(&entry_.failed, 1, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_atomic_store_i32(&entry_.resolved, 0,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 管理構造体が無く、内部失敗中のエントリを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_sym_loader_is_default(&entry_); // [手順] - cplat_sym_loader_is_default() を呼び出す。
    int failed = cplat_sym_loader_is_failed(&entry_); // [手順] - cplat_sym_loader_is_failed() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret); // [確認_異常系] - cplat_sym_loader_is_default() の戻り値が 0 であること。(明示的既定値でないと判定されることの確認)
    EXPECT_EQ(1, failed); // [確認_異常系] - cplat_sym_loader_is_failed() の戻り値が 1 であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。(空の成功として公開しないことの確認)
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.failed,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.failed を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(既存の失敗を解消しないことの確認)
}
// [サブ手順参照 名前=symLoaderIsDefaultTest.TearDown]
