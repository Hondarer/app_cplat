---
short-title: "eventlog"
---

# trace backend: eventlog

`eventlog` backend は、Windows 上で `trace` の OS トレース (運用者向け) を担当する backend です。  
OS トレースは Linux では syslog、Windows ではこの EventLog backend が担当します。  
通常の利用者は `cplat/trace/tracer.h` を経由して使用し、EventLog 固有 API が必要な場合に限り `cplat/trace/eventlog.h` を直接扱います。

## 目的

Windows のアプリケーション イベント ログへ運用ログを出力し、Event Viewer や監視基盤から参照できるようにします。

- Windows 標準の運用ログ経路 (イベント ログ) に統合できます。
- アプリケーション ログを Event Viewer から確認できます。
- `trace` 上位からは syslog との差異を意識せずに利用できます。

## 設計の要点

この backend は `RegisterEventSourceW`、`ReportEventW`、`DeregisterEventSource` をラップする層です。  
イベント ソースは cplat 全体で共通とし、ソース名には `CPLAT_TRACER_DEFAULT_PROVIDER_NAME` を用います。

- `trace` 上位では OS トレースの出力先として利用する。
- Windows では複数の `cplat_tracer` があっても、イベント ソース ハンドルは共有する。
- 通常のメッセージ出力は `cplat_tracer_write()` 系から透過的に EventLog へ出力される (既定は無効)。
- イベント ソースが共通のため、インスタンス識別名を本文先頭に `[name] ` 形式で付与してインスタンスを判別できるようにする。
- `ReportEventW` には置換文字列を 5 件渡し、メッセージ文字列、実行体ファイル パス、ファイル識別子、インスタンス名、インスタンス識別子を EventData に記録する。
- 実行ファイルの絶対パスは初回書き込み時に 1 回だけ解決し、以後はキャッシュを使用する。
- 分析性を高めるため、レベル毎に異なるイベント タイプ・イベント ID・カテゴリを割り当てる。

### レベルとイベント属性の対応

| トレース レベル | イベント タイプ | イベント ID (識別子なし) | カテゴリ |
|---|---|---|---|
| `CRITICAL` | Error | 0x1001 | 1 |
| `ERROR` | Error | 0x1002 | 2 |
| `WARNING` | Warning | 0x1003 | 3 |
| `INFO` | Information | 0x1004 | 4 |
| `VERBOSE` | Information | 0x1005 | 5 |
| `DEBUG` | Information | 0x1006 | 6 |

Table: トレース レベルとイベント属性の対応

強制出力のレベルは、対応する通常のレベルと同じ行の属性で記録します。強制出力の `VERBOSE` と `DEBUG` は `INFO` の行の属性で記録します。

EventLog のイベント タイプは Error / Warning / Information の 3 種のみですが、レベルと表示形式毎にイベント ID を分けることで、Event Viewer 側でのフィルターや分析を容易にします。

イベント ID は 0x1000 番台に配置します。これは、メッセージ ファイル内でカテゴリ メッセージを 1 から CategoryCount までの ID に固定配置する必要があり、イベント メッセージの ID 空間と分離するためです。ファイル識別子またはインスタンス識別子が 0 以外の場合は、末尾の `_0` を出力しないために別のイベント ID を使用します。

## イベント ソースの登録

EventLog のイベント ソースは、レジストリ `HKLM\SYSTEM\CurrentControlSet\Services\EventLog\Application\{ソース名}` に登録します。  
この登録は HKLM への書き込みを伴うため、管理者権限 (UAC 昇格) が必要です。  
登録/削除には `eventlog-register` コマンドを使用します。

```text
eventlog-register install     共通イベント ソースを登録する (UAC 昇格)
eventlog-register uninstall   共通イベント ソースの登録を削除する (UAC 昇格)
```

`eventlog-register` は未昇格で起動された場合に UAC を要求して自身を再実行し、呼び出し元のプロセスが結果を表示します。

ソース未登録でも `ReportEventW` 自体は成功しますが、Event Viewer 上ではソース名の解決が行われません。  
メッセージ テーブル リソース (MESSAGETABLE) は `libcplat_eventlog_messages.dll` に格納します。  
DLL は使用中の `libcplat.dll` と同じディレクトリに配置してください。  
`eventlog-register` は DLL のメッセージ テーブルの存在を確認し、EventMessageFile と CategoryMessageFile へ DLL の絶対パスを設定します。  
DLL を読み込めない場合やメッセージ テーブルが存在しない場合は、登録を行わず失敗します。  
登録に成功すると、標準出力へ登録したメッセージ DLL の絶対パスを表示します。  
同時に、メッセージ DLL はイベント ソースの登録が有効な期間を通じて必要であることを注記します。  
これにより Event Viewer は本文 (`[インスタンス名] メッセージ`) とカテゴリ名のみを表示し、イベント ID の説明が見つからない旨の補完文は表示しません。  
Event Viewer の「全般」では、実行体ファイル パス、インスタンス名、メッセージ文字列を順に表示し、メッセージ文字列の手前に空行を 1 行入れて区切ります。ファイル識別子またはインスタンス識別子が 0 以外の場合は、それぞれ `_識別子` を付与します。イベント XML には 5 件の EventData が記録されるため、ログ収集側は各値を個別に参照できます。

従来の `eventlog-register.exe` を登録済みの場合は、DLL の配置後に `eventlog-register install` を再実行して登録先を更新してください。  
事前の `uninstall` は不要です。  
過去のログを表示する必要がある間は、イベント ソースの登録と登録先の DLL を保持してください。  
共通イベント ソースを利用するアプリケーションが複数ある場合は、DLL の配置先と更新・削除の管理を共通化してください。  
イベント ID、カテゴリ ID、置換文字列の意味と順序、言語別リソースは従来との互換性を維持します。

### カテゴリ名表示の制約

`ReportEventW` で渡すカテゴリは classic Event Log API のカテゴリ番号です。  
`CategoryMessageFile` にカテゴリ メッセージを登録しているため、PowerShell の `Get-EventLog` では `CategoryNumber=4`、`Category=INFO` のようにカテゴリ名を解決できます。

```powershell
Get-EventLog -LogName Application -Source 'cplat.tracer' -Newest 1 |
    Select-Object CategoryNumber, Category, Message |
    Format-List
```

一方、Event Viewer GUI の一覧列「タスクのカテゴリ」と `Get-WinEvent` の `TaskDisplayName` は Windows Event Log API 側の task 表示名として扱われます。  
この backend は manifest provider ではなく classic Event Log source として登録しているため、`Get-WinEvent` では `Task=4`、`TaskDisplayName` は空になり、Event Viewer GUI の一覧では `(4)` のように番号で表示されます。

```powershell
Get-WinEvent -FilterHashtable @{
    LogName = 'Application'
    ProviderName = 'cplat.tracer'
} -MaxEvents 1 |
    Select-Object Task, TaskDisplayName, Id, Message |
    Format-List
```

この差異は Event Viewer GUI の表示経路の制約として扱います。

## 主な用途

### trace.h から使用する場合

通常は本ヘッダーを使用します。  
`cplat_tracer_set_os_level()` で OS トレースを有効にし、`cplat_tracer_start()` の呼び出し後に出力します。  
事前に `eventlog-register install` でイベント ソースを登録してください。

### eventlog.h を直接使用する場合

イベント ソースの登録/削除 API (`cplat_eventlog_register_source()` / `cplat_eventlog_unregister_source()`) を明示的に使用したい場合に限ります。  
通常はこれらを `eventlog-register` コマンド経由で呼び出します。

## 注意点

- Windows 専用です。
- イベント ソースの登録/削除には管理者権限が必要です。
- メッセージ テーブル リソースは `libcplat_eventlog_messages.dll` に格納し、登録時に EventMessageFile / CategoryMessageFile へ設定します。登録後の Event Viewer は本文とカテゴリ名のみを表示します。
- Event Viewer GUI の一覧列「タスクのカテゴリ」は Windows Event Log API 側の task 表示名を使用するため、この backend では `(4)` のようにカテゴリ番号で表示されます。カテゴリ名は `Get-EventLog` の `Category` で確認します。
- イベント ログのメッセージ サイズには上限があり、極端に長いメッセージは記録されません。

## 使い分け

- 通常のトレースを出力する場合: `trace.h`
- イベント ソースの登録または削除を行う場合: `eventlog-register` コマンド (内部で `eventlog.h` を使用)

`trace` の入口としては、あくまで `trace.h` が中心です。
