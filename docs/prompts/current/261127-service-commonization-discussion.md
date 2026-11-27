<!--
生成元: agree-before-implement スキル
文書種別: 検討用 Markdown (discussion)
最終更新日時: 2026-11-27 23:13:00 JST
最終更新エージェント: agy / セッション ID: a422744b-85f9-4bdc-914d-195c5ca8a120
稼働 OS: Linux (WSL)
上流文書: `261127-service-commonization-overview.md` (全体管理文書)。削除されている場合があります。
下流文書: `261127-service-commonization-lightadr.md` (簡易 ADR)、`261127-service-commonization-design.md` (最終仕様)、`261127-service-commonization-backlog.md` (残課題一覧)。未作成の場合や、削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。ユーザーが編集した最新版を読み直し、上流文書の全体管理文書が存在する場合は、その再開手順に従って作業 WBS の `← **CURRENT**` の項目から再開します。存在しない場合は、各 Q の **Status** から未回答と未解決の Q を特定し、扱いをユーザーに確認してください。この文書には Q&A の本文だけを置き、新しい Q は末尾へ追記します。作業 WBS、質問一覧、観点別の確認などの管理情報をこの文書へ追加しないでください。最終更新エージェントのセッション情報や稼働 OS・最終更新日時が参照可能であれば再開時のコンテキストや環境差分の把握に利用し、別エージェントや別環境・別 OS からの引き継ぎ、担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。
-->

# service-sample の共通化可能部分の cplat 取り込み 検討

ファイル名: `261127-service-commonization-discussion.md`

## 明確化のための確認項目

### Q-001 機能カテゴリの配置と命名

**Status**

解決済み

**Question**

`app/service-sample` から取り込むクロスプラットフォーム サービス機能を、`app/cplat` のどの機能カテゴリと命名体系で配置しますか。

**Context**

- **経緯と背景**: `app/cplat/AGENTS.md` および `app/cplat/docs/functional-spec-guideline.md` では、利用側から見て独立した目的を持つ機能カテゴリごとに、公開ヘッダー (`prod/include/cplat/<cat>/`)、実装 (`prod/libsrc/cplat/<cat>/`)、機能仕様 (`docs/functional-spec/<cat>.md`)、Doxygen グループ (`CPLAT_<CAT>`) を 1 対 1 で設けます。現状の `app/service-sample` は `svc_*` 接頭辞を使用していますが、`app/cplat/prod/libsrc/cplat/exports.map` は `cplat_*` と `_cplat_*` のみを公開エクスポート対象としているため、公開シンボルは `cplat_*` 接頭辞へ改める必要があります。
- **根拠資料**: [app/cplat/AGENTS.md](../../../AGENTS.md)、[app/cplat/docs/functional-spec/README.md](../../functional-spec/README.md)、[app/cplat/docs/functional-spec-guideline.md](../../functional-spec-guideline.md)、[exports.map](../../../prod/libsrc/cplat/exports.map)
- **各案の具体例**:

```text
案 A (推奨): 新規カテゴリ service を追加する
- 公開ヘッダー : prod/include/cplat/service/service.h (cplat.h にも含める)
- 実装         : prod/libsrc/cplat/service/
- 機能仕様     : docs/functional-spec/service.md (要件 ID: CPLAT-SERVICE-FUNC-NNN)
- シンボル接頭辞: cplat_service_* / CPLAT_SERVICE_*
```

```text
案 B: 既存カテゴリ runtime の配下に配置する
- 公開ヘッダー : prod/include/cplat/runtime/service.h (cplat.h にも含める)
- 実装         : prod/libsrc/cplat/runtime/
- 機能仕様     : docs/functional-spec/runtime.md へ節と CPLAT-RUNTIME-FUNC-* を追記
- シンボル接頭辞: cplat_service_* / CPLAT_SERVICE_*
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 新規カテゴリ `service` を追加し、`cplat/service/service.h`・`docs/functional-spec/service.md`・`cplat_service_*` として配置する。
- [ ] B: 既存カテゴリ `runtime` の配下に `cplat/runtime/service.h`・`docs/functional-spec/runtime.md` への追記・`cplat_service_*` として配置する。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: 機能カテゴリ間の責務分離と既存カテゴリ粒度 (`argparser`、`prompt`、`mmap`、`net` 等) との統一優先。  
理由: サービス機能はライフサイクル駆動、OS 登録・解除、停止抽象、OS イベント監視、watchdog、設定再読込を含む独立した目的を持ち、実装規模も約 2,500 行に及びます。すでに 18 要件を持つ `runtime` へ統合すると機能仕様とディレクトリが肥大化するため、独立カテゴリ `service` とするほうが探索性と保守性に優れます。

**Assessment**

単一選択で A のみが選択されました。新規カテゴリ `service` として `prod/include/cplat/service/service.h`、`prod/libsrc/cplat/service/`、`docs/functional-spec/service.md` (`CPLAT-SERVICE-*`)、Doxygen グループ `CPLAT_SERVICE`、接頭辞 `cplat_service_*` / `CPLAT_SERVICE_*` で構成します。`app/cplat/AGENTS.md`、`docs/functional-spec-guideline.md`、`exports.map` と完全に整合します。

### Q-002 API の状態モデル

**Status**

解決済み

**Question**

`cplat` に取り込むサービス API の状態管理を、プロセス グローバル方式とハンドル指向方式のどちらにしますか。

**Context**

- **経緯と背景**: 現行の `service-sample` はプロセスにつき 1 つのサービスしか実行されない前提 (`SERVICE_WIN32_OWN_PROCESS`、Linux systemd の `Type=notify`・`SIGHUP`・`NOTIFY_SOCKET`) に基づき、停止要求 (`svc_wait_for_stop()` / `svc_request_stop()` / `svc_stop_requested()`) や状態通知 (`svc_set_status_text()`) をハンドル引数なしのプロセス グローバル関数として提供しています。これは `cplat/runtime/shutdown.h` と同じ設計です。一方、コールバックへ不透明ハンドル (`cplat_service *`) を渡す方式にすると、利用側でグローバル参照が減る反面、コールバック署名と利用側の記述が増えます (OS の制約上、同一プロセス内での複数サービス同時起動はいずれにせよ成立しません)。
- **根拠資料**: [service-sample.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample.h#L258-L345)、[shutdown.h](../../../prod/include/cplat/runtime/shutdown.h)
- **各案の具体例**:

```c
/* 案 A (推奨): プロセス グローバル API (ハンドルなし) */
static int on_run(void *user_data)
{
    while (cplat_service_wait_for_stop(1000) == CPLAT_ERR_TIMEOUT)
    {
        cplat_service_set_status_text("動作中");
    }
    return 0;
}
```

```c
/* 案 B: ハンドル指向 API (コールバックへハンドルを渡す) */
static int on_run(cplat_service *service, void *user_data)
{
    while (cplat_service_wait_for_stop(service, 1000) == CPLAT_ERR_TIMEOUT)
    {
        cplat_service_set_status_text(service, "動作中");
    }
    return 0;
}
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) プロセス グローバル API とする (停止待機・停止要求・状態通知はハンドルを取らず、実行開始時および終了時に内部状態を安全に初期化する)。
- [ ] B: ハンドル指向 API とする (`cplat_service *` ハンドルを生成・破棄し、各コールバックと制御 API へ渡す)。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足: 1 つのプロセスは、マルチインスタンスには対応しなくてよい。

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: OS のプロセス単位制約 (`SIGHUP`、`NOTIFY_SOCKET`、`StartServiceCtrlDispatcher`) との整合性と、既存 `service-sample` / `shutdown.h` との利用感の統一優先。  
理由: Linux のシグナル マスク・`NOTIFY_SOCKET`、および Windows の `SERVICE_WIN32_OWN_PROCESS` はプロセスに 1 つの資源であり、ハンドルを複数生成できるような誤解を与える設計は実態と合いません。プロセス グローバル API としたうえで、単体テストや再実行に備えて内部状態の初期化・後始末を `run` / `run_console` の入口・出口で確実に完結させるほうが簡潔かつ安全です。

**Assessment**

単一選択で A のみが選択され、補足「1 つのプロセスは、マルチインスタンスには対応しなくてよい。」も A と一致しています。プロセス グローバル API として設計し、複数インスタンス非対応 (適用範囲外) を機能仕様へ明記します。

**Related**

[Q-005](#q-005-診断トレースの出力方式と所有権)

### Q-003 CLI エントリ ポイントの取り込み範囲

**Status**

解決済み

**Question**

`service-sample.c` のコマンド ライン引数解析 (`install` / `uninstall` / `run` / `console` / `--help`) とサブコマンド分岐を、`cplat` へ取り込みますか。

**Context**

- **経緯と背景**: 現行の `service-sample.c` は `main()` 内で `cplat_console_attach_parent()`、`cplat_console_init()`、`cplat_argparser_*` による位置引数 `command` の解析、および `install` / `uninstall` / `run` / `console` への分岐を行い、`extern const svc_definition g_service_def` を参照しています。動的ライブラリである `libcplat` は `main()` や未解決の `extern g_service_def` を持てないため、取り込む場合は `cplat_service_main(const cplat_service_definition *def, int argc, char **argv)` のようなヘルパー関数として提供する必要があります。一方、実際のサービス アプリケーションは独自の CLI オプション (例: `--config`、`--log-level`、`status`、`version` 等) や独自の日本語・英語ヘルプ文言を持つことが多く、引数解析をライブラリに閉じ込めると拡張しにくくなります。
- **根拠資料**: [service-sample.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample.c#L101-L237)
- **各案の具体例**:

```c
/* 案 A (推奨): 操作 API のみを提供し、CLI 解析は利用側 (service-sample 等) に残す */
int main(int argc, char *argv[])
{
    /* 利用側が cplat_argparser 等で自由にオプションやサブコマンドを解析する */
    if (strcmp(command, "install") == 0)
        return handle_install(&def);
    if (strcmp(command, "run") == 0)
        return handle_run(&def);
    ...
}
```

```c
/* 案 B: 操作 API に加えて、標準 4 コマンドを処理する高水準ヘルパーも提供する */
int main(int argc, char *argv[])
{
    /* 独自オプションが不要なサービスは 1 行で起動できる */
    return cplat_service_main(&g_service_def, argc, argv);
}
/* 独自オプションが必要なサービスは個別の cplat_service_install / run 等を直接呼ぶ */
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 個別の操作 API (`install` / `uninstall` / `run` / `run_console` / `prepare_run` / 停止抽象 / 状態通知) のみを `cplat` へ取り込み、CLI 解析とディスパッチ (`main()`) は利用側 (`service-sample` 等) に残す。
- [ ] B: 個別の操作 API に加えて、標準 4 コマンド (`install` / `uninstall` / `run` / `console` / `--help`) を処理する高水準ヘルパー (`cplat_service_main()` 相当) も `cplat` で提供する。
- [ ] C: 標準 4 コマンドを処理する高水準ヘルパー (`cplat_service_main()` 相当) のみを提供し、個別の `install` / `uninstall` / `run` API は公開しない。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: ライブラリの責務境界の明確化と、利用側の CLI 設計・国際化・独自オプション追加の自由度優先。  
理由: `cplat` ライブラリ内に固定のコマンド名や日本語ヘルプ文言、`cplat_argparser` の利用を閉じ込めると、独自オプションを持つ実サービスで再利用しにくくなります。操作 API だけを `cplat` が担い、`app/service-sample` が標準 4 コマンドの CLI 構成例を示す役割分担のほうが、ライブラリとサンプルの責務が明瞭です。

**Assessment**

単一選択で A のみが選択されました。`cplat` は個別操作 API のみを提供し、CLI 引数解析とサブコマンド分岐は `app/service-sample` 等の利用側責務とします。[Q-008](#q-008-登録と解除における自動権限昇格の責務) の A および [Q-009](#q-009-サービス登録パラメーターのカスタマイズ範囲) の A と整合します。

**Related**

[Q-008](#q-008-登録と解除における自動権限昇格の責務)、[Q-009](#q-009-サービス登録パラメーターのカスタマイズ範囲)

### Q-004 Linux での libsystemd への依存方式

**Status**

解決済み

**Question**

Linux 実装が使用する `libsystemd` (`sd_notify`、`sd_watchdog_enabled`、`sd_event_*`、`sd_bus_*`) への依存を、`app/cplat` でどの方式にしますか。

**Context**

- **経緯と背景**: 現行の `app/service-sample` は `LIBS += systemd` で `libsystemd` に直接リンクし、ビルド時に `pkg-config --exists libsystemd` を必須にしています。しかし `app/cplat` は単一の共有ライブラリ (`libcplat.so` / `libcplat.dll`) だけを生成し (`docs/link-policy.md`)、サービスを使わない CLI ツール、ライブラリ、テスト、コンテナーを含む全 app が `libcplat.so` をリンクします。`libcplat.so` に直接 `LIBS += systemd` を追加すると、`libsystemd.so.0` がない最小コンテナー等で `libcplat.so` 全体がロード不能になり、すべての `cplat` ビルド環境で `systemd-devel` / `libsystemd-dev` が必須になります。なお、同じディレクトリで並行検討中の [cplat-account-session-discussion.md](cplat-account-session-discussion.md) の Q-017 でも `dlopen()` による実行時読み込みが推奨案として提示されています。
- **根拠資料**: [app/cplat/docs/link-policy.md](../../link-policy.md)、[app/cplat/prod/libsrc/cplat/makepart.mk](../../../prod/libsrc/cplat/makepart.mk)、[app/service-sample/prod/src/cmd/makepart.mk](../../../../service-sample/prod/src/cmd/makepart.mk)、[mock_systemd.h](../../../../service-sample/test/include/mock_systemd.h)
- **各案の具体例とトレードオフ**:

```text
案 A (推奨): libcplat.so 内で libsystemd.so.0 を実行時に dlopen() / dlsym() で動的ロードする
- 非サービス利用側や libsystemd 非導入環境でも libcplat.so のロードに影響なし (dl はすでにリンク済み)。
- ビルド時も自前の関数ポインター表・最小型宣言で完結させれば libsystemd-dev 不要 (またはヘッダーのみ参照も選択可)。
- 単体テストでは関数ポインター表の差し替えで 28 関数をモックでき、include_override より簡潔に検証可能。
- 未導入環境でサービス run を呼んだ場合はエラー (CPLAT_ERR_UNSUPPORTED 等) を返す。
```

```text
案 B: libcplat.so に libsystemd をビルド時直接リンクする (LIBS += systemd)
- 既存の service-sample_linux.c / service-sample_linux_events.c と mock_systemd を最小変更で移植できる。
- 代償として libcplat.so の DT_NEEDED に libsystemd.so.0 が増え、全 cplat 利用側とビルド環境に systemd 依存が波及する。
```

```text
案 C: app/cplat 内に別共有ライブラリ (例: libcplat_service.so / .dll) を新設して libsystemd を直接リンクする
- libcplat.so の依存を増やさず、サービス利用者だけが libcplat_service をリンクする。
- 代償として docs/link-policy.md の「cplat は libcplat 1 つだけを生成する」方針と runtime-bundle.mk の変更が必要になり、配布同梱ライブラリが 1 つ増える。
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `libcplat.so` 内で `libsystemd.so.0` を実行時に `dlopen()` / `dlsym()` で動的ロードする (`docs/link-policy.md` に実行時読み込みの記述を追記する)。
- [ ] B: `libcplat.so` に `libsystemd` をビルド時直接リンクする (`LIBS += systemd` を追加し、`docs/link-policy.md` を更新する)。
- [ ] C: `app/cplat` 内に別共有ライブラリ (`libcplat_service.so` / `.dll` 等) を新設し、サービス機能と `libsystemd` 直接リンクをそちらへ分離する。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: 既存 `libcplat.so` 利用者 (非サービス CLI・コンテナー等) への依存波及防止、および `docs/link-policy.md` の単一ライブラリ方針との整合優先。  
理由: `libcplat.so` はすでに `-ldl` をリンクしており、`libsystemd.so.0` の実行時ロードであれば非サービス利用側のロード時依存 (`DT_NEEDED`) を一切増やさずに済みます。`service-sample` が使用する `sd_*` 関数は約 28 件と有限であり、内部ディスパッチ表を介した `dlopen` / `dlsym` と単体テスト時の差し替えの双方を現実的な規模で実装できます。

**Assessment**

単一選択で A のみが選択されました。`libcplat.so` 内で `libsystemd.so.0` を実行時に `dlopen()` / `dlsym()` で読み込み、`docs/link-policy.md` へ実行時読み込みの旨を追記します。なお、ビルド時に `<systemd/*.h>` を参照するか自前の最小 ABI 宣言でビルド時依存も外すかについては、[Q-014](#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存) で確認します。

**Related**

[Q-012](#q-012-並行検討中の-account-session-案件との調整)、[Q-014](#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存)、[Q-020](#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合)

### Q-005 診断トレースの出力方式と所有権

**Status**

解決済み

**Question**

サービス層が出力する診断トレース (コールバック失敗、`sd_notify` / `SetServiceStatus` 失敗、D-Bus 警告、watchdog 停滞警告、`SIGHUP` 無視など) の出力方式と `cplat_tracer` の所有権をどう設計しますか。

**Context**

- **経緯と背景**: 現行の `service-sample.c` は独自のプロセス グローバルな `cplat_tracer *s_tracer` を自動生成・設定 (非 `run` では stderr=INFO、全コマンドで共有ファイル=VERBOSE) し、`svc_get_tracer()` を通じて全実装ファイルから出力しています。一方、`app/cplat` の既存モジュール (`prod/libsrc/cplat/`) は内部でグローバルな `cplat_tracer` を自動生成しません。サービス実行中は別スレッドで非同期に警告や障害が発生するため、戻り値だけでは伝えきれない診断出力の経路自体は必要ですが、`cplat_tracer` の生成・設定 (出力先、ファイル パス、syslog/ETW/EventLog 有効化、ログ レベル)・ライフサイクルの所有権を誰が持つかが設計上の分岐点になります。
- **根拠資料**: [service-sample.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample.c#L478-L563)、[service-sample_linux_events.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux_events.c#L1092-L1116)
- **各案の具体例**:

```c
/* 案 A (推奨): 定義構造体に任意の cplat_tracer * を受け取る (所有権は利用側) */
cplat_service_definition def = {
    .name = "my-service",
    ...,
    .tracer = my_tracer /* NULL の場合はサービス層からのトレース出力を省略 */
};
```

```c
/* 案 B: 汎用ログ コールバックを受け取る (cplat_tracer への依存を外す) */
typedef void (*cplat_service_log_fn)(cplat_trace_level level, const char *message, void *user_data);
/* 定義構造体に .on_log を指定。利用側で cplat_tracer や独自ロガーへ転送する */
```

```c
/* 案 C: サービス層内部で cplat_tracer を自動生成・管理し、cplat_service_get_tracer() で公開する */
/* 現行 service-sample と同じく、ファイル共有出力 + stderr 切替をサービス層が固定で構築する */
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) サービス定義構造体に任意の `cplat_tracer *tracer` フィールドを設け、生成・設定・開始・破棄は利用側に委ねる (`NULL` 指定時はトレース出力を省略する)。
- [ ] B: サービス定義構造体に汎用ログ コールバック (`on_log`) を設け、`cplat_tracer` の代わりにコールバック経由で診断を通知する。
- [ ] C: `cplat` のサービス層内部で `cplat_tracer` を自動生成・管理し、`cplat_service_get_tracer()` を公開する (現行 `service-sample` 方式を踏襲)。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: `cplat` 既存モジュールの設計方針 (隠しグローバル資源を持たない) と、利用側が syslog / ETW / EventLog / ファイル出力先・レベルを自在に制御できる柔軟性優先。  
理由: `cplat_tracer` はすでに `cplat` 自身の公開機能であり、`cplat_tracer_write(NULL, ...)` が安全な no-op である契約も整っています。所有権を利用側へ委ねつつ `def->tracer` を受け取る形にすれば、サービス層の既存診断メッセージをそのまま活かしつつ、利用側の運用要件 (ログ出力先・ローテーション・OS バックエンド) を妨げません。

**Assessment**

単一選択で A のみが選択されました。`cplat_service_definition` に任意の `cplat_tracer *tracer` を持たせ、所有権は利用側に置きます。[Q-010](#q-010-linux-の-sighup-事前ブロックの公開設計) の A と組み合わせることで、利用側が `cplat_service_prepare_run()` の後に `cplat_tracer` を開始してから `cplat_service_run()` を呼ぶ順序が整合します。

**Related**

[Q-002](#q-002-api-の状態モデル)、[Q-010](#q-010-linux-の-sighup-事前ブロックの公開設計)

### Q-006 サービス操作 API の戻り値規約

**Status**

解決済み

**Question**

`cplat` に取り込むサービス操作 API (`install` / `uninstall` / `prepare_run` / `run` / `run_console`) の戻り値規約と、コールバックが返した終了コードの伝達方法をどう設計しますか。

**Context**

- **経緯と背景**: `app/cplat/docs/coding-guideline.md` では、公開 API の戻り値を共通結果コード (`CPLAT_OK` = 0、負値の `CPLAT_ERR_*`) に統一し、元 API 規約を保存する特定の例外群のみを「適用対象外」として明示する規則です。一方、現行の `service-sample` では `svc_os_install` / `svc_os_uninstall` / `svc_os_prepare_run` が `EXIT_SUCCESS` (0) / `EXIT_FAILURE` (1) を返し、`svc_os_run_service` / `svc_run_lifecycle` が最初に失敗したコールバック (`on_start` / `on_run` / `on_stop`) の終了コード (推奨 1〜124、3 除く) を直接返しています。共通結果コード (負値) とコールバック終了コード (正値 1〜124) を同じ `int` 戻り値に混ぜると、「`cplat` 自体の呼び出し失敗 (引数不正・SCM 接続失敗・権限不足)」と「利用側コールバックが返した失敗コード」の区別や `!= CPLAT_OK` 判定との整合が曖昧になります。
- **根拠資料**: [app/cplat/docs/coding-guideline.md](../../coding-guideline.md#L29-L138)、[result.h](../../../prod/include/cplat/base/result.h)、[service-sample.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample.h#L352-L526)
- **各案の具体例**:

```c
/* 案 A (推奨): 全操作 API が CPLAT_OK / CPLAT_ERR_* を返し、コールバック終了コードは出力引数で受け取る */
int rc = cplat_service_install(&def, &opts);   /* CPLAT_OK / CPLAT_ERR_PERMISSION_DENIED / ... */
int exit_code = 0;
rc = cplat_service_run(&def, &exit_code);      /* CPLAT_OK (exit_code にコールバック結果) or CPLAT_ERR_* */
```

```c
/* 案 B: install / uninstall / prepare_run は CPLAT_OK / CPLAT_ERR_*、run 系はプロセス終了コードを直接返す */
int rc = cplat_service_install(&def, &opts);   /* CPLAT_OK / CPLAT_ERR_* */
int exit_code = cplat_service_run(&def);       /* 0 または非 0 (コールバック失敗値または EXIT_FAILURE) */
```

```c
/* 案 C: 全関数を EXIT_SUCCESS / EXIT_FAILURE / コールバック終了コードのまま維持する */
int exit_code = cplat_service_install(&def);   /* 0 or EXIT_FAILURE */
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 全サービス操作 API の戻り値を共通結果コード (`CPLAT_OK` / `CPLAT_ERR_*`) に統一し、`run` / `run_console` ではコールバックの終了コードを出力引数 `int *exit_code_out` (NULL 可) で返す。
- [ ] B: `install` / `uninstall` / `prepare_run` は共通結果コード (`CPLAT_OK` / `CPLAT_ERR_*`) を返し、`run` / `run_console` はプロセス終了コード (0 または非 0) を直接返す (`docs/coding-guideline.md` の適用対象外に追記する)。
- [ ] C: 全関数で `EXIT_SUCCESS` / `EXIT_FAILURE` / コールバック終了コードを直接返す (`docs/coding-guideline.md` の適用対象外に追記する)。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: `app/cplat/docs/coding-guideline.md` の共通結果コード規約への完全準拠と、失敗原因の識別性優先。  
理由: `cplat_process_run_sync(&options, &exit_code_out)` など既存の `cplat` プロセス API も「API の成否は戻り値 (`CPLAT_OK` / `CPLAT_ERR_*`)、子・対象の終了コードは `int *exit_code_out`」という分離方式を採用しています。案 A であれば `CPLAT_ERR_PERMISSION_DENIED` や `CPLAT_ERR_INVALID_ARGUMENT` を利用側が型安全に判別でき、既存規範へ例外を追加せずに済みます。

**Assessment**

単一選択で A のみが選択されました。すべてのサービス操作 API を `CPLAT_OK` / `CPLAT_ERR_*` へ統一し、`run` / `run_console` はコールバック終了コードを `int *exit_code_out` (NULL 可) で返します。OS エラー詳細を引数 `cplat_error *detail_out` で受け取るか否かについては、[Q-017](#q-017-公開-api-での-cplat_error-出力引数の要否) で確認します。

**Related**

[Q-007](#q-007-停止待機-api-の戻り値とタイムアウト仕様)、[Q-017](#q-017-公開-api-での-cplat_error-出力引数の要否)

### Q-007 停止待機 API の戻り値とタイムアウト仕様

**Status**

解決済み

**Question**

停止要求の待機 API (`svc_wait_for_stop` 相当) の戻り値規約と、負値・無期限タイムアウトの扱いをどう設計しますか。

**Context**

- **経緯と背景**: 現行の `svc_wait_for_stop(int timeout_ms)` は、停止要求到達時および待機エラー時に `1`、タイムアウト時に `0` を返し、負の `timeout_ms` は `0` とみなして即時リターンします。一方、`app/cplat` には `cplat/base/wait.h` (`CPLAT_NO_WAIT` = 0、`CPLAT_WAIT_FOREVER` = `INT_MAX`) と、`cplat_condvar_timed_wait()` のような共通結果コード規約 (`CPLAT_OK` = 条件成立、`CPLAT_ERR_TIMEOUT` = タイムアウト、`CPLAT_ERR_INVALID_ARGUMENT` 等 = 異常) が整備されています。
- **根拠資料**: [service-sample.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample.h#L258-L286)、[service-sample.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample.c#L565-L608)、[wait.h](../../../prod/include/cplat/base/wait.h)
- **各案の具体例**:

```c
/* 案 A (推奨): cplat の共通結果コードと wait.h に準拠 */
/* CPLAT_OK: 停止要求到達 / CPLAT_ERR_TIMEOUT: タイムアウト / 他の CPLAT_ERR_*: 異常 */
/* timeout_ms には 0 (CPLAT_NO_WAIT)、正値、CPLAT_WAIT_FOREVER を指定可能。負値は CPLAT_ERR_INVALID_ARGUMENT */
while (cplat_service_wait_for_stop(1000) == CPLAT_ERR_TIMEOUT)
{
    /* 周期処理 (停止要求到達または異常でループを抜ける) */
}
```

```c
/* 案 B: 現行 service-sample の 0/1 規約を維持 */
/* 1: 停止要求到達または待機異常 / 0: タイムアウト / 負値は 0 扱い */
while (cplat_service_wait_for_stop(1000) == 0)
{
    /* 周期処理 */
}
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `cplat` の共通結果コードと `wait.h` に合わせる (`CPLAT_OK` = 停止要求到達、`CPLAT_ERR_TIMEOUT` = タイムアウト、負値や未初期化等の異常は `CPLAT_ERR_*`、`CPLAT_NO_WAIT` / `CPLAT_WAIT_FOREVER` 対応)。
- [ ] B: 現行 `service-sample` の 0/1 規約を維持する (`1` = 停止要求到達または待機異常、`0` = タイムアウト、負値は 0 扱い、`docs/coding-guideline.md` の適用対象外に追記する)。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: `cplat` の既存待機 API (`cplat_condvar_timed_wait`、`wait.h`) との契約統一と、待機失敗と正常停止要求の区別優先。  
理由: `while (cplat_service_wait_for_stop(1000) == CPLAT_ERR_TIMEOUT)` はタイムアウト時のみループを継続するため、待機失敗時に無待機スピンへ陥る危険を現行と同様に防ぎつつ、ループ脱出後に `rc != CPLAT_OK` で異常終了を検出することも可能になります。また `CPLAT_WAIT_FOREVER` によりイベント駆動型サービスの無期限待機にも対応できます。

**Assessment**

単一選択で A のみが選択されました。`cplat_service_wait_for_stop(int timeout_ms)` は `CPLAT_OK` (停止要求到達)、`CPLAT_ERR_TIMEOUT` (タイムアウト)、負値や未初期化等の異常時は `CPLAT_ERR_*` を返し、`CPLAT_NO_WAIT` / `CPLAT_WAIT_FOREVER` に対応します。[Q-006](#q-006-サービス操作-api-の戻り値規約) の A と整合します。

**Related**

[Q-006](#q-006-サービス操作-api-の戻り値規約)

### Q-008 登録と解除における自動権限昇格の責務

**Status**

解決済み

**Question**

`cplat` のサービス登録 (`install`)・解除 (`uninstall`) API の内部で、`cplat_elevated_process_*` による自動権限昇格 (Windows の UAC 再起動など) を行いますか。

**Context**

- **経緯と背景**: 現行の `service-sample_linux.c` および `service-sample_windows.c` は、それぞれの `svc_os_install()` / `svc_os_uninstall()` の冒頭で `cplat_elevated_process_run_if_needed("install", ...)` / `("uninstall", ...)` を呼んでいます。これをそのまま `cplat` ライブラリへ取り込むと、次の 2 つの課題が生じます。
    1. 再起動時のコマンド ライン引数が `"install"` / `"uninstall"` 固定であるため、利用側の CLI 体系 (例: `--service install` や追加オプション付き) と噛み合わない。
    2. `app/cplat/prod/include/cplat/runtime/elevated_process.h` では、`cplat_elevated_process_run_if_needed()` (親コンソール再接続) に Windows で間欠的な `ERROR_INVALID_HANDLE` 障害が既知の問題として記録されており、利用側が `cplat_elevated_process_run_piped()` や `cplat_elevated_process_run_with_result()` を選択したい場合がある。
- **根拠資料**: [service-sample_linux.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux.c#L679-L705)、[service-sample_windows.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_windows.c#L810-L836)、[elevated_process.h](../../../prod/include/cplat/runtime/elevated_process.h#L112-L233)
- **各案の具体例**:

```text
案 A (推奨): cplat の install / uninstall は自動再起動による昇格を行わない
- 権限が不足している場合は CPLAT_ERR_PERMISSION_DENIED を返す。
- UAC 昇格が必要な CLI アプリ (service-sample 等) は、自身の main() で
  cplat_elevated_process_run_piped() 等を使って適切な引数で昇格再実行する。
```

```text
案 B: cplat の install / uninstall 内部で自動昇格を維持する
- 再起動時の引数文字列や昇格方式 (無効 / run_if_needed / run_piped) をオプションで指定可能にする。
- ただし利用側にも起動直後の attach 関数呼び出し契約が残るため、完全には隠蔽できない。
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `cplat` の `install` / `uninstall` API 内部では自動権限昇格を行わず、権限不足時は `CPLAT_ERR_PERMISSION_DENIED` を返す (UAC 昇格再起動は利用側の CLI 層が `cplat_elevated_process_*` で行う)。
- [ ] B: `cplat` の `install` / `uninstall` API 内部での自動権限昇格を維持し、再起動時の引数文字列をオプション指定できるようにする。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: ライブラリと CLI 層の責務分離、および利用側での昇格方式 (`run_piped` / `run_with_result` / 非対話エラー返却) 選択の自由度優先。  
理由: 自動昇格はプロセス自身のコマンド ライン構造と起動直後の `attach` 処理 (`cplat_console_attach_parent` / `cplat_elevated_process_attach_output_pipes`) と密結合しており、ライブラリ単体では完結しません。`cplat` のサービス API は純粋な登録・解除に徹し、権限不足を `CPLAT_ERR_PERMISSION_DENIED` で返すほうが、GUI インストーラー、非対話バッチ、CLI のいずれからも安全に利用できます。

**Assessment**

単一選択で A のみが選択されました。`cplat_service_install()` / `cplat_service_uninstall()` は自動権限昇格を行わず、権限不足時は `CPLAT_ERR_PERMISSION_DENIED` を返します。[Q-011](#q-011-service-sample-本体の改修と資産移行の範囲) の A により `app/service-sample` を書き換える際の CLI 層での昇格方式は、[Q-018](#q-018-改修後の-service-sample-における-windows-権限昇格方式) で確認します。

**Related**

[Q-003](#q-003-cli-エントリ-ポイントの取り込み範囲)、[Q-018](#q-018-改修後の-service-sample-における-windows-権限昇格方式)

### Q-009 サービス登録パラメーターのカスタマイズ範囲

**Status**

解決済み

**Question**

`install` 時に OS へ登録するサービス設定 (起動引数、自動再起動、watchdog、OOM killer 対策、PRESHUTDOWN 猶予など) を、今回の初回実装でどこまでカスタマイズ可能にしますか。

**Context**

- **経緯と背景**: 現行 `service-sample` の登録処理では、`ExecStart=<exe> run` / `binPath="\"<exe>\" run"`、`Restart=on-failure`、`RestartSec=5`、`StartLimitIntervalSec=60`、`StartLimitBurst=5`、`WatchdogSec=30`、`OOMScoreAdjust=-1000` (systemd 248 以降は `ManagedOOMPreference=omit`)、`SVC_PRESHUTDOWN_TIMEOUT_MS=30000`、`SERVICE_AUTO_START`、`LocalSystem`、`After=network.target`、`WantedBy=multi-user.target` がすべて定数固定です。特に [Q-003](#q-003-cli-エントリ-ポイントの取り込み範囲) で CLI を利用側へ委ねる場合、サービス起動引数が `"run"` 固定のままだと、異なるコマンド体系や設定ファイル引数を持つアプリで利用できません。また、watchdog 周期や再起動間隔、OOM 保護の有無はサービス種別によって調整が必要です。
- **根拠資料**: [service-sample_internal.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample_internal.h)、[service-sample_linux.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux.c#L59-L140)、[service-sample_windows.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_windows.c#L54-L62)
- **各案の具体例**:

```c
/* 案 A (推奨): 任意の install オプション構造体 (NULL またはゼロ初期化時は service-sample と同じ既定値) */
typedef struct cplat_service_install_options
{
    const char *run_args;            /* NULL 時は "run" */
    unsigned int restart_delay_sec;  /* 0 時は 5 [s] (無効化フラグも別途考慮) */
    unsigned int start_limit_interval_sec; /* Linux: 0 時は 60 [s] */
    unsigned int start_limit_burst;        /* Linux: 0 時は 5 回 */
    unsigned int watchdog_sec;             /* Linux: 0 時は 30 [s] (無効化指定も考慮) */
    unsigned int preshutdown_timeout_ms;   /* Windows: 0 時は 30000 [ms] */
    int disable_auto_restart;              /* 0: 自動再起動あり (既定) / 非 0: なし */
    int disable_oom_protection;            /* Linux: 0: OOM 保護あり (既定) / 非 0: なし */
} cplat_service_install_options;
```

```c
/* 案 B: run_args (起動引数) のみ変更可能にし、再起動・watchdog・OOM 保護等は固定値 */
int cplat_service_install(const cplat_service_definition *def, const char *run_args);
```

```c
/* 案 C: 全設定を service-sample と同じ固定値にし、オプション引数を設けない */
int cplat_service_install(const cplat_service_definition *def);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 任意の `cplat_service_install_options` 構造体 (`NULL` またはゼロ初期化で現行 `service-sample` と同じ既定値) を設け、起動引数・自動再起動・StartLimit・watchdog・OOM 保護・PRESHUTDOWN 猶予を上書き可能にする。
- [ ] B: サービス起動引数 (`run_args`、既定 `"run"`) のみ指定可能とし、再起動・watchdog・OOM 保護・PRESHUTDOWN 猶予は現行の固定値とする。
- [ ] C: すべての登録設定を現行 `service-sample` と同じ固定値にし、オプション引数を設けない。
- [ ] D: 案 A に加えて、Linux の任意の systemd unit 追加行 (`User=` / `After=` 等) や Windows の実行アカウント・依存サービス・開始種別 (`MANUAL` / `DELAYED_AUTO`) まで初回から提供する。
- [ ] E: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: 初回実装の過度な肥大化防止と、複数サービスでの実用的な再利用性の両立優先。  
理由: ゼロ初期化で現行 `service-sample` と同一の安全な既定値になるオプション構造体を用意しておけば、簡単な利用側は `NULL` を渡すだけで使え、運用ポリシーが異なるサービスも `cplat` 本体を改変せずに起動引数や watchdog・再起動遅延を調整できます。Windows のアカウント資格情報管理や任意の unit 注入はセキュリティ・検証コストが大きいため、必要時に後方互換で拡張するほうが堅実です。

**Assessment**

単一選択で A のみが選択されました。`cplat_service_install_options` (`NULL` またはゼロ初期化で現行 `service-sample` と同じ既定値) を導入します。可変パラメーター化に伴い、現行コードの `static_assert` が担っていた 2 点の制約 (`watchdog_sec` とイベント監視スレッド終了待ち上限の関係、`StartLimit` と `restart_delay_sec` の到達可能性検査) の実行時仕様を [Q-015](#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限) および [Q-016](#q-016-install-時の-startlimit-と再起動遅延の整合性検査) で確認します。

**Related**

[Q-003](#q-003-cli-エントリ-ポイントの取り込み範囲)、[Q-015](#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限)、[Q-016](#q-016-install-時の-startlimit-と再起動遅延の整合性検査)

### Q-010 Linux の SIGHUP 事前ブロックの公開設計

**Status**

解決済み

**Question**

Linux でスレッド生成前に `SIGHUP` をブロックする処理 (`svc_os_prepare_run()` 相当) を、`cplat` の公開 API として提供しますか。

**Context**

- **経緯と背景**: Linux の `sd_event_add_signal(SIGHUP)` は、プロセス内の **全スレッド** で `SIGHUP` がブロックされていることを前提とします (`sd_event_add_signal(3)`)。`service-sample` ではトレーサー開始などのスレッド生成より前に `svc_os_prepare_run()` を呼んで `SIGHUP` をブロックし、以後生成されるスレッドへマスクを継承させています。Windows では何もせず成功を返します。`cplat` へ取り込む際、利用側が `cplat_service_run()` の前に `cplat_tracer_start()` や独自スレッドを起動するケースを安全に扱えるかどうかが分岐点になります。
- **根拠資料**: [service-sample.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample.h#L382-L398)、[service-sample_linux.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux.c#L173-L200)、[service-sample_linux_events.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux_events.c#L18-L27)
- **各案の具体例**:

```c
/* 案 A (推奨): 公開 API として提供する (Linux で SIGHUP をブロック、Windows は CPLAT_OK の no-op) */
cplat_service_prepare_run(); /* スレッド生成前 (tracer 開始前など) に呼ぶ */
/* tracer 開始など */
cplat_service_run(&def, &exit_code);
```

```c
/* 案 B: 公開 API にせず、cplat_service_run() 内部の先頭で SIGHUP をブロックする */
/* 利用側には「cplat_service_run() 呼び出し前にスレッドを生成しないこと」を前提制約とする */
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `cplat_service_prepare_run()` をクロスプラットフォーム公開 API として提供する (Linux で `SIGHUP` をブロック、Windows は `CPLAT_OK` を返す no-op)。
- [ ] B: 公開 API としては設けず、`cplat_service_run()` の冒頭で `SIGHUP` をブロックし、事前スレッド生成禁止を前提制約とする。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: 現行 `service-sample` で検証済みのスレッド安全性契約の維持と、利用側が `cplat_service_run()` 前にトレーサー等を初期化できる柔軟性優先。  
理由: [Q-005](#q-005-診断トレースの出力方式と所有権) で利用側が `cplat_tracer` を起動してから `cplat_service_run()` を呼ぶ構成を取る場合、`cplat_service_run()` の内部だけで `SIGHUP` をブロックすると、万一先行して生成されたスレッドがあると `SIGHUP` が漏れるリスクがあります。明示的な `cplat_service_prepare_run()` を設けることで、利用側が `main()` の単一スレッド段階で確実にマスクを確定できます。

**Assessment**

単一選択で A のみが選択されました。`cplat_service_prepare_run()` をクロスプラットフォーム公開 API として提供します (Linux で `SIGHUP` ブロック、Windows で `CPLAT_OK` 返却の no-op)。

**Related**

[Q-003](#q-003-cli-エントリ-ポイントの取り込み範囲)、[Q-005](#q-005-診断トレースの出力方式と所有権)

### Q-011 service-sample 本体の改修と資産移行の範囲

**Status**

解決済み

**Question**

本案件の実装フェーズにおいて、`app/cplat` への機能追加とあわせて、`app/service-sample` の書き換え (重複コード削除・テスト整理) および仕様文書の移設まで実施しますか。

**Context**

- **経緯と背景**: `app/cplat` は独立した Git サブモジュール、`app/service-sample` は親リポジトリ (`c-modernization-kit`) 管理下にあります。現在 `app/service-sample` には、共通部・Linux 部・Windows 部の実装、計 87 件の単体テスト (共通 36 / Linux 40 / Windows 11)、`mock_systemd`、および仕様文書 (`docs/service-events.md`、`docs/systemd-oomkiller-advisory.md`) が置かれています。`cplat` へ共通機能を取り込んだ後に `service-sample` の重複実装を残すと二重保守になる一方、親リポジトリとサブモジュールの両方にまたがる大規模変更になります。
- **根拠資料**: [app/service-sample/docs/service-events.md](../../../../service-sample/docs/service-events.md)、[app/service-sample/docs/systemd-oomkiller-advisory.md](../../../../service-sample/docs/systemd-oomkiller-advisory.md)、[app/cplat/AGENTS.md](../../../AGENTS.md)
- **各案の比較**:

```text
案 A (推奨): 本案件内で一括実施する
1. app/cplat へ実装、機能仕様、api-cheatsheet、exportTest、mock_cplat、単体テスト一式を追加。
2. app/service-sample を cplat の新 API を使う薄いサンプルへ書き換え、重複実装と重複テストを削除。
3. service-events.md / systemd-oomkiller-advisory.md の正本を app/cplat/docs/ へ移設し、
   app/service-sample/docs/ からは参照案内へ整理する。
```

```text
案 B: 2 段階に分け、本案件では app/cplat 側のみ完了させる
- app/service-sample の書き換え・重複削除・文書移設は後続案件 (backlog へ記録) とする。
```

```text
案 C: コード (app/cplat 追加 + app/service-sample 書き換え) は一括実施し、仕様文書は app/service-sample/docs/ に残す
- app/cplat 側には機能仕様 (docs/functional-spec/service.md) と api-cheatsheet.md のみ追加する。
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `app/cplat` への追加・テスト移植、`app/service-sample` の新 API 利用への書き換え、および仕様文書 (`service-events.md` / `systemd-oomkiller-advisory.md`) の `app/cplat/docs/` への移設までを本案件で一括実施する。
- [ ] B: 本案件では `app/cplat` への追加・テスト・文書整備のみを実施し、`app/service-sample` の書き換えは後続案件 (残課題へ記録) に分ける。
- [ ] C: `app/cplat` への追加と `app/service-sample` のコード書き換えは本案件で実施し、`service-events.md` / `systemd-oomkiller-advisory.md` は `app/service-sample/docs/` に残す。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: 新設 API の実用検証 (`service-sample` 自身を利用側として結合検証すること) と、コード・文書の二重保守防止優先。  
理由: `app/service-sample` を実際に新 API へ書き換えてビルド・テストを通すことで、公開ヘッダーの使い勝手やリンク構成 (`runtime-bundle.mk` 等) の不足を実装段階で検出できます。また、`service-events.md` と `systemd-oomkiller-advisory.md` はサービス基盤そのものの仕様・根拠であるため、`app/cplat/docs/` を正本とするほうが `app/cplat` 単独のドキュメント完結性にも合致します。

**Assessment**

単一選択で A のみが選択されました。本案件内で `app/cplat` への追加・移植、`app/service-sample` の書き換え、および `service-events.md` / `systemd-oomkiller-advisory.md` の `app/cplat/docs/` への移設まで一括実施します。移設元の旧ファイル 2 件の扱いは [Q-019](#q-019-移設後の旧ドキュメント-ファイルの扱い) で、書き換え後の `app/service-sample` の昇格方式は [Q-018](#q-018-改修後の-service-sample-における-windows-権限昇格方式) で確認します。

**Related**

[Q-013](#q-013-検証範囲と-windows-確認の扱い)、[Q-018](#q-018-改修後の-service-sample-における-windows-権限昇格方式)、[Q-019](#q-019-移設後の旧ドキュメント-ファイルの扱い)

### Q-012 並行検討中の account-session 案件との調整

**Status**

解決済み

**Question**

同じ `app/cplat/docs/prompts/current/` にある並行検討中の [cplat-account-session-discussion.md](cplat-account-session-discussion.md) との関係を、本案件でどう扱いますか。

**Context**

- **経緯と背景**: `app/cplat/docs/prompts/current/cplat-account-session-discussion.md` は現在未回答・検討中ですが、`systemd-logind` (`libsystemd`) の利用方針 (Q-017) やセッション変化イベント (`SessionNew` / `SessionRemoved` / `WTS_SESSION_LOGON` / `WTS_SESSION_LOGOFF`) が、`service-sample` の `SVC_EVENT_SESSION_LOGON` / `SVC_EVENT_SESSION_LOGOFF` および `libsystemd` 利用と領域的に近接しています。一方、サービス側のイベント監視は Linux の `sd_event` ループおよび Windows の `ServiceCtrlHandler` (`SERVICE_CONTROL_SESSIONCHANGE`) と一体で動作するため、サービス機能単独で完結した実装として成立します。
- **根拠資料**: [cplat-account-session-discussion.md](cplat-account-session-discussion.md)

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 本案件は `cplat-account-session` に依存せず自己完結で設計・実装する (`service-sample` 相当のセッション イベントを含めて取り込み、将来的な `libsystemd` ローダー共有などの余地があれば `backlog.md` へ記録する)。
- [ ] B: セッション変化イベント (`SESSION_LOGON` / `SESSION_LOGOFF`) は本案件のスコープから外し、将来 `cplat-account-session` 側へ委ねる。
- [ ] C: `cplat-account-session` 案件の合意・設計を待ってから本案件を統合設計する。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: 現行 `service-sample` からの機能欠損防止と、未着手の並行案件によるブロック回避優先。  
理由: Windows サービスでは `SERVICE_CONTROL_SESSIONCHANGE` が SCM ハンドラー経由で直接届くためサービス機能側に残す合理性があり、Linux でも既存の `sd_event` + `sd_bus` 実装をそのまま活かして自己完結させられます。将来 `account` / `session` が実装された際に `libsystemd` 動的ローダーを内部共有するリファクタリングは内部実装の変更のみで済み、公開 API を壊しません。

**Assessment**

単一選択で A のみが選択されました。本案件は自己完結で進め、将来の `libsystemd` 動的ローダー共有の余地は [261127-service-commonization-backlog.md](261127-service-commonization-backlog.md) の `BL-001` として随時追記しました。

**Related**

[Q-004](#q-004-linux-での-libsystemd-への依存方式)

### Q-013 検証範囲と Windows 確認の扱い

**Status**

解決済み

**Question**

実装完了後の検証範囲、特に現在の作業環境 (`Linux (WSL)`) では直接実行できない Windows 側のビルド・テスト・実機確認の扱いをどう設定しますか。

**Context**

- **経緯と背景**: 現在の稼働 OS は `Linux (WSL)` のため、この環境で直接実行できる検証は Linux の `make` / `make test` (影響する `app/cplat`、および [Q-011](#q-011-service-sample-本体の改修と資産移行の範囲) で対象とする場合は `app/service-sample`)、内容がある `.warn` ファイルの確認、`python3 app/general/bin/check_functional_spec.py`、`text_style_jp.py` です。一方、`app/service-sample/AGENTS.md` には「サービス制御と終了処理は、Linux と Windows の双方で確認してください」とあり、Windows/MSVC ビルド、Windows 単体テスト (`service-sampleWindowsTest` 相当)、および Windows SCM 実機動作は Windows 環境または CI での確認が必要です。また、ルート `AGENTS.md` により複数 app にまたがる全体テストは事前確認が必要です。
- **根拠資料**: [app/service-sample/AGENTS.md](../../../../service-sample/AGENTS.md)、[app/cplat/AGENTS.md](../../../AGENTS.md)

**Answer**

選択方式: 単一選択

- [x] A: (推奨) Linux 上で対象 app (`app/cplat`、および改修対象に含める場合は `app/service-sample`) のビルド・全単体テスト・`.warn`・`check_functional_spec.py`・`text_style_jp.py` を完了し、Windows 側のビルド・テスト・実機確認は未確認事項として明示したうえでユーザー環境または CI で確認する。
- [ ] B: 案 A の Linux 局所検証に加えて、Linux 上でワークスペース全体のテスト (`make test`) まで実行し、Windows 確認はユーザー環境または CI に委ねる。
- [ ] C: Windows 環境でのビルド・テスト結果が確認できるまで第 3 ゲート (成果物承認) を保留し、WBS の該当項目を要注意 (⚠️) として残す。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 19:28:39 JST

**Recommendation**

推奨: A  
優先した基準: 現在の実行環境で実施可能な品質検証の徹底と、未検証範囲の透明な共有優先。  
理由: 既存の他 `cplat` 変更と同様、影響する app の局所ビルド・全テストと各種機械的検証を Linux 上で完結させ、環境制約により実施できない Windows/MSVC 側の確認項目を明示的に報告して別環境または CI で検証する運用が最も現実的です。

**Assessment**

単一選択で A のみが選択されました。Linux 上で `app/cplat` と `app/service-sample` のビルド・全単体テスト・`.warn`・`check_functional_spec.py`・`text_style_jp.py` を完了し、Windows 側のビルド・テスト・実機確認は未確認事項として明示してユーザー環境または CI へ委ねます。

**Related**

[Q-011](#q-011-service-sample-本体の改修と資産移行の範囲)、[Q-021](#q-021-testfw-変更時のテスト検証範囲)

### Q-014 dlopen 時のビルド時 libsystemd ヘッダー依存

**Status**

未解決

**Question**

[Q-004](#q-004-linux-での-libsystemd-への依存方式) で決定した `libsystemd.so.0` の `dlopen()` / `dlsym()` 方式において、`app/cplat` のビルド時に `<systemd/*.h>` (`systemd-devel` / `libsystemd-dev`) を必要としますか。

**Context**

- **経緯と背景**: `service-sample` が利用する `libsystemd` の型と定数は、不透明ポインター (`sd_event`、`sd_event_source`、`sd_bus`、`sd_bus_message`、`sd_bus_slot`)、定数 3 個 (`SD_EVENT_ONESHOT = -1`、`SD_EVENT_PRIORITY_IMPORTANT = -100`、`SD_EVENT_PRIORITY_NORMAL = 0`)、および値型構造体 1 個 (`sd_bus_error` = `{ const char *name; const char *message; int _need_free; }`、systemd v239 以降の安定 ABI) に限られます。ビルド時に `<systemd/*.h>` を参照しない構成にすれば、`libsystemd-dev` / `systemd-devel` が入っていない最小 Linux ビルド環境やコンテナーでも `app/cplat` をビルドできます。一方、ヘッダーを `#include` すれば ABI 構造体や関数プロトタイプの自前記述を避けられますが、`app/cplat` のビルド前提に `systemd-devel` が加わります。
- **根拠資料**: `/usr/include/systemd/sd-bus.h` (systemd 239)、`/usr/include/systemd/sd-event.h`、[app/cplat/docs/link-policy.md](../../link-policy.md)
- **各案の具体例**:

```c
/* 案 A (推奨): ビルド時も <systemd/*.h> に依存しない (内部ヘッダーへ最小 ABI 宣言を定義) */
typedef struct sd_event sd_event;
typedef struct sd_bus_error {
    const char *name;
    const char *message;
    int _need_free;
    int pad; /* x64 アラインメント明示 (-Wpadded 対策) */
} sd_bus_error;
/* ビルド環境に systemd-devel / libsystemd-dev が不要 */
```

```c
/* 案 B: ビルド時は <systemd/*.h> を #include し、リンク (LIBS += systemd) のみ外す */
#include <systemd/sd-bus.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-event.h>
/* 型定義の重複はないが、cplat ビルド環境に systemd-devel / libsystemd-dev が必須になる */
```

**Answer**

選択方式: 単一選択

- [ ] A: (推奨) ビルド時も `<systemd/*.h>` に依存しない (内部ヘッダーへ不透明型・`sd_bus_error`・最小定数・関数ポインター型を自前定義し、`systemd-devel` / `libsystemd-dev` なしで `app/cplat` をビルド可能にする)。
- [x] B: ビルド時は `<systemd/sd-daemon.h>`・`<systemd/sd-event.h>`・`<systemd/sd-bus.h>` を `#include` し、実行時リンク (`LIBS += systemd`) のみを外す。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足: 現在の環境に入っているようであれば、利用してよいと考えている。testfwにもmockを追加し、C標準ライブラリ同等でよい。libsystemd-dev は入っていないと思うが問題あるか。

回答者: Tetsuo Honda  
回答日時: 2026-11-27 23:10:48 JST

**Recommendation**

推奨: A  
優先した基準: `app/cplat` のビルド前提パッケージを増やさないこと (非 systemd ビルド環境との互換性) 優先。  
理由: `libsystemd` の公開 ABI は 239 以降後方互換が保たれており、使用する値型構造体は `sd_bus_error` (3 フィールド) のみです。`dlopen` を選んだ利点をビルド環境にも一貫させるには、ヘッダー依存も持たない案 A が最も整合します。単体テストも `libmock_systemd.a` や `test/include_override/systemd/` を介さず、内部ディスパッチ表の差し替えで簡潔かつ完全にモックできます。

**Assessment**

単一選択で B が選択されましたが、補足に「現在の環境に入っているようであれば、利用してよいと考えている。testfwにもmockを追加し、C標準ライブラリ同等でよい。libsystemd-dev は入っていないと思うが問題あるか。」との確認事項および新方針が含まれるため、本 Q は未解決とします。  
事実確認の結果、現在の環境は Oracle Linux 8.10 (RPM 系) であり、Debian/Ubuntu の `libsystemd-dev` に相当する `systemd-devel-239-82.0.13.el8_10.19.x86_64` が導入済みです (`/usr/include/systemd/*.h` および `/lib64/libsystemd.so.0` が存在し、CI の `ol8`/`ol9`/`ol10` コンテナーでも既存 `app/service-sample` のビルドにより導入済みです)。したがって `<systemd/*.h>` のビルド時利用自体は問題ありません。  
一方、`testfw` への mock 追加は (1) `libmock_libc.a` へ統合するとワークスペース全テストに `-lsystemd` 依存が波及するため `libmock_libssh.a` と同様に独立ライブラリ (`libmock_systemd.a`) とする設計論点、および (2) [Q-004](#q-004-linux-での-libsystemd-への依存方式) の `dlopen` / `dlsym` 経由の関数ポインター呼び出しは `include_override/systemd/*.h` のマクロ置換で自動捕捉できないという技術的整合論点が生じます。これらを [Q-020](#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) と [Q-021](#q-021-testfw-変更時のテスト検証範囲) で確認します。

**Related**

[Q-004](#q-004-linux-での-libsystemd-への依存方式)、[Q-020](#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合)、[Q-021](#q-021-testfw-変更時のテスト検証範囲)

### Q-015 install オプションでの watchdog 無効化とスレッド終了待ち上限

**Status**

解決済み

**Question**

[Q-009](#q-009-サービス登録パラメーターのカスタマイズ範囲) の `cplat_service_install_options` で `watchdog_sec` を変更可能にする際、Linux での watchdog 無効化指定と、`WATCHDOG_USEC` が 30 [s] 未満のときのイベント監視スレッド終了待ち上限 (現行 30 [s]) をどう扱いますか。

**Context**

- **経緯と背景**: 現行の `service-sample_linux_events.c` では `WatchdogSec=30` が固定だったため、イベント監視スレッド終了待ち上限 `SVC_EVENTS_JOIN_TIMEOUT_MS = 30000` に対して `static_assert(SVC_EVENTS_JOIN_TIMEOUT_MS <= (SVC_UNIT_WATCHDOG_SEC * 1000U))` が置かれていました。`cplat_service_install_options` で `watchdog_sec` を任意指定できるようにすると、(1) watchdog を無効にしたいサービス向けの `disable_watchdog` フラグの要否、および (2) `WatchdogSec=` が 30 [s] より短い環境 (例: 10 [s]) で `cplat_service_run()` が停止する際のイベント監視スレッド join 上限の扱い、を決める必要があります。
- **根拠資料**: [service-sample_linux_events.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux_events.c#L90-L108)
- **各案の具体例**:

```text
案 A (推奨): disable_watchdog フラグを設け、join タイムアウトは WATCHDOG_USEC に応じて動的調整する
- install: disable_watchdog != 0 のとき WatchdogSec= を出力しない。0 のときは watchdog_sec (0 なら既定 30 [s]) を出力。
- run    : イベント監視スレッドの join 上限は既定 30000 [ms]。
           sd_watchdog_enabled() で得た watchdog_usec が 30 [s] 未満なら、watchdog_usec / 1000 [ms] (下限 1000 [ms]) へ縮める。
```

```text
案 B: disable_watchdog フラグは設けるが、watchdog_sec は 30 [s] 以上のみ許可し join 上限は 30 [s] 固定
- install: watchdog_sec が 1..29 の場合は CPLAT_ERR_INVALID_ARGUMENT を返す。
```

```text
案 C: disable_watchdog は設けず、watchdog_sec は 30 [s] 以上のみ許可し join 上限は 30 [s] 固定
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `disable_watchdog` フラグを設け、`run` 時のイベント監視スレッド終了待ち上限は `WATCHDOG_USEC` が 30 [s] 未満のときに `min(30000 ms, watchdog_usec / 1000)` (下限 1000 [ms]) へ自動調整する。
- [ ] B: `disable_watchdog` フラグは設けるが、`watchdog_sec` は 30 [s] 以上のみ許可 (1〜29 は `CPLAT_ERR_INVALID_ARGUMENT`) し、終了待ち上限は 30 [s] 固定とする。
- [ ] C: `disable_watchdog` は設けず、`watchdog_sec` は 30 [s] 以上のみ許可し、終了待ち上限は 30 [s] 固定とする。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 23:10:48 JST

**Recommendation**

推奨: A  
優先した基準: systemd の `WatchdogSec=` 契約との実行時整合性 (手動編集された unit ファイルへの耐性を含む) と利用側の柔軟性優先。  
理由: 運用者が unit ファイルの `WatchdogSec=` を 10 [s] などに短く設定した場合でも、`run` 側が `sd_watchdog_enabled()` の実値に合わせて join 上限を縮めれば、watchdog 期限内に `CPLAT_ABORT` 判定へ到達できます。また `disable_watchdog` を設けることで、watchdog を使わないサービスへも対応できます。

**Assessment**

単一選択で A のみが選択されました。`cplat_service_install_options` に `disable_watchdog` を設け、実行時のイベント監視スレッド終了待ち上限は `WATCHDOG_USEC` が 30 [s] 未満のときに `min(30000 ms, watchdog_usec / 1000)` (下限 1000 [ms]) へ自動調整します。

**Related**

[Q-009](#q-009-サービス登録パラメーターのカスタマイズ範囲)、[Q-016](#q-016-install-時の-startlimit-と再起動遅延の整合性検査)

### Q-016 install 時の StartLimit と再起動遅延の整合性検査

**Status**

解決済み

**Question**

Linux の `cplat_service_install()` において、自動再起動が有効なときに `((start_limit_burst - 1) * restart_delay_sec) < start_limit_interval_sec` を満たさない設定をエラーとして拒否しますか。

**Context**

- **経緯と背景**: 現行の `service-sample_linux.c` は `static_assert(((SVC_START_LIMIT_BURST - 1U) * SVC_FAILURE_RESTART_DELAY_SEC) < SVC_START_LIMIT_INTERVAL_SEC)` により、「`RestartSec=` 間隔の再起動が `StartLimitIntervalSec=` 内に `StartLimitBurst=` 回到達できること (そうでないと起動回数制限が永遠に発動せず無限再起動になる)」を保証しています。`cplat_service_install_options` で `restart_delay_sec` 等を上書き可能にした場合、たとえば `restart_delay_sec = 20` のみを変更し `start_limit_interval_sec` (既定 60) と `start_limit_burst` (既定 5) を変えないと `(5 - 1) * 20 = 80 >= 60` となり制限が発動しなくなります。
- **根拠資料**: [service-sample_linux.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux.c#L80-L109)、[service-events.md](../../../../service-sample/docs/service-events.md#L352-L359)

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 自動再起動が有効な場合、`((burst - 1) * restart_delay_sec) >= interval_sec` (または乗算オーバーフロー) であれば `CPLAT_ERR_INVALID_ARGUMENT` を返して登録を拒否する。
- [ ] B: 到達可能性の相関検査ではエラーにせず、警告トレース (`CPLAT_TRACE_LEVEL_WARNING`) を出力して unit ファイルをそのまま生成する。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 23:10:48 JST

**Recommendation**

推奨: A  
優先した基準: 現行 `service-sample_linux.c` の不変条件維持と、意図しない無限再起動ループ設定の早期検出優先。  
理由: 到達不能な `StartLimit` は運用時に障害ループが止まらない事故の原因になりやすく、`install` 時点で `CPLAT_ERR_INVALID_ARGUMENT` として明示的に失敗させるほうが安全です。

**Assessment**

単一選択で A のみが選択されました。自動再起動有効時に `((burst - 1) * restart_delay_sec) >= interval_sec` (または乗算オーバーフロー) を検出した場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。

**Related**

[Q-009](#q-009-サービス登録パラメーターのカスタマイズ範囲)、[Q-015](#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限)

### Q-017 公開 API での cplat_error 出力引数の要否

**Status**

解決済み

**Question**

`cplat_service_*` の操作 API (`install` / `uninstall` / `prepare_run` / `run` / `run_console`) に、`cplat_error *detail_out` 出力引数を設けますか。

**Context**

- **経緯と背景**: `app/cplat` の公開 API では、`crt/file.h` や `net/socket.h` のように低水準 OS エラー詳細の個別受け取りが必要な API に `cplat_error *detail_out` 引数を設ける一方、`runtime/process.h`、`runtime/shutdown.h`、`runtime/elevated_process.h` のように高水準のプロセス・実行時制御 API には `detail_out` 引数を設けず、戻り値の共通結果コード (`CPLAT_OK` / `CPLAT_ERR_*`) で成否を返しています。`cplat_service_*` では [Q-005](#q-005-診断トレースの出力方式と所有権) で `def->tracer` への診断トレース出力も決まっているため、`detail_out` 引数の有無を確認します。
- **根拠資料**: [app/cplat/docs/coding-guideline.md](../../coding-guideline.md#L119-L188)、[process.h](../../../prod/include/cplat/runtime/process.h)、[elevated_process.h](../../../prod/include/cplat/runtime/elevated_process.h)
- **各案の具体例**:

```c
/* 案 A (推奨): detail_out 引数なし (runtime 系と同じ簡潔な署名。OS 詳細は cplat_error_get_last() と tracer で参照可能) */
CPLAT_EXPORT int CPLAT_API cplat_service_install(const cplat_service_definition *def,
                                                 const cplat_service_install_options *options);
CPLAT_EXPORT int CPLAT_API cplat_service_uninstall(const cplat_service_definition *def);
CPLAT_EXPORT int CPLAT_API cplat_service_run(const cplat_service_definition *def, int *exit_code_out);
```

```c
/* 案 B: 各操作 API の末尾に cplat_error *detail_out (NULL 可) を追加する */
CPLAT_EXPORT int CPLAT_API cplat_service_install(const cplat_service_definition *def,
                                                 const cplat_service_install_options *options,
                                                 cplat_error *detail_out);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `detail_out` 引数は設けず、戻り値 (`CPLAT_OK` / `CPLAT_ERR_*`)、OS エラー時のスレッド ローカル直前値 (`cplat_error_get_last()`)、および `def->tracer` への診断トレースで詳細を伝える。
- [ ] B: 操作 API (`install` / `uninstall` / `prepare_run` / `run` / `run_console`) の末尾に `cplat_error *detail_out` (NULL 可) 引数を設ける。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 23:10:48 JST

**Recommendation**

推奨: A  
優先した基準: `runtime/process.h`・`runtime/elevated_process.h` 等の高水準 API との署名統一と、呼び出し側の簡潔さ優先。  
理由: サービス操作は内部で複数の OS 呼び出しや外部コマンド (`systemctl`) を連鎖実行するため、単一の OS エラー値だけでは原因を言い尽くせず、実運用では `def->tracer` への詳細トレースが主たる診断経路になります。OS エラー起因の失敗時は `cplat_internal_error_report_*` でスレッド ローカル直前値 (`cplat_error_get_last()`) へも記録しておけば、必要時に詳細取得もでき、呼び出し側の引数が煩雑になりません。

**Assessment**

単一選択で A のみが選択されました。操作 API に `detail_out` 引数は設けず、戻り値・`cplat_error_get_last()`・`def->tracer` で詳細を伝えます。

**Related**

[Q-005](#q-005-診断トレースの出力方式と所有権)、[Q-006](#q-006-サービス操作-api-の戻り値規約)

### Q-018 改修後の service-sample における Windows 権限昇格方式

**Status**

解決済み

**Question**

[Q-008](#q-008-登録と解除における自動権限昇格の責務) および [Q-011](#q-011-service-sample-本体の改修と資産移行の範囲) を受けて書き換える `app/service-sample` の CLI 層では、Windows の `install` / `uninstall` 時の権限昇格をどう実装しますか。

**Context**

- **経緯と背景**: 現行の `app/service-sample` は、`main()` 冒頭で `cplat_console_attach_parent()` を呼び、`install` / `uninstall` 時に `cplat_elevated_process_run_if_needed()` で UAC 昇格再実行しています。一方、`app/cplat/prod/include/cplat/runtime/elevated_process.h` には、`cplat_elevated_process_run_if_needed()` の親コンソール再接続に Windows 実機での間欠的な `ERROR_INVALID_HANDLE` 障害があることが記載されており、その対策として `cplat_elevated_process_run_piped()` + `cplat_elevated_process_attach_output_pipes()` が用意されています。今回の `app/service-sample` 書き換えに合わせて昇格方式をどうするか確認します。
- **根拠資料**: [service-sample.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample.c#L103-L110)、[elevated_process.h](../../../prod/include/cplat/runtime/elevated_process.h#L190-L260)

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `app/service-sample` の CLI 層で `cplat_elevated_process_run_piped()` と `cplat_elevated_process_attach_output_pipes()` を使った昇格再実行へ更新する (Linux は未昇格時にエラーを返す)。
- [ ] B: `app/service-sample` の CLI 層で、現行と同じ `cplat_elevated_process_run_if_needed()` と `cplat_console_attach_parent()` による昇格再実行を維持する。
- [ ] C: `app/service-sample` では自動昇格を行わず、`cplat_service_install()` / `uninstall()` が `CPLAT_ERR_PERMISSION_DENIED` を返した際に管理者権限または `sudo` での再実行を案内して終了する。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 23:10:48 JST

**Recommendation**

推奨: A  
優先した基準: `cplat` 自身が推奨する昇格 API の模範例としての品質と、Windows 実機での間欠的コンソール出力失敗の回避優先。  
理由: `app/service-sample` は `cplat` を使ったサービスの模範実装であり、昇格処理を CLI 層へ引き上げるこの機に `cplat_elevated_process_run_piped()` へ移行すれば、リダイレクト時の出力保持や `ERROR_INVALID_HANDLE` 回避の良いサンプルになります。

**Assessment**

単一選択で A のみが選択されました。改修後の `app/service-sample` の CLI 層では `cplat_elevated_process_run_piped()` と `cplat_elevated_process_attach_output_pipes()` による昇格再実行を採用します。

**Related**

[Q-008](#q-008-登録と解除における自動権限昇格の責務)、[Q-011](#q-011-service-sample-本体の改修と資産移行の範囲)

### Q-019 移設後の旧ドキュメント ファイルの扱い

**Status**

解決済み

**Question**

[Q-011](#q-011-service-sample-本体の改修と資産移行の範囲) により `service-events.md` と `systemd-oomkiller-advisory.md` の正本を `app/cplat/docs/` へ移設した後、`app/service-sample/docs/` にあった元の 2 ファイルをどう扱いますか。

**Context**

- **経緯と背景**: 現在 `app/service-sample/docs/` には `service-events.md` と `systemd-oomkiller-advisory.md` が配置されており、`app/service-sample/prod/src/cmd/service-sample/README.md` や `app/service-sample/docs/README.md` の `\toc` から参照されています。`app/cplat/docs/` へ移設して正本を 1 か所に集約するにあたり、元の 2 ファイルを削除して README 類から `app/cplat/docs/` へリンクするか、それとも元のパスに薄い案内ファイル (移設先リンクのみ) を残すかを確認します。
- **根拠資料**: [app/service-sample/docs/README.md](../../../../service-sample/docs/README.md)、[app/service-sample/prod/src/cmd/service-sample/README.md](../../../../service-sample/prod/src/cmd/service-sample/README.md#L30)、[app/cplat/docs/README.md](../../README.md)

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `app/service-sample/docs/service-events.md` と `app/service-sample/docs/systemd-oomkiller-advisory.md` は削除し、`app/service-sample/docs/README.md` および `app/service-sample/prod/src/cmd/service-sample/README.md` から `app/cplat/docs/` の正本へリンクする。
- [ ] B: `app/service-sample/docs/` に 2 ファイルを薄い案内ページ (移設先リンクのみ) として残す。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-11-27 23:10:48 JST

**Recommendation**

推奨: A  
優先した基準: 正本の一元化と不要な空ページ・二重管理の排除優先。  
理由: リポジトリ内からの参照箇所は `service-sample` 内の README とソース コメントに限られており、一括でリンクを張り替えられるため、旧ファイルを削除して `app/cplat/docs/` を唯一の正本にするほうが構成が明瞭です。

**Assessment**

単一選択で A のみが選択されました。`app/service-sample/docs/` の旧 2 ファイルは削除し、`app/service-sample/docs/README.md` と `app/service-sample/prod/src/cmd/service-sample/README.md` から `app/cplat/docs/` の正本へリンクします。

**Related**

[Q-011](#q-011-service-sample-本体の改修と資産移行の範囲)

### Q-020 libsystemd モックの配置先と dlopen 呼び出し経路の整合

**Status**

未回答

**Question**

[Q-014](#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存) の補足 (「`testfw` にも mock を追加し、C 標準ライブラリ同等でよい」) と、[Q-004](#q-004-linux-での-libsystemd-への依存方式) の `dlopen` 決定を整合させるため、`libsystemd` のモック配置先と呼び出し経路をどの方式にしますか。

**Context**

- **経緯と背景**:
    1. **環境確認の結果**: 現在の環境は Oracle Linux 8.10 であり、`systemd-devel-239-82.0.13.el8_10.19.x86_64` (`/usr/include/systemd/*.h` および `/lib64/libsystemd.so.0`) が導入済みです。CI (`.github/workflows/ci.yml`) の `ol8`/`ol9`/`ol10` コンテナーにも導入済みのため、ビルド時に `<systemd/*.h>` をインクルードすること自体は問題ありません。
    2. **`libmock_libc.a` と別ライブラリの違い**: `framework/testfw` では、全 C/C++ テストがリンクする `libmock_libc.a` とは別に、特定ライブラリ向けのモック (`libmock_libssh.a`、`libmock_syslog.so`) を独立ライブラリへ分離しています。`libmock_libc.a` へ `mock_systemd` を含めると、`delegate_real_sd_*` が実 `libsystemd` を参照するためワークスペースの全テストに `LIBS += systemd` が必要になります。独立ライブラリ `libmock_systemd.a` (`framework/testfw/libsrc/mock_systemd/`、`SKIP_BUILD = WINDOWS`) であれば、必要なテストだけが `LIBS += mock_systemd systemd` をリンクできます。
    3. **`dlopen` 方式 ([Q-004](#q-004-linux-での-libsystemd-への依存方式) 案 A) と `include_override` マクロの不整合**: `testfw` の C 標準ライブラリ同等モック (`include_override/systemd/*.h` による `#define sd_notify(...) mock_sd_notify(...)`) は、製品コードが関数名 `sd_notify(...)` を**直接呼び出す**前提の仕組みです。一方、[Q-004](#q-004-linux-での-libsystemd-への依存方式) で決定した `dlopen("libsystemd.so.0")` + `dlsym()` 方式では、製品コードが関数ポインター (`api->sd_notify(...)`) 経由で呼ぶため、`include_override` のマクロ置換では自動捕捉されません。
- **根拠資料**: [framework/testfw/libsrc/makepart.mk](../../../../../framework/testfw/libsrc/makepart.mk)、[framework/testfw/include/mock_libssh.h](../../../../../framework/testfw/include/mock_libssh.h)、[framework/testfw/include/mock_dlfcn.h](../../../../../framework/testfw/include/mock_dlfcn.h)、[app/service-sample/test/include/mock_systemd.h](../../../../service-sample/test/include/mock_systemd.h)
- **各案の具体例**:

```text
案 A (推奨): Q-004 (dlopen) と Q-014 (ビルド時 <systemd/*.h> 参照) を維持し、framework/testfw に独立ライブラリ libmock_systemd.a を追加する
- framework/testfw:
  - libsrc/mock_systemd/ (SKIP_BUILD = WINDOWS)、include/mock_systemd.h、include_override/systemd/ を追加
    (app/service-sample/test/ から移設し、libmock_libssh.a と同等の独立ライブラリ libmock_systemd.a としてビルド)。
- app/cplat:
  - 製品ビルド (libcplat.so) は <systemd/*.h> で型・定数を参照しつつ、実行時は dlopen("libsystemd.so.0") + dlsym() でロード (DT_NEEDED なし)。
  - 単体テスト時は、cplat の libsystemd ローダーのテスト用差し替え口 (または mock_dlfcn の dlsym) から
    libmock_systemd.a の Mock_systemd へ委譲する薄い中継関数テーブルを接続する。
- メリット: libcplat.so に libsystemd 実行時依存を増やさず、かつ testfw 側でも再利用可能な Mock_systemd を標準整備できる。
```

```text
案 B: Q-004 (dlopen) と Q-014 (ビルド時 <systemd/*.h> 参照) を維持し、framework/testfw は変更しない
- libsystemd のモックは app/cplat/test/ 内に閉じたテスト用関数テーブル差し替え (Google Mock) として配置する。
- メリット: framework/testfw (別サブモジュール) の変更と framework 変更に伴う全体テスト範囲の拡大を回避できる。
```

```text
案 C: Q-004 の方針を「直接リンク (LIBS += systemd)」へ変更し、framework/testfw に libmock_systemd.a + include_override/systemd/ を追加する
- libcplat.so が libsystemd を直接リンクし、製品コードは sd_notify() 等を直接呼び出す。
- テスト時は testfw の include_override/systemd/*.h が C 標準ライブラリとまったく同じ仕組みで mock_sd_* へ置換する。
- デメリット: libcplat.so の DT_NEEDED に libsystemd.so.0 が追加され、全 cplat 利用側へ実行時依存が波及する。
```

**Answer**

選択方式: 単一選択

- [ ] A: (推奨) `Q-004` (`dlopen`) と `Q-014` (ビルド時 `<systemd/*.h>` 参照) を維持し、`framework/testfw` に独立ライブラリ `libmock_systemd.a` (`mock_libssh` 方式) を追加して、`cplat` テストからはローダー差し替え口経由で `Mock_systemd` へ接続する。
- [ ] B: `Q-004` (`dlopen`) と `Q-014` (ビルド時 `<systemd/*.h>` 参照) を維持し、`framework/testfw` は変更せず、`app/cplat/test/` 内のテスト用ディスパッチ表モックで検証する。
- [ ] C: `Q-004` を「`libcplat.so` への `libsystemd` 直接リンク」へ変更し、`framework/testfw` に `libmock_systemd.a` + `include_override/systemd/` を追加して C 標準ライブラリと同一のマクロ置換でテストする。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

**Recommendation**

推奨: A  
優先した基準: `libcplat.so` の実行時非依存 ([Q-004](#q-004-linux-での-libsystemd-への依存方式) 案 A) の維持と、ユーザーの「`testfw` に `mock_systemd` を標準整備し、ビルド時 `<systemd/*.h>` を活用する」意図の両立優先。  
理由: `libmock_systemd.a` を `libmock_libssh.a` と同じ独立ライブラリとして `framework/testfw` に置けば、他の全テストへ `-lsystemd` 依存を波及させずに済みます。また、`cplat` の `dlopen` ローダーに内部差し替え口を設けてテスト時に `Mock_systemd` へつなぐことで、`libcplat.so` の `DT_NEEDED` 非依存と `testfw` 上での `Mock_systemd` 再利用を両方満たせます。

**Related**

[Q-004](#q-004-linux-での-libsystemd-への依存方式)、[Q-014](#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存)、[Q-021](#q-021-testfw-変更時のテスト検証範囲)

### Q-021 testfw 変更時のテスト検証範囲

**Status**

未回答

**Question**

[Q-020](#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) で `framework/testfw` へ `libmock_systemd.a` を追加する場合、Linux 上でのテスト検証範囲をどこまで実施しますか。

**Context**

- **経緯と背景**: ルートの `AGENTS.md` では「framework または複数 app にまたがる全体テストは、コストを説明してからユーザーに確認してください」と定めています。`framework/testfw` へ独立ライブラリ `libmock_systemd.a` と `include_override/systemd/` を新設する場合、既存の `libmock_libc.a` や他の `include_override/*.h` は変更しないため、実質的な影響範囲は `framework/testfw` 自身のビルド・テスト、および `mock_systemd` を利用する `app/cplat`・`app/service-sample` に限られます。一方、ワークスペース全体の `make test` は全 app (約 20 個) のビルドとテストを伴うため所要時間が大きくなります。
- **根拠資料**: [AGENTS.md](../../../../../AGENTS.md)、[framework/testfw/AGENTS.md](../../../../../framework/testfw/AGENTS.md)

**Answer**

選択方式: 単一選択

- [ ] A: (推奨) 影響する範囲の局所検証 (`framework/testfw` のビルドおよび `mock_systemd` 局所テスト + `app/cplat` のビルド・全単体テスト + `app/service-sample` のビルド・全単体テスト) を実施し、ワークスペース全体テストは CI に委ねる (Q-020 で B を選んだ場合は `framework/testfw` 検証自体が不要)。
- [ ] B: 上記の局所検証に加えて、Linux 上でワークスペース全体の `make test` まで実施する。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

**Recommendation**

推奨: A  
優先した基準: 変更が独立ライブラリ (`libmock_systemd.a`) と新規 `include_override/systemd/` に局所化される性質を踏まえた、現実的な検証コストと確実な品質担保の両立優先。  
理由: 既存の `libmock_libc.a` や既存の `include_override/*.h` に手を入れないため、他 app のテストへ破壊的影響が及ぶ経路がありません。影響範囲 3 か所 (`framework/testfw`、`app/cplat`、`app/service-sample`) のビルドと全テストをローカルで通し、全 app 横断の回帰は CI (`ol8`/`ol9`/`ol10` + Windows) で確認する運用が妥当です。

**Related**

[Q-013](#q-013-検証範囲と-windows-確認の扱い)、[Q-020](#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合)
