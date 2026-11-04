# Lab3 A3+A4：上下文主测与异常复测记录

执行日期：2026-11-04。负责人：A，学号 `2412712`。任务标记：`lab3 A3+A4`。

## 版本与环境

| 项目 | 实际值 |
| --- | --- |
| 分支 | `work/lab3/ziqiu` |
| 测试提交 | `e88ec4421a4415b10b0711c26c2667f6bc9e0f0c`，与本地 `lab3` 一致 |
| 执行环境 | WSL 登录环境 |
| 编译器 | `riscv64-unknown-elf-gcc` 15.1.0 |
| QEMU | **4.1.1** |
| 实际 QEMU 路径 | `/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64` |
| 启动方式 | 沿用评分脚本的 `virt`、默认 BIOS 和 `0x80200000` 镜像加载地址 |

当前版本已集成 A1、A2 和 B、C 的真实实现。A 在该版本上完成了两轮实际评分：第一轮完成 A3 的 35 分主测；随后按要求重跑 B2、C2，其中对 C2 的独立复测计入 A4，结果为 25/25。两轮总评分均为 100/100，退出码均为 0。

| 轮次 | 目的 | 结果与材料 |
| --- | --- | --- |
| 第一轮 | A3：上下文与断点主测 | 35/35，通过；保存完整评分日志，现名为 `a3a4-grade.log` |
| 第二轮 | A4：独立复测 C 的非法指令与兜底；同时复核 B2 | A4 为 25/25，B2 为 40/40；按当时要求未另存日志，结果在本次材料整理中补记 |

第一轮日志最初名为 `a3-grade.log`，材料整理后的现存文件为 `a3a4-grade.log`。下方执行命令保留当时使用的原文件名。第二轮依据实际控制台结果记录，没有将第一轮日志改作第二轮日志。

## A3：执行命令与结果

第一轮在 WSL 的 `code` 目录运行一次现有完整评分，并保存日志：

```bash
cd /mnt/a/OS/exp/Operation-System-Labs/code
mkdir -p ../report/_work/logs
export PATH="/home/catalpa/.local/opt/qemu-4.1.1/bin:$PATH"
set -euo pipefail
{
    command -v qemu-system-riscv64
    qemu-system-riscv64 --version
    riscv64-unknown-elf-gcc --version
    make grade CC='riscv64-unknown-elf-gcc -Dsbadaddr=stval'
} 2>&1 | tee ../report/_work/logs/a3-grade.log
```

评分脚本直接调用 `qemu-system-riscv64`，通过本次会话的 `PATH` 选中 4.1.1。CSR 兼容宏通过 `CC` 传入，保留脚本为各用例设置的 `DEFS`。无需修改构建规则。

本次命令退出码为 **0**。完整控制台日志见 [a3a4-grade.log](./_work/logs/a3a4-grade.log)，包含实际版本输出、三组构建和评分结果。

```text
  - trap entry, register restore, and breakpoint EPC: PASS (35/35)
  - illegal instruction EPC advance and return: PASS (25/25)
  - re-armed supervisor timer and repeated trap return: PASS (40/40)
Total Score: 100/100
```

| 项目 | 第一轮结果 | 任务归属 |
| --- | --- | --- |
| Trap 入口、寄存器恢复、断点返回 | **35/35，通过** | A3 独立主测 |
| 非法指令与兜底 | 25/25，通过 | 本次完整评分附带的集成结果 |
| 持续时钟 | 40/40，通过 | 本次完整评分附带的集成结果 |
| 总评分 | **100/100** | 当前集成版本的本次评分结果 |

现有评分入口自行进行测试副本注入和各用例的干净构建；评分成功后自动清理临时副本。第一轮保留完整控制台日志；两轮均未修改内核、测试或预期。

## 35 分项验证了什么

`test_lab3_register_context()` 在触发 `ebreak` 前为一组寄存器写入已知值，返回后逐一比较；全部相符时返回 0，由 `test_lab3_exception_context()` 输出通过标记。评分还要求串口出现 `Breakpoint at`，因此本次 35 分通过同时满足现有断点输出和上下文返回条件。

执行链路与当前源码对应如下：

1. Trap 进入 `__alltraps`，`SAVE_ALL` 在内核栈上建立 36 槽、288 字节的帧，将原始 `sp` 保存到第 2 槽。
2. 当前帧基址通过 `a0` 传给 `trap(tf)`，由 C 层分派到断点处理。异常处理依据指令前缀推进 `tf->epc`，使返回位置越过引发异常的指令。
3. `RESTORE_ALL` 将帧中的 `status`、更新后的 `epc` 写回 `sstatus`、`sepc`，恢复其他通用寄存器，最后从第 2 槽恢复原始 `sp`。
4. `sret` 按保存的状态返回，测试继续执行寄存器比较并输出通过结果。

用例显式比较 `t0` 至 `t6`、`s0` 至 `s11`、`a1` 至 `a7`，共 **26 个寄存器**。`sp`、`ra` 参与实际调用与返回链路，但该用例没有单独逐值比较它们。本次结果限定于现有测试覆盖范围，不扩展为全部寄存器、其他特权级或嵌套 Trap 均已动态验证。

## A4：独立复测 C 的异常处理

第一轮后，用户要求“再跑下 B2、C2 的测试，不用保存，测试后告知结果”。A 因此在同一提交上重新运行现有评分，使用以下命令，未将输出写入新日志：

```bash
cd /mnt/a/OS/exp/Operation-System-Labs/code
export PATH="/home/catalpa/.local/opt/qemu-4.1.1/bin:$PATH"
qemu-system-riscv64 --version
make grade CC='riscv64-unknown-elf-gcc -Dsbadaddr=stval'
```

该轮实际使用 QEMU **4.1.1**。控制台再次输出 35/35、25/25、40/40 全部通过，总分 **100/100**，命令退出码为 **0**。

A4 按分工要求独立运行 25 分项。该项通过意味着现有评分同时核对了：

| 检查 | 评分要求 | 第二轮结果 |
| --- | --- | --- |
| 非法指令处理后继续执行 | `Illegal instruction at` 及非法指令返回 PASS | 通过 |
| 软件中断提示 | `Supervisor software interrupt` | 通过 |
| 未知中断提示 | `Unknown interrupt: cause 15` | 通过 |
| 未知异常兜底 | cause 20 的 Trap frame，包含 `cause    0x00000014` | 通过 |

这些条件由原有评分脚本检查，25 分项为 **PASS (25/25)**，满足 A4 的复测范围。实际测试使用 C 的真实实现及集成依赖，没有复用其他成员的测试结论。额外的 16/32 位编码组合测试不属于本次新增验证。

## 完成状态

**A3 的 35 分独立主测和 A4 的 25 分独立复测均已完成。** 第二轮 B2 的 40 分项也复核通过；其他成员的交叉复测、报告合并和共同终审仍按分工完成。

两轮评分均一次通过，没有源码修复或额外调试迭代。既有 CSR 名称兼容参数直接用于构建，没有发生新的环境故障。本次材料更新仅整理已经执行的结果。

实际提示词和两轮执行记录已更新到 `report/_work/prompts/ziqiu.md`。本次没有提交或推送。
