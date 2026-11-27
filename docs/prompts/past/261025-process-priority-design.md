<!--
生成元: agree-before-implement スキル
文書種別: 最終仕様 (design)
最終更新エージェント: claude code / セッション ID: session_018UJwBxipLvskQMgge7hiBW
上流文書: `261025-process-priority-discussion.md` (検討用 Markdown、合意版: 版 4)。削除されている場合があります。
再開手順: このファイルを開いたエージェントは、作業の前に agree-before-implement スキルを読み込んでください。上流文書が存在する場合は、上流文書の再開手順に従って進行位置を確認し、判断の経緯を上流文書の Q&A で確認します。存在しない場合は、この文書を単体の最終仕様として扱います。最終更新エージェントのセッション情報が参照可能であれば再開時のコンテキストとして利用し、別エージェントからの引き継ぎや担当変更によりセッションが見つからない場合も異常とせず、文書の記載から継続してください。
-->

# cplat のプロセス優先度変更 API 最終仕様

ファイル名: `prompt/current/process-priority-design.md`

検討 ID: process-priority-20261025

## 概要

`app/cplat` の `runtime` カテゴリに、Linux と Windows それぞれに特化したプロセス優先度・スケジューリング設定 API を追加します。  
各 OS が公開 API で制御できる実行優先度の軸を網羅し、軸ごとに設定と取得の関数を設けます。  
権限不足は、両 OS とも戻り値 `CPLAT_ERR_PERMISSION_DENIED` で呼び出し元へ通知します。

プラットフォーム共通の抽象化 API は、この仕様の範囲外です。

> [!NOTE]
> 共通の抽象化を先に設計せず、特化 API だけを設けます。各 OS の軸は意味も粒度も異なるため、特化 API の利用実績から共通の意図を抽出してから抽象化します。

## 範囲

Table: 対象の軸と範囲外の軸

| 区分 | Linux | Windows |
| --- | --- | --- |
| 対象の軸 | スケジューリング ポリシーと静的優先度、fork 時の既定値への復帰フラグ、デッド ライン スケジューリング、nice 値、I/O 優先度、CPU 使用率クランプ | 優先度クラス、バックグラウンド処理モード、メモリ優先度、電力スロットリング (EcoQoS)、優先度ブーストの有効・無効、スレッド優先度 |
| 範囲外の軸 | autogroup の nice 値、cgroup の CPU / I/O 重み、CPU アフィニティ | 非公開 API による I/O 優先度、CPU アフィニティ / CPU セット、スレッド単位のバックグラウンド処理モード |
| 対象 | 任意のスレッド (ID はスレッド ID) | 任意のプロセスとスレッド (バックグラウンド処理モードは自プロセスのみ) |

権限の事前確認 API は設けません。権限の有無は、変更操作の結果コードで判別します。

> [!NOTE]
> Linux の権限判定には cgroup のリアルタイム帯域による拒否が含まれ、事前に計算できません。確認と変更の間に権限が変わる可能性もあるため、事前確認の結果は保証になりません。

## 共通の契約

### 対象の指定

対象は `uint32_t` の ID で指定し、0 は自分を表します。

Table: ID の解釈

| 関数の単位 | Linux | Windows |
| --- | --- | --- |
| プロセス単位 | (該当なし) | 0 は自プロセス (`GetCurrentProcess()`)。それ以外は `OpenProcess()` で開く |
| スレッド単位 | 0 は呼び出しスレッド。それ以外はスレッド ID (TID) | 0 は呼び出しスレッド (`GetCurrentThread()`)。それ以外は `OpenThread()` で開く |

- Linux の優先度属性はすべてスレッド単位であり、OS が受け取る ID はスレッド ID です。PID を渡すと、そのプロセスのメイン スレッドだけが変わります。0 と自プロセスの PID は意味が異なります (0 は呼び出しスレッド、PID はメイン スレッド)。
- Linux で ID を指定しない (0 を渡す) 場合は呼び出しスレッドだけが変わります。新しいスレッドは作成元スレッドの値を引き継ぐため、スレッドを作成する前に呼べば、以降に作るスレッド全体に及びます。
- 他プロセスの全スレッドを変更したい呼び出し元は、`/proc/<pid>/task` を自分で列挙して各スレッド ID を渡します。
- Linux の ID が `INT32_MAX` を超える場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。
- Linux の PID とスレッド ID は終了後に再利用されるため、指定した ID が意図と別のスレッドを指す可能性があります。この制約を API の文書に明記します。

> [!IMPORTANT]
> Linux の ID は、`cplat_process_get_tid()` が返す値、または他プロセスの PID とスレッド ID です。cplat の各関数は ID をそのまま OS へ渡し、プロセス単位への読み替えを行いません。1 回の呼び出しが 1 回の OS 操作に対応するため、部分的な適用や取りこぼしが起きません。

### 戻り値とエラー詳細

すべての関数は `int` の結果コードを返し、最後の引数に `cplat_error *detail_out` (NULL 可) を持ちます。  
失敗時は `detail_out` とスレッド ローカルの直前値 (`cplat_error_get_last()`) に OS の詳細を記録し、成功時と省略時は両方をクリアします。

Table: 結果コードと OS のエラー

| 状況 | 戻り値 | OS のエラー |
| --- | --- | --- |
| 成功 | `CPLAT_OK` | - |
| 権限不足 | `CPLAT_ERR_PERMISSION_DENIED` | `EPERM`、`EACCES`、`ERROR_ACCESS_DENIED`、`ERROR_PRIVILEGE_NOT_HELD`、Windows の `REALTIME` の格下げ |
| 対象が存在しない | `CPLAT_ERR_NOT_FOUND` | `ESRCH`、`OpenProcess()` / `OpenThread()` の `ERROR_INVALID_PARAMETER` |
| OS が機能に対応しない | `CPLAT_ERR_UNSUPPORTED` | `ENOSYS`、`ENOTSUP`、`EOPNOTSUPP`、`ERROR_NOT_SUPPORTED`、`ERROR_CALL_NOT_IMPLEMENTED`、CPU 使用率クランプの `E2BIG` |
| 引数の誤り | `CPLAT_ERR_INVALID_ARGUMENT` | cplat の引数検査、`EINVAL`、`ERROR_INVALID_PARAMETER` |
| デッド ラインの帯域不足 | `CPLAT_ERR_BUSY` | `EBUSY` |
| 省略 (エラーではない) | `CPLAT_SKIPPED` | Windows のバックグラウンド処理モードの二重開始と、モード外での終了 |
| 上記以外 | `CPLAT_ERR_UNKNOWN` | 写像のない OS エラー、取得した値が列挙型にない場合 |

- Linux の権限不足の原因 (ケーパビリティ、資源上限 `RLIMIT_RTPRIO` / `RLIMIT_NICE`、cgroup のリアルタイム帯域) は、OS が同じ errno を返すため区別できません。`detail_out` で `EPERM` と `EACCES` を区別できます。
- 取得関数の出力引数は、失敗時に変更しません。
- 出力引数が `NULL` の場合と、列挙型の範囲外の値を渡した場合は `CPLAT_ERR_INVALID_ARGUMENT` を返します。

### 設定値の型

選択値は cplat 独自の列挙型で受け取り、OS の定数は公開ヘッダーに現れません。  
利用側は `<sched.h>` や `<windows.h>` をインクルードせずに API を使えます。  
列挙子の名前は略語を展開します (`ROUND_ROBIN`、`REAL_TIME`、`BEST_EFFORT` など)。

真偽値の引数と出力は、cplat の既存 API と同じく `int` (0 または 1) とします。

## 公開ヘッダーと命名

Table: 公開ヘッダー

| ヘッダー | 対象 | 囲う条件 |
| --- | --- | --- |
| `app/cplat/prod/include/cplat/runtime/process_priority_linux.h` | Linux | ヘッダー全体を `#if defined(PLATFORM_LINUX)` で囲う |
| `app/cplat/prod/include/cplat/runtime/process_priority_windows.h` | Windows | ヘッダー全体を `#if defined(PLATFORM_WINDOWS)` で囲う |

- 関数名は `cplat_process_priority_<linux|windows>_<動詞>_<属性>` とします。
- 型名は `cplat_process_priority_<linux|windows>_<名詞>`、列挙子は `CPLAT_PROCESS_PRIORITY_<LINUX|WINDOWS>_<名詞>_<値>` とします。
- 両ヘッダーは、片側専用ヘッダーの規則に従って `cplat.h` の集約ヘッダーには含めず、`cplat.h` の除外ヘッダーの一覧へ追加します。
- 将来の抽象化 API は、プラットフォーム名を含まない `cplat/runtime/process_priority.h` と `cplat_process_priority_*` を使う想定です。

## Linux API

### 型

```c
typedef enum cplat_process_priority_linux_policy
{
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER = 0,       /* SCHED_OTHER */
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH = 1,       /* SCHED_BATCH */
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_IDLE = 2,        /* SCHED_IDLE */
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO = 3,        /* SCHED_FIFO */
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_ROUND_ROBIN = 4, /* SCHED_RR */
    CPLAT_PROCESS_PRIORITY_LINUX_POLICY_DEADLINE = 5     /* SCHED_DEADLINE (取得専用) */
} cplat_process_priority_linux_policy;

typedef struct cplat_process_priority_linux_deadline
{
    uint64_t runtime_nanoseconds;  /* 周期あたりの実行時間 */
    uint64_t deadline_nanoseconds; /* 周期の開始から期限までの時間 */
    uint64_t period_nanoseconds;   /* 周期 (0 は deadline_nanoseconds と同じ) */
} cplat_process_priority_linux_deadline;

typedef enum cplat_process_priority_linux_io_class
{
    CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_NONE = 0,        /* IOPRIO_CLASS_NONE (nice 値から導出) */
    CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_REAL_TIME = 1,   /* IOPRIO_CLASS_RT */
    CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_BEST_EFFORT = 2, /* IOPRIO_CLASS_BE */
    CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE = 3         /* IOPRIO_CLASS_IDLE */
} cplat_process_priority_linux_io_class;
```

CodeBlock: Linux API の型

### 関数

Table: Linux API の関数

| 関数 | 引数 (ID と `detail_out` を除く) | OS の操作 |
| --- | --- | --- |
| `cplat_process_priority_linux_set_policy` | `policy`、`static_priority`、`reset_on_fork` | `sched_setscheduler()` |
| `cplat_process_priority_linux_get_policy` | `policy_out`、`static_priority_out`、`reset_on_fork_out` | `sched_getscheduler()`、`sched_getparam()` |
| `cplat_process_priority_linux_set_deadline` | `const deadline *`、`reset_on_fork` | `sched_setattr()` |
| `cplat_process_priority_linux_get_deadline` | `deadline_out` | `sched_getattr()` |
| `cplat_process_priority_linux_set_nice` | `nice_value` | `setpriority(PRIO_PROCESS)` |
| `cplat_process_priority_linux_get_nice` | `nice_value_out` | `getpriority(PRIO_PROCESS)` |
| `cplat_process_priority_linux_set_io_priority` | `io_class`、`level` | `ioprio_set(IOPRIO_WHO_PROCESS)` |
| `cplat_process_priority_linux_get_io_priority` | `io_class_out`、`level_out` | `ioprio_get(IOPRIO_WHO_PROCESS)` |
| `cplat_process_priority_linux_set_utilization_clamp` | `minimum`、`maximum` | `sched_setattr()` の `SCHED_FLAG_UTIL_CLAMP` |
| `cplat_process_priority_linux_get_utilization_clamp` | `minimum_out`、`maximum_out` | `sched_getattr()` |

すべての関数は先頭の引数に `uint32_t thread_id` を持ちます。

```c
CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_set_policy(uint32_t thread_id,
                                                                   cplat_process_priority_linux_policy policy,
                                                                   int static_priority, int reset_on_fork,
                                                                   cplat_error *detail_out);
CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_get_policy(uint32_t thread_id,
                                                                   cplat_process_priority_linux_policy *policy_out,
                                                                   int *static_priority_out, int *reset_on_fork_out,
                                                                   cplat_error *detail_out);
```

CodeBlock: Linux API の宣言例

`sched_setattr()`、`sched_getattr()`、`ioprio_set()`、`ioprio_get()` は glibc 2.28 にラッパー関数がないため、`syscall()` で呼び出します。構造体 `sched_attr` と `IOPRIO_*` の定数は、glibc とカーネル ヘッダーの版に依存しないよう実装内で定義します。

### スケジューリング ポリシーと静的優先度

- `set_policy` は `OTHER`、`BATCH`、`IDLE`、`FIFO`、`ROUND_ROBIN` を受け付けます。`DEADLINE` は `CPLAT_ERR_INVALID_ARGUMENT` とし、`set_deadline` を使います。
- `static_priority` は、`FIFO` と `ROUND_ROBIN` では 1 〜 99、それ以外では 0 だけを受け付けます。範囲外は `CPLAT_ERR_INVALID_ARGUMENT` です。
- `reset_on_fork` が 1 の場合は `SCHED_RESET_ON_FORK` を付けます。子プロセスはポリシーと優先度を既定値へ戻して開始します。
- `get_policy` は `sched_getscheduler()` の戻り値から `SCHED_RESET_ON_FORK` を分離して `reset_on_fork_out` へ出力します。デッド ライン スケジューリング中は `DEADLINE` と静的優先度 0 を返します。

### デッド ライン スケジューリング

- `set_deadline` は `sched_attr` (最初の版の 48 バイト) に `SCHED_DEADLINE` と 3 つの時間を設定します。`reset_on_fork` が 1 の場合は `SCHED_FLAG_RESET_ON_FORK` を付けます。
- 時間の関係 (実行時間 ≦ 期限 ≦ 周期、最小値) はカーネルが検証し、違反は `EINVAL` (`CPLAT_ERR_INVALID_ARGUMENT`) になります。
- システムの帯域を超える要求は `EBUSY` (`CPLAT_ERR_BUSY`) になります。
- `get_deadline` は、対象がデッド ライン スケジューリング中でない場合、3 つの時間に 0 を出力して `CPLAT_OK` を返します。

### nice 値

- `nice_value` は -20 〜 19 だけを受け付け、範囲外は `CPLAT_ERR_INVALID_ARGUMENT` を返します (OS は範囲外を黙って丸めるため、cplat が検査します)。
- 値を下げる (優先度を上げる) 操作は、`CAP_SYS_NICE` または `RLIMIT_NICE` が必要です。不足すると `EACCES` (`CPLAT_ERR_PERMISSION_DENIED`) になります。
- nice 値は `OTHER` と `BATCH` にだけ作用します。
- `get_nice` は、`getpriority()` が正当な値として -1 を返すため、呼び出し前に `errno` を 0 にして失敗を判定します。

### I/O 優先度

- `level` は `REAL_TIME` と `BEST_EFFORT` では 0 (最高) 〜 7 (最低)、`NONE` と `IDLE` では 0 だけを受け付けます。範囲外は `CPLAT_ERR_INVALID_ARGUMENT` です。
- `REAL_TIME` は `CAP_SYS_ADMIN` が必要です。不足すると `EPERM` (`CPLAT_ERR_PERMISSION_DENIED`) になります。
- `get_io_priority` は、OS が返すクラスとレベルをそのまま出力します。
- I/O 優先度は、I/O スケジューラーが対応する場合 (BFQ など) にだけ効果があります。この制約を API の文書に明記します。

### CPU 使用率クランプ

- `minimum` と `maximum` は 0 〜 1024 で、`minimum` ≦ `maximum` を満たす必要があります。違反は `CPLAT_ERR_INVALID_ARGUMENT` です。
- `set_utilization_clamp` は `sched_attr` (2 番目の版の 56 バイト) に `SCHED_FLAG_KEEP_POLICY`、`SCHED_FLAG_KEEP_PARAMS`、`SCHED_FLAG_UTIL_CLAMP_MIN`、`SCHED_FLAG_UTIL_CLAMP_MAX` を設定し、ポリシーと優先度を変えずにクランプだけを変更します。
- 上限を引き上げる操作は `CAP_SYS_NICE` が必要です。不足すると `EPERM` (`CPLAT_ERR_PERMISSION_DENIED`) になります。
- カーネルが CPU 使用率クランプに対応しない場合は `CPLAT_ERR_UNSUPPORTED` を返します。判定は次のとおりです。

Table: CPU 使用率クランプの非対応の判定

| 状況 | OS の挙動 | 判定 |
| --- | --- | --- |
| Linux 5.3 より前 (`sched_attr` の 2 番目の版を知らない) | 設定は `E2BIG`。取得はカーネルが返す `sched_attr.size` が 56 未満 | 本 API 内で `CPLAT_ERR_UNSUPPORTED` へ変換する |
| `CONFIG_UCLAMP_TASK` が無効 | 設定は `EOPNOTSUPP`。取得は成功し、0 と 0 を返す | 設定は共通写像。取得は `/proc/sys/kernel/sched_util_clamp_min` がない場合に `CPLAT_ERR_UNSUPPORTED` |

> [!NOTE]
> `E2BIG` は「構造体が大きすぎる」という意味で他の用途とも共有されるため、共通写像では扱いません。CPU 使用率クランプの関数内でだけ、カーネルが 2 番目の版を知らないことの通知として変換します。  
> `CONFIG_UCLAMP_TASK` が無効なカーネルでも取得は成功するため、0 と 0 を正しい値として返さないよう、クランプ機能と同時にだけ存在する sysctl の有無で判定します。

## Windows API

### 型

```c
typedef enum cplat_process_priority_windows_class
{
    CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_IDLE = 0,         /* IDLE_PRIORITY_CLASS */
    CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_BELOW_NORMAL = 1, /* BELOW_NORMAL_PRIORITY_CLASS */
    CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_NORMAL = 2,       /* NORMAL_PRIORITY_CLASS */
    CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_ABOVE_NORMAL = 3, /* ABOVE_NORMAL_PRIORITY_CLASS */
    CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_HIGH = 4,         /* HIGH_PRIORITY_CLASS */
    CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_REAL_TIME = 5     /* REALTIME_PRIORITY_CLASS */
} cplat_process_priority_windows_class;

typedef enum cplat_process_priority_windows_memory_priority
{
    CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_VERY_LOW = 1,     /* MEMORY_PRIORITY_VERY_LOW */
    CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_LOW = 2,          /* MEMORY_PRIORITY_LOW */
    CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_MEDIUM = 3,       /* MEMORY_PRIORITY_MEDIUM */
    CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_BELOW_NORMAL = 4, /* MEMORY_PRIORITY_BELOW_NORMAL */
    CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_NORMAL = 5        /* MEMORY_PRIORITY_NORMAL */
} cplat_process_priority_windows_memory_priority;

typedef enum cplat_process_priority_windows_power_throttling
{
    CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_SYSTEM_MANAGED = 0, /* OS に任せる */
    CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_ENABLED = 1,        /* 常に抑制する (EcoQoS) */
    CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_DISABLED = 2        /* 常に抑制しない */
} cplat_process_priority_windows_power_throttling;

typedef enum cplat_process_priority_windows_thread_priority
{
    CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_IDLE = 0,          /* THREAD_PRIORITY_IDLE */
    CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_LOWEST = 1,        /* THREAD_PRIORITY_LOWEST */
    CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_BELOW_NORMAL = 2,  /* THREAD_PRIORITY_BELOW_NORMAL */
    CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_NORMAL = 3,        /* THREAD_PRIORITY_NORMAL */
    CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL = 4,  /* THREAD_PRIORITY_ABOVE_NORMAL */
    CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_HIGHEST = 5,       /* THREAD_PRIORITY_HIGHEST */
    CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_TIME_CRITICAL = 6  /* THREAD_PRIORITY_TIME_CRITICAL */
} cplat_process_priority_windows_thread_priority;
```

CodeBlock: Windows API の型

### 関数

Table: Windows API の関数

| 関数 | 対象の引数 | その他の引数 (`detail_out` を除く) | OS の操作 | 対象を開くアクセス権 |
| --- | --- | --- | --- | --- |
| `cplat_process_priority_windows_set_class` | `process_id` | `priority_class` | `SetPriorityClass()` | `PROCESS_SET_INFORMATION`、`PROCESS_QUERY_LIMITED_INFORMATION` |
| `cplat_process_priority_windows_get_class` | `process_id` | `priority_class_out` | `GetPriorityClass()` | `PROCESS_QUERY_LIMITED_INFORMATION` |
| `cplat_process_priority_windows_begin_background_mode` | なし | なし | `SetPriorityClass(PROCESS_MODE_BACKGROUND_BEGIN)` | (自プロセス) |
| `cplat_process_priority_windows_end_background_mode` | なし | なし | `SetPriorityClass(PROCESS_MODE_BACKGROUND_END)` | (自プロセス) |
| `cplat_process_priority_windows_set_memory_priority` | `process_id` | `memory_priority` | `SetProcessInformation(ProcessMemoryPriority)` | `PROCESS_SET_INFORMATION` |
| `cplat_process_priority_windows_get_memory_priority` | `process_id` | `memory_priority_out` | `GetProcessInformation(ProcessMemoryPriority)` | `PROCESS_QUERY_LIMITED_INFORMATION` |
| `cplat_process_priority_windows_set_power_throttling` | `process_id` | `power_throttling` | `SetProcessInformation(ProcessPowerThrottling)` | `PROCESS_SET_INFORMATION` |
| `cplat_process_priority_windows_get_power_throttling` | `process_id` | `power_throttling_out` | `GetProcessInformation(ProcessPowerThrottling)` | `PROCESS_QUERY_LIMITED_INFORMATION` |
| `cplat_process_priority_windows_set_priority_boost` | `process_id` | `enabled` | `SetProcessPriorityBoost()` | `PROCESS_SET_INFORMATION` |
| `cplat_process_priority_windows_get_priority_boost` | `process_id` | `enabled_out` | `GetProcessPriorityBoost()` | `PROCESS_QUERY_LIMITED_INFORMATION` |
| `cplat_process_priority_windows_set_thread_priority` | `thread_id` | `thread_priority` | `SetThreadPriority()` | `THREAD_SET_LIMITED_INFORMATION` |
| `cplat_process_priority_windows_get_thread_priority` | `thread_id` | `thread_priority_out` | `GetThreadPriority()` | `THREAD_QUERY_LIMITED_INFORMATION` |

- 対象を開く段階のアクセス拒否も、値を設定する段階の特権不足も、`CPLAT_ERR_PERMISSION_DENIED` を返します。`detail_out` の Windows エラー値で区別できます。
- `OpenProcess()` と `OpenThread()` が `ERROR_INVALID_PARAMETER` を返した場合は、対象が存在しないとみなして `CPLAT_ERR_NOT_FOUND` を返します。`ERROR_INVALID_PARAMETER` は他の用途でも使われるため、この変換は対象を開く箇所でだけ行います。
- 開いたハンドルは、関数を抜ける前に必ず閉じます。擬似ハンドル (`GetCurrentProcess()`、`GetCurrentThread()`) は閉じません。

### 優先度クラス

`REAL_TIME` を指定し、呼び出し元が特権 `SeIncreaseBasePriorityPrivilege` を持たない場合、OS は失敗を返さず `HIGH` を設定します。  
`set_class` は `REAL_TIME` を指定した場合に、設定前のクラスを `GetPriorityClass()` で保存し、設定後に再取得します。  
再取得したクラスが `REAL_TIME` でなければ、保存したクラスへ戻して `CPLAT_ERR_PERMISSION_DENIED` を返します。  
`detail_out` には `ERROR_PRIVILEGE_NOT_HELD` を記録します。

> [!IMPORTANT]
> この格下げの挙動は公式文書に記載がありません。cplat は OS 内部の特権判定を再現せず、設定後の結果だけを確認します。失敗時に状態を変えないという他の API と同じ契約を保つため、格下げを検出した場合は元のクラスへ戻します。

`get_class` は、OS が返した値が列挙型にない場合に `CPLAT_ERR_UNKNOWN` を返します。

### バックグラウンド処理モード

- 自プロセス専用で、対象の引数を持ちません。CPU に加えて I/O とメモリの優先度も下げます。
- 開始関数は `ERROR_PROCESS_MODE_ALREADY_BACKGROUND`、終了関数は `ERROR_PROCESS_MODE_NOT_BACKGROUND` を受けた場合に `CPLAT_SKIPPED` を返します。
- 現在モード中かを取得する公開 API がないため、取得関数は設けません。呼び出し元は開始関数の戻り値が `CPLAT_OK` の場合だけ終了関数を呼びます。

```c
ret = cplat_process_priority_windows_begin_background_mode(&detail);
if (ret == CPLAT_OK)
{
    /* 自分が開始した。処理後に終了する */
    do_background_work();
    (void)cplat_process_priority_windows_end_background_mode(NULL);
}
else if (ret == CPLAT_SKIPPED)
{
    /* 他の部品がすでに開始している。終了は呼ばない */
    do_background_work();
}
```

CodeBlock: バックグラウンド処理モードの利用例

> [!NOTE]
> 戻り値を `ret == CPLAT_OK` だけで判定すると、省略を失敗として扱います。API の文書に明記します。

### メモリ優先度、電力スロットリング、優先度ブースト

- メモリ優先度は `MEMORY_PRIORITY_INFORMATION` の 5 段階を設定・取得します。
- 電力スロットリングは `PROCESS_POWER_THROTTLING_EXECUTION_SPEED` だけを扱います。`SYSTEM_MANAGED` は `ControlMask` から該当ビットを外し、`ENABLED` は `ControlMask` と `StateMask` の両方に、`DISABLED` は `ControlMask` だけに該当ビットを設定します。取得は同じ対応で逆に写像します。
- 優先度ブーストは、OS の引数 (`DisablePriorityBoost`) と逆の意味の `enabled` で扱います。

### スレッド優先度

- 優先度クラス内の相対値 7 段階を設定・取得します。
- `REAL_TIME` クラス内でだけ使える -7 〜 -3 と 3 〜 6 の値は扱いません。`get_thread_priority` がこれらの値を得た場合は `CPLAT_ERR_UNKNOWN` を返します。
- `GetThreadPriority()` の失敗は `THREAD_PRIORITY_ERROR_RETURN` で判定します。

## 共通写像の変更

`app/cplat/prod/libsrc/cplat/base/` の共通写像を次のとおり変更します。

Table: 共通写像の変更

| OS のエラー | 結果コード (`result.c`) | 要因 (`error.c`) |
| --- | --- | --- |
| `ESRCH` | `CPLAT_ERR_UNKNOWN` から `CPLAT_ERR_NOT_FOUND` へ | `CPLAT_CAUSE_OTHER` から `CPLAT_CAUSE_NOT_FOUND` へ |
| `ENOSYS`、`ENOTSUP`、`EOPNOTSUPP` | `CPLAT_ERR_UNKNOWN` から `CPLAT_ERR_UNSUPPORTED` へ | `CPLAT_CAUSE_UNSUPPORTED` (変更なし) |
| `ERROR_NOT_SUPPORTED`、`ERROR_CALL_NOT_IMPLEMENTED` | `CPLAT_ERR_UNKNOWN` から `CPLAT_ERR_UNSUPPORTED` へ | `CPLAT_CAUSE_UNSUPPORTED` (変更なし) |

- 結果コードと要因の分類を一致させます。
- 既存の API でも、これらのエラーが起きた場合の戻り値が変わります。既存の製品コードとテストに、これらの値で `CPLAT_ERR_UNKNOWN` を期待する箇所はありません。
- `result.h` の `CPLAT_ERR_NOT_FOUND` と `CPLAT_ERR_UNSUPPORTED` の説明、`error.h` の `CPLAT_CAUSE_NOT_FOUND` の説明を、写像の対象に合わせて更新します。`CPLAT_ERR_UNSUPPORTED` の「変換しない」とする `@note` は、写像の対象の列挙に置き換えます。

## テストと mock

### testfw の mock

`framework/testfw` に、新しく使う OS API の mock を追加します。既存の mock と同じく、マクロ置換で呼び出し元の情報を付けて mock 関数へ渡し、既定では実関数へ委譲します。

Table: 追加する testfw の mock

| 対象 | mock を追加する API |
| --- | --- |
| Linux | `sched_setscheduler`、`sched_getscheduler`、`sched_getparam`、`setpriority`、`getpriority`、`syscall` |
| Windows | `SetPriorityClass`、`GetPriorityClass`、`OpenProcess`、`OpenThread`、`SetProcessInformation`、`GetProcessInformation`、`SetProcessPriorityBoost`、`GetProcessPriorityBoost`、`SetThreadPriority`、`GetThreadPriority` |

- `syscall` は可変長引数関数です。既存の `fcntl` の mock と同じく、マクロで引数の個数を判定し、追加引数を `intptr_t` 5 個へ畳んだ固定シグネチャの mock 関数へ渡します。実関数への委譲は 5 個の追加引数をそのまま渡します (カーネルは使わない引数を無視します)。
- `syscall` の mock は既存の `syscall(SYS_gettid)` (`process.c`) にも適用されますが、既定で実関数へ委譲するため挙動は変わりません。

### mock_cplat

`app/cplat/test/libsrc/mock_cplat` に、追加するすべての公開関数の mock を `create-mock-cplat-mock` スキルの手順で追加します。

### 単体テスト

`app/cplat/test/src/libcplatTest/runtime/` に Linux と Windows の単体テストを追加します。

- 実機の挙動の確認は、テスト内で作成した作業スレッドを対象にします。一般ユーザーは nice 値を戻せないため、テスト プロセスのメイン スレッドの優先度を変えないようにします。
- Linux の実機では、優先度を下げる操作の成功、取得値の一致、優先度を上げる操作の `CPLAT_ERR_PERMISSION_DENIED`、存在しないスレッド ID の `CPLAT_ERR_NOT_FOUND`、CPU 使用率クランプの `CPLAT_ERR_UNSUPPORTED` (`CONFIG_UCLAMP_TASK` が無効な環境) を確認します。特権を持つ実行環境では、権限不足の確認を省略します。
- testfw の mock で、権限不足、対象不在、非対応、`E2BIG`、`REAL_TIME` の格下げと復元、バックグラウンド処理モードの `CPLAT_SKIPPED` の経路を確認します。
- 引数検査 (範囲外の値、NULL の出力引数、`INT32_MAX` を超える ID) を確認します。
- `app/cplat/test/src/libcplatTest/exportTest/exportTest.cc` の `CPLAT_EXPORT_TABLE_PLATFORM` に、各 OS の公開関数を追加します。
- `resultTest.cc` と `errorTest.cc` に、共通写像へ追加した値の確認を加えます。

Windows のビルドとテストは、ユーザーの Windows 環境または CI で実行し、その結果を受入確認に使います。

## 文書と知見の反映

Table: 知見の反映先

| 反映先 | 内容 |
| --- | --- |
| 公開ヘッダーの Doxygen コメント | 各関数の契約 (ID の解釈、必要な権限と結果コード、Linux のスレッド単位の属性とメイン スレッドだけが変わる制約、ID の再利用、`CPLAT_SKIPPED` の判定、I/O スケジューラーの制約) |
| 実装のコード コメント (`see: <URL>`) | `REAL_TIME` の格下げの検出と復元、`getpriority()` の -1 の判定、`sched_attr` の版と `E2BIG`、`CONFIG_UCLAMP_TASK` の判定、`OpenProcess()` の `ERROR_INVALID_PARAMETER`、バックグラウンド処理モードのエラー |
| `app/cplat/docs/functional-spec/runtime.md` | プロセス優先度の要件と節 (対象の軸、権限不足の通知、適用範囲外としてプラットフォーム共通の抽象化) |
| `app/cplat/docs/api-cheatsheet.md` | 追加する公開関数 |
| `result.h`、`error.h` の Doxygen コメント | 共通写像の対象 |

> [!NOTE]
> `REAL_TIME` の格下げは公式文書に記載がないため、`see:` には挙動を確認できた資料だけを残します。

## 変更対象ファイル

Table: 変更対象ファイル

| 区分 | ファイル |
| --- | --- |
| 公開ヘッダー | `app/cplat/prod/include/cplat/runtime/process_priority_linux.h`、`app/cplat/prod/include/cplat/runtime/process_priority_windows.h`、`app/cplat/prod/include/cplat.h` |
| 実装 | `app/cplat/prod/libsrc/cplat/runtime/process_priority_linux.c`、`app/cplat/prod/libsrc/cplat/runtime/process_priority_windows.c` |
| 共通写像 | `app/cplat/prod/libsrc/cplat/base/result.c`、`app/cplat/prod/libsrc/cplat/base/error.c`、`app/cplat/prod/include/cplat/base/result.h`、`app/cplat/prod/include/cplat/base/error.h` |
| testfw の mock | `framework/testfw/include/` と `framework/testfw/libsrc/mock_libc/` の該当ファイル |
| mock_cplat | `app/cplat/test/libsrc/mock_cplat/` の該当ファイル |
| テスト | `app/cplat/test/src/libcplatTest/runtime/` の新規テスト、`exportTest.cc`、`resultTest.cc`、`errorTest.cc` |
| 文書 | `app/cplat/docs/functional-spec/runtime.md`、`app/cplat/docs/api-cheatsheet.md` |

## 受入条件

- この仕様のすべての決定が、公開ヘッダー、実装、機能仕様、API チート シートに反映されていること。
- 権限不足が両 OS で `CPLAT_ERR_PERMISSION_DENIED` として返り、`detail_out` とスレッド ローカルの直前値に OS の詳細が記録されること。
- Linux の局所ビルドと `app/cplat` の関連テスト (新規の単体テスト、`resultTest`、`errorTest`、`exportTest`) が成功し、`.warn` に今回の変更による警告がないこと。testfw の変更は、testfw のビルドと mock を利用する cplat のテストで確認すること。
- 新しく使う OS API に testfw の mock があり、mock で失敗経路 (権限不足、対象不在、非対応) を確認していること。
- Windows のビルドとテストが、ユーザーの Windows 環境または CI で成功すること。
- 変更した Markdown に `text_style_jp.py --dry-run` の指摘がないこと。
