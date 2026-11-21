---
short-title: "crt"
---

# crt - C ランタイム抽象レイヤー

`crt` は、標準 C / CRT API のプラットフォーム差異を吸収するためのレイヤーです。  
標準入口は `cplat/crt/crt.h` です。  
公開ヘッダーは標準ヘッダー名と対応するように `cplat/crt/*.h` に分割しており、必要に応じて個別に取り込むこともできます。

## 目的

Linux と Windows では、ファイル パス、セキュア関数、ワイド文字 API、時刻変換などの扱いが異なります。  
このモジュールは、呼び出し側が同じ `cplat_*` API を使えるように、その差異を集約します。

- UTF-8 パスを Windows ではワイド文字 API に変換します。
- `stdio.h`、`sys/stat.h`、`fcntl.h`、`unistd.h` 相当の API を分けて提供します。
- Windows の `HANDLE` と Linux の `fd` を薄い低レベル I/O API で共通化します。
- `string.h` / `time.h` まわりの `_s` 系差異を薄いラッパーで扱います。
- printf 形式のパス生成付き API を対応する CRT ヘッダーに配置します。

## 公開ヘッダー

- `cplat/crt/crt.h`: `crt` 配下の公開ヘッダーをまとめて取り込む標準入口
- `cplat/crt/stdio.h`: `FILE *` 操作、`remove`、`rename`、`fopen_fmt`、`remove_fmt`、切り詰めを検出する `snprintf` と `fgets`
- `cplat/crt/sys/stat.h`: `stat`、`mkdir`、`stat_fmt`、`mkdir_fmt`、ファイル種別の判定
- `cplat/crt/fcntl.h`: `open`、`open_fmt`
- `cplat/crt/file.h`: 書き込み用低レベル ファイル ハンドル、`open`、`write`、`size`、最終更新日時、`close`
- `cplat/crt/unistd.h`: `access`、`access_fmt`、`CPLAT_ACCESS_FMT_*`
- `cplat/crt/string.h`: 安全な文字列コピー/連結、再入可能な `strtok_r`、ASCII の大文字小文字を無視する比較、`sscanf` 抽象
- `cplat/crt/stdlib.h`: 環境変数の取得と設定、完全消費を検査する文字列から数値への変換
- `cplat/crt/time.h`: UTC / local 時刻変換
- `cplat/crt/path.h`: `PLATFORM_PATH_MAX`、`PLATFORM_PATH_UTF8_MAX`

## 実装方針

基礎 API とフォーマット付き API は別の実装ファイルに分けます。  
これは、フォーマット付き API のテストで下位の `cplat_fopen` や `cplat_stat` をモックできるようにするためです。

`*_fmt` API は、内部で `vsnprintf` によりパスを `PLATFORM_PATH_UTF8_MAX` のバッファーへ展開し、対応する基礎 API を呼び出します。

## 使用例

```c
#include <cplat/crt/crt.h>

FILE *fp = cplat_fopen_fmt("w", NULL, "./logs/app_%03d.txt", 7);
if (fp != NULL)
{
    (void)cplat_fprintf(fp, "hello\n");
    (void)cplat_fclose(fp, NULL);
}
```

```c
#include <cplat/crt/crt.h>

if (cplat_access_fmt(CPLAT_ACCESS_FMT_F_OK, "./logs/app_%03d.txt", 7) != 0)
{
    (void)cplat_mkdir_fmt("./logs");
}
```
