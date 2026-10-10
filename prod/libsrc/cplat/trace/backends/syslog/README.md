---
short-title: "syslog"
---

# trace backend: syslog

`syslog` backend は、Linux 上で `trace` の OS トレース出力を担当する backend です。  
通常の利用者は `cplat/trace/tracer.h` を経由して使い、syslog 固有 API が必要な場合だけ `cplat/trace/syslog.h` を直接扱います。

## 目的

Linux の標準ログ経路へトレースを流し、systemd / rsyslog など既存の収集運用に統合できるようにします。

- Linux 標準のログ基盤に接続できます。
- アプリケーション側は syslog API を直接知らなくてよい
- `trace` 上位からは Windows EventLog と同じ感覚で OS トレースを使える

## 設計の要点

この backend は `openlog` / `syslog` / `closelog` を包む薄い層です。  
`trace` 上位は共通レベルを syslog severity に変換して、この backend へ渡します。

- `ident` を持つ sink を作成します。
- メッセージは UTF-8 テキストとして syslog へ送る
- 通常は `cplat_tracer_set_os_level()` と `cplat_tracer_write()` 系から透過的に利用される
- `CPLAT_TRACE_LEVEL_VERBOSE` と `CPLAT_TRACE_LEVEL_DEBUG` はどちらも `LOG_DEBUG` に集約される

## trace から見た役割

Linux では OS トレース先が syslog になります。  
そのため `trace.h` の利用者は、Windows では EventLog、Linux では syslog へ出ることだけ意識すれば十分です。

## backend 単体の役割

`cplat/trace/syslog.h` は、syslog sink を直接生成したい場合の lower layer です。  
ただし通常の利用では `trace.h` を入口にし、識別子管理やレベル変換は上位へ任せます。

## 注意点

- Linux 専用です
- `syslog()` 自体にはタイムアウト制御手段がありません  
  現代の systemd / rsyslog 環境では大きな問題になりにくい一方、旧来の `SOCK_STREAM` ベース環境ではブロッキングの懸念があります  
  その懸念がある場合は、OS トレースを `CPLAT_TRACE_LEVEL_NONE` にしてファイル backend を主経路にする運用を検討してください

## 向いている用途

- Linux サービスを既存の syslog 運用へ統合したい場合
- journald / rsyslog 側で集約・転送したい場合
- ファイル出力よりも OS 標準のログ経路を優先したい場合

## テスト用途: SYSLOG_TEST_FD

環境変数 `SYSLOG_TEST_FD` にパイプの書き込み端 FD 番号を設定すると、`/dev/log` への送信を行わず、その FD にデバッグ用メッセージを書き込みます。  
環境変数は `cplat_syslog_sink_create()` の時点で 1 回だけ参照するため、シンクを生成する前に設定してください。

`cplat_tracer_write*()` からの経路では、debug FD へ出る行は `YYYY-MM-DDTHH:MM:SS.mmm+09:00 <PRI>TAG[PID]: MSG` の形式になります。  
一方、`/dev/log` へ送る通常 syslog メッセージ本体は従来通り RFC 3164 形式で、タイムスタンプはプラットフォーム側に任せます。

これにより、実際の syslog デーモンなしで送信内容をテスト プロセスが受け取れます。  
`testfw` の `processController` が `preload_lib` オプションと組み合わせてこの仕組みを使用します。

```c
/* テスト プロセス側でパイプを作成し、書き込み端の FD 番号を環境変数に設定する */
setenv("SYSLOG_TEST_FD", "<fd_number>", 1);
```
