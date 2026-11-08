<!--
生成元: agree-before-implement スキル
文書種別: 最終仕様 (design)
最終更新エージェント: copilot / セッション ID: 8f839972-2ad8-411b-8421-b03bde2c138a
上流文書: `261108-sym-loader-null-contract-discussion.md` (検討用 Markdown)。削除されている場合があります。
再開手順: agree-before-implement スキルを読み込み、上流文書がある場合は作業 WBS と承認状況を確認してください。上流文書がない場合は、この文書を単体の最終仕様として扱います。
-->

# sym_loader の不正引数と資源の寿命 最終仕様

検討 ID: `sym-loader-null-contract-20261108`  
検討用 Markdown の合意版: 9  
承認状態: 承認済み (2026-11-08 15:14:40 JST)

## 目的と対象

`sym_loader` の公開 API は NULL や内部失敗を正常な空の結果と区別します。  
モジュールの解放に失敗した場合はハンドルを失わず、明示的アンロード前に利用側が解放を再試行できます。  
対象は cplat の sym_loader、呼び出し側の override-sample、関連テストと公開契約です。

戻り値と引数の変更、および `cplat_sym_loader_get_as` の削除はソース互換性と ABI を変更します。  
利用側、mock、エクスポート型検査を移行し、再ビルドします。

## 取得と診断の API

次の API は成否を `int` の共通結果コードで返し、値は出力引数へ格納します。  
`cplat_sym_loader_reject_get` の引数は維持します。

```c
int cplat_sym_loader_count(cplat_sym_loader_entry *entry, size_t *count_out);
int cplat_sym_loader_get(cplat_sym_loader_entry *entry, size_t index, void **address_out);
int cplat_sym_loader_reject_count(cplat_sym_loader_entry *entry, size_t *count_out);
int cplat_sym_loader_reject_get(cplat_sym_loader_entry *entry, size_t index,
                                const char **lib_name, const char **func_name, int *reason);
int cplat_sym_loader_is_default(cplat_sym_loader_entry *entry, int *value_out);
int cplat_sym_loader_is_failed(cplat_sym_loader_entry *entry, int *value_out);
```

必須入力または出力が NULL の場合、`CPLAT_ERR_INVALID_ARGUMENT` を返し、出力を変更しません。  
`get` と `reject_get` の範囲外の添字も `CPLAT_ERR_INVALID_ARGUMENT` とし、出力を変更しません。  
解決処理の内部失敗や解放待ちのエントリに対する取得・診断は `CPLAT_ERR_UNKNOWN` を返し、出力を変更しません。

`is_failed` は有効なエントリの内部失敗の状態を成功時の出力値 0 または 1 で返します。  
失敗がすでに有効なときは解消を試みません。未解決で失敗していない場合は従来どおり解決を試み、失敗すれば値 1 を返します。  
`info` は配列の NULL を事前検査し、内部解決失敗時は `CPLAT_ERR_UNKNOWN` を返します。表示済みの内容は取り消しません。

`cplat_sym_loader_get_as` は削除します。  
利用側は `void *` に取得し、`CPLAT_OK` の確認後に関数ポインター型へ変換します。関数ポインター変数のアドレスを `void **` に変換しません。

```c
void *address;
int rc = cplat_sym_loader_get(entry, 0u, &address);
if (rc == CPLAT_OK)
{
    sample_func_fn func = (sample_func_fn)address;
    /* func を使用する */
}
```

## 設定の初期化

`cplat_sym_loader_init` は `int` の共通結果コードを返します。  
要素数 0 は NULL 配列を許容します。正の要素数で配列または要素が NULL なら、設定ファイルを開く前に `CPLAT_ERR_INVALID_ARGUMENT` を返し、エントリを変更しません。  
解放待ちのエントリを含む配列は、設定を読み込む前に `CPLAT_ERR_BUSY` で拒否します。

NULL または空のパス、および存在しない設定ファイルは `CPLAT_SKIPPED` を返し、エントリを変更しません。  
ファイルを開く失敗は `cplat_fopen` のエラー詳細で不存在とそれ以外を区別します。  
読み込みや解析に失敗した場合は、原因を判別できるときは共通の分類済みエラーを返し、判別できないときは `CPLAT_ERR_UNKNOWN` を返します。  
解析器が構文不正とメモリ不足を区別できない失敗を、構文不正と断定しません。

設定適用中にエントリごとの内部失敗が生じた場合は、ほかのエントリの処理を続け、1 件でも失敗があれば `CPLAT_ERR_UNKNOWN` を返します。  
成功したエントリの適用結果は保持し、失敗したエントリは原因に応じて次の取得で再試行するか、解放待ちにします。

## モジュールとロックの解放

`cplat_sym_loader_dispose` は `int` の共通結果コードを返します。  
要素数 0 は NULL 配列でも `CPLAT_OK` です。正の要素数で配列または要素が NULL の場合は、全要素を解放する前に `CPLAT_ERR_INVALID_ARGUMENT` を返します。

各ハンドルは開いた回数に対応する回数だけ閉じます。  
OS の解放に失敗した場合は、失敗したハンドルと再試行に必要な管理情報を保持し、ほかのハンドルとエントリの解放を続けて `CPLAT_ERR_UNKNOWN` を返します。  
すべてのモジュールを解放できたエントリは管理領域とロックを破棄し、管理ポインター、ロック ポインター、解決・失敗フラグ、ロック初期化状態を戻します。  
解放に失敗したエントリはロックと残資源を保持します。

解放待ちの間、取得・診断・情報表示はそのエントリを `CPLAT_ERR_UNKNOWN` とし、`is_failed` は失敗を示します。  
後続の `dispose` は残ったハンドルだけを再試行します。  
`init` の設定入れ替えとシンボル解決途中の後始末で生じた解放失敗も同じように保持し、呼び出し側へ失敗を通知します。

解決時はモジュールを開く前に、開く可能性があるハンドルを保持できる領域を確保します。  
確保に失敗した場合は、その領域が必要なモジュールを開かず、内部失敗として通知します。  
解決後の名前の確保、列への追加、シンボル不在、ほかの解決失敗に伴う後始末でも、解放に失敗したハンドルを新たな確保なしで保持します。

## 寿命とスレッドの契約

取得した関数ポインターと診断文字列は呼び出し側で解放しません。  
対象エントリを含む `init` または `dispose` の開始後は、それ以前に取得したポインターを使わず、必要なら取得し直します。  
これは設定が省略された場合や引数が拒否された場合にも適用します。

取得・診断と情報表示の既存のスレッド セーフ契約は維持します。  
`init`、`dispose`、終了関数は、同じエントリの取得・診断・情報表示および取得済み関数ポインターの利用と並行して呼びません。  
Windows の `DllMain` では、モジュールのロードや解放を行い得る取得・診断・情報表示、再初期化、解放を実行しません。

## override-sample の起動と終了

ロード時の初回設定読み込みは従来の `onLoad` で行います。  
`init` の `CPLAT_SKIPPED` はエラーと区別し、エラーの場合は `CPLAT_DLLMAIN_INFO_MSG` で通知して起動を続けます。  
環境変数が未設定の場合や送信に失敗した場合は通知されません。

`base_calc` の件数またはポインターの取得が失敗した場合は、`BASE_ERR_UNKNOWN` を返し、結果出力を変更しません。  
取得成功時は従来どおり、0 件なら加算し、1 件以上なら先頭の移譲先を呼びます。

`libbase` に `int base_sym_loader_shutdown(void)` を公開し、Windows / Linux の利用側は明示的アンロード前に通常の呼び出しコンテキストで実行します。  
終了関数は sym_loader の解放成功時に `BASE_OK`、失敗時に `BASE_ERR_UNKNOWN` を返します。失敗時は利用側がアンロードせず、残資源の解放を再試行できます。  
終了に成功した後、アンロード前に `base_calc` や情報表示を再び呼びません。

プロセス終了時に終了関数を呼ぶことは必須ではなく、残資源は OS の回収に任せます。  
Windows の `DllMain` と Linux の `destructor` では sym_loader の解放を行いません。  
Windows の明示的アンロード時に終了関数の未実行を検出できた場合は `CPLAT_DLLMAIN_INFO_MSG` で通知します。Linux のフックはプロセス終了と区別できないため通知しません。

> [!IMPORTANT]
> 終了関数の未実行は、フックが実行されなければ検出できません。通知も環境変数や送信結果に依存します。利用側は通知に頼らず、明示的アンロード前に終了関数の成功を確認してください。

## 既存の複数先動作

JSONC のオブジェクトと配列、移譲先の設定順、重複キーの後勝ち、および既存の診断理由と並びを維持します。  
明示的 `default`、空名、名前の長さ、部分的 `default`、ロード失敗、シンボル不在の既存の分類は変更しません。  
移譲先を開けなかったことや除外したことだけでは内部失敗にしません。  
ただし、シンボル不在などの後始末でモジュールを閉じられなかった場合は、ハンドルを保持し、内部失敗として通知します。

## 正本文書への反映

- `app\cplat\prod\include\cplat\runtime\sym_loader.h` に引数、出力、結果コード、所有権、寿命、スレッドとローダー コンテキストの契約を記載します。
- `app\cplat\prod\libsrc\cplat\runtime\README.md` と `app\cplat\docs\api-cheatsheet.md` を公開 API と利用手順に合わせます。利用者の前提と機能の変更は `app\cplat\docs\functional-spec\` の該当文書も確認し、必要な範囲を更新します。
- `app\override-sample\prod\include\base\base_spec.h`、`app\override-sample\prod\README.md` と関連文書に、公開終了関数と明示的アンロード時の利用側の責任を記載します。
- 対策元 backlog と重複する過去 backlog は削除せず、今回の対処とマージ後の経緯を記録します。

## 受入条件と検証

- NULL の各必須引数、0 件と不正配列、範囲外、内部失敗、出力不変、正常系、既存の複数先の順序と診断を局所テストで確認します。
- 初期化、解決、解放の各経路で解放失敗とメモリ確保失敗を注入し、ハンドル保持、ほかの解放、取得拒否、`is_failed`、`init` の拒否、再試行、ロックの寿命を確認します。
- 設定省略、読込・解析失敗、複数エントリの部分失敗、起動継続、両 OS の明示的アンロード前の終了、失敗時の再試行、プロセス終了時の扱いと Windows の通知条件を確認します。
- cplat と override-sample の影響箇所を Linux / GCC と Windows / MSVC で局所ビルド・テストし、Linux の条件網羅と Windows の行網羅、内容のある `.warn` を確認します。公開 API と Doxygen、エクスポート型検査、mock、変更した Markdown も確認します。
- framework または複数 app にまたがる全体テストは、コストを説明してユーザーの許可を得た場合に実行します。環境不足で確認できない項目は明示します。
