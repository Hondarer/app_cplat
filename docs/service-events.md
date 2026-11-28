# サービス イベントのタイミング

この文書では、Windows サービスと systemd サービスで扱えるイベントのタイミングと、`cplat/service/service.h` (`cplat_service_*`) で抽象化している範囲および内部契約を整理します。

## Windows サービスの一般的なイベント

Windows サービスは Service Control Manager (SCM) から起動され、サービス コントロール ハンドラーで制御要求を受け取ります。

Table: Windows サービスの一般的なイベント

| タイミング | SCM の制御または状態 | 説明 |
|---|---|---|
| 起動開始 | `ServiceMain` | SCM からサービス プロセスが起動され、`StartServiceCtrlDispatcher` 経由で呼ばれます。 |
| 起動中 | `SERVICE_START_PENDING` | 初期化中であることを SCM に通知します。 |
| 起動完了 | `SERVICE_RUNNING` | サービスが要求を受け付けられる状態になったことを SCM に通知します。 |
| 停止要求 | `SERVICE_CONTROL_STOP` | `sc stop`、サービス管理画面、削除前の停止などで送られます。 |
| 停止中 | `SERVICE_STOP_PENDING` | 停止処理中であることを SCM に通知します。 |
| 停止完了 | `SERVICE_STOPPED` | サービスが終了したことを SCM に通知します。 |
| OS シャットダウン | `SERVICE_CONTROL_SHUTDOWN` | OS のシャットダウン時に、受け付けを宣言したサービスへ送られます。 |
| OS シャットダウン前 | `SERVICE_CONTROL_PRESHUTDOWN` | 通常のシャットダウン通知より前に送られます。受け付けの宣言が必要です。 |
| 状態照会 | `SERVICE_CONTROL_INTERROGATE` | SCM が現在状態の再通知を求めます。 |
| 電源状態変更 | `SERVICE_CONTROL_POWEREVENT` | サスペンド、レジュームなどの電源イベントで送られます。 |
| セッション変更 | `SERVICE_CONTROL_SESSIONCHANGE` | ログオン、ログオフなどで送られます。 |
| 設定再読込 | `SERVICE_CONTROL_PARAMCHANGE` | 設定再読込要求で送られます。 |

## systemd サービスの一般的なイベント

systemd では、unit ファイル、プロセス終了、シグナル、`sd_notify()` でライフサイクルを表現します。  
`cplat` の Linux 実装 (`libcplat.so`) は `libsystemd` (`LIBS += systemd`) を直接リンクし、`sd_notify(3)`、`sd_event_*`、`sd_bus_*` を使用します。

## cplat service モジュールにおける抽象化範囲

`cplat_service_*` はライフサイクルの遷移点と再読込の前後で OS へ自動通知します。  
すべての操作 API と停止待機 API の戻り値は共通結果コード (`CPLAT_OK` / `CPLAT_ERR_*`) です。

Table: `cplat` サービス機能の通知抽象

| 概念 | タイミング | Windows (SCM) | Linux (systemd) |
|---|---|---|---|
| 起動完了 | `on_start` 成功直後 | `SERVICE_RUNNING` を SCM に通知します。 | `READY=1` を送信します。 |
| 停止開始 | `on_run` 復帰直後 | `SERVICE_STOP_PENDING` を SCM に通知します。 | `STOPPING=1` を送信します。 |
| 再読込開始 | `on_reload` 呼び出し直前 | 何もしない (SCM に等価通知なし) | `RELOADING=1` を送信します。 |
| 再読込完了 | `on_reload` 復帰直後 | `SERVICE_RUNNING` を再報告します (停止中は報告しません)。 | `READY=1` を再送信します (停止中は送信しません)。 |
| 状態テキスト | `cplat_service_set_status_text()` 呼び出し時 | トレース出力のみ | `STATUS=` を送信します。 |

ライフサイクル コールバック (`on_start` / `on_run` / `on_stop`) は、戻り値で成功 (`0`) と失敗 (`0` 以外) を返します。  
`cplat_service_run()` および `cplat_service_run_console()` は API 自体の成否を共通結果コードで返し、コールバックの終了コードを出力引数 `exit_code_out` へ格納します。  
失敗の値は [プロセスの終了コード](../../general/docs/coding-guideline.md#プロセスの終了コード) に従い、1 から 125 の範囲 (3 を除く) を推奨します。

Table: `cplat` サービス機能の OS イベント抽象

| 共通イベント (`cplat_service_event_type`) | Windows (SCM) | Linux (systemd-logind) |
|---|---|---|
| `CPLAT_SERVICE_EVENT_POWER_SUSPEND` | `SERVICE_CONTROL_POWEREVENT` + `PBT_APMSUSPEND` | `PrepareForSleep(true)` |
| `CPLAT_SERVICE_EVENT_POWER_RESUME` | `SERVICE_CONTROL_POWEREVENT` + `PBT_APMRESUMEAUTOMATIC` / `PBT_APMRESUMESUSPEND` | `PrepareForSleep(false)` |
| `CPLAT_SERVICE_EVENT_SESSION_LOGON` | `SERVICE_CONTROL_SESSIONCHANGE` + `WTS_SESSION_LOGON` | `SessionNew` |
| `CPLAT_SERVICE_EVENT_SESSION_LOGOFF` | `SERVICE_CONTROL_SESSIONCHANGE` + `WTS_SESSION_LOGOFF` | `SessionRemoved` |
| `CPLAT_SERVICE_EVENT_PRESHUTDOWN` | `SERVICE_CONTROL_PRESHUTDOWN` | `PrepareForShutdown(true)` |

`on_event` と `on_reload` の契約:

- 両 OS とも `on_run` とは別スレッド (Windows: SCM ハンドラー スレッド、Linux: イベント監視スレッド) から呼ばれるため、共有データへのアクセスには同期が必要です。
- OS 側の応答期限があるため短時間で戻る必要があります (Linux の delay inhibitor lock は logind の `InhibitDelayMaxSec`、既定 5 秒。Windows の pre-shutdown 猶予は既定 30 秒、`cplat_service_install_options.preshutdown_timeout_ms` で変更可能)。
- `on_start` の成功後から `on_run` が戻るまでの間に呼び出しを開始します。開始済みの呼び出しは `on_stop` と並行実行中の場合があります。
- 動作段階の確認・更新と OS 通知は同一の通知ロック内で直列化し、停止中遷移後の `READY=1`・`RELOADING=1`・`WATCHDOG=1`・`SERVICE_RUNNING` を抑止します。`on_event`、`on_reload`、トレース出力は通知ロック外で実行します。

## Linux 固有の動作と watchdog・動的 join 上限

- `cplat_service_prepare_run()` はスレッド生成前・トレーサー開始前に呼び出し、`SIGHUP` をブロックします。
- `cplat_service_install()` の既定オプション (`options == NULL` または `{0}`) は、`ExecStart=<exe> run`、`StartLimitIntervalSec=60`、`StartLimitBurst=5`、`Restart=on-failure`、`RestartSec=5`、`WatchdogSec=30`、`OOMScoreAdjust=-1000` (および systemd 248 以降で `ManagedOOMPreference=omit`) を生成します。
- 自動再起動有効時 (`disable_auto_restart == 0`) は、実効の `start_limit_burst`、`restart_delay_sec`、`start_limit_interval_sec` に対して `((burst - 1) * restart_delay_sec) < interval_sec` (オーバーフロー検査込み) を検査し、不適合時は `CPLAT_ERR_INVALID_ARGUMENT` を返して登録を拒否します。
- `WATCHDOG_USEC` が設定されている場合、期限の 1/4 周期で `on_run` が `cplat_service_wait_for_stop()` を呼び続けているかを確認し、進行している間だけ `WATCHDOG=1` を送信します。
- 停止時のイベント監視スレッド終了待ち上限は既定 30000 [ms] です。`WATCHDOG_USEC` が 30 [s] 未満の場合は、`min(30000 [ms], watchdog_usec / 1000 [ms])` (下限 1000 [ms]) へ自動調整します。上限時間内に終了しない場合は `cplat_abort_at()` でプロセスを異常終了させます。

## 自動再起動の対応状況

Table: `cplat` サービス機能の自動再起動の対応状況

| 観点 | Linux | Windows |
|---|---|---|
| 異常終了時の自動再起動 | `disable_auto_restart == 0` のとき `Restart=on-failure` / `RestartSec=<遅延>`、`StartLimitIntervalSec=<期間>` / `StartLimitBurst=<回数>` を出力します。 | `disable_auto_restart == 0` のとき `SERVICE_CONFIG_FAILURE_ACTIONS` (`SC_ACTION_RESTART`、遅延 `<遅延> * 1000` [ms]) と `SERVICE_CONFIG_FAILURE_ACTIONS_FLAG` (`fFailureActionsOnNonCrashFailures = TRUE`) を設定します。 |
| 無効化 | `cplat_service_install_options.disable_auto_restart != 0` | `cplat_service_install_options.disable_auto_restart != 0` |
