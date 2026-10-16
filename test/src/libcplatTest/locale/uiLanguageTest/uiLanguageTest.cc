#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/locale/ui_language.h>
#include <cplat/locale/ui_language_internal.h>

#include <stddef.h>
#include <string.h>

#if defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
#endif /* PLATFORM_WINDOWS */

namespace
{

/** 表示言語の決定で評価する環境変数です。 */
const char *const kEnvironmentNames[] = {"LC_ALL", "LC_MESSAGES", "LANG"};

/** `kEnvironmentNames` の要素数です。 */
const size_t kEnvironmentNameCount = sizeof(kEnvironmentNames) / sizeof(kEnvironmentNames[0]);

/** 退避する環境変数の値の最大長です (NUL 終端込み)。 */
const size_t kEnvironmentValueMax = 256U;

/**
 *  退避した環境変数の値です。
 *
 *  テスト フィクスチャのメンバーにすると、基底クラスを含む大きさが整列の境界に満たず、
 *  -Wpadded の警告が発生します。テストは 1 つずつ順に実行されるため、ファイル内で保持します。
 */
char g_saved_values[kEnvironmentNameCount][kEnvironmentValueMax];

/** 退避した環境変数が設定されていたかどうかです。 */
int g_saved_exists[kEnvironmentNameCount];

} // namespace

// 実行環境の環境変数を退避し、テストごとに未設定の状態から開始する
class uiLanguageTest : public Test
{
  protected:
    // [サブ手順 名前=uiLanguageTest.SetUp]
    void SetUp() override
    {
        for (size_t index = 0U; index < kEnvironmentNameCount; index++)
        {
            int exists = 0;

            g_saved_values[index][0] = '\0';
            (void)cplat_getenv(kEnvironmentNames[index], g_saved_values[index], kEnvironmentValueMax, &exists, nullptr);
            g_saved_exists[index] = exists;
            (void)cplat_unsetenv(kEnvironmentNames[index], nullptr);
        }
    }
    // [サブ手順終了]

    // [サブ手順 名前=uiLanguageTest.TearDown]
    void TearDown() override
    {
        for (size_t index = 0U; index < kEnvironmentNameCount; index++)
        {
            if (g_saved_exists[index] != 0)
            {
                (void)cplat_setenv(kEnvironmentNames[index], g_saved_values[index], 1, nullptr);
            }
            else
            {
                (void)cplat_unsetenv(kEnvironmentNames[index], nullptr);
            }
        }
    }
    // [サブ手順終了]
};

// LANG だけが設定されている場合に、その指定を使用することの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, UsesLangWhenOnlyLangIsSet)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    ASSERT_EQ(CPLAT_OK,
              cplat_setenv("LANG", "ja_JP.UTF-8", 1, nullptr)); // [状態] - LANG へ日本語のロケールを設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ja-JP", tag);      // [確認_正常系] - tag が "ja-JP" であること。
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// LC_ALL が他の環境変数より優先されることの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, PrefersLcAllOverOtherVariables)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    ASSERT_EQ(CPLAT_OK,
              cplat_setenv("LC_ALL", "en_US.UTF-8", 1, nullptr)); // [状態] - LC_ALL へ英語のロケールを設定する。
    // [状態確認] - LC_ALL を設定した cplat_setenv() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_setenv("LC_MESSAGES", "fr_FR", 1, nullptr)); // [状態] - LC_MESSAGES へ別のロケールを設定する。
    // [状態確認] - LC_MESSAGES を設定した cplat_setenv() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_setenv("LANG", "ja_JP.UTF-8", 1, nullptr)); // [状態] - LANG へ別のロケールを設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("en-US", tag);      // [確認_正常系] - tag が "en-US" であること。(LC_ALL の指定を優先したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// LC_MESSAGES が LANG より優先されることの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, PrefersLcMessagesOverLang)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    ASSERT_EQ(CPLAT_OK, cplat_setenv("LC_MESSAGES", "en_US.UTF-8", 1,
                                     nullptr)); // [状態] - LC_MESSAGES へ英語のロケールを設定する。
    // [状態確認] - LC_MESSAGES を設定した cplat_setenv() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_setenv("LANG", "ja_JP.UTF-8", 1, nullptr)); // [状態] - LANG へ日本語のロケールを設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("en-US", tag); // [確認_正常系] - tag が "en-US" であること。(LC_MESSAGES の指定を優先したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// 解釈できない指定を読み飛ばし、次の候補を使用することの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, SkipsUninterpretableValue)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    ASSERT_EQ(CPLAT_OK, cplat_setenv("LC_ALL", "!!!", 1, nullptr)); // [状態] - LC_ALL へ解釈できない値を設定する。
    // [状態確認] - LC_ALL を設定した cplat_setenv() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_setenv("LANG", "ja_JP.UTF-8", 1, nullptr)); // [状態] - LANG へ日本語のロケールを設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "ja-JP",
        tag); // [確認_正常系] - tag が "ja-JP" であること。(解釈できない LC_ALL を読み飛ばし、LANG の指定を使用したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// 言語を指定しないロケールで、後続の候補を評価せずにニュートラルを返すことの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, StopsAtLanguageNeutralLocale)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    ASSERT_EQ(CPLAT_OK, cplat_setenv("LC_ALL", "C", 1, nullptr)); // [状態] - LC_ALL へ C を設定する。
    // [状態確認] - LC_ALL を設定した cplat_setenv() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_setenv("LANG", "ja_JP.UTF-8", 1, nullptr)); // [状態] - LANG へ日本語のロケールを設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", tag); // [確認_正常系] - tag が "" であること。(LANG を評価せず、ニュートラルを返したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// 文字コードを伴う C の指定でニュートラルを返すことの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, ReturnsNeutralForCLocaleWithCodeset)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    ASSERT_EQ(CPLAT_OK, cplat_setenv("LANG", "C.UTF-8", 1, nullptr)); // [状態] - LANG へ C.UTF-8 を設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", tag);           // [確認_正常系] - tag が "" であること。(ニュートラルを返したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// POSIX の指定でニュートラルを返すことの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, ReturnsNeutralForPosixLocale)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    ASSERT_EQ(CPLAT_OK, cplat_setenv("LANG", "POSIX", 1, nullptr)); // [状態] - LANG へ POSIX を設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", tag);           // [確認_正常系] - tag が "" であること。(ニュートラルを返したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// 出力先の容量が不足する場合に、容量不足を通知することの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, ReportsSmallBuffer)
{
    // Arrange
    char tag[3] = {'x', 'x', '\0'};

    ASSERT_EQ(CPLAT_OK,
              cplat_setenv("LANG", "ja_JP.UTF-8", 1, nullptr)); // [状態] - LANG へ日本語のロケールを設定する。
    // [状態確認] - LANG を設定した cplat_setenv() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - ja-JP が収まらない出力先へ表示言語を取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", tag); // [確認_異常系] - tag が "" であること。
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// 不正な出力引数を拒否することの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, RejectsInvalidOutputArguments)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX] = {'x', '\0'};

    // Pre-Assert

    // Act
    int actual_ret_null =
        cplat_ui_language_get_tag(nullptr, sizeof(tag));     // [手順] - 出力先に NULL を渡して表示言語を取得する。
    int actual_ret_zero = cplat_ui_language_get_tag(tag, 0); // [手順] - 出力先サイズに 0 を渡して表示言語を取得する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null); // [確認_異常系] - 出力先が NULL の cplat_ui_language_get_tag() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero); // [確認_異常系] - 出力先サイズが 0 の cplat_ui_language_get_tag() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('x',
              tag[0]); // [確認_異常系] - tag[0] が 'x' であること。(不正引数の呼び出しで出力先が変更されないことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

#if defined(PLATFORM_LINUX)

// 環境変数が未設定の Linux でニュートラルを返すことの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, ReturnsNeutralWhenEnvironmentIsUnset)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 環境変数が未設定の状態で表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", tag);           // [確認_正常系] - tag が "" であること。(ニュートラルを返したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

#elif defined(PLATFORM_WINDOWS)

// 環境変数が未設定の Windows で、OS の表示言語を言語タグの表記で返すことの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, ReturnsWindowsUiLanguageWhenEnvironmentIsUnset)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];
    char normalized[CPLAT_UI_LANGUAGE_TAG_MAX] = {};
    int normalize_ret = CPLAT_OK;

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 環境変数が未設定の状態で表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    if (tag[0] != '\0')
    {
        // OS の表示言語は実行環境によって異なるため、値ではなく表記が規則に従うことを確認する
        normalize_ret = cplat_internal_ui_language_normalize(tag, normalized, sizeof(normalized));
    }
    EXPECT_EQ(
        CPLAT_OK,
        normalize_ret); // [確認_正常系] - normalize_ret が CPLAT_OK であること。(空の言語タグを許容し、取得した言語タグを解釈できることの確認)
    EXPECT_STREQ(
        tag,
        normalized); // [確認_正常系] - normalized が tag と一致すること。(取得した言語タグが空または正規化済みの表記であることの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// mock 化した表示言語の優先順位の先頭が、言語タグへ変換されることの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, ConvertsMockedWindowsUiLanguage)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetUserPreferredUILanguages(_, _, _, MUI_LANGUAGE_NAME, _, _, _))
        .WillRepeatedly(Invoke(
            [](const char *, const int, const char *, DWORD, PULONG language_count, PZZWSTR languages,
               PULONG language_size) -> BOOL
            {
                /* 表示言語の優先順位は NUL 区切りの一覧で返り、末尾は 2 つの NUL となる */
                static const wchar_t kLanguages[] = L"ja-JP\0en-US\0";
                const ULONG needed = (ULONG)(sizeof(kLanguages) / sizeof(kLanguages[0]));

                if (language_count == nullptr || language_size == nullptr)
                {
                    return FALSE;
                }
                if (languages == nullptr)
                {
                    *language_count = 2;
                    *language_size = needed;
                    return TRUE;
                }
                if (*language_size < needed)
                {
                    *language_size = needed;
                    return FALSE;
                }
                memcpy(languages, kLanguages, sizeof(kLanguages));
                *language_count = 2;
                *language_size = needed;
                return TRUE;
            })); // [Pre-Assert確認_正常系] - GetUserPreferredUILanguages() が MUI_LANGUAGE_NAME を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - ja-JP と en-US の一覧を返却する。

    // Act
    int actual_ret =
        cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 環境変数が未設定の状態で表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ja-JP", tag); // [確認_正常系] - tag が "ja-JP" であること。(一覧の先頭の表示言語を取得したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// 表示言語の優先順位を取得できない場合に、地域設定の名前を使用することの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, FallsBackToUserDefaultLocaleName)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetUserPreferredUILanguages(_, _, _, _, _, _, _))
        .WillRepeatedly(
            Return(FALSE)); // [Pre-Assert確認_正常系] - GetUserPreferredUILanguages() が任意の回数呼び出されること。
                            // [Pre-Assert手順] - 失敗を返却する。
    EXPECT_CALL(mock_windows, GetUserDefaultLocaleName(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, LPWSTR locale_name, int locale_name_count) -> int
            {
                static const wchar_t kName[] = L"en-US";
                const int needed = (int)(sizeof(kName) / sizeof(kName[0]));

                if (locale_name == nullptr || locale_name_count < needed)
                {
                    return 0;
                }
                memcpy(locale_name, kName, sizeof(kName));
                return needed;
            })); // [Pre-Assert確認_正常系] - GetUserDefaultLocaleName() が 1 回呼び出されること。
                 // [Pre-Assert手順] - en-US を返却する。

    // Act
    int actual_ret =
        cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 環境変数が未設定の状態で表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("en-US",
                 tag); // [確認_正常系] - tag が "en-US" であること。(地域設定の名前を言語タグとして取得したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

// OS から表示言語を取得できない場合にニュートラルを返すことの確認
// [サブ手順参照 名前=uiLanguageTest.SetUp]
TEST_F(uiLanguageTest, ReturnsNeutralWhenWindowsApisFail)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetUserPreferredUILanguages(_, _, _, _, _, _, _))
        .WillRepeatedly(
            Return(FALSE)); // [Pre-Assert確認_異常系] - GetUserPreferredUILanguages() が任意の回数呼び出されること。
                            // [Pre-Assert手順] - 失敗を返却する。
    EXPECT_CALL(mock_windows, GetUserDefaultLocaleName(_, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - GetUserDefaultLocaleName() が 1 回呼び出されること。
                              // [Pre-Assert手順] - 失敗を返却する。

    // Act
    int actual_ret =
        cplat_ui_language_get_tag(tag, sizeof(tag)); // [手順] - 環境変数が未設定の状態で表示言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ui_language_get_tag() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", tag);           // [確認_正常系] - tag が "" であること。(ニュートラルを返したことの確認)
}
// [サブ手順参照 名前=uiLanguageTest.TearDown]

#endif /* PLATFORM_ */

// 言語タグの正規化を、環境変数を介さずに確認する
class uiLanguageTagTest : public Test
{
};

// 区切り文字と大文字小文字を正規化することの確認
TEST_F(uiLanguageTagTest, NormalizesSeparatorAndLetterCase)
{
    // Arrange
    char tag_underscore[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_upper[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_script[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_digit_region[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert

    // Act
    int actual_ret_underscore = cplat_internal_ui_language_normalize(
        "zh_TW", tag_underscore, sizeof(tag_underscore)); // [手順] - 下線区切りの指定を正規化する。
    int actual_ret_upper = cplat_internal_ui_language_normalize(
        "JA_jp", tag_upper, sizeof(tag_upper)); // [手順] - 大文字小文字が規則と異なる指定を正規化する。
    int actual_ret_script = cplat_internal_ui_language_normalize(
        "zh-hans-cn", tag_script, sizeof(tag_script)); // [手順] - 表記体系を含む指定を正規化する。
    int actual_ret_digit_region = cplat_internal_ui_language_normalize(
        "es-419", tag_digit_region, sizeof(tag_digit_region)); // [手順] - 数字の地域を含む指定を正規化する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_underscore); // [確認_正常系] - 下線区切りの指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("zh-TW", tag_underscore); // [確認_正常系] - tag_underscore が "zh-TW" であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_upper); // [確認_正常系] - 大文字小文字が異なる指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ja-JP", tag_upper); // [確認_正常系] - tag_upper が "ja-JP" であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_script); // [確認_正常系] - 表記体系を含む指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("zh-Hans-CN", tag_script); // [確認_正常系] - tag_script が "zh-Hans-CN" であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_digit_region); // [確認_正常系] - 数字の地域を含む指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("es-419", tag_digit_region); // [確認_正常系] - tag_digit_region が "es-419" であること。
}

// 文字コードと修飾子を取り除くことの確認
TEST_F(uiLanguageTagTest, RemovesCodesetAndModifier)
{
    // Arrange
    char tag_codeset[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_modifier[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert

    // Act
    int actual_ret_codeset = cplat_internal_ui_language_normalize(
        "ja_JP.UTF-8", tag_codeset, sizeof(tag_codeset)); // [手順] - 文字コードを含む指定を正規化する。
    int actual_ret_modifier = cplat_internal_ui_language_normalize(
        "ca_ES@valencia", tag_modifier, sizeof(tag_modifier)); // [手順] - 修飾子を含む指定を正規化する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_codeset); // [確認_正常系] - 文字コードを含む指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ja-JP", tag_codeset); // [確認_正常系] - tag_codeset が "ja-JP" であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_modifier); // [確認_正常系] - 修飾子を含む指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ca-ES", tag_modifier); // [確認_正常系] - tag_modifier が "ca-ES" であること。
}

// 表記体系と地域として解釈できない区別を取り込まないことの確認
TEST_F(uiLanguageTagTest, IgnoresSubtagsAfterRegion)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_mixed[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_internal_ui_language_normalize("ja-JP-u-ca-japanese", tag,
                                             sizeof(tag)); // [手順] - 地域の後ろに区別が続く指定を正規化する。
    int actual_ret_mixed = cplat_internal_ui_language_normalize(
        "es-4a9", tag_mixed, sizeof(tag_mixed)); // [手順] - 英字と数字が混在する区別を含む指定を正規化する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 地域の後ろに区別が続く指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ja-JP", tag); // [確認_正常系] - tag が "ja-JP" であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_mixed); // [確認_正常系] - 英字と数字が混在する区別を含む指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "es",
        tag_mixed); // [確認_正常系] - tag_mixed が "es" であること。(地域として解釈できない区別を取り込まないことの確認)
}

// 解釈できない指定を通知することの確認
TEST_F(uiLanguageTagTest, RejectsUninterpretableValue)
{
    // Arrange
    char tag_symbol[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_short[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_trailing[CPLAT_UI_LANGUAGE_TAG_MAX];
    char tag_codeset_only[CPLAT_UI_LANGUAGE_TAG_MAX];

    // Pre-Assert

    // Act
    int actual_ret_symbol = cplat_internal_ui_language_normalize(
        "!!!", tag_symbol, sizeof(tag_symbol)); // [手順] - 英字以外の指定を正規化する。
    int actual_ret_short =
        cplat_internal_ui_language_normalize("j", tag_short,
                                             sizeof(tag_short)); // [手順] - 言語が 1 文字の指定を正規化する。
    int actual_ret_trailing = cplat_internal_ui_language_normalize(
        "ja_", tag_trailing, sizeof(tag_trailing)); // [手順] - 区切り文字で終わる指定を正規化する。
    int actual_ret_codeset_only = cplat_internal_ui_language_normalize(
        ".UTF-8", tag_codeset_only, sizeof(tag_codeset_only)); // [手順] - 文字コードだけの指定を正規化する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_symbol); // [確認_異常系] - 英字以外の指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("", tag_symbol); // [確認_異常系] - tag_symbol が "" であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_short); // [確認_異常系] - 言語が 1 文字の指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_trailing); // [確認_異常系] - 区切り文字で終わる指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_codeset_only); // [確認_異常系] - 文字コードだけの指定に対する cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 不正な引数を拒否することの確認
TEST_F(uiLanguageTagTest, RejectsInvalidArguments)
{
    // Arrange
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX] = {'x', '\0'};

    // Pre-Assert

    // Act
    int actual_ret_null_value =
        cplat_internal_ui_language_normalize(nullptr, tag, sizeof(tag)); // [手順] - 正規化する値に NULL を渡す。
    int actual_ret_null_tag =
        cplat_internal_ui_language_normalize("ja", nullptr, sizeof(tag));     // [手順] - 出力先に NULL を渡す。
    int actual_ret_zero = cplat_internal_ui_language_normalize("ja", tag, 0); // [手順] - 出力先サイズに 0 を渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_value); // [確認_異常系] - 値が NULL の cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_tag); // [確認_異常系] - 出力先が NULL の cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero); // [確認_異常系] - 出力先サイズが 0 の cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 出力先の容量が不足する場合に、容量不足を通知することの確認
TEST_F(uiLanguageTagTest, ReportsSmallBuffer)
{
    // Arrange
    char tag[3];

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_ui_language_normalize(
        "ja_JP", tag, sizeof(tag)); // [手順] - ja-JP が収まらない出力先へ正規化する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_internal_ui_language_normalize() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}
