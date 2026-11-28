<!--
生成元: agree-before-implement スキル
文書種別: 最終仕様 (design)
最終更新日時: 2026-11-28 08:46:00 JST
最終更新エージェント: agy / セッション ID: 73c19cdb-9b01-4b7c-831a-02f92420ab83
稼働 OS: Linux (WSL)
上流文書: `261127-service-commonization-overview.md` (全体管理文書)、`261127-service-commonization-discussion.md` (検討用 Markdown)、`261127-service-commonization-lightadr.md` (簡易 ADR)。いずれも削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。上流文書の全体管理文書が存在する場合は、その再開手順に従って進行位置を確認し、決定事項を簡易 ADR で、判断の経緯を検討用 Markdown の Q&A で確認します。存在しない場合は、この文書を単体の最終仕様として扱います。最終更新エージェントのセッション情報や稼働 OS・最終更新日時が参照可能であれば再開時のコンテキストや環境差分の把握に利用し、別エージェントや別環境・別 OS からの引き継ぎ、担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。
-->

# service-sample の共通化可能部分の cplat 取り込み 最終仕様

ファイル名: `261127-service-commonization-design.md`

`app/service-sample` のクロスプラットフォーム サービス基盤を `app/cplat` の独立した機能カテゴリ `service` として取り込み、`libsystemd` の単体テスト用モックを `framework/testfw` へ整備し、`app/service-sample` を新 API を利用する薄いサンプル実装へ書き換えます。  
対象環境は 64 ビットの Linux (systemd) および Windows (Service Control Manager: SCM) です。

## 対象範囲と構成の全体像

本改修は、役割の異なる 3 つのコンポーネント (`framework/testfw`、`app/cplat`、`app/service-sample`) で構成します。  
`app/cplat` は他の検討中機能に依存せず、サービス基盤および電源・セッション・シャットダウン前イベント監視を含めて自己完結で動作します。

Table: コンポーネントごとの責務と変更概要

| コンポーネント | 責務 | 主な変更内容 |
| --- | --- | --- |
| `framework/testfw` | `libsystemd` 単体テスト用モックの提供 | 独立静的ライブラリ `libmock_systemd.a` (`include/mock_systemd.h`、`include_override/systemd/`、`libsrc/mock_systemd/`、`SKIP_BUILD = WINDOWS`) および局所テスト `test/src/mockSystemdTest/` を新設します。 |
| `app/cplat` | クロスプラットフォーム サービス基盤 API の提供 | 新規機能カテゴリ `service` (`prod/include/cplat/service/service.h`、`prod/libsrc/cplat/service/`、`docs/functional-spec/service.md`) を追加します。Linux は `libcplat.so` へ `LIBS += systemd` を直接リンクし、`exportTest.cc`、`mock_cplat`、単体テスト、関連文書を同期更新します。 |
| `app/service-sample` | 新 API を使ったサービス アプリケーションの模範実装 | `cplat/service/service.h` と `cplat_elevated_process_run_piped()` を使う薄い CLI・コールバック雛形へ書き換え、重複するサービス実装・旧 `mock_systemd`・重複テスト・移設済みの旧仕様文書 2 件を削除します。 |

```text
framework/testfw/
+-- include/
|   +-- mock_systemd.h
+-- include_override/
|   +-- systemd/
|       +-- sd-bus.h
|       +-- sd-daemon.h
|       +-- sd-event.h
+-- libsrc/
|   +-- mock_systemd/
|       +-- makefile
|       +-- makepart.mk                  # SKIP_BUILD = WINDOWS、pkg-config 検査
|       +-- mock_systemd.cc
|       +-- mock_sd_bus.cc
|       +-- mock_sd_daemon.cc
|       +-- mock_sd_event.cc
+-- test/
    +-- src/
        +-- mockSystemdTest/             # SKIP_BUILD = WINDOWS

app/cplat/
+-- docs/
|   +-- README.md                        # service 関連文書の案内を追加
|   +-- api-cheatsheet.md                # cplat_service_* を追加
|   +-- link-policy.md                   # Linux の libsystemd 直接リンクを追記
|   +-- functional-spec-guideline.md     # カテゴリ SERVICE の主語を追加
|   +-- service-events.md                # app/service-sample/docs/ から移設・API 名更新
|   +-- systemd-oomkiller-advisory.md    # app/service-sample/docs/ から移設・API 名更新
|   +-- functional-spec/
|       +-- README.md                    # service カテゴリを追加
|       +-- service.md                   # 新規機能仕様 (CPLAT-SERVICE-*)
+-- prod/
|   +-- include/
|   |   +-- cplat.h                      # CPLAT_SERVICE グループ定義と service.h の include
|   |   +-- cplat/
|   |       +-- service/
|   |           +-- service.h            # 公開 API ヘッダー
|   +-- include_internal/
|   |   +-- cplat/
|   |       +-- service/
|   |           +-- service_internal.h   # 共通・OS 実装間の内部契約
|   |           +-- service_linux_events.h # Linux イベント監視スレッドの内部契約
|   +-- libsrc/
|       +-- cplat/
|           +-- makepart.mk              # Linux に pkg-config 検査と LIBS += systemd を追加
|           +-- service/
|               +-- makefile             # makefw 標準テンプレート (サブディレクトリ走査)
|               +-- service.c            # 共通: 停止抽象、ライフサイクル駆動、イベント・再読込配送
|               +-- service_linux.c      # Linux: prepare_run / install / uninstall / run / sd_notify
|               +-- service_linux_events.c # Linux: sd_event / logind D-Bus / SIGHUP / watchdog
|               +-- service_windows.c    # Windows: SCM ディスパッチャー / install / uninstall
+-- test/
    +-- include/
    |   +-- mock_cplat.h                 # cplat_service_* の宣言と MOCK_CPLAT_LINK_IMPL
    +-- libsrc/
    |   +-- mock_cplat/
    |       +-- mock_cplat.cc            # cplat_service_* の ON_CALL 既定委譲
    |       +-- service/                 # 公開関数ごとの mock_cplat_service_*.cc
    +-- src/
        +-- libcplatTest/
            +-- exportTest/
            |   +-- exportTest.cc        # CPLAT_EXPORT_TABLE_COMMON へ cplat_service_* を追加
            +-- service/
                +-- makefile
                +-- serviceCommonTest/   # 共通層の単体テスト
                +-- serviceLinuxTest/    # SKIP_BUILD = WINDOWS、LIBS += mock_libc mock_cplat mock_systemd systemd
                +-- serviceWindowsTest/  # Windows SCM 層の単体テスト
```

## cplat 公開 API 仕様

### 配置と命名体系

公開ヘッダーは `app/cplat/prod/include/cplat/service/service.h` に配置し、`app/cplat/prod/include/cplat.h` のアンブレラ ヘッダーへ追加します。  
Doxygen グループは `CPLAT_SERVICE` (`@ingroup CPLAT_PUBLIC_API`) とし、公開シンボル接頭辞は関数・型を `cplat_service_*`、列挙値・マクロ定数を `CPLAT_SERVICE_*` へ統一します。

すべての公開関数は `CPLAT_EXPORT` および `CPLAT_API` を付与し、`app/cplat/prod/libsrc/cplat/exports.map` の `global: cplat_*;` に一致させます。

### API の状態モデルと責務境界

`cplat_service_*` はプロセス グローバル API として提供します。  
1 プロセスでのマルチ インスタンス実行は適用範囲外とします。  
停止待機・停止要求・状態通知の API はハンドル引数を取らず、サービス実行の開始時と終了時に内部状態を初期化・解放します。

> [!NOTE]
> Linux のシグナル マスク (`SIGHUP`) と `NOTIFY_SOCKET`、および Windows の `SERVICE_WIN32_OWN_PROCESS` と `StartServiceCtrlDispatcher` はいずれもプロセス単位の資源です。実態と一致するプロセス グローバル API とし、単体テストや同一プロセス内での連続実行に備えて入口と出口で内部状態の初期化・後始末を完結させます。

`app/cplat` が担う範囲は個別のサービス操作 API に限定します。  
コマンド ライン引数の解析 (`install` / `uninstall` / `run` / `console` / `--help`)、`main()` のディスパッチ、および UAC 等による権限昇格再起動は利用側の CLI 層の責務とします。

### 診断トレースの所有権

`cplat_service_definition` に任意の `cplat_tracer *tracer` フィールドを設け、サービス層から非同期に発生する診断ログ (コールバック失敗、`sd_notify` / `SetServiceStatus` 失敗、D-Bus 警告、watchdog 停滞警告、`SIGHUP` 無視など) の出力先とします。  
`cplat_tracer` の生成・設定・開始・停止・破棄の所有権は利用側にあります。  
`tracer` が `NULL` の場合、サービス層はトレース出力を省略します (`cplat_tracer_write(NULL, ...)` の no-op 契約に依存します)。

### 戻り値規約とエラー伝達

すべてのサービス操作 API および停止待機 API の戻り値は、`cplat/base/result.h` の共通結果コード (`CPLAT_OK` = 0、負値の `CPLAT_ERR_*`) に統一します。  
公開 API に `cplat_error *detail_out` 出力引数は設けません。  
OS エラー起因の失敗時は、内部で `cplat_internal_error_report_*` によりスレッド ローカルの直前エラー (`cplat_error_get_last()`) を記録し、あわせて `def->tracer` へ診断トレースを出力します。

`cplat_service_run()` および `cplat_service_run_console()` は、API 自体の成否を戻り値 (`CPLAT_OK` / `CPLAT_ERR_*`) で返し、利用側コールバック (`on_start` / `on_run` / `on_stop`) が返した終了コードを出力引数 `int *exit_code_out` (`NULL` 可) へ格納します。

### 公開型・列挙・構造体定義

#### ライフサイクル コールバック型

```c
typedef int (*cplat_service_on_start_fn)(void *user_data);
typedef int (*cplat_service_on_run_fn)(void *user_data);
typedef int (*cplat_service_on_stop_fn)(void *user_data);
```

各コールバックは成功時に `0`、失敗時に `0` 以外を返します。  
失敗値はプロセスの終了コードまたは Windows SCM の `dwServiceSpecificExitCode` として OS へ伝わるため、1 から 125 の範囲 (3 は `abort()` と重複するため除く) を推奨します。

- `on_start`: 起動時に 1 回呼び出されます。`0` 以外を返すと起動を中断し、`on_run` および `on_stop` は呼び出しません。
- `on_run`: `on_start` 成功後に 1 回呼び出されます。停止要求が届くまで戻らないように実装します。`0` 以外を返しても、後始末のため `on_stop` を呼び出します。
- `on_stop`: `on_run` が戻った後に必ず 1 回呼び出されます。`on_run` と `on_stop` の双方が失敗した場合は、`on_run` の戻り値を採用します。

#### OS イベント型とコールバック型

```c
typedef enum cplat_service_event_type
{
    CPLAT_SERVICE_EVENT_POWER_SUSPEND = 1,
    CPLAT_SERVICE_EVENT_POWER_RESUME = 2,
    CPLAT_SERVICE_EVENT_SESSION_LOGON = 3,
    CPLAT_SERVICE_EVENT_SESSION_LOGOFF = 4,
    CPLAT_SERVICE_EVENT_PRESHUTDOWN = 5
} cplat_service_event_type;

typedef struct cplat_service_event_info
{
    cplat_service_event_type type;
    unsigned int pad; /**< 64 ビット環境でのアラインメント明示用パディング。未使用。 */
    const char *session_id; /**< SESSION_LOGON / SESSION_LOGOFF のみ有効。他は NULL。 */
} cplat_service_event_info;

typedef void (*cplat_service_on_event_fn)(const cplat_service_event_info *info, void *user_data);
typedef void (*cplat_service_on_reload_fn)(void *user_data);
```

`session_id` はコールバック復帰後に無効となるため、保持が必要な利用側は複製します。  
`on_event` と `on_reload` は `on_run` とは別スレッド (Linux: イベント監視スレッド、Windows: SCM コントロール ハンドラー スレッド) から呼び出されるため、共有データへのアクセスには同期が必要です。  
配送は `on_start` 成功後 (動作中段階) から `on_run` 復帰前までに開始し、すでに開始した呼び出しは `on_stop` と並行実行中の場合があります。

#### サービス定義構造体

```c
typedef struct cplat_service_definition
{
    const char *name;                       /**< 登録名 (必須・空文字不可)。 */
    const char *display_name;               /**< 表示名 (Windows SCM 表示名。install 時は必須)。 */
    const char *description;                /**< 説明文 (NULL 可、NULL 時は空文字扱い)。 */
    cplat_service_on_start_fn on_start;     /**< 初期化コールバック (NULL 可)。 */
    cplat_service_on_run_fn on_run;         /**< メイン ループ コールバック (run / run_console 時は必須)。 */
    cplat_service_on_stop_fn on_stop;       /**< 停止処理コールバック (NULL 可)。 */
    void *user_data;                        /**< 各コールバックへ渡す任意ポインター (NULL 可)。 */
    cplat_service_on_event_fn on_event;     /**< OS イベント コールバック (NULL 可)。 */
    cplat_service_on_reload_fn on_reload;   /**< 設定再読込コールバック (NULL 可)。 */
    cplat_tracer *tracer;                   /**< 診断トレース出力先 (NULL 可、所有権は利用側)。 */
} cplat_service_definition;
```

全メンバーが 8 バイト ポインターのため、64 ビット環境で構造体パディング (`-Wpadded`) は発生しません。

#### サービス登録オプション構造体

```c
typedef struct cplat_service_install_options
{
    const char *run_args;                  /**< 起動引数 (NULL 時は "run"、"" 時は引数なし)。 */
    unsigned int restart_delay_sec;        /**< 自動再起動遅延 [s] (0 時は既定 5 [s])。 */
    unsigned int start_limit_interval_sec; /**< Linux: 起動回数集計期間 [s] (0 時は既定 60 [s])。 */
    unsigned int start_limit_burst;        /**< Linux: 期間内最大起動回数 (0 時は既定 5 回)。 */
    unsigned int watchdog_sec;             /**< Linux: WatchdogSec [s] (0 時は既定 30 [s])。 */
    unsigned int preshutdown_timeout_ms;   /**< Windows: PRESHUTDOWN 猶予 [ms] (0 時は既定 30000 [ms])。 */
    int disable_auto_restart;              /**< 0: 自動再起動あり (既定) / 非 0: 自動再起動なし。 */
    int disable_watchdog;                  /**< Linux: 0: watchdog あり (既定) / 非 0: WatchdogSec= なし。 */
    int disable_oom_protection;            /**< Linux: 0: OOM 保護あり (既定) / 非 0: OOM 保護なし。 */
} cplat_service_install_options;
```

`options == NULL` またはゼロ初期化 (`{0}`) を渡した場合、すべての項目に安全な既定値を適用します。  
構造体サイズは 40 バイト (8 + 4 × 8) であり、64 ビット環境でパディングは発生しません。

Table: `cplat_service_install_options` の既定値と OS 別の反映内容

| フィールド | ゼロ / NULL 時の既定値 | Linux (systemd unit) | Windows (SCM) |
| --- | --- | --- | --- |
| `run_args` | `"run"` | `ExecStart=<exe> <run_args>` (`""` 時は `ExecStart=<exe>`) | `binPath="\"<exe>\" <run_args>"` (`""` 時は `"\"<exe>\""`) |
| `restart_delay_sec` | `5` [s] | `disable_auto_restart == 0` 時に `Restart=on-failure` および `RestartSec=<値>` を出力 | `disable_auto_restart == 0` 時に `SC_ACTION_RESTART` (遅延 `<値> * 1000` [ms]) および `fFailureActionsOnNonCrashFailures = TRUE` を設定 |
| `start_limit_interval_sec` | `60` [s] | `disable_auto_restart == 0` 時に `[Unit]` へ `StartLimitIntervalSec=<値>` を出力 | 未使用 |
| `start_limit_burst` | `5` 回 | `disable_auto_restart == 0` 時に `[Unit]` へ `StartLimitBurst=<値>` を出力 | 未使用 |
| `watchdog_sec` | `30` [s] | `disable_watchdog == 0` 時に `WatchdogSec=<値>` を出力 | 未使用 |
| `preshutdown_timeout_ms` | `30000` [ms] | 未使用 | `SERVICE_CONFIG_PRESHUTDOWN_INFO` に設定 |
| `disable_auto_restart` | `0` (有効) | 非 0 時は `Restart=`・`RestartSec=`・`StartLimit*` を出力しない | 非 0 時は `SERVICE_CONFIG_FAILURE_ACTIONS*` を設定しない |
| `disable_watchdog` | `0` (有効) | 非 0 時は `WatchdogSec=` を出力しない | 未使用 |
| `disable_oom_protection` | `0` (有効) | 0 時は `OOMScoreAdjust=-1000`、および systemd 248 以降で `ManagedOOMPreference=omit` を出力。非 0 時は両方出力しない | 未使用 |

> [!IMPORTANT]
> Linux の `cplat_service_install()` で `disable_auto_restart == 0` の場合、実効の `start_limit_burst`、`restart_delay_sec`、`start_limit_interval_sec` に対して `((burst - 1) * restart_delay_sec) >= interval_sec` が成り立つとき、または乗算 `(burst - 1) * restart_delay_sec` が `uint64_t` 範囲でオーバーフローするとき、`CPLAT_ERR_INVALID_ARGUMENT` を返して登録を拒否します。起動回数制限が永遠に発動せず無限再起動ループへ陥る誤設定を防止するためです。同様に、Windows で `restart_delay_sec * 1000` が `DWORD` 上限 (`UINT32_MAX`) を超える場合も `CPLAT_ERR_INVALID_ARGUMENT` を返します。

### 公開関数一覧と契約

```c
CPLAT_EXPORT int CPLAT_API cplat_service_prepare_run(void);
CPLAT_EXPORT int CPLAT_API cplat_service_install(const cplat_service_definition *def,
                                                 const cplat_service_install_options *options);
CPLAT_EXPORT int CPLAT_API cplat_service_uninstall(const cplat_service_definition *def);
CPLAT_EXPORT int CPLAT_API cplat_service_run(const cplat_service_definition *def, int *exit_code_out);
CPLAT_EXPORT int CPLAT_API cplat_service_run_console(const cplat_service_definition *def, int *exit_code_out);
CPLAT_EXPORT int CPLAT_API cplat_service_wait_for_stop(int timeout_ms);
CPLAT_EXPORT int CPLAT_API cplat_service_stop_requested(void);
CPLAT_EXPORT void CPLAT_API cplat_service_request_stop(void);
CPLAT_EXPORT void CPLAT_API cplat_service_set_status_text(const char *text);
```

Table: 公開関数ごとの契約・結果コード・スレッド セーフ性

| 関数名 | 動作概要 | 主な戻り値 | スレッド セーフ |
| --- | --- | --- | --- |
| `cplat_service_prepare_run()` | Linux で `pthread_sigmask(SIG_BLOCK, {SIGHUP})` を実行します (スレッド生成前、トレーサー開始前に呼び出します)。Windows では何もしません。 | `CPLAT_OK`、`CPLAT_ERR_*` (Linux の `pthread_sigmask` 失敗時) | 非対応 (起動直後の単一スレッド段階で呼び出し) |
| `cplat_service_install(def, options)` | OS へサービスを登録します。自動権限昇格は行いません。 | `CPLAT_OK`、`CPLAT_ERR_INVALID_ARGUMENT`、`CPLAT_ERR_PERMISSION_DENIED`、`CPLAT_ERR_BUFFER_TOO_SMALL`、`CPLAT_ERR_DUPLICATE_DEFINITION` (Windows 既存サービス時)、`CPLAT_ERR_BUSY` (Windows 削除待ち時)、`CPLAT_ERR_UNKNOWN` | 非対応 |
| `cplat_service_uninstall(def)` | OS からサービスを停止・解除します。自動権限昇格は行いません。 | `CPLAT_OK`、`CPLAT_ERR_INVALID_ARGUMENT`、`CPLAT_ERR_PERMISSION_DENIED`、`CPLAT_ERR_NOT_FOUND` (Windows 未登録時)、`CPLAT_ERR_TIMEOUT` (Windows 停止待機時間切れ時)、`CPLAT_ERR_BUSY` (Windows 削除待ち時)、`CPLAT_ERR_UNKNOWN` | 非対応 |
| `cplat_service_run(def, exit_code_out)` | OS サービス マネージャー管理下 (Linux: `Type=notify` + イベント監視スレッド、Windows: `StartServiceCtrlDispatcherU`) で常駐実行します。入口で停止抽象を初期化し、出口で解放します。 | `CPLAT_OK` (コールバック終了コードは `*exit_code_out` に格納)、`CPLAT_ERR_INVALID_ARGUMENT`、`CPLAT_ERR_OUT_OF_MEMORY`、`CPLAT_ERR_UNKNOWN` (イベント監視スレッド初期化失敗・イベント ループ異常終了・SCM 接続失敗など) | 非対応 |
| `cplat_service_run_console(def, exit_code_out)` | フォアグラウンドのコンソール モードでライフサイクルを実行します。`cplat_console_init()` および `cplat_shutdown_request_register()` により `SIGINT` / `SIGTERM` / `SetConsoleCtrlHandler` を停止要求へ接続します。 | `CPLAT_OK` (コールバック終了コードは `*exit_code_out` に格納)、`CPLAT_ERR_INVALID_ARGUMENT`、`CPLAT_ERR_OUT_OF_MEMORY`、`CPLAT_ERR_UNKNOWN` | 非対応 |
| `cplat_service_wait_for_stop(timeout_ms)` | 停止要求が届くか `timeout_ms` [ms] 経過するまで待機します。`CPLAT_NO_WAIT` (0) および `CPLAT_WAIT_FOREVER` (`INT_MAX`) に対応します。入口・出口で `on_run` の進行カウンターを更新します。 | `CPLAT_OK` (停止要求到達)、`CPLAT_ERR_TIMEOUT` (時間経過)、`CPLAT_ERR_INVALID_ARGUMENT` (`timeout_ms < 0` または未初期化状態での呼び出し)、`CPLAT_ERR_*` (条件変数待機失敗時) | 対応 |
| `cplat_service_stop_requested()` | 停止要求済みかを即時判定します。未初期化時は `0` を返します。 | `1` (停止要求済み)、`0` (未要求または未初期化) | 対応 |
| `cplat_service_request_stop()` | 停止要求を設定し、待機中のスレッドへブロードキャストします。べき等であり、未初期化時は何もしません。 | なし (`void`) | 対応 |
| `cplat_service_set_status_text(text)` | 状態テキストを診断トレース (`VERBOSE`) へ出力し、Linux では `sd_notify("STATUS=...")` を送信します。`text == NULL` は何もしません。 | なし (`void`) | 非対応 |

> [!NOTE]
> `cplat_service_wait_for_stop()` が `CPLAT_ERR_TIMEOUT` を返す場合のみ周期ループを継続 (`while (cplat_service_wait_for_stop(1000) == CPLAT_ERR_TIMEOUT)`) する慣用句により、停止要求到達時 (`CPLAT_OK`) と待機異常時 (`CPLAT_ERR_*`) の双方で安全にループを抜け、無待機スピンを防ぎます。ループ脱出後に戻り値を確認すれば、正常停止と異常終了を判別できます。

## 内部設計とプラットフォーム実装仕様

### 内部ヘッダーとファイル分割

`app/cplat` のサブディレクトリ走査方式に従い、`prod/libsrc/cplat/service/` に `makefile` (makefw 標準テンプレート) と 4 つの `.c` ファイルを配置します。  
`prod/libsrc/cplat/makepart.mk` へ相対パスを `ADD_SRCS` で列挙しません。

- `prod/include_internal/cplat/service/service_internal.h`:
    - 共通定数 (`CPLAT_SERVICE_EXEC_PATH_MAX = 4096U`、既定の `CPLAT_SERVICE_DEFAULT_RESTART_DELAY_SEC = 5U`、`CPLAT_SERVICE_DEFAULT_START_LIMIT_INTERVAL_SEC = 60U`、`CPLAT_SERVICE_DEFAULT_START_LIMIT_BURST = 5U`、`CPLAT_SERVICE_DEFAULT_WATCHDOG_SEC = 30U`、`CPLAT_SERVICE_DEFAULT_PRESHUTDOWN_TIMEOUT_MS = 30000U`)
    - 共通層・OS 層間の内部共有関数 (すべて `_cplat_` または `cplat_internal_service_*` 接頭辞、非エクスポート):
        - 停止抽象の初期化・解放・進行判定 (`cplat_internal_service_stop_init` / `dispose` / `on_run_made_progress`)
        - アクティブ トレーサーの参照 (`cplat_internal_service_get_tracer`)
        - ライフサイクル駆動・配送 (`cplat_internal_service_run_callbacks` / `run_lifecycle` / `dispatch_event` / `dispatch_reload`)
        - OS フック (`cplat_internal_service_os_prepare_run` / `install` / `uninstall` / `run` / `notify_ready` / `notify_stopping` / `notify_reloading` / `notify_status`)
- `prod/include_internal/cplat/service/service_linux_events.h`:
    - Linux 専用 (`PLATFORM_LINUX`) のイベント監視スレッド開始・停止・失敗照会・通知ロック・動作段階遷移の内部宣言

### 共通層 (service.c) の設計

- `cplat_service_run()` および `cplat_service_run_console()` の入口で、引数検証 (`def != NULL`、`def->name != NULL && def->name[0] != '\0'`、`def->on_run != NULL`)、アクティブ `def->tracer` の登録、停止抽象 (ロック・条件変数・停止フラグ・進行カウンター) の初期化を行い、出口で確実に解放・リセットします。
- `cplat_service_install()` および `cplat_service_uninstall()` の入口・出口でも、`def->tracer` の一時設定と解除を行い、OS 層から統一的に `cplat_internal_service_get_tracer()` で診断トレースを出力できるようにします。
- `cplat_service_wait_for_stop()` は `CPLAT_WAIT_FOREVER` 指定時に無期限待機 (`cplat_condvar_wait` 相当または `cplat_condvar_timed_wait` の `CPLAT_WAIT_FOREVER` 契約) を行い、`timeout_ms < 0` は即時 `CPLAT_ERR_INVALID_ARGUMENT` を返します。待機前に `s_wait_count` と `s_wait_depth` をアトミック加算 (`CPLAT_MEMORY_ORDER_RELAXED`) し、復帰後に `s_wait_depth` を減算、`s_wait_count` を加算します。
- `cplat_service_install()` の `def->name` には、unit ファイル パス生成時のパス トラバーサルや書式崩れを防ぐため、空文字および不正文字 (`/`, `\`, 制御文字) を拒否する検証を行います。

### Linux 実装 (service_linux.c・service_linux_events.c)

#### libsystemd 直接リンクとビルド設定

- `service_linux.c` および `service_linux_events.c` は `<systemd/sd-daemon.h>`・`<systemd/sd-event.h>`・`<systemd/sd-bus.h>` を直接 `#include` し、`sd_notify()`・`sd_watchdog_enabled()`・`sd_event_*()`・`sd_bus_*()` を直接呼び出します。
- `app/cplat/prod/libsrc/cplat/makepart.mk` の `ifdef PLATFORM_LINUX` ブロックに以下を追加します。
    - `ifndef MAKEFW_SYNC_EVAL` の条件下で `pkg-config --exists libsystemd` を検査し、未導入時は `systemd-devel` または `libsystemd-dev` の導入を促すエラーで停止します (`MAKEFW_SYNC_EVAL` 定義時は Windows ホストでの IntelliSense 同期評価を妨げないため検査をスキップします)。
    - `LIBS += systemd` を追加します (`-lpthread` は `makefw` 経由ですでにリンク済みです)。
- `libcplat.so` は `-fvisibility=hidden` および `--version-script=exports.map` でビルドされるため、`libsystemd` のシンボルは一切エクスポートされず、`DT_NEEDED` に `libsystemd.so.0` が加わるだけです。
- `libsystemd.so.0` は各 Linux マシンの OS 標準ライブラリとして解決するため、`prod/cbin` への同梱対象には含めません。

#### cplat コーディング規範への適合

`app/service-sample` からの移植にあたり、`app/cplat/docs/coding-guideline.md` に適合させます。

- `fopen()` は `cplat_fopen()`、`strerror()` は `cplat_error_message()` (`cplat_internal_error_report_errno()` 経由) へ置き換えます。
- `cplat_service_install()` / `cplat_service_uninstall()` では、`cplat_elevated_process_is_elevated()` で実効 UID の root 権限を確認し、非 root 時は自動昇格を行わずに `CPLAT_ERR_PERMISSION_DENIED` を返します。
- `STATUS=` 通知文の意図的な切り詰め (`snprintf`) は `/* 置換対象外: 通知文の意図的な切り詰め */` コメントを維持します。

#### cplat_service_install() の unit 生成

- 出力先は `/etc/systemd/system/<name>.service` です。
- `run_args` の反映: `NULL` 時は `ExecStart=<exe> run`、非空文字列時は `ExecStart=<exe> <run_args>`、`""` 時は `ExecStart=<exe>` とします (`run_args` に改行 `\r` / `\n` が含まれる場合は `CPLAT_ERR_INVALID_ARGUMENT` で拒否します)。
- 自動再起動 (`disable_auto_restart == 0`): `[Unit]` に `StartLimitIntervalSec=<interval>` と `StartLimitBurst=<burst>`、`[Service]` に `Restart=on-failure` と `RestartSec=<delay>` を出力します。出力前に到達可能性検査 `((burst - 1) * delay) < interval` (オーバーフロー検査込み) を実行し、不適合時は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
- watchdog (`disable_watchdog == 0`): `WatchdogSec=<watchdog_sec>` を出力します (`disable_watchdog != 0` 時は出力しません)。
- OOM killer 保護 (`disable_oom_protection == 0`): `OOMScoreAdjust=-1000` を出力し、`systemctl --version` が systemd 248 以上を返した場合のみ `ManagedOOMPreference=omit` を出力します。
- `def->on_reload != NULL` の場合のみ `ExecReload=/bin/kill -HUP $MAINPID` を出力します。

#### イベント監視スレッドと watchdog 動的 join 上限

- `service_linux_events.c` は専用スレッドで `sd_event` ループを駆動し、(1) `def->on_event != NULL` 時の systemd-logind D-Bus シグナル監視 (`PrepareForSleep` / `PrepareForShutdown` / `SessionNew` / `SessionRemoved`) と delay inhibitor lock 管理 (起動時同期呼び出し・運転中 `sd_bus_call_async`、各期限 10 [s])、(2) `SIGHUP` による設定再読込 (`on_reload == NULL` 時は WARNING トレースを出力して無視)、(3) `WATCHDOG_USEC` 有効時の `on_run` 進行確認 (期限の 1/4 周期) と `WATCHDOG=1` 送信、を担います。
- 動作段階 (`STARTING` / `RUNNING` / `STOPPING`) の確認・更新と `sd_notify()` 送信は同一の通知ロック内で直列化し、`STOPPING` 遷移後の `READY=1`・`RELOADING=1`・`WATCHDOG=1` を抑止します。`on_event`・`on_reload`・トレース出力は通知ロック外で実行します。
- **イベント監視スレッド終了待ち上限の動的調整**: 停止時の `join` 待機上限は既定 `30000` [ms] とします。`sd_watchdog_enabled()` で取得した `watchdog_usec` が 30 [s] (`30000000` [us]) 未満のときは、`min(30000 [ms], watchdog_usec / 1000 [ms])` (下限 `1000` [ms]) へ自動調整します。上限時間内にスレッドが終了しない場合は、解放済みサービス定義やトレーサーへのダングリング参照を防ぐため `cplat_abort_at()` によりプロセスを異常終了させます。
- イベント監視スレッドの必須初期化 (`sd_event_new`、停止監視、watchdog タイマー、`SIGHUP` 登録、通知ロック生成) に失敗した場合、`cplat_service_run()` は `on_start` を呼び出さず `READY=1` も送信せずに `CPLAT_ERR_UNKNOWN` を返します。実行中にイベント ループが異常終了した場合は停止を要求し、コールバックが成功していても `cplat_service_run()` は `CPLAT_ERR_UNKNOWN` を返します。

### Windows 実装 (service_windows.c)

- `cplat_service_prepare_run()` は何もしないで `CPLAT_OK` を返します。
- `cplat_service_install()` は管理者権限を確認 (`cplat_elevated_process_is_elevated()`。未昇格時は自動昇格せず `CPLAT_ERR_PERMISSION_DENIED` を返す) のうえ、`OpenSCManagerU` / `CreateServiceU` (`SERVICE_WIN32_OWN_PROCESS`、`SERVICE_AUTO_START`、実行アカウント `LocalSystem`) で登録します。
    - `binPath` は `run_args` に応じて `"\"<exe>\" run"` / `"\"<exe>\" <run_args>"` / `"\"<exe>\""` を生成します (`run_args` に改行を含む場合は `CPLAT_ERR_INVALID_ARGUMENT`)。
    - `ChangeServiceConfig2U` で説明 (`SERVICE_CONFIG_DESCRIPTION`)、`ChangeServiceConfig2W` で `SERVICE_CONFIG_PRESHUTDOWN_INFO` (既定 30000 [ms])、および `disable_auto_restart == 0` 時に `SERVICE_CONFIG_FAILURE_ACTIONS` (`SC_ACTION_RESTART`、遅延 `restart_delay_sec * 1000` [ms]、`dwResetPeriod = 86400` [s]) と `SERVICE_CONFIG_FAILURE_ACTIONS_FLAG` (`fFailureActionsOnNonCrashFailures = TRUE`) を設定します。
    - Win32 エラーは `ERROR_ACCESS_DENIED` → `CPLAT_ERR_PERMISSION_DENIED`、`ERROR_SERVICE_EXISTS` → `CPLAT_ERR_DUPLICATE_DEFINITION`、`ERROR_SERVICE_MARKED_FOR_DELETE` → `CPLAT_ERR_BUSY` などへ写像し、`cplat_internal_error_report_windows_error()` で直前エラーも記録します。
- `cplat_service_uninstall()` は権限確認後に `OpenSCManagerU` / `OpenServiceU` で対象を開き、最大 30 [s] の停止確認 (50 [ms] ポーリング) 後に `DeleteService` を呼び出します。`ERROR_SERVICE_DOES_NOT_EXIST` は `CPLAT_ERR_NOT_FOUND`、停止確認タイムアウトは `CPLAT_ERR_TIMEOUT` を返します。
- `cplat_service_run()` はステータス ロックを生成し、`StartServiceCtrlDispatcherU` で SCM ディスパッチャーへ接続します。`ServiceMain` 内で `RegisterServiceCtrlHandlerExU`、`SERVICE_START_PENDING`、`cplat_internal_service_run_callbacks()`、`SERVICE_STOPPED` (失敗時は `ERROR_SERVICE_SPECIFIC_ERROR` + `dwServiceSpecificExitCode`) を実行し、コールバック終了コードを `*exit_code_out` へ返します。
- ステータス更新と `SetServiceStatus()` はステータス ロック内で直列化し、失敗時はロック内で `GetLastError()` を保存してロック解放後に診断トレースを出力します。

## テスト基盤とモック設計

### framework/testfw への libmock_systemd.a 追加

`framework/testfw` の既存独立モック (`libmock_libssh.a`、`libmock_syslog.so`) および `framework/testfw/docs/how-to-mock.md` の規則に準拠し、`libsystemd` モックを独立静的ライブラリ `libmock_systemd.a` として追加します。

- `libmock_libc.a` へは統合しません。ワークスペース内の他の全テストへ `-lsystemd` リンク依存が波及することを防ぐためです。
- 配置ファイル:
    - `framework/testfw/include/mock_systemd.h`: `#ifndef _WIN32` ガード付き。`sd-event.h`・`sd-daemon.h`・`sd-bus.h` の対象 25 関数について、`mock_sd_*` C リンケージ宣言、`_IN_OVERRIDE_HEADER_SYSTEMD_SD_{EVENT,DAEMON,BUS}_H` による置換マクロ (呼び出し元 `__FILE__`, `__LINE__`, `__func__` 付き)、`delegate_real_sd_*` 宣言、および `Mock_systemd` クラス (`virtual ~Mock_systemd();`) を定義します。
    - `framework/testfw/include_override/systemd/sd-daemon.h`、`sd-event.h`、`sd-bus.h`: `#ifndef _WIN32` 内で `#include_next <systemd/...>` の後に各 `_IN_OVERRIDE_HEADER_SYSTEMD_*` を定義して `<mock_systemd.h>` を読み込みます。`include_override/` は `TEST_SRCS` のコンパイル時のみ適用され、`<systemd/*.h>` を取り込まない他テストには一切影響しません。
    - `framework/testfw/libsrc/mock_systemd/`:
        - `makefile`: makefw 標準テンプレート
        - `makepart.mk`: `SKIP_BUILD = WINDOWS` を指定し、`ifndef MAKEFW_SYNC_EVAL` 時に `pkg-config --exists libsystemd` を検査します (`-fno-var-tracking-assignments` は上位 `framework/testfw/libsrc/makepart.mk` が自動付与するため個別指定しません)。
        - `mock_systemd.cc`: `Mock_systemd::Mock_systemd()` (全 25 関数の `ON_CALL` → `delegate_real_sd_*` 既定委譲と `TESTFW_REGISTER_MOCK_INSTANCE(_mock_systemd)`)、および唯一の仮想デストラクター定義 `Mock_systemd::~Mock_systemd()` (`TESTFW_UNREGISTER_MOCK_INSTANCE(_mock_systemd)`)。
        - `mock_sd_bus.cc`、`mock_sd_daemon.cc`、`mock_sd_event.cc`: 各 `delegate_real_sd_*` および `mock_sd_*` 実装。`how-to-mock.md` の規則に従い、戻り値中継変数は `mock_ret` を使用し、`WEAK_ATR` は付与しません。
    - `framework/testfw/test/src/mockSystemdTest/`: `SKIP_BUILD = WINDOWS`、`LIBS += mock_systemd systemd` の局所テストを追加し、`Mock_systemd` の登録・多重生成検出・実関数委譲・`include_override/systemd/` 経由の差し替えを単体検証します。

### app/cplat の exportTest・mock_cplat・単体テスト

- **`exportTest.cc`**: `app/cplat/test/src/libcplatTest/exportTest/exportTest.cc` の `CPLAT_EXPORT_TABLE_COMMON` に `<cplat/service/service.h>` の公開 9 関数と正確な関数ポインター型を追加します。
- **`mock_cplat`**: `app/cplat/docs/mock-cplat-guideline.md` に従い、公開 9 関数すべての宣言と `MOCK_CPLAT_LINK_IMPL` を `test/include/mock_cplat.h` に追加し、`test/libsrc/mock_cplat/mock_cplat.cc` の `ON_CALL` 既定委譲と `test/libsrc/mock_cplat/service/mock_cplat_service_*.cc` (`MOCK_WEAK_IMPL`・`resolveSharedSymbolOrExit()`・`mock_ret`) を整備します。
- **単体テスト (`app/cplat/test/src/libcplatTest/service/`)**: `app/service-sample/test/src/cmd/` にあった既存 87 件のテスト資産を新 API 契約へ適合させて移植し、新設オプション・エラー コード検証を追加します。
    - `serviceCommonTest/`: 共通層 (`service.c`) のテスト。停止要求・待機 (`CPLAT_OK` / `CPLAT_ERR_TIMEOUT` / `CPLAT_ERR_INVALID_ARGUMENT` / `CPLAT_NO_WAIT` / `CPLAT_WAIT_FOREVER`)、進行計数、`on_start` / `on_run` / `on_stop` 順序と `*exit_code_out`、イベント・再読込配送、`tracer == NULL` 動作を検証します。
    - `serviceLinuxTest/`: `SKIP_BUILD = WINDOWS`、`LIBS += mock_libc mock_cplat mock_systemd systemd`。`service_linux.c` と `service_linux_events.c` を `TEST_SRCS` とし、`framework/testfw/include_override/systemd/` 経由で `Mock_systemd` を注入して `prepare_run`、unit 生成 (`run_args`・`disable_auto_restart`・`disable_watchdog`・`disable_oom_protection`・`StartLimit` 到達可能性拒否・systemd 248 判定)、権限不足時の `CPLAT_ERR_PERMISSION_DENIED`、`sd_notify` 通知ロック、`WATCHDOG_USEC < 30 [s]` 時の動的 join 上限、D-Bus inhibitor lock、`SIGHUP` を検証します。
    - `serviceWindowsTest/`: Windows 向け (`service_windows.c`) のテスト。SCM 状態遷移、`CreateServiceU` / `DeleteService` / `ChangeServiceConfig2*`、各 `cplat_service_install_options` の反映、権限不足時の `CPLAT_ERR_PERMISSION_DENIED`、イベント配送を検証します。

## app/service-sample の改修仕様

`app/service-sample` は `cplat/service/service.h` の利用方法を示す薄いサンプル アプリケーションへ書き換えます。

### ソース構成と CLI 権限昇格の更新

- `app/service-sample/prod/src/cmd/service-sample/` から重複実装 (`service-sample_internal.h`、`service-sample_linux.c`、`service-sample_linux_events.h`、`service-sample_linux_events.c`、`service-sample_windows.c`) を削除します。
- `service-sample.c` (CLI 層) の構成:
    1. `main()` の最冒頭 (引数解析および `cplat_console_init()` より前) で `cplat_elevated_process_attach_output_pipes(&argc, argv, &attached)` を呼び出します。失敗時は標準出力・標準エラー出力へ頼らず `EXIT_FAILURE` で即終了します。
    2. `cplat_console_init()` および `cplat_argparser_*` で `install` / `uninstall` / `run` / `console` / `-h` / `--help` を解析します。
    3. `run` コマンドの場合、トレーサー開始前に `cplat_service_prepare_run()` を呼び出します。
    4. サンプル用 `cplat_tracer` を生成・設定・開始し、`cplat_service_definition.tracer` へ渡します。
    5. `install` / `uninstall` コマンドでは、`cplat_elevated_process_run_piped(command, NULL, NULL, &elevated_exit_code, &handled)` で必要に応じて Windows UAC 昇格再実行を行います (`handled != 0` なら `elevated_exit_code` を返して終了、Linux 未昇格時はエラー メッセージを出力して `EXIT_FAILURE`)。昇格済みまたは現プロセス継続時は `cplat_service_install(&def, NULL)` / `cplat_service_uninstall(&def)` を呼び出します。
    6. `run` / `console` コマンドでは `cplat_service_run(&def, &exit_code)` / `cplat_service_run_console(&def, &exit_code)` を呼び出し、戻り値が `CPLAT_OK` なら `exit_code`、そうでなければ `EXIT_FAILURE` を返します。
- `service-sample-impl.c` は `cplat_service_definition`、5 コールバック (`on_start` / `on_run` / `on_stop` / `on_event` / `on_reload`)、`while (cplat_service_wait_for_stop(SAMPLE_CYCLE_INTERVAL_MS) == CPLAT_ERR_TIMEOUT)` ループ、および `cplat_service_set_status_text()` の呼び出し例へ書き換えます。
- `app/service-sample/prod/src/cmd/makepart.mk` から `pkg-config --exists libsystemd` 検査および `LIBS += systemd pthread` を削除し、`LIBS += cplat` (`LDFLAGS += -Wl,-z,origin -Wl,-rpath,'$$ORIGIN'` は維持) のみとします。

### テストとドキュメントの整理

- `app/service-sample/test/` から `include/mock_systemd.h`、`include_override/systemd/`、`libsrc/linux/mock_systemd/`、および `src/cmd/linux/service-sampleLinuxTest/`・`src/cmd/windows/service-sampleWindowsTest/` を削除します (基盤テストは `app/cplat/test/src/libcplatTest/service/` へ移設済みのため)。
- `app/service-sample/test/src/cmd/service-sampleTest/` は、薄い CLI 層 (引数解析、`attach_output_pipes`、`run_piped` 呼び出し、`cplat_service_*` へのディスパッチ、トレーサー開閉、サンプル コールバック) の単体テストへ整理します。
- `app/service-sample/docs/service-events.md` と `app/service-sample/docs/systemd-oomkiller-advisory.md` は削除し、`app/service-sample/docs/README.md` および `app/service-sample/prod/src/cmd/service-sample/README.md` から `app/cplat/docs/` の正本へリンクします。

## ドキュメント整備と知見の反映計画

検討・設計を通じて確定した仕様のうち、将来の開発・保守において永続的に参照される内容を選定し、次の正本ドキュメントおよびコード コメントへ反映します。

Table: 正本ドキュメントとコード コメントへの反映計画

| 対象ファイル | 反映する恒久的な仕様・知見 |
| --- | --- |
| `app/cplat/docs/functional-spec/service.md` (新規) | カテゴリ `SERVICE` の要件 ID (`CPLAT-SERVICE-FUNC-NNN` / `QUAL-NNN`、新規 UUID 付き) として、サービス登録・解除、常駐実行・コンソール実行、停止待機・停止要求、OS イベント・設定再読込、watchdog・自動再起動・OOM 保護、適用範囲外 (1 プロセス複数インスタンス非対応、自動権限昇格非実施、一時停止・再開非対応) を関数名や実装方式に依存しない要件粒度で記述します。 |
| `app/cplat/docs/functional-spec/README.md`、`app/cplat/docs/functional-spec-guideline.md` | 機能仕様一覧へ `service` を追加し、カテゴリ主語表へ `SERVICE` (`cplat のサービス機能`) を追加します。 |
| `app/cplat/docs/api-cheatsheet.md` | `cplat/service/service.h` の全公開型・定数・関数と利用例を追記します。 |
| `app/cplat/docs/link-policy.md` | Linux で `libcplat.so` が OS 提供の `libsystemd` (`LIBS += systemd`、`DT_NEEDED: libsystemd.so.0`) を直接リンクする方針、ビルド時の `systemd-devel` / `libsystemd-dev` 要件、実行ファイル同梱対象 (`prod/cbin`) へは含めない理由、および `readelf` / `nm` 確認コマンドを追記します。 |
| `app/cplat/docs/service-events.md` (移設・更新) | `app/service-sample/docs/` から移設し、シンボル名 (`cplat_service_*` / `CPLAT_SERVICE_*`)、共通結果コード、`cplat_service_install_options`、`WATCHDOG_USEC < 30 [s]` 時の動的 join 上限、`StartLimit` 整合性検査、`libsystemd` 直接リンク前提へ内容を更新します。 |
| `app/cplat/docs/systemd-oomkiller-advisory.md` (移設・更新) | `app/service-sample/docs/` から移設し、`cplat_service_install()` および `disable_oom_protection` オプションの説明へ更新します。 |
| `app/cplat/docs/README.md` | 機能構成表と詳細仕様リンク一覧へ `service`・`service-events.md`・`systemd-oomkiller-advisory.md` を追加します。 |
| `app/cplat/prod/include/cplat/service/service.h` (Doxygen コメント) | `cplat_service_prepare_run()` の呼び出し順序契約 (スレッド生成前・子プロセスへの `SIGHUP` マスク継承)、`on_run` と watchdog の進行判定契約、`cplat_service_wait_for_stop()` の `CPLAT_ERR_TIMEOUT` ループ慣用句、`StartLimit` 到達可能性検査の条件、および `on_event` / `on_reload` の別スレッド呼び出し・応答期限契約を宣言側へ明記します。 |
| `framework/testfw/docs/how-to-mock.md` (必要最小限の追記) | 独立ライブラリ モックの参照例として `include/mock_systemd.h`、`include_override/systemd/`、`libsrc/mock_systemd/` (`SKIP_BUILD = WINDOWS`) を必要に応じて追記します。 |

## 検証計画と受入条件

### Linux 上での局所検証

実装完了後、Linux 環境で影響範囲 3 か所の局所検証と機械的検査を実施します。

1. `framework/testfw`:
    - `make -C framework/testfw` で `libmock_systemd.a` を含むライブラリをビルドし、新規 `override` ヘッダー導入に伴う依存更新を確認します。
    - `make -C framework/testfw/test/src/mockSystemdTest test` (および `framework/testfw` の既存局所テスト) を実行し全件通過を確認します。
2. `app/cplat`:
    - `make -C app/cplat` で `libcplat.so` をビルドし、`readelf -d app/cplat/prod/lib/libcplat.so` で `libsystemd.so.0` の `DT_NEEDED` と、`nm -D --defined-only app/cplat/prod/lib/libcplat.so` で `sd_*` 非エクスポートを確認します。
    - `make -C app/cplat test` で `exportTest`、`serviceCommonTest`、`serviceLinuxTest` を含む `app/cplat` 全単体テストの通過を確認します。
3. `app/service-sample`:
    - `make -C app/service-sample` および `make -C app/service-sample test` で書き換え後のビルドと全単体テストの通過を確認します。
4. 警告・仕様・文書検査:
    - ビルド・テスト対象範囲で内容がある `.warn` ファイルが存在しないこと (または解消済みであること) を確認します。
    - ワークスペース ルートで `python3 app/general/bin/check_functional_spec.py` を実行し、要件 ID と UUID の整合を確認します。
    - 作成・変更したすべての Markdown ファイルへ `python3 framework/docsfw/bin/text_style_jp.py --dry-run` を実行して意味変化がないことを確認後、`--in-place` を適用します。

### CI・ユーザー環境へ委ねる確認範囲

次の項目は Linux (WSL) ローカル検証の対象外とし、成果物提示時に未確認事項として明示したうえで CI (`ol8`/`ol9`/`ol10` + Windows) またはユーザー環境で確認します。

- ワークスペース全体の `make test` (`framework/testfw` の他 app への横断回帰確認)
- Windows/MSVC でのビルド (`framework/testfw` の `SKIP_BUILD = WINDOWS` 動作、`libcplat.dll` / `libcplat.lib`、`service-sample.exe`)
- Windows 単体テスト (`exportTest`、`serviceCommonTest`、`serviceWindowsTest`、`service-sampleTest`)
- Windows SCM 実機および systemd 有効 Linux 環境での `install` / `start` / `reload` / `stop` / `uninstall` の実機動作確認

### 受入条件

- `framework/testfw` で `libmock_systemd.a` と `mockSystemdTest` が Linux 上で警告・失敗なくビルドおよびテスト通過すること。
- `app/cplat` にカテゴリ `service` の機能仕様・公開ヘッダー・実装・`LIBS += systemd` 直接リンク・`exportTest`・`mock_cplat`・単体テスト・移設文書および関連正本ドキュメントが整合して整備され、Linux 上で全テスト・`check_functional_spec.py`・`text_style_jp.py`・`.warn` 確認を通過すること。
- `app/service-sample` が `cplat/service/service.h` と `cplat_elevated_process_run_piped()` 利用へ書き換えられ、旧重複実装・旧 `mock_systemd`・重複テスト・旧仕様文書 2 件が削除され、Linux 上でビルド・全テスト・`.warn` 確認を通過すること。
- ワークスペース全体テストと Windows 側確認項目が未確認事項として整理され、成果物とともに提示されること。
