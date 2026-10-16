# cplat

`cplat` は C プロジェクト向けの汎用ユーティリティ群を提供するライブラリです。  
トレース、同期、コンソール入出力、文字列処理、時計、ファイル操作、ハッシュ テーブルなど、複数プロジェクトで再利用できる共通処理をまとめています。  
Linux / Windows 両プラットフォームでの利用を想定しています。

`cplat` は `c-platform` の略です。

> [!NOTE]
> このリポジトリは単独で動作するライブラリですが、通常は他のサブモジュールと組み合わせて利用することを想定しています。  
> [c-modernization-kit](https://github.com/Hondarer/c-modernization-kit) に統合された利用例があります。  
> `c-modernization-kit` リポジトリ内の `app/cplat` サブモジュールの統合例を参照してください。

## 主な構成

| 機能 | 詳細 |
|---|---|
| トレース | レベル別トレース出力と複数バックエンド |
| 同期 | Linux / Windows 共通の同期プリミティブ |
| コンソール / プロンプト | 端末入出力と対話補助 |
| 文字列 / 時刻 / ランタイム | CRT 補助、時計、ランタイム支援 |
| 圧縮 / 暗号 / ファイル | 基盤機能を支えるユーティリティ |
| ネットワーク | IPv4 ソケットの生成、接続、送受信、アドレス解決 |
| ハッシュ テーブル | 固定レコード数と固定ストレージ容量。外部領域への構築と再接続に対応 |
| 文字列カタログ | 文字列キー、引数スキーマ、言語別リソースを分離した文字列の組み立て |
| ロケール | 実行環境が示す表示言語の決定 |
| テスト支援 | テスト用の補助 API とユーティリティ |
| プラットフォーム | Linux、Windows |

Table: cplat の主な機能構成と概要

## クイック スタート

`cplat` をビルドしてテストするには、`make` を利用します。詳細は各サブディレクトリの `makefile` を参照してください。

```sh
# ビルド
make -C app/cplat

# テスト実行
make -C app/cplat test
```

### 使用例

トレース API の利用例を次に示します。標準エラー出力へ INFO レベルのトレースを出力します。

```c
#include <cplat.h>

int main(void)
{
    cplat_tracer *tracer = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);

    if (tracer == NULL)
    {
        return 1;
    }

    cplat_tracer_set_stderr_level(tracer, CPLAT_TRACE_LEVEL_INFO);
    cplat_tracer_start(tracer);

    cplat_tracer_writef(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "timeout=%d", 1000);

    cplat_tracer_stop(tracer);
    cplat_tracer_dispose(&tracer);

    return 0;
}
```

### テスト

```sh
# app/cplat 配下でのテスト
make -C app/cplat test
```

## API 仕様書

### Doxygen

<!-- docsfw の仕上がりパスに対する相対リンク。この Markdown からの相対パスではないことに注意 -->

生成前またはリンク先を参照できない場合は、[Doxygen の生成入口](../prod/README.md) と [doxyfw の生成手順](../../../framework/doxyfw/docs/makefile-usage.md) を参照してください。

- [cplat (public)](../../../doxygen/cplat_public/index.html)
    - [公開 API (cplat)](../../../doxygen/cplat_public/group__CPLAT__PUBLIC__API.html)

### 単一ファイル版

- [cplat (public)](doxybook2_public/README.md)
    - [公開 API (cplat)](doxybook2_public/Modules/group__CPLAT__PUBLIC__API.md)

## モジュール仕様書

### Doxygen

<!-- docsfw の仕上がりパスに対する相対リンク。この Markdown からの相対パスではないことに注意 -->
- [cplat (internal)](../../../doxygen/cplat_internal/index.html)
    - [ファイルの一覧](../../../doxygen/cplat_internal/files.html)
    - [hashtable 可変長ストレージの管理方式](../../../doxygen/cplat_internal/md_libsrc_2cplat_2hashtable_2hashtable-storage-allocator.html)

### 単一ファイル版

- [cplat (internal)](doxybook2_internal/README.md)
    - [ファイルの一覧](doxybook2_internal/Files/README.md)
    - [カテゴリの一覧](doxybook2_internal/Modules/README.md)
    - [hashtable 可変長ストレージの管理方式](doxybook2_internal/Files/libsrc/cplat/hashtable/hashtable-storage-allocator.md)

## 関連ドキュメント

cplat が満たす要件と提供機能は、[機能仕様](functional-spec/README.md) を入口として参照してください。  
cplat が公開する API 全体の一覧は [cplat API チート シート](api-cheatsheet.md) を入口として参照してください。

### 機能仕様

- [機能仕様の入口](functional-spec/README.md) - 利用側または上位設計から見た cplat の要件と振る舞いを、機能カテゴリごとに説明します。

### 規範

- [機能仕様の記載規範](../../general/docs/functional-spec-guideline.md)
- [cplat 機能仕様の記載規範](functional-spec-guideline.md)
- [cplat コーディング規範](coding-guideline.md)
- [プラットフォーム抽象化ガイドライン](platform-abstraction-guideline.md)
- [mock_cplat の実装規則](mock-cplat-guideline.md)
- [リンク方式の規約](link-policy.md)

### 詳細仕様と測定結果

- [メモリ ロック API](memory-lock.md)
- [ファイル入出力 API の選定基準](fileio-api-selection-guideline.md)
- [hashtable 可変長ストレージの管理方式](doxybook2_internal/Files/libsrc/cplat/hashtable/hashtable-storage-allocator.md)
- [プロセス間 RW ロックの提案](proposals/interprocess-rwlock-shared-table.md)
- [文字列カタログの条件式フィルターの提案](proposals/string-catalog-filter-design.md)
- [トレース種別の文字列カタログの提案](proposals/string-catalog-trace-kind-design.md)
- [表示言語の取得の設計](proposals/ui-language-design.md)

## 文書一覧

\toc depth=-1 exclude-basedir=true exclude="doxybook2_public/*" exclude="doxybook2_internal/*"
