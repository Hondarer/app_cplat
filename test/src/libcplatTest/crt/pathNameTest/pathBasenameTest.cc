#include <testfw.h>
#include <cplat/crt/path.h>
#include <string.h>

class pathBasenameTest : public Test
{
};

// セパレータを含むパスからベース名が取り出せることの確認
TEST_F(pathBasenameTest, returns_name_after_last_separator)
{
    // Arrange
    const char *path = "a/b/c.txt"; // [状態] - 複数階層を含むパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_STREQ("c.txt",
                 actual); // [確認_正常系] - cplat_path_basename() の戻り値が "c.txt" であること。
}

// セパレータを含まないパスがそのまま返ることの確認
TEST_F(pathBasenameTest, returns_input_when_no_separator)
{
    // Arrange
    const char *path = "c.txt"; // [状態] - セパレータを含まないパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_EQ(path,
              actual); // [確認_正常系] - cplat_path_basename() の戻り値が path と一致すること。
}

// '\\' もセパレータとして扱われることの確認
TEST_F(pathBasenameTest, treats_backslash_as_separator)
{
    // Arrange
    const char *path = "a\\b\\c"; // [状態] - '\\' 区切りのパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_STREQ("c", actual); // [確認_正常系] - cplat_path_basename() の戻り値が "c" であること。
}

// '/' と '\\' が混在するパスで最後のセパレータ以降が返ることの確認
TEST_F(pathBasenameTest, handles_mixed_separators)
{
    // Arrange
    const char *path = "a/b\\c"; // [状態] - '/' と '\\' が混在するパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_STREQ("c", actual); // [確認_正常系] - cplat_path_basename() の戻り値が "c" であること。
}

// 末尾がセパレータの場合に空文字列が返ることの確認
TEST_F(pathBasenameTest, returns_empty_string_when_trailing_separator)
{
    // Arrange
    const char *path = "/opt/bin/"; // [状態] - 末尾がセパレータのパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_STREQ("", actual); // [確認_正常系] - cplat_path_basename() の戻り値が "" であること。
}

// ルートのみのパスで空文字列が返ることの確認
TEST_F(pathBasenameTest, returns_empty_string_for_root)
{
    // Arrange
    const char *path = "/"; // [状態] - ルートのみのパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_STREQ("", actual); // [確認_正常系] - cplat_path_basename() の戻り値が "" であること。
}

// 空文字列パスで空文字列が返ることの確認
TEST_F(pathBasenameTest, returns_empty_string_for_empty_input)
{
    // Arrange
    const char *path = ""; // [状態] - 空文字列のパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_STREQ("", actual); // [確認_正常系] - cplat_path_basename() の戻り値が "" であること。
}

// NULL を渡した場合に NULL が返ることの確認
TEST_F(pathBasenameTest, returns_null_for_null_input)
{
    // Arrange

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(nullptr); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, actual); // [確認_異常系] - cplat_path_basename() の戻り値が nullptr であること。
}

// 返却ポインターが入力文字列を指す (複製されない) ことの確認
TEST_F(pathBasenameTest, returns_pointer_within_input_buffer)
{
    // Arrange
    const char path[] = "dir/name.txt"; // [状態] - ローカル バッファーにパスを用意する。

    // Pre-Assert

    // Act
    const char *actual = cplat_path_basename(path); // [手順] - cplat_path_basename() を呼び出す。

    // Assert
    EXPECT_EQ(path + 4, actual); // [確認_正常系] - cplat_path_basename() の戻り値が path + 4 と一致すること。
}
