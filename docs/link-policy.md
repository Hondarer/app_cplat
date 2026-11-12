# リンク方式の規約

## 概要

`cplat` は動的ライブラリだけを生成します。

| プラットフォーム | 成果物 |
|---|---|
| Linux | `libcplat.so` |
| Windows | `libcplat.dll` / `libcplat.lib` (import library) |

Table: プラットフォームごとの cplat 動的ライブラリ成果物

静的ライブラリ `libcplat_static.a` / `libcplat_static.lib` は生成しません。

`cplat` はプロセス グローバルな状態を持つため、すべての利用者を同じ動的ライブラリへ接続します。

## 利用側の記述

利用側は `makepart.mk` で次のように指定します。

```make
LIBS += cplat
```

Windows の利用側で `CPLAT_STATIC` を定義しないでください。

`CPLAT_STATIC` は、製品ソースやモックの関数をテスト実行ファイルへ直接定義する場合に `dllimport` を抑止する目的でのみ使用します。

`libcplat` は、両 OS で cJSON と zlib を動的に利用します。  
ワークスペースの `app/cjson` と `app/zlib` で生成したライブラリを使用します。

実行時は `libcplat`、`libcjson_app`、`libzlib_app` に加え、Linux では `libcrypto_app.so.3` も解決できるようにします。

Linux の暗号処理と乱数生成は、ワークスペースの `app/openssl` で生成した共有ライブラリ `libcrypto_app.so.3` を使います。  
リンク指定は `LIBS += crypto_app` とし、配布時は `libcplat.so` と同じディレクトリへ同梱します。  
OpenSSL の `shlib_variant` に `_app` を指定し、共有ライブラリ名とシンボル バージョン (`OPENSSL_APP_*`) を OS の OpenSSL と区別します。  
Windows の暗号処理と乱数生成は CNG を使い、OpenSSL をリンクしません。  
ただし、ビルド順序をそろえるため、両 OS で `app/openssl` に依存します (`appdeps.mk`)。  
OpenSSL の配布条件は `app/openssl/README.md` を参照してください。

## 実行ファイルへの同梱

単体配布する CLI や OS のサービスとして起動する実行ファイルには、`libcplat`、`libcjson_app`、`libzlib_app` を同じディレクトリへ同梱します。

Linux では、OpenSSL の `libcrypto_app.so.3` も同梱します。

このリポジトリでは、対象 app の製品ビルドが次の成果物を `prod/cbin` へコピーします。

| プラットフォーム | 同梱するファイル |
|---|---|
| Linux | `libcplat.so` / `libcjson_app.so` / `libzlib_app.so` / `libcrypto_app.so.3` |
| Windows | `libcplat.dll` / `libcjson_app.dll` / `libzlib_app.dll` |

Table: 実行ファイル同梱対象の共有ライブラリ一覧

Linux の対象実行ファイルと `libcplat.so` は、`$ORIGIN` の実行時探索パス (RPATH または RUNPATH) で同じディレクトリを探索します。

Windows は実行ファイルと同じディレクトリの DLL を標準の探索順序で解決します。

この配置により、systemd や Windows SCM から起動する `service-sample` も、開発者の `LD_LIBRARY_PATH` や `PATH` に依存しません。

`libzlib_app` の配布条件は `app/zlib/README.md` を参照してください。

`libcjson_app` を配布物に含める場合は、`app/cjson/README.md` に記載された MIT License の条件に従います。

## 内部 API はエクスポートしない

`libcplat` がエクスポートするのは、`prod/include/` の公開 API だけです。

Linux では `-fvisibility=hidden` でビルドし、`CPLAT_EXPORT` を付けた関数と変数だけが visibility default になります。`prod/libsrc/cplat/exports.map` の `global: cplat_*` は、コンパイル時に hidden となった記号を再公開しません。Windows も同様に、`CPLAT_EXPORT` を付けた記号だけが `libcplat.lib` に現れます。

したがって `prod/include_internal/` で宣言する内部 API (`cplat_internal_result_from_errno`、`cplat_internal_error_report_errno` など) は、ライブラリの利用者からリンクできません。

テストが内部 API に依存する場合は、定義元の `.c` を `makepart.mk` の `ADD_SRCS` へ追加します。テスト対象のソースが間接的に呼び出す場合も同様です。

```make
# テスト対象が依存するソース ファイル
ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/error.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c
```

`ADD_SRCS` へ追加したソースは、ビルド ディレクトリへシンボリック リンクとして取り込まれるため、テスト ディレクトリの `.gitignore` にも同名のファイルを登録します。

エクスポートの有無は `nm` で確認できます。大文字の `T` が公開 API、小文字の `t` が内部 API です。

```bash
nm prod/lib/libcplat.so | grep ' [Tt] cplat_'
```

## 確認方法

Linux では `readelf` と `ldd` を使い、動的リンクと実行時の解決先を確認できます。

```bash
readelf -d <利用側>/prod/cbin/<実行ファイル> | grep -E 'NEEDED|RPATH|RUNPATH'
env -u LD_LIBRARY_PATH ldd <利用側>/prod/cbin/<実行ファイル> | grep -E 'cplat|cjson|zlib|crypto'
```

`libcplat.so`、`libcjson_app.so`、`libzlib_app.so`、`libcrypto_app.so.3` の解決先が、実行ファイルと同じ `prod/cbin` になることを確認します。

OpenSSL の共有ライブラリへの依存と、OpenSSL の関数をエクスポートしていないことは、次のコマンドで確認できます。  
1 行目は `libcrypto_app.so.3`、2 行目は 0、3 行目は `OPENSSL_APP_` 付きの参照を表示します。

```bash
readelf -d prod/lib/libcplat.so | grep -E 'NEEDED.*(libcrypto|libssl)'
nm -D --defined-only prod/lib/libcplat.so | grep -cE ' (EVP|RAND|OPENSSL|CRYPTO)_'
nm -D -u prod/lib/libcplat.so | grep 'OPENSSL_APP_'
```

Windows では `dumpbin /dependents` を使い、実行ファイルが `libcplat.dll` を参照することを確認します。

## 関連ドキュメント

- [cplat API チート シート](api-cheatsheet.md)
- [コーディング規範 (特化事項)](coding-guideline.md)
- [プラットフォーム抽象化ガイドライン](platform-abstraction-guideline.md)
- [makepart.mk の記述](../../../framework/makefw/docs/makeparts.md)
