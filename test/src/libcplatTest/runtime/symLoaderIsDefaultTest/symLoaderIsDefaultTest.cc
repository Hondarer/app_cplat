#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/runtime/sym_loader.h>

using testing::_;
using testing::NiceMock;

class symLoaderIsDefaultTest : public Test
{
  protected:
    cplat_sym_loader_entry entry_ = CPLAT_SYM_LOADER_ENTRY_INIT("test_key", void (*)(void));
};

// 未解決のエントリが解決されたうえで明示的既定値と判定されることの確認
TEST_F(symLoaderIsDefaultTest, resolves_and_reports_explicit_default)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_atomic_store_i32(&entry_.resolved, 0, CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 未解決のエントリを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_sym_loader_resolve(&entry_))
        .WillOnce(
            [](cplat_sym_loader_entry *entry)
            {
                cplat_atomic_store_i32(&entry->resolved, 2, CPLAT_MEMORY_ORDER_RELAXED);
                return nullptr;
            }); // [Pre-Assert確認_正常系] - 未解決のため cplat_sym_loader_resolve が 1 回呼び出されること。
                // [Pre-Assert手順] - resolved を明示的既定値の 2 に設定する。

    // Act
    int actual_ret = cplat_sym_loader_is_default(&entry_); // [手順] - cplat_sym_loader_is_default を呼び出す。

    // Assert
    EXPECT_EQ(1, actual_ret); // [確認_正常系] - cplat_sym_loader_is_default の戻り値が 1 であること。
    EXPECT_EQ(2,
              cplat_atomic_load_i32(
                  &entry_.resolved,
                  CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - 呼び出しの中で解決が行われ resolved が 2 になること。
}

// 解決済みのエントリが明示的既定値でないと判定されることの確認
TEST_F(symLoaderIsDefaultTest, reports_not_default_for_resolved_symbol)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_atomic_store_i32(&entry_.resolved, 1,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 解決済み (resolved が 1) のエントリを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_sym_loader_resolve(_))
        .Times(0); // [Pre-Assert確認_正常系] - 解決済みのため cplat_sym_loader_resolve が呼び出されないこと。

    // Act
    int actual_ret = cplat_sym_loader_is_default(&entry_); // [手順] - cplat_sym_loader_is_default を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_sym_loader_is_default の戻り値が 0 であること。
    EXPECT_EQ(
        1, cplat_atomic_load_i32(&entry_.resolved,
                                 CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - resolved が 1 のまま変化しないこと。
}

// 解決に失敗したエントリが明示的既定値でないと判定されることの確認
TEST_F(symLoaderIsDefaultTest, reports_not_default_for_unresolved_entry)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_atomic_store_i32(&entry_.resolved, 0, CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 未解決のエントリを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_sym_loader_resolve(&entry_))
        .WillOnce(
            [](cplat_sym_loader_entry *entry)
            {
                cplat_atomic_store_i32(&entry->resolved, -1, CPLAT_MEMORY_ORDER_RELAXED);
                return nullptr;
            }); // [Pre-Assert確認_異常系] - 未解決のため cplat_sym_loader_resolve が 1 回呼び出されること。
                // [Pre-Assert手順] - resolved を定義なしの -1 に設定する。

    // Act
    int actual_ret = cplat_sym_loader_is_default(&entry_); // [手順] - cplat_sym_loader_is_default を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_異常系] - cplat_sym_loader_is_default の戻り値が 0 であること。
    EXPECT_EQ(-1, cplat_atomic_load_i32(
                      &entry_.resolved,
                      CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - resolved が定義なしを示す -1 になること。
}
