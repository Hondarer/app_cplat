# cplat hashtable バイナリ キーの有効長 検討記録

## 背景

利用者のキー バッファーが key_size より短い場合も、呼び出し側で key_size まで 0 埋めせずに扱いたい。

## 決定事項

| 論点 | 決定 | 理由 |
|---|---|---|
| 一致判定 | 有効長をエントリに格納し、有効長と有効長までの内容がともに等しいときに一致とする | 0 埋めだけでは末尾の 0 だけが異なるキーを区別できない |
| 形式の名前 | `CPLAT_HASHTABLE_FIELD_FIXED_BINARY` を `CPLAT_HASHTABLE_FIELD_BINARY` へ改名し、形式の値は増やさない | key_size を既定かつ最大の長さとするバイナリと定義する |
| 値側 | 値は改名のみで、常に value_size バイト | 今回の対象はキーに限る |
| 公開関数 | 既存関数は有効長 = key_size とし、`_with_key_length` 版を 11 個併設する | 既存の呼び出し元を変更しない。cplat の `_with_<追加するもの>` の命名例に合わせる |
| 文字列キーの表での有効長版 | `CPLAT_ERR_INVALID_ARGUMENT` | NUL で終わらない文字列の受け付けは別機能 |
| 有効長の取得 | `cplat_hashtable_get_key_ref` に `key_length_out` (NULL 可) を追加する。`get_key_copy` の必要量は有効長 | |
| 有効長の欄 | `uint32_t` を status 直後の詰め物 (offset 12) に置く。key_size は UINT32_MAX 以下 | SCOPE_RECORD ではエントリが増えない。SCOPE_TABLE では key_size が 8 の倍数なら増えない |
| 永続化形式 | 版を 4 から 5 へ上げ、破壊的に移行する | ユーザーが破壊的移行を許可 |
| 有効長の外 | 格納時に 0 埋めし、attach 時の検査で 0 であることを確かめる | 永続化した領域の内容を有効長だけで決める |

## 性能の見込み

- ハッシュと比較は有効長までしか処理しないため、短いキーでは従来より速い。
- 既存関数 (有効長 = key_size) では、比較ごとに 32 ビット整数の比較が 1 回増えるだけである。
- エントリ サイズの増加は、SCOPE_TABLE で key_size が 8 の倍数でない場合の 8 バイトだけである。

## 進捗

- [x] 公開ヘッダー、内部実装、mock_cplat、exportTest、api-cheatsheet、機能仕様
- [x] 単体テスト `hashtableKeyLengthTest.cc` の追加
- [x] app/cplat の make と make test
- [x] app/struct-meta の再ビルドと test (永続化イメージの再生成)
