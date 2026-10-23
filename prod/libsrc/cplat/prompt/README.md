---
short-title: "prompt"
---

# prompt - pinned_prompt

## 目的

`prompt` は対話的な入力編集と履歴を扱う共通入力 API です。  
`pinned_prompt` は、chat-style TUI / console chat interface に見られる「入力プロンプトを画面最下段に表示し、アプリケーション出力をその上に表示する」画面制御を `cplat` で扱うための試験的 API です。

`cplat_prompt` は 1 行入力と履歴を扱います。`pinned_prompt` はその上位の利用形態として、入力中のプロンプト再描画、専用出力 API、ステータス表示、リサイズ後の再描画をまとめます。

## 初期対象範囲

- 1 行のコマンド入力
- 呼び出し元単位の履歴
- 入力候補の補完
- UTF-8 文字境界を保ったカーソル移動、削除、履歴復元
- 画面幅を超える入力の横スクロール表示
- `stdout` / `stderr` を選べる専用出力 API
- 上部 / 下部ステータス表示
- 通常画面での固定プロンプト表示
- readline 開始時、キー入力時、専用出力時の端末サイズ確認
- 非 TTY での通常入出力フォールバック

## 初期対象外

- `printf` / `fprintf` の自動捕捉
- 候補の一覧表示
- 複数行入力の実装
- スクロール履歴ビュー
- 代替画面

## API 契約

公開ヘッダーは `cplat/prompt/pinned_prompt.h` です。

- `cplat_pinned_prompt_create(NULL)` は既定設定で pinned prompt ハンドルを作成します。
- `cplat_prompt_options` と `cplat_pinned_prompt_options.input` は、履歴数、入力バッファー初期容量、入力バッファー最大容量を指定します。
- `history_max == 0` は `CPLAT_PROMPT_HISTORY_DEFAULT` を使います。
- `input_max_bytes == 0` は `CPLAT_PROMPT_INPUT_BYTES_DEFAULT` を使います。
- `cplat_pinned_prompt_readline()` は Enter で `CPLAT_OK`、EOF で `CPLAT_ERR_EOF`、Ctrl+C で `CPLAT_ERR_CANCELED` を返します。
- フォールバック時に入力行が呼び出し側バッファーに収まらない場合は `CPLAT_ERR_BUFFER_TOO_SMALL` を返し、バッファーは空文字列になります。行の残りは次の呼び出しで取得できます。
- 履歴リングは `cplat_pinned_prompt_readline()` / `cplat_pinned_prompt_readline_fmt()` の呼び出し元ごとに分かれます。
- `cplat_pinned_prompt_write()` と `cplat_pinned_prompt_printf()` は、入力中でも呼び出せます。
- `cplat_pinned_prompt_write()` は渡されたバイト列だけを書き込み、改行を自動追加しません。
- API から出力する文字列では ANSI CSI SGR (`ESC [ ... m`) による着色を利用できます。
- ステータス行の配置計算では ANSI CSI SGR を表示幅 0 として扱います。
- 描画操作は pinned prompt ハンドル内の mutex で直列化します。
- 非 TTY では固定描画を行わず、入力は `cplat_fgets`、出力は `fwrite` 相当に戻ります。
- Windows では利用側が `activeCodePage=UTF-8` マニフェストを設定し、`cplat_console_init()` を呼ぶ前提です。ただし、呼び忘れても問題が起きにくいように create 時にも防御的に呼び出します。
- この API は試験的 API です。機能仕様の見直しに合わせて関数、型、挙動を変更する可能性があります。

## 複数行入力の設計メモ

複数行入力は `readline` の意味を変えず、別 API として追加する方針です。  
`readline` は「Enter で 1 行を確定する」契約を維持し、複数行 API は確定キー、改行キー、返却メモリの所有権を明示します。

候補となる宣言案:

```c
typedef struct cplat_prompt_input cplat_prompt_input;

int cplat_prompt_read_text_at(cplat_prompt *prompt,
                                 cplat_prompt_input *out,
                                 const char *prompt_str,
                                 const char *file,
                                 int line);
```

`cplat_prompt_input` は固定長 `char *` ではなく、入力本文、長さ、行数、確定理由を保持できる結果型にします。  
`pinned_prompt` では画面下部に複数行編集領域を予約できるよう、プロンプト行、編集行、ステータス行を同じ layout 計算で扱う必要があります。

## 手動評価シナリオ

初期実装では mock と自動テストを後段に回し、Prod ビルドと手動確認で仕様を評価します。

1. `cd app/cplat && make` で Prod 側がビルドできること。
2. 固定プロンプトが画面最下段に表示されること。
3. 入力中に専用出力 API で `stdout` / `stderr` へ出力しても、出力がプロンプト上部に表示され、入力中の内容が再描画されること。
4. 上下キーで pinned prompt 単位の履歴を移動できること。
5. 左右キー、Home、End、Backspace、Delete が UTF-8 文字境界を壊さないこと。
6. 画面幅を超える入力で横スクロールし、プロンプト行が 1 行に収まること。
7. リサイズ後、次のキー入力または専用出力で表示が再描画されること。
8. サンプル コマンド `pinned-prompt` で、末尾に空白を付けた 1 語目の命令名 `help`、`echo`、`start`、`stop`、`status`、`read`、`exit`、`quit` の補完が次のように動作すること。
    - 候補が複数の場合: `st` と入力した段階では、`start`、`stop`、`status` の 3 件が該当するため、灰色の続きは表示されません。
    - 候補が一意の場合: `sto` と入力すると候補が `stop ` の 1 件に定まり、続きの `p ` が灰色で表示されます。
    - Tab での確定: 灰色の続きが表示されている状態で Tab キーを押すと、末尾の空白を含めて `stop ` が入力欄へ補われ、カーソルが行末へ移ります。
    - 行末の右矢印での確定: `he` と入力して灰色の `lp ` が表示されている状態で、行末で右矢印キーを押すと Tab キーと同じく `help ` が確定してカーソルが行末へ移ります。
    - Enter での非確定: `sto` と入力して灰色の続きが表示されている状態で Enter キーを押すと、灰色の部分を含めずに入力済みの `sto` だけが確定します。
    - 2 語目の非補完: 1 語目の確定後など空白に続けて 2 語目を入力しても、候補は補完されません。
    - 他スレッドの出力: `start stdout` で別スレッドの定期出力を開始してから `sto` と入力すると、出力による入力欄の再描画後も灰色の表示が保たれます。
    - 端末サイズの変更: `sto` と入力して灰色の続きを表示した状態で端末サイズを変更すると、再描画後も灰色の表示が保たれます。
9. 終了時に raw mode が復元され、固定プロンプト行が消去されること。
10. パイプまたはリダイレクト時は通常入出力として動作することを確認してください。
