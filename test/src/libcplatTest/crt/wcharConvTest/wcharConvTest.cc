#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/crt/stdlib.h>
    #include <cplat/crt/wchar_conv.h>
    #include <mock_cplat.h>
    #include <mock_windows.h>

    #include <array>
    #include <cstring>
    #include <string>

using testing::_;
using testing::NiceMock;
using testing::Return;

class wcharConvTest : public Test
{
  protected:
    NiceMock<Mock_windows> mock_windows_;
};

TEST_F(wcharConvTest, converts_japanese_supplementary_and_empty_text_with_nul_inclusive_counts)
{
    // Arrange
    wchar_t wide[4] = {};
    char utf8[8] = {};
    size_t wide_count = 0U;
    size_t utf8_count = 0U;

    // Act
    int wide_result = cplat_utf8_to_wstr(wide, 4U, &wide_count, "\xE3\x81\x82\xF0\x9F\x98\x80");
    // [手順] - 日本語と補助文字を含む UTF-8 を、ちょうど収まる容量へ変換する。

    // Assert
    ASSERT_EQ(CPLAT_OK, wide_result); // [確認_正常系] - cplat_utf8_to_wstr() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(4U, wide_count); // [確認_正常系] - 終端 NUL を含む UTF-16 の要素数が 4 であること。
    EXPECT_STREQ(L"\u3042\U0001F600", wide); // [確認_正常系] - 変換結果が日本語と補助文字に一致すること。

    // Act
    int utf8_result = cplat_wstr_to_utf8(utf8, sizeof(utf8), &utf8_count, wide);
    // [手順] - 日本語と補助文字を含む UTF-16 を、ちょうど収まる容量へ変換する。

    // Assert
    ASSERT_EQ(CPLAT_OK, utf8_result); // [確認_正常系] - cplat_wstr_to_utf8() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(sizeof(utf8), utf8_count); // [確認_正常系] - 終端 NUL を含む UTF-8 の byte 数が配列サイズと一致すること。
    EXPECT_STREQ("\xE3\x81\x82\xF0\x9F\x98\x80", utf8); // [確認_正常系] - UTF-8 の変換結果が入力に一致すること。

    // Act
    wide_count = 0U;
    int empty_wide_result = cplat_utf8_to_wstr(wide, 1U, &wide_count, "");
    // [手順] - 空の UTF-8 を 1 要素のバッファーへ変換する。

    // Assert
    ASSERT_EQ(CPLAT_OK, empty_wide_result); // [確認_正常系] - 空文字列の cplat_utf8_to_wstr() が CPLAT_OK を返すこと。
    EXPECT_EQ(1U, wide_count); // [確認_正常系] - 空文字列の UTF-16 要素数が終端 NUL を含めて 1 であること。
    EXPECT_EQ(L'\0', wide[0]); // [確認_正常系] - 空文字列の変換結果が NUL で終端すること。

    // Act
    utf8_count = 0U;
    int empty_utf8_result = cplat_wstr_to_utf8(utf8, 1U, &utf8_count, L"");
    // [手順] - 空の UTF-16 を 1 byte のバッファーへ変換する。

    // Assert
    ASSERT_EQ(CPLAT_OK, empty_utf8_result); // [確認_正常系] - 空文字列の cplat_wstr_to_utf8() が CPLAT_OK を返すこと。
    EXPECT_EQ(1U, utf8_count); // [確認_正常系] - 空文字列の UTF-8 byte 数が終端 NUL を含めて 1 であること。
    EXPECT_EQ('\0', utf8[0]); // [確認_正常系] - 空文字列の UTF-8 変換結果が NUL で終端すること。
}

TEST_F(wcharConvTest, rejects_invalid_utf8_without_replacement)
{
    // Arrange
    const std::array<std::string, 4> invalid = {std::string("\x80", 1U), std::string("\xE3\x81", 2U),
                                                std::string("\xC0\xAF", 2U), std::string("\xED\xA0\x80", 3U)};

    // Pre-Assert
    // 特記すべき mock の期待なし。

    for (const std::string &input : invalid)
    {
        // Arrange
        wchar_t wide[8] = {L'x'};
        size_t count = 42U;

        // Act
        int actual_result = cplat_utf8_to_wstr(wide, sizeof(wide) / sizeof(wide[0]), &count, input.c_str());
        // [手順] - 不正な UTF-8 を指定して cplat_utf8_to_wstr() を呼び出す。

        // Assert
        EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, actual_result); // [確認_異常系] - cplat_utf8_to_wstr() が CPLAT_ERR_INVALID_ENCODING を返すこと。
        EXPECT_EQ(L'\0', wide[0]); // [確認_異常系] - 失敗時に出力バッファーの先頭が NUL であること。
        EXPECT_EQ(0U, count); // [確認_異常系] - 失敗時に count_out が 0 であること。
    }
}

TEST_F(wcharConvTest, rejects_unpaired_utf16_surrogates)
{
    // Arrange
    const wchar_t unpaired_high[] = {static_cast<wchar_t>(0xD800), L'\0'};
    const wchar_t unpaired_low[] = {static_cast<wchar_t>(0xDC00), L'\0'};

    // Pre-Assert
    // 特記すべき mock の期待なし。

    for (const wchar_t *input : {unpaired_high, unpaired_low})
    {
        // Arrange
        char utf8[16] = {'x'};
        size_t count = 42U;

        // Act
        int actual_result = cplat_wstr_to_utf8(utf8, sizeof(utf8), &count, input);
        // [手順] - 対になっていないサロゲートを指定して cplat_wstr_to_utf8() を呼び出す。

        // Assert
        EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, actual_result); // [確認_異常系] - cplat_wstr_to_utf8() が CPLAT_ERR_INVALID_ENCODING を返すこと。
        EXPECT_EQ('\0', utf8[0]); // [確認_異常系] - 失敗時に出力バッファーの先頭が NUL であること。
        EXPECT_EQ(0U, count); // [確認_異常系] - 失敗時に count_out が 0 であること。
    }
}

TEST_F(wcharConvTest, distinguishes_null_arguments_and_clears_valid_output)
{
    // Arrange
    wchar_t wide[4] = {L'x'};
    char utf8[4] = {'x'};
    size_t count = 42U;
    wchar_t *wide_out = reinterpret_cast<wchar_t *>(1);
    char *utf8_out = reinterpret_cast<char *>(1);

    // Act
    int wide_null_result = cplat_utf8_to_wstr(wide, 4U, &count, nullptr);
    // [手順] - UTF-8 入力に nullptr を指定して cplat_utf8_to_wstr() を呼び出す。
    count = 42U;
    int utf8_null_result = cplat_wstr_to_utf8(utf8, sizeof(utf8), &count, nullptr);
    // [手順] - UTF-16 入力に nullptr を指定して cplat_wstr_to_utf8() を呼び出す。
    int wide_out_null_result = cplat_utf8_to_wstr_alloc(nullptr, "text");
    // [手順] - cplat_utf8_to_wstr_alloc() の出力先に nullptr を指定する。
    int wide_source_null_result = cplat_utf8_to_wstr_alloc(&wide_out, nullptr);
    // [手順] - cplat_utf8_to_wstr_alloc() の入力に nullptr を指定する。
    int utf8_out_null_result = cplat_wstr_to_utf8_alloc(nullptr, L"text");
    // [手順] - cplat_wstr_to_utf8_alloc() の出力先に nullptr を指定する。
    int utf8_source_null_result = cplat_wstr_to_utf8_alloc(&utf8_out, nullptr);
    // [手順] - cplat_wstr_to_utf8_alloc() の入力に nullptr を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, wide_null_result); // [確認_異常系] - cplat_utf8_to_wstr() が CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(L'\0', wide[0]); // [確認_異常系] - 有効な出力バッファーの先頭を NUL にすること。
    EXPECT_EQ(0U, count); // [確認_異常系] - cplat_wstr_to_utf8() の失敗時に count_out が 0 であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, utf8_null_result); // [確認_異常系] - cplat_wstr_to_utf8() が CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ('\0', utf8[0]); // [確認_異常系] - 有効な出力バッファーの先頭を NUL にすること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, wide_out_null_result); // [確認_異常系] - NULL 出力先が CPLAT_ERR_INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, wide_source_null_result); // [確認_異常系] - NULL UTF-8 入力が CPLAT_ERR_INVALID_ARGUMENT になること。
    EXPECT_EQ(nullptr, wide_out); // [確認_異常系] - cplat_utf8_to_wstr_alloc() の失敗出力が nullptr であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, utf8_out_null_result); // [確認_異常系] - NULL 出力先が CPLAT_ERR_INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, utf8_source_null_result); // [確認_異常系] - NULL UTF-16 入力が CPLAT_ERR_INVALID_ARGUMENT になること。
    EXPECT_EQ(nullptr, utf8_out); // [確認_異常系] - cplat_wstr_to_utf8_alloc() の失敗出力が nullptr であること。
}

TEST_F(wcharConvTest, distinguishes_exact_capacity_from_one_element_too_small)
{
    // Arrange
    wchar_t wide[4] = {L'x'};
    char utf8[7] = {'x'};
    size_t count = 0U;

    // Act
    int wide_result = cplat_utf8_to_wstr(wide, 3U, &count, "\xE3\x81\x82\xF0\x9F\x98\x80");
    // [手順] - 必要な容量より UTF-16 の要素が 1 つ少ない出力先で変換する。
    int utf8_result = cplat_wstr_to_utf8(utf8, sizeof(utf8), &count, L"\u3042\U0001F600");
    // [手順] - 必要な容量より UTF-8 の byte が 1 つ少ない出力先で変換する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, wide_result); // [確認_異常系] - cplat_utf8_to_wstr() が CPLAT_ERR_BUFFER_TOO_SMALL を返すこと。
    EXPECT_EQ(L'\0', wide[0]); // [確認_異常系] - 容量不足時に UTF-16 出力の先頭が NUL であること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, utf8_result); // [確認_異常系] - cplat_wstr_to_utf8() が CPLAT_ERR_BUFFER_TOO_SMALL を返すこと。
    EXPECT_EQ('\0', utf8[0]); // [確認_異常系] - 容量不足時に UTF-8 出力の先頭が NUL であること。
    EXPECT_EQ(0U, count); // [確認_異常系] - 容量不足時に count_out が 0 であること。
}

TEST_F(wcharConvTest, normalizes_windows_path_separators)
{
    // Arrange
    char path[32] = {};
    size_t count = 0U;

    // Act
    int actual_result = cplat_wpath_to_utf8(path, sizeof(path), &count, L"C:\\temp\\name");
    // [手順] - バックスラッシュを含む UTF-16 パスを UTF-8 に変換する。

    // Assert
    ASSERT_EQ(CPLAT_OK, actual_result); // [確認_正常系] - cplat_wpath_to_utf8() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("C:/temp/name", path); // [確認_正常系] - 変換後の区切り文字が '/' であること。
    EXPECT_EQ(std::strlen(path) + 1U, count); // [確認_正常系] - count_out が NUL を含む byte 数であること。
}

TEST_F(wcharConvTest, allocation_failures_return_out_of_memory_and_null_outputs)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    wchar_t *wide_out = reinterpret_cast<wchar_t *>(1);
    char *utf8_out = reinterpret_cast<char *>(1);

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .Times(2)
        .WillOnce(Return(nullptr))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_正常系] - cplat_calloc() が 2 回呼び出されること。
                                   // [Pre-Assert手順] - cplat_calloc() から nullptr を 2 回返す。

    // Act
    int wide_result = cplat_utf8_to_wstr_alloc(&wide_out, "valid");
    // [手順] - 有効な UTF-8 の割り当て変換を実行し、確保を失敗させる。
    int utf8_result = cplat_wstr_to_utf8_alloc(&utf8_out, L"valid");
    // [手順] - 有効な UTF-16 の割り当て変換を実行し、確保を失敗させる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, wide_result); // [確認_異常系] - cplat_utf8_to_wstr_alloc() が CPLAT_ERR_OUT_OF_MEMORY を返すこと。
    EXPECT_EQ(nullptr, wide_out); // [確認_異常系] - UTF-16 の失敗出力が nullptr であること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, utf8_result); // [確認_異常系] - cplat_wstr_to_utf8_alloc() が CPLAT_ERR_OUT_OF_MEMORY を返すこと。
    EXPECT_EQ(nullptr, utf8_out); // [確認_異常系] - UTF-8 の失敗出力が nullptr であること。
}

TEST_F(wcharConvTest, conversion_apis_receive_strict_flags)
{
    // Arrange
    wchar_t *wide = nullptr;
    char *utf8 = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_windows_, MultiByteToWideChar(_, _, _, CP_UTF8, MB_ERR_INVALID_CHARS, _, -1, _, _))
        .Times(2); // [Pre-Assert確認_正常系] - MultiByteToWideChar() が厳密変換フラグで 2 回呼び出されること。
    EXPECT_CALL(mock_windows_, WideCharToMultiByte(_, _, _, CP_UTF8, WC_ERR_INVALID_CHARS, _, -1, _, _, _, _))
        .Times(2); // [Pre-Assert確認_正常系] - WideCharToMultiByte() が厳密変換フラグで 2 回呼び出されること。

    // Act
    int wide_result = cplat_utf8_to_wstr_alloc(&wide, "valid");
    // [手順] - 有効な UTF-8 を厳密変換フラグで割り当て版により UTF-16 へ変換する。
    int utf8_result = cplat_wstr_to_utf8_alloc(&utf8, L"valid");
    // [手順] - 有効な UTF-16 を厳密変換フラグで割り当て版により UTF-8 へ変換する。

    // Assert
    EXPECT_EQ(CPLAT_OK, wide_result); // [確認_正常系] - cplat_utf8_to_wstr_alloc() が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, utf8_result); // [確認_正常系] - cplat_wstr_to_utf8_alloc() が CPLAT_OK を返すこと。
    EXPECT_STREQ(L"valid", wide); // [確認_正常系] - UTF-16 の変換結果が入力に一致すること。
    EXPECT_STREQ("valid", utf8); // [確認_正常系] - UTF-8 の変換結果が入力に一致すること。

    cplat_free(wide);
    cplat_free(utf8);
}

TEST_F(wcharConvTest, maps_native_conversion_errors)
{
    // Arrange
    wchar_t wide[8] = {};
    char utf8[16] = {};

    // Pre-Assert
    EXPECT_CALL(mock_windows_, MultiByteToWideChar(_, _, _, CP_UTF8, MB_ERR_INVALID_CHARS, _, -1, _, _))
        .WillOnce([](const char *, int, const char *, UINT, DWORD, LPCCH, int, LPWSTR, int)
                  {
                      SetLastError(ERROR_NO_UNICODE_TRANSLATION);
                      return 0;
                  }); // [Pre-Assert確認_正常系] - MultiByteToWideChar() が 1 回呼び出されること。
                      // [Pre-Assert手順] - MultiByteToWideChar() から 0 を返し、最終エラーを ERROR_NO_UNICODE_TRANSLATION にする。

    // Act
    int invalid_utf8_result = cplat_utf8_to_wstr(wide, 8U, nullptr, "valid");
    // [手順] - 変換 API が ERROR_NO_UNICODE_TRANSLATION を返す状態で UTF-8 変換を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, invalid_utf8_result); // [確認_異常系] - 不正な文字コードの結果が CPLAT_ERR_INVALID_ENCODING であること。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, WideCharToMultiByte(_, _, _, CP_UTF8, WC_ERR_INVALID_CHARS, _, -1, _, _, _, _))
        .WillOnce([](const char *, int, const char *, UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL)
                  {
                      SetLastError(ERROR_NO_UNICODE_TRANSLATION);
                      return 0;
                  }); // [Pre-Assert確認_正常系] - WideCharToMultiByte() が 1 回呼び出されること。
                      // [Pre-Assert手順] - WideCharToMultiByte() から 0 を返し、最終エラーを ERROR_NO_UNICODE_TRANSLATION にする。

    // Act
    int invalid_utf16_result = cplat_wstr_to_utf8(utf8, sizeof(utf8), nullptr, L"valid");
    // [手順] - 変換 API が ERROR_NO_UNICODE_TRANSLATION を返す状態で UTF-16 変換を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, invalid_utf16_result); // [確認_異常系] - 不正な文字コードの結果が CPLAT_ERR_INVALID_ENCODING であること。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, MultiByteToWideChar(_, _, _, CP_UTF8, MB_ERR_INVALID_CHARS, _, -1, _, _))
        .WillOnce([](const char *, int, const char *, UINT, DWORD, LPCCH, int, LPWSTR, int)
                  {
                      SetLastError(ERROR_ACCESS_DENIED);
                      return 0;
                  }); // [Pre-Assert確認_正常系] - MultiByteToWideChar() が 1 回呼び出されること。
                      // [Pre-Assert手順] - MultiByteToWideChar() から 0 を返し、最終エラーを ERROR_ACCESS_DENIED にする。

    // Act
    int other_utf8_error = cplat_utf8_to_wstr(wide, 8U, nullptr, "valid");
    // [手順] - 変換 API が ERROR_ACCESS_DENIED を返す状態で UTF-8 変換を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, other_utf8_error); // [確認_異常系] - その他の Win32 エラーを共通結果コードへ変換すること。

    // Pre-Assert
    EXPECT_CALL(mock_windows_, WideCharToMultiByte(_, _, _, CP_UTF8, WC_ERR_INVALID_CHARS, _, -1, _, _, _, _))
        .WillOnce([](const char *, int, const char *, UINT, DWORD, LPCWCH, int, LPSTR, int, LPCCH, LPBOOL)
                  {
                      SetLastError(ERROR_ACCESS_DENIED);
                      return 0;
                  }); // [Pre-Assert確認_正常系] - WideCharToMultiByte() が 1 回呼び出されること。
                      // [Pre-Assert手順] - WideCharToMultiByte() から 0 を返し、最終エラーを ERROR_ACCESS_DENIED にする。

    // Act
    int other_utf16_error = cplat_wstr_to_utf8(utf8, sizeof(utf8), nullptr, L"valid");
    // [手順] - 変換 API が ERROR_ACCESS_DENIED を返す状態で UTF-16 変換を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, other_utf16_error); // [確認_異常系] - その他の Win32 エラーを共通結果コードへ変換すること。
}

#endif /* PLATFORM_WINDOWS */
