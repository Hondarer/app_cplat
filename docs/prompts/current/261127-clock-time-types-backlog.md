<!--
文書種別: 残課題一覧 (backlog)
作成日: 2026-11-27
再開手順: このファイルは、cplat の時刻型の型安全性向上に関する初期検討のアイデアと論点を記録した残課題一覧です。現時点では方針および導入の可否は未決定です。後日さらに精査して実装を判断する際は、この文書に記載した検討案をたたき台として見直し、必要に応じて agree-before-implement スキルで検討用 Markdown を作成して進めてください。
-->

# 残課題一覧 (バックログ)

ファイル名: `261127-clock-time-types-backlog.md`

## 案件の識別

案件名: `cplat` の時計・時刻型 (時間幅、単調時刻、UTC 時刻、ローカル時刻) の導入検討  
作成日: `2026-11-27`  
整理状態: 整理済み (初期検討・未決定)

この文書は、`cplat` を C 言語の汎用フレームワークとして見たときの時刻型の初期検討メモと論点を記録します。  
現時点ではアイデアの検討段階であり、方針や導入の可否は決定していません。  
時間幅、単調時刻、UTC 時刻、ローカル時刻の 4 型体系を導入する案を後日さらに精査し、実装の可否と進め方を判断するための残課題を整理します。

## 初期検討のアイデアと方向性案 (未決定)

- **検討中の 4 つの型案**:
    - **時間幅型**: `cplat_timespan` (または `cplat_time_span`)。経過時間やタイムアウト幅を表す案です。
    - **単調時刻型**: `cplat_monotonic_time`。時刻同期の影響を受けない単調増加クロックの時刻点を表す案です。
    - **UTC 時刻型**: `cplat_utc_time` (または `cplat_utctime`)。Unix epoch 基準の UTC 絶対時刻の瞬間を表す案です。`cplat_realtime` という名称は POSIX の `CLOCK_REALTIME` 由来であり、分解能が高い・正確であると誤解されやすいため、UTC 基準であることが直感的に伝わる名称を候補としています。
    - **ローカル時刻型**: `cplat_local_time` (または `cplat_localtime`)。実行環境のローカル タイム ゾーンで解釈した分解済み日時スナップショットを表し、年月日時分秒・ナノ秒に加えて UTC オフセットと夏時間フラグ (`is_dst`)、任意で表示用のタイム ゾーン名を持つ案です。
- **現時点で対象外とする方向で考えている拡張**:
    - UTC 時刻型に対して、C# の `DateTime` のような Local/UTC 区別フラグ、時差、夏時間フラグを内包させる拡張は行わない方向で考えています。保存・比較・通信・`timespan` との加減算は常に UTC 時刻型で行い、ローカル表現とは型で責務を分離する案です。
    - タイム ゾーン データベース (IANA tzdb 相当) の同梱や、任意のタイム ゾーン変換、ローカル時刻型上での暦計算 (+1 か月、+1 日など) は持たない方向で考えています。ローカル時刻型は各プラットフォームの標準機能 (Linux POSIX / glibc、Windows Win32 / UCRT) で実現可能な範囲に留める案です。

## 残課題一覧

Table: 時計・時刻型の導入検討に関する残課題一覧

| 課題 ID | 題名 | 区分 | 優先度 / 影響度 | 状態 |
| --- | --- | --- | --- | --- |
| BL-001 | 時間幅型 `cplat_timespan` の詳細設計と導入判断 | 設計検討 | 中 | 未着手 |
| BL-002 | 単調時刻型と UTC 時刻型の型分離および命名設計 | 設計検討 | 中 | 未着手 |
| BL-003 | ローカル時刻型の詳細設計、名前衝突解消および UTC 変換境界の整理 | 設計検討 | 中 | 未着手 |
| BL-004 | 既存 `cplat_timespec` と利用箇所の扱いおよび移行・検証計画の策定 | 設計・移行検討 | 中 | 未着手 |

## BL-001 時間幅型 cplat_timespan の詳細設計と導入判断

### 概要と問題点

- **現状の事象**: `cplat` では時間幅の表現が `cplat_timespec` (`cplat_timespec_sub()` の出力)、`int64_t` (`cplat_timespec_diff_ms()` の出力)、`uint64_t` (`cplat_timespec_add_ms()` の引数)、`int` (`*_timed_*` 系待機 API の `timeout_ms`) に分散しています。また、時刻点と同じ `cplat_timespec` を共有しているため、時刻点同士の加算や時刻点と時間幅の取り違えをコンパイル時に検出できません。
- **検討中の理想像**: 時間幅を表す専用型 `cplat_timespan` を設け、時刻点との加減算や差分の入出力を型レベルで区別し、負の時間幅の正規化規則や単位変換 (ns / us / ms / s) を一元化する案です。

### 経緯と見送り理由

- **摘出契機**: `cplat` の時刻型の設計を見直す中で、C 言語の汎用フレームワークとしての型安全性と API 一貫性を高める観点で検討を始めました。
- **今回の見送り理由**: 現時点はアイデアの初期検討段階であり、導入可否や既存 API との整合、具体的な演算・変換 API の粒度を後日さらに精査してから判断します。

### 影響範囲とリスク

- **影響対象**: `app/cplat/prod/include/cplat/clock/`、`app/cplat/prod/libsrc/cplat/clock/`、待機 API (`sync.h`、`process.h` など) との境界、`app/cplat/docs/functional-spec/clock.md`、`app/cplat/docs/coding-guideline.md`。
- **放置した場合のリスク**: 時刻点と時間幅の混同による不具合をレビューや命名規約だけで防ぐ運用が続きます。

### 推奨される対応策 (次回精査項目)

- **内部表現の検討**: 符号付き秒とナノ秒の組にするか単一の 64 ビット整数にするか、負の時間幅の正規化規則 (`tv_sec` と `tv_nsec` の符号ルール) を比較検討します。
- **既存待機 API との関係整理**: `int timeout_ms` (`CPLAT_WAIT_FOREVER` / `CPLAT_NO_WAIT`) を維持しつつ変換ヘルパーを設けるか、`cplat_timespan` を直接受け取る API を併設するかを検討します。
- **演算・変換 API の定義**: 加減算、比較、符号反転、正規化、各単位 (s / ms / us / ns) との相互変換、オーバーフロー時の `CPLAT_ERR_OUT_OF_RANGE` 契約を検討します。

## BL-002 単調時刻型と UTC 時刻型の型分離および命名設計

### 概要と問題点

- **現状の事象**: `cplat_clock_get_monotonic()` と `cplat_clock_get_realtime()` はどちらも `cplat_timespec` を返します。`app/cplat/docs/functional-spec/clock.md` で「異なる基準の値を相互に比較することはできません」と規定していますが、同じ型であるため誤った比較や減算がコンパイルを通ります。また、`realtime` という語は高精度・正確と誤解されやすい課題があります。
- **検討中の理想像**: 単調増加クロックの時刻点を `cplat_monotonic_time`、Unix epoch 基準の UTC 実時刻を `cplat_utc_time` (または `cplat_utctime`) として別型にし、異種クロック間の比較や差分計算をコンパイル エラーとして防ぐ案です。

### 経緯と見送り理由

- **摘出契機**: 時刻型の厳密化を検討する中で、単調時刻と UTC 実時刻を型レベルで分離し、かつ利用者が直感的に意味を理解できる命名へ見直すアイデアを整理しました。
- **今回の見送り理由**: 導入の可否、ローカル時刻型や既存 `cplat_timespec` との命名統一を含めて後日精査するため残課題とします。

### 影響範囲とリスク

- **影響対象**: `app/cplat/prod/include/cplat/clock/clock.h`、`timespec.h`、`crt/file.h`、`hashtable/hashtable.h`、`string_catalog/filter.h`、`trace/` 系ヘッダー、および利用側 (`app/porter`、`app/string-catalog-sample` 等)。
- **放置した場合のリスク**: 実時刻を経過時間やタイムアウト判定へ誤用する不具合、または単調時刻をログ刻印や外部比較へ誤用する不具合を型検査で防げません。

### 推奨される対応策 (次回精査項目)

- **型定義と演算の対応表の検討**:
    - `cplat_monotonic_time` 同士の差分 → `cplat_timespan`、`cplat_monotonic_time` への `cplat_timespan` 加減算 → `cplat_monotonic_time`
    - UTC 時刻型同士の差分 → `cplat_timespan`、UTC 時刻型への `cplat_timespan` 加減算 → UTC 時刻型
    - ISO 8601 整形やローカル時刻への変換は UTC 時刻型のみを受け付ける契約とする案を精査します。
- **OS 境界でのレイアウト互換性の確認**: 内部メンバーに `cplat_timespec` を保持するか同等の 16 バイト レイアウトとするか、ネイティブ `struct timespec` との変換関数の配置を検討します。

## BL-003 ローカル時刻型の詳細設計、名前衝突解消および UTC 変換境界の整理

### 概要と問題点

- **現状の事象**: アプリケーション利用者がローカル時刻の年月日時分秒、UTC オフセット、夏時間フラグ、タイム ゾーン名を扱おうとすると、C 標準の `struct tm` (年が 1900 引き、月が 0 始まり、Windows には `tm_gmtoff` がない) とプラットフォーム固有 API を直接組み合わせる必要があります。
- **検討中の理想像**: UTC 時刻型とは分離したローカル特化型 (`cplat_local_time` または `cplat_localtime`) を設け、実行環境のローカル タイム ゾーンにおける分解日時・UTC オフセット・夏時間フラグ (`is_dst`)・表示用タイム ゾーン名を OS 非依存の形式で扱えるようにする案です。

### 経緯と見送り理由

- **摘出契機**: UTC 時刻型にタイム ゾーン情報を混ぜない前提のもと、アプリケーション利用者にとって価値が高いローカル属性を標準機能の範囲で安全に提供する案を検討しました。
- **今回の見送り理由**: 導入の可否に加え、命名規則 (既存関数 `cplat_localtime()` との衝突解消)、オフセットの精度単位、タイム ゾーン名の扱い、壁時計日時からの構築契約を後日精査するため残課題とします。

### 影響範囲とリスク

- **影響対象**: `app/cplat/prod/include/cplat/clock/`、`app/cplat/prod/include/cplat/crt/time.h`、`app/cplat/prod/libsrc/cplat/clock/`、`app/cplat/prod/libsrc/cplat/crt/time.c`、`app/cplat/docs/functional-spec/clock.md`。
- **放置した場合のリスク**: 利用側が `struct tm` や `tm_gmtoff` の OS 差を個別に吸収するコードが重複し、夏時間や時差計算のバグが散在します。

### 推奨される対応策 (次回精査項目)

- **命名と既存関数 `cplat_localtime()` の衝突解消**:
    - **案 A (アンダースコア統一)**: `cplat_utc_time`、`cplat_local_time`、`cplat_monotonic_time`、`cplat_timespan` (または `cplat_time_span`) とし、既存関数 `cplat_localtime()` との衝突を自然に回避する案。
    - **案 B (連結語優先)**: `cplat_utctime`、`cplat_localtime` を型名とし、既存の CRT ラッパー関数 `cplat_localtime()` / `cplat_gmtime()` は `cplat_crt_localtime()` 等へ改名するか、新設型で置き換えて内部化・廃止する案。
- **標準機能で取得するメンバーと制約の検討**:
    - 年 (西暦そのまま)、月 (1..12)、日 (1..31)、時・分・秒・ナノ秒、UTC オフセット (`utc_offset_seconds` または `utc_offset_minutes`)、`is_dst` (有効 / 無効 / 不明の 3 値) を基本メンバーとする案。
    - タイム ゾーン名は Linux (POSIX 略称 `"JST"` 等) と Windows (キー名 `"Tokyo Standard Time"` やロケール依存名) で形式が異なるため、比較や永続化には使わず参考表示用と位置づける案。
- **UTC 時刻型への逆変換と壁時計構築の責務分離**:
    - **オフセット保持済みのローカル時刻型 → UTC 時刻型**: ローカル時刻型自身が確定した UTC オフセットを保持している場合、`mktime()` に頼らず純粋な算術 (`civil_to_epoch - utc_offset`) で 100% 決定論的に UTC 時刻型へ逆変換できます。秋の夏時間終了時の重複時刻でも、オフセットの違いにより一意に元へ戻ります。歴史的タイム ゾーンの秒単位オフセットまで厳密に往復させたい場合は `utc_offset_seconds` の採用を検討します。
    - **オフセット未指定の壁時計日時 → ローカル時刻型 (構築時)**: ユーザー入力などオフセットを持たない年月日時分秒からローカル時刻型を構築する関数を設ける場合、夏時間終了時の曖昧時刻を解決する引数 (例: `ambiguous` / Python PEP 495 の `fold` 相当) を設け、`0` (既定) は最初の時刻、`1` は 2 回目の時刻を選択する案が挙がっています。実装面では `tm_isdst = 1` と `tm_isdst = 0` でそれぞれ `mktime()` を呼び出し、`localtime_r` / `localtime_s` で逆検算することで、タイム ゾーン DB なしでも「有効な解が 2 つ (曖昧時刻)」「有効な解が 1 つ (通常時刻)」「有効な解が 0 つ (春の夏時間開始による存在しない時刻)」を両 OS 共通に判別できる見込みです。次回精査では、C 言語での省略表現 (既定関数と明示指定関数の分離か `enum` 引数か)、通常時刻で `1` が渡された場合の扱い、存在しない時刻の扱い (エラー返却か丸めか) を検討します。
    - **手動書き換えによる不整合の扱い**: C の構造体メンバーを利用者が直接書き換えた場合、算術的な UTC 変換自体は決定論的に成立しますが、実行環境のローカル ルールとの整合性は保証されない点を契約でどう示すか検討します。

## BL-004 既存 cplat_timespec と利用箇所の扱いおよび移行・検証計画の策定

### 概要と問題点

- **現状の事象**: `cplat_timespec` は `app/cplat` の複数モジュール (`clock`、`crt/file`、`hashtable`、`string_catalog`、`trace`、`bench-*`) に加えて、`app/porter` や `app/string-catalog-sample`、および `mock_cplat`・エクスポート テスト・単体テストで広く使用されています。
- **検討中の理想像**: 4 型体系を導入する判断となった場合に、既存 `cplat_timespec` を廃止して全面置換するか、低レイヤー互換型として残して段階移行するかを明確にし、文書・モック・テストの同期漏れを防ぐ計画を整えることです。

### 経緯と見送り理由

- **摘出契機**: 4 型体系の初期検討に伴い、仮に実装へ進む場合に精査すべき移行・付随作業を洗い出しました。
- **今回の見送り理由**: BL-001 から BL-003 までの設計検討とあわせて、導入の是非と移行方式を後日判断します。

### 影響範囲とリスク

- **影響対象**: `app/cplat/docs/functional-spec/clock.md`、`app/cplat/docs/coding-guideline.md`、`app/cplat/docs/api-cheatsheet.md`、`app/cplat/test/src/libcplatTest/exportTest/exportTest.cc`、`app/cplat/test/include/mock_cplat.h`、`app/cplat/test/libsrc/mock_cplat/`、および `cplat_timespec` を参照する各 app。
- **放置した場合のリスク**: 新旧の時刻型が場当たり的に混在すると、かえって API の学習コストと変換コストが増大します。

### 推奨される対応策 (次回精査項目)

- **移行方針の検討**: 既存 `cplat_timespec` を全面置換するか、OS 境界の内部表現として残しつつ公開 API を 4 型へ統一するかを比較検討します。
- **付随更新のチェックリスト化**: 導入を決めた場合は、`app/cplat/AGENTS.md` の規則に従い、機能仕様、コーディング規範、API チート シート、エクスポート テスト、`mock_cplat`、Linux / Windows 双方の単体テストと Doxygen 生成を同一変更または計画的な段階変更に含めます。
