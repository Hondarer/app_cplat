<!--
生成元: agree-before-implement スキル
文書種別: 最終仕様 (design)
上流文書: `261012-cplat-prompt-completion-discussion.md` (検討用 Markdown、合意版: 版 11)。削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。上流文書が存在する場合は、上流文書の再開手順に従って進行位置を確認し、判断の経緯を上流文書の Q&A で確認します。存在しない場合は、この文書を単体の最終仕様として扱います。
-->

# プロンプトの入力補完 最終仕様

検討 ID: CPLAT-PROMPT-COMPLETION

## 概要

cplat のプロンプト機能 (`cplat_prompt` と `cplat_pinned_prompt`) に、入力中の語を補う機能を追加します。  
利用側が登録した補完関数から候補を受け取り、候補が 1 つに定まる場合だけ、入力の続きを灰色で表示します。利用者は Tab または行末での → で、灰色の部分を確定します。

あわせて、prompt に横スクロールを実装し、既存の要件 `CPLAT-PROMPT-FUNC-003` <!-- cplat-req: uuid=bdf48e47-ef70-4fdd-a765-fe08b46f22ac --> を満たすようにします。表示幅の計算は prompt と pinned_prompt で共通にし、プロンプト文字列の幅も表示幅の規則で数えるように直します。

## 利用者から見た動作

### 候補を求める契機

次の条件をすべて満たすとき、ライブラリは補完関数を呼び出します。

- 入力と出力が端末に接続されている。
- 補完関数が登録されている。
- 直前の操作が、文字の入力、後退削除 (Backspace)、前方削除 (Delete)、または灰色の候補の確定である。
- 操作の後、カーソルが行末にあり、入力が空でない。
- 入力の末尾が UTF-8 の文字の途中ではない。

履歴の呼び出し (↑↓)、初期値の設定、カーソルの移動 (←→、Home、End)、Esc による消去の直後は、補完関数を呼ばず、灰色の表示も消します。  
何文字目から候補を返すかは、補完関数が判定します。

> [!NOTE]
> 入力は 1 バイトずつ読み取るため、マルチバイト文字の入力中は、編集バッファーの末尾が文字の途中になる瞬間があります。その時点で補完関数を呼ぶと、壊れた UTF-8 を渡すことになるため、文字が完成したときにだけ呼びます。

### 灰色の表示

補完関数が受け付けた候補が 1 件だけで、候補が入力済みの部分より長い場合に限り、続きの部分を表示します。

- 表示は `ESC [ 90 m` (明るい黒) で始め、直後に `ESC [ 0 m` で戻します。
- 灰色の部分は端末の右端で切り詰めます。入力が右端に達している場合は表示しません。
- 候補が複数ある場合は、共通の先頭部分も含めて何も表示しません。
- 候補が入力済みの部分と同じ長さの場合は、続きがないため何も表示しません。

```text
> st|          (start、stop、status の 3 件のため表示なし)
> star|t       (候補 "start " が 1 件。"t " を灰色で表示)
```

pinned_prompt では、他のスレッドの出力や端末サイズの変更で入力欄を描き直すときも、灰色の表示を保ちます。  
固定領域 (ステータス行とセパレーター) は初期表示、リサイズ、ステータスの変更や有効・無効の切り替え、および消去後の再表示のときだけ描画し、入力行の編集、カーソルの移動、補完の更新だけではセパレーターを再描画しません。  
入力待ちのタイムアウトやリサイズ通知を受けたときは端末のサイズを確認し、幅と高さのどちらも変化していなければ描画しません。  
幅または高さが変化すれば、表示範囲の調整と固定領域の再描画を行います。

### 確定と取り消し

Table: 灰色の表示とキー操作

| 操作 | 灰色の表示がある場合 | 灰色の表示がない場合 |
|---|---|---|
| Tab | 灰色の部分を入力欄へ入れ、カーソルを行末へ移す | 何もしない |
| → (行末) | Tab と同じ | 何もしない (現状と同じ) |
| Enter | 灰色の部分を含めず、入力済みの部分だけを確定する | 現状と同じ |
| その他の編集とカーソル移動 | 灰色の表示を消し、通常の処理を行う | 現状と同じ |

確定した後は、確定によって文字が入力されたものとして、補完関数を再び呼びます。候補に区切り (`src/` など) を含めると、Tab を続けて押すことで 1 段ずつ補完できます。  
ライブラリは確定時に空白を自動で付けません。次の語の入力に移らせたい場合は、補完関数が候補の末尾に空白を含めます。

Shift+Tab は対象外とし、現状どおり無視します。

### 端末でない場合

入力または出力が端末に接続されていない場合は、補完関数を呼びません。入力中の Tab は、通常の 1 文字として読み取ります。

### 横スクロール (prompt)

prompt でも、入力が端末の表示幅を超えた場合は、カーソル位置が見える範囲を横にずらして表示します。表示範囲の調整は pinned_prompt と同じ規則で行います。  
端末の幅は、再描画のたびに取得します。

### プロンプト文字列の幅

prompt と pinned_prompt は、プロンプト文字列の幅を表示幅の規則で数えます。全角文字は 2 桁、文字色の指定 (ANSI CSI SGR) は 0 桁として扱います。  
これにより、日本語や文字色の指定を含むプロンプト文字列でも、入力欄の幅とカーソル位置が正しくなります。ASCII だけのプロンプト文字列では、動作は変わりません。

## 公開 API

### 宣言

```{.c caption="app/cplat/prod/include/cplat/prompt/prompt.h"}
/* 補完関数へ渡す候補の受け取り先です。補完関数の呼び出し中だけ有効です。 */
typedef struct cplat_prompt_completions cplat_prompt_completions;

/* 補完関数の型です。 */
typedef void (*cplat_prompt_completion_fn)(const char *line, size_t cursor,
                                           cplat_prompt_completions *completions, void *user_data);

CPLAT_EXPORT int CPLAT_API cplat_prompt_completions_add(cplat_prompt_completions *completions,
                                                        size_t start, const char *candidate);

CPLAT_EXPORT int CPLAT_API cplat_prompt_completion_set(cplat_prompt *prompt,
                                                       cplat_prompt_completion_fn fn, void *user_data);
```

```{.c caption="app/cplat/prod/include/cplat/prompt/pinned_prompt.h"}
CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_completion_set(cplat_pinned_prompt *screen,
                                                              cplat_prompt_completion_fn fn, void *user_data);
```

既存の関数、型、構造体 (`cplat_prompt_options` を含む) は変更しません。ハンドルは中身を公開していない型のため、内部の状態を追加してもバイナリ互換性は保たれます。

### 補完関数 (cplat_prompt_completion_fn)

Table: 補完関数の引数

| 引数 | 内容 |
|---|---|
| `line` | 入力欄の内容です。NUL 終端の UTF-8 文字列です。 |
| `cursor` | カーソルのバイト位置です。呼び出し条件により、常に `line` の長さと等しくなります。 |
| `completions` | 候補の受け取り先です。`cplat_prompt_completions_add` に渡します。 |
| `user_data` | 登録時に指定した値です。 |

補完関数は戻り値を持ちません。候補がない場合は、何も追加せずに戻ります。

> [!IMPORTANT]
> pinned_prompt の補完関数は、ハンドル内の mutex を持ったまま呼ばれます。補完関数から pinned_prompt の API を呼んではなりません。Windows の mutex は再取得できないため、呼ぶと停止します。  
> また、補完関数の処理時間はキー入力のたびに他のスレッドの出力を待たせるため、短時間で終わる処理に限ります。

> [!NOTE]
> `cursor` は常に `line` の長さと等しくなりますが、将来カーソル位置の条件を広げても型を変えずに済むよう、引数に残しています。

### cplat_prompt_completions_add

置換開始位置 `start` と候補 `candidate` を 1 件追加します。確定すると、入力欄の `start` からカーソルまでを `candidate` で置き換えます。

Table: cplat_prompt_completions_add の戻り値

| 戻り値 | 条件 |
|---|---|
| `CPLAT_OK` | 候補を受け付けた場合。同じ置換開始位置と同じ文字列の候補がすでにある場合も、1 件として数えて `CPLAT_OK` を返します。 |
| `CPLAT_ERR_INVALID_ARGUMENT` | `completions` または `candidate` が NULL の場合、`start` がカーソルより後ろか UTF-8 の文字の途中にある場合、`candidate` に制御文字 (0x00 から 0x1F、および 0x7F) が含まれる場合、`candidate` の先頭が入力済みの部分 (`start` からカーソルまで) と一致しない場合。 |
| `CPLAT_ERR_BUFFER_TOO_SMALL` | 置き換えた結果が、NUL 終端を含めて入力欄の上限 (`input_max_bytes`) を超える場合。 |
| `CPLAT_ERR_OUT_OF_MEMORY` | 1 件目の候補を保持する領域を確保できない場合。 |

`CPLAT_OK` 以外を返した候補は追加せず、件数にも数えません。

> [!NOTE]
> 制御文字の判定は、入力欄の初期値の検証と同じ規則です。上限を超える候補を追加の時点で除外するのは、表示した候補が確定できない状態を作らないためです。

### cplat_prompt_completion_set と cplat_pinned_prompt_completion_set

補完関数を登録します。`fn` に NULL を指定すると登録を解除し、Tab は現状どおり無視されます。

Table: 補完関数の登録関数の戻り値

| 戻り値 | 条件 |
|---|---|
| `CPLAT_OK` | 登録または解除した場合。 |
| `CPLAT_ERR_INVALID_ARGUMENT` | ハンドルが NULL の場合。 |

登録はハンドル単位です。readline の呼び出しごとに補完を切り替える場合は、readline の前に登録し直します。  
`cplat_pinned_prompt_completion_set` は、ハンドル内の mutex を取ってから登録を変更するため、別のスレッドが readline を実行している間でも呼べます。`cplat_prompt_completion_set` は、prompt の他の関数と同じく、複数のスレッドからの同時呼び出しを想定しません。

## 内部設計

### 共通処理 (prompt_edit)

次の処理を `pinned_prompt.c` の static 関数から `prompt_edit.c` へ移し、`prompt_edit.h` で宣言して prompt と pinned_prompt の両方から使います。pinned_prompt の動作は、プロンプト文字列の幅の数え方を除いて変えません。

Table: prompt_edit へ移す処理

| 現在の関数 (`pinned_prompt.c`) | 移動後の内部関数 |
|---|---|
| `utf8_char_display_width` | `cplat_internal_prompt_edit_char_display_width` |
| `ansi_sgr_sequence_len` | `cplat_internal_prompt_edit_sgr_sequence_len` |
| `pinned_prompt_visible_bytes_from` | `cplat_internal_prompt_edit_visible_bytes_from` |
| `pinned_prompt_display_width_between` | `cplat_internal_prompt_edit_display_width_between` |
| `pinned_prompt_adjust_view` の計算部分 | `cplat_internal_prompt_edit_adjust_view_start` (編集バッファー、カーソル、表示開始位置、入力欄の幅を受け取り、新しい表示開始位置を返す) |

あわせて、次の処理を `prompt_edit.c` へ追加します。

- 文字列全体の表示幅を求める関数 (プロンプト文字列の幅の計算に使用)
- `cplat_prompt_completions` の実体の定義 (`prompt_edit.h`) と、初期化、解放、灰色の部分の算出を行う内部関数
- 公開関数 `cplat_prompt_completions_add` の実装

`cplat_prompt_completions` は、補完関数へ渡す入力欄の内容とカーソル位置、入力欄の上限、受け付けた候補の件数、1 件目の候補 (置換開始位置と文字列の複製) を保持します。2 件目以降は、1 件目と同じかどうかだけを判定し、文字列を保持しません。  
1 件目の候補の文字列は、ハンドルが持つ領域へ複製します。領域は入力欄の上限を超えない範囲で拡張し、ハンドルの破棄時に解放します。

### prompt (prompt.c、prompt_linux.c、prompt_windows.c)

- `cplat_prompt` の内部構造体へ、補完関数と `user_data`、灰色の候補の状態、表示開始位置、端末の幅を追加します。
- `read_key()` に Tab (0x09) を表す内部キーを追加します。
- 再描画処理を、端末の幅を取得したうえで、表示開始位置の調整、プロンプト文字列、入力の見える範囲、灰色の部分の順に出力する形へ変えます。カーソルは行頭からの桁数で位置を決めます。
- 端末の幅を取得する内部関数を `prompt_internal.h` に宣言し、`prompt_linux.c` (`ioctl` の `TIOCGWINSZ`) と `prompt_windows.c` (`GetConsoleScreenBufferInfo`) に実装します。取得できない場合は 80 桁とします。

### pinned_prompt (pinned_prompt.c)

- `cplat_pinned_prompt` の内部構造体へ、補完関数と `user_data`、灰色の候補の状態を追加します。
- `pinned_prompt_read_key()` に Tab を表す内部キーを追加します。
- 補完関数は、readline のキー処理の中で、mutex を持ったまま呼びます。
- 入力欄の描画で、入力の見える範囲の後ろへ灰色の部分を出力します。
- プロンプト文字列の幅を、バイト数 (`strlen`) ではなく、共通の表示幅の計算で求めます。
- `pinned_prompt_render_locked` は、固定領域と入力行の描画を分け、`status_dirty` のときのみ固定領域の消去 (`pinned_prompt_clear_control_area`)、ステータス行、およびセパレーターを描画します。入力行は別に描画するため、入力行の編集、カーソルの移動、補完の更新や登録だけではセパレーターを再描画しません。
- `status_dirty` は固定領域の描画後に、小さい端末で非表示のステータス行があっても `0` にします。`pinned_prompt_update_size` は端末の幅または高さの変更時に `status_dirty = 1` を設定するため、端末が大きくなってステータス行が表示可能になれば描画されます。`pinned_prompt_cleanup_terminal_locked` で固定領域を消去した場合も `status_dirty = 1` を設定し、再表示可能とします。ステータスの変更 (`cplat_pinned_prompt_status_set`) や有効・無効の切り替え (`cplat_pinned_prompt_status_enable`)、初期表示における既存の `status_dirty = 1` 設定は維持します。
- 入力待ちのタイムアウトやリサイズ通知 (`PINNED_PROMPT_KEY_RESIZE`) で呼ばれる `pinned_prompt_resize_locked` は、以前の幅と高さを保存して `pinned_prompt_update_size` で端末サイズを取得し、幅または高さのいずれかが変化した場合に限り `status_dirty` を設定して表示範囲の調整と再描画を行います。幅と高さのどちらも変化しなければ描画しません。
- 端末サイズの取得処理 (`pinned_prompt_platform_get_size`) は変更しません。

## 機能仕様と文書の改定

### 機能仕様 (app/cplat/docs/functional-spec/prompt.md)

「機能要件」に次の 1 行を追加します。

```text
| `CPLAT-PROMPT-FUNC-010` <!-- cplat-req: uuid=fd51697b-913f-4763-b3aa-b2e5f95e9de0 --> | cplat のプロンプト機能は、入力中の語に対して利用側が示す候補が 1 つに定まる場合、その続きを入力内容と区別して表示し、利用者の操作で入力欄へ補います。 |
```

「入力候補の補完」の節を追加し、候補を求める契機、続きの表示、確定の操作、Enter で表示中の続きを含めないこと、端末でない場合は補完しないことを、関数名を用いずに記載します。  
「適用範囲外」の「入力候補の補完」を「入力候補の一覧表示」に置き換えます。

### その他の文書

Table: 補完に伴って改定する文書

| 文書 | 改定内容 |
|---|---|
| `app/cplat/prod/libsrc/cplat/prompt/README.md` | 「初期対象範囲」に入力候補の補完を追加し、「初期対象外」の「補完、候補表示」を「候補の一覧表示」に置き換えます。「手動評価シナリオ」に補完の確認手順を追加します。 |
| `app/cplat/docs/api-cheatsheet.md` | prompt 節の表に、「入力中の語を補完する (候補が 1 つに定まると灰色で表示し、Tab または → で確定する)」の行を追加し、3 つの公開関数を記載します。 |
| `app/cplat/test/src/libcplatTest/prompt/TESTING.md` | テスト数、カバレッジの計測結果、テスト構成の説明を更新します。 |

公開ヘッダーの Doxygen コメントには、補完関数の呼び出し条件、mutex に関する制約、`cplat_prompt_completions_add` の戻り値の条件を記載します。

## サンプル コマンド

`app/cplat/prod/src/cmd/pinned-prompt/pinned-prompt.c` に、1 語目の命令名を補う補完関数を登録します。  
候補は `help`、`echo`、`start`、`stop`、`status`、`read`、`exit`、`quit` とし、それぞれ末尾に空白を付けて返します。2 語目以降は補完しません。

```text
> st|          (start、stop、status のため表示なし)
> sto|p        Tab → > stop |
```

## テスト

Table: 追加・変更するテスト

| テスト | 追加・変更する観点 |
|---|---|
| `promptEditTest` | 移した表示幅の計算と表示範囲の調整、文字列全体の表示幅、`cplat_prompt_completions_add` の各戻り値、重複の扱い、灰色の部分の算出 |
| `promptTest` | Tab と → での確定、Enter で灰色の部分を含めないこと、確定直後の再呼び出し、呼び出し条件 (行末、空入力、マルチバイト文字の途中、履歴と初期値の直後)、候補の件数による表示の有無、登録の解除、端末でない場合、横スクロール、プロンプト文字列の表示幅 |
| `promptAllocFailureTest` | 1 件目の候補の複製でのメモリ確保失敗 |
| `promptLinuxTest`、`promptWindowsTest` | 端末の幅の取得と、取得できない場合の既定値 |
| `pinnedPromptTest` | Tab と → での確定、灰色の表示の描画と右端での切り詰め、他の出力やサイズ変更後の再描画、サイズ不変時の再描画抑止、文字入力やカーソル移動でセパレーターを再描画しないことと再表示条件 (初期表示、ステータス変更、極小端末での繰り返し描画抑止、消去後の再表示、描画時の幅変更検出) の確認、プロンプト文字列の表示幅、登録関数 |
| `exportTest` | `CPLAT_EXPORT_TABLE_COMMON` への 3 関数の追加 |

`mock_cplat` には、3 つの公開関数の mock を追加し、実 libcplat へ既定で委譲します。

## 受入条件

- `prompt_edit.c`、`prompt.c`、`pinned_prompt.c` の行カバレッジと条件網羅 (C2) が、WSL (Linux) の gcov / gcovr で 100% であること。`pinned_prompt.c` の固定 FD 呼び出し行 (`CPLAT_FD_SET(STDIN_FILENO, &fds)`) には `GCOVR_EXCL_BR_WITHOUT_HIT: 2/4` を付与し、行自体は除外しません。gcovr 8.6 は未到達分岐の本数の一致を検証した後にその行の全 4 分岐を集計から除外するため、受入判定の C2 カバレッジには除外後の集計値を用い、除外前の計測値と到達不能の理由もあわせて記録します。
- 既存のテストを含め、prompt 系と exportTest のテストがすべて成功すること。
- 対象範囲の `.warn` ファイルに、今回の変更に起因する警告がないこと。
- 変更した Markdown が `text_style_jp.py --dry-run` で指摘なしであること。
- サンプル コマンド `pinned-prompt` で、README の手動評価シナリオに追加した補完の確認手順を実施できること。

`make` と `make test` は、ユーザーの許可がある場合に AI も実行できます。  
端末での手動確認は、実施した環境を明記して記録します。

## 作業後に記録する残課題

pinned_prompt の端末サイズの取得処理は prompt 側と重複したまま残すため、作業の完了時に `261012-cplat-prompt-completion-backlog.md` へ記録します。

> [!NOTE]
> pinned_prompt は、raw モード、入力の読み取り、端末の判定などの OS ごとの処理を自前でまとめて持っています。端末サイズの取得だけを共通にすると構成が不ぞろいになるため、OS ごとの処理の共通化は別の整理として扱います。
