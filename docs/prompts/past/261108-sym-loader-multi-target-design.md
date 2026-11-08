<!--
生成元: agree-before-implement スキル
文書種別: 最終仕様 (design)
最終更新エージェント: grok / セッション ID: 01a1b8e0-a733-76f2-86a6-3e08d3489aff
上流文書: `261108-sym-loader-multi-target-discussion.md` (検討用 Markdown)。削除されている場合があります。
下流文書: `261108-sym-loader-multi-target-backlog.md` (残課題一覧)。未作成の場合や、削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。上流文書が存在する場合は、上流文書の再開手順に従って進行位置を確認し、判断の経緯を上流文書の Q&A で確認します。存在しない場合は、この文書を単体の最終仕様として扱います。最終更新エージェントのセッション情報が参照可能であれば再開時のコンテキストとして利用し、別エージェントからの引き継ぎや担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。
-->

# sym_loader で 1 つのコールバック元に複数のコールバック先を持たせる最終仕様

ファイル名: `261108-sym-loader-multi-target-design.md`

検討 ID: 261108-sym-loader-multi-target

合意版: 検討用 Markdown 版 9 (2026-11-08)

状態: 承認済み。2026-11-08 の対話で成果物を承認しました。

## 目的

1 つのコールバック元に、複数のコールバック先を設定できます。  
`cplat` は解決できた移譲先を設定順に保持し、件数とインデックスで関数ポインターを返します。すべてを呼ぶか、先頭の 1 つだけを呼ぶかは、利用側のコードが決めます。戻り値の集約も利用側が担当します。

従来の単一移譲先から複数移譲先へ拡張し、一部を解決できなくても、解決できた移譲先を利用できます。除外した項目と理由は診断 API で取得できるため、通常の情報表示と障害の調査を分けられます。

`override-sample` の `base_calc` は、この契約の計算例です。リストの長さが 0 のときは `result = a + b` を行い、1 以上のときは添字 0 だけを呼びます。サンプル設定には複数の先を書きません。0 件と 1 件の観測できる結果は、今のサンプルと同じです。

## 公開 API と ABI

### 公開関数の構成

`sym_loader` の公開関数は、既存の 4 関数と新規の 5 関数を合わせた次の 9 関数です。宣言には `CPLAT_EXPORT` と `CPLAT_API` を付け、C++ から参照する場合も C リンケージを維持します。

```c
CPLAT_EXPORT void CPLAT_API cplat_sym_loader_init(
    cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
    const char *configpath);
CPLAT_EXPORT void CPLAT_API cplat_sym_loader_dispose(
    cplat_sym_loader_entry *const *fobj_array, size_t fobj_length);
CPLAT_EXPORT int CPLAT_API cplat_sym_loader_is_default(cplat_sym_loader_entry *fobj);
CPLAT_EXPORT int CPLAT_API cplat_sym_loader_is_failed(cplat_sym_loader_entry *fobj);
CPLAT_EXPORT int CPLAT_API cplat_sym_loader_info(
    cplat_sym_loader_entry *const *fobj_array, size_t fobj_length);
CPLAT_EXPORT size_t CPLAT_API cplat_sym_loader_count(cplat_sym_loader_entry *fobj);
CPLAT_EXPORT void *CPLAT_API cplat_sym_loader_get(cplat_sym_loader_entry *fobj, size_t index);
CPLAT_EXPORT size_t CPLAT_API cplat_sym_loader_reject_count(cplat_sym_loader_entry *fobj);
CPLAT_EXPORT int CPLAT_API cplat_sym_loader_reject_get(
    cplat_sym_loader_entry *fobj, size_t index,
    const char **lib_name, const char **func_name, int *reason);
```

値渡し引数 `fobj_length` の最上位の `const` は、関数型を変えないため一覧では省略しています。

| 対象 | 変更内容 | 互換性への影響 |
| --- | --- | --- |
| `cplat_sym_loader_resolve` | 公開関数を削除 | バイナリ ABI とソース API の破壊的変更 |
| `cplat_sym_loader_count`、`cplat_sym_loader_get`、`cplat_sym_loader_reject_count`、`cplat_sym_loader_reject_get`、`cplat_sym_loader_is_failed` | 公開関数を追加 | エクスポート表と `mock_cplat` へ同じシグネチャで追加 |
| `cplat_sym_loader_entry` | 単一移譲先から複数移譲先を保持する構造へ変更 | サイズとメンバー配置が変わるため利用側の再ビルドが必要 |
| `CPLAT_SYM_LOADER_ENTRY_INIT` | 引数をキーだけに変更 | ソース API の破壊的変更。マクロ自体はエクスポート シンボルを持たない |
| `cplat_sym_loader_resolve_as` / `cplat_sym_loader_get_as` | 旧マクロを削除し、インデックス指定のマクロを追加 | 呼び出し側のソースを更新 |

Table: 公開インターフェイスの変更と互換性

### 移譲先と診断の取得

`cplat_sym_loader_resolve` と `cplat_sym_loader_resolve_as` は削除します。  
`cplat_sym_loader_init`、`cplat_sym_loader_dispose`、`cplat_sym_loader_is_default`、`cplat_sym_loader_info` は残し、リストに合わせます。  
`cplat_sym_loader_is_failed` を追加します。内部失敗が有効なあいだは 1、それ以外は 0 を返します。この呼び出しは、すでに有効な失敗を解消しません。未解決で失敗でもないエントリでは、返す前に解決を試みます。失敗中の件数、診断件数、再試行は「解決と並び」に書きます。

移譲先は、件数と添字で渡します。

```c
size_t cplat_sym_loader_count(cplat_sym_loader_entry *fobj);
void *cplat_sym_loader_get(cplat_sym_loader_entry *fobj, size_t index);
#define cplat_sym_loader_get_as(fobj, index, type) ((type)cplat_sym_loader_get((fobj), (index)))
```

件数 0 は移譲先なしです。範囲外の添字は NULL です。先頭は添字 0 です。  
未解決のエントリに対する件数と取得は、返す前にそのエントリを解決します。

無効な設定項目は、別の件数と添字で受け取ります。

```c
size_t cplat_sym_loader_reject_count(cplat_sym_loader_entry *fobj);
int cplat_sym_loader_reject_get(cplat_sym_loader_entry *fobj, size_t index,
    const char **lib_name, const char **func_name, int *reason);
```

`cplat_sym_loader_reject_get` は、成功時に `CPLAT_OK` を返し、3 つの出力引数へライブラリ名、関数名、理由を書きます。  
範囲外の添字、または 3 つの出力引数のいずれかが NULL のときは `CPLAT_ERR_INVALID_ARGUMENT` を返し、名前と理由は書きません。  
返した文字列は、そのエントリを `cplat_sym_loader_dispose` するまで有効です。呼び出し側は解放しません。  
診断の取得も、未解決なら解決してから返します。

`CPLAT_SYM_LOADER_ENTRY_INIT` は、キーだけを受け取ります。型引数は展開に使っていないため、削ります。関数型は `cplat_sym_loader_get_as` の呼び出し側だけが持ちます。型を渡す古い呼び出しは、コンパイルできないようにします。

```c
typedef struct cplat_sym_loader_entry_impl cplat_sym_loader_entry_impl;

typedef struct cplat_sym_loader_entry {
    const char *func_key;
    cplat_sym_loader_entry_impl *impl;
    cplat_atomic_i32 resolved;
    cplat_atomic_i32 failed;
    cplat_atomic_i32 lock_state;
    cplat_local_lock *lock;
} cplat_sym_loader_entry;

#define CPLAT_SYM_LOADER_ENTRY_INIT(key) \
    {(key), NULL, CPLAT_ATOMIC_INIT(0), CPLAT_ATOMIC_INIT(0), CPLAT_ATOMIC_INIT(0), NULL}
```

公開構造体は、1 つのエントリが先の列を持つ完全な型のままです。不透明ポインターにはしません。  
先の列、診断の列、モジュール ハンドル、関数ポインターは、`impl` が指す非公開の管理構造体に置きます。`impl` の中身は公開ヘッダーに書きません。利用側が名前の配列へ直接書き込む契約はありません。設定の取り込みは `cplat_sym_loader_init` が行います。  
`resolved` は 0 または 1 です。1 は、利用側の列と診断の列を公開したあとに release で書きます。高速経路は、この値を acquire で読みます。  
`failed` は 0 または 1 です。内部失敗のあいだは `resolved` を 0 のままにし、`failed` を 1 にします。失敗を `resolved` の 0 以外にすると、次の取得が再試行しません。初期化で列を確保できないときは `impl` が NULL のままになり得るため、`failed` はエントリ本体に置きます。  
`lock_state` と `lock` は、ロックを取らない高速経路とロックの初期化に使います。0 は未初期化、1 は初期化中、2 は初期化済みです。ロックの生成に失敗したときは `failed` を 1 にし、`lock_state` を 0 に戻します。生成できたロックは残します。  
構造体の大きさが変わるため、このヘッダーを使用する利用側はソースを更新し、新しいヘッダーとライブラリで再ビルドします。旧ヘッダーでビルドした利用側と、新しいライブラリの組み合わせは互換性の対象外です。

> [!NOTE]
> 先の列は非公開の管理構造体へまとめ、キー、管理ポインター、同期状態はエントリ本体に残します。`failed` を本体に置くのは、列の確保に失敗したときに `impl` が NULL でも失敗を読めるようにするためです。明示的既定値は管理構造体側に置き、`cplat_sym_loader_is_default` は `resolved` の acquire のあとにそれを読みます。

### 寿命とスレッド契約

利用側は、有効なエントリを取得系 API に渡します。`reject_get` の出力引数に対する NULL 検査と、今回の対象外であるエントリ ポインターの NULL 検査は区別します。

取得した関数ポインターと診断文字列は、エントリが保持するリソースを参照します。利用側は解放せず、`dispose` 後には使用しません。`init` と `dispose` は、取得系 API の実行および取得した関数ポインターの呼び出しと並行して実行しません。

| 関数 | スレッド |
| --- | --- |
| `cplat_sym_loader_count`、`cplat_sym_loader_get`、`cplat_sym_loader_reject_count`、`cplat_sym_loader_reject_get`、`cplat_sym_loader_is_default`、`cplat_sym_loader_is_failed`、`cplat_sym_loader_info` | スレッド セーフ |
| `cplat_sym_loader_init`、`cplat_sym_loader_dispose` | 単一スレッド。DLL のロード直後やアンロード時に呼ぶ |

Table: 公開関数のスレッド契約

件数と取得は、削除する `cplat_sym_loader_resolve` のスレッド契約を引き継ぎます。初期化と解放の単一スレッド契約は変えません。

## 設定の読み方

ルートは今どおり JSONC のオブジェクトです。キーは `func_key` です。  
値がオブジェクトなら、先は 1 件です。値が配列なら、各要素が先 1 件です。要素の順は、設定に書いた順です。  
オブジェクト 1 件の既存の設定は、そのまま先 1 件として読みます。

各先は、文字列の `lib` と `func` を持つオブジェクトです。  
値がオブジェクトでも配列でもない項目、配列要素がオブジェクトでない項目、`lib` または `func` が文字列でない項目は、今の初期化と同じく読み飛ばします。診断の列には入れません。設定に無いキーも入れません。キーが空の項目も読み飛ばします。

一致する `func_key` の最初のエントリへ書きます。同じキーの静的エントリが複数あっても、2 件目以降には書きません。

同じキーがファイルに複数回現れたときは、後のオブジェクトまたは配列で、そのキーの先の列を置き換えます。オブジェクトでも配列でもない後の値は読み飛ばし、置き換えません。  
置き換えて捨てた前の各先は、書いた順に診断の列へ入れます。理由は `CPLAT_SYM_LOADER_REJECT_DUPLICATE_KEY` の -7 です。ライブラリは開きません。捨てた先の中身は、空や長すぎるなどの理由へ再分類しません。採用した後の値から出た診断は、その後に続けます。同じキーが 3 回以上あるときは、最後より前の各先を書いた順に -7 で入れます。明示的既定値かどうかは、採用した後の値だけで決めます。

```jsonc
{
  "base_calc": { "lib": "libfirst", "func": "first_calc" },
  "base_calc": { "lib": "libsecond", "func": "second_calc" }
}
```

この例では、診断の先頭が `libfirst` と `first_calc` の -7 です。`second_calc` を解決できたときは、利用側の列がその 1 件です。

1 つのキーの件数に、ファイル全体の `1024 * 1024` バイト以外の上限は設けません。  
設定されたライブラリ名と関数名は、両 OS とも終端を除く 255 バイトまで解決できます。長さは `strlen` によるバイト数です。256 バイト以上は名前が長すぎるものとして診断へ入ります。  
公開定数 `CPLAT_SYM_LOADER_NAME_MAX` は 256 のままです。これは終端を含む配列長であり、設定名の上限そのものではありません。  
拡張子を付ける作業バッファーは、この定数の配列を使わず、255 バイトに拡張子と終端を足した長さを別に確保します。  
名前が空でも長すぎても、診断で返せるように、設定された文字列を切り詰めずに保持します。255 バイトの判定は分類であり、診断文字列の保持長ではありません。  
列の記憶を確保できないときは、そのエントリを未設定にはしません。`failed` を 1 にし、件数、診断件数、`cplat_sym_loader_is_default` は 0 です。起動は続けます。次の件数、取得、診断、`cplat_sym_loader_is_default`、`cplat_sym_loader_info` で、確保からやり直します。読んだ設定を保持する記憶まで確保できないときも、同じ失敗のまま次の取得でその確保からやり直します。

## 解決と並び

解決は、件数、取得、診断、`cplat_sym_loader_is_default`、`cplat_sym_loader_info` が、まだ解決していないエントリに触れたときに行います。`failed` が 1 のエントリでは、これらの呼び出しがその失敗を解消してからやり直します。  
`cplat_sym_loader_is_failed` は、すでに `failed` が 1 のときは 1 を返し、状態を変えません。  
エントリのロックは、Linux では既定の `pthread_mutex`、Windows では `SRWLOCK` です。同じスレッドがロックを重ねて取る使い方は想定しません。  
使えた先の列と、診断の列を公開してから、`resolved` を 1 として release で書き込みます。高速経路は、その値を acquire で読みます。関数の呼び出しはロックの外で、利用側が行います。  
ロックの初期化・取得、解決に必要なメモリの確保、初期化時の列の確保に失敗したときは、移譲先ごとの診断理由には入れません。`failed` を 1、`resolved` を 0 にします。失敗中の件数、診断件数、`cplat_sym_loader_is_default` は 0 です。その試行で開いたモジュールと、途中まで作った列は解放します。起動は続けます。やり直しがまた失敗したときは、その呼び出しも同じ失敗中の契約です。

次の分類は、採用した後の値に書いた先へ行います。重複で捨てた先は、この分類には入れず、診断の列の前に -7 だけで並びます。採用した先には、書いた順に次の順で 1 つの理由を付けます。

1. 書いた先が 2 件以上あり、その中に `lib` と `func` がどちらも `default` である先が 1 件でもあれば、ライブラリは開きません。利用側の件数は 0、`cplat_sym_loader_is_default` は 0 です。書いた先はすべて診断の列に入り、理由は `CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT` の -5 です。この判定は、片方だけが `default` である -6 より先です。
2. 書いた先が 1 件で、`lib` と `func` がどちらも `default` なら、ライブラリは開きません。利用側の件数は 0、`cplat_sym_loader_is_default` は 1、診断の件数は 0 です。
3. 書いた先が 0 件なら、利用側の件数は 0、`cplat_sym_loader_is_default` は 0、診断の件数は 0 です。
4. それ以外は、先ごとに次の順で判定し、先に一致した理由だけを付けます。`default` は、文字列 `default` と一致することです。
    - `lib` と `func` の片方だけが `default` のときは、ライブラリを開かず、診断の列に入れます。理由は `CPLAT_SYM_LOADER_REJECT_PARTIAL_DEFAULT` の -6 です。明示的既定値にはしません。もう片方の名前が空でも長すぎても、この先には -6 だけを付けます。同じキーのほかの使えた先は残します。
    - `lib` または `func` の長さが 0 のときは、名前が空であるものとして診断の列に入れます。
    - 終端を除く長さが 255 バイトを超えるときは、名前が長すぎるものとして診断の列に入れます。
    - ライブラリを開けないときは、その理由で診断の列に入れます。
    - シンボルが無いときは、そのハンドルを閉じ、シンボルが見つからないものとして診断の列に入れます。
    - それ以外は利用側の列に残し、ハンドルと関数ポインターを保持します。

```jsonc
"base_calc": [
  { "lib": "liboverride", "func": "override_calc" },
  { "lib": "default", "func": "override_calc" }
]
```

この例では、`liboverride` の `override_calc` が利用側の列に残ります。診断は 2 件目だけで、理由は -6 です。ライブラリは開きません。

利用側の列は、使えなかった先を除いたあとの、設定に書いた順です。オブジェクト 1 件のときは、その 1 件が添字 0 です。  
診断の列は、重複キーで捨てた前の各先を書いた順に -7 で並べ、その後に採用した値の上記の判定で無効とした移譲先を設定順に続けます。明示的既定値と、初期化時に型不一致などで読み飛ばした項目は含めません。

移譲先ごとの診断理由は、次の 7 件です。内部失敗は、この表には入れません。

| 公開定数 | 値 | 理由 |
| --- | --- | --- |
| `CPLAT_SYM_LOADER_REJECT_EMPTY_NAME` | -1 | 名前が空 |
| `CPLAT_SYM_LOADER_REJECT_NAME_TOO_LONG` | -2 | 名前が長すぎる |
| `CPLAT_SYM_LOADER_REJECT_OPEN_FAILED` | -3 | ライブラリを開けない |
| `CPLAT_SYM_LOADER_REJECT_SYMBOL_NOT_FOUND` | -4 | シンボルが見つからない |
| `CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT` | -5 | 複数の移譲先に明示的既定値が含まれるため、キー全体を未適用にした |
| `CPLAT_SYM_LOADER_REJECT_PARTIAL_DEFAULT` | -6 | `lib` と `func` の片方だけが `default` のため、その先を開かなかった |
| `CPLAT_SYM_LOADER_REJECT_DUPLICATE_KEY` | -7 | 同じキーの後の値で置き換えたため、前の先を捨てた |

Table: `cplat_sym_loader_reject_get` が返す理由

これらは `reason` 出力引数に書き込む値です。`reject_get` 自体の成否を返す `CPLAT_OK` / `CPLAT_ERR_INVALID_ARGUMENT` とは用途が異なります。

> [!NOTE]
> -1 から -3 は、これまでの `resolved` の負の値と同じです。シンボルが見つからない状態は、これまで負の値ではなく、解決済みかつ関数ポインター NULL でした。並びに `default` がある状態、片方だけが `default` である状態、重複キーで捨てた先は、今回の列で分けた区分です。そのため -4 から -7 を足します。公開の `resolved` は、これらの理由を持たず、0 か 1 だけです。

同じライブラリを先が 2 件開いたときは、ハンドルも 2 つ保持します。OS の参照カウントに従い、解放時に 2 回閉じます。

## 情報表示と明示的既定値

`cplat_sym_loader_is_default` は、上記の 2 のときだけ 1 を返します。未設定、キー全体を未適用にした状態、片方だけが `default` の先、内部失敗が有効な状態は 0 です。  
長さ 0 のときの `base_calc` は、明示的既定値、キー全体の未適用、未設定のいずれでも、組み込みの加算です。明示的既定値かどうかは `cplat_sym_loader_is_default` で区別します。

`cplat_sym_loader_info` は、利用側に渡す列だけを表示します。外した先と理由、`failed` の値は出しません。外したことだけでは、戻り値を失敗にしません。内部失敗が残っていても、引数が正しければ失敗にしません。  
引数が不正なときの `CPLAT_ERR_INVALID_ARGUMENT` は、今のとおりです。引数が正しければ `CPLAT_OK` を返します。失敗中の表示は、件数 0、明示的既定値 0、残った先なしです。

表示は、エントリごとにキー、明示的既定値、利用側の件数、残った先のライブラリ名、関数名、関数ポインターです。

```text
- [0] base_calc
    - is_default : 0
    - count : 1
    - [0] lib=liboverride func=override_calc ptr=0x1
```

## 再入

リストを返す関数が戻ったあとに、先が元の関数を再び呼ぶ経路も、ライブラリを開いている途中に元へ戻る経路も、保証しません。  
機能仕様の「差し替えた機能から同じ差し替え対象を再帰的に呼び出す構成の動作は保証しません」は維持します。

## ソースの置き場所

公開宣言は `app/cplat/prod/include/cplat/runtime/sym_loader.h` です。`CPLAT_SYM_LOADER_ENTRY_INIT` の呼び出しは、このヘッダー、`app/cplat/prod/libsrc/cplat/runtime/README.md`、`app/override-sample/prod/libsrc/base/sym_loader_libbase.c`、および `sym_loader` の単体テストから、キーだけの形へ更新します。  
初期化は `sym_loader_init.c`、解放は `sym_loader_dispose.c`、情報表示は `sym_loader_info.c`、明示的既定値は `sym_loader_is_default.c`、内部失敗は `sym_loader_is_failed.c` です。  
件数、取得、診断の取得と、列の解決は `sym_loader_resolve.c` に置きます。公開シンボル `cplat_sym_loader_resolve` は置きません。ファイル名は、単体テストの `TEST_SRCS` がこのパスを指しているため維持します。  
テスト ディレクトリに生成される `sym_loader_*.c` の複製は編集しません。  
`symLoaderResolveTest` のロック初期化フックが依存する内部関数は、同じ翻訳単位に残します。

`base_calc` は次の形にします。

```c
size_t count = cplat_sym_loader_count(pfo_base_calc);

if (count == 0)
{
    *result = a + b;
    return BASE_OK;
}

base_calc_fn fp = cplat_sym_loader_get_as(pfo_base_calc, 0, base_calc_fn);
return fp(a, b, result);
```

サンプルの `libbase_extdef.jsonc` は、オブジェクト 1 件のままにします。複数の先は足しません。

## 知見の反映

機能仕様 `app/cplat/docs/functional-spec/runtime.md` の「実行時の機能差し替え」は、関数名、型名、設定ファイルの書式を書かずに、次の振る舞いに更新します。再入を保証しない前提の文は残します。

設定に、対象の機能と、読み込むモジュールおよび解決するシンボルの組を指定できます。組は 1 つでも複数でも指定できます。  
利用側は、解決できた参照先の列を受け取ります。すべてを呼ぶか、列の先頭だけを呼ぶかは、利用側が決めます。列の長さが 0 のときは、参照先がありません。  
列の順序は、設定に書いた順です。使えなかった組を除いても、残った組の前後は書いた順のままです。  
使えなかった組は列から外し、使えた組は残します。名前が空である、名前が長すぎる、モジュールを開けない、シンボルが見つからない、モジュールとシンボルの片方だけが既定である、のいずれかがその組です。同じ機能の指定がファイルに複数回あるときは、後の指定で前の組を置き換え、置き換えて外した組も診断の対象にします。外した組と理由は、診断のために別途取得できます。外したことだけでは、起動を失敗にしません。  
モジュールとシンボルの両方が既定である指定は、その機能の組が 1 つだけのとき、明示的な既定値です。このとき参照先の列は空です。同じ指定が複数の組に含まれるときは、その機能の差し替え全体を適用しません。この全体の未適用は、片方だけが既定である組を外す処理より先に判定します。  
設定ファイルがない場合、必須情報が不足する場合、未知の項目がある場合、型が一致しない場合は、その項目を差し替えに使いません。名前やモジュール解決の失敗は、その組だけを外し、解決できた組は残します。ローダー内部の確保や同期に失敗したときは、参照先が無い状態と区別できる失敗として残し、次の取得でやり直します。これらの場合でも、設定を使用しない通常の起動は続けます。

公開 API の逆引き `app/cplat/docs/api-cheatsheet.md` と、利用手順 `app/cplat/prod/libsrc/cplat/runtime/README.md` は、削除する関数を外し、件数、取得、診断の取得、内部失敗の取得を記載します。  
`app/override-sample/prod/libsrc/base/sym_loader_libbase.md` の呼び出し説明も、同じ契約へ更新します。  
ヘッダーのファイル先頭コメントは、取得の手順を件数と添字に合わせます。

## 受入条件

次を満たしたとき、この仕様の実装は完成です。

- オブジェクト 1 件の設定は先 1 件として読め、配列は書いた順の複数の先として読める。
- 使えなかった先だけが外れ、残った先の前後は書いた順のままである。添字 0 がその先頭である。
- 先が 1 件で `lib` と `func` がどちらも `default` のとき、件数は 0、`cplat_sym_loader_is_default` は 1、診断の件数は 0 である。
- 移譲先が 2 件以上あり、その中に `lib` と `func` がともに `default` の指定があるキーは未適用となる。件数は 0、`cplat_sym_loader_is_default` は 0、保持した移譲先はすべて診断の列に理由 -5 で入る。
- 設定されたライブラリ名と関数名は、両 OS とも終端を除く 255 バイトまで解決できる。256 バイト以上は診断の列に -2 で入る。拡張子を付けるバッファーは、255 バイトに拡張子と終端を足した長さを別に確保する。診断が返す名前は、設定に書いた文字列を切り詰めない。
- 名前が空、名前が長すぎる、ライブラリを開けない、シンボルが見つからない、は診断の列にそれぞれ -1、-2、-3、-4 で入り、使えた先は残る。
- `lib` と `func` の片方だけが `default` の先はライブラリを開かず、診断の列に -6 で入る。同じキーの使えた先は残る。`cplat_sym_loader_is_default` は 0 である。先が 2 件以上で両方が `default` の先を含むキーは、この -6 より先に -5 でキー全体を未適用にする。
- 同じキーの後の値で置き換えたとき、捨てた前の各先は書いた順に -7 で診断へ入る。採用した後の値から出た診断は、その後に続く。
- ロックの初期化・取得、解決用メモリ、初期化時の列の確保に失敗したとき、`cplat_sym_loader_is_failed` は 1、件数、診断件数、`cplat_sym_loader_is_default` は 0 である。次の件数、取得、診断、`cplat_sym_loader_is_default`、`cplat_sym_loader_info` で再試行する。起動は続く。
- 設定に無いキー、型が一致しない値、文字列でない `lib` または `func` は、診断の列に入らない。
- `cplat_sym_loader_info` は利用側の列だけを表示する。除外した項目があっても、引数が正しければ `CPLAT_OK` を返す。
- `base_calc` は、長さ 0 で加算し、長さ 1 以上で添字 0 だけを呼ぶ。サンプル設定に複数の先は無い。
- 公開関数 9 件の宣言、エクスポート表、`mock_cplat` のシグネチャが一致し、旧公開関数 `cplat_sym_loader_resolve` が残っていない。旧マクロ `cplat_sym_loader_resolve_as` とその呼び出しも残っていない。
- `get_as` は関数ポインター型への変換を行い、診断理由の公開定数 7 件は仕様の値と一致する。
- `CPLAT_SYM_LOADER_ENTRY_INIT` はキーだけを受け取る。型を渡す呼び出しは残っていない。
- 差し替えた先から元を再び呼ぶ動作を、新たに保証するテストは置かない。

## 検証

実行するのは次です。`app/cplat` 全体の `make test` と、ワークスペース ルートの `make test` は実行しません。

- `symLoaderInitTest`、`symLoaderResolveTest`、`symLoaderDisposeTest`、`symLoaderInfoTest`、`symLoaderIsDefaultTest`
- 公開関数の追加と削除に必要な `exportTest`
- `override-sample` の `base_calc` の単体テスト。長さ 0、長さ 1、長さ 2 以上で先頭だけを呼ぶことを確認する。サンプル設定ファイルには複数の先を足さない

ディレクトリ名 `symLoaderResolveTest` は維持します。中身は件数、取得、診断の検証へ変えます。  
内部失敗、再試行、初期化時の確保失敗は、`symLoaderResolveTest` と `symLoaderIsDefaultTest` で確認します。新しいテスト ディレクトリは作りません。  
ロック初期化の既存のフック試験は、内部関数が残る範囲で維持します。

変更した Markdown は、対象ファイルごとに `text_style_jp.py --dry-run` のあと `--in-place` を実行します。  
ビルド後は、対象範囲の内容がある `.warn` を確認します。

`mock_cplat` の更新は、`create-mock-cplat-mock` スキルの手順に従います。

## 今回含めないこと

エントリのポインターや、解放時の配列要素が NULL のときの検査は、今回の変更に含めません。事象は残課題一覧に記録します。
