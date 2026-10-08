#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/result.h>
#include <cplat/runtime/sym_loader.h>

// シンボル情報 API が空の配列を受け付けることの確認
TEST(symLoaderInfoTest, sym_loader_info_accepts_empty_array)
{
    // Arrange

    // Pre-Assert

    // Act
    int result = cplat_sym_loader_info(NULL, 0); // [手順] - 要素数 0 と NULL の配列を渡してシンボル情報を表示する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_sym_loader_info の戻り値が CPLAT_OK であること。
}

// シンボル情報 API が不正な配列を拒否することの確認
TEST(symLoaderInfoTest, sym_loader_info_rejects_invalid_arrays)
{
    // Arrange
    cplat_sym_loader_entry *entries[] = {NULL};

    // Pre-Assert

    // Act
    int null_array_result =
        cplat_sym_loader_info(NULL, 1); // [手順] - 要素数 1 と NULL の配列を渡してシンボル情報を表示する。
    int null_entry_result =
        cplat_sym_loader_info(entries, 1); // [手順] - NULL 要素を含む配列を渡してシンボル情報を表示する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_array_result); // [確認_異常系] - 配列が NULL の呼び出しの戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_entry_result); // [確認_異常系] - 配列要素が NULL の呼び出しの戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// シンボル情報 API が解決状態を結果コードへ反映することの確認
TEST(symLoaderInfoTest, sym_loader_info_reports_resolution_state)
{
    // Arrange
    cplat_sym_loader_entry resolved = CPLAT_SYM_LOADER_ENTRY_INIT("resolved", void (*)(void));
    cplat_sym_loader_entry unresolved = CPLAT_SYM_LOADER_ENTRY_INIT("unresolved", void (*)(void));
    cplat_sym_loader_entry *resolved_entries[] = {&resolved};
    cplat_sym_loader_entry *unresolved_entries[] = {&unresolved};
    cplat_atomic_store_i32(&resolved.resolved, 1,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 1 個のエントリを解決済みとする。
    cplat_atomic_store_i32(&unresolved.resolved, -1,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 1 個のエントリを解決失敗済みとする。

    // Pre-Assert

    // Act
    int resolved_result = cplat_sym_loader_info(resolved_entries, 1); // [手順] - 解決済みのエントリ情報を表示する。
    int unresolved_result =
        cplat_sym_loader_info(unresolved_entries, 1); // [手順] - 解決失敗済みのエントリ情報を表示する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              resolved_result); // [確認_正常系] - 解決済みエントリに対する戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              unresolved_result); // [確認_異常系] - 解決失敗済みエントリに対する戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 未解決エントリを情報表示時に解決してから状態を表示することの確認
TEST(symLoaderInfoTest, sym_loader_info_resolves_unresolved_entry)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("default_entry", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry};
    ASSERT_EQ(CPLAT_OK, cplat_strcpy(entry.lib_name, sizeof(entry.lib_name),
                                     "default")); // [状態] - 明示的既定値として解決できるエントリを用意する。
    // [状態確認] - lib_name への cplat_strcpy の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_strcpy(entry.func_name, sizeof(entry.func_name),
                                     "default")); // [状態] - func_name に "default" を設定する。
    // [状態確認] - func_name への cplat_strcpy の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_sym_loader_info(entries, 1u); // [手順] - 未解決エントリの情報を表示する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_sym_loader_info の戻り値が CPLAT_OK であること。
    EXPECT_EQ(2, cplat_atomic_load_i32(
                     &entry.resolved,
                     CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - 情報表示前の解決により resolved が 2 になること。
}
