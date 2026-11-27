<!--
生成元: agree-before-implement スキル
文書種別: 検討用 Markdown (discussion)
最終更新エージェント: claude code / セッション ID: session_018UJwBxipLvskQMgge7hiBW
下流文書: `261025-process-priority-design.md` (最終仕様)、`261025-process-priority-backlog.md` (残課題一覧)。未作成の場合や、削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。ユーザーが編集した最新版を読み直し、作業 WBS の `← **CURRENT**` の項目から再開します。最終更新エージェントのセッション情報が参照可能であれば再開時のコンテキストとして利用し、別エージェントからの引き継ぎや担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。`← **CURRENT**` の項目は処理が中止された項目である場合があり、どこまで進んでいたかは文書だけでは分かりません。対象成果物の内容を確認して実施済みの範囲を特定してから、作業を再開してください。`← **CURRENT**` がなく、作業 WBS がすべて完了 (✅) であれば、この案件の合意フローは完了しています。
-->

# cplat のプロセス優先度変更 API の検討

ファイル名: `prompt/current/process-priority-discussion.md`

## 検討の識別

検討 ID: process-priority-20261025

版: 4

状態: 合意済み

## 作業 WBS と進捗管理

この検討用 Markdown が管理する作業 WBS です。情報収集と質問の実施、残懸念がないことの確認 (第 1 ゲート)、最終仕様 `prompt/current/process-priority-design.md` の作成、最終仕様の承認の受付 (第 2 ゲート)、承認された最終仕様に従った作業の実施、および成果物の承認の受付 (第 3 ゲート) に至る手順を記載します。  
第 4 節の対象ファイルは現時点の候補であり、Q&A の回答に応じて見直します。見送りの決定や範囲外の気付きは、生じた時点で `prompt/current/process-priority-backlog.md` に追記します。

凡例: ⏸️ 未着手、🔄 実施中、🚫 ブロック中、⚠️ 要注意、✅ 完了。現在取り組んでいる項目の末尾に `← **CURRENT**` を付けます。

- ✅ 1. 検討用 Markdown のための情報収集と質問の実施
    - ✅ 1.1. 前提条件と要求事項の調査 (cplat の結果コード規範、プラットフォーム専用 API の規範、命名規約、runtime カテゴリの既存 API、OS の公式文書)
    - ✅ 1.2. 課題・論点の抽出と確認項目 ([Q-001](#q-001-対象プロセスの範囲) 〜 [Q-013](#q-013-windows-実装の検証方法)) の作成
    - ✅ 1.3. Q&A ターンごとのユーザー回答の受付と記録 ([Q-001](#q-001-対象プロセスの範囲) 〜 [Q-013](#q-013-windows-実装の検証方法)、2026-10-25 19:54:50 JST 保存分)
    - ✅ 1.4. 回答に基づく前提・制約の検証と WBS の見直し (第 2 版)
    - ✅ 1.5. 追加質問 ([Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) 〜 [Q-018](#q-018-対象が存在しない場合の結果コード)) の回答の受付と記録 (2026-10-25 20:55:50 JST 保存分)
    - ✅ 1.6. 追加回答に基づく前提・制約の検証と WBS の見直し (第 3 版)
    - ✅ 1.7. 追加質問 ([Q-019](#q-019-os-が機能に対応しない場合の結果コード)、[Q-020](#q-020-バックグラウンド処理モードの二重開始と未開始での終了)) の回答の受付と記録 (2026-10-26 07:51:10 JST 保存分)
    - ✅ 1.8. 追加回答に基づく横断的な再検証と WBS の見直し (第 4 版)
- ✅ 2. 前提合意と残懸念の解消 (第 1 ゲート: 最終仕様作成への着手)
    - ✅ 2.1. 全論点に対する未解決事項および質問の解消確認
    - ✅ 2.2. 方針・前提条件の合意確定 (懸念がないことをもってフェーズ 2 へ着手。合意版は第 4 版)
- ✅ 3. 最終決定事項 (最終仕様) の作成と承認の受付
    - ✅ 3.1. `prompt/current/process-priority-design.md` の作成
    - ✅ 3.2. 最終仕様における受入条件と整合性の確認
    - ✅ 3.3. 対話セッションにおける最終仕様の承認の受付 (第 2 ゲート: 作業着手の合意。2026-10-26 承認)
- ✅ 4. 承認された最終仕様に従った作業の実施
    - ✅ 4.1. 公開ヘッダーの追加 (`app/cplat/prod/include/cplat/runtime/process_priority_linux.h`、`app/cplat/prod/include/cplat/runtime/process_priority_windows.h`)
    - ✅ 4.2. Linux 実装の追加 (`app/cplat/prod/libsrc/cplat/runtime/process_priority_linux.c`)
    - ✅ 4.3. Windows 実装の追加 (`app/cplat/prod/libsrc/cplat/runtime/process_priority_windows.c`) (CI の build-and-test-windows で確認。「反映と検証の記録」を参照)
    - ✅ 4.4. `cplat.h` の除外ヘッダー一覧への追加 (`exports.map` はワイルドカードで公開されるため変更不要。「反映と検証の記録」を参照)
    - ✅ 4.4.1. 共通写像の更新 (`ESRCH` の追加は [Q-018](#q-018-対象が存在しない場合の結果コード)、非対応系の追加は [Q-019](#q-019-os-が機能に対応しない場合の結果コード) で確定。対象: `app/cplat/prod/libsrc/cplat/base/result.c`、`app/cplat/prod/libsrc/cplat/base/error.c`、`app/cplat/prod/include/cplat/base/result.h`、`app/cplat/prod/include/cplat/base/error.h`、`resultTest.cc` と `errorTest.cc` の写像テスト)
    - ✅ 4.5. testfw の mock の追加 (Linux: `include/mock_sched.h`、`include/sys/mock_resource.h`、`include_override/sched.h`、`include_override/sys/resource.h`、`mock_unistd.h` の `syscall`。Windows: `mock_windows.h` の 10 関数)
    - ✅ 4.6. `mock_cplat` への公開関数の mock 追加 (`create-mock-cplat-mock` スキル)
    - ✅ 4.7. 単体テストの追加 (`app/cplat/test/src/libcplatTest/runtime/processPriorityTest/`) と `exportTest.cc` の `CPLAT_EXPORT_TABLE_PLATFORM` の更新
    - ✅ 4.8. 機能仕様 `app/cplat/docs/functional-spec/runtime.md` への要件 (`CPLAT-RUNTIME-FUNC-014` <!-- cplat-req: uuid=bd2d6d79-1efb-4bf0-a62f-937b6191b8a4 --> 〜 `016`) と節の追加
    - ✅ 4.9. `app/cplat/docs/api-cheatsheet.md` への追記
    - ✅ 4.10. 正本ドキュメントおよびコード コメント (see: URL を含む) への永続的知見の反映
    - ✅ 4.11. 局所ビルド・テスト、`.warn` の確認、Markdown の `text_style_jp.py` による確認、`make doxy` の警告確認
    - ✅ 4.12. 受入条件への適合確認 (全完確認)
    - ✅ 4.13. 残課題一覧 (`process-priority-backlog.md`) の再構築と整理
- ✅ 5. 成果物の承認の受付 (第 3 ゲート)
    - ✅ 5.1. 成果物・検証結果・残課題の提示
    - ✅ 5.2. 対話セッションにおける成果物の承認の受付 (「承認」または「完了」の受領によるスキル完了。2026-10-27 完了)

## 対象と参照資料

対象成果物: `app/cplat` に追加するプロセス優先度変更 API (公開ヘッダー、Linux と Windows の実装、mock、テスト、機能仕様、API チート シート)

今回の対象範囲: Linux と Windows それぞれに特化した優先度・スケジューリング設定 API。プラットフォーム共通の抽象化 API は今回の対象外です (ユーザー指示: 「抽象化は今後の課題とする」)。

書き込み可能な検討用 Markdown: この文書 (`prompt/current/process-priority-discussion.md`)

参照資料と版:

- ワークスペース ルート: HEAD `d38d86c86e6fb312d46ec6916983ab841967dfa6` (未コミット差分なし)
- `app/cplat`: HEAD `ed5b1b4ca835b31a47415731e3b5b6f8bf2789a8` (未コミット差分なし)
- [cplat コーディング規範](../../coding-guideline.md) の「エラー処理と戻り値規約」「ラッパーの設計方針」「API 命名規約」
- [プラットフォーム抽象化ガイドライン](../../platform-abstraction-guideline.md) の「単一プラットフォーム専用 API はヘッダー全体で限定する」
- [result.c](../../../prod/libsrc/cplat/base/result.c) の OS エラーから結果コードへの写像
- [process.h](../../../prod/include/cplat/runtime/process.h)、[memory_lock.h](../../../prod/include/cplat/runtime/memory_lock.h)、[elevated_process.h](../../../prod/include/cplat/runtime/elevated_process.h)
- [実行時支援 (runtime) 機能仕様](../../functional-spec/runtime.md)
- Linux: [sched(7)](https://man7.org/linux/man-pages/man7/sched.7.html)、[setpriority(2)](https://man7.org/linux/man-pages/man2/setpriority.2.html)、[sched_setattr(2)](https://man7.org/linux/man-pages/man2/sched_setattr.2.html)、[ioprio_set(2)](https://man7.org/linux/man-pages/man2/ioprio_set.2.html)
- Windows: [SetPriorityClass](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass)、[Scheduling Priorities](https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities)、[SetProcessInformation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation)、[SetProcessPriorityBoost](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocesspriorityboost)、[SetThreadPriority](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadpriority)
- 開発環境: WSL2 (Linux 6.6.87.2)、glibc 2.28、一般ユーザー (UID 1000、`ulimit -e` と `ulimit -r` はともに 0)

## 現行の合意内容

ユーザーが明示した内容と、解決済み Q の決定を記載します。未確定の論点は質問一覧を参照してください。

- cplat に、プラットフォームごとのプロセス優先度変更ユーティリティ関数を設けます (依頼文)。
- Linux と Windows で制御可能なプロセス優先度とスケジューリング ポリシーを制御する、プラットフォーム特化 API とします。抽象化は今後の課題とします (依頼文)。
- 対象は、自プロセスと、ID で指定した任意のプロセスです ([Q-001](#q-001-対象プロセスの範囲))。Linux の PID 再利用による誤指定の可能性は、API の制約として文書化します。
- Linux の各関数は ID をスレッド ID として扱い、PID を渡すとメイン スレッドだけを変更します ([Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド))。
- Windows のスレッド優先度はスレッド ID で対象を指定します ([Q-015](#q-015-windows-のスレッド優先度の対象))。
- Windows のバックグラウンド処理モードは、対象の引数を持たない自プロセス専用の関数とします ([Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定))。
- ID 0 は自分 (Linux は呼び出しスレッド、Windows は自プロセスまたは呼び出しスレッド) を意味します ([Q-017](#q-017-自プロセスと呼び出しスレッドの指定方法))。
- 対象が存在しない場合は、両 OS とも `CPLAT_ERR_NOT_FOUND` を返します。`ESRCH` は共通写像に追加し、Windows は本 API 内で変換します ([Q-018](#q-018-対象が存在しない場合の結果コード))。
- OS が機能に対応しない場合のエラー (`ENOSYS`・`ENOTSUP`・`EOPNOTSUPP`・`ERROR_NOT_SUPPORTED`・`ERROR_CALL_NOT_IMPLEMENTED`) は、共通写像で `CPLAT_ERR_UNSUPPORTED` へ写像し、要因判定との食い違いを解消します。既存 API の戻り値も同じ変更で変わります ([Q-019](#q-019-os-が機能に対応しない場合の結果コード))。
- Windows のバックグラウンド処理モードの二重開始と、モード外での終了は `CPLAT_SKIPPED` を返します ([Q-020](#q-020-バックグラウンド処理モードの二重開始と未開始での終了))。
- Linux で自分を対象にする場合は、呼び出しスレッドだけを変更し、スレッド生成前に呼ぶとプロセス全体に及ぶことを文書化します ([Q-002](#q-002-linux-で変更が及ぶスレッドの範囲))。
- Linux の対象軸は、スケジューリング ポリシーと静的優先度、デッド ライン スケジューリング、nice 値、fork 時の既定値への復帰フラグ、I/O 優先度、CPU 使用率クランプです ([Q-003](#q-003-linux-で扱う優先度の軸))。
- Windows の対象軸は、優先度クラス、バックグラウンド処理モード、メモリ優先度、電力スロットリング (EcoQoS)、優先度ブーストの無効化、スレッド優先度です ([Q-004](#q-004-windows-で扱う優先度の軸))。
- 権限不足は戻り値 `CPLAT_ERR_PERMISSION_DENIED` で通知し、OS の詳細を `detail_out` 出力引数とスレッド ローカルの直前値で返します ([Q-005](#q-005-権限不足の通知方式))。事前確認 API は設けません ([Q-006](#q-006-権限の事前確認-api-の要否))。
- Windows で `REALTIME` が `HIGH` へ格下げされた場合は、元のクラスへ戻して `CPLAT_ERR_PERMISSION_DENIED` を返します ([Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い))。
- 取得操作が公開されている軸には、設定と対になる取得 API を設けます ([Q-008](#q-008-現在値の取得-api-の要否))。
- API は軸ごとの個別関数とし、OS が 1 回の操作で同時に設定する値は 1 つの関数の引数にまとめます ([Q-009](#q-009-api-の粒度))。
- 選択値は cplat 独自の列挙型で受け取ります ([Q-010](#q-010-設定値の型))。
- 公開ヘッダーは `cplat/runtime/process_priority_linux.h` と `cplat/runtime/process_priority_windows.h` の 2 つです ([Q-011](#q-011-公開ヘッダーの配置))。
- 関数名は `cplat_process_priority_<linux|windows>_<動詞>_<属性>` とします ([Q-012](#q-012-関数名の構成))。
- Windows のビルドとテストは、ユーザーが手元の Windows 環境または CI で実行した結果を受入確認に使います ([Q-013](#q-013-windows-実装の検証方法))。

既存の規範と実装から確認できた事実は次のとおりです。

- `result.h` には `CPLAT_ERR_PERMISSION_DENIED` (-4) がすでに存在し、`result.c` は `EPERM`・`EACCES`・`ERROR_ACCESS_DENIED`・`ERROR_PRIVILEGE_NOT_HELD` をこのコードへ写像しています。権限不足の通知に新しい結果コードは不要です。
- 片側専用の公開面は、ヘッダー全体を `PLATFORM_*` で囲う規則があります (先例: `trace/syslog.h` は Linux 専用、`trace/etw.h` は Windows 専用)。
- 公開関数の追加時は、`exportTest.cc`、`mock_cplat`、`api-cheatsheet.md`、機能仕様を同じ変更で更新する規則があります (`app/cplat/AGENTS.md`)。
- 新しい OS API を使う場合は、testfw に mock を追加して経路を検証する方針があります (ユーザーの過去の指示)。
- glibc 2.28 には `sched_setattr()` と `ioprio_set()` のラッパー関数がなく、`syscall()` で直接呼び出す必要があります。

## 観点別の確認

| 観点 | 状態 | 根拠・関連 Q |
| --- | --- | --- |
| 目的・利用者 | 確認済み | 対象プロセスは [Q-001](#q-001-対象プロセスの範囲) で確定。他プロセスのスレッドの扱いは [Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド)、[Q-015](#q-015-windows-のスレッド優先度の対象) |
| 範囲・境界 | 確認済み | 扱う軸は [Q-003](#q-003-linux-で扱う優先度の軸)、[Q-004](#q-004-windows-で扱う優先度の軸)。取得 API は [Q-008](#q-008-現在値の取得-api-の要否)。抽象化は依頼文で対象外。共通写像の変更範囲は [Q-018](#q-018-対象が存在しない場合の結果コード) |
| 入力・根拠 | 確認済み | 「対象と参照資料」の OS 公式文書と cplat 規範。Windows の格下げ挙動は [Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い) で実測による確認方式に決定 |
| 整合性・依存関係 | 確認済み | 結果コード規範との整合は [Q-005](#q-005-権限不足の通知方式)、[Q-006](#q-006-権限の事前確認-api-の要否) で確定。対象不在の写像は [Q-018](#q-018-対象が存在しない場合の結果コード)、非対応の写像は [Q-019](#q-019-os-が機能に対応しない場合の結果コード) で確定。命名と配置は [Q-010](#q-010-設定値の型)、[Q-011](#q-011-公開ヘッダーの配置)、[Q-012](#q-012-関数名の構成) で確定 |
| 実現性・例外 | 確認済み | 自プロセス専用の操作は [Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定)、自分の指定方法は [Q-017](#q-017-自プロセスと呼び出しスレッドの指定方法)、対象不在は [Q-018](#q-018-対象が存在しない場合の結果コード)、モードの二重開始は [Q-020](#q-020-バックグラウンド処理モードの二重開始と未開始での終了) で確定。PID 再利用は API の制約として文書化 ([Q-001](#q-001-対象プロセスの範囲) の Assessment) |
| 受入条件・検証 | 確認済み | [Q-013](#q-013-windows-実装の検証方法) |

## 質問一覧

| Q-ID | 論点 | 状態 | 依存・関連 |
| --- | --- | --- | --- |
| [Q-001](#q-001-対象プロセスの範囲) | 対象プロセスの範囲 | 回答済み (追加確認あり) | [Q-002](#q-002-linux-で変更が及ぶスレッドの範囲)、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) 〜 [Q-018](#q-018-対象が存在しない場合の結果コード) |
| [Q-002](#q-002-linux-で変更が及ぶスレッドの範囲) | Linux で変更が及ぶスレッドの範囲 | 解決済み | [Q-001](#q-001-対象プロセスの範囲)、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) |
| [Q-003](#q-003-linux-で扱う優先度の軸) | Linux で扱う優先度の軸 | 解決済み | [Q-009](#q-009-api-の粒度) |
| [Q-004](#q-004-windows-で扱う優先度の軸) | Windows で扱う優先度の軸 | 解決済み | [Q-015](#q-015-windows-のスレッド優先度の対象)、[Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定) |
| [Q-005](#q-005-権限不足の通知方式) | 権限不足の通知方式 | 解決済み | [Q-006](#q-006-権限の事前確認-api-の要否)、[Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い) |
| [Q-006](#q-006-権限の事前確認-api-の要否) | 権限の事前確認 API の要否 | 解決済み | [Q-005](#q-005-権限不足の通知方式) |
| [Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い) | Windows のリアルタイム指定の格下げの扱い | 解決済み | [Q-004](#q-004-windows-で扱う優先度の軸)、[Q-005](#q-005-権限不足の通知方式) |
| [Q-008](#q-008-現在値の取得-api-の要否) | 現在値の取得 API の要否 | 解決済み | [Q-009](#q-009-api-の粒度) |
| [Q-009](#q-009-api-の粒度) | API の粒度 | 解決済み | [Q-003](#q-003-linux-で扱う優先度の軸)、[Q-004](#q-004-windows-で扱う優先度の軸) |
| [Q-010](#q-010-設定値の型) | 設定値の型 | 解決済み | [Q-009](#q-009-api-の粒度) |
| [Q-011](#q-011-公開ヘッダーの配置) | 公開ヘッダーの配置 | 解決済み | [Q-012](#q-012-関数名の構成) |
| [Q-012](#q-012-関数名の構成) | 関数名の構成 | 解決済み | [Q-011](#q-011-公開ヘッダーの配置) |
| [Q-013](#q-013-windows-実装の検証方法) | Windows 実装の検証方法 | 解決済み | なし |
| [Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) | Linux で ID を指定した場合に変更するスレッド | 解決済み | [Q-001](#q-001-対象プロセスの範囲)、[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲) |
| [Q-015](#q-015-windows-のスレッド優先度の対象) | Windows のスレッド優先度の対象 | 解決済み | [Q-004](#q-004-windows-で扱う優先度の軸)、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) |
| [Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定) | Windows のバックグラウンド処理モードの対象指定 | 解決済み | [Q-001](#q-001-対象プロセスの範囲)、[Q-004](#q-004-windows-で扱う優先度の軸) |
| [Q-017](#q-017-自プロセスと呼び出しスレッドの指定方法) | 自プロセスと呼び出しスレッドの指定方法 | 解決済み | [Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド)、[Q-015](#q-015-windows-のスレッド優先度の対象) |
| [Q-018](#q-018-対象が存在しない場合の結果コード) | 対象が存在しない場合の結果コード | 解決済み | [Q-001](#q-001-対象プロセスの範囲)、[Q-005](#q-005-権限不足の通知方式)、[Q-019](#q-019-os-が機能に対応しない場合の結果コード) |
| [Q-019](#q-019-os-が機能に対応しない場合の結果コード) | OS が機能に対応しない場合の結果コード | 解決済み | [Q-003](#q-003-linux-で扱う優先度の軸)、[Q-018](#q-018-対象が存在しない場合の結果コード) |
| [Q-020](#q-020-バックグラウンド処理モードの二重開始と未開始での終了) | バックグラウンド処理モードの二重開始と未開始での終了 | 解決済み | [Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定) |

## 明確化のための確認項目

### Q-001 対象プロセスの範囲

**Status**

回答済み (追加確認あり)

**Question**

優先度を変更できる対象プロセスを、どの範囲にしますか。

**Context**

依頼文は「プロセス優先度変更ユーティリティ」であり、変更対象が自プロセスだけか、他プロセスも含むかが明示されていません。

- 自プロセスだけであれば、Windows は `GetCurrentProcess()` の擬似ハンドルで常に必要なアクセス権を持ちます。権限不足は、優先度を上げる操作の特権不足だけになります。
- 他プロセスを対象にすると、Windows は `OpenProcess(PROCESS_SET_INFORMATION)` の段階でもアクセス拒否が起こり、Linux は他ユーザーのプロセスに対して `EPERM` になります。権限不足の原因が 2 種類 (対象を開けない、値を設定できない) になります。
- Windows のバックグラウンド処理モード ([Q-004](#q-004-windows-で扱う優先度の軸) の選択肢 B) は、OS の仕様で自プロセスにしか適用できません。
- 既存の [メモリ ロック API](../../memory-lock.md) は自プロセスだけを対象とし、他プロセスを機能仕様の適用範囲外としています。
- `cplat_process_start()` が返す子プロセス ハンドル (`cplat_process *`) を対象にする案は、子プロセスの起動直後に優先度を変える用途を満たします。

```c
/* 案 A: 自プロセスのみ (引数に対象を持たない) */
int cplat_process_priority_linux_set_nice(int nice_value);

/* 案 B: 自プロセスと、cplat_process_start() で起動した子プロセス */
int cplat_process_priority_linux_set_nice(cplat_process *target /* NULL は自プロセス */, int nice_value);

/* 案 C: 任意の PID */
int cplat_process_priority_linux_set_nice(uint32_t pid /* 0 は自プロセス */, int nice_value);
```

**Answer**

選択方式: 単一選択

- [ ] A: (推奨) 自プロセスのみを対象とする。
- [ ] B: 自プロセスと、`cplat_process_start()` で起動した子プロセスを対象とする。
- [x] C: 自プロセスと、PID で指定した任意のプロセスを対象とする。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 依頼文と既存のメモリ ロック API の範囲が自プロセスであり、権限不足の原因を「優先度を上げる特権の不足」の 1 種類に保てます。B と C は、対象を開く段階の権限不足と、Linux の PID 再利用による誤操作の考慮が加わります。  
制約: 子プロセスの優先度を変えたい場合は、子プロセス側で本 API を呼ぶ必要があります。

**Related**

[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲)、[Q-004](#q-004-windows-で扱う優先度の軸)、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド)、[Q-015](#q-015-windows-のスレッド優先度の対象)、[Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定)、[Q-017](#q-017-自プロセスと呼び出しスレッドの指定方法)、[Q-018](#q-018-対象が存在しない場合の結果コード)

**Assessment**

推奨 A と異なる C (任意の PID) が選択されました。回答は明確であり、方針として確定します。

C の採用により、次の論点が新たに生じたため、追加質問で確認します。

- Linux では他プロセスの PID を指定しても、OS はそのプロセスのメイン スレッドだけを変更します。変更するスレッドの範囲を [Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) で確認します。
- Windows のスレッド優先度の対象指定を [Q-015](#q-015-windows-のスレッド優先度の対象) で確認します。
- Windows のバックグラウンド処理モードは OS の仕様で自プロセス専用のため、その関数の対象指定を [Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定) で確認します。
- 自プロセスと呼び出しスレッドの指定方法を [Q-017](#q-017-自プロセスと呼び出しスレッドの指定方法) で確認します。
- 指定したプロセスが存在しない場合の結果コードを [Q-018](#q-018-対象が存在しない場合の結果コード) で確認します。

他プロセスを開けない場合 (Windows の `OpenProcess` の `ERROR_ACCESS_DENIED`、Linux の他ユーザーのプロセスに対する `EPERM`) も、[Q-005](#q-005-権限不足の通知方式) の回答に従い `CPLAT_ERR_PERMISSION_DENIED` になります。  
Linux の PID は終了後に再利用されるため、指定した PID が意図と別のプロセスを指す可能性は API の制約として文書に明記します。

### Q-002 Linux で変更が及ぶスレッドの範囲

**Status**

解決済み

**Question**

Linux で、nice 値・スケジューリング ポリシー・I/O 優先度の変更を、どのスレッドへ適用しますか。

**Context**

Linux では、これらの値は POSIX の想定と異なり、プロセスではなくスレッドごとの属性です ([setpriority (2) の BUGS](https://man7.org/linux/man-pages/man2/setpriority.2.html)、[sched(7)](https://man7.org/linux/man-pages/man7/sched.7.html))。  
`setpriority(PRIO_PROCESS, 0, n)` や `sched_setscheduler(0, ...)` は、呼び出したスレッドだけを変更します。  
新しく作るスレッドは、作成元スレッドの値を引き継ぎます。

一方、Windows の優先度クラスはプロセス全体の属性であり、全スレッドに及びます。

- 呼び出しスレッドだけを変更する案は、OS の操作 1 回で完結し、部分的な失敗が起きません。`main()` の冒頭などスレッドを作る前に呼べば、以降のスレッドが値を引き継ぐため、結果としてプロセス全体に及びます。
- 全スレッドを変更する案は、`/proc/self/task` を列挙して各スレッドへ適用します。列挙中に作られたスレッドを取りこぼす可能性があり、途中のスレッドで失敗した場合に、適用済みのスレッドを戻すかどうかの規則が別途必要です。
- TID を指定する案は、`cplat_process_get_tid()` が返す値を使って自プロセス内の任意のスレッドを変更できます。

```c
/* 案 A: 呼び出しスレッドのみ */
ret = cplat_process_priority_linux_set_nice(10);

/* 案 B: 自プロセスの全スレッド (内部で /proc/self/task を列挙) */
ret = cplat_process_priority_linux_set_nice(10);   /* 途中失敗時の扱いは別途決定 */

/* 案 C: 自プロセス内のスレッドを TID で指定 (0 は呼び出しスレッド) */
ret = cplat_process_priority_linux_set_nice(cplat_process_get_tid(), 10);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 呼び出しスレッドのみを変更し、スレッド生成前に呼ぶとプロセス全体に及ぶことを文書化する。
- [ ] B: 自プロセスの全スレッドを変更する。
- [ ] C: 自プロセス内のスレッドを TID で指定して変更する。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足: プロセス外から操作することはできないのか

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: OS の操作単位と一致し、部分失敗と取りこぼしが起きません。全スレッドへの一括適用は原子的に実現できないため、規則を追加しても完全な保証にはなりません。  
制約: スレッド生成後に呼ぶと、既存の他スレッドには及びません。この制約は API の文書に明記します。

**Related**

[Q-001](#q-001-対象プロセスの範囲)、[Q-004](#q-004-windows-で扱う優先度の軸) (Windows のスレッド優先度を扱う場合も同じ範囲を適用します)、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド)、[Q-015](#q-015-windows-のスレッド優先度の対象)

**Assessment**

A が選択され、自プロセスで ID を指定しない場合は呼び出しスレッドだけを変更する方針として確定します。

補足「プロセス外から操作することはできないのか」への回答です。  
Linux と Windows のいずれも、権限があれば他プロセスの優先度を外から変更できます。  
Linux は `sched_setscheduler`・`setpriority`・`ioprio_set` などに PID を渡し、同じユーザーのプロセスであるか `CAP_SYS_NICE` が必要です。  
Windows は `OpenProcess(PROCESS_SET_INFORMATION)` で対象を開いて操作します。  
[Q-001](#q-001-対象プロセスの範囲) の回答 C により、外からの操作も今回の範囲に含まれました。  
ただし Linux では、外から PID を指定しても変更されるのはメイン スレッドだけであるため、その扱いを [Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) で確認します。

### Q-003 Linux で扱う優先度の軸

**Status**

解決済み

**Question**

Linux で扱う優先度・スケジューリングの軸として、どれを含めますか。

**Context**

依頼文は「スケジューリング ポリシーなど複数の軸を網羅すること」としています。  
Linux でスレッドまたはプロセスの実行優先度に作用する仕組みを調べると、次の候補があります。

Table: Linux の実行優先度に作用する仕組みの候補

| 記号 | 軸 | OS の操作 | 値 | 必要な権限 (権限不足時の errno) |
| --- | --- | --- | --- | --- |
| A | スケジューリング ポリシーと静的優先度 | `sched_setscheduler` | `SCHED_OTHER` / `SCHED_BATCH` / `SCHED_IDLE` / `SCHED_FIFO` / `SCHED_RR`、リアルタイムは優先度 1 〜 99 | リアルタイムは `CAP_SYS_NICE` または `RLIMIT_RTPRIO` (`EPERM`) |
| B | デッド ライン スケジューリング | `sched_setattr` (`syscall` で呼び出し) | `SCHED_DEADLINE` の実行時間・期限・周期 (ナノ秒) | `CAP_SYS_NICE` (`EPERM`)。帯域不足は `EBUSY` |
| C | nice 値 | `setpriority` | -20 〜 19 | 値を下げる (優先度を上げる) 操作は `CAP_SYS_NICE` または `RLIMIT_NICE` (`EACCES`) |
| D | fork 時の既定値への復帰 | `SCHED_RESET_ON_FORK` フラグ | 有効 / 無効 | 無効化は `CAP_SYS_NICE` (`EPERM`) |
| E | I/O 優先度 | `ioprio_set` (`syscall` で呼び出し) | クラス `RT` / `BE` / `IDLE`、レベル 0 〜 7 | `RT` クラスは `CAP_SYS_ADMIN` (`EPERM`) |
| F | CPU 使用率クランプ | `sched_setattr` の `SCHED_FLAG_UTIL_CLAMP` (Linux 5.3 以降) | 最小・最大の使用率 0 〜 1024 | 上限を上げる操作は `CAP_SYS_NICE` (`EPERM`) |
| G | autogroup の nice 値 | `/proc/self/autogroup` への書き込み | -20 〜 19 | 値を下げる操作は `CAP_SYS_NICE` |
| H | cgroup の CPU / I/O 重み | cgroup v2 の `cpu.weight`、`io.weight` | 1 〜 10000 など | cgroup ファイルへの書き込み権限 |
| I | CPU アフィニティ | `sched_setaffinity` | CPU の集合 | 他プロセスの場合のみ制約 |

- A 〜 F は、スレッドまたはプロセスへシステム コールで設定する属性です。
- G は同じセッション (autogroup) に属する全プロセスに作用し、カーネル設定で autogroup が無効な環境ではファイルが存在しません。この開発環境 (WSL2) にも存在しません。
- H は cgroup の管理者 (systemd など) の領域であり、プロセス自身が変更する前提ではありません。
- I は実行する CPU の指定であり、優先度ではありません。
- C の nice 値は `SCHED_OTHER` と `SCHED_BATCH` にだけ作用し、autogroup が有効な環境では同じ autogroup 内での相対値になります。

**Answer**

選択方式: 複数選択

- [x] A: (推奨) スケジューリング ポリシーと静的優先度
- [x] B: (推奨) デッド ライン スケジューリング
- [x] C: (推奨) nice 値
- [x] D: (推奨) fork 時の既定値への復帰フラグ
- [x] E: (推奨) I/O 優先度
- [x] F: (推奨) CPU 使用率クランプ
- [ ] G: autogroup の nice 値
- [ ] H: cgroup の CPU / I/O 重み
- [ ] I: CPU アフィニティ
- [ ] J: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A、B、C、D、E、F  
理由: 依頼文の「網羅」を、プロセス自身がシステム コールで設定できる実行優先度の属性すべてと解釈した場合の範囲です。G はセッション単位、H は管理者の領域、I は優先度ではないため除外を推奨します。  
制約: B と F は、カーネルの版や構成によって `ENOSYS` や `EINVAL` になる場合があり、その場合は `CPLAT_ERR_UNSUPPORTED` で通知する設計を想定しています。

**Related**

[Q-009](#q-009-api-の粒度)、[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲)、[Q-019](#q-019-os-が機能に対応しない場合の結果コード)

**Assessment**

推奨どおり A 〜 F が選択されました。G (autogroup)、H (cgroup)、I (CPU アフィニティ) は範囲外として確定します。

### Q-004 Windows で扱う優先度の軸

**Status**

解決済み

**Question**

Windows で扱う優先度の軸として、どれを含めますか。

**Context**

依頼文は「Windows に関してはプロセス優先度だけが設定できると思う」としていますが、公式文書で確認すると、優先度クラス以外にも次の調整軸があります。

Table: Windows の実行優先度に作用する仕組みの候補

| 記号 | 軸 | OS の操作 | 値 | 権限・制約 |
| --- | --- | --- | --- | --- |
| A | 優先度クラス | `SetPriorityClass` | `IDLE` / `BELOW_NORMAL` / `NORMAL` / `ABOVE_NORMAL` / `HIGH` / `REALTIME` | `REALTIME` は特権 `SeIncreaseBasePriorityPrivilege` が必要 ([Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い)) |
| B | バックグラウンド処理モード | `SetPriorityClass` の `PROCESS_MODE_BACKGROUND_BEGIN` / `END` | 開始 / 終了 | 自プロセスのみ。CPU に加えて I/O とメモリの優先度も下げる。二重開始は `ERROR_PROCESS_MODE_ALREADY_BACKGROUND` |
| C | メモリ優先度 | `SetProcessInformation(ProcessMemoryPriority)` | 1 〜 5 | Windows 8 以降 |
| D | 電力スロットリング (EcoQoS) | `SetProcessInformation(ProcessPowerThrottling)` | 有効 / 無効 / OS 任せ | Windows 10 1709 以降。Windows 11 で効果が大きい |
| E | 優先度ブーストの無効化 | `SetProcessPriorityBoost` | 有効 / 無効 | なし |
| F | スレッド優先度 | `SetThreadPriority` | 優先度クラス内の相対値 7 段階 | `REALTIME` クラス内の一部の値は特権が必要 |
| G | I/O 優先度の直接指定 | `NtSetInformationProcess(ProcessIoPriority)` | 非公開 API | 公開文書がなく、互換性の保証がない |
| H | CPU アフィニティ / CPU セット | `SetProcessAffinityMask` など | CPU の集合 | 優先度ではない |

- 出典: [SetPriorityClass](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass)、[Scheduling Priorities](https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities)、[SetProcessInformation](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation)
- B は公式文書が「ファイル I/O などのバックグラウンド処理では、CPU 優先度の調整だけでは不十分」として推奨する方式です。Linux の I/O 優先度 (`IDLE` クラス) に近い効果を、公開 API で得られる唯一の手段です。
- F は Linux の [Q-002](#q-002-linux-で変更が及ぶスレッドの範囲) と同じ「どのスレッドに適用するか」の論点を持ちます。

**Answer**

選択方式: 複数選択

- [x] A: (推奨) 優先度クラス
- [x] B: (推奨) バックグラウンド処理モード
- [x] C: (推奨) メモリ優先度
- [x] D: (推奨) 電力スロットリング (EcoQoS)
- [x] E: (推奨) 優先度ブーストの無効化
- [x] F: (推奨) スレッド優先度
- [ ] G: I/O 優先度の直接指定 (非公開 API)
- [ ] H: CPU アフィニティ / CPU セット
- [ ] I: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A、B、C、D、E、F  
理由: Linux 側を網羅する方針と対称に、公開 API で設定できる実行優先度の属性をすべて含める範囲です。G は非公開 API のため、H は優先度ではないため除外を推奨します。  
制約: 依頼文の「Windows はプロセス優先度だけ」と範囲が異なるため、A だけに限定する選択も可能です。

**Related**

[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲)、[Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い)、[Q-009](#q-009-api-の粒度)、[Q-015](#q-015-windows-のスレッド優先度の対象)、[Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定)

**Assessment**

推奨どおり A 〜 F が選択されました。G (非公開 API による I/O 優先度) と H (CPU アフィニティ / CPU セット) は範囲外として確定します。  
F (スレッド優先度) の対象指定は [Q-015](#q-015-windows-のスレッド優先度の対象)、B (バックグラウンド処理モード) の対象指定は [Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定) で確認します。

### Q-005 権限不足の通知方式

**Status**

解決済み

**Question**

権限不足を呼び出し元へ通知する方式を、どれにしますか。

**Context**

依頼文は「権限が不足していることを呼び出し元がわかるようにすること」としています。  
cplat の規範では、戻り値は分類済みの結果コード、OS 由来の詳細は `cplat_error *detail_out` 出力引数とスレッド ローカルの直前値で伝えます ([cplat コーディング規範の「戻り値とエラー詳細の役割分担」](../../coding-guideline.md#戻り値とエラー詳細の役割分担))。  
`CPLAT_ERR_PERMISSION_DENIED` はすでに存在し、`EPERM`・`EACCES`・`ERROR_ACCESS_DENIED`・`ERROR_PRIVILEGE_NOT_HELD` がこのコードへ写像されます。

Linux では、権限不足の原因が複数あります。

- `CAP_SYS_NICE` などのケーパビリティがない (`EPERM`)
- `RLIMIT_RTPRIO` や `RLIMIT_NICE` の上限を超える (`EPERM` または `EACCES`)
- cgroup v1 の `cpu.rt_runtime_us` が 0 で、root でもリアルタイム ポリシーにできない (`EPERM`)

いずれも戻り値は `CPLAT_ERR_PERMISSION_DENIED` になります。  
`detail_out` を設けると、`EPERM` と `EACCES` の区別や、権限以外の失敗 (`EBUSY`、`ENOSYS` など) の生の値を呼び出し元が得られます。

```c
/* 案 A: 戻り値 + detail_out (NULL 可) + 直前値 */
cplat_error detail;
ret = cplat_process_priority_linux_set_policy(&policy, &detail);
if (ret == CPLAT_ERR_PERMISSION_DENIED)
{
    /* 権限不足。detail で EPERM / EACCES の区別も可能 */
}

/* 案 B: 戻り値のみ (直前値は cplat_error_get_last() で取得) */
ret = cplat_process_priority_linux_set_policy(&policy);
if (ret == CPLAT_ERR_PERMISSION_DENIED)
{
    /* 権限不足 */
}
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 戻り値 `CPLAT_ERR_PERMISSION_DENIED` に加え、`detail_out` 出力引数とスレッド ローカルの直前値で OS の詳細を返す。
- [ ] B: 戻り値 `CPLAT_ERR_PERMISSION_DENIED` とスレッド ローカルの直前値で返し、`detail_out` 出力引数は設けない。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: cplat 規範の 3 手段の役割分担にそのまま従い、権限不足以外の失敗 (デッド ラインの帯域不足、カーネル非対応など) の詳細も同じ手段で得られます。詳細が不要な呼び出し元は `NULL` を渡せます。  
制約: Linux の権限不足の原因 (ケーパビリティ、資源上限、cgroup) は、OS が同じ errno を返すため区別できません。

**Related**

[Q-006](#q-006-権限の事前確認-api-の要否)、[Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い)

**Assessment**

推奨 A が選択されました。各関数は `cplat_error *detail_out` (NULL 可) を持ち、失敗時に出力引数とスレッド ローカルの直前値へ OS の詳細を記録します。

### Q-006 権限の事前確認 API の要否

**Status**

解決済み

**Question**

変更を試みる前に、設定できる範囲を問い合わせる API を設けますか。

**Context**

[Q-005](#q-005-権限不足の通知方式) の通知は、変更を試みた結果として権限不足を知る方式です。  
これとは別に、試す前に「現在の権限で設定できる範囲」を問い合わせる API を設ける案があります。

- Linux では、`CAP_SYS_NICE` の有無 (`capget`)、`RLIMIT_RTPRIO` と `RLIMIT_NICE` (`getrlimit`) から、設定できるリアルタイム優先度の上限と nice 値の下限を計算できます。ただし cgroup のリアルタイム帯域による拒否は計算に含められません。
- Windows では、プロセス トークンに `SeIncreaseBasePriorityPrivilege` があるかを確認できます。
- 問い合わせ結果と実際の変更の間で権限が変わる可能性があり (確認と使用の時間差)、問い合わせ結果は目安にとどまります。
- 既存の `cplat_elevated_process_is_elevated()` は、管理者権限の有無だけを返す近い前例です。

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 事前確認 API は設けず、変更操作の結果コードで判別させる。
- [ ] B: 事前確認 API を設ける。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 依頼文の要件は変更操作の結果コードで満たせます。事前確認は cgroup による拒否を予測できず、結果が保証にならないため、変更操作の結果コードと二重の判定経路を持つことになります。  
制約: 権限に応じて設定値を自動で下げる処理を呼び出し側で組む場合は、変更操作を試して結果を見る形になります。

**Related**

[Q-005](#q-005-権限不足の通知方式)

**Assessment**

推奨 A が選択されました。事前確認 API は設けません。

### Q-007 Windows のリアルタイム指定の格下げの扱い

**Status**

解決済み

**Question**

Windows で `REALTIME` 優先度クラスを指定し、OS が黙って `HIGH` へ格下げした場合に、API をどう振る舞わせますか。

**Context**

Windows の `SetPriorityClass` は、呼び出し元が特権 `SeIncreaseBasePriorityPrivilege` を持たない状態で `REALTIME_PRIORITY_CLASS` を指定すると、失敗を返さず `HIGH_PRIORITY_CLASS` を設定して成功を返すことが知られています。  
この挙動は [SetPriorityClass の公式文書](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass) と [Scheduling Priorities](https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities) には記載されていません。この環境では Windows 実機で再現を確認できていません。

この挙動をそのまま通すと、権限不足でも `CPLAT_OK` が返り、依頼文の「権限が不足していることを呼び出し元がわかるようにする」を満たせません。

```c
/* 案 A: 設定後に GetPriorityClass で確認し、格下げなら元のクラスへ戻して権限不足を返す */
previous = GetPriorityClass(GetCurrentProcess());
SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
if (GetPriorityClass(GetCurrentProcess()) != REALTIME_PRIORITY_CLASS)
{
    SetPriorityClass(GetCurrentProcess(), previous);
    return CPLAT_ERR_PERMISSION_DENIED;
}

/* 案 B: 設定前にトークンの特権を確認し、なければ変更せず権限不足を返す */
if (!token_has_privilege(SE_INC_BASE_PRIORITY_NAME))
{
    return CPLAT_ERR_PERMISSION_DENIED;
}
SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);

/* 案 C: 設定後に確認し、HIGH のまま権限不足を返す */
/* 案 D: OS の挙動を通し CPLAT_OK を返す (呼び出し元が取得 API で確認する) */
```

- 案 A は文書化されていない OS 内部の判定に依存せず、結果だけを確認します。元のクラスへ戻す操作自体が失敗する可能性は極めて低いものの、その場合の扱いを決める必要があります。
- 案 B は変更前に判定できますが、OS が特権をどう判定しているか (トークンに存在するだけで足りるか、有効化が必要か) が公式文書で確認できず、判定が OS の実際と食い違うおそれがあります。
- 案 C は、呼び出し元が `HIGH` で動作を続けるか自分で戻すかを選べますが、失敗を返したのに状態が変わっている点が cplat の他の API と異なります。

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 設定後に確認し、格下げされていれば元のクラスへ戻して `CPLAT_ERR_PERMISSION_DENIED` を返す。
- [ ] B: 設定前に特権の有無を確認し、なければ変更せずに `CPLAT_ERR_PERMISSION_DENIED` を返す。
- [ ] C: 設定後に確認し、`HIGH` のまま `CPLAT_ERR_PERMISSION_DENIED` を返す。
- [ ] D: OS の挙動を通し、`CPLAT_OK` を返す。
- [ ] E: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 非公開の判定規則に依存せず、失敗時は状態を変えないという他の API と同じ契約を保てます。  
制約: 格下げ挙動の根拠 URL は公式文書で見つかっていないため、ソースへ残す `see:` は確認できた資料に限ります。Windows 実機での確認方法は [Q-013](#q-013-windows-実装の検証方法) で決めます。

**Related**

[Q-004](#q-004-windows-で扱う優先度の軸)、[Q-005](#q-005-権限不足の通知方式)、[Q-013](#q-013-windows-実装の検証方法)

**Assessment**

推奨 A が選択されました。`REALTIME` を指定した場合は設定後に `GetPriorityClass` で確認し、格下げされていれば元のクラスへ戻して `CPLAT_ERR_PERMISSION_DENIED` を返します。  
他プロセスを対象とする場合も同じ確認を行います (`OpenProcess` に `PROCESS_QUERY_LIMITED_INFORMATION` も要求します)。

### Q-008 現在値の取得 API の要否

**Status**

解決済み

**Question**

設定 API に対応する、現在値の取得 API を設けますか。

**Context**

依頼文は「優先度変更」であり、取得には触れていません。  
取得 API があると、変更前の値を保存して処理後に戻す用途や、テストで設定結果を確認する用途に使えます。  
OS はいずれの軸にも取得操作を用意しています (`sched_getscheduler`、`getpriority`、`ioprio_get`、`sched_getattr`、`GetPriorityClass`、`GetProcessInformation`、`GetProcessPriorityBoost`、`GetThreadPriority`)。  
ただし、Windows のバックグラウンド処理モードには、現在モード中かを取得する公開 API がありません。

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 取得操作が公開されている軸には、設定と対になる取得 API を設ける。
- [ ] B: 設定 API だけを設ける。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 一時的に優先度を上げて戻す用途は、変更前の値を得られないと実現できません。テストでの設定結果の確認にも使えます。  
制約: 公開 API 数が設定 API のおよそ 2 倍になり、`mock_cplat` と `exportTest.cc` の追加量も増えます。

**Related**

[Q-009](#q-009-api-の粒度)、[Q-020](#q-020-バックグラウンド処理モードの二重開始と未開始での終了)

**Assessment**

推奨 A が選択されました。取得操作が公開されている軸には、設定と同じ対象指定を持つ取得 API を設けます。Windows のバックグラウンド処理モードには取得 API を設けません。

### Q-009 API の粒度

**Status**

解決済み

**Question**

設定 API を、軸ごとの個別関数にしますか、複数の軸をまとめて指定する 1 つの関数にしますか。

**Context**

軸ごとに必要な権限と失敗の原因が異なります。  
複数の軸をまとめて適用する関数では、途中の軸で権限不足になった場合に、適用済みの軸を戻すかどうかの規則が必要です。OS には複数の軸を原子的に変更する操作がないため、戻す処理も失敗しうります。

```c
/* 案 A: 軸ごとの個別関数 */
ret = cplat_process_priority_linux_set_policy(CPLAT_..._SCHED_FIFO, 10, &detail);
ret = cplat_process_priority_linux_set_io(CPLAT_..._IO_CLASS_IDLE, 0, &detail);

/* 案 B: 構造体で複数の軸をまとめて指定 */
cplat_process_priority_linux_options options = {0};
options.fields = CPLAT_..._FIELD_POLICY | CPLAT_..._FIELD_IO;
options.policy = ...;
options.io_class = ...;
ret = cplat_process_priority_linux_apply(&options, &detail);  /* 途中失敗時の扱いが別途必要 */
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 軸ごとの個別関数にする。
- [ ] B: 複数の軸をまとめて指定する 1 つの関数にする。
- [ ] C: A に加えて、B の一括関数も設ける。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 1 回の呼び出しが 1 回の OS 操作に対応し、権限不足がどの軸で起きたかを戻り値だけで特定できます。部分適用の規則が不要です。  
制約: 複数の軸を変更する呼び出し元は、関数を順に呼び、失敗時の扱いを自分で決めます。なお、Linux のスケジューリング ポリシー・静的優先度・fork 時の復帰フラグのように、OS が 1 回の操作で同時に設定する値は、1 つの関数の引数にまとめます。

**Related**

[Q-003](#q-003-linux-で扱う優先度の軸)、[Q-004](#q-004-windows-で扱う優先度の軸)、[Q-008](#q-008-現在値の取得-api-の要否)、[Q-010](#q-010-設定値の型)

**Assessment**

推奨 A が選択されました。

### Q-010 設定値の型

**Status**

解決済み

**Question**

ポリシーや優先度クラスなどの選択値を、cplat 独自の列挙型で受け取りますか、OS の定数をそのまま受け取りますか。

**Context**

プラットフォーム特化 API であるため、`SCHED_FIFO` や `HIGH_PRIORITY_CLASS` などの OS 定数をそのまま受け取ることもできます。

- cplat 独自の列挙型にすると、引数の検証 (未知の値の拒否) が明確になり、将来の抽象化 API で共通の値へ写像しやすくなります。利用側が `<sched.h>` や `<windows.h>` をインクルードせずに済みます。
- OS 定数をそのまま受け取ると、薄い API になり、OS が将来追加する値もそのまま渡せます。一方、未知の値の検証は OS 任せになり、Linux の `SCHED_DEADLINE` のように別の操作が必要な値を誤って渡せます。

```c
/* 案 A: cplat 独自の列挙型 */
typedef enum cplat_process_priority_linux_policy
{
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER = 0,
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH = 1,
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_IDLE = 2,
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO = 3,
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_ROUND_ROBIN = 4
} cplat_process_priority_linux_policy;

/* 案 B: OS 定数をそのまま受け取る */
int cplat_process_priority_linux_set_policy(int sched_policy /* SCHED_FIFO など */, int static_priority, cplat_error *detail_out);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) cplat 独自の列挙型で受け取る。
- [ ] B: OS の定数をそのまま受け取る。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 引数の検証を API 側に内包でき、将来の抽象化で値の写像を一か所にまとめられます。利用側が OS のヘッダーに依存しません。  
制約: OS が新しい値を追加した場合は、cplat 側の列挙型へ追加するまで使えません。列挙子の名前は、略語を避ける規範に従い `ROUND_ROBIN` のように展開します。

**Related**

[Q-009](#q-009-api-の粒度)、[Q-012](#q-012-関数名の構成)

**Assessment**

推奨 A が選択されました。

### Q-011 公開ヘッダーの配置

**Status**

解決済み

**Question**

公開ヘッダーを、どのように配置しますか。

**Context**

[プラットフォーム抽象化ガイドライン](../../platform-abstraction-guideline.md#単一プラットフォーム専用-api-はヘッダー全体で限定する) は、片側専用の公開面をヘッダー全体で `PLATFORM_*` で囲うと定めています。  
先例は `trace/syslog.h` (Linux 専用) と `trace/etw.h` (Windows 専用) の 2 ヘッダー構成です。  
将来の抽象化 API は、プラットフォーム名を含まないヘッダーに置く想定です。

```text
案 A: プラットフォーム別の 2 ヘッダー
  cplat/runtime/process_priority_linux.h    (全体を PLATFORM_LINUX で囲う)
  cplat/runtime/process_priority_windows.h  (全体を PLATFORM_WINDOWS で囲う)
  (将来の抽象化) cplat/runtime/process_priority.h

案 B: 1 ヘッダー内で分岐
  cplat/runtime/process_priority.h
    #if defined(PLATFORM_LINUX) ... #elif defined(PLATFORM_WINDOWS) ... #endif
  (将来の抽象化も同じヘッダーへ追加)
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `cplat/runtime/` にプラットフォーム別の 2 ヘッダーを置く。
- [ ] B: `cplat/runtime/` に 1 ヘッダーを置き、内部で `PLATFORM_*` により分岐する。
- [ ] C: その他 (別カテゴリの新設など)
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 規範と syslog / etw の先例に一致し、将来の抽象化 API を別ヘッダーに追加しても既存の特化 API と混ざりません。プロセス情報と起動を扱う `runtime` カテゴリに属します。  
制約: 両方を使う利用側は、2 つのヘッダーを `PLATFORM_*` の条件付きでインクルードします。

**Related**

[Q-012](#q-012-関数名の構成)

**Assessment**

推奨 A が選択されました。`cplat/runtime/process_priority_linux.h` と `cplat/runtime/process_priority_windows.h` を設けます。

### Q-012 関数名の構成

**Status**

解決済み

**Question**

公開関数名の構成を、どれにしますか。

**Context**

[cplat の API 命名規約](../../coding-guideline.md#api-命名規約) は `cplat_<カテゴリ名詞>_<動詞または属性>` を正とします。  
プラットフォーム特化 API の名前にプラットフォーム名を含めない場合、将来の抽象化 API が同じ名前を使えなくなり、特化 API の改名が必要になります。

```c
/* 案 A: カテゴリ名詞にプラットフォーム名を含める */
cplat_process_priority_linux_set_policy(...)
cplat_process_priority_linux_set_nice(...)
cplat_process_priority_windows_set_class(...)
/* 将来の抽象化: cplat_process_priority_set(...) */

/* 案 B: プラットフォーム名を先頭に置く */
cplat_linux_process_priority_set_policy(...)
cplat_windows_process_priority_set_class(...)

/* 案 C: プラットフォーム名を含めない */
cplat_process_priority_set_policy(...)   /* Linux のみ宣言 */
cplat_process_priority_set_class(...)    /* Windows のみ宣言 */
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `cplat_process_priority_<linux|windows>_<動詞>_<属性>` とする。
- [ ] B: `cplat_<linux|windows>_process_priority_<動詞>_<属性>` とする。
- [ ] C: プラットフォーム名を含めない。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: カテゴリ名詞 `process_priority` が先頭に来るため、命名規約の「カテゴリ名詞を先に置く」順序を保ちつつ、将来の抽象化 API (`cplat_process_priority_*`) と特化 API を同じ接頭辞で並べられます。  
制約: 関数名が長くなります (例: `cplat_process_priority_linux_set_io_priority`)。

**Related**

[Q-010](#q-010-設定値の型)、[Q-011](#q-011-公開ヘッダーの配置)

**Assessment**

推奨 A が選択されました。

### Q-013 Windows 実装の検証方法

**Status**

解決済み

**Question**

Windows 実装の動作確認を、どの方法で行いますか。

**Context**

この開発環境は WSL2 上の Linux であり、Windows の実装をビルド・実行できません。  
Windows 実装には、[Q-007](#q-007-windows-のリアルタイム指定の格下げの扱い) の格下げの検出のように、実機でしか確かめられない挙動があります。  
Linux 実装は、testfw の mock による経路の検証と、一般ユーザー権限での実際の呼び出し (優先度を下げる操作の成功、上げる操作の `CPLAT_ERR_PERMISSION_DENIED`) でこの環境で確認できます。

**Answer**

選択方式: 単一選択

- [x] A: (推奨) Windows はユーザーが手元の Windows 環境または CI でビルドとテストを実行し、その結果を受入確認に使う。
- [ ] B: Windows は単体テストを用意するだけとし、実行結果の確認は今回の受入条件に含めない (未確認として残課題に記録する)。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 19:54:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 「Linux/GCC と Windows/MSVC の双方で動作する構成を維持する」というリポジトリの規則 (`AGENTS.md`) を受入時点で確認できます。  
制約: 管理者権限の有無で結果が変わるテスト (リアルタイム クラスなど) は、実行環境の権限に応じて検証内容を切り替える必要があります。

**Assessment**

推奨 A が選択されました。Windows のビルドとテストは、ユーザーが手元の Windows 環境または CI で実行した結果を受入確認に使います。

### Q-014 Linux で ID を指定した場合に変更するスレッド

**Status**

解決済み

**Question**

Linux で他プロセスを対象にする場合、ID で指定したスレッドだけを変更しますか、そのプロセスの全スレッドを変更しますか。

**Context**

[Q-001](#q-001-対象プロセスの範囲) で任意の PID を対象にすることが決まり、[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲) の補足で「プロセス外から操作することはできないのか」という確認がありました。

Linux の `sched_setscheduler`・`setpriority`・`ioprio_set`・`sched_setattr` が受け取る ID は、実際にはスレッド ID (TID) です。  
PID を渡すと、TID が PID と等しいスレッド、つまりそのプロセスのメイン スレッドだけが変更され、他のスレッドは変わりません ([setpriority(2)](https://man7.org/linux/man-pages/man2/setpriority.2.html)、[sched_setattr(2)](https://man7.org/linux/man-pages/man2/sched_setattr.2.html))。  
OS 標準のコマンドも同じ挙動であり、`renice -p` はメイン スレッドだけを変え、`chrt` は `--all-tasks` を指定した場合に限り全スレッドを変えます。

全スレッドを変更するには `/proc/<pid>/task` を列挙して各スレッドへ適用します。  
列挙中に作られたスレッドの取りこぼしと、途中のスレッドで失敗した場合の扱い (適用済みを戻すか、そのまま失敗を返すか) が別途必要になります。

```c
/* 案 A: 指定した ID のスレッドだけを変更する (ID は TID として扱い、PID を渡せばメイン スレッド) */
ret = cplat_process_priority_linux_set_nice(target_pid, 10, &detail);       /* メイン スレッドのみ */
ret = cplat_process_priority_linux_set_nice(target_tid, 10, &detail);       /* 任意のスレッド */

/* 案 B: PID で指定したプロセスの全スレッドを変更する */
ret = cplat_process_priority_linux_set_nice(target_pid, 10, &detail);       /* /proc/<pid>/task を列挙 */

/* 案 C: 案 A を既定とし、全スレッドを対象にするフラグを設ける */
ret = cplat_process_priority_linux_set_nice(target_pid, CPLAT_..._ALL_THREADS, 10, &detail);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 指定した ID のスレッドだけを変更する。ID はスレッド ID として扱い、PID を渡した場合はメイン スレッドだけが変わることを文書化する。
- [ ] B: PID で指定したプロセスの全スレッドを変更する。
- [ ] C: A を既定とし、全スレッドを変更するフラグを設ける。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 20:55:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 1 回の呼び出しが 1 回の OS 操作に対応し ([Q-009](#q-009-api-の粒度) と同じ考え方)、部分失敗と取りこぼしが起きません。OS 標準のコマンドの既定動作とも一致します。  
制約: 他プロセスの全スレッドを変更したい呼び出し元は、`/proc/<pid>/task` を自分で列挙して各スレッドの ID を渡します。B または C を選択した場合は、途中失敗時の扱いを追加で確認します。

**Related**

[Q-001](#q-001-対象プロセスの範囲)、[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲)、[Q-015](#q-015-windows-のスレッド優先度の対象)、[Q-017](#q-017-自プロセスと呼び出しスレッドの指定方法)

**Assessment**

推奨 A が選択されました。Linux の各関数は ID をスレッド ID として扱い、PID を渡した場合はメイン スレッドだけが変わることを API の文書に明記します。

### Q-015 Windows のスレッド優先度の対象

**Status**

解決済み

**Question**

Windows のスレッド優先度の設定と取得で、対象のスレッドをどう指定しますか。

**Context**

[Q-004](#q-004-windows-で扱う優先度の軸) でスレッド優先度 (`SetThreadPriority`) を扱うことが決まりました。  
[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲) は呼び出しスレッドだけを対象とする回答でしたが、その後 [Q-001](#q-001-対象プロセスの範囲) で他プロセスも対象になりました。

Windows では、他のスレッドは `OpenThread(THREAD_SET_LIMITED_INFORMATION, tid)` で開いて操作できます。  
[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) を推奨の A とした場合、Linux はスレッド ID で任意のスレッドを指定できるため、Windows も同じくスレッド ID を受け取ると対称になります。

```c
/* 案 A: スレッド ID で指定する (自プロセス・他プロセスを問わない) */
ret = cplat_process_priority_windows_set_thread_priority(tid, CPLAT_..._THREAD_PRIORITY_LOWEST, &detail);

/* 案 B: 呼び出しスレッドのみ (対象の引数を持たない) */
ret = cplat_process_priority_windows_set_thread_priority(CPLAT_..._THREAD_PRIORITY_LOWEST, &detail);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) スレッド ID で指定する。
- [ ] B: 呼び出しスレッドのみを対象とする。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 20:55:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: [Q-001](#q-001-対象プロセスの範囲) で他プロセスを対象にした方針と、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド) の推奨案のスレッド ID 指定に対称になります。  
制約: 他プロセスのスレッドを開けない場合は `CPLAT_ERR_PERMISSION_DENIED` になります。

**Related**

[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲)、[Q-004](#q-004-windows-で扱う優先度の軸)、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド)、[Q-017](#q-017-自プロセスと呼び出しスレッドの指定方法)

**Assessment**

推奨 A が選択されました。Windows のスレッド優先度の設定と取得は、スレッド ID で対象を指定します。

### Q-016 Windows のバックグラウンド処理モードの対象指定

**Status**

解決済み

**Question**

自プロセスにしか適用できない Windows のバックグラウンド処理モードの関数に、対象プロセスの引数を設けますか。

**Context**

[Q-001](#q-001-対象プロセスの範囲) で他プロセスも対象になりましたが、`SetPriorityClass` の `PROCESS_MODE_BACKGROUND_BEGIN` / `END` は「`hProcess` が現在のプロセスのハンドルである場合に限り指定できる」と [公式文書](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass) が定めています。

```c
/* 案 A: 対象の引数を持たない (自プロセス専用であることが関数の形で分かる) */
ret = cplat_process_priority_windows_begin_background_mode(&detail);

/* 案 B: 他の関数と同じく PID を受け取り、他プロセスなら CPLAT_ERR_UNSUPPORTED を返す */
ret = cplat_process_priority_windows_begin_background_mode(pid, &detail);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 対象の引数を持たない自プロセス専用の関数にする。
- [ ] B: PID を受け取り、他プロセスを指定した場合は `CPLAT_ERR_UNSUPPORTED` を返す。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 20:55:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 必ず失敗する引数の組み合わせを、関数の形で作れないようにできます。  
制約: 他の関数と引数の並びが揃いません。

**Related**

[Q-001](#q-001-対象プロセスの範囲)、[Q-004](#q-004-windows-で扱う優先度の軸)、[Q-020](#q-020-バックグラウンド処理モードの二重開始と未開始での終了)

**Assessment**

推奨 A が選択されました。バックグラウンド処理モードの開始と終了は、対象の引数を持たない自プロセス専用の関数とします。二重開始と未開始での終了の扱いは [Q-020](#q-020-バックグラウンド処理モードの二重開始と未開始での終了) で確認します。

### Q-017 自プロセスと呼び出しスレッドの指定方法

**Status**

解決済み

**Question**

自プロセスまたは呼び出しスレッドを対象にするとき、どう指定させますか。

**Context**

[Q-001](#q-001-対象プロセスの範囲) で、対象を PID で指定する形になりました。  
ID の型は、既存の `cplat_process_get_pid()` / `cplat_process_get_tid()` に合わせて `uint32_t` とする想定です。

- Linux のシステム コールは、ID に 0 を渡すと呼び出しスレッドを対象にします。
- Windows の PID 0 は System Idle Process であり、優先度を変更できる対象ではありません。そのため 0 を「自分」の意味に予約しても、指定できる対象は減りません。Windows は `GetCurrentProcess()` / `GetCurrentThread()` で自分を開かずに操作できます。

```c
/* 案 A: ID 0 を自分 (Linux は呼び出しスレッド、Windows は自プロセスまたは呼び出しスレッド) とする */
ret = cplat_process_priority_linux_set_nice(0, 10, &detail);

/* 案 B: 自分用の関数と ID 指定の関数を分ける */
ret = cplat_process_priority_linux_set_self_nice(10, &detail);
ret = cplat_process_priority_linux_set_nice(target_tid, 10, &detail);

/* 案 C: 呼び出し側が自分の ID を渡す (0 は CPLAT_ERR_INVALID_ARGUMENT) */
ret = cplat_process_priority_linux_set_nice(cplat_process_get_tid(), 10, &detail);
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) ID 0 を自分 (Linux は呼び出しスレッド、Windows は自プロセスまたは呼び出しスレッド) の意味にする。
- [ ] B: 自分用の関数と ID 指定の関数を分ける。
- [ ] C: 呼び出し側が自分の ID を渡し、0 は不正な引数とする。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 20:55:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: Linux のシステム コールの規約と一致し、Windows でも 0 は操作できない ID のため衝突しません。関数の数が増えません。  
制約: Linux で 0 と自プロセスの PID は意味が異なります (0 は呼び出しスレッド、PID はメイン スレッド)。この差は API の文書に明記します。

**Related**

[Q-001](#q-001-対象プロセスの範囲)、[Q-002](#q-002-linux-で変更が及ぶスレッドの範囲)、[Q-014](#q-014-linux-で-id-を指定した場合に変更するスレッド)、[Q-015](#q-015-windows-のスレッド優先度の対象)

**Assessment**

推奨 A が選択されました。ID 0 は、Linux では呼び出しスレッド、Windows ではプロセス単位の関数で自プロセス、スレッド単位の関数で呼び出しスレッドを意味します。

### Q-018 対象が存在しない場合の結果コード

**Status**

解決済み

**Question**

指定した PID またはスレッド ID が存在しない場合の結果コードを、どう扱いますか。

**Context**

[Q-001](#q-001-対象プロセスの範囲) で ID 指定が加わり、対象が存在しない失敗が起こるようになりました。  
現状の共通写像では、この失敗が `CPLAT_ERR_NOT_FOUND` になりません。

- Linux は `ESRCH` を返しますが、[result.c](../../../prod/libsrc/cplat/base/result.c) の `cplat_internal_result_from_errno()` は `ESRCH` を扱わず `CPLAT_ERR_UNKNOWN` になります。`error.c` の要因も `CPLAT_CAUSE_OTHER` になります。
- Windows の `OpenProcess` / `OpenThread` は、存在しない ID に対して `ERROR_INVALID_PARAMETER` を返し、これは `CPLAT_ERR_INVALID_ARGUMENT` に写像されます。`ERROR_INVALID_PARAMETER` は他の用途でも使われるため、共通写像で `CPLAT_ERR_NOT_FOUND` へ変えることはできません。
- cplat の製品コードには、現在 `ESRCH` を参照する箇所がありません。共通写像へ `ESRCH` を追加しても、既存 API の結果は変わりません。

```c
/* 案 A: 共通写像に ESRCH → CPLAT_ERR_NOT_FOUND / CPLAT_CAUSE_NOT_FOUND を追加し、
         Windows は本 API 内で OpenProcess / OpenThread の ERROR_INVALID_PARAMETER を CPLAT_ERR_NOT_FOUND へ変換する */

/* 案 B: 共通写像は変えず、Linux と Windows の両方で本 API 内だけ変換する */

/* 案 C: 変換しない (Linux は CPLAT_ERR_UNKNOWN、Windows は CPLAT_ERR_INVALID_ARGUMENT) */
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 共通写像に `ESRCH` を追加し、Windows は本 API 内で変換して、両方とも `CPLAT_ERR_NOT_FOUND` を返す。
- [ ] B: 共通写像は変えず、本 API 内だけで変換して、両方とも `CPLAT_ERR_NOT_FOUND` を返す。
- [ ] C: 変換しない。
- [ ] D: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-25 20:55:50 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: `ESRCH` は「対象のプロセスまたはスレッドが存在しない」という意味しか持たないため、共通写像で扱うのが正しく、将来プロセスを ID で扱う API が増えても個別の変換が不要になります。Windows の `ERROR_INVALID_PARAMETER` は多義的なため、ID を開く箇所でだけ変換します。  
制約: 共通写像の変更に伴い、`result.c` と `error.c` の写像表、`error.h` の `CPLAT_CAUSE_NOT_FOUND` の説明、既存の写像テストを更新します。

**Related**

[Q-001](#q-001-対象プロセスの範囲)、[Q-005](#q-005-権限不足の通知方式)、[Q-019](#q-019-os-が機能に対応しない場合の結果コード)

**Assessment**

推奨 A が選択されました。共通写像 (`result.c` と `error.c`) に `ESRCH` を追加し、Windows は `OpenProcess` / `OpenThread` の `ERROR_INVALID_PARAMETER` を本 API 内で `CPLAT_ERR_NOT_FOUND` へ変換します。本 API 内の変換には、既存の `cplat_internal_error_report_windows_error()` に相当する結果コード指定版を使います (同じ仕組みの errno 版 `cplat_internal_error_report_errno_as()` がすでにあります)。  
OS が機能に対応しない場合の写像にも同種の論点があるため、[Q-019](#q-019-os-が機能に対応しない場合の結果コード) で確認します。

### Q-019 OS が機能に対応しない場合の結果コード

**Status**

解決済み

**Question**

カーネルや OS が機能に対応しない場合の結果コードの写像を、どこで行いますか。

**Context**

[Q-003](#q-003-linux-で扱う優先度の軸) の推奨理由で、デッド ライン スケジューリングと CPU 使用率クランプがカーネルの版や構成によって使えない場合は `CPLAT_ERR_UNSUPPORTED` で通知する想定を示し、その案が採用されました。  
ただし、現状の共通写像では、その結果コードになりません。

- Linux は `ENOSYS` (システム コールがない)、`EOPNOTSUPP` / `ENOTSUP` (カーネル構成で無効) を返します。[error.c](../../../prod/libsrc/cplat/base/error.c) の要因判定はこれらを `CPLAT_CAUSE_UNSUPPORTED` にしますが、[result.c](../../../prod/libsrc/cplat/base/result.c) の `cplat_internal_result_from_errno()` は扱わず、戻り値は `CPLAT_ERR_UNKNOWN` になります。要因と戻り値が食い違っている状態です。
- Windows の `ERROR_NOT_SUPPORTED` も、要因は `CPLAT_CAUSE_UNSUPPORTED`、戻り値は `CPLAT_ERR_UNKNOWN` です。
- 共通写像は、`cplat_internal_error_report_errno()` を通じて製品コードの約 20 ファイル、`cplat_internal_error_report_windows_error()` を通じて 6 ファイルから使われています。共通写像を直すと、これらの API でも該当する errno が起きたときの戻り値が `CPLAT_ERR_UNKNOWN` から `CPLAT_ERR_UNSUPPORTED` に変わります。
- [Q-018](#q-018-対象が存在しない場合の結果コード) の `ESRCH` は製品コードに参照がなく既存の結果を変えませんでしたが、この論点は既存 API の結果を変える点が異なります。
- 本 API 内だけで変換する場合は、既存の `cplat_internal_error_report_errno_as()` で結果コードを指定できます。

```c
/* 案 A: 共通写像を直す (result.c) */
else if ((errno_value == ENOSYS) || (errno_value == ENOTSUP) || (errno_value == EOPNOTSUPP))
{
    result = CPLAT_ERR_UNSUPPORTED;
}
/* Windows も ERROR_NOT_SUPPORTED → CPLAT_ERR_UNSUPPORTED */

/* 案 B: 本 API 内だけで変換する */
if ((errno_value == ENOSYS) || (errno_value == EOPNOTSUPP))
{
    ret = cplat_internal_error_report_errno_as(detail_out, errno_value, CPLAT_ERR_UNSUPPORTED);
}
```

**Answer**

選択方式: 単一選択

- [x] A: (推奨) 共通写像を直し、`ENOSYS` / `ENOTSUP` / `EOPNOTSUPP` と `ERROR_NOT_SUPPORTED` を `CPLAT_ERR_UNSUPPORTED` にする (要因判定との食い違いも解消する)。
- [ ] B: 共通写像は変えず、本 API 内だけで変換する。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-26 07:51:10 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 要因判定 (`CPLAT_CAUSE_UNSUPPORTED`) と戻り値 (`CPLAT_ERR_UNKNOWN`) の食い違いは、本案件と無関係に存在する不整合です。本 API の局所変換で済ませると不整合が残り、同じ変換を将来の API ごとに書くことになります。`result.h` の `CPLAT_ERR_UNSUPPORTED` の定義 (現在のプラットフォームまたは状態での非サポート操作) にも合致します。  
制約: 既存 API で該当する errno が起きた場合の戻り値が変わります。既存の写像テストと、`CPLAT_ERR_UNKNOWN` を期待している既存テストの有無を確認し、必要な箇所を同じ変更で更新します。B を選択した場合は、食い違いを残課題に記録します。

**Related**

[Q-003](#q-003-linux-で扱う優先度の軸)、[Q-018](#q-018-対象が存在しない場合の結果コード)

**Assessment**

推奨 A が選択されました。`result.c` の `cplat_internal_result_from_errno()` に `ENOSYS`・`ENOTSUP`・`EOPNOTSUPP`、`cplat_internal_result_from_windows_error()` に `ERROR_NOT_SUPPORTED` を追加し、`CPLAT_ERR_UNSUPPORTED` へ写像します。  
選択肢の「要因判定との食い違いも解消する」に従い、`error.c` で同じく `CPLAT_CAUSE_UNSUPPORTED` に分類している `ERROR_CALL_NOT_IMPLEMENTED` (Windows で `ENOSYS` に相当) も対象に含めます。  
`result.h` の `CPLAT_ERR_UNSUPPORTED` の `@note` は「これらを変換しない」と明記しているため、同じ変更で写像の対象を列挙する記述へ改めます。  
既存の製品コードとテストを調べた結果、これらの値に依存して `CPLAT_ERR_UNKNOWN` を期待している箇所はありません (`errorTest.cc` の要因判定のテストだけが `ENOSYS` を参照し、結果は変わりません)。写像テスト (`resultTest.cc`) に、追加した値の確認を加えます。  
あわせて、Linux 6.6 の開発環境は `CONFIG_UCLAMP_TASK` が無効であり、CPU 使用率クランプの設定は `EOPNOTSUPP` で失敗します。この経路は `CPLAT_ERR_UNSUPPORTED` になることを実機で確認できます。  
Linux 5.3 より前のカーネルは CPU 使用率クランプの項目を持たない `sched_attr` しか知らず、`sched_setattr()` が `E2BIG` を返します ([sched_setattr(2)](https://man7.org/linux/man-pages/man2/sched_setattr.2.html))。`E2BIG` は他の用途と共有されるため共通写像は変えず、本 API の CPU 使用率クランプの関数内でだけ `CPLAT_ERR_UNSUPPORTED` へ変換します ([Q-018](#q-018-対象が存在しない場合の結果コード) の Windows の変換と同じ考え方です)。

### Q-020 バックグラウンド処理モードの二重開始と未開始での終了

**Status**

解決済み

**Question**

Windows のバックグラウンド処理モードを、すでにモード中に開始した場合と、モード外で終了した場合に、何を返しますか。

**Context**

[公式文書](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setpriorityclass) では、モード中の開始は `ERROR_PROCESS_MODE_ALREADY_BACKGROUND`、モード外での終了は `ERROR_PROCESS_MODE_NOT_BACKGROUND` で失敗します。  
現在モード中かどうかを取得する公開 API はありません ([Q-008](#q-008-現在値の取得-api-の要否) の Assessment)。  
これらのエラーは現状の共通写像にないため、何もしなければ `CPLAT_ERR_UNKNOWN` になります。

プロセス内の複数の部品がそれぞれモードを開始・終了する場合、2 つ目の部品の開始は「すでにモード中」になります。

```c
/* 案 A: 省略として CPLAT_SKIPPED を返す (エラーではない) */
ret = cplat_process_priority_windows_begin_background_mode(&detail);
if (ret == CPLAT_SKIPPED)
{
    /* 自分は開始していないので、終了も呼ばない */
}

/* 案 B: 状態に対する非サポート操作として CPLAT_ERR_UNSUPPORTED を返す */
ret = cplat_process_priority_windows_begin_background_mode(&detail);
if (ret != CPLAT_OK)
{
    /* 失敗扱い。すでにモード中なのか他の失敗なのかは detail で区別する */
}
```

- 案 A は、`result.h` の `CPLAT_SKIPPED` (適用対象外による省略、エラーではない) を使います。呼び出し側は「自分が開始したか」を戻り値で知り、終了の要否を判断できます。成否判定を `ret != CPLAT_OK` で書いている呼び出し側は、省略を失敗として扱います。
- 案 B は、`CPLAT_ERR_UNSUPPORTED` の定義 (現在の状態での非サポート操作) に合致します。目的の状態にはなっているのに失敗が返ります。

**Answer**

選択方式: 単一選択

- [x] A: (推奨) `CPLAT_SKIPPED` を返す。
- [ ] B: `CPLAT_ERR_UNSUPPORTED` を返す。
- [ ] C: その他
- [ ] X: 意図不明 (詳細な質問内容に再生成すること)

その他の回答 / 補足:

回答者: Tetsuo Honda  
回答日時: 2026-10-26 07:51:10 JST (検討用 Markdown の保存時刻)

**Recommendation**

推奨: A  
理由: 呼び出し後の状態は要求どおり (モード中、またはモード外) であり、失敗ではありません。モード中かどうかを取得する API がないため、「自分が開始したか」を戻り値で伝えると、呼び出し側が終了の要否を判断できます。  
制約: 戻り値の判定を `ret == CPLAT_OK` だけで書くと省略を見落とすため、API の文書に明記します。

**Related**

[Q-008](#q-008-現在値の取得-api-の要否)、[Q-016](#q-016-windows-のバックグラウンド処理モードの対象指定)

**Assessment**

推奨 A が選択されました。開始関数は `ERROR_PROCESS_MODE_ALREADY_BACKGROUND`、終了関数は `ERROR_PROCESS_MODE_NOT_BACKGROUND` を受けた場合に `CPLAT_SKIPPED` を返します。  
`CPLAT_SKIPPED` はエラーではないため、cplat 規範の「成功時は出力引数と直前値をクリアする」に合わせ、`detail_out` とスレッド ローカルの直前値をクリアします。  
戻り値を `ret == CPLAT_OK` だけで判定すると省略を失敗として扱う点は、API の文書に明記します。

## 対象成果物と合意版

最終決定事項 (最終仕様) の設計書: `prompt/current/process-priority-design.md`

検討用 Markdown の合意版 (第 1 ゲート): 版 4 (2026-10-26 確定)

最終仕様 (design.md) の合意版 (第 2 ゲート): 2026-10-26 承認 (`prompt/current/process-priority-design.md`)

更新予定の成果物: 作業 WBS の第 4 節

受入条件:

- 「現行の合意内容」のすべての決定が、公開ヘッダー、実装、機能仕様、API チート シートに反映されていること。
- 権限不足が、両 OS で `CPLAT_ERR_PERMISSION_DENIED` として返り、`detail_out` とスレッド ローカルの直前値に OS の詳細が記録されること。
- Linux の局所ビルドと `app/cplat` の関連テスト (新規の単体テスト、`resultTest`、`errorTest`、`exportTest`) が成功し、`.warn` に今回の変更による警告がないこと。
- 新規の OS API に testfw の mock があり、mock で失敗経路 (権限不足、対象不在、非対応) を検証していること。
- Windows のビルドとテストが、ユーザーの Windows 環境または CI で成功すること ([Q-013](#q-013-windows-実装の検証方法))。
- 変更した Markdown に `text_style_jp.py --dry-run` の指摘がないこと。

## 反映と検証の記録

### 検証結果 (Linux、2026-10-26)

- `framework/testfw/libsrc/mock_libc`、`app/cplat/prod/libsrc/cplat`、`app/cplat/test/libsrc/mock_cplat` のビルド: 成功、`.warn` の内容なし
- `processPriorityTest`: 18 件成功 (実機で権限不足と対象不在、CPU 使用率クランプの非対応を確認。`process_priority_linux.c` の行カバレッジ 83%)
- `resultTest` 4 件、`errorTest` 16 件、`exportTest` 2 件、`processTest` 38 件: すべて成功
- `make doxy` (`app/cplat`): 警告なし
- Markdown (`runtime.md`、`api-cheatsheet.md`、design、discussion) と新規ソースの `text_style_jp.py`: 指摘なし

### 検証結果 (CI、2026-10-26)

- 対象: c-modernization-kit `6d8bbc8a` (cplat `6eea02c1`、testfw `b7f953b`)。上流の cplat の変更へリベース後、ローカルで `processPriorityTest` と `exportTest` を再実行して成功
- c-modernization-kit の CI (run 40257847104): 全ジョブ成功、警告アーティファクトなし
    - `build-and-test-windows`: `processPriorityWindowsTest` 9 件成功
    - `build-and-test-linux` (ol8、ol9、ol10): `processPriorityLinuxTest` 16 件成功、2 件省略。省略は、CI のコンテナーに優先度を上げる権限があることと、カーネルが CPU 使用率クランプに対応していることによる設計どおりの省略
- testfw の CI (Build Libraries、run 40255221615): Linux 3 環境と Windows で成功

### 最終仕様との差異 (実装時の補正)

- `cplat.h`: 最終仕様は「集約ヘッダーへ追加」としていましたが、`cplat.h` は片側専用ヘッダー (`trace/etw.h`、`trace/syslog.h` など) を含めず、除外ヘッダーの一覧に記載する規則です。規則に従い除外一覧へ追加し、最終仕様の記述も同じ内容へ改めました。
- `exports.map`: `cplat_*` のワイルドカードで公開されるため、変更は不要でした。最終仕様の変更対象ファイルから除きました。
