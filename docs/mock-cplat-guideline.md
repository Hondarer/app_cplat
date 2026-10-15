# mock_cplat の実装規則

## 対象

この文書は、`app/cplat/test/` の `mock_cplat` を実装または変更するときの規則を示します。  
app 向け mock の一般的な配置と仕組みは [テスト チュートリアル](../../general/docs/testing-tutorial.md) と [testfw の mock](../../../framework/testfw/docs/how-to-mock.md) を参照してください。

## 既定動作

すべての関数に `delegate_real_<func>` を用意します。  
`mock_cplat` が未注入の場合は、実 `libcplat` へ委譲します。  
mock 注入後の `ON_CALL` も、既定では real delegate を呼び出します。

実シンボルは `resolveSharedSymbolOrExit()` で解決します。  
ライブラリを開けない場合やシンボルが見つからない場合は、理由を標準エラーへ出力して終了します。

## 実装箇所

- **宣言とリンク指定**: `test/include/mock_cplat.h` (`MOCK_CPLAT_LINK_IMPL`)
- **Mock クラスと既定委譲**: `test/libsrc/mock_cplat/mock_cplat.cc` (`ON_CALL`)
- **関数ラッパー**: `test/libsrc/mock_cplat/<module>/mock_cplat_<func>.cc`

関数ラッパーには `MOCK_WEAK_IMPL` を使用します。  
`WEAK_ATR` を関数へ直接付与しません。

委譲先の戻り値を受けて返す一時受けは、型を問わず `mock_ret` とします。  
`ptr`、`fp`、`handle` などの意味名や、テスト本体用の `actual_ret` は使いません。  
詳細は [How to mock](../../../framework/testfw/docs/how-to-mock.md) の「試験側の戻り値中継」を正とします。

## Windows のリンク保持

MSVC では、弱リンク実装を含むオブジェクトが静的ライブラリから取り込まれない場合があります。  
real delegate から直接参照されない関数は、`mock_cplat.h` の `MOCK_CPLAT_LINK_IMPL(func)` に追加します。

`MOCK_CPLAT_LINK_IMPL` は `/INCLUDE:_mock_impl_<func>` を生成します。  
`#pragma` を個別の `.cc` へ重複して記載しません。

## 可変長引数

可変長引数関数の `ON_CALL` は、対応する `va_list` 版へ委譲します。  
可変長引数を Google Mock のメソッドへ直接渡しません。

## 変更時の確認

- 公開 API の追加または削除に合わせ、宣言、`MOCK_METHOD`、`ON_CALL`、関数ラッパーを漏れなく更新したか確認します。
- real delegate と mock ラッパーのシグネチャが公開ヘッダーと一致することを確認します。
- 必要な `MOCK_CPLAT_LINK_IMPL` がヘッダーに登録されていることを確認します。
- `app/cplat` の局所テストを実行し、テストが成功することを確認します。
