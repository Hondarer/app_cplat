#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/sym_loader.h>

#include "sym_loader_internal.h"

using testing::NiceMock;

namespace
{
void plant_both_default(cplat_sym_loader_entry *entry)
{
    entry->impl = static_cast<cplat_sym_loader_entry_impl *>(cplat_calloc(1u, sizeof(*entry->impl)));
    ASSERT_NE(nullptr, entry->impl);
    entry->impl->pending = static_cast<cplat_sym_loader_name_pair *>(cplat_calloc(1u, sizeof(*entry->impl->pending)));
    ASSERT_NE(nullptr, entry->impl->pending);
    entry->impl->pending[0].lib_name = cplat_strdup("default");
    entry->impl->pending[0].func_name = cplat_strdup("default");
    ASSERT_NE(nullptr, entry->impl->pending[0].lib_name);
    ASSERT_NE(nullptr, entry->impl->pending[0].func_name);
    entry->impl->pending_count = 1u;
}
} // namespace

// シンボル情報 API が空の配列を受け付けることの確認
TEST(symLoaderInfoTest, sym_loader_info_accepts_empty_array)
{
    // Arrange

    // Pre-Assert

    // Act
    int result = cplat_sym_loader_info(
        nullptr, 0); // [手順] - 要素数 0 と nullptr の配列を指定して cplat_sym_loader_info() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_sym_loader_info() の戻り値が CPLAT_OK であること。
}

// シンボル情報 API が不正な配列を拒否することの確認
TEST(symLoaderInfoTest, sym_loader_info_rejects_invalid_arrays)
{
    // Arrange
    cplat_sym_loader_entry *entries[] = {nullptr};

    // Pre-Assert

    // Act
    int null_array_result = cplat_sym_loader_info(
        nullptr, 1); // [手順] - 要素数 1 と nullptr の配列を指定して cplat_sym_loader_info() を呼び出す。
    int null_entry_result = cplat_sym_loader_info(
        entries, 1); // [手順] - nullptr 要素を含む配列を指定して cplat_sym_loader_info() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_array_result); // [確認_異常系] - 配列が nullptr の cplat_sym_loader_info() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_entry_result); // [確認_異常系] - 配列要素が nullptr の cplat_sym_loader_info() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// シンボル情報 API が解決済みと内部失敗のどちらでも成功を返すことの確認
TEST(symLoaderInfoTest, sym_loader_info_reports_resolution_state)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_sym_loader_entry resolved = CPLAT_SYM_LOADER_ENTRY_INIT("resolved");
    cplat_sym_loader_entry failed = CPLAT_SYM_LOADER_ENTRY_INIT("failed");
    cplat_sym_loader_entry *resolved_entries[] = {&resolved};
    cplat_sym_loader_entry *failed_entries[] = {&failed};
    cplat_atomic_store_i32(&resolved.resolved, 1,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 1 個のエントリを解決済みとする。
    cplat_atomic_store_i32(&failed.failed, 1, CPLAT_MEMORY_ORDER_RELAXED);
    cplat_atomic_store_i32(&failed.resolved, 0,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 管理構造体が無く、内部失敗中のエントリを用意する。

    // Pre-Assert

    // Act
    int resolved_result = cplat_sym_loader_info(
        resolved_entries, 1); // [手順] - 解決済みのエントリを指定して cplat_sym_loader_info() を呼び出す。
    int failed_result = cplat_sym_loader_info(
        failed_entries, 1); // [手順] - 内部失敗中のエントリを指定して cplat_sym_loader_info() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        resolved_result); // [確認_正常系] - 解決済みエントリに対する cplat_sym_loader_info() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        failed_result); // [確認_正常系] - 内部失敗中のエントリに対する cplat_sym_loader_info() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &failed.failed,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - failed.failed を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(情報表示が失敗を成功へ変えないことの確認)
}

// 未解決エントリを情報表示時に解決してから状態を表示することの確認
TEST(symLoaderInfoTest, sym_loader_info_resolves_unresolved_entry)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("default_entry");
    cplat_sym_loader_entry *entries[] = {&entry};
    plant_both_default(&entry); // [状態] - 明示的既定値として解決できるエントリを用意する。

    // Pre-Assert

    // Act
    int result =
        cplat_sym_loader_info(entries, 1u); // [手順] - 未解決のエントリを指定して cplat_sym_loader_info() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_sym_loader_info() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(情報表示前に解決済みになることの確認)
    ASSERT_NE(nullptr, entry.impl); // [確認_正常系] - entry.impl が nullptr 以外であること。
    EXPECT_EQ(1, entry.impl->is_default); // [確認_正常系] - entry.impl->is_default が 1 であること。(明示的既定値になることの確認)

    // Cleanup
    cplat_sym_loader_dispose(entries, 1u);
}
