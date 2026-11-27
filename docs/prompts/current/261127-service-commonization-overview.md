<!--
生成元: agree-before-implement スキル
文書種別: 全体管理文書 (overview)
最終更新日時: 2026-11-27 23:13:00 JST
最終更新エージェント: agy / セッション ID: a422744b-85f9-4bdc-914d-195c5ca8a120
稼働 OS: Linux (WSL)
下流文書: `261127-service-commonization-discussion.md` (検討用 Markdown)、`261127-service-commonization-lightadr.md` (簡易 ADR)、`261127-service-commonization-design.md` (最終仕様)、`261127-service-commonization-backlog.md` (残課題一覧)。未作成の場合や、削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。この文書と、ユーザーが編集した最新の検討用 Markdown を読み直し、作業 WBS の `← **CURRENT**` の項目から再開します。最終更新エージェントのセッション情報や稼働 OS・最終更新日時が参照可能であれば再開時のコンテキストや環境差分の把握に利用し、別エージェントや別環境・別 OS からの引き継ぎ、担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。`← **CURRENT**` の項目は処理が中止された項目である場合があり、どこまで進んでいたかは文書だけでは分かりません。対象成果物の内容を確認して実施済みの範囲を特定してから、作業を再開してください。`← **CURRENT**` がなく、作業 WBS がすべて完了 (✅) であれば、この案件の合意フローは完了しています。
-->

# service-sample の共通化可能部分の cplat 取り込み 全体管理

ファイル名: `261127-service-commonization-overview.md`

この文書は、`app/service-sample` の共通化可能部分を `app/cplat` に取り込む案件の識別、作業 WBS、対象範囲、確認状況、経緯を管理します。  
質問と回答の本文は検討用 Markdown (`261127-service-commonization-discussion.md`) に、決定事項は簡易 ADR (`261127-service-commonization-lightadr.md`) に記載し、この文書には重複して記載しません。

## 案件の識別

検討 ID: 261127-service-commonization

検討用 Markdown の版: 3

状態: 検討中

## 作業 WBS と進捗管理

この案件の作業 WBS です。  
検討用 Markdown のための情報収集と質問の実施、残懸念がないことの確認 (第 1 ゲート: 最終仕様作成への着手)、最終決定事項 (最終仕様) を記載した `261127-service-commonization-design.md` の作成、最終仕様の承認の受付 (第 2 ゲート)、承認された最終仕様に従った作業の実施、および作業完了後の成果物の承認の受付 (第 3 ゲート) に至る詳細な手順を記載します。  
各ゲートの通過・承認および作業の進行に従って、小項目 (末端項目) 1 件ごとに着手直前 (🔄 への更新と `← **CURRENT**` の付け替え) と完了直後 (✅ への更新と親項目の状態再判定) に都度この文書を更新します。  
決定事項は、Q を解決済みにした時点、またはユーザーが決定を明示した時点で `261127-service-commonization-lightadr.md` へ反映します。  
見送りの決定や範囲外の気付きは、どの段階でも生じた時点で `261127-service-commonization-backlog.md` に追記します。

凡例: ⏸️ 未着手、🔄 実施中、🚫 ブロック中、⚠️ 要注意、✅ 完了。現在取り組んでいる項目の末尾に `← **CURRENT**` を付けます。

- 🔄 1. 検討用 Markdown のための情報収集と質問の実施
    - ✅ 1.1. 前提条件と要求事項の調査 (`app/service-sample` の全実装・テスト・仕様文書、`app/cplat` の `AGENTS.md`・規範・リンク方針・機能仕様・既存 API の調査)
    - ✅ 1.2. 課題・論点の抽出と明確化のための確認項目 ([Q-001](261127-service-commonization-discussion.md#q-001-機能カテゴリの配置と命名) から [Q-013](261127-service-commonization-discussion.md#q-013-検証範囲と-windows-確認の扱い)) および初期簡易 ADR (`261127-service-commonization-lightadr.md`) の作成
    - ✅ 1.3. 版 1 のユーザー回答の受付と記録 (2026-11-27 19:28:39 JST、Tetsuo Honda、検討用 Markdown の保存時刻)
    - ✅ 1.4. 版 1 の回答に基づく前提・制約の検証、簡易 ADR (`LADR-002` 〜 `LADR-013`) への反映、残課題 (`BL-001`) の追記、WBS の具体化
    - ✅ 1.5. 詳細設計論点に関する追加確認項目 ([Q-014](261127-service-commonization-discussion.md#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存) から [Q-019](261127-service-commonization-discussion.md#q-019-移設後の旧ドキュメント-ファイルの扱い)) の作成 (版 2)
    - ✅ 1.6. 版 2 のユーザー回答の受付と記録 (2026-11-27 23:10:48 JST、Tetsuo Honda、検討用 Markdown の保存時刻)
    - ✅ 1.7. 版 2 の回答に基づく検証・簡易 ADR 更新 (Q-015 〜 Q-019 を解決済みとし `LADR-007` / `LADR-009` / `LADR-011` へ反映。Q-014 は補足確認のため未解決とし環境・`framework/testfw` 構成を調査)
    - ✅ 1.8. Q-014 の補足に伴う追加確認項目 ([Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合)、[Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲)) の作成 (版 3)
    - 🔄 1.9. 版 3 のユーザー回答の受付と記録 (JST 日時、Git ユーザー名または発言者) ← **CURRENT**
    - ⏸️ 1.10. 版 3 の回答に基づく前提・制約の検証、簡易 ADR への決定事項の反映、WBS の見直し
- ⏸️ 2. 前提合意と残懸念の解消 (第 1 ゲート: 最終仕様作成への着手)
    - ⏸️ 2.1. 全論点に対する未解決事項および質問の解消確認
    - ⏸️ 2.2. 方針・前提条件の合意確定 (簡易 ADR の決定事項と、根拠となった解決済み Q の回答またはユーザーの明示的な指示との一致を確認し、懸念がないことをもってフェーズ 2 へ着手)
- ⏸️ 3. 最終決定事項 (最終仕様) の作成と承認の受付
    - ⏸️ 3.1. `261127-service-commonization-design.md` の作成 (公開 API、型、結果コード、OS 別実装方式、`libsystemd` 依存・モック方式、テスト・文書移行計画、知見の反映計画)
    - ⏸️ 3.2. 最終仕様における受入条件と整合性の確認 (`app/cplat`、`app/service-sample`、および対象に含める場合は `framework/testfw` の各種規範との整合確認)
    - ⏸️ 3.3. 対話セッションにおける最終仕様 (`261127-service-commonization-design.md`) の承認の受付 (第 2 ゲート: 作業着手の合意)
- ⏸️ 4. 承認された最終仕様に従った作業の実施 (Q-020 / Q-021 の結果に応じて `framework/testfw` 項目を確定する)
    - ⏸️ 4.1. `framework/testfw` への `libmock_systemd.a` (`include/mock_systemd.h`、`include_override/systemd/`、`libsrc/mock_systemd/`) 追加 ([Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) の結果に従う)
    - ⏸️ 4.2. `app/cplat` の機能仕様の追加・更新 (`docs/functional-spec/service.md`、`docs/functional-spec/README.md`、`docs/functional-spec-guideline.md`)
    - ⏸️ 4.3. `app/cplat` の公開ヘッダー・内部ヘッダー・アンブレラ ヘッダーの作成・更新 (`prod/include/cplat/service/service.h`、`prod/include/cplat.h`、`prod/include_internal/cplat/service/`)
    - ⏸️ 4.4. `app/cplat` の `service` モジュール実装 (`prod/libsrc/cplat/service/` の共通・Linux・Linux イベント・`libsystemd` 連携・Windows 実装、および `makefile` / `makelocal.mk` 更新)
    - ⏸️ 4.5. `app/cplat` の `exportTest.cc`、`mock_cplat` (`test/include/mock_cplat.h`、`test/libsrc/mock_cplat/mock_cplat.cc`、`test/libsrc/mock_cplat/service/`) の更新
    - ⏸️ 4.6. `app/cplat` の単体テストの作成・移植 (`test/src/libcplatTest/service/` の共通・Linux・Windows テスト群)
    - ⏸️ 4.7. `app/service-sample` の `cplat/service/service.h` 利用への書き換え (CLI 層の `run_piped` 昇格を含む)、重複実装・`mock_systemd`・重複テストの削除、およびサンプル向けテストの整理
    - ⏸️ 4.8. 仕様文書と正本ドキュメントの更新・移設 (`app/cplat/docs/service-events.md`、`app/cplat/docs/systemd-oomkiller-advisory.md`、`app/cplat/docs/README.md`、`app/cplat/docs/api-cheatsheet.md`、`app/cplat/docs/link-policy.md`、`app/service-sample/docs/` 旧 2 ファイル削除および `README.md` 更新)
    - ⏸️ 4.9. Linux 上でのビルド、`.warn` 確認、単体テスト、`check_functional_spec.py`、`text_style_jp.py` による機械的検証の実施 ([Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) の結果に従う)
    - ⏸️ 4.10. 受入条件への適合確認 (Windows 確認が環境制約により未確認である旨の整理を含む)
    - ⏸️ 4.11. 簡易 ADR (`261127-service-commonization-lightadr.md`) の最終確認 (各決定が案件終了時点の決定を示し、経緯が時系列にそろっていることの確認)
    - ⏸️ 4.12. 残課題一覧 (`261127-service-commonization-backlog.md`) の再構築と整理 (起票できる詳細度への書き直しと `整理状態: 整理済み` への更新)
- ⏸️ 5. 成果物の承認の受付 (第 3 ゲート)
    - ⏸️ 5.1. 成果物・検証結果 (Windows 未確認項目を含む)・残課題の提示
    - ⏸️ 5.2. 対話セッションにおける成果物の承認の受付 (「承認」または「完了」の受領によるスキル完了)

## 対象と参照資料

対象成果物: `app/cplat` の新設カテゴリ `service` (機能仕様、公開ヘッダー、実装、`exportTest`、`mock_cplat`、単体テスト、移設文書・関連正本ドキュメント)、`app/service-sample` の書き換え (薄いサンプル実装、サンプル向けテスト、旧 2 文書削除・参照先更新)、および [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) の決定に応じて `framework/testfw` (`libmock_systemd.a`)。

今回の対象範囲: `app/service-sample` のクロスプラットフォーム サービス基盤を `app/cplat` の独立カテゴリ `service` (プロセス グローバル API、`cplat_service_*`、共通結果コード、`detail_out` なし、`cplat_tracer *` 外部注入、既定値付き `cplat_service_install_options`、自動昇格なし) として取り込み、`app/service-sample` を新 API + `run_piped` 昇格利用へ書き換えて仕様文書 2 件を `app/cplat/docs/` へ移設すること。`libsystemd` モックの配置先と `dlopen` 呼び出し経路の整合、および `framework/testfw` 変更時の検証範囲は [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) と [Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) で確定します。

書き込み可能な検討文書: この文書 (`261127-service-commonization-overview.md`)、検討用 Markdown (`261127-service-commonization-discussion.md`)、簡易 ADR (`261127-service-commonization-lightadr.md`)、残課題一覧 (`261127-service-commonization-backlog.md`)。

参照資料と版: 2026-11-27 時点の作業ツリー。親ワークスペース `c-modernization-kit` `a2aba192` (`app/service-sample` を含む)、サブモジュール `app/cplat` `05dcfcb7`。環境は Oracle Linux 8.10 (`systemd-devel-239-82.0.13.el8_10.19.x86_64` 導入済み)。CI (`.github/workflows/ci.yml`) は `oracle-linux-8-dev` / `9-dev` / `10-dev` コンテナーを使用。

Table: 調査時に参照した主要資料と対象ファイル

| 参照資料 | 確認した要点 |
| --- | --- |
| [service-sample.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample.h) | 公開・内部混在の `svc_*` API、コールバック型、イベント型、watchdog 契約 |
| [service-sample_internal.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample_internal.h) | 共通定数 (`SVC_EXEC_PATH_MAX`、`SVC_FAILURE_RESTART_DELAY_SEC`、`SVC_UNIT_WATCHDOG_SEC`) |
| [service-sample.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample.c) | `main()` 引数解析、グローバル `s_tracer`、停止抽象、ライフサイクル駆動、イベント配送 |
| [service-sample-impl.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample-impl.c) | 利用側雛形 (`g_service_def` と 5 コールバックのサンプル実装) |
| [service-sample_linux.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux.c) | `SIGHUP` 事前ブロック、`sd_notify`、systemd unit 生成・`systemctl` 操作、OOM killer 対策 |
| [service-sample_linux_events.h](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux_events.h) | Linux イベント監視スレッドの内部インターフェイス |
| [service-sample_linux_events.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_linux_events.c) | `sd_event` ループ、logind D-Bus 監視、delay inhibitor lock、`SIGHUP`、watchdog 応答 |
| [service-sample_windows.c](../../../../service-sample/prod/src/cmd/service-sample/service-sample_windows.c) | Windows SCM ディスパッチャー、`ServiceCtrlHandler`、`CreateServiceU` / `DeleteService` |
| [service-events.md](../../../../service-sample/docs/service-events.md) | 両 OS のイベント対応表、通知ロック契約、自動再起動の仕様 |
| [systemd-oomkiller-advisory.md](../../../../service-sample/docs/systemd-oomkiller-advisory.md) | `OOMScoreAdjust=-1000` と systemd 248 以降の `ManagedOOMPreference=omit` の根拠 |
| [app/service-sample/prod/src/cmd/makepart.mk](../../../../service-sample/prod/src/cmd/makepart.mk) | Linux での `pkg-config --exists libsystemd` 検査と `LIBS += systemd pthread` |
| [mock_systemd.h](../../../../service-sample/test/include/mock_systemd.h) | `libsystemd` (`sd-event`、`sd-daemon`、`sd-bus`) の既存 Google Mock 定義 |
| [app/cplat/AGENTS.md](../../../AGENTS.md) | 公開 API と `api-cheatsheet.md`・`exportTest.cc`・`mock_cplat`・機能仕様の同期規則 |
| [app/cplat/docs/coding-guideline.md](../../coding-guideline.md) | 共通結果コード (`CPLAT_OK` / `CPLAT_ERR_*`)、`cplat_error`、代替 CRT 関数 (`cplat_fopen`、`cplat_snprintf`、`strerror` 禁止) |
| [app/cplat/docs/link-policy.md](../../link-policy.md) | 単一動的ライブラリ (`libcplat.so` / `libcplat.dll`) の方針、依存ライブラリ一覧、内部 API 非エクスポート |
| [app/cplat/docs/platform-abstraction-guideline.md](../../platform-abstraction-guideline.md) | `PLATFORM_*` / `COMPILER_*` 分岐、ファイル分割、公開ヘッダー規則 |
| [app/cplat/docs/functional-spec/README.md](../../functional-spec/README.md) | 現行の 18 機能カテゴリ一覧 |
| [app/cplat/docs/functional-spec-guideline.md](../../functional-spec-guideline.md) | 要件 ID (`CPLAT-<CATEGORY>-<TYPE>-NNN`) とカテゴリ別主語表 |
| [app/cplat/docs/mock-cplat-guideline.md](../../mock-cplat-guideline.md) | `mock_cplat` への公開関数追加規則 |
| [app/cplat/prod/libsrc/cplat/exports.map](../../../prod/libsrc/cplat/exports.map) | `libcplat.so` のエクスポート対象シンボル (`cplat_*` / `_cplat_*`) |
| [app/cplat/prod/libsrc/cplat/makepart.mk](../../../prod/libsrc/cplat/makepart.mk) | 現行の Linux リンク先 (`cjson_app`、`zlib_app`、`crypto_app`、`dl`、`stdc++`) |
| [framework/testfw/AGENTS.md](../../../../../framework/testfw/AGENTS.md) | `testfw` の構成 (`include/`、`include_override/`、`libsrc/`、`lib/`) と作業規則 |
| [framework/testfw/include/mock_libssh.h](../../../../../framework/testfw/include/mock_libssh.h) | 独立モック ライブラリ (`libmock_libssh.a`) の既存構成例 |
| [framework/testfw/include/mock_dlfcn.h](../../../../../framework/testfw/include/mock_dlfcn.h) | `dlopen` / `dlsym` の既存モック定義 |
| [cplat-account-session-discussion.md](cplat-account-session-discussion.md) | 並行検討中の `account` / `session` 案件 (`libsystemd` の `dlopen` 案 Q-017 を含む) |

## 観点別の確認

Table: 6 観点ごとの確認状況と関連 Q

| 観点 | 状態 | 根拠・関連 Q |
| --- | --- | --- |
| 目的・利用者 | 確認済み | [Q-001](261127-service-commonization-discussion.md#q-001-機能カテゴリの配置と命名)、[Q-002](261127-service-commonization-discussion.md#q-002-api-の状態モデル)、[Q-003](261127-service-commonization-discussion.md#q-003-cli-エントリ-ポイントの取り込み範囲)、[Q-009](261127-service-commonization-discussion.md#q-009-サービス登録パラメーターのカスタマイズ範囲) (いずれも解決済み) |
| 範囲・境界 | 未解決 | Q-003、Q-005、Q-008、Q-009、Q-011、Q-012、Q-018、Q-019 は解決済み。`framework/testfw` を改修範囲に含めるかを [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) で確認中 |
| 入力・根拠 | 確認済み | 上記「参照資料と版」の全ソース・テスト・仕様文書・規範・環境パッケージ (`systemd-devel-239`)・CI 構成を確認済み |
| 整合性・依存関係 | 未解決 | Q-001、Q-004、Q-005、Q-006、Q-007、Q-010、Q-012、Q-017 は解決済み。[Q-014](261127-service-commonization-discussion.md#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存) の補足に伴う `dlopen` と `testfw` モックの整合を [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) で確認中 |
| 実現性・例外 | 確認済み | Q-004、Q-006、Q-007、Q-008、Q-010、Q-013、Q-015、Q-016 (いずれも解決済み) |
| 受入条件・検証 | 未解決 | Q-011、Q-013 は解決済み。`framework/testfw` 変更時のテスト検証範囲を [Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) で確認中 |

## 質問一覧

各 Q の本文は検討用 Markdown (`261127-service-commonization-discussion.md`) にあります。

Table: 確認項目の一覧と状態

| Q-ID | 論点 | 状態 | 依存・関連 | 反映先の決定 |
| --- | --- | --- | --- | --- |
| [Q-001](261127-service-commonization-discussion.md#q-001-機能カテゴリの配置と命名) | 機能カテゴリの配置と命名 | 解決済み | なし | [LADR-002](261127-service-commonization-lightadr.md#ladr-002-機能カテゴリの配置と命名) |
| [Q-002](261127-service-commonization-discussion.md#q-002-api-の状態モデル) | API の状態モデル | 解決済み | [Q-005](261127-service-commonization-discussion.md#q-005-診断トレースの出力方式と所有権) | [LADR-003](261127-service-commonization-lightadr.md#ladr-003-api-の状態モデル) |
| [Q-003](261127-service-commonization-discussion.md#q-003-cli-エントリ-ポイントの取り込み範囲) | CLI エントリ ポイントの取り込み範囲 | 解決済み | [Q-008](261127-service-commonization-discussion.md#q-008-登録と解除における自動権限昇格の責務)、[Q-009](261127-service-commonization-discussion.md#q-009-サービス登録パラメーターのカスタマイズ範囲) | [LADR-004](261127-service-commonization-lightadr.md#ladr-004-cli-エントリ-ポイントの責務境界) |
| [Q-004](261127-service-commonization-discussion.md#q-004-linux-での-libsystemd-への依存方式) | Linux での libsystemd への依存方式 | 解決済み | [Q-012](261127-service-commonization-discussion.md#q-012-並行検討中の-account-session-案件との調整)、[Q-014](261127-service-commonization-discussion.md#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存)、[Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) | [LADR-005](261127-service-commonization-lightadr.md#ladr-005-linux-での-libsystemd-動的ロード方式) |
| [Q-005](261127-service-commonization-discussion.md#q-005-診断トレースの出力方式と所有権) | 診断トレースの出力方式と所有権 | 解決済み | [Q-002](261127-service-commonization-discussion.md#q-002-api-の状態モデル)、[Q-010](261127-service-commonization-discussion.md#q-010-linux-の-sighup-事前ブロックの公開設計) | [LADR-006](261127-service-commonization-lightadr.md#ladr-006-診断トレースの出力方式と-cplat_tracer-の所有権) |
| [Q-006](261127-service-commonization-discussion.md#q-006-サービス操作-api-の戻り値規約) | サービス操作 API の戻り値規約 | 解決済み | [Q-007](261127-service-commonization-discussion.md#q-007-停止待機-api-の戻り値とタイムアウト仕様)、[Q-017](261127-service-commonization-discussion.md#q-017-公開-api-での-cplat_error-出力引数の要否) | [LADR-007](261127-service-commonization-lightadr.md#ladr-007-サービス操作-api-と停止待機-api-の戻り値規約) |
| [Q-007](261127-service-commonization-discussion.md#q-007-停止待機-api-の戻り値とタイムアウト仕様) | 停止待機 API の戻り値とタイムアウト仕様 | 解決済み | [Q-006](261127-service-commonization-discussion.md#q-006-サービス操作-api-の戻り値規約) | [LADR-007](261127-service-commonization-lightadr.md#ladr-007-サービス操作-api-と停止待機-api-の戻り値規約) |
| [Q-008](261127-service-commonization-discussion.md#q-008-登録と解除における自動権限昇格の責務) | 登録と解除における自動権限昇格の責務 | 解決済み | [Q-003](261127-service-commonization-discussion.md#q-003-cli-エントリ-ポイントの取り込み範囲)、[Q-018](261127-service-commonization-discussion.md#q-018-改修後の-service-sample-における-windows-権限昇格方式) | [LADR-008](261127-service-commonization-lightadr.md#ladr-008-登録と解除における自動権限昇格の責務境界) |
| [Q-009](261127-service-commonization-discussion.md#q-009-サービス登録パラメーターのカスタマイズ範囲) | サービス登録パラメーターのカスタマイズ範囲 | 解決済み | [Q-003](261127-service-commonization-discussion.md#q-003-cli-エントリ-ポイントの取り込み範囲)、[Q-015](261127-service-commonization-discussion.md#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限)、[Q-016](261127-service-commonization-discussion.md#q-016-install-時の-startlimit-と再起動遅延の整合性検査) | [LADR-009](261127-service-commonization-lightadr.md#ladr-009-サービス登録パラメーターのカスタマイズ方式) |
| [Q-010](261127-service-commonization-discussion.md#q-010-linux-の-sighup-事前ブロックの公開設計) | Linux の SIGHUP 事前ブロックの公開設計 | 解決済み | [Q-003](261127-service-commonization-discussion.md#q-003-cli-エントリ-ポイントの取り込み範囲)、[Q-005](261127-service-commonization-discussion.md#q-005-診断トレースの出力方式と所有権) | [LADR-010](261127-service-commonization-lightadr.md#ladr-010-linux-の-sighup-事前ブロックの公開-api) |
| [Q-011](261127-service-commonization-discussion.md#q-011-service-sample-本体の改修と資産移行の範囲) | service-sample 本体の改修と資産移行の範囲 | 解決済み | [Q-013](261127-service-commonization-discussion.md#q-013-検証範囲と-windows-確認の扱い)、[Q-018](261127-service-commonization-discussion.md#q-018-改修後の-service-sample-における-windows-権限昇格方式)、[Q-019](261127-service-commonization-discussion.md#q-019-移設後の旧ドキュメント-ファイルの扱い) | [LADR-011](261127-service-commonization-lightadr.md#ladr-011-service-sample-本体の改修と仕様文書の移設範囲) |
| [Q-012](261127-service-commonization-discussion.md#q-012-並行検討中の-account-session-案件との調整) | 並行検討中の account-session 案件との調整 | 解決済み | [Q-004](261127-service-commonization-discussion.md#q-004-linux-での-libsystemd-への依存方式) | [LADR-012](261127-service-commonization-lightadr.md#ladr-012-並行検討中の-account-session-案件との独立性) |
| [Q-013](261127-service-commonization-discussion.md#q-013-検証範囲と-windows-確認の扱い) | 検証範囲と Windows 確認の扱い | 解決済み | [Q-011](261127-service-commonization-discussion.md#q-011-service-sample-本体の改修と資産移行の範囲) | [LADR-013](261127-service-commonization-lightadr.md#ladr-013-検証範囲と-windows-確認の扱い) |
| [Q-014](261127-service-commonization-discussion.md#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存) | dlopen 時のビルド時 libsystemd ヘッダー依存 | 未解決 | [Q-004](261127-service-commonization-discussion.md#q-004-linux-での-libsystemd-への依存方式)、[Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合)、[Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) | 未反映 (Q-020 解決後に LADR-005 へ反映) |
| [Q-015](261127-service-commonization-discussion.md#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限) | install オプションでの watchdog 無効化とスレッド終了待ち上限 | 解決済み | [Q-009](261127-service-commonization-discussion.md#q-009-サービス登録パラメーターのカスタマイズ範囲)、[Q-016](261127-service-commonization-discussion.md#q-016-install-時の-startlimit-と再起動遅延の整合性検査) | [LADR-009](261127-service-commonization-lightadr.md#ladr-009-サービス登録パラメーターのカスタマイズ方式) |
| [Q-016](261127-service-commonization-discussion.md#q-016-install-時の-startlimit-と再起動遅延の整合性検査) | install 時の StartLimit と再起動遅延の整合性検査 | 解決済み | [Q-009](261127-service-commonization-discussion.md#q-009-サービス登録パラメーターのカスタマイズ範囲)、[Q-015](261127-service-commonization-discussion.md#q-015-install-オプションでの-watchdog-無効化とスレッド終了待ち上限) | [LADR-009](261127-service-commonization-lightadr.md#ladr-009-サービス登録パラメーターのカスタマイズ方式) |
| [Q-017](261127-service-commonization-discussion.md#q-017-公開-api-での-cplat_error-出力引数の要否) | 公開 API での cplat_error 出力引数の要否 | 解決済み | [Q-005](261127-service-commonization-discussion.md#q-005-診断トレースの出力方式と所有権)、[Q-006](261127-service-commonization-discussion.md#q-006-サービス操作-api-の戻り値規約) | [LADR-007](261127-service-commonization-lightadr.md#ladr-007-サービス操作-api-と停止待機-api-の戻り値規約) |
| [Q-018](261127-service-commonization-discussion.md#q-018-改修後の-service-sample-における-windows-権限昇格方式) | 改修後の service-sample における Windows 権限昇格方式 | 解決済み | [Q-008](261127-service-commonization-discussion.md#q-008-登録と解除における自動権限昇格の責務)、[Q-011](261127-service-commonization-discussion.md#q-011-service-sample-本体の改修と資産移行の範囲) | [LADR-011](261127-service-commonization-lightadr.md#ladr-011-service-sample-本体の改修と仕様文書の移設範囲) |
| [Q-019](261127-service-commonization-discussion.md#q-019-移設後の旧ドキュメント-ファイルの扱い) | 移設後の旧ドキュメント ファイルの扱い | 解決済み | [Q-011](261127-service-commonization-discussion.md#q-011-service-sample-本体の改修と資産移行の範囲) | [LADR-011](261127-service-commonization-lightadr.md#ladr-011-service-sample-本体の改修と仕様文書の移設範囲) |
| [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) | libsystemd モックの配置先と dlopen 呼び出し経路の整合 | 未回答 | [Q-004](261127-service-commonization-discussion.md#q-004-linux-での-libsystemd-への依存方式)、[Q-014](261127-service-commonization-discussion.md#q-014-dlopen-時のビルド時-libsystemd-ヘッダー依存)、[Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) | 未反映 |
| [Q-021](261127-service-commonization-discussion.md#q-021-testfw-変更時のテスト検証範囲) | testfw 変更時のテスト検証範囲 | 未回答 | [Q-013](261127-service-commonization-discussion.md#q-013-検証範囲と-windows-確認の扱い)、[Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) | 未反映 |

## 対象成果物と合意版

最終決定事項 (最終仕様) の設計書: `261127-service-commonization-design.md`

検討用 Markdown の合意版 (第 1 ゲート): 未確定 (残懸念なし確認時に確定)

最終仕様 (`design.md`) の合意版 (第 2 ゲート): 未承認

更新予定の成果物: `app/cplat` (`docs/functional-spec/service.md`、`prod/include/cplat/service/service.h`、`prod/include/cplat.h`、`prod/include_internal/cplat/service/`、`prod/libsrc/cplat/service/`、`test/src/libcplatTest/exportTest/exportTest.cc`、`test/include/mock_cplat.h`、`test/libsrc/mock_cplat/`、`test/src/libcplatTest/service/`、`docs/api-cheatsheet.md`、`docs/link-policy.md`、`docs/service-events.md`、`docs/systemd-oomkiller-advisory.md`、`docs/README.md`)、`app/service-sample` (`prod/src/cmd/service-sample/`、`prod/src/cmd/makepart.mk`、`test/`、`docs/`、`README.md`)、および Q-020 の決定に応じて `framework/testfw` (`include/mock_systemd.h`、`include_override/systemd/`、`libsrc/mock_systemd/`)

受入条件: 未確定 (版 3 の回答後に確定)

## 経緯

案件の進行上の出来事を発生順に記録します。

Table: 案件進行の経緯

| 日時 (JST) | 出来事 | 内容・参照先 |
| --- | --- | --- |
| 2026-11-27 08:06 | 案件の開始 | 全体管理文書 (`261127-service-commonization-overview.md`)、検討用 Markdown (`261127-service-commonization-discussion.md`、版 1、Q-001 〜 Q-013)、簡易 ADR (`261127-service-commonization-lightadr.md`、LADR-001) を作成 |
| 2026-11-27 19:30 | 版 1 の回答反映と版 2 の起票 | Q-001 〜 Q-013 の回答 (いずれも A) を検証・解決済みとし、`LADR-002` 〜 `LADR-013` と `BL-001` (`261127-service-commonization-backlog.md`) を記録。残る詳細設計論点として Q-014 〜 Q-019 を追記し、検討用 Markdown を版 2 へ更新 |
| 2026-11-27 23:13 | 版 2 の回答反映と版 3 の起票 | Q-015 〜 Q-019 (いずれも A) を検証・解決済みとし `LADR-007`・`LADR-009`・`LADR-011` を更新。Q-014 は補足 (環境確認と `testfw` モック追加) に伴う整合確認のため未解決とし、Q-020・Q-021 を追記して検討用 Markdown を版 3 へ更新 |

## 反映と検証の記録

未着手。作業後の対応箇所、検証結果、未完了項目を記録します。
