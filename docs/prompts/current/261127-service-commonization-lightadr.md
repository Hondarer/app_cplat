<!--
生成元: agree-before-implement スキル
文書種別: 簡易 ADR (lightadr)
最終更新日時: 2026-11-28 08:42:00 JST
最終更新エージェント: agy / セッション ID: 73c19cdb-9b01-4b7c-831a-02f92420ab83
稼働 OS: Linux (WSL)
上流文書: `261127-service-commonization-overview.md` (全体管理文書)、`261127-service-commonization-discussion.md` (検討用 Markdown)。いずれも削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。上流文書の全体管理文書が存在する場合は、その再開手順に従って進行位置を確認します。存在しない場合は、この文書を単体の決定事項の記録として扱います。この文書は正式な ADR ではありません。正式な ADR への転記はユーザーが必要時に行うため、エージェントは依頼なしに転記や正式な ADR の作成・更新を行わないでください。最終更新エージェントのセッション情報や稼働 OS・最終更新日時が参照可能であれば再開時のコンテキストや環境差分の把握に利用し、別エージェントや別環境・別 OS からの引き継ぎ、担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。
-->

# service-sample の共通化可能部分の cplat 取り込み 簡易 ADR

ファイル名: `261127-service-commonization-lightadr.md`

この文書は、検討で得られた事実、ユーザーの選択とコメント、およびそれらに基づく決定事項を、決定 1 件ごとにまとめた簡易 ADR (Architecture Decision Record: 設計判断の記録) です。  
正式な ADR ではなく、対案の比較や決定の影響の評価は記載していません。  
各決定の「決定」は、方針が変わるたびに書き換え、常に最新の決定だけを示します。変更の推移は「経緯」に時系列で記録します。

## 決定事項一覧

Table: 簡易 ADR の決定事項一覧

| 決定 ID | 題名 | 最終更新日 | 源泉 |
| --- | --- | --- | --- |
| [LADR-001](#ladr-001-検討対象と検討文書の配置先) | 検討対象と検討文書の配置先 | 2026-11-27 | 初回依頼 (対話) |
| [LADR-002](#ladr-002-機能カテゴリの配置と命名) | 機能カテゴリの配置と命名 | 2026-11-27 | [Q-001](261127-service-commonization-discussion.md#q-001-機能カテゴリの配置と命名) |
| [LADR-003](#ladr-003-api-の状態モデル) | API の状態モデル | 2026-11-27 | [Q-002](261127-service-commonization-discussion.md#q-002-api-の状態モデル) |
| [LADR-004](#ladr-004-cli-エントリ-ポイントの責務境界) | CLI エントリ ポイントの責務境界 | 2026-11-27 | [Q-003](261127-service-commonization-discussion.md#q-003-cli-エントリ-ポイントの取り込み範囲) |
| [LADR-005](#ladr-005-linux-での-libsystemd-直接リンクとモック配置) | Linux での libsystemd 直接リンクとモック配置 | 2026-11-28 | [Q-004](261127-service-commonization-discussion.md#q-004-linux-での-libsystemd-への依存方式)、[Q-014](261127-service-commonization-discussion.md#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存)、[Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) |
| [LADR-006](#ladr-006-診断トレースの出力方式と-cplat_tracer-の所有権) | 診断トレースの出力方式と cplat_tracer の所有権 | 2026-11-27 | [Q-005](261127-service-commonization-discussion.md#q-005-診断トレースの出力方式と所有権) |
| [LADR-007](#ladr-007-サービス操作-api-と停止待機-api-の戻り値規約) | サービス操作 API と停止待機 API の戻り値規約 | 2026-11-27 | [Q-006](261127-service-commonization-discussion.md#q-006-サービス操作-api-の戻り値規約)、[Q-007](261127-service-commonization-discussion.md#q-007-停止待機-api-の戻り値とタイムアウト仕様)、[Q-017](261127-service-commonization-discussion.md#q-017-公開-api-での-cplat_error-出力引数の要否) |
| [LADR-008](#ladr-008-登録と解除における自動権限昇格の責務境界) | 登録と解除における自動権限昇格の責務境界 | 2026-11-27 | [Q-008](261127-service-commonization-discussion.md#q-008-登録と解除における自動権限昇格の責務) |
| [LADR-009](#ladr-009-サービス登録パラメーターのカスタマイズ方式) | サービス登録パラメーターのカスタマイズ方式 | 2026-11-27 | [Q-009](261127-service-commonization-discussion.md#q-009-サービス登録パラメーターのカスタマイズ範囲)、[Q-015](261127-service-commonization-discussion.md#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限)、[Q-016](261127-service-commonization-discussion.md#q-016-install-時の-startlimit-と再起動遅延の整合性検査) |
| [LADR-010](#ladr-010-linux-の-sighup-事前ブロックの公開-api) | Linux の SIGHUP 事前ブロックの公開 API | 2026-11-27 | [Q-010](261127-service-commonization-discussion.md#q-010-linux-の-sighup-事前ブロックの公開設計) |
| [LADR-011](#ladr-011-service-sample-本体の改修と仕様文書の移設範囲) | service-sample 本体の改修と仕様文書の移設範囲 | 2026-11-27 | [Q-011](261127-service-commonization-discussion.md#q-011-service-sample-本体の改修と資産移行の範囲)、[Q-018](261127-service-commonization-discussion.md#q-018-改修後の-service-sample-における-windows-権限昇格方式)、[Q-019](261127-service-commonization-discussion.md#q-019-移設後の旧ドキュメント-ファイルの扱い) |
| [LADR-012](#ladr-012-並行検討中の-account-session-案件との独立性) | 並行検討中の account-session 案件との独立性 | 2026-11-28 | [Q-012](261127-service-commonization-discussion.md#q-012-並行検討中の-account-session-案件との調整)、[Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) |
| [LADR-013](#ladr-013-検証範囲と-windows-確認の扱い) | 検証範囲と Windows 確認の扱い | 2026-11-28 | [Q-013](261127-service-commonization-discussion.md#q-013-検証範囲と-windows-確認の扱い)、[Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) |

## LADR-001 検討対象と検討文書の配置先

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

`app/service-sample` の共通化可能部分を `app/cplat` に取り込むにあたり、計画策定の進行方式と検討用ファイル群の配置先を確定する必要がありました。

### 得られた事実

- `app/service-sample` は親リポジトリ (`c-modernization-kit`) にあり、Windows SCM と Linux systemd に対応したクロスプラットフォーム サービス実装と単体テスト (87 件)・仕様文書を持つ。
- `app/cplat` はサブモジュールであり、`app/cplat/docs/prompts/current/` に検討中案件の Markdown 文書群を配置するディレクトリが存在する。

### 決定

`app/service-sample` の共通化可能部分を `app/cplat` に取り込む計画を `agree-before-implement` スキルで立案し、関連ファイル群 (`overview.md`、`discussion.md`、`lightadr.md`、および後続の `design.md`・必要時の `backlog.md`) を `app/cplat/docs/prompts/current/` に配置します。

### 経緯

Table: LADR-001 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 08:00 | 初回依頼 (対話) | `app/service-sample` の共通化可能部分を `cplat` に取り込む計画を `agree-before-implement` スキルで立案し、`app/cplat/docs/prompts/current` へファイル群を作成する | なし | 新規に決定 |

## LADR-002 機能カテゴリの配置と命名

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

`app/service-sample` から取り込むクロスプラットフォーム サービス機能を、`app/cplat` のどの機能カテゴリとシンボル命名体系で配置するかを決める必要がありました。

### 得られた事実

- `app/cplat/AGENTS.md` および `app/cplat/docs/functional-spec-guideline.md` では、独立した目的を持つ機能カテゴリごとに `prod/include/cplat/<cat>/`、`prod/libsrc/cplat/<cat>/`、`docs/functional-spec/<cat>.md`、`CPLAT_<CAT>` (Doxygen) を 1 対 1 で対応付ける。
- `app/cplat/prod/libsrc/cplat/exports.map` は `cplat_*` および `_cplat_*` のみを公開エクスポート対象としている。

### 決定

新規機能カテゴリ `service` を追加し、公開ヘッダー `prod/include/cplat/service/service.h` (`prod/include/cplat.h` に含める)、実装 `prod/libsrc/cplat/service/`、機能仕様 `docs/functional-spec/service.md` (要件 ID `CPLAT-SERVICE-*`)、Doxygen グループ `CPLAT_SERVICE`、シンボル接頭辞 `cplat_service_*` / `CPLAT_SERVICE_*` として配置します。

### 経緯

Table: LADR-002 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-001](261127-service-commonization-discussion.md#q-001-機能カテゴリの配置と命名) | A: 新規カテゴリ `service` を追加し、`cplat/service/service.h`・`docs/functional-spec/service.md`・`cplat_service_*` として配置する | なし | 新規に決定 |

## LADR-003 API の状態モデル

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

サービス実行時の停止抽象や状態通知などの状態管理を、プロセス グローバル API とハンドル指向 API のどちらで提供するかを決める必要がありました。

### 得られた事実

- 現行 `service-sample.h` はハンドルなしのプロセス グローバル API で構成され、Windows の `SERVICE_WIN32_OWN_PROCESS` および Linux の systemd `Type=notify`・`SIGHUP`・`NOTIFY_SOCKET` はプロセス単位の資源である。
- `app/cplat/prod/include/cplat/runtime/shutdown.h` もプロセス グローバル API として設計されている。

### 決定

プロセス グローバル API を採用し、1 プロセスでのマルチ インスタンス実行は対象外とします。停止待機・停止要求・状態通知の API はハンドルを取らず、実行開始時および終了時に内部状態を安全に初期化・解放します。

### 経緯

Table: LADR-003 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-002](261127-service-commonization-discussion.md#q-002-api-の状態モデル) | A: プロセス グローバル API とする | 1 つのプロセスは、マルチ インスタンスには対応しなくてよい。 | 新規に決定 |

## LADR-004 CLI エントリ ポイントの責務境界

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

`service-sample.c` の `main()` にあるコマンド ライン引数解析 (`install` / `uninstall` / `run` / `console` / `--help`) とサブコマンド分岐を `cplat` へ取り込むか、利用側へ残すかを決める必要がありました。

### 得られた事実

- `service-sample.c` の `main()` は `cplat_argparser_*` で位置引数 `command` を解析し、`extern const svc_definition g_service_def` を参照して各操作へ分岐している。
- `libcplat` は共有ライブラリであり、エントリ ポイント `main()` や利用側の未解決シンボルを持てない。

### 決定

`cplat` には個別のサービス操作 API (`install` / `uninstall` / `run` / `run_console` / `prepare_run` / 停止抽象 / 状態通知) のみを取り込み、CLI 引数解析とディスパッチ (`main()`) は利用側 (`app/service-sample` 等) に残します。

### 経緯

Table: LADR-004 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-003](261127-service-commonization-discussion.md#q-003-cli-エントリ-ポイントの取り込み範囲) | A: 個別の操作 API のみを `cplat` へ取り込み、CLI 解析とディスパッチ (`main()`) は利用側に残す | なし | 新規に決定 |

## LADR-005 Linux での libsystemd 直接リンクとモック配置

決定者: Tetsuo Honda  
最終更新日: 2026-11-28

### 背景と課題

Linux のサービス実装が使用する `libsystemd` (`sd_notify`、`sd_watchdog_enabled`、`sd_event_*`、`sd_bus_*`) への依存方式、ビルド時のヘッダー参照方針、および単体テスト用モックの配置先を、`libcplat.so` の単一ライブラリ方針と `framework/testfw` の構成を踏まえて決める必要がありました。

### 得られた事実

- `app/cplat/docs/link-policy.md` は `cplat` が単一の動的ライブラリ (`libcplat.so` / `libcplat.dll`) のみを生成すると定めている。
- `app/cplat/prod/libsrc/cplat/makepart.mk` の現行 Linux リンク先は `cjson_app`、`zlib_app`、`crypto_app`、`dl`、`stdc++` である。
- Oracle Linux 8.10 環境には `systemd-devel-239-82.0.13.el8_10.19.x86_64` (`/usr/include/systemd/*.h` および `/lib64/libsystemd.so.0`) が導入済みであり、CI の `ol8`/`ol9`/`ol10` コンテナーでも既存 `app/service-sample` のビルドにより導入済みである。
- `framework/testfw` では全テストがリンクする `libmock_libc.a` とは別に、`libmock_libssh.a` や `libmock_syslog.so` (`SKIP_BUILD = WINDOWS`) のように特定ライブラリ向けモックを独立ライブラリとして配置している。

### 決定

Linux の `libcplat.so` へ `LIBS += systemd` で `libsystemd` を直接リンクし、ビルド時に `<systemd/sd-daemon.h>`・`<systemd/sd-event.h>`・`<systemd/sd-bus.h>` を `#include` します (`app/cplat/prod/libsrc/cplat/makepart.mk` に `MAKEFW_SYNC_EVAL` 未定義時の `pkg-config --exists libsystemd` 検査を追加し、`app/cplat/docs/link-policy.md` に OS 提供の `libsystemd.so.0` 直接リンクを追記します)。`libsystemd` の単体テスト用モックは、`framework/testfw` に独立静的ライブラリ `libmock_systemd.a` (`include/mock_systemd.h`、`include_override/systemd/{sd-bus.h,sd-daemon.h,sd-event.h}`、`libsrc/mock_systemd/`、`SKIP_BUILD = WINDOWS`) として配置し、C 標準ライブラリ モックと同じ `include_override` マクロ置換方式でテストします。

### 経緯

Table: LADR-005 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-004](261127-service-commonization-discussion.md#q-004-linux-での-libsystemd-への依存方式) | A: `libcplat.so` 内で `libsystemd.so.0` を実行時に `dlopen()` / `dlsym()` で動的ロードする | なし | 新規に決定 |
| 2026-11-27 23:10 | [Q-014](261127-service-commonization-discussion.md#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存) | B: ビルド時は `<systemd/*.h>` を `#include` し、実行時リンクのみ外す | 現在の環境に入っているようであれば、利用してよいと考えている。testfw にも mock を追加し、C 標準ライブラリ同等でよい。libsystemd-dev は入っていないと思うが問題あるか。 | 環境確認と `testfw` モック整合を Q-020 へ引き継ぎ |
| 2026-11-28 08:35 | [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) | C: `Q-004` を「`libcplat.so` への `libsystemd` 直接リンク」へ変更し、`framework/testfw` に `libmock_systemd.a` + `include_override/systemd/` を追加して C 標準ライブラリと同一のマクロ置換でテストする | リンク対象ライブラリが増えたとして、libsystemd そのものは各 Linux マシンに存在するので、問題にならないと判断。 | 決定を直接リンク (`LIBS += systemd`) および `framework/testfw` の `libmock_systemd.a` 配置へ改訂 |

## LADR-006 診断トレースの出力方式と cplat_tracer の所有権

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

サービス層から非同期に発生する診断トレースの出力方式と、`cplat_tracer` の生成・設定・破棄の所有権を決める必要がありました。

### 得られた事実

- 現行 `service-sample.c` は独自のグローバル `cplat_tracer *s_tracer` を生成・管理している。
- `app/cplat` の既存モジュールは内部にグローバルな `cplat_tracer` を自動生成せず、`cplat_tracer_write(NULL, ...)` は安全な no-op として定義されている。

### 決定

サービス定義構造体 (`cplat_service_definition`) に任意の `cplat_tracer *tracer` フィールドを設け、生成・設定・開始・破棄の所有権を利用側へ委ねます。`NULL` 指定時はサービス層からのトレース出力を省略します。

### 経緯

Table: LADR-006 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-005](261127-service-commonization-discussion.md#q-005-診断トレースの出力方式と所有権) | A: サービス定義構造体に任意の `cplat_tracer *tracer` を設け、所有権を利用側に委ねる | なし | 新規に決定 |

## LADR-007 サービス操作 API と停止待機 API の戻り値規約

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

`cplat` のサービス操作 API (`install` / `uninstall` / `prepare_run` / `run` / `run_console`) および停止待機 API (`wait_for_stop`) の戻り値規約と、コールバック終了コード・タイムアウトの扱いを決める必要がありました。

### 得られた事実

- `app/cplat/docs/coding-guideline.md` は公開 API の戻り値を共通結果コード (`CPLAT_OK` / `CPLAT_ERR_*`) へ統一している。
- `app/cplat/prod/include/cplat/base/wait.h` は `CPLAT_NO_WAIT` (0) および `CPLAT_WAIT_FOREVER` (`INT_MAX`) を定義している。
- 現行 `service-sample` は `EXIT_SUCCESS` / `EXIT_FAILURE` / コールバック終了コードおよび 0/1 規約を使っていた。

### 決定

すべてのサービス操作 API の戻り値を共通結果コード (`CPLAT_OK` / `CPLAT_ERR_*`) に統一し、`run` / `run_console` はコールバックの終了コードを出力引数 `int *exit_code_out` (NULL 可) で返します。公開 API に `cplat_error *detail_out` 引数は設けず、戻り値、OS エラー時のスレッド ローカル直前値 (`cplat_error_get_last()`)、および `def->tracer` への診断トレースで詳細を伝えます。停止待機 API (`cplat_service_wait_for_stop`) も共通結果コードと `wait.h` に準拠し、停止要求到達時は `CPLAT_OK`、タイムアウト時は `CPLAT_ERR_TIMEOUT`、負値や未初期化等の異常時は `CPLAT_ERR_*` を返し、`CPLAT_NO_WAIT` と `CPLAT_WAIT_FOREVER` に対応します。

### 経緯

Table: LADR-007 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-006](261127-service-commonization-discussion.md#q-006-サービス操作-api-の戻り値規約) | A: 全サービス操作 API の戻り値を共通結果コードに統一し、`run` / `run_console` のコールバック終了コードは `int *exit_code_out` (NULL 可) で返す | なし | 新規に決定 |
| 2026-11-27 19:28 | [Q-007](261127-service-commonization-discussion.md#q-007-停止待機-api-の戻り値とタイムアウト仕様) | A: `cplat` の共通結果コードと `wait.h` に合わせる (`CPLAT_OK` / `CPLAT_ERR_TIMEOUT` / `CPLAT_ERR_*`、`CPLAT_NO_WAIT` / `CPLAT_WAIT_FOREVER` 対応) | なし | 決定を補足 |
| 2026-11-27 23:10 | [Q-017](261127-service-commonization-discussion.md#q-017-公開-api-での-cplat_error-出力引数の要否) | A: `detail_out` 引数は設けず、戻り値・`cplat_error_get_last()`・`def->tracer` で詳細を伝える | なし | 決定を補足 |

## LADR-008 登録と解除における自動権限昇格の責務境界

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

`cplat` のサービス登録 (`install`)・解除 (`uninstall`) API 内部で `cplat_elevated_process_*` による自動権限昇格を行うか否かを決める必要がありました。

### 得られた事実

- 現行 `service-sample` の `svc_os_install` / `svc_os_uninstall` は `"install"` / `"uninstall"` 固定引数で `cplat_elevated_process_run_if_needed()` を呼んでいる。
- `app/cplat/prod/include/cplat/runtime/elevated_process.h` には、`run_if_needed` (親コンソール再接続) の Windows での間欠的な `ERROR_INVALID_HANDLE` 障害と、代替の `run_piped` / `run_with_result` が文書化されている。

### 決定

`cplat` の `install` / `uninstall` API 内部では自動権限昇格を行わず、権限不足時は `CPLAT_ERR_PERMISSION_DENIED` を返します。UAC 昇格再起動が必要な場合は、利用側の CLI 層が `cplat_elevated_process_*` で実施します。

### 経緯

Table: LADR-008 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-008](261127-service-commonization-discussion.md#q-008-登録と解除における自動権限昇格の責務) | A: `cplat` の `install` / `uninstall` API 内部では自動昇格を行わず `CPLAT_ERR_PERMISSION_DENIED` を返す | なし | 新規に決定 |

## LADR-009 サービス登録パラメーターのカスタマイズ方式

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

`install` 時に OS へ登録するサービス設定 (起動引数、自動再起動、StartLimit、watchdog、OOM 保護、PRESHUTDOWN 猶予) を初回実装でどこまでカスタマイズ可能にするかを決める必要がありました。

### 得られた事実

- 現行 `service-sample` は `run` 起動引数、`RestartSec=5`、`StartLimitIntervalSec=60`、`StartLimitBurst=5`、`WatchdogSec=30`、OOM 保護 (`OOMScoreAdjust=-1000` / `ManagedOOMPreference=omit`)、`SVC_PRESHUTDOWN_TIMEOUT_MS=30000` を定数固定している。

### 決定

任意の `cplat_service_install_options` 構造体 (`NULL` またはゼロ初期化で現行 `service-sample` と同じ既定値を使用) を設け、起動引数・自動再起動・StartLimit・watchdog (`disable_watchdog` を含む)・OOM 保護・PRESHUTDOWN 猶予を上書き可能にします。Linux の `install` で自動再起動が有効な場合、`((burst - 1) * restart_delay_sec) >= interval_sec` (または乗算オーバーフロー) であれば `CPLAT_ERR_INVALID_ARGUMENT` で拒否します。Linux の `run` におけるイベント監視スレッド終了待ち上限は既定 30 [s] とし、`WATCHDOG_USEC` が 30 [s] 未満のときは `min(30000 ms, watchdog_usec / 1000)` (下限 1000 [ms]) へ自動調整します。

### 経緯

Table: LADR-009 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-009](261127-service-commonization-discussion.md#q-009-サービス登録パラメーターのカスタマイズ範囲) | A: 任意の `cplat_service_install_options` 構造体を設け、起動引数・自動再起動・StartLimit・watchdog・OOM 保護・PRESHUTDOWN 猶予を上書き可能にする | なし | 新規に決定 |
| 2026-11-27 23:10 | [Q-015](261127-service-commonization-discussion.md#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限) | A: `disable_watchdog` フラグを設け、`WATCHDOG_USEC` が 30 [s] 未満のときに終了待ち上限を `min(30000 ms, watchdog_usec / 1000)` (下限 1000 [ms]) へ自動調整する | なし | 決定を補足 |
| 2026-11-27 23:10 | [Q-016](261127-service-commonization-discussion.md#q-016-install-時の-startlimit-と再起動遅延の整合性検査) | A: 自動再起動有効時、`((burst - 1) * restart_delay_sec) >= interval_sec` (または乗算オーバーフロー) なら `CPLAT_ERR_INVALID_ARGUMENT` で拒否する | なし | 決定を補足 |

## LADR-010 Linux の SIGHUP 事前ブロックの公開 API

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

Linux で `sd_event_add_signal(SIGHUP)` が要求する「スレッド生成前の `SIGHUP` ブロック」(`svc_os_prepare_run()` 相当) を、`cplat` の公開 API として提供するかを決める必要がありました。

### 得られた事実

- `sd_event_add_signal(3)` は対象シグナルがプロセス内の全スレッドでブロックされていることを前提とする。
- `service-sample` ではトレーサー開始などのスレッド生成より前に `svc_os_prepare_run()` を呼んで `SIGHUP` をブロックし、Windows では no-op としている。

### 決定

`cplat_service_prepare_run()` をクロスプラットフォーム公開 API として提供します (Linux で `SIGHUP` をブロックし、Windows では `CPLAT_OK` を返す no-op)。

### 経緯

Table: LADR-010 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-010](261127-service-commonization-discussion.md#q-010-linux-の-sighup-事前ブロックの公開設計) | A: `cplat_service_prepare_run()` をクロスプラットフォーム公開 API として提供する | なし | 新規に決定 |

## LADR-011 service-sample 本体の改修と仕様文書の移設範囲

決定者: Tetsuo Honda  
最終更新日: 2026-11-27

### 背景と課題

本案件の実装フェーズにおいて、`app/cplat` への追加だけでなく、`app/service-sample` 本体の書き換えおよび仕様文書 (`service-events.md`、`systemd-oomkiller-advisory.md`) の移設まで一括実施するかを決める必要がありました。

### 得られた事実

- `app/service-sample` は親リポジトリ管理下にあり、87 件の単体テスト、`mock_systemd`、`docs/service-events.md`、`docs/systemd-oomkiller-advisory.md` を保持している。

### 決定

本案件内で、(1) `app/cplat` への機能追加・テスト移植・`exportTest` / `mock_cplat` / 文書更新、(2) `app/service-sample` の新 API 利用への書き換え (CLI 層の Windows 権限昇格は `cplat_elevated_process_run_piped()` + `cplat_elevated_process_attach_output_pipes()` へ更新) と重複資産削除、(3) `service-events.md` と `systemd-oomkiller-advisory.md` の `app/cplat/docs/` への移設および `app/service-sample/docs/` の旧 2 ファイル削除 (README から移設先へリンク) まで一括実施します。

### 経緯

Table: LADR-011 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-011](261127-service-commonization-discussion.md#q-011-service-sample-本体の改修と資産移行の範囲) | A: `app/cplat` への追加・テスト移植、`app/service-sample` の書き換え、仕様文書の `app/cplat/docs/` 移設を一括実施する | なし | 新規に決定 |
| 2026-11-27 23:10 | [Q-018](261127-service-commonization-discussion.md#q-018-改修後の-service-sample-における-windows-権限昇格方式) | A: `app/service-sample` の CLI 層で `cplat_elevated_process_run_piped()` と `cplat_elevated_process_attach_output_pipes()` を使った昇格再実行へ更新する | なし | 決定を補足 |
| 2026-11-27 23:10 | [Q-019](261127-service-commonization-discussion.md#q-019-移設後の旧ドキュメント-ファイルの扱い) | A: `app/service-sample/docs/` の旧 2 ファイルは削除し、README から `app/cplat/docs/` の正本へリンクする | なし | 決定を補足 |

## LADR-012 並行検討中の account-session 案件との独立性

決定者: Tetsuo Honda  
最終更新日: 2026-11-28

### 背景と課題

同じディレクトリで並行検討中の `cplat-account-session-discussion.md` と本案件の依存・調整方針を決める必要がありました。

### 得られた事実

- `cplat-account-session-discussion.md` は未回答・検討中であり、`systemd-logind` (`libsystemd`) やセッション変化イベントを扱っている。
- サービス側のイベント監視は Linux の `sd_event` ループおよび Windows の `ServiceCtrlHandler` と一体で自己完結できる。

### 決定

本案件は `cplat-account-session` に依存せず自己完結で設計・実装します (`service-sample` 相当のセッション イベントを含む)。`libcplat.so` が `libsystemd` を直接リンクし、`framework/testfw` に `libmock_systemd.a` を配置する方針へ変更されたため、`libsystemd` 動的ローダー共有の残課題 (`BL-001`) は解消しました。

### 経緯

Table: LADR-012 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-012](261127-service-commonization-discussion.md#q-012-並行検討中の-account-session-案件との調整) | A: `cplat-account-session` に依存せず自己完結で設計・実装し、将来の共有余地があれば `backlog.md` へ記録する | なし | 新規に決定 |
| 2026-11-28 08:35 | [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) | C: `libcplat.so` への `libsystemd` 直接リンクと `framework/testfw` への `libmock_systemd.a` 追加へ変更 | リンク対象ライブラリが増えたとして、libsystemd そのものは各 Linux マシンに存在するので、問題にならないと判断。 | 動的ローダーを作成しない方針に伴い `BL-001` の前提を解消 |

## LADR-013 検証範囲と Windows 確認の扱い

決定者: Tetsuo Honda  
最終更新日: 2026-11-28

### 背景と課題

稼働 OS が `Linux (WSL)` である制約および `framework/testfw` の変更範囲を踏まえ、実装完了後の検証範囲と Windows 側確認の扱いを決める必要がありました。

### 得られた事実

- 本環境 (`Linux (WSL)`) では Linux のビルド・テスト・`.warn`・`check_functional_spec.py`・`text_style_jp.py` を直接実行できるが、Windows/MSVC ビルドおよび Windows SCM 実機確認は実行できない。
- `framework/testfw` への変更は独立静的ライブラリ `libmock_systemd.a` と新規 `include_override/systemd/` に局所化され、既存の `libmock_libc.a` および既存 `include_override/*.h` は変更しない。

### 決定

Linux 上で影響する 3 か所 (`framework/testfw` のビルドおよび `mock_systemd` 局所テスト、`app/cplat` のビルド・全単体テスト、`app/service-sample` のビルド・全単体テスト)、内容がある `.warn` ファイルの確認、`check_functional_spec.py`、`text_style_jp.py` を完了します。ワークスペース全体テストおよび Windows 側のビルド・テスト・実機確認は未確認事項として明示したうえでユーザー環境または CI で確認します。

### 経緯

Table: LADR-013 の決定経緯

| 日時 (JST) | 源泉 | ユーザーの選択 | ユーザーのコメント | 決定への反映 |
| --- | --- | --- | --- | --- |
| 2026-11-27 19:28 | [Q-013](261127-service-commonization-discussion.md#q-013-検証範囲と-windows-確認の扱い) | A: Linux 上で対象 app の検証を完了し、Windows 側は未確認事項として明示してユーザー環境または CI で確認する | なし | 新規に決定 |
| 2026-11-28 08:35 | [Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) | A: 影響する範囲の局所検証 (`framework/testfw`・`app/cplat`・`app/service-sample`) を実施し、ワークスペース全体テストは CI に委ねる | なし | `framework/testfw` 変更に伴う Linux 局所検証範囲を補足 |
