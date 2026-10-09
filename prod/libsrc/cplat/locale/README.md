---
short-title: "locale"
---

# locale - 表示言語の取得

`locale` は、実行環境が示す表示言語を取得するモジュールです。  
表示言語は、画面やメッセージの表示に使用する言語であり、日付や数値の書式に使用する地域設定とは別に扱います。

宣言は `cplat/locale/ui_language.h` にあります。API の詳細は同ヘッダーの Doxygen コメントを参照してください。  
要件と外部から観測できる振る舞いは [ロケールの機能仕様](https://github.com/Hondarer/app_cplat/blob/main/docs/functional-spec/locale.md) が正本です。

## 構成

- `cplat_ui_language_get_tag`: 実行環境が示す表示言語を言語タグで取得します。
- `CPLAT_UI_LANGUAGE_TAG_MAX`: 言語タグ格納用の推奨配列サイズ (NUL 終端込み)

## 決定の順序

環境変数、OS の設定、ニュートラルの順に評価します。

```text
LC_ALL -> LC_MESSAGES -> LANG -> (Windows のみ) OS の表示言語 -> ニュートラル
```

環境変数の順序は POSIX の規則に合わせています。値が空文字列の環境変数は未設定として扱います。  
指定が `C` または `POSIX` の場合はニュートラルとして確定し、言語タグとして解釈できない指定は次の候補へ進みます。

環境変数は Windows でも優先します。同じ指定に対して両プラットフォームが同じ言語タグを返します。  
Linux では、システム設定がログイン時に環境変数へ反映されるため、環境変数以外の候補を評価しません。

Windows では、表示言語の優先順位を返す `GetUserPreferredUILanguages` を使用し、取得できない場合に `GetUserDefaultLocaleName` を使用します。地域設定は表示言語と別に設定できるため、第一の候補にしません。

## 言語タグの表記

言語は小文字、表記体系は先頭だけ大文字、地域は大文字とし、区別の間を 1 文字のハイフンで区切ります。  
`ja`、`ja-JP`、`zh-TW`、`zh-Hans-CN` のような表記になります。ニュートラルは空文字列で表します。

正規化は `ui_language_tag.c` の `cplat_internal_ui_language_normalize` に集約し、環境変数の値と OS が返す名前へ同じ規則を適用します。  
表記体系と地域より後ろの区別は、表示する文言の選択に使用しないため取り込みません。

## 設計の要点

- 結果を保持しません。呼び出しのたびに実行環境を評価します。プロセス内で同じ値を使用する場合は、利用側で保持してください。
- プロセスのロケール設定を変更しません。`setlocale` は呼び出し元の状態を書き換え、ロケールが導入されていない環境では失敗します。
- 表示言語を決定できないことは失敗として扱わず、ニュートラルを表す空文字列を返します。

## 利用例

文字列カタログの出力言語は、設定していない場合に本モジュールの結果から決定します。  
利用側が言語を明示的に選択する場合は、言語タグを扱う対応付けを使用します。

```c
#include <cplat/locale/ui_language.h>
#include <cplat/string_catalog/string_catalog.h>

char tag[CPLAT_UI_LANGUAGE_TAG_MAX];
cplat_string_catalog_language language = CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL;

if (cplat_ui_language_get_tag(tag, sizeof(tag)) == CPLAT_OK)
{
    (void)cplat_string_catalog_language_from_tag(tag, &language);
    (void)cplat_string_catalog_set_language(language);
}
```

## 実行環境での確認

`prod/src/cmd/show-ui-language` は、取得した言語タグを標準出力へ 1 行で出力するコマンドです。  
ニュートラルの場合は空行を出力します。環境変数を変えたときの結果を確認する用途に使用できます。

```sh
LANG=ja_JP.UTF-8 ./prod/cbin/show-ui-language
```

## 関連ヘッダー

- `cplat/locale/ui_language.h`: 表示言語の取得
- `cplat/string_catalog/string_catalog.h`: 言語タグから出力言語への対応付け
