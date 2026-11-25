---
short-title: "string-catalog-filter-required-size"
---

# string-catalog-filter-required-size - 条件式フィルターのソース領域の大きさの表示

`string-catalog-filter-required-size` は、条件式フィルターの行数の上限と行幅から、ソース領域とフィルター オブジェクトに必要なバイト数を表示するコマンドです。  
共有メモリやファイルなど、ソース領域を利用側で確保する際の大きさの見積もりに使います。

計算は公開ヘッダー `cplat/string_catalog/filter.h` のマクロ `CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE` と `CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE` と同じです。

## 受け付ける引数

Table: string-catalog-filter-required-size のコマンド ライン引数一覧

| 引数 | 種別 | 説明 |
|---|---|---|
| `-h`, `--help` | フラグ | usage を表示して終了する |
| `-l N`, `--line-capacity N` | 値付きオプション (int、必須) | 条件式の行数の上限。1 以上 1024 以下 |
| `-w N`, `--line-width N` | 値付きオプション (int、必須) | 条件式 1 行のバイト数。8 以上 1024 以下 |

範囲外の値を指定した場合は、エラー メッセージを標準エラー出力へ出力して `EXIT_FAILURE` で終了します。  
フィルター スロットを作成できる範囲と同じです。

## 出力

ソース領域のバイト数とフィルター オブジェクトのバイト数を、この順に空白区切りで 1 行に出力します。

```text
$ ./prod/cbin/string-catalog-filter-required-size --line-capacity 32 --line-width 256
131712 131648
```

ソース領域は、64 バイトのヘッダーとフィルター オブジェクトで構成します。  
フィルター オブジェクトは、64 バイトのヘッダーと、行数の上限の数だけ並ぶ行レコードで構成します。  
行レコードは、16 バイトの見出しと、行幅 1 バイトあたり 16 バイトの命令領域と定数領域です。

## ビルドと実行

`make -C app/cplat` でビルドすると `app/cplat/prod/cbin/string-catalog-filter-required-size` が生成されます。
