<!--
生成元: agree-before-implement スキル
文書種別: 残課題一覧 (backlog)
最終更新エージェント: copilot / セッション ID: 8f839972-2ad8-411b-8421-b03bde2c138a
上流文書: `261108-sym-loader-multi-target-design.md` (最終仕様)、`261108-sym-loader-multi-target-discussion.md` (検討用 Markdown)。いずれも削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。「整理状態」が「随時追記中」であれば、案件の作業中です。気付いた事項を追記し、第 3 ゲートの前に文書を再構築して整理します。上流文書が存在する場合は、各課題の背景と見送りの判断を上流文書で確認します。存在しない場合は、この文書を単体の残課題一覧として扱います。このファイルの残課題を参照して新たな成果物の作成または更新を始めるときは、元案件の再開とは別の作業として扱います。その開始には agree-before-implement を原則として使い、新しい検討用 Markdown から開始してください。継続作業によってこのファイルの残課題を対策する場合は、このファイルや各項目の記載を削除することなく、各項目の状態および経緯と対処を更新することを原則とします。また、フェーズ分けした対策で対処ができていない項目がある場合は、当該継続作業の backlog にも必ず転記し、最新作業の backlog を見れば残件が把握できるようにしてください。最終更新エージェントのセッション情報が参照可能であれば再開時のコンテキストとして利用し、別エージェントからの引き継ぎや担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。
-->

# 残課題一覧 (バックログ)

ファイル名: `261108-sym-loader-multi-target-backlog.md`

## 案件の識別

案件名: `sym_loader` で 1 つのコールバック元に複数のコールバック先を持たせる  
作成日: `2026-11-08`  
整理状態: 整理済み

この文書は、見送る決定がなされた問題点や、作業中に潜在または別問題として判明したが今回は対策しなかった残渣を記録します。実装と検証のあと、第 3 ゲートの前に再構築して整理しました。

## 残課題一覧

| 課題 ID | 題名 | 区分 | 優先度 / 影響度 | 状態 |
| --- | --- | --- | --- | --- |
| BL-001 | エントリ ポインターが NULL のときの検査がない | 作業中摘出 | 中 | 対応済み |

Table: 残課題一覧

## BL-001 エントリ ポインターが NULL のときの検査がない

### 概要と問題点

- **現状の事象**: `cplat_sym_loader_count`、`cplat_sym_loader_get`、`cplat_sym_loader_reject_count`、`cplat_sym_loader_reject_get`、`cplat_sym_loader_is_default`、`cplat_sym_loader_is_failed` は、エントリのポインターが NULL でも検査せずにメンバーを読みます。`cplat_sym_loader_dispose` は、配列とその要素が NULL でも検査せずにメンバーを読みます。NULL を渡すと、NULL ポインターの間接参照になります。削除した `cplat_sym_loader_resolve` も同じ未検査でした。`cplat_sym_loader_info` は配列と要素の NULL を検査し、`cplat_sym_loader_init` は配列と要素の NULL を読み飛ばします。
- **本来あるべき姿**: 公開関数は、NULL のエントリを受け取ったときに失敗を返すか、何もしないかを契約として持ち、間接参照を起こしません。

### 経緯と見送り理由

- **摘出契機**: 複数のコールバック先を検討する際に、既存の解決と解放の契約を確認した。同じ事象は、ソース コメントの機械的な残課題 `app/general/docs/prompts/current/261108-source-comment-mechanical-backlog.md` の BL-013 にすでに記載されています。
- **今回の見送り理由**: 今回の範囲は、1 つのコールバック元が複数のコールバック先を持つ契約です。NULL 検査の追加は、その契約とは別の防御的変更です。最終仕様の「今回含めないこと」に同じ判断を書いています。
- **継続作業での経緯と対処**: [NULL 引数と資源の寿命の最終仕様](261108-sym-loader-null-contract-design.md) に従い、取得・診断と初期化・解放の引数を検査し、結果コードと出力引数で失敗を通知します。解放失敗時のハンドル保持と再試行も実装し、利用側の終了手順を更新しました。Linux と Windows の局所テストで確認しました。

### 影響範囲とリスク

- **影響対象**: `app/cplat/prod/libsrc/cplat/runtime/sym_loader_resolve.c` の件数、取得、診断、`sym_loader_is_default.c`、`sym_loader_is_failed.c`、`sym_loader_dispose.c`。
- **放置した場合のリスク**: 呼び出し側の不具合が、結果コードではなくプロセスの異常終了として現れます。今回の関数を足したあとも、NULL を渡したときの振る舞いは変わりません。

### 推奨される対応策

- **恒久対応案**: 件数、取得、診断、明示的既定、内部失敗、解放の入口で NULL を検査し、取得系は NULL または `CPLAT_ERR_INVALID_ARGUMENT`、解放は何もしない、と契約を分けて実装とテストを追加します。ソース コメント側の残課題と重複するため、対策するときは両方の記載を更新します。
- **参考情報**: `app/cplat/prod/libsrc/cplat/runtime/sym_loader_dispose.c`、`app/cplat/prod/libsrc/cplat/runtime/sym_loader_resolve.c`、`app/cplat/prod/libsrc/cplat/runtime/sym_loader_is_failed.c`
