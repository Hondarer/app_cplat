---
short-title: "argparser-sample"
---

# argparser-sample - cplat_argparser 動作確認コマンド

`argparser-sample` は、`cplat_argparser` の動作確認用サンプル コマンドです。  
パーサーが対応する全種別 (フラグ、値付きオプション、複数値オプション、位置引数) を登録し、解析結果を標準出力へ出力します。  
解析エラー時はエラー メッセージと usage を標準エラー出力へ出力して `EXIT_FAILURE` で終了します。

ライブラリ利用者向けのユース ケース別の解説は、[cplat/argparser/README.md](../../../libsrc/cplat/argparser/README.md) を参照してください。

## 受け付ける引数

Table: argparser-sample のコマンド ライン引数一覧

| 引数 | 種別 | 説明 |
|---|---|---|
| `-h`, `--help` | フラグ | usage を表示して終了する (必須引数の省略より優先) |
| `-v`, `--verbose` | フラグ | 出現回数をカウントする (複数回指定可) |
| `-c N`, `--count N` | 値付きオプション (int) | 繰り返し回数 (既定値 1) |
| `-n NAME`, `--name NAME` | 値付きオプション (文字列) | 表示名 |
| `-i DIR`, `--include DIR` | 複数値オプション (文字列、最大 4 回) | インクルード ディレクトリ |
| `input` | 位置引数 (必須) | 入力ファイル |
| `output` | 位置引数 (任意) | 出力ファイル |

## ビルドと実行

`make -C app/cplat` でビルドすると `app/cplat/prod/cbin/argparser-sample` が生成されます。

```bash
# usage の表示
app/cplat/prod/cbin/argparser-sample --help

# 全種別の指定例
app/cplat/prod/cbin/argparser-sample -v -v -c 3 --name=alice -i dir1 -i dir2 in.txt out.txt

# 解析エラーの例 (必須の位置引数 input を省略)
app/cplat/prod/cbin/argparser-sample -v
```

## コードの構成

`argparser-sample.c` は、暗黙のシングルトン パーサーを使用する API (`cplat_argparser_*`) の参考実装です。

- 解析結果の格納先を `argparser_sample_options` 構造体に集約します。
- `cplat_argparser_init()` は `argc` と `argv` を受け取るため、main で呼び出します。
- 登録処理を `register_argparser()` という別関数に分離し、登録エラーの有無を `cplat_argparser_get_register_error_count()` でまとめて判定します。
- 結果表示を `print_result()` に分離します。

構造体への集約と登録の別関数化は、登録内容が多いコマンドで構成を分かりやすくするための一例であり、必須の作法ではありません。  
軽量なプログラムでは、main 内のローカル変数を格納先として直接登録すれば十分です。
