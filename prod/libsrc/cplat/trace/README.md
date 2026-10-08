---
short-title: "trace"
---

# trace - クロスプラットフォーム トレース基盤

`trace` は、Windows と Linux の両方で同じコードからトレースを出力するためのユーティリティです。  
`cplat/trace/tracer.h` を唯一の入口として、OS トレース、ファイル、`stderr` を共通のトレース レベルで扱えます。

## 目的

アプリケーションから見れば、OS トレース (Windows では EventLog、Linux では syslog) の違いを意識せずにトレースを書けます。  
OS トレースは運用者が参照する OS ネイティブの運用ログです。  
さらに、OS トレースとは別にファイル出力と `stderr` 出力を併用でき、それぞれに独立したしきい値を設定できます。  
Windows では、EventLog とは別に、開発者向けの低オーバーヘッド診断チャネルとして ETW を独立した軸で扱います。

- OS ごとのトレース API 差異を吸収します。
- OS トレース (EventLog / syslog) / ETW / ファイル / `stderr` を同じハンドルで管理します。
- 出力先ごとに `CRITICAL` から `DEBUG` までのしきい値を分けられる
- 呼び出し側は `trace.h` だけを見ればよい

## 設計の要点

`trace` は、設定フェーズと出力フェーズを分けたライフサイクルを持ちます。  
`cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)` 直後は stopped 状態で、`cplat_tracer_set_*()` による設定変更が可能です。  
`cplat_tracer_start()` 後は started 状態となり、`cplat_tracer_write()` 系で出力できます。

```text
create -> set_name / set_* -> start -> write -> stop -> dispose
```

トレース レベルの変更 (`cplat_tracer_set_os_level()` 系) は started 中でも行えます。停止せずにしきい値を変えられ、変更は排他制御下で原子的に反映されるため、旧しきい値と新しきい値の両方で出力対象となるトレースを取りこぼしません。  
識別子・ファイル名・フックの設定 (`cplat_tracer_set_name()`, `cplat_tracer_set_file_name()`, `cplat_tracer_set_hook()`, `cplat_tracer_remove_hook()`) は stopped 中のみ変更でき、変える場合は一度 `cplat_tracer_stop()` で stopped に戻します。  
`cplat_tracer_dispose()` は started / stopped のどちらからでも呼べます。  
破棄にはハンドル変数のアドレスを渡します。破棄後は変数へ `NULL` が設定され、tracer が所有する同期オブジェクトを利用者が解放する必要はありません。  
`cplat_tracer_get_state()` を使うと、handle が `started` / `stopped` / `disposed` のどれかを確認できます。

### 並行処理管理モード

`cplat_tracer_create()` は、ハンドルごとの同期方式を明示的に受け取ります。  
`CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED` は tracer が読み書きロックを生成、使用、破棄するため、同一ハンドルを複数スレッドから呼び出せます。  
`CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED` はハンドル専用ロックを生成しないため、単一スレッドで利用する場合や、呼び出し側に既存の排他制御がある場合に余分な同期コストを避けられます。  
このモードでは、同一ハンドルへの呼び出しが重ならないことを利用者が保証します。異なるハンドルを並行して生成、利用、破棄することはできます。

## トレース レベル

共通レベルは `CPLAT_TRACE_LEVEL_CRITICAL` から `CPLAT_TRACE_LEVEL_DEBUG` までの 6 段階で、`CPLAT_TRACE_LEVEL_NONE` はその出力先を無効化します。

- `CPLAT_TRACE_LEVEL_CRITICAL`: 致命的エラー
- `CPLAT_TRACE_LEVEL_ERROR`: エラー
- `CPLAT_TRACE_LEVEL_WARNING`: 警告
- `CPLAT_TRACE_LEVEL_INFO`: 通常の運用情報
- `CPLAT_TRACE_LEVEL_VERBOSE`: 詳細な診断情報
- `CPLAT_TRACE_LEVEL_DEBUG`: 最も詳細な診断情報
- `CPLAT_TRACE_LEVEL_NONE`: 出力しません。

Linux syslog と Windows ETW には `VERBOSE` より細かい標準レベルがないため、`CPLAT_TRACE_LEVEL_DEBUG` はこれらでは `CPLAT_TRACE_LEVEL_VERBOSE` と同じ詳細度として扱われます。  
Windows EventLog はイベント タイプが Error / Warning / Information の 3 種のみのため、`INFO` / `VERBOSE` / `DEBUG` はいずれも Information になりますが、分析性を高めるためレベル毎に異なるイベント ID とカテゴリを割り当てます。  
一方でファイルと `stderr` では `DEBUG` を独立したレベル文字で区別します。

各出力先は「設定レベル以上に重大なメッセージだけを出力する」動作です。

### レベルの選び方

`INFO` 以上は常時記録の対象となる想定でレベルを設計しています。運用中のシステムで何が起きたかを、設定を変更せずに後から追えるようにするためです。

`INFO` には、繰り返し発生する処理の記録や、1 件ごとの詳細を含めないでください。常時記録の対象が増え続けると保存期間が短くなり、必要な記録が先に失われます。

量の多い記録や、内部の状態を細かく残す記録は `VERBOSE` または `DEBUG` にしてください。これらは既定では記録せず、調査の必要が生じたときに有効にします。

強制出力のレベルで `VERBOSE` と `DEBUG` を syslog の `LOG_INFO` と ETW の Informational へ引き上げているのは、この設計に合わせて常時記録の帯へ入れるためです。

## 既定の動作

`cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)` 直後の既定値は次のとおりです。

- OS トレース (EventLog / syslog): `CPLAT_TRACE_LEVEL_NONE`
- ETW (Windows のみ): `CPLAT_TRACE_LEVEL_NONE`
- ファイル: `CPLAT_TRACE_LEVEL_NONE`
- `stderr`: `CPLAT_TRACE_LEVEL_NONE`

すべての出力先を既定で無効とし、利用者が出力先ごとの詳細度を設定してはじめて出力します。  
明示的に設定しない限り、トレースがログ ファイルの生成などの副作用を起こさないようにするためです。

出力するには、`cplat_tracer_start()` の前に `cplat_tracer_set_os_level()`、`cplat_tracer_set_etw_level()`、`cplat_tracer_set_file_level()`、`cplat_tracer_set_stderr_level()` のうち必要なものを呼び出します。  
ファイル出力は、`cplat_tracer_set_file_level()` で `CPLAT_TRACE_LEVEL_NONE` 以外の詳細度を設定した場合に有効になります。  
このとき出力先パスを指定しなければ、`cplat_tracer_start()` 時に既定パス (実行ファイルのディレクトリ配下の `log/{ファイル名}.log`) を使用します。

## 出力先

### OS トレース

OS 標準のログ基盤 (運用者向け) へ送ります。

- Windows: EventLog (イベント ログ)
- Linux: syslog

`cplat_tracer_set_os_level()` でしきい値を設定します (既定は無効)。  
通常は `trace.h` 経由で使い、プラットフォームごとの backend を直接操作する必要はありません。  
Windows の EventLog はイベント ソースが cplat 全体で共通のため、利用前に `eventlog-register` コマンドでソースを登録します (後述)。

### ETW (Windows のみ)

開発者向けの低オーバーヘッド診断チャネルです。  
`cplat_tracer_set_etw_level()` でしきい値を設定します (既定は無効)。  
ETW イベントは consumer (`etw-viewer` など) が購読したときのみ実体化されるため、有効にしても通常時のコストは小さく抑えられます。  
Linux には ETW が存在しないため、`cplat_tracer_set_etw_level()` / `cplat_tracer_get_etw_level()` は何もせず、しきい値は常に `NONE` を返します。

### ファイル

ローカル ファイルへ 1 行ずつ追記します。  
サイズ上限に達すると `path`, `path.1`, `path.2` の形式でローテーションします。

### stderr

コンソールやサービス起動時の即時確認向けです。  
ローカル時刻の ISO8601 タイムスタンプ付き 1 行テキストで出力されます。

```text
2026-04-02T12:34:56.789+09:00 I application started
2026-04-02T12:34:56.790+09:00 D polling state dump
```

## 代表 API

### cplat_tracer_create

トレース ハンドルを作成します。  
初期名は実行ファイル名で、取得できない場合は `"unknown"` になります。

### cplat_tracer_set_name

識別名を設定します。  
複数インスタンスを識別したい場合は識別番号付きの名前を使えます。

### cplat_tracer_set_os_level

OS トレース (Windows は EventLog、Linux は syslog) のしきい値を設定します。

### cplat_tracer_set_etw_level

ETW のしきい値を設定します (Windows のみ)。OS トレースとは独立した軸です。  
Linux では何もせず、`cplat_tracer_get_etw_level()` は常に `NONE` を返します。

### cplat_tracer_set_file_level

ファイル出力先、ファイル用しきい値、最大サイズ、世代数、動作フラグを設定します。  
ファイル出力を使う場合の入口です。  
flags に 0 を指定した場合は単一プロセス専用であり、プロセス間の調停を行いません。  
このモードは OS の排他的オープンを使用しないため、呼び出し側が他プロセスから同一パスへ書き込まないことを保証する必要があります。  
`CPLAT_TRACE_FILE_SINK_SHARED` を指定すると複数プロセスから同一パスへ書き込むための調停が有効になり、ローテーションはロック ファイル `<path>.lock` によるプロセス間排他のもとで実行されます (ロック ファイルは常設で、削除されません)。

### cplat_tracer_set_stderr_level

`stderr` 出力のしきい値を設定します。

### cplat_tracer_start / cplat_tracer_stop

出力の開始と停止を行います。  
書き込みは started 中に行います。トレース レベルの変更は started / stopped のどちらでも行え、識別子・ファイル名・フックの設定は stopped 中に行います。

### cplat_tracer_get_state

現在の handle 状態を返します。  
`create` 直後と `stop` 後は `STOPPED`、`start` 後は `STARTED`、`NULL` または利用不可状態は `DISPOSED` です。

### cplat_tracer_write / cplat_tracer_writef

通常のトレース メッセージを書き込みます。  
公開名の `cplat_tracer_write*()` は source location (`[file:line]`) を自動付与する関数風マクロです。  
`timestamp` に `NULL` を渡すと内部で現在時刻を取得し、明示した `cplat_timespec` を渡すと file / `stderr` 出力と Linux の `SYSLOG_TEST_FD` デバッグ経路にその時刻を使います。  
明示タイムスタンプが不正な場合も出力自体は継続し、内部で現在時刻へ代替します。この場合の戻り値は `-1` です。

### cplat_tracer_write_hex / cplat_tracer_write_hexf

バイナリ データを HEX テキストとして書き込みます。  
通信データやバッファー内容を確認したいときに使います。  
タイムスタンプ引数の扱いは `cplat_tracer_write` / `cplat_tracer_writef` と同じです。

source location を付けずに生のメッセージを書き込みたい場合は、低レベル関数 `cplat_tracer_write_at*()` を直接呼びます。

## 使い方

呼び出し側は backend の違いを書き分けず、同じコードで利用できます。  
すべての出力先が既定で無効のため、start の前に出力したい出力先の詳細度を設定します。

```c
#include <cplat/trace/tracer.h>

int main(void)
{
    cplat_tracer *tracer = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
    if (tracer == NULL) {
        return 1;
    }

    cplat_tracer_set_name(tracer, "myapp", 0);
    cplat_tracer_set_stderr_level(tracer, CPLAT_TRACE_LEVEL_INFO);
    cplat_tracer_start(tracer);

    cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "application started");
    cplat_tracer_writef(tracer, CPLAT_TRACE_LEVEL_WARNING, NULL, "retry=%d", 3);

    cplat_tracer_stop(tracer);
    cplat_tracer_dispose(&tracer);
    return 0;
}
```

ファイル出力を有効にする場合は、start 前に file sink を設定します。

```c
#include <cplat/trace/tracer.h>

cplat_tracer *tracer = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);

cplat_tracer_set_name(tracer, "myapp", 0);
cplat_tracer_set_os_level(tracer, CPLAT_TRACE_LEVEL_WARNING);
cplat_tracer_set_file_level(tracer, "./logs/myapp.log",
                           CPLAT_TRACE_LEVEL_INFO, 0, 0, 0);
cplat_tracer_set_stderr_level(tracer, CPLAT_TRACE_LEVEL_CRITICAL);
cplat_tracer_start(tracer);

cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "service ready");

cplat_tracer_dispose(&tracer);
```

`max_bytes == 0` は既定値、`generations <= 0` も既定値として扱われます。  
末尾の引数は動作フラグで、0 はプロセス間調停を行わない単一プロセス専用モード、`CPLAT_TRACE_FILE_SINK_SHARED` は複数プロセス共有モードです。

## プラットフォームごとの動作

### Windows

- OS トレースは EventLog を使う (運用者向け。既定は無効)
- ETW は独立した診断チャネルとして使う (既定は無効)
- EventLog のイベント ソースと ETW プロバイダー登録は、いずれも `trace` 上位でプロセス内共有します。
- EventLog のイベント ソースは `eventlog-register` コマンドで登録/削除します。
- `stderr` とファイルは共通の書式で扱います。

### Linux

- OS トレースは syslog を使用します。
- `stderr` とファイルは共通の書式で扱います。
- syslog の facility や `ident` は内部で `trace` が管理します。

## 注意点

- トレース レベルの設定関数は started / stopped のどちらでも使えます。識別子・ファイル名・フックの設定関数は stopped 状態でのみ使えます
- メッセージ長には共通上限があり、長文は安全な範囲で切り詰められます
- HEX 出力も上限があり、収まらない場合は末尾を省略します
- ファイル出力はローカル ファイル システム向けです
- backend 固有の制約は各 backend README を参照してください

## backend ドキュメント

- `backends/eventlog/README.md`: Windows EventLog backend (OS トレース)
- `backends/etw/README.md`: Windows ETW backend (独立した診断チャネル)
- `backends/file/README.md`: ファイル backend
- `backends/syslog/README.md`: Linux syslog backend (OS トレース)
