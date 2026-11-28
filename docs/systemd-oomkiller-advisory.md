# Linux systemd OOM killer 対策アドバイザリ

この文書では、Linux の systemd unit で管理されるプロセスが OOM killer (Out-Of-Memory Killer) により停止されにくくするための設定と、`cplat_service_install()` での既定動作を説明します。

## 一般的な考え方

Linux では、OOM による停止経路を 2 つに分けて考える必要があります。

Table: OOM による停止経路

| 種類 | 主な設定 | 説明 |
|---|---|---|
| カーネル OOM killer | `OOMScoreAdjust=` | メモリ不足がカーネル空間で限界に達したとき、プロセスの OOM score をもとに停止対象を選択します。 |
| `systemd-oomd` | `ManagedOOMPreference=` | cgroup v2 と PSI を使用し、カーネル OOM より前に cgroup 単位で停止対象を選択します。 |

カーネル OOM killer への対策には `OOMScoreAdjust=-1000` を設定します。  
`systemd-oomd` への対策には `ManagedOOMPreference=omit` を設定します (`ManagedOOMPreference=` は systemd 248 以降で利用できます)。

## 推奨 unit 設定

systemd 248 以降では、次の設定を `[Service]` に追加します。

```ini
[Service]
OOMScoreAdjust=-1000
ManagedOOMPreference=omit
```

systemd 247 以前では、`ManagedOOMPreference=` が未知の設定となるため、次の設定にします。

```ini
[Service]
OOMScoreAdjust=-1000
```

## cplat_service_install() での実装

`cplat_service_install()` の Linux 実装は、`/etc/systemd/system/<name>.service` を生成します。

- `options == NULL` または `options->disable_oom_protection == 0` (既定) の場合:
    - カーネル OOM killer 向けに `OOMScoreAdjust=-1000` を出力します。
    - `systemctl --version` で systemd の major version を確認し、systemd 248 以降では `ManagedOOMPreference=omit` も出力します。
    - systemd 247 以前、またはバージョン取得に失敗した場合は互換性を優先して `ManagedOOMPreference=omit` を省略します。
- `options->disable_oom_protection != 0` の場合:
    - `OOMScoreAdjust=` と `ManagedOOMPreference=` のどちらも出力しません。

コンソール モード (`cplat_service_run_console()`) で直接起動したプロセスは systemd unit の管理外になるため、この unit 設定は適用されません。

## 確認方法

```bash
systemctl --version
systemctl show <service-name> -p OOMScoreAdjust -p ManagedOOMPreference
pid=$(systemctl show <service-name> -p MainPID --value)
cat /proc/${pid}/oom_score_adj
systemd-cgls /system.slice/<service-name>.service
```

## 注意事項

`OOMScoreAdjust=-1000` と `ManagedOOMPreference=omit` は、対象 unit を停止対象にしにくくする設定であり、システム全体のメモリ不足を解消するものではありません。  
保護対象を増やしすぎると他の unit が停止されやすくなるため、恒久運用ではメモリ使用量の上限や監視メトリクスを別途設計してください。

## 参考

- systemd.exec: <https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html>
- systemd.resource-control: <https://www.freedesktop.org/software/systemd/man/latest/systemd.resource-control.html>
- systemd-oomd.service: <https://www.freedesktop.org/software/systemd/man/latest/systemd-oomd.service.html>
