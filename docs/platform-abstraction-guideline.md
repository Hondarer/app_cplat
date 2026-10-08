# C/C++ プラットフォーム抽象化ガイドライン

## 概要

C/C++ コードでは、OS・CPU・コンパイラ差異の判定を次のヘッダーに集約しています。

- `app/cplat/prod/include/cplat/base/platform.h`
- `app/cplat/prod/include/cplat/base/compiler.h`

利用側のコードは、処理系依存マクロを直接判定するのではなく、`PLATFORM_*` / `COMPILER_*` / `FORCE_INLINE` / `NO_INLINE` を使って分岐してください。

cplat が公開する API 全体の一覧は [cplat API チート シート](api-cheatsheet.md) を参照してください。

## 対応環境

cplat は 64 ビット環境専用です。  
`platform.h` はポインターの幅を `static_assert` で検査し、32 ビット環境ではコンパイル時にエラーとします。  
32 ビット環境向けの条件分岐、互換処理、回避策は一切追加しません。

## 基本ルール

### OS 判定は PLATFORM_* を使う

- Linux 判定: `PLATFORM_LINUX`
- Windows 判定: `PLATFORM_WINDOWS`
- 未知の環境: `PLATFORM_UNKNOWN`

`__linux__` や `_WIN32` をアプリケーションのコードへ直接記述しないでください。

OS 分岐が必要なコードでは、まず `#include <cplat/base/platform.h>` を追加し、`PLATFORM_*` ベースで分岐します。

原則として、`Linux -> Windows` の順で分岐してください。  
ただし、ファイル全体が Windows 向け実装を意図している場合は、`#if defined(PLATFORM_WINDOWS)` を先頭にします。

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    /* Linux 向け処理 */
#elif defined(PLATFORM_WINDOWS)
    /* Windows 向け処理 */
#else
    /* 未対応環境向けの保守的な処理 */
#endif /* PLATFORM_ */
```

Windows 専用バックエンドや Windows 専用 API 実装では、次のように `PLATFORM_WINDOWS` を先頭に置きます。

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)
    /* Windows 向け実装 */
#endif /* PLATFORM_WINDOWS */
```

### CPU 名は ARCH_NAME を使う

診断情報に CPU アーキテクチャー名を出力するときは `ARCH_NAME` を使います。  
64 ビット幅は `platform.h` が保証するため、利用側でアーキテクチャーを判定しません。  
MSVC の対応対象は x64 に限定し、対象の検査も `platform.h` に集約します。

### コンパイラ判定は COMPILER_* を使う

- GCC 判定: `COMPILER_GCC`
- MSVC 判定: `COMPILER_MSVC`
- 未知の処理系: `COMPILER_UNKNOWN`

`__GNUC__` や `_MSC_VER` は `compiler.h` の内部で扱い、利用側では `COMPILER_*` だけを利用します。  
`COMPILER_*` の分岐順は、`GCC -> MSVC` とします。

```c
#include <cplat/base/compiler.h>

#if defined(COMPILER_GCC)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpadded"
#elif defined(COMPILER_MSVC)
    /* MSVC 向け処理 */
#endif /* COMPILER_ */
```

### インライン属性は共通マクロを使う

- 強制インライン: `FORCE_INLINE`
- 非インライン化: `NO_INLINE`

`__forceinline` や `__attribute__((noinline))` を利用側へ直接記述しません。

### platform.h を基本入口にする

OS 判定とコンパイラ判定の両方が関係する可能性があるコードでは、原則として `#include <cplat/base/platform.h>` を使います。

`platform.h` は `compiler.h` を読み込むため、両方の共通マクロを利用できます。

## 直接使用を禁止するマクロ

`platform.h` / `compiler.h` の外側で、次の新規使用を禁止します。

- `_WIN32`
- `_WIN64`
- `__linux__`
- `_MSC_VER`
- `__GNUC__`
- `__clang__`
- `__x86_64__`
- `_M_X64`
- `__i386__`
- `_M_IX86`

既存コードの修正でも、新しい分岐を追加する場合は共通マクロへ置き換えてください。

## 例外として残してよいマクロ

次は環境制御や言語切り替えのためのマクロであり、このガイドラインの禁止対象外です。

- `__cplusplus`
- `DOXYGEN`
- `__INTELLISENSE__`
- `_GNU_SOURCE`
- `WIN32_LEAN_AND_MEAN`

これらは OS / コンパイラ判定の代替として使うのではなく、本来の目的に限定して使います。

## 分岐の置き方

### 小さい差異は同一ファイル内で分岐する

型定義、include、定数、薄いラッパー程度の差異であれば、同一ファイル内の `PLATFORM_*` 分岐に留めます。

この書き方を使う場面:

- OS ごとに include するシステム ヘッダーが違う
- typedef だけが違う
- 呼び出す API 名だけが違う
- テストのモック名だけが違う

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #include <unistd.h>
    typedef int native_file_handle_t;
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    typedef HANDLE native_file_handle_t;
#else
    typedef int native_file_handle_t;
#endif /* PLATFORM_ */
```

呼び出す API 名だけが違う場合は、利用箇所の直前で差異を吸収します。

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #define NATIVE_STAT_FUNC stat
#elif defined(PLATFORM_WINDOWS)
    #define NATIVE_STAT_FUNC _stat64
#endif /* PLATFORM_ */

int call_native_stat(const char *path, native_stat_t *buf)
{
    return NATIVE_STAT_FUNC(path, buf);
}
```

### 差異が大きい実装はファイル分割する

プラットフォームごとに処理フローや依存 API 群が大きく異なる場合は、`*_linux.c` / `*_windows.c` のようにファイルを分けます。

共通ヘッダーで公開する型や関数を定義し、実装ファイル側で `PLATFORM_*` ごとに対象 OS を限定します。

Linux 用ファイルと Windows 用ファイルを同じビルド対象の一覧へ含める構成では、各ファイルが「自分の対象 OS だけを実装する」形にします。

`*_linux.c` 側は、Linux 実装本体を `#if defined(PLATFORM_LINUX)` で囲い、Windows + MSVC でそのファイルが走査されたときだけ C4206 を抑止します。

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

int feature_start(void)
{
    /* Linux 向け実装 */
    return 0;
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif /* PLATFORM_ */
```

Windows 側の実装も同じ考え方で、Windows 実装本体を `PLATFORM_WINDOWS` で囲います。

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

int feature_start(void)
{
    /* Windows 向け実装 */
    return 0;
}

#endif /* PLATFORM_ */
```

MSVC では、中身のない翻訳単位に対して C4206 ("translation unit is empty") を検出します。  
そのため、片側専用ファイルでも、別 OS のビルドで同じソース一覧を使う場合は `#elif defined(...) && defined(COMPILER_MSVC)` を併記してください。

判断目安:

- 依存ヘッダーが大きく違う
- 関数本体の大半が分岐で占められる
- Linux と Windows で異なるシステム モデルを使用している

### 分岐の軸を混ぜない

OS 差異は `PLATFORM_*`、コンパイラ差異は `COMPILER_*` で記述します。

`#ifdef _WIN32` の見た目でも、実際には pragma や属性の違いを扱っているなら、`PLATFORM_*` ではなく `COMPILER_*` へ置き換えます。

分岐順も軸ごとに扱いを分けます。

- `PLATFORM_*`:  
  原則は `Linux -> Windows`。ただし、ファイル全体が Windows 向け実装なら Windows を先頭にします。
- `COMPILER_*`:  
  `GCC -> MSVC` とします。

判断目安:

- システム API、パス、ソケット、スレッド、ファイル操作の違い:  
  `PLATFORM_*`
- pragma、attribute、警告抑制、関数属性の違い:  
  `COMPILER_*`

## 公開ヘッダーのルール

### export / calling convention の公開マクロ名をライブラリ方針に合わせる

公開 API で使う export / calling convention マクロ名は、ライブラリごとの方針に合わせて統一します。

```c
MYLIB_EXPORT int MYLIB_API mylib_open(void);
```

### 定義本体は共通テンプレートを使う

公開ヘッダーごとに `__declspec(dllexport)` や `__stdcall` を定義せず、共通テンプレートのマクロを使って定義します。

```c
#ifndef MYLIB_STATIC
    #define MYLIB_STATIC 0
#endif
#ifndef MYLIB_EXPORTS
    #define MYLIB_EXPORTS 0
#endif
#include <cplat/base/dll_exports.h>
#define MYLIB_EXPORT CPLAT_DLL_EXPORT(MYLIB)
#define MYLIB_API    CPLAT_DLL_API(MYLIB)
```

makefile から渡す場合は `CFLAGS += /DMYLIB_EXPORTS` のように値なしで定義してもよく、このワークスペースでは GCC / Clang / MSVC が暗黙に `1` を与える前提で扱います。

### 変数 (グローバル データ) をエクスポートする場合は export マクロと export テーブルの両方に登録する

公開ヘッダーで `extern` 変数を宣言する場合も、関数と同じ export マクロを先頭に付けます。呼び出し規約マクロ (`MYLIB_API` 相当) は関数専用のため、変数には付けません。

```c
MYLIB_EXPORT extern int g_mylib_feature_flag;
```

`cplat` では、`app/cplat/test/src/libcplatTest/exportTest/exportTest.cc` の `CPLAT_EXPORT_VARIABLE_TABLE(EXPORT_ENTRY)` に `EXPORT_ENTRY(変数名, 型 *)` の形で登録してください。関数と同じ `CPLAT_EXPORT_TABLE` の仕組み (シグネチャの static_assert、実バイナリのエクスポート一覧との突き合わせ) がそのまま変数にも適用され、export マクロの付け忘れとテーブル登録漏れの両方を検出できます。

さらに、`exportTest.cc` の `public_header_variables_declare_export_macro` テストが `prod/include/` 配下を直接走査し、export マクロを伴わない `extern` 変数宣言がないかを機械的に確認します。テーブルへの登録を忘れた場合でも、この走査によって export マクロの付け忘れが検出されます。

この検証の仕組み自体 (テーブルからの static_assert 生成、実バイナリとの突き合わせ、ヘッダー走査) は `cplat` 固有ではなく `framework/testfw` の共通処理として提供されています。他ライブラリでの使い方や、公開ヘッダーと DLL/SO が 1:1 とは限らない構成への対応は [export-symbol-check.md](../../../framework/testfw/docs/export-symbol-check.md) を参照してください。

### 単一プラットフォーム専用 API はヘッダー全体で限定する

Linux 専用 API、Windows 専用 API のように、公開面そのものが片側専用である場合は、ヘッダー全体を `PLATFORM_*` で囲います。

利用側も同じ条件で include / 呼び出しを行ってください。

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

int linux_only_feature_start(void);

#endif /* PLATFORM_LINUX */
```

## コンパイラ依存構文のルール

### GCC 向けの属性は COMPILER_GCC で囲う

`__attribute__((format(...)))` や `#pragma GCC diagnostic` は GCC 専用構文です。

ヘッダーやテストへ書く場合も、必ず `COMPILER_GCC` でガードします。  
コンパイラ差異を書くときは、ファイルの主題が Windows 寄りでも GCC 優先で並べます。

```c
#include <cplat/base/compiler.h>

#if defined(COMPILER_GCC)
int log_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
#else
int log_printf(const char *format, ...);
#endif /* COMPILER_ */
```

### OS 依存とコンパイラ依存を同じ分岐へ含めない

Windows であっても GCC 系を使う可能性、Linux であっても将来別処理系を使う可能性があります。

属性や pragma の差異は、OS ではなくコンパイラで判断してください。

## テスト コードのルール

### モック差異は吸収マクロや helper に限定する

プラットフォームごとにモック対象の API 名が異なる場合、テスト本体へ条件分岐を散在させず、先頭で吸収マクロを定義してから使います。

```c
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #define NATIVE_STAT_MOCK_METHOD stat
#elif defined(PLATFORM_WINDOWS)
    #define NATIVE_STAT_MOCK_METHOD stat64
#endif /* PLATFORM_ */
```

### 非対応環境のテストはテストそのものを無視する

片側プラットフォーム専用のテストは、反対側で無理に疑似実行せず、`#if defined(PLATFORM_)` を使ってコンパイル段階で除外します。  
`GTEST_SKIP()` は使用しません。

```c
#if defined(PLATFORM_LINUX)
// Linux 専用のテスト
TEST(featureTest, linux_only_behavior)
{
    /* Linux 向け確認 */
}
#endif /* PLATFORM_LINUX */
```

### OS API を直接記述する処理はテスト ヘルパーへ限定する

一時ファイル作成やパス操作のように OS API を呼び分ける必要がある場合、テスト ケース本体ではなく helper で扱います。

```c
static std::string make_temp_config_path(void)
{
#if defined(PLATFORM_LINUX)
    /* mkstemps などを使う */
    return "/tmp/example.conf";
#elif defined(PLATFORM_WINDOWS)
    /* GetTempPathA などを使う */
    return "example.conf";
#else
    return "";
#endif /* PLATFORM_ */
}
```

## レビュー時のチェックリスト

- `platform.h` / `compiler.h` の外で予約済みマクロを直接使っていないか
- OS 差異を `COMPILER_*` で、またはコンパイラ差異を `PLATFORM_*` で判定していないか
- `PLATFORM_*` の分岐順がファイルの意図と一致しているか
- `COMPILER_*` の分岐順が `GCC -> MSVC` になっているか
- 小さい差異なのに不要なファイル分割をしていないか
- 大きい差異なのに 1 ファイル内の条件分岐が過剰になっていないか
- 公開ヘッダーで export / calling convention を独自実装していないか
- 単一プラットフォーム専用 API を無条件で include / 呼び出ししていないか
- 非対応プラットフォームのテストは `#if defined(PLATFORM_)` で除外されているか
