<!--
生成元: agree-before-implement スキル
文書種別: 残課題一覧 (backlog)
最終更新日時: 2026-11-28 08:42:00 JST
最終更新エージェント: agy / セッション ID: 73c19cdb-9b01-4b7c-831a-02f92420ab83
稼働 OS: Linux (WSL)
上流文書: `261127-service-commonization-overview.md` (全体管理文書)、`261127-service-commonization-design.md` (最終仕様)、`261127-service-commonization-discussion.md` (検討用 Markdown)、`261127-service-commonization-lightadr.md` (簡易 ADR)。いずれも削除されている場合があります。
下流文書: 未作成
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。「整理状態」が「随時追記中」であれば、案件の作業中です。気付いた事項を追記し、第 3 ゲートの前に文書を再構築して整理します。上流文書が存在する場合は、各課題の背景と見送りの判断を上流文書で確認します。存在しない場合は、この文書を単体の残課題一覧として扱います。このファイルの残課題を参照して新たな成果物の作成または更新を始めるときは、元案件の再開とは別の作業として扱います。その開始には agree-before-implement を原則として使い、新しい全体管理文書と検討用 Markdown から開始してください。継続作業によってこのファイルの残課題を対策する場合は、このファイルや各項目の記載を削除することなく、継続作業の開始時 (全体管理文書と検討用 Markdown の作成時) から検討・設計・実装・整理の各段階において、このファイルのメタ情報、各項目の状態、および経緯と対処を随時更新することを原則とします。また、フェーズ分けした対策で対処できていない項目がある場合は、当該継続作業の backlog にも必ず転記し、最新作業の backlog を見れば残件が把握できるようにしてください。最終更新エージェントのセッション情報や稼働 OS・最終更新日時が参照可能であれば再開時のコンテキストや環境差分の把握に利用し、別エージェントや別環境・別 OS からの引き継ぎ、担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。
-->

# 残課題一覧 (バックログ)

ファイル名: `261127-service-commonization-backlog.md`

## 案件の識別

案件名: `service-sample の共通化可能部分の cplat 取り込み`  
作成日: `2026-11-27`  
整理状態: 随時追記中

この文書は、見送る決定がなされた事項や、作業中に判明した潜在的課題・将来の改善余地を随時記録する残課題一覧です。  
第 3 ゲートの前に内容を再構築して整理します。

## 残課題一覧

Table: 残課題一覧

| 課題 ID | 題名 | 区分 | 優先度 / 影響度 | 状態 |
| --- | --- | --- | --- | --- |
| [BL-001](#bl-001-account-session-導入時の-libsystemd-動的ローダー内部共有) | account-session 導入時の libsystemd 動的ローダー内部共有 | 検討時見送り | 低 | 解消 |

## BL-001 account-session 導入時の libsystemd 動的ローダー内部共有

### 概要と問題点

- **現状の事象**: 本案件では `prod/libsrc/cplat/service/` 内で `libsystemd.so.0` を `dlopen()` / `dlsym()` で動的ロードする実装を自己完結で構築します。並行検討中の [cplat-account-session-discussion.md](cplat-account-session-discussion.md) でも `libsystemd` の実行時動的ロード (Q-017) が検討されており、将来両者が実装されると `libsystemd.so.0` の `dlopen` / `dlsym` 基盤が 2 か所に重複する可能性があります。
- **本来あるべき姿**: `account` / `session` 機能が `libsystemd.so.0` の動的ロードを採用して実装される時点で、内部共有モジュール (`prod/include_internal/cplat/` 配下など) へ動的ローダーを共通化し、重複を解消します。

### 経緯と見送り理由

- **摘出契機**: 前提検討時 ([Q-012](261127-service-commonization-discussion.md#q-012-並行検討中の-account-session-案件との調整)) に確認・見送り決定。
- **今回の見送り理由**: `cplat-account-session` 案件が未確定・未着手であるため、本案件では依存を持たず自己完結で実装し、将来の統合余地として残す決定 ([LADR-012](261127-service-commonization-lightadr.md#ladr-012-並行検討中の-account-session-案件との独立性)) としたため。
- **継続作業での経緯と対処**: 2026-11-28 の [Q-020](261127-service-commonization-discussion.md#q-020-libsystemd-モックの配置先と-dlopen-呼び出し経路の整合) で C が選択され、`libcplat.so` から `libsystemd` を直接リンク (`LIBS += systemd`) し、`framework/testfw` に `libmock_systemd.a` を配置する方針 ([LADR-005](261127-service-commonization-lightadr.md#ladr-005-linux-での-libsystemd-直接リンクとモック配置)) へ改訂されました。本案件で `libsystemd` 動的ローダーを構築しなくなったため、重複の懸念は解消しました (WBS 4.12 の第 3 ゲート前整理で本項目を除外する予定)。

### 影響範囲とリスク

- **影響対象**: `app/cplat/prod/libsrc/cplat/service/` および将来の `account` / `session` 実装 (内部実装のみ、公開 API への影響なし)。
- **放置した場合のリスク**: 機能上の不具合はないものの、将来 `libsystemd` の動的ロード処理やモック差し替え機構が 2 か所で重複保守になる。

### 推奨される対応策

- **恒久対応案**: `cplat-account-session` 実装時に、`libsystemd.so.0` のハンドル管理とシンボル解決を `app/cplat` の内部共有層へ切り出し、`service` 側からも共用する。
- **参考情報**: [Q-004](261127-service-commonization-discussion.md#q-004-linux-での-libsystemd-への依存方式)、[Q-012](261127-service-commonization-discussion.md#q-012-並行検討中の-account-session-案件との調整)、[cplat-account-session-discussion.md](cplat-account-session-discussion.md)
