# prompt 系テスト

`app/cplat/prod/libsrc/cplat/prompt/` の Linux および Windows 実装を対象とするテスト構成と、Linux の gcov によるカバレッジ結果を記載します。

## カバレッジ基準

カバレッジは条件網羅 (C2) を基準とし、Linux の gcov 結果を計測値として扱います。

テスト対象ソースとテスト数、2026-09-26 の計測結果は次のとおりです。

| 対象ソース | テスト ディレクトリ | テスト数 | 行カバレッジ | C2 カバレッジ |
|---|---|---:|---:|---:|
| `prompt_edit.c` | `promptEditTest` | 28 | 100% (80/80) | 100% (68/68) |
| `prompt_linux.c` | `promptLinuxTest` | 14 | 100% (48/48) | 92% (24/26) |
| `prompt.c` | `promptTest` | 58 | 100% (345/345) | 100% (173/173) |
| `pinned_prompt.c` | `pinnedPromptTest` | 54 | 100% (807/807) | 100% (471/471) |
| `prompt_windows.c` | `promptWindowsTest` | 13 | Linux では計測対象外 | Linux では計測対象外 |

Table: prompt 系テストのカバレッジ計測結果

## テスト構成

### promptEditTest

`prompt_edit.c` の文字境界、バッファー容量、オプション解決をテストします。

`realloc` の失敗、NULL 引数、容量上限、UTF-8 の継続バイトを含む境界条件を扱います。  
入力欄の初期値の検証 (制御文字の拒否、上限ちょうどと超過、上限 0) も扱います。

### promptTest

`prompt.c` が呼び出す端末制御関数を `promptPlatformFake.cc` で差し替え、実端末を使わずに入力処理をテストします。

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

`prompt.c` の非 TTY 経路を試験するときは、ハンドル生成後に内部状態 `is_tty` を 1 以外へ設定します。

```cpp
prompt_ = cplat_prompt_create(NULL);
prompt_->is_tty = 0;
```

`promptAllocFailureTest` では、ハンドル、編集バッファー、履歴、コンテキスト、書式付き入力、初期値付き入力におけるメモリ確保失敗を注入します。  
行編集の内部 API は `stub_cplat` が提供し、編集バッファーの拡張は本番の `prompt_edit.c` と同じく `cplat_realloc` を使います。  
そのため、編集バッファーの拡張の失敗は `Mock_cplat` の `cplat_realloc` で注入できます。

### promptLinuxTest

`prompt_linux.c` の標準入力をパイプまたは疑似端末へ差し替え、raw モード、SIGWINCH、blocking 読み取り、non-blocking 読み取りをテストします。

`Mock_unistd`、`Mock_termios`、`Mock_signal`、`Mock_sys_select` を使い、`read` の EINTR、EOF、`tcsetattr` の失敗、`select` のタイムアウトを再現します。

ファイル内 static 状態の `s_resize_pending` と `s_sigwinch_installed` は、`prompt_linux.inject.c` のアクセサーから操作します。

### pinnedPromptTest

`pinned_prompt.c` の static 関数は `pinned_prompt.inject.c` のアクセサーから直接呼び出し、文字列処理と Linux プラットフォーム処理を分離してテストします。

```makefile
TEST_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/pinned_prompt.c

ADD_SRCS := \
	$(MYAPP_DIR)/prod/libsrc/cplat/prompt/prompt_edit.c \
	$(MYAPP_DIR)/prod/libsrc/cplat/base/result.c
```

テスト対象は次のとおりです。

- UTF-8 文字幅、ANSI SGR シーケンス、表示バイト数、表示幅
- 非 TTY の `cplat_fgets` fallback、EOF、バッファー不足
- status API の上下位置、左右配置、NULL 引数
- `ioctl` による端末サイズ取得と既定値 fallback
- raw モードの移行と復帰、SIGWINCH ハンドラーの登録
- 入力の EINTR、リサイズ通知、通常読み取り、EOF
- `select` のタイムアウトと入力可能状態
- 初期値付き readline の確定、編集、履歴から初期値へ戻る操作、初期値の拒否、非 TTY の fallback、編集バッファー拡張の失敗

Linux では `Mock_ioctl`、`Mock_signal`、`Mock_termios`、`Mock_unistd`、`Mock_sys_select` を使用します。

### promptWindowsTest

`prompt_windows.c` の Win32 API を `Mock_windows` で差し替え、コンソール取得、モード変更、入力待機、読み取り、タイムアウト、API 失敗をテストします。

Windows 専用実装のため、Linux で実行した場合はテスト数とカバレッジを計測しません。

## 計測方法

prompt 系テストをまとめて実行する場合は、次のコマンドを使用します。

```bash
cd app/cplat/test/src/libcplatTest/prompt
make test
```

各テストの詳細なカバレッジ結果は、テスト ディレクトリ下の `results/all_tests/summary.md` と `coverage.xml` に出力されます。

テスト ディレクトリを改名した場合は、旧パスを含む `obj/` を削除してから再ビルドします。

`ADD_SRCS` に指定したソースは testfw の override ヘッダーによる API 差し替え対象にならないため、OS API mock を必要とする実装を `ADD_SRCS` で取り込まないようにします。
