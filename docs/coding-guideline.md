# cplat コーディング規範 (特化事項)

## 対応環境

cplat は 64 ビット環境専用です。  
32 ビット環境向けの条件分岐、互換処理、回避策は一切追加しません。

## 概要

本書は、上位の [共通コーディング規範](../../general/docs/coding-guideline.md) の一般則に対して、cplat を利用するコードおよび cplat 自身に適用する特化事項をまとめます。  
章立ては上位文書の章に対応させ、cplat 固有の追記・上書き事項のみを記載します。

cplat 固有の規則、制限、遵守事項は、今後もすべて本書に集約します。

規範本文と、その判断の背景の書き分けは、上位文書の「本書の記述形式」に従います。  
平文が規範本文、`> [!NOTE]` が背景と根拠、`> [!IMPORTANT]` が見落とすと規範違反になる要点、`> [!WARNING]` が誤った書き方の招く具体的な障害です。

### 関連ドキュメント

- [`api-cheatsheet.md`](api-cheatsheet.md) - cplat API チート シート (公開 API 全体の一覧、入り口)
- [`platform-abstraction-guideline.md`](platform-abstraction-guideline.md) - `platform.h` / `compiler.h` の共通マクロ利用規則

## エラー処理と戻り値規約

cplat の公開 API が戻り値として使用する共通結果コードの運用を示します。

### 定義ヘッダー

共通結果コードは `prod/include/cplat/base/result.h` に定義します。

> [!IMPORTANT]
> `result.h` が正であり、本節のコード一覧は参照用の写しです。  
> 両者が食い違う場合は `result.h` に従い、本節を更新してください。

定義は課題別の帯に分けて並べ、各帯に将来の追加用の余白を設けています。

| 帯 | コード | 値 | 意味 |
|---|---|---|---|
| 分類不能 | `CPLAT_OK` | 0 | 成功 |
| 省略 | `CPLAT_SKIPPED` | 1 | 適用せずに省略した (エラーではない) |
| 分類不能 | `CPLAT_ERR_UNKNOWN` | -1 | -2 以下の分類済みコードに該当しないその他のエラー |
| 引数・状態・権限<br> (-2 〜 -9) | `CPLAT_ERR_INVALID_ARGUMENT` | -2 | API 引数が不正 (NULL、負値など) |
| | `CPLAT_ERR_UNSUPPORTED` | -3 | 現在のプラットフォームまたは状態では操作がサポートされない |
| | `CPLAT_ERR_PERMISSION_DENIED` | -4 | 権限不足 |
| | `CPLAT_ERR_DUPLICATE_DEFINITION` | -5 | 同名の定義が登録済み |
| | `CPLAT_ERR_NOT_FOUND` | -6 | 対象が存在しない (ファイル、ディレクトリ、ホスト名など) |
| | `CPLAT_ERR_DUPLICATE_KEY` | -7 | 同一キーがすでに存在する |
| | `CPLAT_ERR_IDENTITY_MISMATCH` | -8 | 期待する対象の識別値と一致しない (定義から求める識別値を含む) |
| | `CPLAT_ERR_VERSION_MISMATCH` | -9 | 対応する形式の版と一致しない |
| リソース・バッファー<br> (-10 〜 -19) | `CPLAT_ERR_OUT_OF_MEMORY` | -10 | メモリを確保できません。 |
| | `CPLAT_ERR_BUSY` | -11 | リソースがビジー状態 |
| | `CPLAT_ERR_TIMEOUT` | -12 | タイムアウト |
| | `CPLAT_ERR_LIMIT_EXCEEDED` | -13 | リソースの上限を超過した |
| | `CPLAT_ERR_BUFFER_TOO_SMALL` | -14 | 出力バッファーが不足している |
| | `CPLAT_ERR_CORRUPT_DESCRIPTOR` | -15 | ディスクリプタが破損している |
| | `CPLAT_ERR_STORAGE_FULL` | -16 | 固定容量ストレージに必要な連続空き領域がない |
| 入力解析<br> (-20 〜 -39) | `CPLAT_ERR_UNKNOWN_OPTION` | -20 | 未登録のオプションが指定された |
| | `CPLAT_ERR_MISSING_VALUE` | -21 | 値を要する項目に値が指定されていない |
| | `CPLAT_ERR_UNEXPECTED_VALUE` | -22 | 値を取らない項目に値が指定された |
| | `CPLAT_ERR_INVALID_INTEGER` | -23 | 整数値として解釈できません。 |
| | `CPLAT_ERR_OUT_OF_RANGE` | -24 | 値が表現可能な範囲を超えている |
| | `CPLAT_ERR_MISSING_REQUIRED` | -25 | 必須の項目が指定されていない |
| | `CPLAT_ERR_DUPLICATE_OPTION` | -26 | 単数指定の項目が複数回指定された |
| | `CPLAT_ERR_TOO_MANY_ARGUMENTS` | -27 | 引数の個数が受入数を超えている |
| | `CPLAT_ERR_TOO_MANY_OCCURRENCES` | -28 | 同一項目の出現回数が容量を超えている |
| | `CPLAT_ERR_INVALID_PATTERN` | -29 | 正規表現パターンの構文が不正です。 |
| | `CPLAT_ERR_INVALID_ENCODING` | -30 | 文字列が UTF-8 として不正です。 |
| 制御<br> (-40 〜) | `CPLAT_ERR_EOF` | -40 | 入力が EOF に達した |
| | `CPLAT_ERR_CANCELED` | -41 | ユーザー操作 (Ctrl+C など) による中断 |

Table: cplat 結果コードの帯分類・数値・意味一覧

`ret <= -20` のような範囲比較で種別を判定せず、個々のコード名との比較を使用します。

各コードの値は ABI として凍結します。  
既存の値の変更は禁止し、コードの追加は該当する帯の余白への追記のみとします。

> [!WARNING]
> 帯は定義の整理と追加位置を示すためのものであり、**範囲判定による分類は API の契約に含めません**。  
> 範囲比較で種別を判定すると、余白へコードを追加した時点で判定結果が変わります。

> [!NOTE]
> 値は `test/src/libcplatTest/base/resultTest/` の `static_assert` で固定しています。  
> 既存の値を変更するとコンパイル時に検出されます。

### 判定慣用句

呼び出し側の成否判定は、コード名との比較を正とします。

```c
int ret;

ret = cplat_mmap_attach(path, access, create_size, &map);
if (ret != CPLAT_OK)
{
    return ret;
}
```

結果コードを受ける変数名と宣言位置は、上位規範に従います。  
特定のエラーを区別する場合は、`ret == CPLAT_ERR_TIMEOUT` のようにコード名で比較します。  
`-1` などの数値リテラルとの比較は行いません。

> [!NOTE]
> 全エラーが負値のため `ret < 0` も等価ですが、名前比較を推奨します。  
> 数値リテラルとの比較は、コードの追加や帯の見直しに対して意図が追随しません。

### 戻り値とエラー詳細の役割分担

戻り値は「分類済みの結果コード」を伝達し、OS 由来の詳細はドメイン付きの `cplat_error` で伝達します。

| 伝達手段 | 内容 |
|---|---|
| 戻り値 (`int`) | `CPLAT_OK`、正値の省略 (`CPLAT_SKIPPED`)、または負値の分類済みエラー コード |
| `cplat_error *detail_out` 出力引数 | OS エラーのドメイン、共通結果コード、生の詳細値 |
| スレッド ローカルの直前値 | `cplat_error_get_last()` で取得する、直前の対応 API と同じ詳細 |

Table: 戻り値とエラー詳細における情報伝達の役割分担

分類済みコードでは失われる詳細 (ともに `CPLAT_ERR_UNKNOWN` へ写像される `ENOSPC` と `EIO` の区別など) が必要な API は、`detail_out` を提供します (`cplat_fopen`、`crt/path.h` の各関数など)。  
対応 API は失敗時に出力引数とスレッド ローカルの直前値へ同じ詳細を記録し、成功時は両方をクリアします。  
`errno`、`GetLastError()`、`HRESULT` などの OS エラー値を、共通結果コードとして直接返しません。

> [!NOTE]
> 伝達手段を 3 つに分けているのは、詳細を必要とする呼び出し元と、分類済みコードだけで足りる呼び出し元の双方を、同じ API で扱えるようにするためです。  
> 詳細が不要な呼び出し元は `detail_out` に `NULL` を渡し、戻り値だけで判定できます。

### 命名

エラー コードの名称は `CPLAT_ERR_*` に統一します。  
`ERROR` という略記は使用しません。

### 詳細分類の扱い

操作結果として呼び出し元の制御フローを分岐させる分類は、`result.h` の `CPLAT_ERR_*` へ追加します。  
`CPLAT_ERR_*` は関数の戻り値として使用できます。

OS エラーの原因をプラットフォーム共通で調べる場合は、`error.h` の `CPLAT_CAUSE_*` を使用します。  
要因コードは `cplat_error_get_cause()` または `cplat_error_is()` による詳細エラーの解釈専用であり、関数の戻り値には使用しません。  
新しい要因を追加する場合は、errno と Win32 エラー コードの両方の対応表を確認します。

`result.h` は、粗い分類 (`CPLAT_ERR_INVALID_ARGUMENT` など) と細かい操作結果 (`CPLAT_ERR_UNKNOWN_OPTION` など) の両方を含みます。  
モジュール固有の戻り値コード体系は別に設けません。  
argparser の `cplat_argparser_parse()` は解析エラーの種別に対応するコードを直接返し、`cplat_argparser_get_error()` はその種別を後から再取得する用途で提供しています。

識別値の不一致は `CPLAT_ERR_IDENTITY_MISMATCH`、署名が一致した同じ種類の形式で、形式版だけが異なる場合は `CPLAT_ERR_VERSION_MISMATCH` で通知します。  
識別値を定義の内容から求める場合 (文字列カタログの識別値など) は、定義の変更も識別値の不一致として `CPLAT_ERR_IDENTITY_MISMATCH` になります。  
署名 (マジック値) は形式を見分けるための値であり、ここでいう識別値には含めません。  
長さ、範囲、内部整合性の不正は、従来どおり `CPLAT_ERR_CORRUPT_DESCRIPTOR` で通知します。  
マジック値だけでは別形式と破損を区別できないため、その不一致は `CPLAT_ERR_CORRUPT_DESCRIPTOR` とします。

### OS エラー詳細の抽象化

OS 由来の詳細は、`cplat_error` にドメイン、対応する共通結果コード、生のエラー値をまとめて保持します。  
公開 API の引数や戻り値で、生の `errno` または `GetLastError()` の値だけを受け渡してはなりません。  
自前の OS 呼び出しで得た値は `cplat_error_capture_errno()` または `cplat_error_capture_windows_error()` で取り込みます。

`detail_out` を持つ API は、失敗時に出力引数と現在のスレッドの直前値へ同じ詳細を記録し、成功時に両方をクリアします。  
`detail_out` へ `NULL` を指定した場合、本引数へはエラー詳細を設定せず、返却しませんが、スレッドの直前値は更新されます。  
OS API の失敗後に後処理を行うアダプターは、先に詳細を保存し、後処理で直前値が変化した場合に `cplat_error_set_last()` で保存値を復元します。  
`cplat_error_set_last()` には有効な保存値だけを指定し、NULL を指定した場合は現在のスレッドの直前値をクリアします。

失敗の原因が OS 呼び出しに由来しない場合 (引数の検証エラー、パターンの構文エラーなど) は、`cplat_error` に格納すべき詳細が存在しません。  
この場合は `detail_out` と直前値の双方をクリアし、原因は共通結果コードだけで表します。

> [!WARNING]
> `cplat_error_get_last()` の値は、次に対応 API を呼び出すと更新されます。  
> 保持が必要な場合は直ちにコピーするか、`detail_out` を使用してください。

> [!WARNING]
> `cplat_error_is_set()` が偽であることは「OS 由来の詳細がない」ことを意味し、**成功したことを意味しません**。  
> 引数の検証エラーなど、OS 呼び出しに由来しない失敗でも偽になります。  
> 呼び出し側は必ず戻り値で成否を判定してください。

`cplat_error_get_cause()` は OS ごとの差を吸収した原因判定に使用し、`cplat_error_to_result()` は詳細を共通結果コードへ変換する場合に使用します。  
人間可読の文字列は `cplat_error_message()` で取得し、公開 API から生の OS エラー値を直接文字列化しません。

### プロセス終了と shutdown コールバックの関係

上位規範の「異常状態の検出とプロセス終了」に従い、`assert` は使用しません。  
プロセスの継続が危険な状態でのみ `abort()` を直接呼び出します。

cplat には終了処理の機構が 2 つあり、`abort()` はいずれも実行しません。

| 終了の経路 | shutdown コールバック | 終了コードの伝達 |
|---|---|---|
| `cplat_exit(code)` | 実行される | `CPLAT_SHUTDOWN_CODE_KIND_EXIT_CODE` として伝わる |
| `exit()` の直接呼び出し、`main()` からの復帰 | 実行される | 終了コードは取得できません。 |
| `abort()` | **実行されない** | 伝達しません。 |

Table: プロセス終了経路ごとの shutdown コールバック実行と終了コード伝達

通常終了で終了コードを確実に渡したい場合は `cplat_exit()` を使用します。

> [!IMPORTANT]
> `abort()` が shutdown コールバックをバイパスするのは、**意図した挙動** です。  
> `abort()` を呼ぶのは内部状態が壊れている場面であり、その状態でコールバックを実行すると、壊れたデータの書き出しや二次障害を招きます。  
> 後始末を実行したい終了は、`abort()` ではなく `cplat_exit()` を使用してください。

### スレッド ローカル記憶域

cplat 内部のスレッド ローカル変数には `compiler.h` の `THREAD_LOCAL` を使用します。  
TLS 変数はソース ファイル内のファイル スコープ `static` に限定し、ヘッダーで `extern` 宣言しません。  
`-ftls-model=initial-exec` や `-ftls-model=local-exec` を指定しません。

> [!WARNING]
> 同一プロセスで cplat の静的ライブラリ版と動的ライブラリ版を混在させると、直前値が複数に分かれます。  
> `cplat_error_get_last()` が想定と異なる値を返すため、混在させてはなりません。

> [!NOTE]
> TLS モデルを指定しないのは、Linux の共有ライブラリおよび `dlopen()` による読み込みへ対応するためです。  
> `initial-exec` や `local-exec` を指定すると、`dlopen()` で読み込んだ場合に TLS の割り当てに失敗します。

### 適用対象外

以下の API 群は元 API の戻り値規約を保存し、共通結果コードの適用対象外とします。

> [!NOTE]
> 対象外を設けるのは、cplat が Linux / Windows のインターフェイス差異を抽象化する層でもあるためです。  
> 元 API の戻り値規約を保存することで、標準 C / POSIX / Win32 の感覚のまま差し替えて使えます。

| 対象外の API 群 | 現行規約 | 理由 |
|---|---|---|
| CRT ラッパー (`cplat_fopen`、`cplat_fclose`、`cplat_fread`、`cplat_open`、`cplat_read`、`cplat_fseek`、`cplat_ftell` など) | `FILE *`/NULL、0/EOF、要素数、fd/-1 など元 API と同一 | 標準 C / POSIX の感覚で使えることが設計意図 |
| `cplat_strcpy` 系、`cplat_getenv` | 成功 0 / バッファー不足 `ERANGE` | CRT の `strcpy_s` 系規約に準拠 |
| `cplat_sscanf`、`cplat_utf8_to_wpath` など | 変換項目数 / 変換文字数 | 元 API の意味を保存 |
| `win32/win32.h` の UTF-8 ラッパー (`CreateFileU` など) | `HANDLE`、`BOOL` など Windows ネイティブ規約 | 元 API の差し替えとして使えることが設計意図 |
| `cplat_strtok_r` | トークンへのポインター / 終了時 NULL | 元 API (`strtok_r` / `strtok_s`) の差し替えとして使えることが設計意図 |
| ハンドル生成系 (`*_create` など) | 成功時ポインター / 失敗時 NULL | ポインター返却 API の慣用 |
| 値をそのまま返す関数 (getter、`cplat_timespec_cmp`、`cplat_strcasecmp`、`cplat_strncasecmp` など) | 値そのもの | 結果コードの概念が適用されない |
| 戻り値を持たない関数 (`*_destroy` など) | `void` | 同上 |

Table: 共通戻り値規約の適用対象外 API 群と適用除外理由

対象外の API 群を新設する場合は、元 API との対応と戻り値規約をヘッダーの Doxygen コメントに明記します。

比較や演算で NULL を正当入力とし、定義済みの値を返す API は、`@param` に「NULL を指定できます」と戻りを記載します。  
`@warning` には置きません。単体テストの確認タグは `[確認_正常系]` です。  
詳細は上位規範の [NULL を正当入力とする API](../../general/docs/coding-guideline.md#null-を正当入力とする-api) に従います。

> [!IMPORTANT]
> [危険な標準関数とマクロの代替](#危険な標準関数とマクロの代替) が定める代替 API のうち、`cplat_snprintf`、`cplat_vsnprintf`、`cplat_fgets`、`cplat_parse_*` は共通結果コードを返し、**上表の対象外には含まれません**。  
> これらは元 API の戻り値規約を保存せず、検査を内包した結果コードへ正規化することが設計意図です。

> [!IMPORTANT]
> `cplat_getenv` は上表の「成功 0 / バッファー不足 `ERANGE`」に加えて「未設定 -1」を返す三値規約です。  
> 本規約への適合は [解消済みの逸脱](#解消済みの逸脱) に整理しています。

### 検証

```bash
# 数値リテラル比較や三値規約の残存確認
grep -nE '(==|!=)[[:space:]]*-1\b' prod/libsrc/cplat/**/*.c

# 局所テスト
make -C app/cplat test
```

## ラッパーの設計方針

### ラッパーを作る条件

CRT / POSIX / Win32 関数のラッパーを cplat へ追加してよいのは、次のいずれかを満たす場合に限ります。

| 条件 | 例 |
|---|---|
| プラットフォームで異なる API を呼び分ける | `cplat_fseek` (`fseeko` と `_fseeki64`)、`cplat_gmtime` (`gmtime_r` と `gmtime_s`)、`cplat_socket_*` (BSD ソケットと Winsock)、`cplat_host_get_name` (`gethostname` と `GetComputerNameExW`)、`cplat_strcasecmp` (`strcasecmp` と `_stricmp` の名前差と locale 差) |
| Windows で UTF-8 と UTF-16 を変換します。 | `cplat_fopen`、`cplat_access`、`CreateFileU` |
| 戻り値やエラー伝達の規約を正規化します。 | `cplat_dup2` (POSIX は newfd、Windows は 0 を返すため 0 へ統一)、`cplat_strcpy` (`ERANGE` を返す) |
| MSVC のセキュア版と意味のある挙動差がある | `cplat_vfprintf` (`vfprintf_s` は不正な書式を拒否する) |
| 境界検査または失敗通知を欠く標準関数を、共通結果コード規約へ正規化します。 | `cplat_snprintf` (切り詰めを検出する)、`cplat_parse_int64` (完全消費と範囲を検査する)、`cplat_fgets` (切り詰めと EOF を区別する) |
| cplat 定義の型を扱います。 | `cplat_timespec_*`、`cplat_file_*` |
| cplat の他機能と統合する必要がある | `cplat_exit` (登録済みシャットダウン コールバックを実行する) |

Table: cplat ラッパー API の追加許容条件と具体例

通信 API の公開面の方針は [net カテゴリの公開面の方針](#net-カテゴリの公開面の方針) に定めます。

境界検査または失敗通知を欠く標準関数の正規化は、[危険な標準関数とマクロの代替](#危険な標準関数とマクロの代替) が定める代替先を cplat 側に用意するための条件です。  
戻り値やエラー伝達の規約を正規化する条件の延長であり、元 API が呼び出し側に委ねている検査を、ラッパーが必ず実施する形へ移します。

> [!NOTE]
> 元 API との差は名前や引数ではなく「検査を省略できないこと」にあります。  
> `snprintf` は切り詰めを検出できる情報を返しますが、呼び出し側が戻り値を捨てれば検査は消えます。  
> ラッパーが検査を内包し、結果コードでしか成否を判定できない形にすると、検査の省略がコンパイル時または明示的な戻り値の破棄としてのみ起こりえます。

### プラットフォーム差異の共通契約

Linux と Windows でラッパー先の API の制約や既定動作が異なる場合、cplat は両プラットフォームで同じ公開契約になるように差異を吸収します。

| 差異 | 共通契約 | 例 |
|---|---|---|
| 引数の限界値 | 両プラットフォームのうち、より厳しい限界値を採用します。 | `RAND_bytes` と `BCryptGenRandom` の要求サイズを `INT_MAX` に統一 |
| オープン時の共有モード | 他プロセスの読み書きを拒否しない既定動作に統一します。 | `cplat_fopen` と `cplat_open` の Windows 実装で `_SH_DENYNO` を指定 |
| シグナルによる待機の中断 | 中断されない Windows の動作に統一します。 | Linux 実装が `EINTR` を吸収します。詳細は [シグナル割り込み (EINTR) の扱い](#シグナル割り込み-eintr-の扱い) |
| 切断済みソケットへの送信 | プロセスを終了させず、送信エラーとして通知します。 | Linux 実装が `MSG_NOSIGNAL` で SIGPIPE を抑制します。 |
| locale 依存の大文字小文字比較 | ASCII の `A`〜`Z` だけを畳み、`setlocale` に依存しません。 | `cplat_strcasecmp` / `cplat_strncasecmp` |

Table: プラットフォーム差異吸収における共通契約方針と具体例

製品実装は、プラットフォーム固有の API を呼び出す前に共通契約を検査または設定します。  
より緩い限界値を持つプラットフォームの追加差分は、共通 API では公開しません。

共有可能なオープンは排他制御を意味しません。  
プロセス間のアクセスを直列化する場合は、`cplat_interprocess_lock` または `cplat_interprocess_rwlock` を使用します。

### net カテゴリの公開面の方針

`net` カテゴリは、BSD ソケットと Winsock の差異を公開面へ出さないことを設計の前提とします。  
要件と、その解決策としての機能は [ネットワーク (net) 機能仕様](functional-spec/net.md) を参照してください。

`net` の公開ヘッダーは、`winsock2.h`、`ws2tcpip.h`、`netinet/in.h`、`sys/socket.h`、`arpa/inet.h`、`netdb.h` のいずれも include しません。  
`PLATFORM_*` による分岐も置きません。  
利用側のライブラリがシステムのソケット ヘッダーに依存しなくなるため、モックだけで通信経路を差し替えた単体テストが成立します。

`struct sockaddr_in` を API 境界へ出しません。  
アドレスとポートの受け渡しには `cplat_ipv4_endpoint` を使用し、`struct sockaddr_in` への変換は `net` の実装ファイル内部でのみ行います。

ソケット オプションは、用途ごとの名前付き API で提供します。  
`level` と `optname` を引数に取る汎用の `setsockopt` 相当 API は公開しません。  
`SOL_SOCKET`、`SO_REUSEADDR`、`SO_BROADCAST`、`IPPROTO_IP`、`IP_MULTICAST_IF`、`IP_ADD_MEMBERSHIP`、`IP_DROP_MEMBERSHIP` などのシステム ヘッダーが定義する定数は、`net` の実装ファイル内部でのみ使用します。

Winsock の初期化と終了は公開 API にしません。  
`net` の内部で初回利用時に初期化し、共有ライブラリのアンロード時に終了します。

バイト オーダー変換は `cplat/net/byteorder.h` の `static inline` 関数として実装し、`htons` などの OS API を呼び出しません。  
OS API への依存が消えるため、変換の検証にモックが不要になります。

### 接続済みソケット送信時の SIGPIPE

Linux の `cplat_socket_send()` と `cplat_socket_send_all()` は、送信ごとに `MSG_NOSIGNAL` を指定します。  
プロセス全体のシグナル ハンドラーやシグナル マスクを変更せず、切断済みの接続への送信による SIGPIPE だけを抑制します。

`MSG_NOSIGNAL` を指定した送信も `EPIPE` を返すため、送信エラーは従来どおり結果コードと `cplat_error` で通知します。  
利用者へ SIGPIPE の無視やハンドラー登録を要求しません。

根拠は [send (2) の MSG_NOSIGNAL](https://man7.org/linux/man-pages/man2/send.2.html) を参照してください。

### シグナル割り込み (EINTR) の扱い

#### 基本原則

cplat の公開 API は、シグナルによる中断を利用者へ観測させません。

Linux では、ユーザー コードの中断を意図しないシグナルが配信されただけでも、ブロッキング中のシステム コールが `EINTR` で復帰します。  
Windows には対応する機構がなく、非アラータブル待機が同じ理由で中断されることはありません。  
この差異は Linux 実装が吸収し、両プラットフォームで同じ契約を提供します。

`SA_RESTART` には依存しません。  
ライブラリは利用者が設置するシグナル ハンドラーのフラグを制御できず、`SA_RESTART` を指定しても再開されない呼び出しが存在するためです。

> [!NOTE]
> `man 7 signal` は、`epoll_wait(2)`、`poll(2)`、`select(2)`、`nanosleep(2)` について「`SA_RESTART` を使っているかどうかに関わらず、再スタートすることは決してない」と規定しています。
> また Linux では、一時停止シグナルによってプロセスが停止し `SIGCONT` で再開された後、シグナル ハンドラーが設定されていなくても `EINTR` で失敗する場合があります。
> 出典は [参照](#参照-シグナル割り込み) にまとめます。

#### API 分類ごとの規範

| 分類 | 対象 | 規範 |
|---|---|---|
| 期限付き待機 | `poll` 系、`nanosleep` | 単調時刻で期限を保持し、`EINTR` では残り時間を再計算して待機を継続します。要求時間より早く復帰しません。 |
| 無期限ブロッキング I/O | `accept`、`send`、`recv`、`sendto`、`recvfrom`、`read`、`write`、`open` | `EINTR` では同じ引数で再試行します。部分転送済みで復帰した場合は OS の規定どおり成功として転送量を返し、再試行しません。 |
| 接続確立 | `connect` | 再呼び出ししません。書き込み可能待ちと `SO_ERROR` の確認によって完了を判定します。 |
| 記述子の解放 | `close` | 再試行しません。 |
| stdio | `fopen`、`fread`、`fwrite`、`fgets`、`fclose` | 再試行しません。適用対象を通常ファイルに限定することで中断を回避します。 |
| pthread 同期 | mutex、condvar、`pthread_join` | `EINTR` を返さないため、`EINTR` の分岐を書かない |
| Windows 実装 | 全般 | 中断されないため `EINTR` 相当の分岐を書かない。アラータブル待機を使用しません。 |

Table: シグナル割り込み時における API 分類別の処理規範

`close` を再試行しないのは、Linux では `EINTR` で復帰した時点で記述子が解放済みであり、再呼び出しが別スレッドの再利用した記述子を対象にしうるためです。

`connect` を呼び直さないのは、中断された接続確立が非同期に継続するためです。  
同じソケットに対する 2 回目の `connect` は `EALREADY` または `EISCONN` を返します。

stdio を再試行しないのは、`FILE *` の内部状態と読み書き位置が中断時点で確定しないためです。  
その代わり、stdio ラッパーの適用対象を通常ファイルに限定します。端末、パイプ、ソケットを `FILE *` として扱う場合は、stdio ラッパーではなく記述子を扱う API を使用します。

> [!NOTE]
> Linux では通常ファイルに対する入出力はシグナルで中断されません。
> `man 7 signal` は、中断されうる「低速デバイス」を端末、パイプ、ソケットと定義し、ローカル ディスクはこれに当たらないと規定しています。
> 適用対象を通常ファイルに限定することで、stdio 分類でも実際にはプラットフォーム間の動作差が生じません。

Windows でアラータブル待機を使用しないのは、`WaitForSingleObjectEx` や `SleepEx` が APC の実行によって `WAIT_IO_COMPLETION` で復帰し、Linux と同種の中断を持ち込むためです。

#### cplat 内部での EINTR の利用

cplat の内部実装が `EINTR` を検出手段として利用することは認めます。  
ただし公開契約には出さず、抽象化した通知へ変換します。

コンソール入力の実装は、`SIGWINCH` によって `read` が中断されたことを端末サイズの変更として扱います。  
利用者が受け取るのは `EINTR` ではなく、サイズ変更を表す抽象化された結果です。

#### 意図的な中断の代替手段

シグナルによってブロッキング呼び出しを打ち切る設計は採用しません。  
Windows へ移植できず、プラットフォームで動作が変わるためです。

停止要求には次の形を使用します。

- ソケットは `cplat_socket_set_nonblocking()` で非ブロッキングに設定し、`cplat_socket_wait_readable()` などへ短いタイムアウトを与えたループの中で停止フラグを判定します。
- スレッド間の停止通知は `cplat_condvar` で行います。

#### CPLAT_CAUSE_INTERRUPTED の位置付け

`CPLAT_CAUSE_INTERRUPTED` は、実行中の操作が中断されたことを表します。  
次の 2 つの経路でこの要因が設定されます。

| 経路 | 設定元 | 内容 |
|---|---|---|
| Windows の I/O キャンセル | `ERROR_OPERATION_ABORTED` | `CancelIo` などによる中断であり、シグナルとは無関係に発生します。 |
| 利用者が持ち込んだ errno | `cplat_error_capture_errno()` | 利用者が自前で呼び出した OS API の `EINTR` を分類します。 |

Table: CPLAT_CAUSE_INTERRUPTED の発生経路と設定元

cplat がシグナルによる中断を理由にこの要因を返すことはありません。  
利用者のコードに、シグナル中断への対処として `CPLAT_CAUSE_INTERRUPTED` の判定と再試行を求めません。

#### 受信タイムアウトと送信タイムアウト

受信タイムアウト (`SO_RCVTIMEO`) と送信タイムアウト (`SO_SNDTIMEO`) を設定する API は公開しません。  
これらを設定したソケットの送受信は `SA_RESTART` の有無にかかわらず中断され、再試行によって指定した時間を超過するためです。  
待機時間を制限する場合は `cplat_socket_wait_readable()` などの期限付き待機を使用します。

#### 検証 (シグナル割り込み)

```bash
# EINTR を参照している箇所が、本節の分類に沿っているかを確認する
grep -rn "EINTR" app/cplat/prod/libsrc/ --include=*.c

# アラータブル待機を使用していないことを確認する
grep -rn "WaitForSingleObjectEx\|SleepEx\|WAIT_IO_COMPLETION" app/cplat/prod/libsrc/
```

#### 参照 (シグナル割り込み)

- [signal(7) - Interruption of system calls and library functions by signal handlers](https://man7.org/linux/man-pages/man7/signal.7.html)
- [close(2) - NOTES](https://man7.org/linux/man-pages/man2/close.2.html)
- [connect(2) - EINTR](https://man7.org/linux/man-pages/man2/connect.2.html)
- [WaitForSingleObjectEx function](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobjectex)

### ラッパーを作らないもの

上記のいずれにも当たらず、両プラットフォームで同じ関数を素通しするだけのラッパーは作りません。  
すでに存在する場合は撤去します。

`memcpy`、`memmove`、`memset`、`strcmp` / `strncmp` / `memcmp`、`malloc` 系、標準出力への `printf` が該当します。  
これらはプラットフォーム差異がなく、MSVC のセキュア版にも意味のある挙動差がなく、境界検査と失敗通知の欠落もありません。

`strcasecmp` / `strncasecmp` は対象外です。  
Linux と Windows で名前とヘッダーが異なり、locale 依存の畳み込み結果も処理系で分かれるため、`cplat_strcasecmp` / `cplat_strncasecmp` を提供します。

テストでモックしたいという理由も、cplat 側にラッパーを作る根拠にはなりません。  
libc 関数のモックは `framework/testfw/libsrc/mock_libc/` が提供する仕組みで受けます。

> [!IMPORTANT]
> **利用件数の多さは抽象価値の根拠になりません。**  
> 上記の関数はリポジトリ内で最も多く使われていますが、ラッパーを作る条件のいずれにも該当しないため対象外です。

### ラッパーがある関数の使用

ラッパーが存在することだけを理由に、`app/` 配下のすべての呼び出しをラッパーへ置き換えることは要求しません。  
安全性向上を目的とするラッパーは、`app/` 配下のすべてのコードで必ず使用します。  
安全性向上には、境界検査、失敗通知の追加、未定義動作またはメモリ破壊の防止を含みます。  
プラットフォーム差異の吸収や単純な委譲だけを目的とするラッパーは、直接呼び出しを一律に禁止しません。

安全性向上を目的とするラッパーは、cplat 自身の実装 (`prod/libsrc/`) にも適用します。  
例外は、そのラッパー自身の実装だけです。  
ラッパーが元の関数またはマクロへ委譲する箇所は、元の関数またはマクロを直接呼び出します。

> [!NOTE]
> 安全性向上を目的とするラッパーを cplat 自身の実装でも使用するのは、実装内部の呼び出しでも安全性の契約を維持するためです。  
> `mock_cplat` による差し替えが必要な場合も、ラッパーを経由していれば実装内部の呼び出しをモックの対象にできます。  
> モック未設定時は実関数へ委譲されます。

### 危険な標準関数とマクロの代替

境界検査を持たない、または終端の保証がない標準関数とマクロは、`app/` 配下の管理対象コードで直接使用しません。  
代替は cplat がすべて提供します。標準関数を代替として指定することはありません。  
外部 OSS 由来のコード (`app/lua`、`app/sqlite`、`app/cjson`、`app/zlib`) は本規則の対象外です。

| 使用しない関数・マクロ | 問題 | 代替 |
|---|---|---|
| `strcpy` | コピー先の容量を受け取らず、境界を検査しません。 | `cplat_strcpy(dest, dest_size, src)` |
| `strncpy` | コピー元が `count` 以上のとき null 終端しません。`count` は宛先容量ではなく最大コピー文字数 | 文字列全体をコピーするなら `cplat_strcpy`、意図的に切り詰めるなら `cplat_strncpy` |
| `strcat` | 連結先の容量を受け取らず、境界を検査しません。 | `cplat_strcat(dest, dest_size, src)` |
| `strncat` | 連結先の容量を受け取らない。`count` は連結元から読む最大文字数であり、終端の 1 バイトも別に必要 | `cplat_strncat(dest, dest_size, src, count)` |
| `wcscpy` | `strcpy` と同じ | `cplat_wcscpy(dest, dest_size, src)` |
| `strdup` / `_strdup` | MSVC では `strdup` が非推奨 (C4996) であり名前が異なります。 | `cplat_strdup(src)` |
| `strtok` | 解析状態をライブラリ内の静的変数に持ち、再入できません。 | `cplat_strtok_r(str, delim, saveptr)` |
| `strtok_r` / `strtok_s` | Linux と Windows で名前が異なり、直接呼び出すと呼び出し側にプラットフォームごとの分岐が必要になります。 | `cplat_strtok_r(str, delim, saveptr)` |
| `gets` | 宛先の容量を指定できません。C11 で標準から削除された | `cplat_fgets(dest, dest_size, stream, detail_out)` |
| `fgets` | 切り詰めと EOF を戻り値で区別できず、改行の有無を呼び出し側が判定する必要がある | `cplat_fgets(dest, dest_size, stream, detail_out)` |
| `sprintf` / `vsprintf` | 出力先の容量を受け取らず、境界を検査しません。 | `cplat_snprintf(dest, dest_size, format, ...)` / `cplat_vsnprintf` |
| `snprintf` / `vsnprintf` | 境界は検査するが、切り詰めの検出を呼び出し側の戻り値検査に委ねている | `cplat_snprintf` / `cplat_vsnprintf` ([書式化](#書式化) を参照) |
| `atoi` / `atol` / `atoll` / `atof` | 変換の失敗を通知せず、範囲外の入力が未定義動作になります。 | `cplat_parse_int` / `cplat_parse_int64` / `cplat_parse_double` ([数値変換](#数値変換) を参照) |
| `strtol` / `strtoll` / `strtoul` / `strtoull` / `strtod` | 完全消費と `errno` の検査を呼び出し側に委ねており、検査を省略しても失敗が表面化しません。 | 同上 |
| `scanf` / `fscanf` / `sscanf` と各 `v*` 版 | 幅を指定しない `%s` が境界外書き込みを起こす | cplat の対応するラッパー ([scanf 系ラッパー](#scanf-系ラッパー) を参照) |
| `strerror` | 戻り値の生存期間が処理系依存で、スレッド セーフとは限らない。再入可能版は `strerror_r` と `strerror_s` で名前と引数が異なります。 | `cplat_error_message(buf, buf_size, error)` ([エラー文字列化](#エラー文字列化) を参照) |
| `malloc` / `calloc` | 長さ 0 の戻り値が処理系定義。`malloc(count * size)` は乗算の回り込みを検出しません。 | `cplat_malloc` / `cplat_malloc_zerofill` / `cplat_calloc` ([メモリ確保の代替](#メモリ確保の代替) を参照) |
| `realloc` | 上記に加え、失敗時の受け方と長さ 0 の扱いを呼び出し側に委ねている | `cplat_realloc` / `cplat_realloc_zerofill` (同上) |
| `free` | 共有ライブラリの境界をまたぐと、確保側と解放側で C ランタイムのヒープが一致しない場合がある | `cplat_free(ptr)` (同上) |
| `FD_SET` | 追加する FD が集合の容量に収まるかを検査せず、Linux では領域外へ書き込み、Windows では追加できない FD を通知せず破棄する | `CPLAT_FD_SET(fd, &set)` ([ファイル記述子集合への追加](#ファイル記述子集合への追加) を参照) |

Table: 使用を禁止する標準関数・マクロと cplat 代替一覧

`cplat_strcpy`、`cplat_strcat`、`cplat_strncat` は、バッファー不足を `CPLAT_ERR_BUFFER_TOO_SMALL` で通知します。  
戻り値を破棄せず、`CPLAT_OK` との比較で判定してください。

```c
/* 望ましい */
int ret;

ret = cplat_strcpy(path, sizeof(path), input);
if (ret != CPLAT_OK)
{
    return ret;
}
```

意図的に文字列を切り詰める場合は `cplat_strncpy` を使用し、切り詰める条件を呼び出し側の仕様として明記します。

> [!WARNING]
> `strncpy` は、コピー元の長さが `count` 以上のときコピー先を null 終端しません。  
> 終端を確認せずに文字列として扱うと、バッファーの外まで読み進めます。  
> `count` がコピー先の容量ではなく最大コピー文字数である点も、容量を渡す API と誤認しやすい原因です。

#### 書式化

バッファーへの書式化は `cplat_snprintf` と `cplat_vsnprintf` を使用します。  
出力が宛先に収まらない場合は `CPLAT_ERR_BUFFER_TOO_SMALL` を返し、宛先を空文字列にします。  
書き込んだ文字数が必要な場合は、成功後に宛先へ `strlen` を適用します。

```c
/* 望ましい */
int ret;

ret = cplat_snprintf(buf, sizeof(buf), "%s/%s", dir, name);
if (ret != CPLAT_OK)
{
    return ret;
}
```

> [!WARNING]
> `snprintf` の戻り値は「書き込んだ文字数」ではなく「終端を除いて必要だった文字数」です。  
> **戻り値が宛先の容量以上なら切り詰めが起きています。**  
> 戻り値を書き込み済みバイト数として次の書き込み位置へ加算すると、バッファーの外を指します。

> [!NOTE]
> 切り詰め時に宛先を空文字列にするのは、`cplat_strcpy` と `cplat_strcat` の既存の振る舞いに合わせるためです。  
> 中途半端に切り詰められた文字列がパスやコマンドとして使われる事故を、戻り値の検査漏れがあっても防げます。

#### 行入力

ストリームからの 1 行読み取りは `cplat_fgets` を使用します。  
戻り値は 1 行を取得した場合に `CPLAT_OK`、読み取る行がない場合に `CPLAT_ERR_EOF`、行が宛先に収まらない場合に `CPLAT_ERR_BUFFER_TOO_SMALL` です。  
取得した行の末尾にある改行 (LF、CR、CRLF) は除去して格納されるため、呼び出し側での改行除去は不要です。

非信頼なストリーム入力は `cplat_fgets` で 1 行を読み取ってから `cplat_sscanf` で解析します。

> [!WARNING]
> `fgets` は、行が宛先に収まらない場合も EOF に達した場合と同じく「途中まで格納して成功を返す」か「NULL を返す」かでしか区別できません。  
> 切り詰めを検出するには、格納結果の末尾に改行があるかと `feof` の状態を呼び出し側で組み合わせる必要があります。

#### 数値変換

文字列から数値への変換は `cplat_parse_int`、`cplat_parse_int64`、`cplat_parse_uint64`、`cplat_parse_double` を使用します。  
これらは文字列全体が数値として解釈されたことと、値が目的の型の範囲に収まることを関数側で検査します。  
解釈できない場合は `CPLAT_ERR_INVALID_INTEGER`、範囲外の場合は `CPLAT_ERR_OUT_OF_RANGE` を返します。  
一般の整数演算と型変換の規則、および本 API が担う範囲の位置づけは [整数演算の安全性](#整数演算の安全性) を参照してください。

```c
/* 望ましい */
int ret;
int64_t size;

ret = cplat_parse_int64(&size, argv[1], 10);
if (ret != CPLAT_OK)
{
    return ret;
}
```

外部文字列を `size_t` やファイル位置へ変換する場合は、`cplat_parse_int64` で解析してから用途上の下限と上限を検査します。  
手順は [コーディング規範](../../general/docs/coding-guideline.md) の「文字列入力から意味付き型への変換」に従います。

幅が処理系に依存する `long` を返す変換 API は提供しません。

> [!WARNING]
> `strtol` 系は、変換できなかった場合も 0 を返します。  
> 成否は戻り値だけでは判定できません。  
> `atoi` 系に至っては失敗を通知する手段がなく、範囲外の入力が未定義動作になります。

> [!IMPORTANT]
> `cplat_parse_uint64` は、先頭の符号 `'-'` を `CPLAT_ERR_OUT_OF_RANGE` として拒否します。  
> `strtoull` は負値を符号なしの折り返し値として受け付けるため、素の呼び出しでは `"-1"` が `UINT64_MAX` になります。

#### エラー文字列化

OS エラーの文字列化は `cplat_error_message` を使用します。  
生の `errno` を持っている場合は、`cplat_error_capture_errno` で `cplat_error` へ取り込んでから渡します。  
`detail_out` を提供する API から受け取った詳細は、そのまま渡せます。

```c
/* 望ましい */
cplat_error err;
char message[256];

cplat_error_capture_errno(&err, errno_value);
(void)cplat_error_message(message, sizeof(message), &err);
```

> [!WARNING]
> `strerror` の戻り値は、次の呼び出しで上書きされる場合があります。  
> 保持したまま別のエラーを文字列化すると、先に取得した文字列の内容が変わります。

> [!NOTE]
> 再入可能版の名前と引数はプラットフォームで異なります。  
> GNU 版 `strerror_r` はバッファーではなくポインターを返すことがあり、XSI 版と戻り値の意味も違います。  
> `cplat_error_message` は、この差と Win32 の `FormatMessageW` をドメインに基づいて吸収します。

#### メモリ確保の代替

動的メモリの一般的な扱いは上位規範の「動的メモリの確保と解放」が定めます。  
cplat は、そこで呼び出し側の責務としている検査を関数側へ内包した確保 API を提供します。  
要素数とサイズの乗算オーバーフローを関数側で担うことと、一般の加減乗算との分担は [整数演算の安全性](#整数演算の安全性) を参照してください。

| 関数 | 用途 | ゼロ初期化 |
|---|---|---|
| `cplat_malloc(size)` | 単一オブジェクト、バイト バッファー | しません。 |
| `cplat_malloc_zerofill(size)` | 同上 | 全体 |
| `cplat_calloc(count, size)` | 要素数を伴う配列 | 全体 |
| `cplat_realloc(ptr, count, size)` | 配列の伸長・縮小 | しません。 |
| `cplat_realloc_zerofill(ptr, old_count, count, size)` | 同上 | 拡張した範囲のみ |
| `cplat_free(ptr)` | 上記すべての解放 | - |

Table: cplat メモリ確保関数の用途とゼロ初期化仕様

いずれも失敗を NULL で表し、共通結果コードを返しません。  
関数側で検査する条件は次のとおりで、該当する場合は確保を行わずに NULL を返します。

- 要求サイズが 0 である (`size == 0`、`count == 0`)
- `count` と `size` の乗算が `size_t` を回り込む

`cplat_free` は NULL を受け取っても何もしません。

```c
/* 望ましい。伸長は別変数で受け、成功時にのみ元のポインターへ代入する */
sample_entry *new_entries;

new_entries = (sample_entry *)cplat_realloc(entries, new_count, sizeof(*new_entries));
if (new_entries == NULL)
{
    return CPLAT_ERR_OUT_OF_MEMORY;
}
entries = new_entries;
```

> [!IMPORTANT]
> `cplat_realloc` と `cplat_realloc_zerofill` は、標準の `realloc` と引数構成が異なります。  
> 要素数とサイズを分けて受け取る 3 引数 (`_zerofill` 版は 4 引数) であり、行を機械的に置換すると引数の位置に不整合が生じます。

> [!IMPORTANT]
> `cplat_realloc(ptr, 0, size)` は、**元の領域を解放せずに NULL を返します**。  
> 標準の `realloc(ptr, 0)` とは異なる扱いです。呼び出し側が `cplat_free` で明示的に解放してください。  
> NULL を「解放済み」と解釈すると領域が漏れます。

> [!NOTE]
> 長さ 0 と乗算オーバーフローの両方で NULL を返すため、NULL は「確保しなかった」ことだけを表します。  
> 上位規範は長さ 0 の確保を呼び出し前の分岐で避けることを求めており、本 API の検査はその二重の防御です。

> [!NOTE]
> `cplat_malloc_zerofill` と `cplat_calloc` は、どちらもゼロ初期化した領域を返します。  
> 単一オブジェクトとバイト バッファーには `cplat_malloc_zerofill`、要素数を伴う配列には `cplat_calloc` を使います。  
> 要素数を伴う確保を `cplat_malloc_zerofill` で記述すると、呼び出し側に乗算の責務が戻るためです。

> [!NOTE]
> `free` に対して `cplat_free` を設けるのは、素通しのラッパーを増やすためではありません。  
> cplat を共有ライブラリとして配布した場合、利用者側と cplat 側で C ランタイムが異なると、`free` に渡したポインターが解放側のヒープに属さない状態になりえます。  
> 確保と解放を同じライブラリ内で完結させることが目的です。

#### ファイル記述子集合への追加

`fd_set` への FD の追加は `CPLAT_FD_SET(fd, &set)` を使用します。  
`<cplat/crt/select.h>` をインクルードしてください。  
`CPLAT_FD_SET()` は FD を集合へ追加できることを検査し、追加できない場合は `abort()` でプロセスを終了させます。  
これは、集合が意図どおりに更新されないまま処理を継続させないための cplat 固有の契約です。

検査する上限は、`fd_set` の構造がプラットフォームで異なるため一致しません。  
Linux は FD を集合のインデックスとして扱うため、FD の値が `0` 以上 `FD_SETSIZE` 未満であることを検査します。  
Windows の `fd_set` は SOCKET の配列と格納数であり、`FD_SETSIZE` は格納できる SOCKET の数の上限です。  
このため Windows では、SOCKET の値ではなく集合の格納数が上限に達していないことを検査します。

生の `FD_SET()` を直接使用できる箇所は、`CPLAT_FD_SET()` がネイティブのマクロへ委譲する定義箇所だけです。

> [!WARNING]
> Linux の `FD_SET()` は、FD が `FD_SETSIZE` 以上かどうかを検査しません。  
> 範囲外の FD を渡すと `fd_set` の領域外へ書き込み、隣接する領域を破壊します。  
> 根拠は [FD_SET(3)](https://man7.org/linux/man-pages/man3/FD_SET.3.html) を参照してください。

> [!WARNING]
> Windows の `FD_SET()` は領域外へ書き込みませんが、集合が満杯の場合は追加せず、失敗も通知しません。  
> 監視されない FD を監視しているつもりで `select` を呼び出す状態になります。  
> 根拠は [fd_set](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-fd_set) を参照してください。

#### 検証

```bash
# 使用しない関数の残存確認
# cplat ラッパー呼び出しと、Doxygen コメント行 (行頭が * のもの) を除外する
grep -rnE '\b(strcpy|strncpy|strcat|strncat|wcscpy|strtok|strtok_r|strtok_s|gets|fgets|sprintf|vsprintf|snprintf|vsnprintf|atoi|atol|atoll|atof|strtol|strtoll|strtoul|strtoull|strtod|strerror)[[:space:]]*\(' \
  app --include=*.c --include=*.h \
  | grep -vE 'app/(lua|sqlite|cjson)/|/obj/|doxybook2_' \
  | grep -vE 'cplat_(strcpy|strncpy|strcat|strncat|wcscpy|strtok_r|fgets|snprintf|vsnprintf|parse_)' \
  | grep -vE ':[[:space:]]*\*'
```

確保系についても同様に確認します。

```bash
# 確保・解放関数の直接使用 (cplat ラッパーの実体を除く)
grep -rnE '(^|[^A-Za-z0-9_])(malloc|calloc|realloc|free)[[:space:]]*\(' \
  app --include=*.c --include=*.h \
  | grep -vE 'app/(lua|sqlite|cjson)/|/obj/|doxybook2_' \
  | grep -vE 'cplat_(malloc|calloc|realloc|free)' \
  | grep -vE ':[[:space:]]*\*'
```

FD 集合への追加についても同様に確認します。

```bash
# 生の FD_SET() の直接使用 (CPLAT_FD_SET() の定義を除く)
grep -rnE '(^|[^A-Za-z0-9_])FD_SET[[:space:]]*\(' \
  app --include=*.c --include=*.h --include=*.cc \
  | grep -vE 'app/(lua|sqlite|cjson)/|/obj/|doxybook2_' \
  | grep -vE ':[[:space:]]*\*'
```

cplat 自身のラッパー実装 (`prod/libsrc/cplat/crt/`、`prod/libsrc/cplat/base/error_message.c`、`prod/include/cplat/crt/select.h`) は、元の関数またはマクロへ委譲するため検出されます。  
[ラッパーがある関数の使用](#ラッパーがある関数の使用) の例外に当たるため、対象外として扱います。

### scanf 系ラッパー

`scanf`、`fscanf`、`sscanf` と各 `v*` 版は、cplat の対応するラッパーを使用します。  
これらの API は Linux の scanf 書式と可変長引数の契約を正とし、Windows でも `_s` 版へ切り替えません。

`%s`、`%S`、`%[` で文字列を格納するときは、終端文字を除いた最大文字数を幅として指定します。  
幅は宛先バッファーの要素数より小さくなければなりません。  
`%c`、`%C` は終端文字を追加しないため、指定幅以上の要素数を持つ宛先を渡します。

非信頼なストリーム入力は `cplat_fgets` で 1 行を読み取ってから `cplat_sscanf` で解析します。  
`cplat_scanf` と `cplat_fscanf` は、既存の scanf 形式との互換が必要な場合に使用します。

> [!WARNING]
> 幅を指定しない `%s` は、宛先の容量に関わらず入力の長さだけ書き込みます。  
> 幅が宛先の要素数以上の場合も、終端文字の分だけ超過します。

> [!NOTE]
> 静的解析と動的解析による検出は次のとおりです。  
> Coverity では、幅なし `%s`、宛先容量以上の幅、cplat ラッパー経由の呼び出しをそれぞれ検出できることを、利用する製品版で確認します。  
> ASan では、幅指定を欠くテスト用入力が境界外書き込みとして報告されることを手動で確認します。  
> どちらも書式が定数で、解析対象に呼び出し元とラッパー実装が含まれる場合に検出を期待できます。

### Win32 API の UTF-8 ラッパーの適用範囲

`CreateFileU` などの `*U` ラッパーは、UTF-8 文字列と Win32 の UTF-16 API の境界を吸収するためのものです。  
したがって、`*U` を使うのは **その呼び出し箇所に UTF-8 との境界が実際に存在する場合** に限ります。

`*W` を直接呼び出してよいのは次の場合です。

- 引数が `L"CONOUT$"` のようなワイド文字列リテラルであり、UTF-8 の文字列が関与しません。
- 呼び出し前後の処理がワイド文字列のまま完結しており、`*U` を経由すると UTF-8 への往復変換が新たに発生します。
- 構造体引数に文字列メンバーが含まれない (`SERVICE_PRESHUTDOWN_INFO` など)

いずれの場合も、`*U` を使わない理由を該当箇所へコメントとして残します。

`*A` 版は使用しません。  
`*A` を使っている箇所は `*U` へ置き換えます。

> [!IMPORTANT]
> `*U` を使わない理由のコメントがないと、将来の点検で「`*U` への置換漏れ」と誤認され、不適切に変更されるおそれがあります。

> [!WARNING]
> `*A` 版はコンソールのコード ページ (日本語環境では cp932) で文字列を解釈します。  
> 非 ASCII 文字を含むパスや文字列で壊れます。

### 新設 API 群の規約

- セキュア消去は volatile 経由で書き込み、コンパイラによる最適化除去を防ぎます。非 volatile のループや素の `memset` は使用しません
- 乱数は暗号論的乱数源のみを使用し、取得に失敗した場合は結果コードで通知します。呼び出し側が失敗を無視して処理を続行しない設計とします
- OS エラーの文字列化は `cplat_error_message()` がドメインに基づいて処理を振り分けます。生の errno と Win32 エラー コードを同一の整数引数で受け取る公開 API は作りません
- 文字列から数値への変換は、完全消費と範囲の検査を関数側に内包します。`endptr` に相当する引数を公開 API に露出させません
- 書式化と行入力は、切り詰めを結果コードで通知します。切り詰めた内容を宛先に残しません
- メモリ確保は、長さ 0 と乗算オーバーフローを関数側で検査し、いずれも NULL を返します。呼び出し側が要素数とサイズの乗算を記述する形式の API は作りません

## アトミック操作

上位規範の「restrict、volatile、inline、register の利用」に対する特化事項です。  
上位規範が「別途設計します」とする atomic API は、cplat を利用するコードでは `cplat/sync/atomic.h` とします。

### volatile を同期に使わない

ロックを取らずにスレッド間、または共有メモリを介してプロセス間で読み書きする値は、`cplat/sync/atomic.h` の専用型に格納し、同ヘッダーの関数だけで読み書きします。  
`volatile` による修飾、GCC の `__atomic` / `__sync` 組み込み関数、Windows の `Interlocked` 関数を、cplat の外で直接使用しません。

| 専用型 | 格納する整数型 | 読み書き、交換、比較交換 | 加算、減算 |
|---|---|---|---|
| `cplat_atomic_u8` | `uint8_t` | 可 | 不可 |
| `cplat_atomic_i32`、`cplat_atomic_u32` | `int32_t`、`uint32_t` | 可 | 可 |
| `cplat_atomic_i64`、`cplat_atomic_u64` | `int64_t`、`uint64_t` | 可 | 可 |
| `cplat_atomic_ptr` | `void *` | 可 | 不可 |

Table: cplat アトミック専用型と対応する整数型および可能操作

専用型のメンバーを直接読み書きしません。  
初期化は `CPLAT_ATOMIC_INIT()`、`{0}`、または他のスレッドへ公開する前のゼロ初期化 (`memset` など) で行います。

ロックの下だけで読み書きする値は、専用型にも `volatile` にもせず、通常の型のままとします。  
ロックの外から 1 か所でも読み書きする値は、専用型とします。

> [!NOTE]
> 専用型を単一メンバーの構造体として定義している理由は、通常の代入や算術演算による直接アクセスをコンパイル エラーとして検出するためです。  
> `volatile` や素の整数型では、アトミックでない読み書きが混ざっても検出できません。

> [!WARNING]
> 専用型のメンバーを直接読み書きすると、データ競合となり動作は未定義です。  
> x64 では多くの場合に偶然動作するため、テストで検出できません。

### メモリ順序の選択

各関数はメモリ順序 (`cplat_memory_order`) を受け取ります。  
順序は、その値でほかのデータの受け渡しを表すかどうかで選びます。

| 用途 | 書き込み側 | 読み取り側 |
|---|---|---|
| 計数だけが目的のカウンター、統計、時刻の記録 | `RELAXED` | `RELAXED` |
| ロックの下での読み書き、またはロックを取る前の緩いチェック | `RELAXED` | `RELAXED` |
| 停止要求や準備完了などのフラグ。フラグの前に書いたデータを、フラグを見た側が読む | `RELEASE` | `ACQUIRE` |
| 確認と更新を 1 回で行う操作 (交換、比較交換、加算) で、前後のデータの受け渡しも担う | `ACQ_REL` | `ACQ_REL` |
| 複数の変数にまたがる全順序が必要な場合 | `SEQ_CST` | `SEQ_CST` |

Table: 用途別のアトミック操作メモリ順序の指定基準

迷った場合は `SEQ_CST` を指定し、根拠を確認できた箇所だけを弱い順序へ緩めます。  
`RELAXED` を指定する箇所では、順序が不要な理由をコメントに記載します。

読み取りに `RELEASE` と `ACQ_REL`、書き込みに `ACQUIRE` と `ACQ_REL` を指定した場合は、`SEQ_CST` として扱います。  
比較交換の失敗時の順序は、成功時の順序から C11 と同じ規則で導きます。

> [!NOTE]
> `RELAXED` は、値そのものの不可分性 (分割アクセスされないこと) と、最適化で読み書きが省略されないことだけを保証します。  
> x64 では、`RELAXED`、`ACQUIRE`、`RELEASE` の読み書きは通常の読み書き命令になり、違いはコンパイラの並べ替えの制限だけです。  
> `SEQ_CST` の書き込みと、交換、比較交換、加算は、ロック付きの命令になります。

### 公開ヘッダーの static inline の例外

`cplat/sync/atomic.h` の関数は、公開ヘッダーの `static inline` 関数として実装します。  
これは上位規範の「`prod/include/` の公開ヘッダーへ新しい `static inline` 関数を追加しません」を、本ヘッダーに限って解除するものです。  
`cplat/net/byteorder.h` と同じ扱いです。

> [!NOTE]
> 解除の理由は次のとおりです。
>
> - アトミック操作の本体は 1 〜 数命令程度であり、通常の関数呼び出しによるオーバーヘッドが目的に対して大きいため。
> - 実装はコンパイラの組み込み関数だけで完結し、OS API を呼ばない。モックによる差し替えが不要なため、エクスポート関数にする利点がないため。
> - 関数が ABI に現れないため、順序の引数の追加や実装の変更が、共有ライブラリの互換性に影響しないため。

### 対応プラットフォーム

GCC は `__atomic` 組み込み関数で実装します。  
MSVC は `<intrin.h>` の intrinsic で実装し、対象を x64 に限定します。  
MSVC の対象アーキテクチャーは `platform.h` で検査し、x64 以外ではコンパイル時にエラーとします。

専用型は、どの型もロック不要 (lock-free) の命令で実装します。  
専用型の大きさと配置は格納する整数型と同じであり、共有メモリ上に置いて複数のプロセスから操作できます。

### 検証 (アトミック操作)

```bash
# 同期目的の volatile の残存確認 (volatile sig_atomic_t と、cplat の実装を除く)
rg -n '\bvolatile\b' app --glob '*.{c,h,cc}' --glob '!**/results/**' \
  | grep -v 'sig_atomic_t' | grep -v 'app/cplat/prod/include/cplat/sync/atomic.h'

# 組み込み関数の直接使用の確認 (cplat/sync/atomic.h を除く)
rg -n '__atomic_|__sync_|\bInterlocked[A-Za-z0-9]*\(' app --glob '*.{c,h,cc}' --glob '!**/results/**' \
  | grep -v 'app/cplat/prod/include/cplat/sync/atomic.h'
```

残存した `volatile` は、上位規範の許容用途の表に照らして分類します。  
外部由来の配布物 (`app/lua`、`app/zlib`、`app/sqlite` など) と、テストのビルドが prod から複製したソースは対象外です。

## API 命名規約

cplat の公開 API 名およびライブラリ内共有 API 名に適用する規則を示します。  
上位規範の「命名規則」がライブラリ接頭辞と記法までを定めるのに対し、本章はカテゴリ名詞と動詞の並び順など cplat 固有の構成規則を定めます。

本規約は既存 API と新設 API の両方に適用します。  
公開関数・型とライブラリ内共有の関数・型・外部リンケージ変数を改名する場合は、宣言、実装、利用側、テスト、文書を同時に更新し、旧名を残しません。

### 基本形

公開 API は `cplat_<カテゴリ名詞>_<動詞または属性>` の順序を正とします。

```c
cplat_file_get_size(...)     /* file カテゴリの getter */
cplat_tracer_set_name(...)   /* tracer カテゴリの setter */
cplat_path_dirname(...)      /* path カテゴリの変換 */
```

ライブラリ内共有 (`include_internal/`) の関数と型は、上位規範に従い `cplat_internal_` を前置きします。  
外部リンケージ変数は `g_cplat_internal_` を前置きします。  
カテゴリ名詞と動詞の並びは公開 API と同じ規則を適用し、接頭辞だけを差し替えます。

```c
/* 公開 */
cplat_tracer_set_name(...)

/* ライブラリ内共有 (関数) */
cplat_internal_trace_resolve_timestamp(...)

/* ライブラリ内共有 (変数) */
extern int g_cplat_internal_sink_count;
```

カテゴリ名詞を持たない横断的な API (`cplat_sleep_ms`、`cplat_parse_int64` など) に限り、動詞先行を許容します。  
元 API 名を保存する CRT ラッパー (`cplat_strcpy`、`cplat_snprintf` など) も、カテゴリ名詞と動詞の並びの対象外です。

### 生成と破棄の動詞対

ハンドルを生成・破棄する API は、`*_create` / `*_dispose` の対を正とします。  
「ハンドルを完全に破棄し二度と使えなくする」という意味の破棄動詞は `*_dispose` に統一します (例: sync カテゴリの破棄関数)。  
`*_detach`・`*_close`・`*_stop`・`*_release` は `*_destroy`/`*_dispose` と意味が異なる (実体は別途終了する、POSIX/CRT の open/close 慣用を模す、再開可能な状態停止、ロック スコープの解除) ため、統一の対象外とし現状の動詞を維持します。

プロセス ライフサイクルで常に有効な既定インスタンスを明示的に初期化する API は `*_init` とし、破棄 API を対にしません。  
`cplat_argparser_init` が該当します。

> [!NOTE]
> 既定パーサーはライブラリが所有し、初期化後はプロセス終了まで常に有効であるため、利用側による破棄を必要としない設計です。  
> `cplat_console_init` の `_dispose` は終了時の状態復元であり、インスタンス破棄ではありません。

### アンダースコア前置きの廃止

`_cplat_` 前置きのシンボルは使用しません。  
用途に応じて、上位規範の「予約識別子の回避」が定める 3 規則に従います。

> [!NOTE]
> 使用しないのは、C 標準がアンダースコアで始まる識別子をファイル スコープで予約しているためです。  
> 以前は、マクロの実体とテスト専用フックの 2 用途に限り `_cplat_` 前置きを許容していましたが、この扱いは撤回しました。

| 用途 | cplat での形式 | 例 |
|---|---|---|
| 呼び出し元情報 (`__FILE__` / `__LINE__`) を補うマクロの実体 | `_at` サフィックス | `cplat_tracer_write_at` |
| 暗黙パーサー版と明示ハンドル版の対 | 暗黙パーサー版は無修飾、明示ハンドル版は `_handle_` を挟む | `cplat_argparser_parse` と `cplat_argparser_handle_parse` |
| テスト専用フック | `_for_test` サフィックスのみ | `cplat_shutdown_invoke_for_test` |

Table: アンダースコア前置き廃止後の代替命名形式と具体例

テスト専用フックは公開ヘッダーに宣言しますが、Doxygen の公開グループには含めず `@internal` を付けます。

> [!NOTE]
> 直接の呼び出しを想定しないことは、名前ではなくドキュメント側で表現します。  
> 名前で表現しようとすると、予約識別子の形式へ寄っていくためです。

### 暗黙パーサー版 API の命名

プロセス共有のパーサーを暗黙に使う API は、`cplat_argparser_` の無修飾名とします。  
明示ハンドルを受け取る API は、`cplat_argparser_handle_` の修飾名とします。

```c
/* 暗黙パーサー版 (一般利用者向け) */
int cplat_argparser_parse(void);

/* 明示ハンドル版 (テスト・複数インスタンス向け) */
int cplat_argparser_handle_parse(cplat_argparser *parser);
```

> [!NOTE]
> 一般利用者はプロセス共有のパーサーを使うため、通常の API 名には実装上の共有方法を含めません。  
> テストや複数インスタンスの利用者は、名前の `_handle_` 修飾によって明示ハンドル版を選択します。

暗黙パーサーの初期化は `cplat_argparser_init` とします。  
明示ハンドルの生成と解放は `cplat_argparser_handle_create` / `cplat_argparser_handle_dispose` とします。

### typedef の規則 (上位規範への追記)

型名の規則は上位規範の「型の命名規則」に完全に従います。  
struct、enum、union、関数ポインターのいずれにも `_t` サフィックスを付けません。

`_t` を維持する例外は、OS / SDK が定義する型の alias に限ります。

> [!NOTE]
> 以前は enum と関数ポインターの `_t` を現状追認として許容していましたが、この扱いは撤回しました。

| 型 | 定義 | 例外とする理由 |
|---|---|---|
| `cplat_file_stat_t` | `include/cplat/crt/sys/stat.h` | POSIX の `struct stat` および MSVC の `struct _stat64` の alias であり、元の型名を保存します。 |
| `cplat_etw_provider_ref_t` | `include/cplat/trace/etw.h` | Windows TraceLogging SDK の内部型への参照の alias であり、SDK の型定義に従う |

Table: _t サフィックスの付与を例外として認める型と理由

## 引数順序規約

cplat の公開 API の引数順序は、API の性格に応じて以下の 3 規則に従います。

### 変換・整形系 (CPLAT_OK 系)

入力を出力バッファーへ変換・整形する API は、CRT の `strcpy_s` 系に合わせて出力バッファーを先頭に置きます。

```c
戻り値 関数名(dest[, dest_size][, detail_out], 入力...);
```

```c
cplat_strcpy(dest, dest_size, src);
cplat_snprintf(dest, dest_size, format, ...);
cplat_parse_int64(value_out, text, base);
cplat_path_dirname(path_out, path_size, detail_out, path);
cplat_gmtime(utc_tm, timep);
cplat_stat(buf, detail_out, path);
```

`detail_out` を提供する場合は、出力引数と、その出力サイズがある場合は出力サイズの直後に置き、後続の入力引数より前に置きます。

> [!IMPORTANT]
> `cplat_stat` は POSIX の `stat(path, buf)` と引数順が逆です。  
> 本規約 (出力先頭) によるものであり、誤りではありません。

### ハンドル・操作系 (CPLAT_OK 系)

ハンドルまたは操作対象を先頭に置き、`*_out` の出力引数は末尾に置きます。

```c
cplat_file_get_size(file, size_out, detail_out);
cplat_path_equal(lhs, rhs, equal_out, detail_out);
cplat_elevated_process_run_with_result(arguments, exit_code, handled, result_message, result_message_size);
```

### 適用対象外 API と _fmt 系

「エラー処理と戻り値規約」の適用対象外 API は、元 API の引数順を保存し、追加の出力引数 (`detail_out` など) は末尾に付加します。

```c
cplat_fopen(path, modes, detail_out);        /* fopen(path, modes) + detail_out */
cplat_fopen_temp(prefix, modes, path_out, path_size, detail_out);
```

`_fmt` 系はパス引数を書式で組み立てる派生 API であり、基底 API からパス引数を除いた残りの引数順を維持し、末尾に `format` と可変長引数を置きます。  
`v*_fmt` は可変長引数を `va_list args` に置き換えます。

```c
cplat_open(path, flags, mode, detail_out);
cplat_open_fmt(flags, mode, detail_out, format, ...);
cplat_vopen_fmt(flags, mode, detail_out, format, args);
```

## 解消済みの逸脱

本規約および上位規範に対する既存公開 API の逸脱として整理していた項目は、すべて解消済みです。

| API | 逸脱内容 | 解消結果 |
|---|---|---|
| `cplat_getenv` | 0 / -1 (未設定) / `ERANGE` の三値。上位規範の三値禁止に抵触 | 設定有無を `int *exists_out` へ分離し、戻り値を 0 / `EINVAL` / `ERANGE` の二値系へ変更 |
| argparser の暗黙パーサー版ラッパー (`cplat_argparser_register_*` など) | `void` 戻りで登録エラーを破棄。明示ハンドル版と成否可視性が異なります。 | 15 関数を `int` 戻りへ変更し、明示ハンドル版の結果コードを転送 |
| `cplat_pinned_prompt_write` | 引数不正時に 0 を返し、正常な 0 バイト書き込みと区別できません。 | 結果コード戻り + `size_t *written_out` へ変更 |
| `cplat_etw_session_start` | ハンドル戻りと `int *out_status` を併用し、他の生成系 (NULL 返却のみ) と失敗通知方式が異なります。 | 結果コード戻り + `cplat_etw_session **session_out` へ変更 |
| `cplat_process_options_t` | typedef struct への `_t` 別名で、上位規範の `_t` 禁止に抵触 | `cplat_process_options` へ統一。同種の `cplat_process_stdio_t` も `cplat_process_stdio` へ統一 |
| enum と関数ポインターの `_t` サフィックス (公開 18 型) | POSIX が予約する名前空間の侵犯。struct とも規則が食い違う | `_t` を除去。関数ポインターはサフィックスを `_fn` へ統一。OS / SDK 由来の alias 2 型のみ例外として維持 |
| `_cplat_` 前置きの公開シンボル (34 件) | C 標準がファイル スコープで予約する識別子形式 | マクロの実体を `_at` サフィックスへ、明示ハンドル版を `_handle_` 付きへ、テスト フックを前置きなしへ変更 |
| 内部共有関数のライブラリ接頭辞漏れ (12 件) | `include_internal/` の宣言に `cplat_` がなく、リンク時に利用側と衝突しうる | 一度 `cplat_` を付与し、その後の全面改名で `cplat_internal_` に統一 |
| `static` 関数へのライブラリ接頭辞 (11 件) | 外部リンケージを持つかのように読め、公開シンボルの点検で偽陽性を生む | 接頭辞を除去 |

Table: 過去の規約逸脱 API とその解消内容一覧

> [!NOTE]
> `cplat_argparser_init` は、既定インスタンスを初期化する `*_init` として本規約に適合するため、逸脱には該当しません。

### 汎用ヘルパー マクロの名前

処理環境を表す次の公開マクロは、上位規範の汎用ヘルパーの例外に従い、現行名を維持します。  
新設する同種のマクロにも用途を表す接頭辞を使用し、cplat 固有の機能を表すマクロには `CPLAT_` を前置きします。

| ヘッダー | マクロ |
|---|---|
| `include/cplat/base/platform.h` と `include/cplat/crt/path.h` | `PLATFORM_WINDOWS`、`PLATFORM_LINUX`、`PLATFORM_UNKNOWN`、`PLATFORM_NAME`、`ARCH_NAME`、`PLATFORM_PATH_MAX`、`PLATFORM_PATH_SEP`、`PLATFORM_PATH_SEP_CHR`、`PLATFORM_NULL_DEVICE_PATH` |
| `include/cplat/base/compiler.h` | `COMPILER_GCC`、`COMPILER_MSVC`、`COMPILER_UNKNOWN`、`COMPILER_NAME`、`COMPILER_VERSION`、`FORCE_INLINE`、`NO_INLINE`、`THREAD_LOCAL` |

Table: 接頭辞 CPLAT_ を付けない汎用ヘルパー マクロ

## 整数演算の安全性

符号混在比較、明示キャストの範囲検査または理由コメント、オーバーフローの事前検査など、整数演算の一般則は上位の [コーディング規範](../../general/docs/coding-guideline.md) の「整数演算の安全性」に従います。

cplat が **関数側で検査を内包する** 範囲は次のとおりです。

| 領域 | API | 関数側の検査 |
|---|---|---|
| 外部文字列から整数への変換 | `cplat_parse_int` / `cplat_parse_int64` / `cplat_parse_uint64` / `cplat_parse_double` | 文字列の完全消費、目的型の範囲、符号なし系での先頭負号の拒否 |
| 要素数とサイズの乗算を伴う確保 | `cplat_calloc` / `cplat_realloc` / `cplat_realloc_zerofill` | 長さ 0、`count * size` の乗算オーバーフロー |

Table: 関数側で整数検査を内包する API と検査内容

一般の加減乗算を検査する API は提供しません。  
呼び出し側は上位規範の事前検査の慣用句に従います。  
検査付きの共通 API が必要になった時点で、本節へ追加を検討します。

詳細は [数値変換](#数値変換) と [メモリ確保の代替](#メモリ確保の代替) を参照してください。

> [!NOTE]
> 一般則を上位規範へ置き、関数を伴う部分だけを本書へ置くのは、cplat が optional であることと整合するためです。  
> 危険な標準関数の代替 (C3) とメモリ確保の代替 (C4) と同じ分担です。

## 標準時刻型 (cplat_timespec)

### 基本ルール

`app/` 配下のコードでは、時刻の受け渡しに `struct timespec` を直接使用せず、公開型 `cplat_timespec` を使用します。  
時刻の秒部、ナノ秒部、期間、時間差の型選択は、上位「コーディング規範」の「値の意味に対応する型」に従います。

| 項目 | 内容 |
|---|---|
| 型定義 | `time_t tv_sec; int64_t tv_nsec;` (16 バイト)。Linux x86-64 の `struct timespec` とレイアウト互換 |
| native 変換 | ネイティブ `struct timespec` が必要な OS API 境界では `cplat_timespec_to_native()` / `cplat_timespec_from_native()` に集約します。キャスト・混用はしません。 |
| 演算 | `cplat_timespec_normalize/add/sub/cmp/add_ms/diff_ms` を使用します。 |

Table: cplat_timespec の型定義と仕様

詳細は `prod/include/cplat/clock/timespec.h` の Doxygen コメントを参照してください。

> [!WARNING]
> Windows UCRT の `struct timespec` は `tv_nsec` が `long` (32bit) であり、Linux とレイアウトが異なります。  
> `struct timespec` のまま共有メモリ、ダンプ、プロセス間で受け渡すと、プラットフォームをまたいだ時点で値が壊れます。

### 例外事項

> [!IMPORTANT]
> 時刻を扱う処理で `cplat_timespec` とは別の時刻型を使用している場合は、互換性を確保するための意図があります。  
> 変更前にユーザーへ確認してください。

## 関数引数の const 付与と Doxygen 方向タグ

### 既存の模範例

上位「コーディング規範」の「関数引数の const 付与と Doxygen 方向タグ」にすでに沿っているヘッダー (新規実装時の参考) を示します。

- `prod/include/cplat/compress/compress.h` - データ系 `[in]` が const
- `prod/include/cplat/crypto/crypto.h` - データ系 `[in]` が const
- `prod/include/cplat/runtime/module.h` - `func_addr` が `const void *`
- `prod/include/cplat/runtime/shutdown.h` - `event` が `const cplat_shutdown_event *`
- `prod/include/cplat/sync/sync.h` の `cplat_interprocess_lock_export_descriptor` - `lock` が `const cplat_interprocess_lock *`

### 内部 lock 取得検査 (grep パターンの拡張)

上位「コーディング規範」の const 付与判定にある「内部 lock 取得検査」の grep パターンには、cplat の同期プリミティブ API を追加してください。

```bash
grep -nE '(pthread_mutex_lock|EnterCriticalSection|cplat_local_(mutex|rwlock|condvar)_)' <dir>/*.c
```

> [!IMPORTANT]
> cplat の同期プリミティブを取得対象として受け取る引数には、論理的に read-only であっても `const` を付けません。  
> `-Wcast-qual` と整合させるためです。判定の詳細は上位規範の「ポインター引数の const 付与判定」を参照してください。
