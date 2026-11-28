# cplat 機能仕様の記載規範

## 概要

本書は、cplat の機能仕様に固有の設定と追加規則を定めます。  
対象は `docs/functional-spec/` 配下の機能仕様です。

記載する内容、記載しない内容、章立て、記載粒度、要件 ID と UUID の運用、下流成果物での参照記法は、[機能仕様の記載規範](../../general/docs/functional-spec-guideline.md) に従います。  
本書は、同規範が app 固有規範に委ねる項目だけを定めます。

## 要件 ID の構成

Table: cplat 要件 ID の構成要素

| 項目 | 値 |
|---|---|
| 要件 ID 接頭辞 | `CPLAT` |
| 参照コメント タグ | `cplat-req` |
| 機能要件表の見出し | `cplat の要件` |

要件 ID は `CPLAT-<CATEGORY>-<TYPE>-NNN` の形式になります。  
例を次に示します。

```text
CPLAT-CLOCK-FUNC-001
CPLAT-HASHTABLE-QUAL-001
```

## カテゴリごとの主語

要件文は、カテゴリに対応する次の主語で始めます。

Table: 機能仕様カテゴリごとの要件文の主語

| カテゴリ | 主語 |
|---|---|
| `ARGPARSER` | cplat のコマンド ライン引数解析機能 |
| `BASE` | cplat の基盤機能 |
| `CLOCK` | cplat の時計・時刻機能 |
| `COMPRESS` | cplat の圧縮機能 |
| `CONSOLE` | cplat のコンソール機能 |
| `CRT` | cplat の C ランタイム抽象機能 |
| `CRYPTO` | cplat の暗号機能 |
| `HASHTABLE` | cplat のハッシュ テーブル機能 |
| `LOCALE` | cplat のロケール機能 |
| `MMAP` | cplat のメモリ マップド ファイル機能 |
| `NET` | cplat のネットワーク機能 |
| `PROMPT` | cplat のプロンプト機能 |
| `REGEX` | cplat の正規表現機能 |
| `RUNTIME` | cplat の実行時支援機能 |
| `SERVICE` | cplat のサービス機能 |
| `STRING_CATALOG` | cplat の文字列カタログ機能 |
| `SYNC` | cplat の同期機能 |
| `TRACE` | cplat のトレース機能 |
| `WIN32` | cplat の Win32 UTF-8 ラッパー機能 |

カテゴリは、`docs/functional-spec/` に配置する機能仕様のファイル名と一対一で対応します。  
機能カテゴリを追加または削除する場合は、この表も同じ変更で更新してください。

## 確認

機能仕様の変更後は、[機能仕様の記載規範](../../general/docs/functional-spec-guideline.md) の「確認」に記載されている項目を確認します。

要件 ID と UUID の形式および参照関係は、ワークスペース ルートで次のコマンドを実行して確認します。  
検査は、機能仕様が配置されているすべての app を対象とします。

```shell
python3 app/general/bin/check_functional_spec.py
```
