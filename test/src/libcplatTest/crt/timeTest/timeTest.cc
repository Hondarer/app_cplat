#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crt/time.h>
#include <mock_time.h>
#include <string.h>

class timeTest : public Test
{
};

// エポック 0 秒が UTC の 1970-01-01 00:00:00 に変換されることの確認
TEST_F(timeTest, gmtime_success_epoch)
{
    // Arrange
    struct tm utc_tm;
    time_t epoch = 0;                      // [状態] - 変換対象のエポック秒を 0 とする。
    memset(&utc_tm, 0xff, sizeof(utc_tm)); // [状態] - 出力構造体を 0xff で埋め、書き換えを検出できるようにする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_gmtime(&utc_tm, &epoch); // [手順] - cplat_gmtime(&utc_tm, &epoch) を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);   // [確認_正常系] - cplat_gmtime の戻り値が CPLAT_OK であること。
    EXPECT_EQ(70, utc_tm.tm_year); // [確認_正常系] - tm_year が 70 (1970 年) であること。
    EXPECT_EQ(0, utc_tm.tm_mon);   // [確認_正常系] - tm_mon が 0 (1 月) であること。
    EXPECT_EQ(1, utc_tm.tm_mday);  // [確認_正常系] - tm_mday が 1 日であること。
    EXPECT_EQ(0, utc_tm.tm_hour);  // [確認_正常系] - tm_hour が 0 時であること。
    EXPECT_EQ(0, utc_tm.tm_min);   // [確認_正常系] - tm_min が 0 分であること。
    EXPECT_EQ(0, utc_tm.tm_sec);   // [確認_正常系] - tm_sec が 0 秒であること。
}

// 出力構造体が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(timeTest, gmtime_null_tm)
{
    // Arrange
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_gmtime(NULL, &epoch); // [手順] - 出力構造体に NULL を渡して cplat_gmtime を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_gmtime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 時刻が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(timeTest, gmtime_null_time)
{
    // Arrange
    struct tm utc_tm; // [状態] - 出力構造体を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_gmtime(&utc_tm, NULL); // [手順] - 時刻に NULL を渡して cplat_gmtime を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_gmtime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// OS の変換関数が失敗した場合に CPLAT_ERR_UNKNOWN を返し出力構造体がゼロ クリアされることの確認
TEST_F(timeTest, gmtime_zeroes_tm_when_platform_conversion_fails)
{
    // Arrange
    struct tm utc_tm;
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。

    utc_tm.tm_year = 1;
    utc_tm.tm_mon = 2;
    utc_tm.tm_mday = 3;
    utc_tm.tm_hour = 4;
    utc_tm.tm_min = 5;
    utc_tm.tm_sec = 6; // [状態] - 出力構造体の各フィールドに 0 以外の値を設定し、ゼロ クリアを検出できるようにする。

    Mock_time mock_time;

    // Pre-Assert
    // OS の時刻変換関数が epoch=0 と有効な出力先で 1 回呼び出されること。
    // [Pre-Assert手順] - OS の時刻変換関数から失敗を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_time, gmtime_r(_, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, const time_t *timep, struct tm *result)
            {
                EXPECT_EQ((time_t)0, *timep);
                // [確認_正常系] - `gmtime_r` に渡す時刻の秒数が `(time_t)0` であること。
                EXPECT_NE((struct tm *)NULL, result);
                // [確認_正常系] - `(struct tm *)NULL` と `result` が異なること。
                return (struct tm *)NULL;
            });
    // [Pre-Assert確認_異常系] - mock_time の gmtime_r(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_time, gmtime_s(_, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, struct tm *result, const time_t *timep)
            {
                EXPECT_EQ((time_t)0, *timep);
                // [確認_正常系] - `gmtime_s` に渡す時刻の秒数が `(time_t)0` であること。
                EXPECT_NE((struct tm *)NULL, result);
                // [確認_正常系] - `(struct tm *)NULL` と `result` が異なること。
                return 1;
            });
    // [Pre-Assert確認_異常系] - mock_time の gmtime_s(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    int actual_ret = cplat_gmtime(&utc_tm, &epoch); // [手順] - cplat_gmtime(&utc_tm, &epoch) を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret);               // [確認_異常系] - cplat_gmtime の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, utc_tm.tm_year); // [確認_異常系] - tm_year が 0 にクリアされること。
    EXPECT_EQ(0, utc_tm.tm_mon);  // [確認_異常系] - tm_mon が 0 にクリアされること。
    EXPECT_EQ(0, utc_tm.tm_mday); // [確認_異常系] - tm_mday が 0 にクリアされること。
    EXPECT_EQ(0, utc_tm.tm_hour); // [確認_異常系] - tm_hour が 0 にクリアされること。
    EXPECT_EQ(0, utc_tm.tm_min);  // [確認_異常系] - tm_min が 0 にクリアされること。
    EXPECT_EQ(0, utc_tm.tm_sec);  // [確認_異常系] - tm_sec が 0 にクリアされること。
}

// cplat_localtime の変換結果が OS のローカル時刻変換の結果と一致することの確認
TEST_F(timeTest, localtime_matches_platform_result)
{
    // Arrange
    struct tm expected_tm = {};
    struct tm actual_tm;
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。
    memset(&actual_tm, 0xff, sizeof(actual_tm)); // [状態] - 出力構造体を 0xff で埋め、書き換えを検出できるようにする。

    // Pre-Assert
    // [Pre-Assert手順] - OS のローカル時刻変換で期待値 expected_tm を取得する。
#if defined(PLATFORM_LINUX)
    ASSERT_NE((struct tm *)NULL, localtime_r(&epoch, &expected_tm));
    // [確認_正常系] - `(struct tm *)NULL` と `localtime_r(&epoch, &expected_tm)` が異なること。
#elif defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, localtime_s(&expected_tm, &epoch));
    // [確認_正常系] - `localtime_s(&expected_tm, &epoch)` の戻り値が `0` であること。
#endif

    // Act
    int actual_ret_localtime =
        cplat_localtime(&actual_tm, &epoch); // [手順] - cplat_localtime(&actual_tm, &epoch) を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_localtime); // [確認_正常系] - cplat_localtime の戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected_tm.tm_year, actual_tm.tm_year); // [確認_正常系] - tm_year が OS の変換結果と一致すること。
    EXPECT_EQ(expected_tm.tm_mon, actual_tm.tm_mon);   // [確認_正常系] - tm_mon が OS の変換結果と一致すること。
    EXPECT_EQ(expected_tm.tm_mday, actual_tm.tm_mday); // [確認_正常系] - tm_mday が OS の変換結果と一致すること。
    EXPECT_EQ(expected_tm.tm_hour, actual_tm.tm_hour); // [確認_正常系] - tm_hour が OS の変換結果と一致すること。
    EXPECT_EQ(expected_tm.tm_min, actual_tm.tm_min);   // [確認_正常系] - tm_min が OS の変換結果と一致すること。
    EXPECT_EQ(expected_tm.tm_sec, actual_tm.tm_sec);   // [確認_正常系] - tm_sec が OS の変換結果と一致すること。
}

// 出力構造体が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(timeTest, localtime_null_tm)
{
    // Arrange
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。

    // Pre-Assert

    // Act
    int actual_ret_localtime =
        cplat_localtime(NULL, &epoch); // [手順] - 出力構造体に NULL を渡して cplat_localtime を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_localtime); // [確認_異常系] - cplat_localtime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 時刻が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(timeTest, localtime_null_time)
{
    // Arrange
    struct tm local_tm; // [状態] - 出力構造体を用意する。

    // Pre-Assert

    // Act
    int actual_ret_localtime =
        cplat_localtime(&local_tm, NULL); // [手順] - 時刻に NULL を渡して cplat_localtime を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_localtime); // [確認_異常系] - cplat_localtime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// OS のローカル時刻変換が失敗した場合に出力構造体をゼロ クリアすることの確認
TEST_F(timeTest, localtime_zeroes_tm_when_platform_conversion_fails)
{
    // Arrange
    struct tm local_tm;
    time_t epoch = 0;                          // [状態] - 変換対象のエポック秒を 0 とする。
    memset(&local_tm, 0xff, sizeof(local_tm)); // [状態] - 出力構造体を 0xff で埋め、ゼロ クリアを検出できるようにする。
    Mock_time mock_time;

    // Pre-Assert
    // OS のローカル時刻変換関数が有効な引数で 1 回呼び出されること。
    // [Pre-Assert手順] - OS のローカル時刻変換関数から失敗を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_time, localtime_r(_, _, _, _, _)).WillOnce(Return((struct tm *)NULL));
    // [Pre-Assert確認_異常系] - mock_time の localtime_r(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_time, localtime_s(_, _, _, _, _)).WillOnce(Return(1));
    // [Pre-Assert確認_異常系] - mock_time の localtime_s(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    int actual_ret_localtime =
        cplat_localtime(&local_tm, &epoch); // [手順] - OS の変換失敗を注入して cplat_localtime を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret_localtime);       // [確認_異常系] - cplat_localtime の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, local_tm.tm_year); // [確認_異常系] - tm_year が 0 にクリアされること。
    EXPECT_EQ(0, local_tm.tm_mon);  // [確認_異常系] - tm_mon が 0 にクリアされること。
    EXPECT_EQ(0, local_tm.tm_mday); // [確認_異常系] - tm_mday が 0 にクリアされること。
    EXPECT_EQ(0, local_tm.tm_hour); // [確認_異常系] - tm_hour が 0 にクリアされること。
    EXPECT_EQ(0, local_tm.tm_min);  // [確認_異常系] - tm_min が 0 にクリアされること。
    EXPECT_EQ(0, local_tm.tm_sec);  // [確認_異常系] - tm_sec が 0 にクリアされること。
}

// エポック 0 秒が ctime 形式の文字列に変換されることの確認
TEST_F(timeTest, ctime_success_epoch)
{
    // Arrange
    char buf[26];
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。
    size_t len;
    memset(buf, 0xff, sizeof(buf)); // [状態] - 出力バッファーを 0xff で埋め、書き換えを検出できるようにする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_ctime(buf, sizeof(buf), &epoch); // [手順] - cplat_ctime(buf, 26, &epoch) を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_ctime の戻り値が CPLAT_OK であること。

    /* ctime はローカル時刻依存のため固定文字列比較は行わず、形式のみを確認する */
    len = strlen(buf);
    EXPECT_GE(len, (size_t)24);    // [確認_正常系] - 変換結果が 24 文字以上であること。
    EXPECT_LE(len, (size_t)25);    // [確認_正常系] - 変換結果が 25 文字以下であること。
    EXPECT_EQ('\n', buf[len - 1]); // [確認_正常系] - 変換結果が改行で終わること。
}

// cplat_ctime の変換結果が OS の ctime 系関数の結果と一致することの確認
TEST_F(timeTest, ctime_matches_platform_result)
{
    // Arrange
    char expected[26] = {0};
    char actual[26];
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。
    memset(actual, 0xff, sizeof(actual)); // [状態] - 出力バッファーを 0xff で埋め、書き換えを検出できるようにする。

    // Pre-Assert
    // [Pre-Assert手順] - OS の ctime 系関数で期待値 expected を取得する。
#if defined(PLATFORM_LINUX)
    ASSERT_NE((char *)NULL, ctime_r(&epoch, expected));
    // [確認_正常系] - `(char *)NULL` と `ctime_r(&epoch, expected)` が異なること。
#elif defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ctime_s(expected, sizeof(expected), &epoch));
    // [確認_正常系] - `ctime_s(expected, sizeof(expected), &epoch)` の戻り値が `0` であること。
#endif

    // Act
    int actual_ret_ctime =
        cplat_ctime(actual, sizeof(actual), &epoch); // [手順] - cplat_ctime(actual, 26, &epoch) を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_ctime); // [確認_正常系] - cplat_ctime の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(expected, actual);    // [確認_正常系] - 変換結果が OS の変換結果と一致すること。
}

// 出力バッファーが NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(timeTest, ctime_null_buf)
{
    // Arrange
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。

    // Pre-Assert

    // Act
    int actual_ret_ctime =
        cplat_ctime(NULL, 26, &epoch); // [手順] - 出力バッファーに NULL を渡して cplat_ctime を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_ctime); // [確認_異常系] - cplat_ctime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 時刻が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返し出力バッファーがゼロ クリアされることの確認
TEST_F(timeTest, ctime_null_time_zeroes_buf)
{
    // Arrange
    char buf[26];
    memset(buf, 0xff, sizeof(buf)); // [状態] - 出力バッファーを 0xff で埋め、ゼロ クリアを検出できるようにする。

    // Pre-Assert

    // Act
    int actual_ret_ctime = cplat_ctime(buf, sizeof(buf), NULL); // [手順] - 時刻に NULL を渡して cplat_ctime を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_ctime); // [確認_異常系] - cplat_ctime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // 出力バッファーの全バイトが '\0' にクリアされること。
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        EXPECT_EQ('\0', buf[i]);
        // [確認_異常系 回数=26] - 出力バッファーの各バイトが `\0` にクリアされること。
    }
}

// 出力バッファーが必要サイズ未満の場合に CPLAT_ERR_INVALID_ARGUMENT を返しバッファーがゼロ クリアされることの確認
TEST_F(timeTest, ctime_small_buf_zeroes_buf)
{
    // Arrange
    char buf[25];                   // [状態] - 必要サイズ 26 より小さい 25 バイトのバッファーを用意する。
    time_t epoch = 0;               // [状態] - 変換対象のエポック秒を 0 とする。
    memset(buf, 0xff, sizeof(buf)); // [状態] - 出力バッファーを 0xff で埋め、ゼロ クリアを検出できるようにする。

    // Pre-Assert

    // Act
    int actual_ret_ctime =
        cplat_ctime(buf, sizeof(buf), &epoch); // [手順] - サイズ 25 のバッファーを渡して cplat_ctime を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_ctime); // [確認_異常系] - cplat_ctime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // 出力バッファーの全バイトが '\0' にクリアされること。
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        EXPECT_EQ('\0', buf[i]);
        // [確認_異常系 回数=25] - 出力バッファーの各バイトが `\0` にクリアされること。
    }
}

// バッファー サイズが 0 の場合に CPLAT_ERR_INVALID_ARGUMENT を返しバッファーへ書き込まないことの確認
TEST_F(timeTest, ctime_zero_buf_size_fails_without_write)
{
    // Arrange
    char buf[1];
    time_t epoch = 0; // [状態] - 変換対象のエポック秒を 0 とする。
    buf[0] = 0x7f;    // [状態] - バッファーの先頭に 0x7f を設定し、書き込みを検出できるようにする。

    // Pre-Assert

    // Act
    int actual_ret_ctime =
        cplat_ctime(buf, 0, &epoch); // [手順] - バッファー サイズに 0 を渡して cplat_ctime を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_ctime); // [確認_異常系] - cplat_ctime の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    /* buf_size が 0 のため memset も書き込みを行わないこと */
    EXPECT_EQ(0x7f, buf[0]); // [確認_異常系] - バッファーの先頭が 0x7f のまま書き込まれないこと。
}

// OS の変換関数が失敗した場合に CPLAT_ERR_UNKNOWN を返し出力バッファーがゼロ クリアされることの確認
TEST_F(timeTest, ctime_zeroes_buf_when_platform_conversion_fails)
{
    // Arrange
    char buf[26];
    time_t epoch = 0;               // [状態] - 変換対象のエポック秒を 0 とする。
    memset(buf, 0xff, sizeof(buf)); // [状態] - 出力バッファーを 0xff で埋め、ゼロ クリアを検出できるようにする。

    Mock_time mock_time;

    // Pre-Assert
    // OS の ctime 系関数が epoch=0 と有効な出力先で 1 回呼び出されること。
    // [Pre-Assert手順] - OS の ctime 系関数から失敗を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_time, ctime_r(_, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, const time_t *timep, char *result)
            {
                EXPECT_EQ((time_t)0, *timep);
                // [確認_正常系] - `ctime_r` に渡す時刻の秒数が `(time_t)0` であること。
                EXPECT_NE((char *)NULL, result);
                // [確認_正常系] - `(char *)NULL` と `result` が異なること。
                return (char *)NULL;
            });
    // [Pre-Assert確認_異常系] - mock_time の ctime_r(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_time, ctime_s(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, char *result, size_t size, const time_t *timep)
            {
                EXPECT_EQ((time_t)0, *timep);
                // [確認_正常系] - `ctime_s` に渡す時刻の秒数が `(time_t)0` であること。
                EXPECT_EQ((size_t)26, size);
                // [確認_正常系] - `size` の値が `(size_t)26` であること。
                EXPECT_NE((char *)NULL, result);
                // [確認_正常系] - `(char *)NULL` と `result` が異なること。
                return 1;
            });
    // [Pre-Assert確認_異常系] - mock_time の ctime_s(_, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    int actual_ret = cplat_ctime(buf, sizeof(buf), &epoch); // [手順] - cplat_ctime(buf, 26, &epoch) を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - cplat_ctime の戻り値が CPLAT_ERR_UNKNOWN であること。

    // 出力バッファーの全バイトが '\0' にクリアされること。
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        EXPECT_EQ('\0', buf[i]);
        // [確認_異常系 回数=26] - 出力バッファーの各バイトが `\0` にクリアされること。
    }
}
