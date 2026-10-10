---
short-title: "argparser"
---

# argparser - コマンド ライン引数パーサー

`cplat_argparser` は、コマンド ライン引数 (argc / argv) を解析する汎用パーサーです。  
フラグ、値付きオプション、位置引数を事前に登録し、解析結果を登録時に指定した格納先へ書き込みます。

対応する構文は次のとおりです。

- フラグ: `-v` / `--verbose` (出現回数を格納)
- 値付きオプション: `-o value` / `--option value` / `--option=value`
- 複数回指定できる値付きオプション: 上記構文の繰り返し (出現順に配列へ格納)
- 位置引数: 登録順に割り当て
- 可変長位置引数: 位置引数列の末尾で複数の値を配列へ格納
- 負の位置整数: 次の位置引数が整数型の場合、`-1` などを位置引数として割り当て

次の構文は対応していません。

- 短オプションの連結 (`-abc`)
- 1 つのオプションに複数の値を続ける方式 (`--option value1 value2 value3`)
- `--` 区切り以降を無条件で位置引数扱いにする慣習

宣言は `cplat/argparser/argparser.h` にあります。  
API の詳細な引数説明は同ヘッダーの Doxygen コメントを参照してください。  
本書ではユース ケース別の使い方をまとめます。

本書で扱うのは、プロセス共有のパーサーを暗黙に使用する API (`cplat_argparser_*`) です。  
通常のアプリケーションはこの API だけで完結し、パーサー ハンドルを意識する必要はありません。  
複数のパーサー インスタンスを同時に扱う場合 (主にテスト) は、明示ハンドル API (`cplat_argparser_handle_*`) を解説する [argparser.internal.md](argparser.internal.md) を参照してください。

## 基本フロー

パーサーの利用は、初期化、登録、解析、参照という一連の流れで行います。  
パーサーの実体はプロセス正常終了時に自動解放されるため、明示的な解放処理は不要です。

```c
#include <cplat/argparser/argparser.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    cplat_console_init();
    cplat_argparser_init(argc, argv, "sample program");

    int need_help = 0;
    int count = 1; /* 既定値は解析前に設定する */
    const char *input = NULL;

    cplat_argparser_register_flag("-h", "--help", "ヘルプを表示する", &need_help);
    cplat_argparser_register_option_int("-c", "--count", "N", "繰り返し回数", 0, &count);
    cplat_argparser_register_positional_string("input", "入力ファイル", CPLAT_ARGPARSER_REQUIRED, &input);

    if (cplat_argparser_get_register_error_count() > 0)
    { /* オプションの登録に失敗した場合 (コーディング エラーの場合) */
        cplat_argparser_print_register_error_messages(stderr);
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    if (need_help != 0)
    {
        /* 必須引数が省略されていても -h, --help を優先する */
        cplat_argparser_print_usage(stdout);
        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        cplat_argparser_print_error_messages(stderr);
        cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    printf("count=%d input=%s\n", count, input);

    return EXIT_SUCCESS;
}
```

`cplat_argparser_init()` には、解析対象の `argc` と `argv` を渡します。  
パーサーは `argv` を複製せずポインターを保持するため、解析結果を参照する間は `argv` を有効なまま維持してください。  
`argv[0]` は usage に表示するプログラム名の既定値として使用します。  
既定値は `argv[0]` のベース名から末尾の `.exe` を除いた名前です。  
Linux と Windows で同じ名前になるよう、大文字・小文字を区別せずに `.exe` を除き、ほかの拡張子は残します。  
プログラム名は `cplat_argparser_get_program_name()` で取得できます。  
`argv[0]` が得られない場合は NULL を返します。

第 3 引数にはプログラムの説明文を設定することができます (不要な場合は NULL)。  
説明文は usage の冒頭に表示されます。  
`cplat_argparser_init()` を呼ばずに register 系 API をいきなり呼び出しても、既定のオプションで暗黙に初期化されます。  
ただしこの場合は解析対象の `argv` を持たないため、`cplat_argparser_parse()` は `CPLAT_ERR_INVALID_ARGUMENT` を返します。

値付きオプションと位置引数の格納先は、コマンド ラインに出現した場合のみ書き込まれます。  
既定値は `cplat_argparser_parse()` を呼ぶ前に呼び出し側で設定してください。

## 登録エラーの確認

register 系 API (`cplat_argparser_register_*()`) は、呼び出しごとの成否確認を省略できるよう結果コードを内部に記録します。  
呼び出しごとの成否確認を省略し、すべての登録を終えた後に `cplat_argparser_get_register_error_count()` でまとめて判定できます。

登録エラーは、同名オプションの二重登録などのコーディング エラーです。  
定型的な報告には `cplat_argparser_print_register_error_messages()` が使用でき、記録されたエラーを発生順にすべて `"error: {メッセージ}\n"` の形式で指定ストリームへ書き出します。  
個別のエラー詳細が必要な場合は `cplat_argparser_get_register_error()` 系の API で取得できます。

## 解析エラー処理の方針

本 API はエラーを標準出力・標準エラーへ出力しません。  
表示するかどうか、どこへ (stdout / stderr / ログ) 出すかは呼び出し側が決定します。

`cplat_argparser_parse()` が解析エラーのコードを返した場合、詳細は次の API で取得します。

- `cplat_argparser_get_error()`: エラー種別 (`int`)
- `cplat_argparser_get_error_target()`: エラーの対象名 (オプション名や位置引数名)
- `cplat_argparser_get_error_index()`: エラーを起こした argv のインデックス (該当なしは -1)
- `cplat_argparser_get_error_message()`: 人間可読の 1 行メッセージを呼び出し側バッファーへ組み立てる

いずれも表示は行わず、文字列や値を返すだけです。  
定型的なエラー表示で足りる場合は、後述の `cplat_argparser_print_error_messages()` が使用できます。

## ユース ケース別の定義例

### フラグ (複数回指定でカウント)

`-v` を複数回指定して詳細度を上げるような使い方です。  
フラグは同一コマンド ラインで複数回指定でき、出現ごとに格納先へ 1 加算されます。

```c
int verbose = 0;

cplat_argparser_register_flag("-v", "--verbose", "詳細出力を有効にする", &verbose);

/* "-v -v --verbose" を解析すると verbose == 3 */
```

`--verbose=1` のように値を指定するとエラー (`CPLAT_ERR_UNEXPECTED_VALUE`) になります。

### 必須の値付きオプション (int)

`CPLAT_ARGPARSER_REQUIRED` を指定すると、1 回も出現しない場合に `CPLAT_ERR_MISSING_REQUIRED` で解析が失敗します。

```c
int port = 0;

cplat_argparser_register_option_int("-p", "--port", "PORT", "待ち受けポート番号",
                                       CPLAT_ARGPARSER_REQUIRED, &port);
```

同一オプションを 2 回指定した場合は `CPLAT_ERR_DUPLICATE_OPTION` になります。  
複数回の指定を許可したい場合は、後述の配列版オプションを使用してください。

### 文字列オプションと値の寿命

文字列オプションの格納先には、argv 内の文字列がそのまま格納されます (コピーしません)。  
格納した文字列の寿命は argv の寿命に従うため、argv が有効な間だけ参照してください。

```c
const char *name = NULL;

cplat_argparser_register_option_string("-n", "--name", "NAME", "表示名", 0, &name);

/* "--name=alice" を解析すると name は argv 内の "alice" 部分を指す */
```

### 複数回指定できるオプション

同じオプションを複数回指定して値を積み上げたい場合は、配列版の登録関数を使用します。  
呼び出し側は格納先の配列 (`storage`)、その要素数 (`capacity`)、出現数の格納先 (`count`) を渡します。

```c
#define INCLUDE_MAX 8

const char *includes[INCLUDE_MAX];
size_t include_count = 0;

cplat_argparser_register_option_string_array("-i", "--include", "DIR", "インクルード ディレクトリ", 0,
                                                includes, INCLUDE_MAX, &include_count);

/* "-i dir1 --include=dir2" を解析すると
   include_count == 2, includes[0] == "dir1", includes[1] == "dir2" */
```

`capacity` を超える出現は `CPLAT_ERR_TOO_MANY_OCCURRENCES` になります。  
int 値の複数回指定には `cplat_argparser_register_option_int_array()` を使用します。

```c
int ports[4];
size_t port_count = 0;

cplat_argparser_register_option_int_array("-p", "--port", "PORT", "待ち受けポート番号",
                                             CPLAT_ARGPARSER_REQUIRED, ports, 4, &port_count);

/* REQUIRED を付けた場合、1 回も出現しないと MISSING_REQUIRED になる */
```

### 位置引数 (必須と任意)

位置引数はオプションとして解釈されなかったトークンを、登録順に割り当てます。

```c
const char *input = NULL;
const char *output = NULL;

cplat_argparser_register_positional_string("input", "入力ファイル", CPLAT_ARGPARSER_REQUIRED, &input);
cplat_argparser_register_positional_string("output", "出力ファイル", 0, &output);

/* "in.txt" だけを渡すと input == "in.txt"、output は未変更 (既定値のまま) */
```

任意 (`CPLAT_ARGPARSER_REQUIRED` なし) の位置引数を登録した後に、必須の位置引数を登録することはできません (登録エラーになります)。  
割り当てが曖昧になるためです。  
必須の位置引数は先に登録してください。

登録数を超える位置引数トークンが出現した場合は `CPLAT_ERR_TOO_MANY_ARGUMENTS` になります。

### 可変長位置引数

残りの位置引数を同じ用途の配列へ格納する場合は、可変長位置引数を使用します。  
呼び出し側は格納先の配列 (`storage`)、その要素数 (`capacity`)、出現数の格納先 (`count`) を渡します。

```c
#define FILE_MAX 8

const char *files[FILE_MAX];
size_t file_count = 0;

cplat_argparser_register_positional_string_array("files", "処理するファイル", CPLAT_ARGPARSER_REQUIRED,
                                                    files, FILE_MAX, &file_count);

/* "a.txt b.txt" を解析すると
   file_count == 2, files[0] == "a.txt", files[1] == "b.txt" */
```

可変長位置引数は位置引数列の末尾に 1 件だけ登録できます。  
可変長位置引数の後に別の位置引数を登録すると、登録エラーになります。  
値付きオプションやフラグは可変長位置引数の後から登録でき、コマンド ライン上でも位置引数の途中に指定できます。

`CPLAT_ARGPARSER_REQUIRED` を指定した場合は 1 個以上、指定しない場合は 0 個以上の値を受け取ります。  
`capacity` を超える値は `CPLAT_ERR_TOO_MANY_ARGUMENTS` になります。  
int 値には `cplat_argparser_register_positional_int_array()` を使用します。  
文字列配列の要素は argv 内の文字列を指し、文字列の寿命は argv に従います。

### ヘルプ表示との組み合わせ

`--help` のようなフラグと `cplat_argparser_print_usage()` を組み合わせると、必須引数が省略されていてもヘルプを優先して表示できます。

```c
int need_help = 0;

cplat_argparser_register_flag("-h", "--help", "ヘルプを表示する", &need_help);

int result = cplat_argparser_parse();

if (need_help != 0)
{
    cplat_argparser_print_usage(stdout);
    return EXIT_SUCCESS;
}

if (result != CPLAT_OK)
{
    /* 通常のエラー処理 (次節を参照) */
}
```

`cplat_argparser_print_usage()` は、内部で必要サイズを問い合わせてから usage 文字列を組み立て、指定したストリームへ書き出す簡易関数です。  
固定長バッファーによる切り詰めは発生しません。  
解析の成否とは独立に、登録が完了していればいつでも呼び出せます。

### エラー時のメッセージと usage をまとめて表示する

解析に失敗した場合の定型的なエラー表示は、`cplat_argparser_print_error_messages()` と `cplat_argparser_print_usage()` の組み合わせでまとめられます。

```c
if (cplat_argparser_parse() != CPLAT_OK)
{
    cplat_argparser_print_error_messages(stderr);
    cplat_argparser_print_usage(stderr);
    return EXIT_FAILURE;
}
```

`cplat_argparser_print_error_messages()` は、内部でエラー メッセージを組み立て、`"error: {メッセージ}\n"` の形式で書き出したあと、区切りの空行を出力します。  
エラーがない場合や対象がない場合は何も出力しません。

### usage を文字列として組み立てる場合

出力先ストリームへ直接書き出すのではなく、usage を文字列として自前のバッファーで扱いたい場合は、`cplat_argparser_get_usage()` を使用します。

必要なバイト数を事前に知りたい場合は、`buffer` に `NULL` を渡して `required_size` だけを問い合わせられます。

```c
size_t required_size = 0;

cplat_argparser_get_usage(NULL, 0, &required_size);
/* required_size バイト分のバッファーを確保してから再度呼び出す */
```

### 再解析

`cplat_argparser_parse()` は、同じ `argc` と `argv` に対して繰り返し呼び出すことができます。

呼び出しの開始時に、フラグの格納先を 0 に、複数値オプションと可変長位置引数の出現数を 0 に初期化し、前回のエラー状態をクリアします。  
値付きオプションと位置引数の格納先は出現時のみ上書きされるため、2 回目の解析前に必要であれば呼び出し側で既定値を設定し直してください。

解析対象の引数を差し替える場合は、`cplat_argparser_init()` から呼び出し直します。  
対話的に複数回コマンド ラインを受け付ける場合など、同一プロセスで `main` 相当の処理を繰り返す場合がこれに当たります。  
`cplat_argparser_init()` は登録済みのオプションと解析結果をすべて捨てるため、オプションの登録もやり直してください。

```c
cplat_argparser_init(argc1, argv1, "sample program");
register_options(&options); /* オプションを登録する */
cplat_argparser_parse();
/* ... 1 回目の結果を利用 ... */

cplat_argparser_init(argc2, argv2, "sample program");
register_options(&options); /* 再初期化で登録が捨てられるため、登録し直す */
cplat_argparser_parse();
/* ... 2 回目の結果を利用 ... */
```

## 参考実装

全種別 (フラグ、必須/任意オプション、複数回指定オプション、位置引数) を組み合わせた実例は、サンプル コマンド `argparser-sample` にあります。

- `app/cplat/prod/src/cmd/argparser-sample/argparser-sample.c`
- コマンドの概要は同ディレクトリの [README.md](../../../src/cmd/argparser-sample/README.md) を参照してください

サンプルでは、解析結果の格納先を `argparser_sample_options` 構造体に集約し、登録処理を `register_argparser()` という別関数に分離しています。  
これは登録内容が多いコマンドで構成を分かりやすくするための一例であり、必須の作法ではありません。  
軽量なプログラムでは、本書の各例のように main 内のローカル変数を格納先として直接登録すれば十分です。

## 複数インスタンスを扱う場合 (主にテスト)

プロセス共有のパーサーはプロセス内で 1 つだけです。  
テストでの独立性検証など、複数のパーサー インスタンスを同時に扱う必要がある場合は、明示ハンドル API (`cplat_argparser_handle_create()` / `cplat_argparser_handle_dispose()` など) を使用します。  
詳細は [argparser.internal.md](argparser.internal.md) を参照してください。
