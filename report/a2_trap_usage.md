# Lab3 A2：Trap 入口与上下文恢复

执行日期：2026-11-03。负责人：A，学号 `2412712`。分支：`work/lab3/ziqiu`。

## 修改内容

在 `code/kern/trap/trapentry.S` 的两个 `lab3 function1 YOUR CODE` 位置补齐实现，注释标明 `lab3 A2 - 2412712`。

- `__alltraps`：已有 `SAVE_ALL` 建立帧后，将当前 `sp` 作为 `a0` 参数传给 `trap()`；C 函数正常返回后，顺序进入 `__trapret`。
- `RESTORE_ALL`：先取出帧中的返回状态和地址并写入 `sstatus`、`sepc`，再恢复全部可写通用寄存器，最后恢复 `sp`。原有 `sret` 完成特权级返回。

保留 `SAVE_ALL`、结构体、C 层处理函数、正式初始化流程及构建规则；A1 的内存管理代码保持原样。实现直接使用现有帧，不增加运行时防御检查、额外上下文结构或测试框架。

## 帧布局与恢复顺序

当前 RISC-V64 的 `REGBYTES` 为 8，帧包含 36 个槽，共 288 字节；已有对齐的栈减去该大小后仍满足 C 调用的 16 字节对齐要求。

| 内容 | 槽位 | 字节偏移 | 本次恢复方式 |
| --- | --- | --- | --- |
| `x0` | 0 | 0 | 保持硬连线零 |
| `x1`、`x3` 至 `x31` | 对应寄存器编号 | 对应编号乘以 8 | 从各自保存槽加载 |
| 原始 `x2(sp)` | 2 | 16 | 在所有其他帧访问完成后加载 |
| `status` | 32 | 256 | 经 `t0` 写入 `sstatus` |
| `epc` | 33 | 264 | 经 `t1` 写入 `sepc` |
| `badvaddr`、`cause` | 34、35 | 272、280 | 留作诊断和分派信息 |

`SAVE_ALL` 已把 Trap 前的原始 `sp` 经 `sscratch` 保存到第 2 槽。调用 C 函数前后的当前 `sp` 指向帧基址；恢复完成后的 `sp` 则取自该保存槽。最后一条加载指令为 `LOAD x2, 2*REGBYTES(sp)`，因此不再额外增加帧大小。

`t0`、`t1` 在写完两个 CSR 后，从各自的第 5、6 槽恢复；`ra`、`a0` 及其他通用寄存器也都从帧恢复。C 层对 `tf->epc` 的合法更新会被用于返回，汇编不额外推进地址。返回特权级和中断状态由保存的 `sstatus` 与 `sret` 决定，未固定改成用户态或额外开启中断。

## 必要验证

在 WSL 登录环境的 `code` 目录执行：

```bash
make clean
make DEFS=-Dsbadaddr=stval
riscv64-unknown-elf-objdump -d --disassemble=__alltraps --disassemble=__trapret bin/kernel
```

| 检查 | 实际结果 |
| --- | --- |
| 干净构建 | 通过；完成所有源码编译、内核链接和 `bin/ucore.img` 生成，命令退出码为 0 |
| 入口指令 | 确认保存帧后执行 `mv a0,sp` 和到 `trap` 的 `jal`，随后衔接 `__trapret` |
| 恢复指令 | 确认 CSR 来自偏移 256、264；全部可写通用寄存器按槽位恢复，最后为 `ld sp,16(sp)` 和 `sret` |
| 断点处理后返回、寄存器动态检查 | 未执行；当前 `exception_handler()` 仍为触发 `panic()` 的占位，依赖 C 的真实断点处理 |

本次反汇编中，`__alltraps` 位于 `0xffffffffc0200a10`，`__trapret` 位于 `0xffffffffc0200a78`。这些是当前构建的地址，后续代码变化时可重新定位。

`DEFS=-Dsbadaddr=stval` 沿用 A1 已确认的工具链兼容方式，使原有 `SAVE_ALL` 中的旧 CSR 名称可由当前汇编器识别，没有修改该宏本身。

本次没有启动 QEMU、运行完整评分或构造测试替身。后续运行仍固定使用 **QEMU 4.1.1**：

```bash
make qemu DEFS=-Dsbadaddr=stval QEMU=/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64
```

A2 实现阶段仅完成上述构建与反汇编检查。C 的真实异常处理合入后，A 已完成 A3 的 35 分主测及 A4 的 25 分复测，见 [A3+A4 测试记录](./a3_a4_test.md)。

实际提示词及执行记录保存在 `report/_work/prompts/ziqiu.md`。A1、A2 实现及相关记录已提交至个人分支 `work/lab3/ziqiu`，后续测试结果按 A3+A4 归档。
