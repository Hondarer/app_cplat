#include <testfw.h>
#include <cplat/crt/stdio.h>
#include <cplat/runtime/sym_loader.h>
#include <mock_cjson.h>
#include <mock_cplat.h>
#include <cstdint>
#include <cstring>
#include <string>

using testing::_;
using testing::DoDefault;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

class symLoaderInitTest : public Test
{
  protected:
    // MSVC では Mock_cplat 生成により mock_cplat の弱参照 obj がリンクに取り込まれ、
    // cplat_fopen 等の /ALTERNATENAME が有効になる。既定動作は製品 (prod) の処理への委譲。
    NiceMock<Mock_cplat> mock_cplat_;
    FILE *const file_ = reinterpret_cast<FILE *>(static_cast<uintptr_t>(0x70));

    // [サブ手順 名前=symLoaderInitTest.expect_config_read]
    void expect_config_read(const char *path, const char *content)
    {
        const size_t size = std::strlen(content);

        // 指定パスを rb で開き、指定本文を読み取って閉じる。
        EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq(path), StrEq("rb"), nullptr)).WillOnce(Return(file_));
        // [Pre-Assert手順] - cplat_fopen() から設定ファイルを開くハンドルを返す。
        // [Pre-Assert確認_正常系] - cplat_fopen() が path と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
        EXPECT_CALL(mock_cplat_, cplat_fseek(file_, 0, SEEK_END)).WillOnce(Return(0));
        // [Pre-Assert手順] - cplat_fseek() から 0 (指定位置への移動に成功) を返す。
        // [Pre-Assert確認_正常系] - cplat_fseek() が file_、0、SEEK_END を指定して 1 回呼び出されること。
        EXPECT_CALL(mock_cplat_, cplat_ftell(file_)).WillOnce(Return(static_cast<int64_t>(size)));
        // [Pre-Assert手順] - cplat_ftell() から設定本文のバイト数を返す。
        // [Pre-Assert確認_正常系] - cplat_ftell() が file_ を指定して 1 回呼び出されること。
        EXPECT_CALL(mock_cplat_, cplat_fseek(file_, 0, SEEK_SET)).WillOnce(Return(0));
        // [Pre-Assert手順] - cplat_fseek() から 0 (指定位置への移動に成功) を返す。
        // [Pre-Assert確認_正常系] - cplat_fseek() が file_、0、SEEK_SET を指定して 1 回呼び出されること。
        EXPECT_CALL(mock_cplat_, cplat_fread(_, 1u, size, file_, nullptr))
            .WillOnce(
                [content, size](void *buffer, size_t, size_t, FILE *, cplat_error *)
                {
                    std::memcpy(buffer, content, size);
                    return size;
                });
        // [Pre-Assert手順] - cplat_fread() から、設定本文を出力バッファーへコピーして読取バイト数を返す。
        // [Pre-Assert確認_正常系] - cplat_fread() が 1u、size、file_、nullptr を指定して 1 回呼び出されること。
        EXPECT_CALL(mock_cplat_, cplat_fclose(file_, nullptr)).WillOnce(Return(0));
        // [Pre-Assert手順] - cplat_fclose() から 0 (設定ファイルのクローズに成功) を返す。
        // [Pre-Assert確認_正常系] - cplat_fclose() が file_、nullptr を指定して 1 回呼び出されること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=symLoaderInitTest.expect_missing_file]
    void expect_missing_file(const char *path)
    {
        // 指定パスの fopen を失敗させる。
        EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq(path), StrEq("rb"), nullptr)).WillOnce(Return(nullptr));
        // [Pre-Assert手順] - cplat_fopen() から nullptr (設定ファイルを開けない) を返す。
        // [Pre-Assert確認_正常系] - cplat_fopen() が path と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
    }
    // [サブ手順終了]
};

// コメントと末尾カンマを含む JSONC が解析されることの確認
TEST_F(symLoaderInitTest, applies_json_with_comments)
{
    // Arrange
    const char *json = "// header comment\n"
                       "{\n"
                       "  /* block comment */\n"
                       "  \"sample_func\": { // trailing line comment\n"
                       "    \"lib\": \"liboverride\",\n"
                       "    \"func\": \"override_func\",\n"
                       "  },\n"
                       "}\n";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {
        &entry}; // [状態] - コメントと末尾カンマを含む JSONC と sample_func エントリを用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("with_comments.json",
                       json); // コメントと末尾カンマを含む JSONC の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、コメントと末尾カンマを含む JSONC 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "with_comments.json"); // [手順] - コメントと末尾カンマを含む JSONC 設定を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("liboverride", entry.lib_name);    // [確認_正常系] - entry.lib_name が "liboverride" であること。
    EXPECT_STREQ("override_func", entry.func_name); // [確認_正常系] - entry.func_name が "override_func" であること。
}

// JSON 設定の lib / func が一致エントリへ反映されることの確認
TEST_F(symLoaderInitTest, applies_matching_func_key)
{
    // Arrange
    const char *json = "{\"sample_func\":{\"lib\":\"liboverride\",\"func\":\"override_func\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - sample_func エントリを 1 件用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("apply_matching.json",
                       json); // 一致キー JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、一致キー JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(entries, 1u,
                          "apply_matching.json"); // [手順] - JSON 設定を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("liboverride", entry.lib_name);    // [確認_正常系] - entry.lib_name が "liboverride" であること。
    EXPECT_STREQ("override_func", entry.func_name); // [確認_正常系] - entry.func_name が "override_func" であること。
}

// 明示的既定値 (lib/func がともに default) が反映されることの確認
TEST_F(symLoaderInitTest, applies_explicit_default)
{
    // Arrange
    const char *json = "{\"sample_func\":{\"lib\":\"default\",\"func\":\"default\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - sample_func エントリを 1 件用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("explicit_default.json",
                       json); // 明示的既定値 JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、明示的既定値 JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "explicit_default.json"); // [手順] - 明示的既定値の JSON を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("default", entry.lib_name);  // [確認_正常系] - entry.lib_name が "default" であること。
    EXPECT_STREQ("default", entry.func_name); // [確認_正常系] - entry.func_name が "default" であること。
}

// 設定ファイルが存在しない場合にエントリが未設定のままであることの確認
TEST_F(symLoaderInitTest, ignores_missing_file)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 設定ファイルを置かない。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_missing_file]
    expect_missing_file("missing.json"); // 存在しない設定の fopen が呼び出されること。
                                         // [Pre-Assert手順] - cplat_fopen() から nullptr を返す。

    // Act
    cplat_sym_loader_init(entries, 1u,
                          "missing.json"); // [手順] - 存在しないパスを指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("", entry.lib_name);  // [確認_正常系] - entry.lib_name が "" であること。(空のままであることの確認)
    EXPECT_STREQ("", entry.func_name); // [確認_正常系] - entry.func_name が "" であること。(空のままであることの確認)
}

// 不正な JSON を無視してエントリが未設定のままであることの確認
TEST_F(symLoaderInitTest, ignores_invalid_json)
{
    // Arrange
    const char *json = "{not-json";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 不正 JSON の設定を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("invalid.json", json); // 不正 JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、不正 JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(entries, 1u,
                          "invalid.json"); // [手順] - 不正 JSON を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("", entry.lib_name);  // [確認_正常系] - entry.lib_name が "" であること。(空のままであることの確認)
    EXPECT_STREQ("", entry.func_name); // [確認_正常系] - entry.func_name が "" であること。(空のままであることの確認)
}

// ルートが object でない場合にエントリが未設定のままであることの確認
TEST_F(symLoaderInitTest, ignores_non_object_root)
{
    // Arrange
    const char *json = "[{\"lib\":\"x\",\"func\":\"y\"}]";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 配列ルートの JSON を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("array_root.json", json); // 配列ルート JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、配列ルート JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u, "array_root.json"); // [手順] - 配列ルートの JSON を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("", entry.lib_name);  // [確認_正常系] - entry.lib_name が "" であること。(空のままであることの確認)
    EXPECT_STREQ("", entry.func_name); // [確認_正常系] - entry.func_name が "" であること。(空のままであることの確認)
}

// 必須フィールド欠落のエントリを無視することの確認
TEST_F(symLoaderInitTest, ignores_missing_required_fields)
{
    // Arrange
    const char *json = "{\"sample_func\":{\"lib\":\"liboverride\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - func 欠落の JSON を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("missing_fields.json",
                       json); // 必須フィールド欠落 JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、func 欠落の JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "missing_fields.json"); // [手順] - 必須フィールド欠落の JSON を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("", entry.lib_name);  // [確認_正常系] - entry.lib_name が "" であること。(空のままであることの確認)
    EXPECT_STREQ("", entry.func_name); // [確認_正常系] - entry.func_name が "" であること。(空のままであることの確認)
}

// 未知の func_key を無視し、一致キーだけ適用することの確認
TEST_F(symLoaderInitTest, ignores_unknown_func_key_and_applies_known)
{
    // Arrange
    const char *json = "{\"unknown_func\":{\"lib\":\"libx\",\"func\":\"fx\"},"
                       "\"sample_func\":{\"lib\":\"liboverride\",\"func\":\"override_func\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 既知キーと未知キーを含む JSON を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("partial.json", json); // 複数キー JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、既知キーと未知キーを含む JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(entries, 1u,
                          "partial.json"); // [手順] - 複数キーの JSON を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("liboverride", entry.lib_name);    // [確認_正常系] - entry.lib_name が "liboverride" であること。
    EXPECT_STREQ("override_func", entry.func_name); // [確認_正常系] - entry.func_name が "override_func" であること。
}

// 名称長超過のエントリを無視することの確認
TEST_F(symLoaderInitTest, ignores_name_too_long)
{
    // Arrange
    std::string long_name(CPLAT_SYM_LOADER_NAME_MAX, 'a'); // 終端込み MAX バイト分の文字
    std::string json = std::string("{\"sample_func\":{\"lib\":\"") + long_name + "\",\"func\":\"override_func\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - lib が上限超過の JSON を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("name_too_long.json",
                       json.c_str()); // 名称長超過 JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、名称長超過 JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "name_too_long.json"); // [手順] - 名称長超過の JSON を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("", entry.lib_name);  // [確認_正常系] - entry.lib_name が "" であること。(空のままであることの確認)
    EXPECT_STREQ("", entry.func_name); // [確認_正常系] - entry.func_name が "" であること。(空のままであることの確認)
}

// 複数エントリへそれぞれ対応する設定が反映されることの確認
TEST_F(symLoaderInitTest, applies_multiple_entries)
{
    // Arrange
    const char *json = "{\"sample_func\":{\"lib\":\"liba\",\"func\":\"fa\"},"
                       "\"other_func\":{\"lib\":\"libb\",\"func\":\"fb\"}}";
    cplat_sym_loader_entry entry_a = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry entry_b = CPLAT_SYM_LOADER_ENTRY_INIT("other_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry_a, &entry_b}; // [状態] - 2 件のエントリを用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("multi.json", json); // 複数エントリ JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、複数エントリ JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 2u, "multi.json"); // [手順] - 複数エントリ向け JSON を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("liba", entry_a.lib_name); // [確認_正常系] - entry_a.lib_name が "liba" であること。
    EXPECT_STREQ("fa", entry_a.func_name);  // [確認_正常系] - entry_a.func_name が "fa" であること。
    EXPECT_STREQ("libb", entry_b.lib_name); // [確認_正常系] - entry_b.lib_name が "libb" であること。
    EXPECT_STREQ("fb", entry_b.func_name);  // [確認_正常系] - entry_b.func_name が "fb" であること。
}

// JSONC 解析の失敗を注入した場合に設定を反映しないことの確認
TEST_F(symLoaderInitTest, ignores_document_when_cjson_parse_fails)
{
    // Arrange
    NiceMock<Mock_cjson> mock_cjson;
    const char *json = "{\"sample_func\":{\"lib\":\"liboverride\",\"func\":\"override_func\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 正常な JSON と未設定の sample_func エントリを用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read 区分=異常系]
    expect_config_read("injected_parse_failure.json",
                       json); // 正常な JSON の読取が呼び出されること。
                              // [Pre-Assert手順] - cplat_fread() から、正常な JSON 本文をコピーして読取バイト数を返す。
    EXPECT_CALL(mock_cjson, cJSON_ParseJSONCWithLength(StrEq(json), std::strlen(json)))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - cJSON_ParseJSONCWithLength() が json と等しい文字列、std::strlen() の戻り値を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - cJSON_ParseJSONCWithLength() から nullptr を返す。
    EXPECT_CALL(
        mock_cjson,
        cJSON_Delete(nullptr)); // [Pre-Assert確認_異常系] - cJSON_Delete() が nullptr を指定して 1 回呼び出されること。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "injected_parse_failure.json"); // [手順] - cJSON_ParseJSONCWithLength() が失敗する状態で cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ("", entry.lib_name);  // [確認_異常系] - entry.lib_name が "" であること。(空のままであることの確認)
    EXPECT_STREQ("", entry.func_name); // [確認_異常系] - entry.func_name が "" であること。(空のままであることの確認)
}

// cJSON の文字列取得が NULL を返した場合に設定を反映しないことの確認
TEST_F(symLoaderInitTest, ignores_entry_when_cjson_string_value_is_null)
{
    // Arrange
    NiceMock<Mock_cjson> mock_cjson;
    const char *json = "{\"sample_func\":{\"lib\":\"liboverride\",\"func\":\"override_func\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 正常な JSON と未設定の sample_func エントリを用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read 区分=異常系]
    expect_config_read("injected_string_failure.json",
                       json); // 正常な JSON の読取が呼び出されること。
                              // [Pre-Assert手順] - cplat_fread() から、正常な JSON 本文をコピーして読取バイト数を返す。
    EXPECT_CALL(mock_cjson, cJSON_GetStringValue(_))
        .WillOnce(Return(nullptr))
        .WillOnce(DoDefault()); // [Pre-Assert確認_異常系] - cJSON_GetStringValue() が 2 回呼び出されること。
    // [Pre-Assert手順] - cJSON_GetStringValue() から 1 回目は nullptr (lib の文字列値の取得失敗) を返し、以降は cJSON の処理へ委譲する。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "injected_string_failure.json"); // [手順] - cJSON_GetStringValue() が lib の文字列値の取得に失敗する状態で cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        entry
            .lib_name); // [確認_異常系] - entry.lib_name が "" であること。(文字列取得失敗時に空のままであることの確認)
    EXPECT_STREQ(
        "",
        entry
            .func_name); // [確認_異常系] - entry.func_name が "" であること。(文字列取得失敗時に空のままであることの確認)
}

// 設定パスが NULL または空文字列の場合に何も行わないことの確認
TEST_F(symLoaderInitTest, ignores_null_or_empty_config_path)
{
    // Arrange
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 未設定のエントリを用意する。

    // Pre-Assert

    // Act
    cplat_sym_loader_init(entries, 1u,
                          nullptr); // [手順] - 設定パスに nullptr を指定して cplat_sym_loader_init() を呼び出す。
    cplat_sym_loader_init(entries, 1u,
                          ""); // [手順] - 空文字列の設定パスを指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        entry
            .lib_name); // [確認_正常系] - entry.lib_name が "" であること。(nullptr または空文字列の設定パスで未設定のままであることの確認)
    EXPECT_STREQ(
        "",
        entry
            .func_name); // [確認_正常系] - entry.func_name が "" であること。(nullptr または空文字列の設定パスで未設定のままであることの確認)
}

// ファイル操作の各失敗時にファイルを閉じて終了することの確認
TEST_F(symLoaderInitTest, closes_file_when_seek_or_size_operation_fails)
{
    // Arrange
    FILE *file = reinterpret_cast<FILE *>(1);
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 擬似ファイルと未設定エントリを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("seek_error"), StrEq("rb"), nullptr))
        .WillOnce(Return(
            file)); // [Pre-Assert確認_異常系] - cplat_fopen() が "seek_error" と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_fopen() から seek_error 用の擬似 FILE を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_END))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_END を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_fseek() から -1 (末尾への移動に失敗) を返す。
    EXPECT_CALL(mock_cplat_, cplat_fclose(file, nullptr))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - cplat_fclose() が file、nullptr を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fclose() から 0 (seek 失敗後のクローズに成功) を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "seek_error"); // [手順] - 末尾への seek が失敗するファイルを指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        entry.lib_name); // [確認_異常系] - entry.lib_name が "" であること。(seek 失敗時に未設定のままであることの確認)

    // Cleanup
    Mock::VerifyAndClearExpectations(&mock_cplat_);

    // Arrange_2

    // Pre-Assert_2
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("size_error"), StrEq("rb"), nullptr))
        .WillOnce(Return(
            file)); // [Pre-Assert確認_異常系] - cplat_fopen() が "size_error" と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_fopen() から size_error 用の擬似 FILE を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_END))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_END を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_fseek() から 0 (サイズ確認の末尾への移動に成功) を返す。
    EXPECT_CALL(mock_cplat_, cplat_ftell(file))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - cplat_ftell() が file を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_ftell() から 0 を返す。
    EXPECT_CALL(mock_cplat_, cplat_fclose(file, nullptr))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - cplat_fclose() が file、nullptr を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fclose() から 0 (サイズ不正後のクローズに成功) を返す。

    // Act_2
    cplat_sym_loader_init(
        entries, 1u, "size_error"); // [手順] - サイズが 0 のファイルを指定して cplat_sym_loader_init() を呼び出す。

    // Assert_2
    EXPECT_STREQ(
        "",
        entry
            .func_name); // [確認_異常系] - entry.func_name が "" であること。(サイズ不正時に未設定のままであることの確認)
}

// 読み込み前の seek、メモリ確保、読み込みの失敗時に後処理することの確認
TEST_F(symLoaderInitTest, releases_resources_when_read_setup_fails)
{
    // Arrange
    FILE *file = reinterpret_cast<FILE *>(1);
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 擬似ファイルと未設定エントリを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("reset_error"), StrEq("rb"), nullptr))
        .WillOnce(Return(
            file)); // [Pre-Assert確認_異常系] - cplat_fopen() が "reset_error" と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_fopen() から reset_error 用の擬似 FILE を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_END))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_END を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_fseek() から 0 (末尾への移動に成功) を返す。
    EXPECT_CALL(mock_cplat_, cplat_ftell(file))
        .WillOnce(Return(4)); // [Pre-Assert確認_異常系] - cplat_ftell() が file を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_ftell() から 4 を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_SET))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_SET を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_fseek() から -1 (読み込み開始位置への移動に失敗) を返す。
    EXPECT_CALL(mock_cplat_, cplat_fclose(file, nullptr))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - cplat_fclose() が file、nullptr を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fclose() から 0 (reset 失敗後のクローズに成功) を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "reset_error"); // [手順] - 読み込み開始位置への seek が失敗するファイルを指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        entry
            .lib_name); // [確認_異常系] - entry.lib_name が "" であること。(読み込み開始位置の seek 失敗時に未設定のままであることの確認)

    // Cleanup
    Mock::VerifyAndClearExpectations(&mock_cplat_);

    // Arrange_2

    // Pre-Assert_2
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("alloc_error"), StrEq("rb"), nullptr))
        .WillOnce(Return(
            file)); // [Pre-Assert確認_異常系] - cplat_fopen() が "alloc_error" と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_fopen() から alloc_error 用の擬似 FILE を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_END))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_END を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_fseek() から 0 (末尾への移動に成功) を返す。
    EXPECT_CALL(mock_cplat_, cplat_ftell(file))
        .WillOnce(Return(4)); // [Pre-Assert確認_異常系] - cplat_ftell() が file を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_ftell() から 4 を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_SET))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_SET を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_fseek() から 0 (先頭への移動に成功) を返す。
    EXPECT_CALL(mock_cplat_, cplat_malloc(5u))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_malloc() が 5u を指定して 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_malloc() から 1 回目は nullptr を返し、以降は製品 (prod) の処理へ委譲する。
    EXPECT_CALL(mock_cplat_, cplat_fclose(file, nullptr))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - cplat_fclose() が file、nullptr を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fclose() から 0 (確保失敗後のクローズに成功) を返す。

    // Act_2
    cplat_sym_loader_init(
        entries, 1u,
        "alloc_error"); // [手順] - 設定バッファーの確保が失敗するファイルを指定して cplat_sym_loader_init() を呼び出す。

    // Assert_2
    EXPECT_STREQ(
        "",
        entry
            .func_name); // [確認_異常系] - entry.func_name が "" であること。(確保失敗時に未設定のままであることの確認)

    // Cleanup
    Mock::VerifyAndClearExpectations(&mock_cplat_);

    // Arrange_3

    // Pre-Assert_3
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("read_error"), StrEq("rb"), nullptr))
        .WillOnce(Return(
            file)); // [Pre-Assert確認_異常系] - cplat_fopen() が "read_error" と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_fopen() から read_error 用の擬似 FILE を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_END))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_END を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_fseek() から 0 (末尾への移動に成功) を返す。
    EXPECT_CALL(mock_cplat_, cplat_ftell(file))
        .WillOnce(Return(4)); // [Pre-Assert確認_異常系] - cplat_ftell() が file を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_ftell() から 4 を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_SET))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_SET を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_fseek() から 0 (先頭への移動に成功) を返す。
    EXPECT_CALL(mock_cplat_, cplat_malloc(5u))
        .Times(AtLeast(1)); // [Pre-Assert確認_異常系] - cplat_malloc() が 5u を指定して 1 回以上呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_fread(_, 1u, 4u, file, nullptr))
        .WillOnce(Return(
            3u)); // [Pre-Assert確認_異常系] - cplat_fread() が 1u、4u、file、nullptr を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_fread() から 3 を返す。
    EXPECT_CALL(mock_cplat_, cplat_fclose(file, nullptr))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - cplat_fclose() が file、nullptr を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fclose() から 0 (読み込み不足後のクローズに成功) を返す。

    // Act_3
    cplat_sym_loader_init(
        entries, 1u,
        "read_error"); // [手順] - 設定内容の読み込みが短くなるファイルを指定して cplat_sym_loader_init() を呼び出す。

    // Assert_3
    EXPECT_STREQ(
        "",
        entry
            .lib_name); // [確認_異常系] - entry.lib_name が "" であること。(読み込み不足時に未設定のままであることの確認)
}

// JSON エントリの型、キー、値の境界条件を無視することの確認
TEST_F(symLoaderInitTest, ignores_invalid_json_entries)
{
    // Arrange
    const char *json = "{\"number\":1,\"\":{\"lib\":\"x\",\"func\":\"y\"},"
                       "\"bad_type\":{\"lib\":1,\"func\":\"y\"},"
                       "\"empty_value\":{\"lib\":\"\",\"func\":\"y\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("number", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry}; // [状態] - 不正な型、空キー、空値を含む JSON を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read 区分=異常系]
    expect_config_read("invalid_entries.json",
                       json); // 不正エントリ JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、不正エントリ JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "invalid_entries.json"); // [手順] - 不正な JSON エントリを含む設定を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        entry
            .lib_name); // [確認_異常系] - entry.lib_name が "" であること。(不正な JSON エントリが反映されないことの確認)
    EXPECT_STREQ(
        "",
        entry
            .func_name); // [確認_異常系] - entry.func_name が "" であること。(不正な JSON エントリが反映されないことの確認)
}

// 配列が NULL の場合に一致設定を読み飛ばすことの確認
TEST_F(symLoaderInitTest, ignores_matching_entry_when_object_array_is_null)
{
    // Arrange
    const char *json =
        "{\"sample_func\":{\"lib\":\"liboverride\",\"func\":\"override_func\"}}"; // [状態] - 一致する設定を含む JSON を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read]
    expect_config_read("null_array.json", json); // 一致設定 JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、一致設定 JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        nullptr, 0u,
        "null_array.json"); // [手順] - エントリ配列に nullptr を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(nullptr 配列の設定読み込みが異常終了しないことの確認)
}

// NULL エントリと func_key NULL の配列要素を読み飛ばすことの確認
TEST_F(symLoaderInitTest, skips_null_cache_entries)
{
    // Arrange
    const char *json = "{\"sample_func\":{\"lib\":\"liboverride\",\"func\":\"override_func\"}}";
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    entry.func_key = nullptr;
    cplat_sym_loader_entry *entries[] = {nullptr,
                                         &entry}; // [状態] - nullptr 要素と func_key が nullptr の要素を用意する。

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read 区分=異常系]
    expect_config_read("null_cache_entries.json",
                       json); // 一致設定 JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、一致設定 JSON 本文をコピーして読取バイト数を返す。

    // Act
    cplat_sym_loader_init(
        entries, 2u,
        "null_cache_entries.json"); // [手順] - 不正なキャッシュ配列を指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        entry
            .lib_name); // [確認_異常系] - entry.lib_name が "" であること。(不正なキャッシュ要素へ設定が反映されないことの確認)
}

// 上限超過の設定ファイルを読み込まないことの確認
TEST_F(symLoaderInitTest, ignores_config_file_larger_than_limit)
{
    // Arrange
    FILE *file = reinterpret_cast<FILE *>(1);
    cplat_sym_loader_entry entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry *entries[] = {&entry};

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_fopen(StrEq("large_file"), StrEq("rb"), nullptr))
        .WillOnce(Return(
            file)); // [Pre-Assert確認_異常系] - cplat_fopen() が "large_file" と等しい文字列、"rb" と等しい文字列、nullptr を指定して 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_fopen() から large_file 用の擬似 FILE を返す。
    EXPECT_CALL(mock_cplat_, cplat_fseek(file, 0, SEEK_END))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - cplat_fseek() が file、0、SEEK_END を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_fseek() から 0 (末尾への移動に成功) を返す。
    EXPECT_CALL(mock_cplat_, cplat_ftell(file))
        .WillOnce(Return(1024 * 1024 +
                         1)); // [Pre-Assert確認_異常系] - cplat_ftell() が file を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_ftell() から上限を 1 バイト超えるサイズを返す。
    EXPECT_CALL(mock_cplat_, cplat_fclose(file, nullptr))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - cplat_fclose() が file、nullptr を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_fclose() から 0 (上限超過後のクローズに成功) を返す。

    // Act
    cplat_sym_loader_init(
        entries, 1u,
        "large_file"); // [手順] - 上限を 1 バイト超える設定ファイルを指定して cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        entry.lib_name); // [確認_異常系] - entry.lib_name が "" であること。(上限超過の設定が反映されないことの確認)
}

// func の NULL、空文字列、上限超過をそれぞれ無視することの確認
TEST_F(symLoaderInitTest, ignores_invalid_function_values)
{
    // Arrange
    NiceMock<Mock_cjson> mock_cjson;
    const char *valid_json = "{\"sample_func\":{\"lib\":\"liboverride\",\"func\":\"override_func\"}}";
    std::string long_func(256u, 'f');
    std::string boundary_json =
        "{\"empty\":{\"lib\":\"lib\",\"func\":\"\"},\"long\":{\"lib\":\"lib\",\"func\":\"" + long_func + "\"}}";
    cplat_sym_loader_entry null_entry = CPLAT_SYM_LOADER_ENTRY_INIT("sample_func", void (*)(void));
    cplat_sym_loader_entry empty_entry = CPLAT_SYM_LOADER_ENTRY_INIT("empty", void (*)(void));
    cplat_sym_loader_entry long_entry = CPLAT_SYM_LOADER_ENTRY_INIT("long", void (*)(void));
    cplat_sym_loader_entry *null_entries[] = {&null_entry};
    cplat_sym_loader_entry *boundary_entries[] = {&empty_entry, &long_entry};

    // Pre-Assert
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read 区分=異常系]
    expect_config_read("null_func.json",
                       valid_json); // func 取得失敗用 JSON の読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、正常な JSON 本文をコピーして読取バイト数を返す。
    EXPECT_CALL(mock_cjson, cJSON_GetStringValue(_))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cJSON_GetStringValue() が 2 回以上呼び出されること。
    // [Pre-Assert手順] - cJSON_GetStringValue() から 1 回目は cJSON の処理へ委譲し、2 回目は nullptr (func の文字列取得失敗) を返し、以降は cJSON の処理へ委譲する。

    // Act
    cplat_sym_loader_init(
        null_entries, 1u,
        "null_func.json"); // [手順] - cJSON_GetStringValue() が func の文字列取得に失敗する状態で cplat_sym_loader_init() を呼び出す。

    // Assert
    EXPECT_STREQ(
        "",
        null_entry
            .func_name); // [確認_異常系] - null_entry.func_name が "" であること。(nullptr の func が反映されないことの確認)

    // Cleanup
    Mock::VerifyAndClearExpectations(&mock_cjson);
    Mock::VerifyAndClearExpectations(&mock_cplat_);

    // Arrange_2

    // Pre-Assert_2
    // [サブ手順参照 名前=symLoaderInitTest.expect_config_read 区分=異常系]
    expect_config_read(
        "invalid_func_values.json",
        boundary_json.c_str()); // 空または上限超過 func の JSON 読取が呼び出されること。
    // [Pre-Assert手順] - cplat_fread() から、境界値 JSON 本文をコピーして読取バイト数を返す。

    // Act_2
    cplat_sym_loader_init(
        boundary_entries, 2u,
        "invalid_func_values.json"); // [手順] - 空または上限超過の func を含む設定を指定して cplat_sym_loader_init() を呼び出す。

    // Assert_2
    EXPECT_STREQ(
        "",
        empty_entry
            .func_name); // [確認_異常系] - empty_entry.func_name が "" であること。(空の func が反映されないことの確認)
    EXPECT_STREQ(
        "",
        long_entry
            .func_name); // [確認_異常系] - long_entry.func_name が "" であること。(上限超過の func が反映されないことの確認)
}
