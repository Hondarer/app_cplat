#include <testfw.h>
#include <mock_cplat.h>

// mock 未注入時のラッパーが新規の整数解析を実 libcplat に委譲することの確認
TEST(parseMockTest, without_mock_delegates_to_real)
{
    // Arrange
    int8_t value_int8 = 0;     // [状態] - 出力先を 0 で初期化する。
    int16_t value_int16 = 0;   // [状態] - 出力先を 0 で初期化する。
    int32_t value_int32 = 0;   // [状態] - 出力先を 0 で初期化する。
    uint8_t value_uint8 = 0;   // [状態] - 出力先を 0 で初期化する。
    uint16_t value_uint16 = 0; // [状態] - 出力先を 0 で初期化する。
    uint32_t value_uint32 = 0; // [状態] - 出力先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_int8 =
        cplat_parse_int8(&value_int8, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_int8() を呼び出す。
    int actual_ret_int16 =
        cplat_parse_int16(&value_int16, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_int16() を呼び出す。
    int actual_ret_int32 =
        cplat_parse_int32(&value_int32, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_int32() を呼び出す。
    int actual_ret_uint8 =
        cplat_parse_uint8(&value_uint8, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_uint8() を呼び出す。
    int actual_ret_uint16 = cplat_parse_uint16(&value_uint16, "42",
                                               10); // [手順] - 文字列 "42" を指定して cplat_parse_uint16() を呼び出す。
    int actual_ret_uint32 = cplat_parse_uint32(&value_uint32, "42",
                                               10); // [手順] - 文字列 "42" を指定して cplat_parse_uint32() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_int8);               // [確認_正常系] - int8 の変換が成功すること。
    EXPECT_EQ(static_cast<int8_t>(42), value_int8);     // [確認_正常系] - int8 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_int16);              // [確認_正常系] - int16 の変換が成功すること。
    EXPECT_EQ(static_cast<int16_t>(42), value_int16);   // [確認_正常系] - int16 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_int32);              // [確認_正常系] - int32 の変換が成功すること。
    EXPECT_EQ(static_cast<int32_t>(42), value_int32);   // [確認_正常系] - int32 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint8);              // [確認_正常系] - uint8 の変換が成功すること。
    EXPECT_EQ(static_cast<uint8_t>(42), value_uint8);   // [確認_正常系] - uint8 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint16);             // [確認_正常系] - uint16 の変換が成功すること。
    EXPECT_EQ(static_cast<uint16_t>(42), value_uint16); // [確認_正常系] - uint16 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint32);             // [確認_正常系] - uint32 の変換が成功すること。
    EXPECT_EQ(static_cast<uint32_t>(42), value_uint32); // [確認_正常系] - uint32 の出力が 42 であること。
}

// 注入した mock の既定動作が新規の整数解析を実 libcplat に委譲することの確認
TEST(parseMockTest, injected_mock_delegates_to_real)
{
    // Arrange
    testing::NiceMock<Mock_cplat> mock_cplat; // [状態] - 既定の実委譲を使う mock を注入する。
    int8_t value_int8 = 0;                    // [状態] - 出力先を 0 で初期化する。
    int16_t value_int16 = 0;                  // [状態] - 出力先を 0 で初期化する。
    int32_t value_int32 = 0;                  // [状態] - 出力先を 0 で初期化する。
    uint8_t value_uint8 = 0;                  // [状態] - 出力先を 0 で初期化する。
    uint16_t value_uint16 = 0;                // [状態] - 出力先を 0 で初期化する。
    uint32_t value_uint32 = 0;                // [状態] - 出力先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_int8 =
        cplat_parse_int8(&value_int8, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_int8() を呼び出す。
    int actual_ret_int16 =
        cplat_parse_int16(&value_int16, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_int16() を呼び出す。
    int actual_ret_int32 =
        cplat_parse_int32(&value_int32, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_int32() を呼び出す。
    int actual_ret_uint8 =
        cplat_parse_uint8(&value_uint8, "42", 10); // [手順] - 文字列 "42" を指定して cplat_parse_uint8() を呼び出す。
    int actual_ret_uint16 = cplat_parse_uint16(&value_uint16, "42",
                                               10); // [手順] - 文字列 "42" を指定して cplat_parse_uint16() を呼び出す。
    int actual_ret_uint32 = cplat_parse_uint32(&value_uint32, "42",
                                               10); // [手順] - 文字列 "42" を指定して cplat_parse_uint32() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_int8);               // [確認_正常系] - int8 の変換が成功すること。
    EXPECT_EQ(static_cast<int8_t>(42), value_int8);     // [確認_正常系] - int8 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_int16);              // [確認_正常系] - int16 の変換が成功すること。
    EXPECT_EQ(static_cast<int16_t>(42), value_int16);   // [確認_正常系] - int16 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_int32);              // [確認_正常系] - int32 の変換が成功すること。
    EXPECT_EQ(static_cast<int32_t>(42), value_int32);   // [確認_正常系] - int32 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint8);              // [確認_正常系] - uint8 の変換が成功すること。
    EXPECT_EQ(static_cast<uint8_t>(42), value_uint8);   // [確認_正常系] - uint8 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint16);             // [確認_正常系] - uint16 の変換が成功すること。
    EXPECT_EQ(static_cast<uint16_t>(42), value_uint16); // [確認_正常系] - uint16 の出力が 42 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint32);             // [確認_正常系] - uint32 の変換が成功すること。
    EXPECT_EQ(static_cast<uint32_t>(42), value_uint32); // [確認_正常系] - uint32 の出力が 42 であること。
}

// 基本整数型 API の既定実委譲を確認する
TEST(parseMockTest, basic_types_without_mock_delegate_to_real)
{
    // Arrange
    char value_char = 0;                    // [状態] - 出力を 0 で初期化する。
    signed char value_schar = 0;            // [状態] - 出力を 0 で初期化する。
    unsigned char value_uchar = 0;          // [状態] - 出力を 0 で初期化する。
    short value_short = 0;                  // [状態] - 出力を 0 で初期化する。
    unsigned short value_ushort = 0;        // [状態] - 出力を 0 で初期化する。
    unsigned int value_uint = 0;            // [状態] - 出力を 0 で初期化する。
    long long value_longlong = 0;           // [状態] - 出力を 0 で初期化する。
    unsigned long long value_ulonglong = 0; // [状態] - 出力を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_char =
        cplat_parse_char(&value_char, "42", 10); // [手順] - 文字列を指定して cplat_parse_char() を呼び出す。
    int actual_ret_schar =
        cplat_parse_schar(&value_schar, "42", 10); // [手順] - 文字列を指定して cplat_parse_schar() を呼び出す。
    int actual_ret_uchar =
        cplat_parse_uchar(&value_uchar, "42", 10); // [手順] - 文字列を指定して cplat_parse_uchar() を呼び出す。
    int actual_ret_short =
        cplat_parse_short(&value_short, "42", 10); // [手順] - 文字列を指定して cplat_parse_short() を呼び出す。
    int actual_ret_ushort =
        cplat_parse_ushort(&value_ushort, "42", 10); // [手順] - 文字列を指定して cplat_parse_ushort() を呼び出す。
    int actual_ret_uint =
        cplat_parse_uint(&value_uint, "42", 10); // [手順] - 文字列を指定して cplat_parse_uint() を呼び出す。
    int actual_ret_longlong = cplat_parse_longlong(&value_longlong, "42",
                                                   10); // [手順] - 文字列を指定して cplat_parse_longlong() を呼び出す。
    int actual_ret_ulonglong = cplat_parse_ulonglong(
        &value_ulonglong, "42", 10); // [手順] - 文字列を指定して cplat_parse_ulonglong() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_char);                            // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<char>(42), value_char);                    // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_schar);                           // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<signed char>(42), value_schar);            // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uchar);                           // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned char>(42), value_uchar);          // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_short);                           // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<short>(42), value_short);                  // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_ushort);                          // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned short>(42), value_ushort);        // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint);                            // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned int>(42), value_uint);            // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_longlong);                        // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<long long>(42), value_longlong);           // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_ulonglong);                       // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned long long>(42), value_ulonglong); // [確認_正常系] - 実関数の変換結果であること。
}

// 基本整数型 API の既定実委譲を確認する
TEST(parseMockTest, basic_types_injected_mock_delegate_to_real)
{
    // Arrange
    testing::NiceMock<Mock_cplat> mock_cplat; // [状態] - 既定の実委譲を使う mock を注入する。
    char value_char = 0;                      // [状態] - 出力を 0 で初期化する。
    signed char value_schar = 0;              // [状態] - 出力を 0 で初期化する。
    unsigned char value_uchar = 0;            // [状態] - 出力を 0 で初期化する。
    short value_short = 0;                    // [状態] - 出力を 0 で初期化する。
    unsigned short value_ushort = 0;          // [状態] - 出力を 0 で初期化する。
    unsigned int value_uint = 0;              // [状態] - 出力を 0 で初期化する。
    long long value_longlong = 0;             // [状態] - 出力を 0 で初期化する。
    unsigned long long value_ulonglong = 0;   // [状態] - 出力を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_char =
        cplat_parse_char(&value_char, "42", 10); // [手順] - 文字列を指定して cplat_parse_char() を呼び出す。
    int actual_ret_schar =
        cplat_parse_schar(&value_schar, "42", 10); // [手順] - 文字列を指定して cplat_parse_schar() を呼び出す。
    int actual_ret_uchar =
        cplat_parse_uchar(&value_uchar, "42", 10); // [手順] - 文字列を指定して cplat_parse_uchar() を呼び出す。
    int actual_ret_short =
        cplat_parse_short(&value_short, "42", 10); // [手順] - 文字列を指定して cplat_parse_short() を呼び出す。
    int actual_ret_ushort =
        cplat_parse_ushort(&value_ushort, "42", 10); // [手順] - 文字列を指定して cplat_parse_ushort() を呼び出す。
    int actual_ret_uint =
        cplat_parse_uint(&value_uint, "42", 10); // [手順] - 文字列を指定して cplat_parse_uint() を呼び出す。
    int actual_ret_longlong = cplat_parse_longlong(&value_longlong, "42",
                                                   10); // [手順] - 文字列を指定して cplat_parse_longlong() を呼び出す。
    int actual_ret_ulonglong = cplat_parse_ulonglong(
        &value_ulonglong, "42", 10); // [手順] - 文字列を指定して cplat_parse_ulonglong() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_char);                            // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<char>(42), value_char);                    // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_schar);                           // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<signed char>(42), value_schar);            // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uchar);                           // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned char>(42), value_uchar);          // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_short);                           // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<short>(42), value_short);                  // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_ushort);                          // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned short>(42), value_ushort);        // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_uint);                            // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned int>(42), value_uint);            // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_longlong);                        // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<long long>(42), value_longlong);           // [確認_正常系] - 実関数の変換結果であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_ulonglong);                       // [確認_正常系] - 変換が成功すること。
    EXPECT_EQ(static_cast<unsigned long long>(42), value_ulonglong); // [確認_正常系] - 実関数の変換結果であること。
}
