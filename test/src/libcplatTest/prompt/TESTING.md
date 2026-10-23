# prompt 系テスト

`app/cplat/prod/libsrc/cplat/prompt/` の Linux および Windows 実装を対象とするテスト構成と、Linux の gcov によるカバレッジ結果を記載します。

## カバレッジ基準

カバレッジは条件網羅 (C2) を基準とし、WSL (Linux、GCC 8 / gcov、gcovr 8.6) の計測結果を C2 カバレッジの正本として扱います。  
Windows のカバレッジ計測は行カバレッジのみを対象とします。

テスト対象ソースとテスト数、および WSL (Linux) における実測結果は次のとおりです。  
`prompt_linux.c` は 2026-10-22 の計測値です。  
その他は、絵文字の文字化け対策を反映した 2026-10-24 の計測値です。

| 対象ソース | テスト ディレクトリ | テスト数 | 行カバレッジ | C2 カバレッジ |
|---|---|---:|---:|---:|
| `prompt_edit.c` | `promptEditTest` | 63 | 100% (278/278) | 100% (230/230) |
| `prompt_linux.c` | `promptLinuxTest` | 15 | 100% (53/53) | 93.33% (28/30) |
| `prompt.c` | `promptTest` | 72 | 100% (391/391) | 100% (202/202) |
| `pinned_prompt.c` | `pinnedPromptTest` | 78 | 100% (772/772) | 100% (414/414) |
| `prompt_windows.c` | `promptWindowsTest` | 18 | Linux では計測対象外 | Linux では計測対象外 |

Table: prompt 系テストのカバレッジ計測結果

Windows では `promptEditTest` 63 件、`promptTest` 72 件、`pinnedPromptTest` 47 件、`promptWindowsTest` 18 件が成功しました。  
固定プロンプト (`pinned_prompt.c`) の最新の除外適用前の C2 カバレッジは 416/418 です。  
未到達の 2 分岐は、既存の `CPLAT_FD_SET(STDIN_FILENO, &fds)` 内の `fd >= 0` と `fd < FD_SETSIZE` の偽の分岐であり、`STDIN_FILENO` が `0` で固定されているため到達不能です。  
この呼び出し行には `GCOVR_EXCL_BR_WITHOUT_HIT: 2/4` を付与しています。  
現行の gcovr 8.6 は未到達分岐の本数が `2/4` と一致することを検証した後にその行の全 4 分岐を集計から除外するため、行カバレッジ 100% (772/772) を維持したまま、除外後の C2 カバレッジは 100% (414/414) となります。

### 日本語と絵文字の入力を文字単位で描画

UTF-8 の入力途中では画面へ出力せず、カーソル直前の文字が完成してから描画します。  
`promptTest` では、1 バイトずつ「テスト」を入力し、描画された文字列が「テ」「テス」「テスト」の 3 回だけであることを確認します。  
`pinnedPromptResizeTest` では 6 種類の端末サイズで、文字の先頭 1～2 バイトでは出力せず、完成後に描画することと、行途中への挿入でも同じ規則になることを確認します。

Windows のコンソール入力は `ReadConsoleW` で UTF-16 のコード単位として読み取り、サロゲート ペアを結合してから UTF-8 へ変換します。  
未取得の UTF-8 バイトと、不正なペアの後続コード単位を内部状態に保持し、入力イベントを待ち直さずに処理します。  
通常・固定の双方でサロゲート ペアの読み取りと後続バイトの取得を検証し、通常プロンプトでは、タイムアウト後も上位サロゲートを保持することを確認します。  
`promptWindowsTest` は、この内部共通処理を実 `prompt_edit.c` の `ADD_SRCS` と `mock_cplat` でリンクします。

Windows の疑似端末 (ConPTY) では、修正前に絵文字が置換文字になる事象を再現しました。  
修正後は `echo TEST😀A🚀🎉` が入力中と Enter 後の双方で正しく表示され、左矢印の後に Backspace を押すと `🚀` だけが削除されました。  
絵文字の表示幅は Unicode 17.0 の EastAsianWidth の W 範囲に基づいて 2 桁とし、この入力の末尾におけるカーソルが行頭から 31 桁進んだ位置にあることも確認しました。  
既存の `echo TESTテスト` も入力中と Enter 後の双方で正常に表示されました。

## テスト構成

### promptEditTest

`prompt_edit.c` の文字境界、バッファー容量、オプション解決に加え、`prompt.c` と `pinned_prompt.c` で共通化した表示幅計算、横スクロールの表示開始位置調整、補完状態と候補検証、候補の確定処理をテストします。

`promptUtf16Test` は 18 条件で UTF-16 と UTF-8 の境界値、サロゲート ペア、不正なペアの後続保持、未完了の上位サロゲートの保持を検証します。  
`promptEmojiWidthTest` は 6 条件で補助平面の絵文字と幅が狭い記号の表示幅を検証します。

`realloc` の失敗、NULL 引数、容量上限、UTF-8 の継続バイトを含む境界条件と、入力欄の初期値の検証 (制御文字の拒否、上限ちょうどと超過、上限 0) を扱います。  
表示幅と表示範囲では、UTF-8 文字幅、ANSI SGR シーケンスの読み飛ばし、表示幅に収まるバイト数、指定範囲および文字列全体の表示幅、カーソル位置に応じた表示開始位置の調整を検証します。  
入力補完では、末尾の UTF-8 文字の完成判定と補完関数の呼び出し条件、`cplat_prompt_completions_add()` の候補検証 (NULL 引数、置換開始位置の境界と UTF-8 文字途中、制御文字、先頭不一致、短い候補、置換後の上限超過、重複候補の統合、複数候補や同長候補での続き非表示)、候補保持領域と確定時の編集バッファー拡張におけるメモリ確保失敗、および候補全体の確定処理を検証します。

### promptTest

`prompt.c` が呼び出す端末制御関数を `promptPlatformFake.cc` で差し替え、実端末を使わずに入力処理をテストします。  
`prompt.c` と `pinned_prompt.c` が共有する編集・表示幅・補完処理は、実 `prompt_edit.c` を `ADD_SRCS` で取り込み、`stub_cplat` は使用しません。  
テスト ディレクトリ内の `promptPlatformFake.cc` は自動収集されるため、`makepart.mk` の `ADD_SRCS` には指定しません。

```makefile
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt.c

ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt_edit.c
```

`promptFakeSetInput()` に入力バイト列を渡し、`cplat_prompt_readline_at()` で行編集、履歴、UTF-8、エスケープ シーケンス、EOF を確認します。  
`cplat_prompt_readline_with_initial_at()` では、初期値の確定と編集、履歴から初期値へ戻る操作、初期値の拒否で入力を消費しないこと、非 TTY で初期値を使わないことを確認します。

```cpp
promptFakeSetInput("abc\r");              // "abc" と Enter
promptFakeSetInput("abc\x1B[D\x1B[3~\r"); // "abc"、左矢印、Delete、Enter
```

入力補完では、`cplat_prompt_completion_set()` による補完関数の登録と解除、補完トリガー条件 (文字挿入・後退削除・前方削除・候補確定の直後、行末、非空)、UTF-8 マルチバイト文字の入力途中では補完関数を呼び出さないこと、履歴の呼び出しや初期値設定の直後は候補を求めず表示中の候補も消去することを確認します。  
あわせて、Tab と行末での右矢印による候補の確定と直後の再補完、Shift+Tab の無視、Enter 確定時に灰色の続きを含めないこと、複数候補がある場合に共通部分を含めて補完しないこと、入出力のいずれかが非端末の場合に補完を行わず Tab を通常文字として読み取ることを確認します。

表示幅と横スクロールでは、`promptFakeSetColumns()` で端末幅を指定し、日本語や ANSI SGR を含むプロンプト文字列の表示幅計算、端末幅を超える入力での横スクロール、灰色の続きの右端での切り詰めと切り詰められた候補全体の確定を確認します。  
また、横スクロール後に履歴を切り替えて短い入力や編集中の入力へ戻ったときに、横スクロールの表示開始位置 (`view_start`) が先頭へリセットされることも確認します。

`prompt.c` の非 TTY 経路を試験するときは、ハンドル生成後に内部状態 `is_tty` を 1 以外へ設定します。

```cpp
prompt_ = cplat_prompt_create(NULL);
prompt_->is_tty = 0;
```

`promptAllocFailureTest` では、ハンドル、編集バッファー、履歴、コンテキスト、書式付き入力、初期値付き入力、および 1 件目の補完候補の保持領域におけるメモリ確保失敗を注入します。  
共有編集処理は `stub_cplat` ではなく `ADD_SRCS` で取り込んだ実 `prompt_edit.c` を使用し、編集バッファーや補完候補領域の拡張は `cplat_realloc` を呼び出します。  
そのため、これらの領域拡張の失敗は `Mock_cplat` の `cplat_realloc` で注入できます。

### promptLinuxTest

`prompt_linux.c` の標準入力をパイプまたは疑似端末へ差し替え、raw モード、SIGWINCH、blocking 読み取り、non-blocking 読み取り、および出力端末の幅の取得をテストします。

`Mock_unistd`、`Mock_termios`、`Mock_signal`、`Mock_sys_select`、`Mock_ioctl` を使い、`read` の EINTR、EOF、`tcsetattr` の失敗、`select` のタイムアウト、`ioctl` (`TIOCGWINSZ`) による端末幅の取得と失敗時・幅 0 時の既定値 (80 桁) へのフォールバックを再現します。

ファイル内 static 状態の `s_resize_pending` と `s_sigwinch_installed` は、`prompt_linux.inject.c` のアクセサーから操作します。

### pinnedPromptTest

`pinned_prompt.c` の static 関数は `pinned_prompt.inject.c` のアクセサーから直接呼び出し、画面制御と Linux プラットフォーム処理を分離してテストします。  
共有する表示幅計算、横スクロールの表示開始位置調整、補完候補の検証と確定処理は実 `prompt_edit.c` を `ADD_SRCS` で取り込み (`stub_cplat` は使用しません)、単体のテストは `promptEditTest` で実施します。

```makefile
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/pinned_prompt.c

ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt_edit.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c
```

テスト対象は次のとおりです。

- 非 TTY の `cplat_fgets` fallback、EOF、バッファー不足
- status API の上下位置、左右配置、NULL 引数
- `ioctl` による端末サイズ取得と既定値 fallback
- raw モードの移行と復帰、SIGWINCH ハンドラーの登録
- 入力の EINTR、リサイズ通知、通常読み取り、EOF
- `select` のタイムアウトと入力可能状態
- 初期値付き readline の確定、編集、履歴から初期値へ戻る操作、初期値の拒否、非 TTY の fallback、編集バッファー拡張の失敗
- `cplat_pinned_prompt_completion_set()` による補完関数の登録・解除と NULL 引数の拒否、補完トリガー条件 (文字挿入・後退削除・前方削除・候補確定、行末、UTF-8 文字の完成、空入力やカーソル移動での非呼び出し)、Tab と行末右矢印による確定と再補完、Enter 確定時の候補除外
- 日本語や ANSI SGR を含むプロンプト文字列の表示幅、灰色の補完候補の描画と端末右端での切り詰め、別出力や端末サイズ変更後の再描画
- 専用 inject アクセサー (`test_pinned_prompt_resize`) とパラメーター テスト `pinnedPromptResizeTest` (既存のサイズ変化テスト `redraws_only_when_dimensions_change` と新しい `typing_preserves_fixed_area` の 2 種類 × `80x24`、`120x24`、`80x40`、`120x40`、および低い端末条件 `80x2`、`80x1` の 6 条件 = 12 件) によるサイズ不変時の再描画抑止と固定領域無効化の検証 (サイズ変化テストでは初期サイズ 80x24 に対して同サイズの 80x24 なら 0 回、幅または高さが変わる残り 5 条件なら 1 回だけ描画され、その後同サイズで再確認しても描画が増えないことを `fflush` 呼び出し回数で確認し、`typing_preserves_fixed_area` では文字入力やカーソル移動時に `pinned_prompt_render_separator` の `printf` 呼び出しが増えないこと、ステータス変更後には描画されること、極小端末 `80x2`、`80x1` で繰り返し描画しないこと、消去後の再表示と `pinned_prompt_render_locked` での幅変更検出時に再描画されることを確認)

Linux では `Mock_ioctl`、`Mock_signal`、`Mock_termios`、`Mock_unistd`、`Mock_sys_select` を使用します。  
UTF-8 の描画と Windows のサロゲート ペア読み取りの検証を含め、Windows 47 件、WSL 78 件が成功しました。

### promptWindowsTest

`prompt_windows.c` の Win32 API を `Mock_windows` で差し替え、コンソール取得、モード変更、入力待機、読み取り、タイムアウト、`GetConsoleScreenBufferInfo` による端末幅の取得と失敗時・不正座標時の既定値 (80 桁) へのフォールバック、および API 失敗をテストします。

Windows 専用実装のため、Windows で 18 件のテストを実行し、Linux で実行した場合はテスト数とカバレッジを計測しません。

## 計測方法

prompt 系テストをまとめて実行する場合は、次のコマンドを使用します。

```bash
cd app/cplat/test/src/libcplatTest/prompt
make test
```

`promptEditTest` と `promptLinuxTest` の Linux カバレッジ計測値は、各テスト ディレクトリ下の `results/all_tests/coverage.xml` (および `summary.md`) に出力されます。  
`promptTest` の最終 71 件はテストを一括実行し、testfw と同じ gcovr 引数 (`--exclude-unreachable-branches --exclude-throw-branches`) と `gcovr_json_normalize.py` を用いて再計測しました。  
一括再計測の証跡は `promptTest/results/final-batch/tests.xml` と `promptTest/results/final-batch/coverage.xml` に配置しています。  
`pinnedPromptTest` のちらつき対策前の 60 件の一括再検証の証跡は、`pinnedPromptTest/results/completion-final/tests.xml` と `pinnedPromptTest/results/completion-final/coverage.xml` に配置しています。  
無入力時のちらつき対策後 (セパレーターちらつき対策前) の WSL 64 件の証跡は、`pinnedPromptTest/results/flicker-final/tests.xml` と `pinnedPromptTest/results/flicker-final/coverage.xml` に配置しています。  
Windows 31 件の証跡は `pinnedPromptTest/results/windows-flicker-final/tests.xml` にあります。  
最新版は `promptEditTest`、`promptTest`、`pinnedPromptTest` の各 `results/emoji-final/` にある `tests.xml`、`coverage.xml` です。  
Windows の最新版は各テストの `results/windows-emoji-final/tests.xml` にあります。  
`pinned_prompt.c` のちらつき対策前・除外適用前の情報 (C2 カバレッジ 414/416) は、除外適用前の `pinnedPromptTest/results/all_tests/coverage.json` および本文書の記録値に基づきます。

テスト ディレクトリを改名した場合は、旧パスを含む `obj/` を削除してから再ビルドします。

`ADD_SRCS` に指定したソースは testfw の override ヘッダーによる API 差し替え対象にならないため、OS API mock を必要とする実装を `ADD_SRCS` で取り込まないようにします。
