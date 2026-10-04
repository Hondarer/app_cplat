# cplat API チート シート

## 概要

本チート シートは、`app/` 配下のコードで生の CRT / POSIX / Win32 API を書こうとしたときに、対応する cplat の代替関数を素早く引くための一覧です。  
同時に、cplat が公開する API 全体を用途から引ける逆引き辞書でもあります。  
規範本文と根拠は [coding-guideline.md](coding-guideline.md) の「[ラッパーの設計方針](coding-guideline.md#ラッパーの設計方針)」「[危険な標準関数とマクロの代替](coding-guideline.md#危険な標準関数とマクロの代替)」に集約されています。  
本チート シートは対応表の抽出であり、規範としての正本は `coding-guideline.md` です。  
各機能が満たす要件と、その解決策としての機能の説明は [機能仕様](functional-spec/README.md) を参照してください。  
公開 API の宣言とシグネチャは、[`prod/include/`](../prod/include/) 配下のヘッダーを正本とします。  
コード、コーディング規範、または本チート シートに不一致がある場合は、正本を確認したうえで同じ変更の中で一致させてください。

表は 3 種類に分かれます。

- **危険な標準関数とマクロの代替**: 境界検査または失敗通知を欠く標準関数とマクロで、`app/` 配下の管理対象コードでの直接使用を禁止しています。
- **プラットフォーム抽象ラッパー**: 危険ではないものの、Linux / Windows の挙動差 (共有モード、UTF-8 パス、64bit オフセットなど) を吸収するために cplat 側で提供しているラッパーです。
- **独自機能 API**: 単一の生 API とは 1 対 1 で対応しない cplat 独自の機能です。「生の構文」列には、対応する生の関数がある場合はその名前を、ない場合は用途を記載します。

外部 OSS 由来のコード (`app/lua`、`app/sqlite`、`app/cjson`、`app/zlib`) はいずれの表も対象外です。

> [!NOTE]
> `memcpy` / `memmove` / `memset` / `strcmp` / `strncmp` / `memcmp` / 単純な `malloc` 呼出 / `printf` など、両プラットフォームで挙動が同じで境界検査の欠落もない関数には、cplat はラッパーを作りません。
> 詳細は [coding-guideline.md の「ラッパーを作らないもの」](coding-guideline.md#ラッパーを作らないもの) を参照してください。

## 危険な標準関数とマクロの代替

対象ヘッダー: `cplat/crt/string.h`、`cplat/crt/stdio.h`、`cplat/crt/stdlib.h`、`cplat/crt/select.h`、`cplat/base/error_message.h`

### 文字列操作

| 生 API | 問題 | cplat 代替 |
|---|---|---|
| `strcpy` | コピー先の容量を受け取らず、境界を検査しません。 | `cplat_strcpy(dest, dest_size, src)` |
| `strncpy` | コピー元が `count` 以上のとき null 終端しません。`count` は宛先容量ではなく最大コピー文字数 | 文字列全体をコピーするなら `cplat_strcpy`、意図的に切り詰めるなら `cplat_strncpy(dest, dest_size, src, count)` |
| `strcat` | 連結先の容量を受け取らず、境界を検査しません。 | `cplat_strcat(dest, dest_size, src)` |
| `strncat` | 連結先の容量を受け取らない。`count` は連結元から読む最大文字数であり、終端の 1 バイトも別に必要 | `cplat_strncat(dest, dest_size, src, count)` |
| `wcscpy` | `strcpy` と同じ | `cplat_wcscpy(dest, dest_size, src)` |
| `strdup` / `_strdup` | MSVC では `strdup` が非推奨 (C4996) であり名前が異なります。 | `cplat_strdup(src)` |
| `strtok` | 解析状態をライブラリ内の静的変数に持ち、再入できません。 | `cplat_strtok_r(str, delim, saveptr)` |
| `strtok_r` / `strtok_s` | Linux と Windows で名前が異なり、直接呼び出すと呼び出し側にプラットフォームごとの分岐が必要になります。 | `cplat_strtok_r(str, delim, saveptr)` |
| `gets` | 宛先の容量を指定できません。C11 で標準から削除された | `cplat_fgets(dest, dest_size, stream, detail_out)` |

Table: 危険な標準文字列操作 API と cplat 代替関数

### 書式化・行入力

| 生 API | 問題 | cplat 代替 |
|---|---|---|
| `fgets` | 切り詰めと EOF を戻り値で区別できず、改行の有無を呼び出し側が判定する必要がある | `cplat_fgets(dest, dest_size, stream, detail_out)` |
| `sprintf` / `vsprintf` | 出力先の容量を受け取らず、境界を検査しません。 | `cplat_snprintf(dest, dest_size, format, ...)` / `cplat_vsnprintf` |
| `snprintf` / `vsnprintf` | 境界は検査するが、切り詰めの検出を呼び出し側の戻り値検査に委ねている | `cplat_snprintf` / `cplat_vsnprintf` (戻り値は文字数ではなく共通結果コード) |
| `scanf` / `fscanf` / `sscanf` と各 `v*` 版 | 幅を指定しない `%s` が境界外書き込みを起こす | `cplat_scanf` / `cplat_fscanf` / `cplat_sscanf` と各 `v*` 版 |

Table: 標準の書式化・行入力 API と cplat 代替関数

### 数値変換

| 生 API | 問題 | cplat 代替 |
|---|---|---|
| `atoi` / `atol` / `atoll` / `atof` | 変換の失敗を通知せず、範囲外の入力が未定義動作になります。 | `cplat_parse_int` / `cplat_parse_int64` / `cplat_parse_double` |
| `strtol` / `strtoll` / `strtoul` / `strtoull` / `strtod` | 完全消費と `errno` の検査を呼び出し側に委ねており、検査を省略しても失敗が表面化しません。 | `cplat_parse_int` / `cplat_parse_int64` / `cplat_parse_uint64` / `cplat_parse_double` |

Table: 標準数値変換 API と cplat 代替関数

`cplat_parse_uint64` は先頭の符号 `'-'` を範囲外エラーとして拒否します。  
`strtoull` が負値を符号なしの折り返し値として受け付ける挙動とは異なります。  
幅が処理系に依存する `long` を返す変換 API は提供しません。

### エラー文字列化

| 生 API | 問題 | cplat 代替 |
|---|---|---|
| `strerror` | 戻り値の生存期間が処理系依存で、スレッド セーフとは限らない。再入可能版は `strerror_r` と `strerror_s` で名前と引数が異なります。 | `cplat_error_message(buf, buf_size, &error)` |

Table: 標準エラー文字列化 API と cplat 代替関数

### メモリ確保

| 生 API | 問題 | cplat 代替 |
|---|---|---|
| `malloc` | 単一オブジェクト、バイト バッファーの確保に使用します。長さ 0 の戻り値が処理系定義 | `cplat_malloc(size)` (ゼロ初期化しない) / `cplat_malloc_zerofill(size)` (ゼロ初期化する) |
| `calloc` | `malloc(count * size)` は乗算の回り込みを検出しません。 | `cplat_calloc(count, size)` (乗算オーバーフローを検査し、ゼロ初期化する) |
| `realloc` | 失敗時の受け方と長さ 0 の扱いを呼び出し側に委ねている | `cplat_realloc(ptr, count, size)` / `cplat_realloc_zerofill(ptr, old_count, count, size)` |
| `free` | 共有ライブラリの境界をまたぐと、確保側と解放側で C ランタイムのヒープが一致しない場合がある | `cplat_free(ptr)` |

Table: 標準メモリ確保 API と cplat 代替関数

> [!IMPORTANT]
> `cplat_realloc` / `cplat_realloc_zerofill` は要素数とサイズを分けて受け取る 3 引数 (`_zerofill` 版は 4 引数) であり、`realloc(ptr, size)` を機械的に置換すると引数の位置に不整合が生じます。
> `cplat_realloc(ptr, 0, size)` は元の領域を解放せずに NULL を返します。標準の `realloc(ptr, 0)` とは異なる扱いです。

### ファイル記述子集合

| 生 API | 問題 | cplat 代替 |
|---|---|---|
| `FD_SET` | 追加する FD が集合の容量に収まるかを検査せず、Linux では集合の領域外へ書き込む。Windows では満杯の集合への追加を通知せず破棄する | `CPLAT_FD_SET(fd, &set)` (追加できない場合は `abort()`) |

Table: ファイル記述子集合操作マクロと cplat 代替マクロ

`CPLAT_FD_SET()` が検査する上限はプラットフォームで異なります。  
Linux は FD の値が `0` 以上 `FD_SETSIZE` 未満であること、Windows は集合が保持する SOCKET の数が `FD_SETSIZE` 未満であることを検査します。

## プラットフォーム抽象ラッパー

危険関数には該当しませんが、Linux / Windows の挙動差を吸収するため、`app/` 配下のコードでは生 API ではなくラッパーを使用します。

### 標準 I/O・ファイル操作

対象ヘッダー: `cplat/crt/stdio.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `fopen` | `cplat_fopen(path, modes, detail_out)` | パスは UTF-8 を受け取り、Windows 内部で `_wfsopen` を `_SH_DENYNO` 指定で呼び出し Linux と同じ共有可の既定にします。 |
| `freopen` | `cplat_freopen(path, modes, stream, detail_out)` | `fopen` と同じ共有モード吸収。テキスト/バイナリ モード フラグは元の `stream` を引き継ぐ |
| `fclose` | `cplat_fclose(stream, detail_out)` | OS エラー詳細を `detail_out` へ格納する以外は元の戻り値規約 (0/EOF) を保持 |
| `fflush` | `cplat_fflush(stream, detail_out)` | 同上 |
| `fread` | `cplat_fread(buffer, size, count, stream, detail_out)` | 同上 |
| `fwrite` | `cplat_fwrite(buffer, size, count, stream, detail_out)` | 同上 |
| `remove` / `_wremove` | `cplat_remove(path, detail_out)` | パスは UTF-8 |
| `rename` / `_wrename` | `cplat_rename(oldpath, newpath, detail_out)` | 同上 |
| `fprintf` | `cplat_fprintf(stream, format, ...)` | 素通しに近いが `detail_out` 系 API との整合のため統一 |
| `vfprintf` | `cplat_vfprintf(stream, format, args)` | 同上 |
| `fseek` | `cplat_fseek(stream, offset, whence)` | `offset` が 64bit (`int64_t`) 対応 |
| `ftell` | `cplat_ftell(stream)` | 戻り値が 64bit (`int64_t`) 対応 |

Table: 標準 I/O・ファイル操作 API と cplat 代替ラッパーの差異

書式指定パスでファイルを開く/削除する用途には `cplat_fopen_fmt` / `cplat_vfopen_fmt` / `cplat_remove_fmt` / `cplat_vremove_fmt` を使用します。  
一意な一時ファイルの作成には `cplat_fopen_temp(prefix, modes, path_out, path_size, detail_out)` を使用します。  
`stdio` ラッパーとメモリ マップド ファイル (後述) のどちらを使うべきかの選定基準は [fileio-api-selection-guideline.md](fileio-api-selection-guideline.md) を参照してください。

### 低レベル fd 操作

対象ヘッダー: `cplat/crt/fcntl.h`、`cplat/crt/unistd.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `open` / `_wopen` | `cplat_open(path, flags, mode, detail_out)` | パスは UTF-8。Windows は `_wsopen_s` を `_SH_DENYNO` 指定で呼び出し Linux と同じ既定にします。 |
| `lseek` / `_lseeki64` | `cplat_lseek(fd, offset, whence, detail_out)` | `offset` が 64bit 対応。`fd` が負の場合は OS API を呼ばず -1 を返す |
| `close` / `_close` | `cplat_close(fd, detail_out)` | `fd` が負の場合は OS API を呼ばず -1 を返す |
| `dup` / `_dup` | `cplat_dup(fd, detail_out)` | 同上 |
| `dup2` / `_dup2` | `cplat_dup2(oldfd, newfd, detail_out)` | 成功時の戻り値を Windows の `_dup2` に合わせて常に 0 とする (POSIX `dup2` は `newfd` を返す) |
| `read` / `_read` | `cplat_read(fd, buf, count, detail_out)` | Windows では 1 回の呼び出しで読み取れる上限が `INT_MAX` バイトであり、超過分は切り詰められる |
| `write` / `_write` | `cplat_write(fd, buf, count, detail_out)` | 同上 (書き込み上限) |
| `access` / `_waccess` | `cplat_access(path, mode, detail_out)` | パスは UTF-8 |
| `isatty` | `cplat_isatty(stream)` | 引数がファイル記述子ではなく `cplat_stream` 列挙型。Windows は `GetFileType`/`GetConsoleMode` の組み合わせで判定 |

Table: 低レベル ファイル記述子操作 API と cplat 代替ラッパーの差異

書式指定パスで開く/アクセス確認する用途には `cplat_open_fmt` / `cplat_vopen_fmt` / `cplat_access_fmt` / `cplat_vaccess_fmt` を使用します。

### ファイル システム

対象ヘッダー: `cplat/crt/sys/stat.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `stat` / `_wstat64` | `cplat_stat(buf, detail_out, path)` | パスは UTF-8。`buf` の型は `cplat_file_stat_t` (Linux: `struct stat`、Windows: `struct _stat64`)。Windows の `st_atime` / `st_mtime` / `st_ctime` は UTC の Unix 秒 |
| `mkdir` / `_wmkdir` | `cplat_mkdir(path, detail_out)` | パスは UTF-8 |
| (`mkdir -p` 相当) | `cplat_makedirs(path, detail_out)` | 中間ディレクトリを再帰的に作成。既存ディレクトリはべき等に成功扱い |
| `rmdir` / `_wrmdir` | `cplat_rmdir(path, detail_out)` | パスは UTF-8。中間ディレクトリの再帰削除はしません。 |
| `S_ISREG(st.st_mode)` / `(st.st_mode & _S_IFMT) == _S_IFREG` | `cplat_file_stat_is_regular(file_stat)` | 通常ファイルなら 1、それ以外と NULL は 0。値だけを参照するため共通結果コードの適用対象外 |

Table: ファイル システム操作 API と cplat 代替ラッパーの差異

最終更新日時の取得と設定は `cplat/crt/file.h` の `cplat_file_get_modified_timestamp` / `cplat_file_set_modified_timestamp` (ハンドル版) と `cplat_file_get_path_modified_timestamp` / `cplat_file_set_path_modified_timestamp` (パス版) を使用します。  
`cplat_stat` の `st_mtime` が秒精度であるのに対し、これらは `cplat_timespec` でサブ秒も扱います。秒部は同じ Unix epoch UTC です。

書式指定パスを使う用途には `cplat_stat_fmt` / `cplat_vstat_fmt` / `cplat_mkdir_fmt` / `cplat_vmkdir_fmt` を使用します。

### 環境変数

対象ヘッダー: `cplat/crt/stdlib.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `getenv` | `cplat_getenv(name, buf, buf_size, exists_out, detail_out)` | Windows は `_dupenv_s` を使用し MSVC セキュリティ警告を回避。`buf` に NULL を渡すと存在確認のみ行う |
| `setenv` | `cplat_setenv(name, value, overwrite, detail_out)` | Linux は `setenv`、Windows は `_putenv_s` を使用。設定は呼び出し元プロセスにのみ反映 |
| `unsetenv` | `cplat_unsetenv(name, detail_out)` | Linux は `unsetenv`、Windows は値に空文字列を指定した `_putenv_s` を使用 |

Table: 環境変数操作 API と cplat 代替ラッパーの差異

### 大文字小文字を無視する比較

対象ヘッダー: `cplat/crt/string.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `strcasecmp` / `_stricmp` | `cplat_strcasecmp(lhs, rhs)` | 名前とヘッダーが異なります。cplat は ASCII の `A`〜`Z` だけを畳み、`setlocale` に依存しません。戻り値は -1 / 0 / 1 |
| `strncasecmp` / `_strnicmp` | `cplat_strncasecmp(lhs, rhs, count)` | 同上。先頭 `count` バイトまで比較します。 |

Table: 大文字小文字を無視する文字列比較 API と cplat 代替ラッパーの差異

`strcmp` / `strncmp` / `memcmp` にはラッパーを作りません。

### 時刻変換

対象ヘッダー: `cplat/crt/time.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `gmtime` (非再入版) | `cplat_gmtime(utc_tm, timep)` | Linux は `gmtime_r`、Windows は `gmtime_s` を使用しスレッド セーフにします。 |
| `localtime` (非再入版) | `cplat_localtime(local_tm, timep)` | Linux は `localtime_r`、Windows は `localtime_s` を使用 |
| `ctime` (非再入版) | `cplat_ctime(buf, buf_size, timep)` | Linux は `ctime_r`、Windows は `ctime_s` を使用。`buf_size` は 26 以上が必要 |

Table: 時刻変換 API と cplat 代替ラッパーの差異

### Win32 UTF-8 ラッパー (Windows 専用)

対象ヘッダー: `cplat/win32/win32.h`

Win32 API はネイティブでは ANSI (現在のコード ページ) または UTF-16 のいずれかを扱い、UTF-8 を直接受け取りません。  
`*U` サフィックスのラッパーは、UTF-8 文字列と Win32 の UTF-16 API の境界を吸収します。

| 生 API (ANSI/Wide 版) | cplat 代替 (UTF-8 版) |
|---|---|
| `CreateFileA` / `CreateFileW` | `CreateFileU` |
| `CreateNamedPipeA` / `CreateNamedPipeW` | `CreateNamedPipeU` |
| `GetModuleFileNameA` / `GetModuleFileNameW` | `GetModuleFileNameU` |
| `WriteConsoleA` / `WriteConsoleW` | `WriteConsoleU` |
| `GetVolumePathNameA` / `GetVolumePathNameW` | `GetVolumePathNameU` |
| `GetVolumeInformationA` / `GetVolumeInformationW` | `GetVolumeInformationU` |
| `LoadLibraryA` / `LoadLibraryW` | `LoadLibraryU` |
| `CreateProcessA` / `CreateProcessW` | `CreateProcessU` |
| `OpenSCManagerA` / `OpenSCManagerW` | `OpenSCManagerU` |
| `CreateServiceA` / `CreateServiceW` | `CreateServiceU` |
| `OpenServiceA` / `OpenServiceW` | `OpenServiceU` |
| `ChangeServiceConfig2A` / `ChangeServiceConfig2W` | `ChangeServiceConfig2U` |
| `RegisterServiceCtrlHandlerExA` / `RegisterServiceCtrlHandlerExW` | `RegisterServiceCtrlHandlerExU` |
| `StartServiceCtrlDispatcherA` / `StartServiceCtrlDispatcherW` | `StartServiceCtrlDispatcherU` (サービス エントリは `cplat_service_entry_u` を使用) |

Table: Win32 生 API と cplat UTF-8 ラッパー API の対応

適用範囲は `CreateFileU` などのファイル ハンドル系に限らず、`app/cplat/docs/coding-guideline.md` の該当節を参照してください。

### クロック

対象ヘッダー: `cplat/clock/clock.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `clock_gettime(CLOCK_MONOTONIC, ...)` (Linux) / `GetTickCount64()` (Windows) | `cplat_clock_get_monotonic_ms()` / `cplat_clock_get_monotonic()` | 単調クロックをミリ秒/ナノ秒精度で返す。Windows の分解能差 (既定 ~15ms 刻み) を吸収 |
| `clock_gettime(CLOCK_REALTIME, ...)` (Linux) / `GetSystemTimeAsFileTime()` (Windows) | `cplat_clock_get_realtime()` | Windows の 1601 年基準 (FILETIME) から Unix epoch 基準への変換を吸収 |

Table: クロック取得 API と cplat 代替ラッパーの差異

### メモリ マップド ファイル

対象ヘッダー: `cplat/mmap/mmap.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `mmap()` (POSIX) / `CreateFileMapping` +`MapViewOfFile` (Win32) | `cplat_mmap_attach(...)` | ファイル オープンとマッピングを一括し、新規作成時のサイズ指定と所有ハンドル化を行います。 |
| `munmap()` (POSIX) / `UnmapViewOfFile` +`CloseHandle` (Win32) | `cplat_mmap_detach(...)` | マップと所有ハンドルをまとめて解放します。 |
| `msync(MS_SYNC)` (POSIX) / `FlushViewOfFile` +`FlushFileBuffers` (Win32) | `cplat_mmap_flush(...)` | - |

Table: メモリ マップド ファイル API と cplat 代替ラッパーの差異

`stdio` ラッパーとの使い分けは [fileio-api-selection-guideline.md](fileio-api-selection-guideline.md) を参照してください。

### ネットワーク バイト オーダー

本節、「IPv4 アドレス」節、「ソケット」節が扱う `net` カテゴリの要件と機能は [ネットワーク (net) 機能仕様](functional-spec/net.md)、公開面の方針は [コーディング規範](coding-guideline.md#net-カテゴリの公開面の方針) を参照してください。

対象ヘッダー: `cplat/net/byteorder.h`

| 生 API | cplat 代替 |
|---|---|
| `htons` | `cplat_hton16` |
| `ntohs` | `cplat_ntoh16` |
| `htonl` | `cplat_hton32` |
| `ntohl` | `cplat_ntoh32` |
| `htobe64` (glibc) | `cplat_hton64` |
| `be64toh` (glibc) | `cplat_ntoh64` |

Table: ネットワーク バイト オーダー変換 API と cplat 代替関数

`cplat_hton16`/`hton32`/`hton64` 系はシフト演算とバイト列再構成のみで実装され、ソケット ヘッダーへの依存を持ちません。

### IPv4 アドレス

対象ヘッダー: `cplat/net/endpoint.h`

| 生 API | cplat 代替 |
|---|---|
| `inet_pton(AF_INET, ...)` | `cplat_ipv4_parse(...)` |
| `getaddrinfo()` | `cplat_ipv4_resolve(...)` |
| `inet_ntop(AF_INET, ...)` | `cplat_ipv4_to_string(...)` |

Table: IPv4 アドレス操作 API と cplat 代替関数

`struct sockaddr_in` は公開面に出さず、独自型 `cplat_ipv4_endpoint` (ネットワーク バイト オーダーの address/port) で置換します。

### ソケット

対象ヘッダー: `cplat/net/socket.h`

IPv4 ソケットの生成、接続、送受信、待機は cplat のソケット API を使用します。

| 生 API | cplat 代替 |
|---|---|
| `socket()` | `cplat_socket_open(...)` |
| `close()` (POSIX) / `closesocket()` (Winsock) | `cplat_socket_close(...)` |
| `shutdown(SHUT_RDWR)` | `cplat_socket_shutdown(...)` |
| `bind()` | `cplat_socket_bind(...)` |
| `listen()` | `cplat_socket_listen(...)` |
| `accept()` | `cplat_socket_accept(...)` |
| `connect()` | `cplat_socket_connect(...)` (`EINPROGRESS`/`WSAEWOULDBLOCK` を `CPLAT_ERR_IN_PROGRESS` に統一) |
| `getsockopt(SO_ERROR)` | `cplat_socket_get_pending_error(...)` |
| `fcntl(O_NONBLOCK)` (POSIX) / `ioctlsocket(FIONBIO)` (Winsock) | `cplat_socket_set_nonblocking(...)` |
| `setsockopt(SO_REUSEADDR)` | `cplat_socket_set_reuse_address(...)` |
| `setsockopt(SO_BROADCAST)` | `cplat_socket_set_broadcast(...)` |
| `setsockopt(IP_MULTICAST_IF)` | `cplat_socket_set_multicast_interface(...)` |
| `setsockopt(IP_ADD_MEMBERSHIP)` / `IP_DROP_MEMBERSHIP` | `cplat_socket_join_multicast_group(...)` / `cplat_socket_leave_multicast_group(...)` |
| `send()` | `cplat_socket_send(...)` |
| `recv()` | `cplat_socket_recv(...)` |
| `sendto()` | `cplat_socket_sendto(...)` |
| `recvfrom()` | `cplat_socket_recvfrom(...)` |
| `poll()` (Linux) / `WSAPoll()` (Winsock) | `cplat_socket_wait_readable(...)` / `cplat_socket_wait_writable(...)` / `cplat_socket_wait_readable_multi(...)` |

Table: ソケット操作 API と cplat 代替関数

`cplat_socket_send_all`/`cplat_socket_recv_all` は複数回の `send`/`recv` をループさせる合成 API です。  
Linux の `cplat_socket_send`/`cplat_socket_send_all` は、切断済みの接続への送信による SIGPIPE を送信単位で抑制し、送信エラーは結果コードと `cplat_error` で通知します。  
`cplat_socket_shutdown_receive` は Linux (`shutdown(SHUT_RD)`) と Windows (実質 close 相当) で意味論が異なる差異を吸収した複合 API です。

### プロセス内メモリ ロック

対象ヘッダー: `cplat/runtime/memory_lock.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `mlock()` (POSIX) / `VirtualLock()` (Win32) | `cplat_memory_lock_range(...)` | - |
| `munlock()` (POSIX) / `VirtualUnlock()` (Win32) | `cplat_memory_unlock_range(...)` | - |
| `explicit_bzero()` (glibc) / `SecureZeroMemory()` (Win32) | `cplat_secure_zero(...)` | コンパイラによるデッド ストア除去が起きないことを保証します。単なる `memset` の代替ではありません。 |
| `mlockall()` (Linux) | `cplat_memory_lock_self(...)` | Windows には相当 API が無いため、`VirtualQuery` で列挙し各領域へ `VirtualLock` を適用する合成実装 (参照カウント管理付き) |
| `munlockall()` (Linux) | `cplat_memory_lock_scope_release(...)` | Windows では各範囲へ `VirtualUnlock` を参照カウント管理で適用 |

Table: プロセス内メモリ ロック API と cplat 代替ラッパーの差異

scope API の設計や結果コードの詳細は [memory-lock.md](memory-lock.md) を参照してください。

### ホスト情報

対象ヘッダー: `cplat/runtime/host.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `gethostname()` (POSIX) / `GetComputerNameExW(ComputerNameDnsHostname)` (Win32) | `cplat_host_get_name(name_out, name_size)` | 返る値は UTF-8 です。Windows は Winsock を使わず DNS ホスト名を取得します。FQDN であることは保証しません。推奨配列サイズは `CPLAT_HOST_NAME_MAX` |

Table: ホスト情報取得 API と cplat 代替ラッパーの差異

### 表示言語

対象ヘッダー: `cplat/locale/ui_language.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `getenv("LC_ALL" / "LC_MESSAGES" / "LANG")` (POSIX) / `GetUserPreferredUILanguages(MUI_LANGUAGE_NAME)` (Win32) | `cplat_ui_language_get_tag(tag_out, tag_size)` | 環境変数を両プラットフォームで優先し、Windows では環境変数で決定できない場合に OS の表示言語を使用します。`C` と `POSIX` の指定と、決定できない場合はニュートラル (空文字列) です。推奨配列サイズは `CPLAT_UI_LANGUAGE_TAG_MAX` |

Table: 表示言語取得 API と cplat 代替ラッパーの差異

返る言語タグは、言語を小文字、表記体系を先頭だけ大文字、地域を大文字にした `ja`、`ja-JP`、`zh-Hans-CN` の表記です。  
書式に使用する地域設定 (`GetUserDefaultLocaleName`) ではなく表示言語を対象とし、プロセスのロケール設定 (`setlocale`) は変更しません。  
結果は保持しないため、プロセス内で使い続ける値は利用側で保持します。

### モジュール / プロセス情報

対象ヘッダー: `cplat/runtime/module.h`、`cplat/runtime/process.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `dladdr()` (POSIX) / `GetModuleHandleEx` +`GetModuleFileName` (Win32) | `cplat_module_get_path(...)` | 関数アドレスから所属モジュールの完全なパスを取得 |
| `readlink("/proc/self/exe")` (Linux) / `GetModuleFileName(NULL, ...)` (Win32) | `cplat_process_get_executable_path(...)` | - |
| `getpid()` (POSIX) / `GetCurrentProcessId()` (Win32) | `cplat_process_get_pid()` | - |
| `gettid()` (Linux) / `GetCurrentThreadId()` (Win32) | `cplat_process_get_tid()` | 診断目的の識別子です。glibc 2.28 でも動作するよう、Linux では `syscall(SYS_gettid)` を使用します |
| `fork()` +`execve()` (POSIX) / `CreateProcess()` (Win32) | `cplat_process_start(...)` | stdio リダイレクト、環境変数上書き、作業ディレクトリ指定を共通オプション構造体 `cplat_process_options` に集約 |
| `waitpid()` (POSIX) / `WaitForSingleObject()` (Win32) | `cplat_process_wait(...)` | - |
| `WEXITSTATUS(status)` (POSIX) / `GetExitCodeProcess()` (Win32) | `cplat_process_get_exit_code(...)` | - |
| `kill(SIGKILL)` (POSIX) / `TerminateProcess()` (Win32) | `cplat_process_terminate(...)` | - |

Table: モジュール・プロセス情報取得 API と cplat 代替ラッパーの差異

`cplat_process_dispose`/`cplat_process_run_sync` はハンドル破棄、`start` +`wait` +`get_exit_code` の合成 API です。

### 暗号論的乱数

対象ヘッダー: `cplat/crypto/random.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `RAND_bytes()` (OpenSSL, Linux) / `BCryptGenRandom()` (CNG, Windows) | `cplat_random_bytes(...)` | 全バイト充足を保証 (部分成功なし)。`rand()` のような予測可能な擬似乱数の使用は明示的に禁止 |

Table: 暗号論的乱数生成 API と cplat 代替ラッパーの差異

### 暗号化・復号

対象ヘッダー: `cplat/crypto/crypto.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `EVP_EncryptInit_ex` +`EVP_EncryptUpdate` +`EVP_EncryptFinal_ex` (OpenSSL) / `BCryptEncrypt` (CNG) | `cplat_crypto_encrypt(...)` | AES-256-GCM 固定のワンショット API に集約し、暗号文+タグ連結フォーマットを標準化 |
| `EVP_DecryptInit_ex` +`EVP_DecryptUpdate` +`EVP_DecryptFinal_ex` (OpenSSL) / `BCryptDecrypt` (CNG) | `cplat_crypto_decrypt(...)` | 同上。タグ検証を内包 |

Table: 暗号化・復号 API と cplat 代替ラッパーの差異

### 正規表現

対象ヘッダー: `cplat/regex/regex.h`

POSIX の照合 3 関数は、UTF-8 文字列を扱う cplat の正規表現 API へ置き換えます。

| 生 API | cplat 代替 |
|---|---|
| `regcomp()` | `cplat_regex_create(...)` |
| `regexec()` | `cplat_regex_search(...)` (部分一致) / `cplat_regex_matches(...)` (全体一致) |
| `regfree()` | `cplat_regex_dispose(...)` |

Table: 正規表現操作 API と cplat 代替関数

置換 (`cplat_regex_replace`)、イテレーター (`cplat_regex_iter_*`)、分割 (`cplat_regex_split`) は POSIX regex にない機能で、対応する単一の生 API はありません。  
フラグの対応関係、文字モデル、制限事項の詳細は [正規表現 (regex) 機能仕様](functional-spec/regex.md) を参照してください。

### コンソール制御 (Windows 専用)

対象ヘッダー: `cplat/console/console.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `SetConsoleOutputCP` +`SetConsoleCP` +`SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)` (Win32) | `cplat_console_init(...)` | Linux では無処理。UTF-8 出力と VT100 制御シーケンスを有効化する複数 API 呼び出しを集約 |
| `AttachConsole()` (Win32) | `cplat_console_attach_parent(...)` | UAC 昇格後の親コンソール再接続に特化 |
| `WriteConsoleA` (Win32) / `write(fd, ...)` (POSIX) | `cplat_console_write(...)` | CRT ストリーム経由の `printf`/`fprintf` が失敗する既知の問題を回避するための直接書き込み |

Table: Windows コンソール制御 API と cplat 代替ラッパーの差異

`cplat_console_dispose()` は `cplat_console_init` が変更したコード ページ/モードを元に戻す後始末です。

### 圧縮・展開

対象ヘッダー: `cplat/compress/compress.h`

| 生 API | cplat 代替 | 差異の要点 |
|---|---|---|
| `deflateInit2` +`deflate` +`deflateEnd` (両 OS 共通の app/zlib) | `cplat_compress(...)` | ワンショット API に集約。先頭 8 バイトへ元サイズ (ネットワーク バイト オーダー) を付加する独自フォーマットで統一 |
| `inflateInit2` +`inflate` +`inflateEnd` (両 OS 共通の app/zlib) | `cplat_decompress(...)` | 同上 |

Table: 圧縮・展開 API と cplat 代替ラッパーの差異

### DLL エクスポート マクロ (Windows 専用)

対象ヘッダー: `cplat/base/dll_exports.h`

関数ではなくプリプロセッサ マクロですが、生のコンパイラ拡張構文を直接使わせず経由させる 1 対 1 の対応です。

| 生の構文 | cplat 代替 |
|---|---|
| `__declspec(dllexport)` / `__declspec(dllimport)` (Win32)、`__attribute__((visibility("default")))` (Linux/GCC) | `CPLAT_DLL_EXPORT(prefix)` |
| `__stdcall` (Win32) | `CPLAT_DLL_API(prefix)` (Linux では空) |

Table: DLL エクスポート・インポート構文と cplat マクロの対応

リンク方式や動的ライブラリの成果物構成は [link-policy.md](link-policy.md) を参照してください。

### プラットフォームとアーキテクチャーの検出

本節と「コンパイラの検出とインライン制御」節が扱う分岐の書き方の規範は [platform-abstraction-guideline.md](platform-abstraction-guideline.md) を参照してください。

対象ヘッダー: `cplat/base/platform.h`

cplat は 64 ビット環境専用です。  
`platform.h` はポインターの幅を `static_assert` で検査し、32 ビット環境ではコンパイル時にエラーとします。

| 生の構文 | cplat 代替 |
|---|---|
| `#if defined(__linux__)` | `#ifdef PLATFORM_LINUX` |
| `#if defined(_WIN32) \|\| defined(_WIN64)` | `#ifdef PLATFORM_WINDOWS` |
| プラットフォーム名を文字列で得たい (`#if` で分岐して個別にリテラルを書く) | `PLATFORM_NAME` ("Windows"/"Linux"/"Unknown") |
| アーキテクチャー名を文字列で得たい | `ARCH_NAME` ("x64"/"Unknown") |

Table: プラットフォームおよびアーキテクチャー検出マクロの対応

### コンパイラの検出とインライン制御

対象ヘッダー: `cplat/base/compiler.h`

| 生の構文 | cplat 代替 |
|---|---|
| `#if defined(_MSC_VER)` | `#ifdef COMPILER_MSVC` |
| `#if defined(__GNUC__) && !defined(__clang__)` | `#ifdef COMPILER_GCC` |
| コンパイラ名/バージョンを文字列や数値で得たい | `COMPILER_NAME` / `COMPILER_VERSION` |
| `__attribute__((always_inline))` (GCC) / `__forceinline` (MSVC) | `FORCE_INLINE` |
| `__attribute__((noinline))` (GCC) / `__declspec(noinline)` (MSVC) | `NO_INLINE` |
| `_Thread_local` (C, GCC) / `thread_local` (C++, GCC) / `__declspec(thread)` (MSVC) | `THREAD_LOCAL` |

Table: コンパイラ検出およびインライン制御マクロの対応

### Windows SDK の取り込み順序 (Windows 専用)

対象ヘッダー: `cplat/base/windows_sdk.h`

| 生の構文 | cplat 代替 | 差異の要点 |
|---|---|---|
| `#include <windows.h>` を先に取り込む | `#include <cplat/base/windows_sdk.h>` | `winsock2.h`/`ws2tcpip.h` を `windows.h` より先に取り込む正しい順序を保証し、レガシ `winsock.h` との衝突を防ぐ |

Table: Windows SDK ヘッダー取り込み構文と cplat ヘッダーの差異

## 独自機能 API (単一の生 API と 1 対 1 対応しない)

以下は cplat 独自の機能で、単一の生 API とは 1 対 1 で対応しません。  
「生の構文」列には、対応する生の関数がある場合はその名前を、ない場合は用途を記載します。  
「生の構文」列を主キーとした逆引きはできませんが、cplat が公開する API を用途から引けるようにするため掲載します。

### ハッシュ テーブル

対象ヘッダー: `cplat/hashtable/hashtable.h`

通常の `cplat_hashtable_create` は、構築時に指定したレコード数とストレージ容量を固定します。  
内部確保した領域を自動拡張する場合は、`cplat_hashtable_growth_config` で上限を指定し、`cplat_hashtable_create_growable` で構築します。  
上限の 0 は上限なしです。

自動拡張版では、`cplat_hashtable_add` と `cplat_hashtable_upsert` がレコード数、キー ストレージ、値ストレージを拡張します。  
`cplat_hashtable_update` と `cplat_hashtable_update_rec` は値ストレージだけを拡張します。  
断片化だけが原因なら、容量を増やさず同容量で再構築します。  
`cplat_hashtable_insert_direct`、外部領域、`cplat_hashtable_attach` は自動拡張の対象外です。

自動再構築が発生した書き込みでは、取得済みの設定、キー、値、管理領域、データ領域への参照がすべて無効になります。  
永続化した領域を `cplat_hashtable_attach` で再接続しても、自動拡張設定は復元されません。

固定レコード数、固定ストレージ容量、遅延削除を持つハッシュ テーブルです。  
単一の標準 API とは対応しません。

キーと値は個別に固定長バイナリ、固定長 NUL 終端文字列、可変長 NUL 終端文字列を選択できます。  
通常の `cplat_hashtable_create` では、可変長文字列の容量を `key_storage_size` または `value_storage_size` で指定し、自動拡張しません。  
断片化で連続領域が不足すると、合計空き容量が足りていても `CPLAT_ERR_STORAGE_FULL` です。  
必要に応じて `cplat_hashtable_compact` を明示的に呼び出してから再試行します。  
格納先の確保は、空き領域を管理する空きリストの先着適合で行い、空きブロックの個数を H として O(H) です。  
断片化がなければ H は 1 のため、実質 O(1) で完了します。  
格納済みブロックの配置は、`cplat_hashtable_compact`、`cplat_hashtable_resize`、`cplat_hashtable_rebuild_into`、または自動再構築で変わります。  
`cplat_hashtable_compact` は同一のストレージ内で、ほかの操作は移行先の領域へ詰め直します。  
確保は空き領域だけを使うため、対象レコード以外のブロックを動かしません。  
取得済みの可変長参照が無効になるのは、そのフィールドの更新・回収・再利用、`cplat_hashtable_compact`、`cplat_hashtable_clear`、`cplat_hashtable_resize`、`cplat_hashtable_rebuild_into`、自動再構築、`cplat_hashtable_dispose` です。  
空きリストは可変長ストレージ 1 個につき `capacity + 1` 要素を占め、必要バッファー サイズに含まれます。  
必要バッファー サイズは `cplat_hashtable_required_size` で求めてください。  
ストレージの管理方式は [hashtable 可変長ストレージの管理方式](doxybook2_internal/Files/libsrc/cplat/hashtable/hashtable-storage-allocator.md) を参照してください。  
固定長バイナリ値は `value_size` バイトのバイト列として `memcpy` で授受します。  
値の格納境界は設定の `value_align` で決まります。  
既定の `0` では値を隙間なく並べ、値への参照を返す API も型のアラインメントを保証しないため、型付きポインターとして直接参照せず `memcpy` で取り出します。  
`value_align` に非 0 (2 の冪、`CPLAT_HASHTABLE_VALUE_ALIGN_MAX` 以下) を指定すると、値をその境界へ整列させ、`buf_data` にも同じ境界を要求します。  
この場合に限り、値への参照を境界を満たす型のポインターとして直接参照できます。  
可変長値では `value_align` は `0` でなければなりません。

固定長文字列を追加または更新するときは、NUL 終端文字列をそのまま渡します。  
未使用部分は内部で 0 埋めするため、呼び出し側で固定長まで fill する必要はありません。

可変長フィールドへの参照は、そのフィールドの更新、回収、再利用、または `compact`、`clear`、`dispose` まで有効です。  
別のレコードや別のフィールドを更新しただけでは無効になりません。  
安全に保持する場合は、固定長文字列と可変長文字列のどちらも `*_copy` を使います。  
`dest == NULL`、`dest_size == 0` で NUL を含む必要量を照会できます。  
固定長バイナリも `*_copy` で取得でき、必要量は設定した固定サイズです。

テーブルは変更時刻と世代カウンターの両方を持ちます。  
変更時刻は実時刻のため時計の巻き戻しで逆行しますが、世代カウンターは変更のたびに 1 ずつ増える単調な値です。  
変更の前後関係を判定する場合は世代カウンターを比較し、変更時刻は表示と外部システムとの相関に使います。

レコード番号は内部スロットの 1 相対番号であり、有効期間を持つ内部キーです。  
永続化した番号を別の機会に突き合わせる用途は対象外です。  
`cplat_hashtable_delete` で直ちに空へ戻ったレコード、`cplat_hashtable_push_deleted` で寿命に到達したレコード、`cplat_hashtable_purge_deleted` の削除済みレコード、`cplat_hashtable_clear` のすべて、`reuse_deleted` が非 0 のときに `cplat_hashtable_add` が追い出した削除済みレコード、および `cplat_hashtable_dispose` をまたぐと、番号は無効になりえます。  
`cplat_hashtable_find_recno` で得た番号は、これらを挟まない範囲で使います。  
空へ戻ったスロットと追い出されたスロットは、別のキーへ再利用されます。

| 用途 | cplat API |
|---|---|
| 必要バッファー サイズを求める | `cplat_hashtable_required_size` / `cplat_hashtable_buffer_size` |
| 管理中バッファーの先頭を得る | `cplat_hashtable_buffer_ref` |
| 構築 / 自動拡張版の構築 / 再接続 / 破棄 | `cplat_hashtable_create` / `cplat_hashtable_create_growable` / `cplat_hashtable_attach` / `cplat_hashtable_dispose` |
| 設定を読む | `cplat_hashtable_get_config_ref` / `cplat_hashtable_get_config_val` |
| 追加 / 更新 / 削除 | `cplat_hashtable_add` / `cplat_hashtable_update` / `cplat_hashtable_update_rec` / `cplat_hashtable_delete` / `cplat_hashtable_delete_rec` |
| 無ければ追加、あれば更新 | `cplat_hashtable_upsert`。新規追加か既存更新かは `inserted_out` で返る。重複も未登録もエラーにしない |
| 削除済み同一キーの追加方針 | `CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE` は新しい値で復活 / `CPLAT_HASHTABLE_ADD_DELETED_REVIVE` は削除前の値で復活 |
| 空きがない場合に削除済みを再利用 | 設定 `reuse_deleted` を非 0 にする。既定値は 0 |
| レコード番号を指定して直接書き込む | `cplat_hashtable_insert_direct` |
| 検索 | `cplat_hashtable_find_value_ref` / `cplat_hashtable_find_value_copy` / `cplat_hashtable_find_recno` / `cplat_hashtable_find_timestamp_ref` / `cplat_hashtable_find_timestamp_val` |
| レコード番号から読む | `cplat_hashtable_get_key_ref` / `cplat_hashtable_get_key_copy` / `cplat_hashtable_get_value_ref` / `cplat_hashtable_get_value_copy` / `cplat_hashtable_get_status` / `cplat_hashtable_get_timestamp_ref` / `cplat_hashtable_get_timestamp_val` |
| テーブル横断の変更時刻 | `cplat_hashtable_get_table_timestamp_ref` / `cplat_hashtable_get_table_timestamp_val` |
| 世代カウンターを読む | `cplat_hashtable_get_table_generation` / `cplat_hashtable_get_generation` / `cplat_hashtable_find_generation` |
| タイムスタンプと世代カウンターの粒度 | `CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE` / `CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD`。テーブル横断の世代カウンターは粒度に関わらず常に有効 |
| 固定長値の格納境界 | 設定 `value_align`。上限は `CPLAT_HASHTABLE_VALUE_ALIGN_MAX` |
| 占有状況 | `cplat_hashtable_count_status` / `cplat_hashtable_count` / `cplat_hashtable_deleted_count` / `cplat_hashtable_empty_count`。永続化領域の保持カウンターから取得するため、スロットを走査せず O(1) で完了 |
| 状態で絞って走査する | `cplat_hashtable_next_record`。`CPLAT_HASHTABLE_SCAN_IN_USE` / `_DELETED` / `_EMPTY` のビット和で対象を選ぶ。列挙の終了はエラーではなく `has_record_out` が 0 |
| レコード数とストレージ容量を変える | 内部確保は `cplat_hashtable_resize`、外部領域は `cplat_hashtable_rebuild_into` |
| 削除の加齢と回収 | `cplat_hashtable_push_deleted` / `cplat_hashtable_purge_deleted` / `cplat_hashtable_clear` |
| 可変長ストレージの明示的な圧縮 | `cplat_hashtable_compact`。キーと値を一括して圧縮し、取得済みの可変長参照を無効化。capacity を N、空きブロックの個数を H、使用バイト数を U として O(N log H + U) |
| 寿命無限 | `CPLAT_HASHTABLE_LIFETIME_INFINITE` |
| 整合性検査 | `cplat_hashtable_validate` |

Table: ハッシュ テーブル操作における用途別 cplat API

`cplat_hashtable_add` と `cplat_hashtable_insert_direct` は、既存キーとの衝突を `CPLAT_ERR_DUPLICATE_KEY` で通知します。  
`cplat_hashtable_insert_direct` の指定スロットが空でない場合は、`CPLAT_ERR_DUPLICATE_DEFINITION` です。

`cplat_hashtable_attach` は、呼び出し引数の不正を `CPLAT_ERR_INVALID_ARGUMENT`、領域の容量不足を `CPLAT_ERR_BUFFER_TOO_SMALL`、保存ヘッダーの版の不一致を `CPLAT_ERR_VERSION_MISMATCH`、それ以外の保存ヘッダーの不正を `CPLAT_ERR_CORRUPT_DESCRIPTOR` で通知します。

`cplat_hashtable_resize` と `cplat_hashtable_rebuild_into` で変えられるのは、`capacity`、`key_storage_size`、`value_storage_size` の 3 つだけです。  
ほかの項目が現在の設定と異なる場合は `CPLAT_ERR_INVALID_ARGUMENT` です。  
拡大ではレコード番号を保存し、縮小では保存しません。縮小後は `cplat_hashtable_find_recno` で取り直します。  
レコードは 1 件も捨てません。使用中が収まらない場合、および `reuse_deleted` が `0` で削除済みが収まらない場合は `CPLAT_ERR_LIMIT_EXCEEDED` です。  
`reuse_deleted` が非 0 のときだけ、`cplat_hashtable_add` の追い出しと同じ規則で削除済みを古い順に空へ戻します。  
可変長ストレージは移行時に詰め直し、詰めた後の使用量が新しい容量を超える場合は `CPLAT_ERR_STORAGE_FULL` です。  
`cplat_hashtable_resize` は内部確保のテーブル専用で、外部領域のテーブルには `CPLAT_ERR_UNSUPPORTED` を返します。

### 文字列カタログ

対象ヘッダー: `cplat/string_catalog/string_catalog.h`、`cplat/string_catalog/catalog.h`、`cplat/string_catalog/argument.h`、`cplat/string_catalog/language.h`

文字列キーごとに引数の型と項目メタデータ、言語別の文言を定義し、実行時に値を差し込んで文字列を組み立てます。  
言語別のリソースには語順だけを持たせ、値の表現は引数の種別が決めます。単一の標準 API とは対応しません。

カタログはライブラリが保持しません。利用側が `cplat_string_catalog_entry` の配列と任意のインデックス表を `cplat_string_catalog` にまとめ、呼び出しごとに渡します。  
配列と項目が参照する引数定義、説明文は複製せず参照だけを保持するため、指す領域はカタログを使用する間ずっと有効である必要があります。

| 用途 | cplat の API |
|---|---|
| 出力言語の設定 | `cplat_string_catalog_set_language` |
| 出力言語の取得 | `cplat_string_catalog_get_language` |
| 言語タグから出力言語への対応付け | `cplat_string_catalog_language_from_tag` |
| 文字列の組み立て | `cplat_string_catalog_format` |
| 文字列の組み立て (`va_list` 版) | `cplat_string_catalog_vformat` |
| カタログの点検 | `cplat_string_catalog_verify` |
| 文字列キーに対応する項目の取得 | `cplat_string_catalog_get_entry` |
| 分類値の取得 | `cplat_string_catalog_get_category` |
| ID の取得 | `cplat_string_catalog_get_id` |
| 備考の取得 | `cplat_string_catalog_get_note` |

Table: 文字列カタログ操作における用途別 cplat API

`cplat_string_catalog_get_entry` は、文字列キーごとの省略可能な `id`、引数の種別・名前・説明、分類値、必須の `brief`、省略可能な `details` と `remarks`、書式、備考をまとめて参照するために使用します。未設定の `id`、`details`、`remarks` は NULL です。返されるポインターはカタログ配列の要素を指し、呼び出し側で解放してはなりません。

出力言語はプロセスで 1 つです。組み立ての API に言語引数はありません。  
選択中の言語の要素が NULL の場合は、ニュートラル言語の要素へフォールバックします。ニュートラル言語の要素は NULL にできません。

出力言語を設定していないプロセスでは、最初の参照時に `cplat_ui_language_get_tag` の結果から出力言語を決定します。  
決定は 1 回だけ行い、`cplat_string_catalog_set_language` による設定はこの決定より優先します。  
言語タグを利用側で明示的に指定する場合は `cplat_string_catalog_language_from_tag` を使用します。対応する言語が存在しない場合はニュートラル言語を格納し、`CPLAT_ERR_NOT_FOUND` を返します。

書式は位置指定 (`{0}` から `{49}`) とエスケープだけです。インデックスは 10 進数 2 桁までで、先行ゼロを認めません。  
引数の個数の上限は `CPLAT_STRING_CATALOG_ARGUMENT_MAX` (50) です。

文字列キー (`cplat_string_catalog_entry::key`) は、処理から項目を参照する識別子です。  
ID (`cplat_string_catalog_entry::id`) と分類値 (`cplat_string_catalog_entry::category`) はライブラリが解釈しません。ID は処理では意味を持たない補足の文字列です。分類値の意味と有効な範囲は利用側が決めます。

`cplat_string_catalog_verify` は、すべての定義が一意な文字列キー、書式、引数の個数、引数の種別とメタデータ、インデックス表からの到達、ニュートラル言語の資源の条件を満たすことを確認します。  
条件を満たさない定義を検出した場合は `CPLAT_ERR_MALFORMED_DEFINITION` を返し、最初の 1 件の文字列キーと言語を出力引数へ書き出します。

カタログ定義 (JSONC) から列挙、カタログの表、型付きラッパーを書き出す生成器を `bin_internal/string_catalog_gen.py` に置いています。  
設計の詳細は [string_catalog モジュール](../prod/libsrc/cplat/string_catalog/README.md) を参照してください。

### 文字列カタログの条件式フィルター

対象ヘッダー: `cplat/string_catalog/filter.h`

条件式リストをポインターを含まないフィルター オブジェクトへコンパイルし、カタログへ関連付けたスロットで、文字列キーと引数値が条件に一致するかを判定します。  
一致の有無にかかわらず文字列を組み立て、一致結果を出力引数で返します。一致したトレースをどう扱うかは利用側が決めます。

| 用途 | cplat の API |
|---|---|
| 条件式リストのコンパイル | `cplat_string_catalog_filter_compile` |
| 行の置換、挿入、削除 | `cplat_string_catalog_filter_compile_line` / `cplat_string_catalog_filter_insert_line` / `cplat_string_catalog_filter_remove_line` |
| フィルター オブジェクトの検証と情報取得 | `cplat_string_catalog_filter_validate` / `cplat_string_catalog_filter_get_info` |
| 行のデコンパイル | `cplat_string_catalog_filter_decompile_line` |
| スロットの作成と破棄 | `cplat_string_catalog_filter_slot_create` / `cplat_string_catalog_filter_slot_dispose` |
| スロットを作成したカタログの取得 | `cplat_string_catalog_filter_slot_get_catalog` |
| フィルター オブジェクトの適用と取得 | `cplat_string_catalog_filter_slot_apply` / `cplat_string_catalog_filter_slot_snapshot` |
| 行の有効、無効と無効にした原因の取得 | `cplat_string_catalog_filter_slot_get_line_error` |
| 行の自然文での表現 | `cplat_string_catalog_filter_slot_describe_line` |
| 引数値によらない判定 | `cplat_string_catalog_filter_slot_test` |
| 判定と文字列の組み立て | `cplat_string_catalog_filter_slot_format` / `cplat_string_catalog_filter_slot_vformat` |
| カタログの識別値の計算 | `cplat_string_catalog_filter_get_catalog_id` |
| ソース領域への公開と情報取得 | `cplat_string_catalog_filter_source_publish` / `cplat_string_catalog_filter_source_get_info` |
| スロットへのソース領域と書き込み側の排他の結び付け、取り込み状態の取得 | `cplat_string_catalog_filter_slot_attach_source` / `cplat_string_catalog_filter_slot_get_source_status` |

Table: 文字列カタログの条件式フィルターにおける用途別 cplat API

フィルター オブジェクトのバイト数は `CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE` で行数の上限と行幅から求めます。  
ソース領域のバイト数は `CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE` で求めます。コマンド `string-catalog-filter-required-size` でも、行数の上限と行幅から表示できます。スロットは判定付きの組み立てのたびに版番号を比べ、変化した場合に取り込みます。  
分類値の名前 (`cplat_string_catalog_filter_category_names`) は、スロットの作成時に `cplat_string_catalog_filter_slot_create` へ渡します。作成後は変更できません。  
設計の詳細は [文字列カタログの条件式フィルターの設計](proposals/string-catalog-filter-design.md) を参照してください。

### スレッドと同期プリミティブ

対象ヘッダー: `cplat/sync/sync.h`

| 生の構文 | cplat API | 補足 |
|---|---|---|
| `pthread_mutex_t` (POSIX) / `CRITICAL_SECTION` (Win32) | `cplat_local_lock_create` / `cplat_local_lock_lock` / `cplat_local_lock_try_lock` / `cplat_local_lock_unlock` / `cplat_local_lock_dispose` | プロセス内ミューテックス。`lock` はタイムアウト付き |
| `pthread_cond_t` (POSIX) / `CONDITION_VARIABLE` (Win32) | `cplat_condvar_create` / `cplat_condvar_wait` / `cplat_condvar_signal` / `cplat_condvar_broadcast` / `cplat_condvar_dispose` | プロセス内条件変数 |
| `pthread_rwlock_t` (POSIX) / `SRWLOCK` (Win32) | `cplat_local_rwlock_create` / `cplat_local_rwlock_lock_shared` / `cplat_local_rwlock_lock_exclusive` / `cplat_local_rwlock_try_lock_shared` / `cplat_local_rwlock_try_lock_exclusive` / `cplat_local_rwlock_unlock_shared` / `cplat_local_rwlock_unlock_exclusive` / `cplat_local_rwlock_dispose` | プロセス内読み書きロック |
| `pthread_create` +`pthread_join` +`pthread_detach` (POSIX) / `CreateThread` +`WaitForSingleObject` +`CloseHandle` (Win32) | `cplat_thread_create` / `cplat_thread_join` / `cplat_thread_detach` | スレッド生成・待機・切り離し |
| 名前付き `flock`/セマフォ (POSIX) / 名前付き `Mutex` (Win32) | `cplat_interprocess_lock_open` / `cplat_interprocess_lock_lock` / `cplat_interprocess_lock_try_lock` / `cplat_interprocess_lock_unlock` / `cplat_interprocess_lock_dispose` | プロセス横断ミューテックス。プロセス間受け渡しには `_export_descriptor` / `_import_descriptor` を使用 |
| プロセス横断読み書きロックを取得したい (POSIX/Win32 に単一の生 API なし) | `cplat_interprocess_rwlock_open` / `_lock_shared` / `_lock_exclusive` / `_try_lock_shared` / `_try_lock_exclusive` / `_unlock` / `_destroy` | ロック ファイル バックエンドで実装 |
| `pthread_once` (POSIX) / `InitOnceExecuteOnce` (Win32) | `cplat_call_once(flag, func)` | - |
| `sleep`/`usleep`/`nanosleep` (POSIX) / `Sleep` (Win32) | `cplat_sleep_ms(ms)` | Linux はシグナル割り込み時に残り時間を再計算し継続待機します。 |
| `_Atomic` / `<stdatomic.h>` (C11、GCC) ・`<intrin.h>` の Interlocked 系 (MSVC) | `cplat_atomic_load_<型>` / `_store_<型>` / `_exchange_<型>` / `_compare_exchange_<型>` / `_fetch_add_<型>` / `_fetch_sub_<型>` / `cplat_atomic_thread_fence` (対象ヘッダー: `cplat/sync/atomic.h`) | ロックを取らない共有状態の読み書き (統計カウンター、フラグ、世代番号など) に使用。型は `u8`/`i32`/`u32`/`i64`/`u64`/`ptr` (`u8` と `ptr` に `_fetch_add`/`_fetch_sub` はなし)。初期化は `CPLAT_ATOMIC_INIT()` または `{0}`。MSVC の C17 モードは `_Atomic`/`<stdatomic.h>` を持たないため代替します。`volatile` は同期を保証しないため、同期には使わず本 API を使用してください。 |

Table: スレッドおよび同期プリミティブと cplat API の対応

### コマンド ライン引数の解析

対象ヘッダー: `cplat/argparser/argparser.h`

`getopt`/`getopt_long` (POSIX) に相当する機能を、Windows でも同じ API で使えるように提供します。

| 用途 | cplat API |
|---|---|
| パーサーを argc/argv とともに生成/初期化します。 | `cplat_argparser_handle_create` / `cplat_argparser_init` / `cplat_argparser_handle_dispose` |
| フラグ/オプション/位置引数を登録します。 | `cplat_argparser_register_flag` / `_register_option_int` / `_register_option_int_array` / `_register_option_string` / `_register_option_string_array` / `_register_positional_int` / `_register_positional_int_array` / `_register_positional_string` / `_register_positional_string_array` |
| 登録エラーを確認します。 | `cplat_argparser_get_register_error` / `_get_register_error_count` / `_get_register_error_target` / `_get_register_error_message` / `_print_register_error_messages` |
| 初期化時に受け取ったコマンド ラインを解析します。 | `cplat_argparser_parse` |
| 解析エラーを確認します。 | `cplat_argparser_get_error` / `_get_error_index` / `_get_error_target` / `_get_error_message` / `_print_error_messages` |
| 使用方法を表示します。 | `cplat_argparser_get_usage` / `cplat_argparser_print_usage` |

Table: コマンド ライン引数解析における用途別 cplat API

### 対話的プロンプト

対象ヘッダー: `cplat/prompt/prompt.h`、`cplat/prompt/pinned_prompt.h`

`fgets(stdin)`/`readline` (POSIX) や `ReadConsole` (Win32) を直接使う代わりに使用します。  
非 TTY では `cplat_fgets` へフォールバックし、行がバッファーに収まらないときは `CPLAT_ERR_BUFFER_TOO_SMALL` を返します。

| 用途 | cplat API |
|---|---|
| 1 行の対話的入力を読み取る | `cplat_prompt_create` / `cplat_prompt_readline` / `cplat_prompt_readline_at` / `cplat_prompt_readline_fmt` / `cplat_prompt_readline_fmt_at` / `cplat_prompt_dispose` |
| 入力欄に初期値を入れた状態で 1 行の対話的入力を読み取る (既存の値を編集させる) | `cplat_prompt_readline_with_initial` / `cplat_prompt_readline_with_initial_at` / `cplat_pinned_prompt_readline_with_initial` / `cplat_pinned_prompt_readline_with_initial_at` |
| 画面下部に固定したプロンプトへ入力しつつ、その上にログを流す | `cplat_pinned_prompt_create` / `cplat_pinned_prompt_readline` / `cplat_pinned_prompt_readline_fmt` / `cplat_pinned_prompt_write` / `cplat_pinned_prompt_printf` / `cplat_pinned_prompt_status_enable` / `cplat_pinned_prompt_status_set` / `cplat_pinned_prompt_dispose` |

Table: 対話的プロンプト入力における用途別 cplat API

### トレース

対象ヘッダー: `cplat/trace/tracer.h`、`cplat/trace/trace_file.h`、`cplat/trace/syslog.h`、`cplat/trace/eventlog.h`、`cplat/trace/etw.h`

Linux syslog、Windows イベント ログ、ETW を個別に呼び出す代わりに、共通のトレーサー API へ集約します。

`cplat_tracer_create` 直後は、すべての出力先のレベルが `CPLAT_TRACE_LEVEL_NONE` (無効) です。  
出力するには、`cplat_tracer_start` の前に出力先ごとのレベルを設定します。

| 用途 | cplat API |
|---|---|
| トレーサーを生成しレベル別に出力を開始/停止します。 | `cplat_tracer_create` / `cplat_tracer_start` / `cplat_tracer_stop` / `cplat_tracer_dispose` |
| 出力先ごとのレベルを設定/取得する (標準エラー、ファイル、OS ログ、ETW) | `cplat_tracer_set_stderr_level` / `_get_stderr_level` / `_set_file_level` / `_get_file_level` / `_set_os_level` / `_get_os_level` / `_set_etw_level` / `_get_etw_level` |
| トレーサーの名前/識別子/ファイル名を設定/取得します。 | `cplat_tracer_set_name` / `_get_name` / `_get_identifier` / `_set_file_name` / `_get_file_name` / `_get_file_identifier` |
| メッセージを出力する (ソース ファイル名/行番号を自動付与するマクロ) | `cplat_tracer_write` / `cplat_tracer_writef` |
| バイナリ データを 16 進数で出力する (同上) | `cplat_tracer_write_hex` / `cplat_tracer_write_hexf` |
| `va_list` 版や、ソース位置を自前で制御したい場合の低レベル関数 | `cplat_tracer_writef_at` / `cplat_tracer_vwritef_at` / `cplat_tracer_write_hexf_at` / `cplat_tracer_vwrite_hexf_at` |
| 出力フックを追加/削除します。 | `cplat_tracer_set_hook` / `cplat_tracer_call_next_hook` / `cplat_tracer_remove_hook` |
| トレーサーの状態を取得します。 | `cplat_tracer_get_state` |
| syslog (RFC5424 系、Linux 専用) の書き込みシンクを扱います。 | `cplat_syslog_sink_create` / `cplat_syslog_sink_write` / `cplat_syslog_sink_rename` / `cplat_syslog_sink_dispose` |
| Windows イベント ログの書き込みシンクを扱います。 | `cplat_eventlog_register_source` / `cplat_eventlog_sink_create` / `cplat_eventlog_sink_write` / `cplat_eventlog_sink_dispose` / `cplat_eventlog_unregister_source` |
| ETW (TraceLogging) の書き込みシンクを扱います。 | `cplat_etw_provider_create` / `cplat_etw_provider_write` / `cplat_etw_provider_dispose` / `cplat_etw_session_start` / `cplat_etw_session_stop` / `cplat_etw_session_check_access` |
| ローテーション付きファイルへ同期書き込みするシンクを扱います。OS バッファーへ委ねる場合は `CPLAT_TRACE_FILE_SINK_OS_BUFFERED` を指定します。 | `cplat_trace_file_sink_create` / `cplat_trace_file_sink_write` / `cplat_trace_file_sink_dispose` |

Table: トレース機能における用途別 cplat API

### 管理者権限の確認と昇格

対象ヘッダー: `cplat/runtime/elevated_process.h`

`GetTokenInformation(TokenElevation)`/`geteuid() == 0` の判定 (実 UID を返す `getuid() == 0` による誤判定を含む) や、UAC による自己再起動を直接書く代わりに使用します。  
`uid == 0` という判定方法自体が Windows には存在しないため、判定の目的である「管理者権限を保有しているか」に着目し、生 API の違いを個別に吸収するのではなく本 API へ抽象化してください。

| 用途 | cplat API |
|---|---|
| 現在のプロセスが管理者権限で実行されているか確認します。 | `cplat_elevated_process_is_elevated` |
| 必要な場合のみ管理者権限で自己を再起動します。 | `cplat_elevated_process_run_if_needed` / `cplat_elevated_process_run_with_result` / `cplat_elevated_process_run_piped` |
| 昇格プロセスの実行結果を受け渡しします。 | `cplat_elevated_process_extract_result_target` / `cplat_elevated_process_report_result` |
| 昇格プロセスの標準出力と標準エラー出力を、呼び出し元へ逐次受け渡します。 | `cplat_elevated_process_run_piped` / `cplat_elevated_process_attach_output_pipes` |

Table: 管理者権限の確認と昇格における用途別 cplat API

### プロセス終了時の共通フック

対象ヘッダー: `cplat/runtime/shutdown.h`

`atexit`、シグナル ハンドラー、`SetConsoleCtrlHandler` を個別に登録する代わりに使用します。

| 用途 | cplat API |
|---|---|
| 終了コードを保ったままプロセスを終了します。 | `cplat_exit(code)` |
| 終了時に呼び出されるコールバックを LIFO で登録します。 | `cplat_shutdown_register` |
| Ctrl+C 等の終了要求を受け取るコールバックを登録します。 | `cplat_shutdown_request_register` |

Table: プロセス終了時共通フックにおける用途別 cplat API

`cplat_shutdown_invoke_for_test` / `cplat_shutdown_request_invoke_for_test` / `cplat_shutdown_reset_for_test` はテスト専用で、本番コードからは呼び出しません。

### 動的シンボル解決

対象ヘッダー: `cplat/runtime/sym_loader.h`

`dlopen` +`dlsym` (POSIX) / `LoadLibrary` +`GetProcAddress` (Win32) を直接呼ぶ代わりに使用します。  
JSONC 設定ファイルからのライブラリ名解決、関数ポインターのキャッシュ、スレッド セーフな初期化を内包します。

| 用途 | cplat API |
|---|---|
| 関数を動的に解決します。 | `cplat_sym_loader_resolve` (型安全なラッパー マクロ `cplat_sym_loader_resolve_as`) |
| ローダーを初期化/破棄します。 | `cplat_sym_loader_init` / `cplat_sym_loader_dispose` |
| 既定ローダーかどうか確認します。 | `cplat_sym_loader_is_default` |
| 解決状況の情報を取得します。 | `cplat_sym_loader_info` |

Table: 動的シンボル解決における用途別 cplat API

### 共有ライブラリのロード/アンロード フック

対象ヘッダー: `cplat/base/shared_lib_lifecycle.h`

`__attribute__((constructor/destructor))` (Linux/GCC) と `DllMain` (Win32) という異なる仕組みを直接書く代わりに、共通のコールバック規約に統一します。  
エクスポートされる関数はなく、呼び出し側が `onLoad`/`onUnload` を実装するヘッダー オンリーの仕組みです。

## 公開関数名の補助索引

前節までの表や説明でまとめて扱った公開関数を、完全な関数名から検索できるように補足します。  
シグネチャを確認する場合は、関数名に対応する [`prod/include/`](../prod/include/) 配下のヘッダーを参照してください。

- 引数解析: `cplat_argparser_register_option_int`、`cplat_argparser_register_option_string`、`cplat_argparser_register_option_int_array`、`cplat_argparser_register_option_string_array`、`cplat_argparser_register_positional_int`、`cplat_argparser_register_positional_int_array`、`cplat_argparser_register_positional_string_array`、`cplat_argparser_get_error_target`、`cplat_argparser_get_error_index`、`cplat_argparser_get_error_message`、`cplat_argparser_print_error_messages`、`cplat_argparser_get_register_error_count`、`cplat_argparser_get_register_error_target`、`cplat_argparser_get_register_error_message`、`cplat_argparser_print_register_error_messages`
- 時刻: `cplat_clock_format_realtime_iso8601_local`、`cplat_clock_format_realtime_iso8601_utc`、`cplat_clock_get_realtime_utc`、`cplat_clock_get_realtime_deadline_ms`、`cplat_timespec_normalize`、`cplat_timespec_add`、`cplat_timespec_sub`、`cplat_timespec_cmp`、`cplat_timespec_add_ms`、`cplat_timespec_diff_ms`、`cplat_timespec_to_native`、`cplat_timespec_from_native`
- ファイル: `cplat_file_init`、`cplat_file_open`、`cplat_file_write`、`cplat_file_read`、`cplat_file_get_size`、`cplat_file_set_size`、`cplat_file_get_id`、`cplat_file_get_path_id`、`cplat_file_get_modified_timestamp`、`cplat_file_set_modified_timestamp`、`cplat_file_get_path_modified_timestamp`、`cplat_file_set_path_modified_timestamp`、`cplat_file_flush`、`cplat_file_close`
- パス: `cplat_path_normalize_sep`、`cplat_path_get_full`、`cplat_path_equal`、`cplat_path_get_temp_dir`、`cplat_path_concat_n`、`cplat_vpath_concat_n`、`cplat_path_basename`、`cplat_path_dirname`、`cplat_path_extension`、`cplat_path_strip_extension`、`cplat_path_join_n`、`cplat_vpath_join_n`
- 文字列: `cplat_strcasecmp`、`cplat_strncasecmp`
- 文字列カタログ: `cplat_string_catalog_set_language`、`cplat_string_catalog_get_language`、`cplat_string_catalog_language_from_tag`、`cplat_string_catalog_format`、`cplat_string_catalog_vformat`、`cplat_string_catalog_verify`、`cplat_string_catalog_get_entry`、`cplat_string_catalog_get_category`、`cplat_string_catalog_get_key`、`cplat_string_catalog_get_note`
- 書式入力: `cplat_vscanf`、`cplat_vfscanf`、`cplat_vsscanf`
- 暗号: `cplat_passphrase_to_key`
- エラー: `cplat_error_clear`、`cplat_error_capture_errno`、`cplat_error_capture_current_errno`、`cplat_error_get_last`、`cplat_error_set_last`、`cplat_error_clear_last`、`cplat_error_is_set`、`cplat_error_get_domain`、`cplat_error_get_errno`、`cplat_error_to_result`、`cplat_error_get_cause`、`cplat_error_is`、`cplat_result_to_string`
- メモリ マップド ファイル: `cplat_mmap_get_address`、`cplat_mmap_get_size`
- 正規表現: `cplat_regex_get_group_count`、`cplat_regex_iter_create`、`cplat_regex_iter_next`、`cplat_regex_iter_dispose`
- ホスト: `cplat_host_get_name`
- 表示言語: `cplat_ui_language_get_tag`
- モジュール: `cplat_module_get_basename`
- プロセス間ロック: `cplat_interprocess_lock_export_descriptor`、`cplat_interprocess_lock_import_descriptor`、`cplat_interprocess_rwlock_export_descriptor`、`cplat_interprocess_rwlock_import_descriptor`、`cplat_interprocess_rwlock_lock_shared`、`cplat_interprocess_rwlock_try_lock_shared`、`cplat_interprocess_rwlock_lock_exclusive`、`cplat_interprocess_rwlock_try_lock_exclusive`、`cplat_interprocess_rwlock_unlock`、`cplat_interprocess_rwlock_dispose`
- トレース: `cplat_tracer_get_name`、`cplat_tracer_get_identifier`、`cplat_tracer_set_file_name`、`cplat_tracer_get_file_name`、`cplat_tracer_get_file_identifier`、`cplat_tracer_get_os_level`、`cplat_tracer_set_os_level`、`cplat_tracer_get_etw_level`、`cplat_tracer_set_etw_level`、`cplat_tracer_get_file_level`、`cplat_tracer_set_file_level`、`cplat_tracer_get_stderr_level`
- Windows 文字列変換: `cplat_utf8_to_wpath`、`cplat_utf8_to_wstr`、`cplat_wpath_to_utf8`、`cplat_wstr_to_utf8`、`cplat_utf8_to_wstr_alloc`、`cplat_wstr_to_utf8_alloc`
- Windows エラー: `cplat_error_capture_windows_error`、`cplat_error_capture_current_windows_error`、`cplat_error_get_windows_error`

## 検証

対象関数の残存を確認する場合は、`coding-guideline.md` の「[検証](coding-guideline.md#検証)」節にある grep コマンドを使用します。  
危険な標準関数のコマンドはそのまま使用でき、プラットフォーム抽象ラッパー分を追加確認する場合は次のように対象関数を拡張します。

```bash
# プラットフォーム抽象ラッパー対象関数の直接使用確認 (例)
grep -rnE '\b(fopen|freopen|fclose|fflush|fread|fwrite|remove|rename|fprintf|vfprintf|fseek|ftell|open|lseek|close|dup|dup2|read|write|access|isatty|stat|mkdir|rmdir|getenv|setenv|unsetenv|gmtime|localtime|ctime)[[:space:]]*\(' \
  app --include=*.c --include=*.h \
  | grep -vE 'app/(lua|sqlite|cjson)/|/obj/|doxybook2_' \
  | grep -vE 'cplat_(fopen|freopen|fclose|fflush|fread|fwrite|remove|rename|fprintf|vfprintf|fseek|ftell|open|lseek|close|dup|dup2|read|write|access|isatty|stat|mkdir|rmdir|getenv|setenv|unsetenv|gmtime|localtime|ctime)' \
  | grep -vE ':[[:space:]]*\*'
```

cplat 自身のラッパー実装 (`prod/libsrc/cplat/crt/`) は、元関数へ委譲するため検出されます。  
[coding-guideline.md の「ラッパーがある関数の使用」](coding-guideline.md#ラッパーがある関数の使用) の例外に当たるため、対象外として扱います。
