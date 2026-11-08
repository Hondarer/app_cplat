#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/runtime/sym_loader.h>

class symLoaderIsDefaultTest : public Test
{
};

// 明示的既定値と未設定の違いを確認
TEST_F(symLoaderIsDefaultTest, distinguishes_default_from_missing)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("key", void (*)(void));
    int value = 123;
    int missing_value = 123;
    cplat_sym_loader_entry missing = CPLAT_SYM_LOADER_ENTRY_INIT("missing", void (*)(void));
    cplat_strcpy(entry.lib_name, sizeof(entry.lib_name), "default");
    cplat_strcpy(entry.func_name, sizeof(entry.func_name), "default"); // [状態] - 明示的既定値を設定する。
    cplat_sym_loader_entry *entries[] = {&entry, &missing};

    // Pre-Assert

    // Act
    int default_ret = cplat_sym_loader_is_default(&entry, &value);
    int missing_ret = cplat_sym_loader_is_default(&missing, &missing_value); // [手順] - 異なる設定を判定する。
    int null_ret = cplat_sym_loader_is_default(nullptr, &value);
    int dispose_ret = cplat_sym_loader_dispose(entries, 2);

    // Assert
    EXPECT_EQ(CPLAT_OK, default_ret); // [確認_正常系] - 既定値の取得に成功すること。
    EXPECT_EQ(1, value);
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, missing_ret); // [確認_異常系] - 未設定を通知し出力を保持すること。
    EXPECT_EQ(123, missing_value);
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, null_ret);
    EXPECT_EQ(CPLAT_OK, dispose_ret);
}

// 解決済み関数は明示的既定値ではないことの確認
TEST_F(symLoaderIsDefaultTest, resolved_function_is_not_default)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("key", void (*)(void));
    cplat_strcpy(entry.lib_name, sizeof(entry.lib_name), "libcplat");
    cplat_strcpy(entry.func_name, sizeof(entry.func_name), "cplat_path_basename");
    int value = 123;
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 解決可能な関数を設定する。

    // Pre-Assert

    // Act
    int actual = cplat_sym_loader_is_default(&entry, &value);
    int null_output = cplat_sym_loader_is_default(&entry, nullptr); // [手順] - 判定と出力引数の検査を行う。
    int dispose_ret = cplat_sym_loader_dispose(entries, 1);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual); // [確認_正常系] - 判定に成功すること。
    EXPECT_EQ(0, value);         // [確認_正常系] - 取得した関数は既定値ではないこと。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, null_output);
    EXPECT_EQ(CPLAT_OK, dispose_ret);
}
