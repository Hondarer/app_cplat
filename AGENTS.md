# AGENTS.md

## 対象範囲

この文書は、`app/cplat/` 配下の作業に適用します。  
目的や利用方法が必要な場合は [README.md](README.md)、変更する契約や設計は対象の詳細文書の該当節を参照してください。

## 作業時の入口

- `prod/include/` は、利用者向けの公開 API ヘッダーです。
- `prod/include_internal/` は、ライブラリ内部の共有ヘッダーです。
- `prod/libsrc/` は、C の実装です。同一ディレクトリの実装だけが共有する宣言は、モジュール私有ヘッダー (例: `prod/libsrc/cplat/hashtable/hashtable.h`) に配置します。
- `test/` は、単体テスト、モック、エクスポート確認です。
- [docs/README.md](docs/README.md) は、発行文書の入口です。
- [docs/functional-spec/README.md](docs/functional-spec/README.md) は、要件と機能を説明する機能仕様の入口です。
- [docs/functional-spec-guideline.md](docs/functional-spec-guideline.md) は、cplat 固有の要件 ID 接頭辞、参照コメント タグ、カテゴリごとの主語を定めます。記載範囲、構成、要件 ID と UUID の運用は [機能仕様の記載規範](../general/docs/functional-spec-guideline.md) が正本です。
- [docs/api-cheatsheet.md](docs/api-cheatsheet.md) は、公開 API の逆引きです。
- [docs/coding-guideline.md](docs/coding-guideline.md) は、cplat 固有の規範です。

## 公開 API と文書の同期

`prod/include/` の関数、型、定数、マクロ、シグネチャ、またはラッパーの利用方針を変更する場合は、同じ変更で `docs/api-cheatsheet.md` の該当箇所を確認し、コードと文書を常に一致させてください。  
不一致は文書の参考情報ではなく、修正が必要な欠陥として扱ってください。

公開関数を追加、削除、または名称変更する場合は、`test/src/libcplatTest/exportTest/exportTest.cc` の `CPLAT_EXPORT_TABLE_COMMON` と `CPLAT_EXPORT_TABLE_PLATFORM` も同じ変更で確認してください。  
公開関数のシグネチャを変更する場合も、型検査を含むエクスポート テストとの一致を確認してください。

## 機能の増減と機能仕様の同期

`docs/functional-spec/` は、利用側または上位設計から見た cplat の要件と振る舞いを説明する正本です。  
機能仕様は API 設計、実装設計、実装、テストの入力であり、これらの下流成果物を根拠として記載しません。  
入口は [機能仕様](docs/functional-spec/README.md)、記載範囲と粒度は [機能仕様の記載規範](../general/docs/functional-spec-guideline.md) と [cplat 機能仕様の記載規範](docs/functional-spec-guideline.md) を参照してください。

次のいずれかに該当する変更を行う場合は、同じ変更の中で該当する機能仕様を見直してください。

| 変更の内容 | 見直す箇所 |
|---|---|
| 機能の追加 | 「機能要件」への要件 ID と UUID 付きの行の追加と、機能ごとの節の追加 |
| 機能の削除 | 対応する行と節の削除。利用側へ代替を示す必要がある場合は「選択の指針」に追記 |
| 満たす要件の変更 | 「解決する課題」と「機能要件」。要件 ID と UUID の扱いは機能仕様の記載規範に従う |
| 適用外とする範囲の変更 | 「適用範囲外」 |
| 利用側の前提条件の変更 | 「前提と制約」 |
| 方式の選択基準の変更 | 「選択の指針」 |
| プラットフォーム差の変更 | 該当する機能の節 |

Table: cplat 機能変更内容と機能仕様の見直し箇所

利用側から見て独立した目的を持つ機能カテゴリを追加または削除した場合は、`docs/functional-spec/` の Markdown と `docs/functional-spec/README.md` の文書一覧も同じ変更で追加または削除してください。  
実装ディレクトリの追加または削除だけを、機能仕様の追加または削除の根拠にしないでください。

機能仕様には、関数名、型名、引数と戻り値、公開ヘッダー、実装方式、テスト方法を記載しません。  
API、実装、テストなどの下流成果物は、必要な場合に機能仕様の要件 ID と UUID を参照できますが、すべての箇所へ機械的に付与しないでください。

## app 固有の規則

- 一般的な C/C++ 規範は、[共通コーディング規範](../general/docs/coding-guideline.md) に従ってください。
- cplat 固有の結果コード、標準時刻型、制約は、[cplat コーディング規範](docs/coding-guideline.md) に集約してください。
- `mock_cplat` を変更する場合は、この app の `create-mock-cplat-mock` スキルを使用してください。
- `mock_cplat` をリンクするテストは、Windows で実装オブジェクトを取り込むため、テスト翻訳単位で `mock_cplat.h` をインクルードしてください。
- 文字列カタログの責務境界と変更時の制約は、[string_catalog モジュール](prod/libsrc/cplat/string_catalog/README.md) を参照してください。
- カタログ生成器 `bin_internal/string_catalog_gen.py` を変更した場合は、`cd bin_test && python3 -m unittest test_string_catalog_gen` を実行してください。
- 公開関数を追加する場合は、`mock_cplat` にも同じ変更で mock を追加してください。可変長引数の関数は `va_list` 版へ委譲します。
- テスト構成は、[testfw のテスト作成手順](../../framework/testfw/docs/how-to-test.md) に従ってください。
- `bench-io` の測定軸を変更する場合は、`prod/src/cmd/bench-io/benchmark-method.md` と `docs/fileio-api-selection-guideline.md` を同じ変更で確認してください。
- `bench-io` の測定結果は管理対象外です。共有する数値と測定環境は、`docs/fileio-api-selection-guideline.md` に記載してください。
- `bench-io` のレコード型を変更する場合は、パディングの有無と `-Wpadded` の警告を確認してください。
- `bench-hashtable` の測定軸を変更する場合は、`prod/src/cmd/bench-hashtable/benchmark-method.md` と `prod/libsrc/cplat/hashtable/hashtable-storage-allocator.md` を同じ変更で確認してください。
- `bench-hashtable` の測定結果は管理対象外です。共有する数値と測定環境は、`prod/libsrc/cplat/hashtable/hashtable-storage-allocator.md` に記載してください。

## 確認コマンド

```bash
make
make test
```
