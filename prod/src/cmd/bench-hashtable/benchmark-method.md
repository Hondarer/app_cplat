# ベンチマークの測定方法

`bench-hashtable` コマンドが何をどう測っているかと、実行手順をまとめます。  
測定結果の解釈と管理方式は [hashtable 可変長ストレージの管理方式](../../../libsrc/cplat/hashtable/hashtable-storage-allocator.md) を参照してください。

## 測定対象

可変長キーと可変長値を持つハッシュ テーブルです。  
キーと値の双方が可変長ストレージを使うため、1 回の追加で確保が 2 回発生します。

Table: ハッシュ テーブル ベンチマークの測定対象設定

| 設定 | 値 |
|------|-----|
| `key_type` / `value_type` | `CPLAT_HASHTABLE_FIELD_VARIABLE_STRING` |
| `key_storage_size` | `capacity` × 16 バイト |
| `value_storage_size` | `capacity` × 24 バイト |
| `lifetime` | 5 |

`lifetime` は削除済みレコードの寿命です。このベンチマークは加齢の処理 (`cplat_hashtable_push_deleted`) を呼び出さないため、3 以上なら測定結果に影響しません。2 を指定すると、削除したレコードが直ちに空へ戻ります。

キーは `k%08zu`、値は `value-%010zu` で組み立てます。  
いずれも長さが一定のため、更新で確保量が変わりません。  
断片化の有無だけが確保の所要時間へ効くようにするための条件です。

## 測定軸

### 操作

Table: ハッシュ テーブル ベンチマークの測定操作一覧

| ID | 内容 | 1 反復で扱う件数 |
|----|------|------|
| `fill` | 空のテーブルへ `capacity` 件を追加します。 | `capacity` |
| `update` | 満杯のテーブルの全件を、同じ長さの値へ更新します。 | `capacity` |
| `churn` | 1 件おきに回収して空きブロックを作った状態で、同数を追加し直します。 | `capacity` / 2 |
| `compact` | 1 件おきに回収した状態のストレージを圧縮します。 | `capacity` |
| `resize` | `capacity` を 2 倍へ広げます。 | `capacity` |

`fill` は断片化のない状態での確保を測ります。空き領域は常に 1 個のため、確保は実質 O(1) です。  
`churn` は断片化した状態での確保を測ります。空き領域が `capacity` / 2 個ある状態からの先着適合になります。  
`update` は自ブロックの解放と再確保の往復を測ります。  
`resize` は全レコードの書き直しを伴うため、確保の計算量がそのまま効きます。

### capacity

256 から始めて 4 倍ずつ増やし、`--max-capacity` (既定 16384) までを測定します。  
`--max-capacity` に 256 未満を指定すると、エラーで終了します。  
共有する結果表は、既定の 4 点 (256、1024、4096、16384) です ([hashtable 可変長ストレージの管理方式](../../../libsrc/cplat/hashtable/hashtable-storage-allocator.md#性能特性))。  
1 件あたりの所要時間 (`ns/unit`) が capacity によらずほぼ一定なら、確保が capacity に比例していないことを示します。

## 計測の仕組み

各操作はテーブルの状態を壊すため、反復して繰り返すことができません。  
`bench_timer_measure_cold` を使い、試行ごとに前処理でテーブルを所定の状態へ作り直し、1 反復だけを測定します。  
前処理の時間は測定に含みません。

試行回数は 5 回で、中央値を代表値とします。  
反復回数の自動調整は行いません。1 反復の所要時間がクロック分解能に対して十分に長いことを、capacity を上げて確かめてください。

`bench_timer` の実装は `bench-io` と共有しています (`makepart.mk` の `ADD_SRCS`)。

## 実行

```bash
make -C app/cplat
app/cplat/prod/cbin/bench-hashtable
```

capacity の上限を変える場合と、CSV を残す場合は次のとおりです。

```bash
app/cplat/prod/cbin/bench-hashtable --max-capacity 65536 \
    --csv app/cplat/prod/src/cmd/bench-hashtable/measurements/linux.csv
```

## CSV の列

Table: ハッシュ テーブル ベンチマーク CSV の列仕様

| 列 | 内容 |
|----|------|
| `capacity` | スロット数 |
| `scenario` | 操作の ID |
| `median_ns` | 1 反復の所要時間の中央値 (ナノ秒) |
| `min_ns` | 1 反復の所要時間の最小値 (ナノ秒) |
| `max_ns` | 1 反復の所要時間の最大値 (ナノ秒) |
| `ns_per_unit` | 1 件あたりの所要時間 (ナノ秒) |
