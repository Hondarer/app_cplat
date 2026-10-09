#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/argparser/argparser.h>
#include <cplat/base/result.h>

#include <cstdio>

using testing::_;
using testing::DoDefault;
using testing::NiceMock;
using testing::Return;

class argparserAllocFailureTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
    cplat_argparser *parser_ = NULL;
    const char *storage_ = NULL;

    // [サブ手順 名前=argparserAllocFailureTest.SetUp]
    void SetUp() override
    {
        parser_ = cplat_argparser_handle_create(0, NULL, NULL); // [状態] - 生成済みの parser を用意する。
        ASSERT_NE((cplat_argparser *)NULL, parser_); // [状態確認] - ハンドルが非 NULL であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=argparserAllocFailureTest.TearDown]
    void TearDown() override
    {
        cplat_argparser_handle_dispose(parser_);
        parser_ = NULL;
    }
    // [サブ手順終了]
};

// 登録項目配列の拡張に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, register_fails_when_spec_array_expansion_fails)
{
    // Arrange

    // Pre-Assert
    /* 名前の複製など他の確保は本物へ委譲する。
       gMock は後から宣言した期待値を優先するため、汎用の期待値を先に宣言する */
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_正常系] - malloc が任意の回数呼び出されること。
                                      // [Pre-Assert手順] - 本物の malloc へ委譲する。
    EXPECT_CALL(mock_cplat_, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - realloc が登録項目配列の拡張のために 1 回呼び出されること。
                          // [Pre-Assert手順] - 1 回目は NULL を返却し、以降 (エラー記録用の確保) は本物へ委譲する。

    // Act
    int actual_ret = cplat_argparser_handle_register_option_string(
        parser_, "-a", "--alpha", "VALUE", "説明", 0u,
        &storage_); // [手順] - 文字列オプション --alpha を登録する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - cplat_argparser_handle_register_option_string の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// 登録項目の名前複製に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, register_fails_when_name_duplication_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - malloc が名前の複製のために 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_argparser_handle_register_option_string(
        parser_, "-a", "--alpha", "VALUE", "説明", 0u,
        &storage_); // [手順] - 文字列オプション --alpha を登録する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - cplat_argparser_handle_register_option_string の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// 位置引数の登録項目配列拡張に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, positional_register_fails_when_spec_array_expansion_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - realloc が位置引数の登録項目配列拡張で 1 回失敗すること。
                          // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物の realloc へ委譲する。

    // Act
    int actual_ret = cplat_argparser_handle_register_positional_string(parser_, "input", "説明", 0u,
                                                             &storage_); // [手順] - 文字列位置引数 input を登録する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - 配列拡張失敗時の cplat_argparser_handle_register_positional_string の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// 位置引数名の複製に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, positional_register_fails_when_name_duplication_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - malloc が位置引数名の複製で 1 回失敗すること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物の malloc へ委譲する。

    // Act
    int actual_ret = cplat_argparser_handle_register_positional_string(parser_, "input", NULL, 0u,
                                                             &storage_); // [手順] - 文字列位置引数 input を登録する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - 名前複製失敗時の cplat_argparser_handle_register_positional_string の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// 使用方法の出力バッファー確保に失敗した場合に出力が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, print_usage_fails_when_buffer_allocation_fails)
{
    // Arrange

    ASSERT_EQ(CPLAT_OK,
              cplat_argparser_handle_register_option_string(parser_, "-a", "--alpha", "VALUE", "説明", 0u,
                                                         &storage_)); // [状態] - オプションを 1 件登録しておく。
                                                                      // [状態確認] - cplat_argparser_handle_register_option_string の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - malloc が使用方法の組み立てバッファーのために 1 回目に呼び出されること。
                          // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_argparser_handle_print_usage(parser_,
                                              stdout); // [手順] - cplat_argparser_handle_print_usage を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - cplat_argparser_handle_print_usage の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// short_name の複製に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, register_fails_when_short_name_duplication_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - short_name の複製で malloc が失敗すること。

    // Act
    int actual_ret =
        cplat_argparser_handle_register_option_string(parser_, "-a", "--alpha", "VALUE", "説明", 0u,
                                                   &storage_); // [手順] - short_name を含む文字列オプションを登録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - short_name 複製失敗時の登録結果が OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// long_name の複製に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, register_fails_when_long_name_duplication_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - long_name の複製で malloc が失敗すること。

    // Act
    int actual_ret =
        cplat_argparser_handle_register_option_string(parser_, "-a", "--alpha", "VALUE", "説明", 0u,
                                                   &storage_); // [手順] - long_name を含む文字列オプションを登録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - long_name 複製失敗時の登録結果が OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// value_name の複製に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, register_fails_when_value_name_duplication_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - value_name の複製で malloc が失敗すること。

    // Act
    int actual_ret =
        cplat_argparser_handle_register_option_string(parser_, "-a", "--alpha", "VALUE", "説明", 0u,
                                                   &storage_); // [手順] - value_name を含む文字列オプションを登録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - value_name 複製失敗時の登録結果が OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// description の複製に失敗した場合に登録が失敗することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, register_fails_when_description_duplication_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(DoDefault())
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - description の複製で malloc が失敗すること。

    // Act
    int actual_ret = cplat_argparser_handle_register_option_string(
        parser_, "-a", "--alpha", "VALUE", "説明", 0u,
        &storage_); // [手順] - description を含む文字列オプションを登録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - description 複製失敗時の登録結果が OUT_OF_MEMORY であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// 登録エラー配列の realloc に失敗した場合に登録結果だけが返ることの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, register_error_is_not_recorded_when_realloc_fails)
{
    // Arrange
    int storage = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - 登録エラー配列の realloc が失敗すること。

    // Act
    int actual_ret = cplat_argparser_handle_register_flag(parser_, NULL, NULL, NULL,
                                                &storage); // [手順] - 名前なし登録で登録エラーを発生させる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - 登録エラー配列確保失敗後も登録結果が INVALID_ARGUMENT であること。
    EXPECT_EQ((size_t)0, cplat_argparser_handle_get_register_error_count(
                             parser_)); // [確認_異常系] - realloc 失敗時に登録エラー件数が 0 のままであること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// create の program_name 複製に失敗した場合に NULL が返ることの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, create_fails_when_program_name_duplication_fails)
{
    // Arrange
    cplat_argparser_options options = {};
    options.program_name = "program";
    options.program_description = "description";

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - program_name の複製で malloc が失敗すること。

    // Act
    cplat_argparser *parser =
        cplat_argparser_handle_create(0, NULL, &options); // [手順] - program_name と description を指定して parser を生成する。

    // Assert
    EXPECT_EQ(nullptr, parser); // [確認_異常系] - program_name 複製失敗時の parser が NULL であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// create の program_description 複製に失敗した場合に NULL が返ることの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, create_fails_when_program_description_duplication_fails)
{
    // Arrange
    cplat_argparser_options options = {};
    options.program_name = "program";
    options.program_description = "description";

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - program_description の複製で malloc が失敗すること。

    // Act
    cplat_argparser *parser =
        cplat_argparser_handle_create(0, NULL, &options); // [手順] - program_name と description を指定して parser を生成する。

    // Assert
    EXPECT_EQ(nullptr, parser); // [確認_異常系] - program_description 複製失敗時の parser が NULL であること。
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]

// argv[0] のベース名複製に失敗した場合も生成と解析が継続することの確認
// [サブ手順参照 名前=argparserAllocFailureTest.SetUp]
TEST_F(argparserAllocFailureTest, create_continues_when_program_name_duplication_fails)
{
    // Arrange
    char *argv[] = {const_cast<char *>("/usr/local/bin/tool")};

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - argv[0] のベース名複製で malloc が失敗すること。

    // Act
    cplat_argparser *parser =
        cplat_argparser_handle_create(1, argv, NULL); // [手順] - ベース名複製が失敗する状態で parser を生成する。

    // Assert
    ASSERT_NE(nullptr, parser); // [確認_正常系] - ベース名複製失敗時も生成が成功すること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_argparser_handle_parse(parser)); // [確認_正常系] - ベース名複製失敗時も cplat_argparser_handle_parse の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_argparser_handle_dispose(parser);
}
// [サブ手順参照 名前=argparserAllocFailureTest.TearDown]
