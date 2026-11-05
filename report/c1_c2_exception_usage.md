# Lab3 C1/C2：异常处理与验收

执行日期：2026-11-04。负责人：向宇航，学号 `2413318`。分支：`work/yuhang/c1-c2`。

## 功能理解与 Spec

Trap 链路依次完成上下文保存、C 层分发、异常处理、上下文恢复和 `sret` 返回。A 的入口将 Trap frame 传给 `trap()`，`trap_dispatch()` 按 `scause` 的最高位区分中断与异常；C1 在 `exception_handler()` 中处理同步异常。修改 `tf->epc` 后，A 的恢复流程把它写回 `sepc`，使执行流越过引发异常的指令。

前置条件：Trap frame 有效，实验内核中需要跳过的指令地址可读，目标指令属于 16/32 位编码范围。

| 异常 | 输出及后置条件 |
| --- | --- |
| `CAUSE_ILLEGAL_INSTRUCTION` | 输出 `Illegal instruction at 0x%016lx` 及换行，打印原 EPC，然后按指令长度更新 EPC。 |
| `CAUSE_BREAKPOINT` | 输出 `Breakpoint at 0x%016lx` 及换行，打印原 EPC，然后按指令长度更新 EPC。 |
| 其他异常 | 调用 `print_trapframe(tf)` 后返回，不改变 EPC，不额外 panic。 |

实现读取 EPC 处首个 `uint16_t`，在本实验的编码范围内，低两位不等于 `0b11` 表示 16 位指令，跳过 2 字节；否则跳过 4 字节。两个已处理分支共用这一小段判断，不需要完整反汇编器。使用半字读取也允许从仅按两字节对齐的指令地址读取前缀。

实现仅修改 `exception_handler()`，未修改 B 的中断处理、A 的内存和上下文代码、函数签名或正式评分脚本。新增贡献注释为 `lab3 C1 - 2413318`。

本实现不处理更长的指令编码，也不扩展用户地址探测。未知异常的兜底只负责诊断；真实的不可恢复异常若原地返回，仍可能再次触发异常，不将其描述为恢复成功。

## 版本与环境

| 项目 | 值 |
| --- | --- |
| 基线 | `cdc9814801ed48fd6ab18420eba371a6ef5909af`，已合入 A、B 的实现 |
| C1 实现 | `efa5e989fc196c21f6b9f2b0b3778d5397a5bcf5` |
| 编译器 | SiFive GCC-Metal `riscv64-unknown-elf-gcc` 10.2.0 |
| 模拟器 | QEMU 4.1.1，`virt`，默认 OpenSBI |
| 兼容参数 | `DEFS=-Dsbadaddr=stval`，沿用已有成员验证方式 |

## C2 测试结果

| 检查 | 实际结果 |
| --- | --- |
| C1 单文件编译 | `make -C code obj/kern/trap/trap.o DEFS=-Dsbadaddr=stval`，退出码 0。 |
| 官方评分 | `make -C code grade DEFS=-Dsbadaddr=stval`，退出码 0，总分 **100/100**。 |
| C 的主测 | 非法指令及兜底项 **25/25**；出现非法指令返回 PASS、软件中断提示、未知中断 cause 15、未知异常 cause 20 的 Trap frame。 |
| 集成检查 | 上下文与断点项 **35/35**，持续时钟项 **40/40**。仅作本次集成检查，不代替 A、B 的主测或 C3 复测。 |
| 直接 EPC 检查 | 16/32 位非法指令、16/32 位断点四项分别增加 **2、4、2、4** 字节。 |
| 实际 Trap 返回 | 四种固定编码均在 QEMU 触发 Trap 后执行后续输出，全部 PASS。 |
| 未知异常 | 直接调用 cause 20、EPC `0x1234` 的 handler，打印 Trap frame 后返回，EPC 保持 `0x1234`。 |
| 补充测试进程 | 构建退出码 0，QEMU 退出码 0，约 10.34 秒，未触发 20 秒超时。 |

补充测试使用 `.2byte 0x0000`、`.4byte 0x0000007b` 触发非法指令，使用 `.2byte 0x9002`、`.4byte 0x00100073` 触发两种断点。显式编码避免汇编器自动压缩导致测试长度不确定。直接调用检查和真实 Trap 返回检查相互补充。

官方日志见 [_work/logs/c2-grade.log](./_work/logs/c2-grade.log)。补充测试的构建、串口及状态分别见：

- [_work/logs/c2-extended-build.log](./_work/logs/c2-extended-build.log)
- [_work/logs/c2-extended-qemu.log](./_work/logs/c2-extended-qemu.log)
- [_work/logs/c2-extended-status.json](./_work/logs/c2-extended-status.json)
- [_work/logs/c2-grade-status.json](./_work/logs/c2-grade-status.json)

官方评分在临时副本运行且结束后删除其串口日志；另一次独立补测保留完整串口，其中包含原 25 分用例及额外长度检查。串口日志仅将 CRLF 换行归一化为 LF，输出内容不变。记录为文本日志，未生成测试截图。

## 补充测试复现

以下命令从仓库根目录执行，所有注入仅作用于唯一临时副本：

```bash
c2_root=$(mktemp -d /tmp/lab3-c2-reproduce.XXXXXX)
cp -a code "$c2_root/code"
python3 code/tools/inject.py "$c2_root/code"
python3 report/_work/c2_extended.py "$c2_root/code"
make -C "$c2_root/code" clean
make -C "$c2_root/code" 'DEFS=-Dsbadaddr=stval -DGRADE_LAB3_ILLEGAL'
(
    cd "$c2_root/code" || exit 1
    timeout 20s qemu-system-riscv64 \
        -machine virt -nographic -bios default \
        -device loader,file=bin/ucore.img,addr=0x80200000 \
        -serial file:c2-qemu.log -monitor none -no-reboot
)
cat "$c2_root/code/c2-qemu.log"
```

应出现四条 EPC PASS、四条实际返回 PASS、兜底 EPC PASS、`[C2 extra] ALL PASS` 以及原 25 分测试通过标记。自动补测实际使用目录和各步骤退出状态保存在状态 JSON 中。

## 实现修正与总结

计划阶段曾考虑将非法指令固定跳过 4 字节、断点固定跳过 2 字节。重新核对手册后，改为按指令前缀判断；固定长度方案没有写入实现或提交。本次实现首次编译、官方评分及补充测试均通过，没有虚构运行故障或额外修复迭代。

本模块体现同步异常与返回地址之间的关系：异步中断通常保留执行位置，而本实验主动跳过已处理的异常指令。Trap frame 是软件保存的执行状态，`sepc`、`sret` 是硬件返回机制，两者共同保证执行流继续。

本实验未实现完整的进程级异常隔离、向用户程序发送信号、缺页恢复或调度切换；兜底打印与这些机制的职责不同。

整理后的示例 Prompt 见 [个人 Prompt](./_work/prompts/yuhang.md)，沿用 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 模板；
