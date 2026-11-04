# Lab3 B1/B2：中断处理与持续时钟测试

执行日期：2026-11-03。负责人：B（学号 `2412700`）。分支：`work/lab3/wenjie`。

## B1 实现规格

实现位于 `code/kern/trap/trap.c` 的 `interrupt_handler()`。处理器按 RISC-V `scause` 最高位识别并去掉中断标记，再分派中断编号：

| 中断 | 行为 |
| --- | --- |
| `IRQ_S_TIMER` | 调用 `clock_set_next_event()` 预约下一次事件，递增 `ticks`；每 `TICK_NUM`（100）次调用 `print_ticks()`。 |
| `IRQ_S_SOFT` | 输出 `Supervisor software interrupt`。 |
| 其他编号 | 输出 `Unknown interrupt: cause N` 并打印 Trap frame，之后返回分派流程。 |

分工文档要求达到条件后关机，但没有给出数值阈值。实现采用 `TICK_SHUTDOWN_NUM = TICK_NUM * 10`，即 1000 次时钟中断。现有 40 分测试在 `ticks == 200` 输出通过标记后继续空转；1000 次阈值让评分标记先输出，再验证 SBI 关机。QEMU 实测在 10 次 `100 ticks` 输出后正常退出。

`exception_handler()`、A1 的内存管理文件、A2 的 `trapentry.S` 均未修改。

## B2 测试记录

环境：GNU Make 4.3、SiFive GCC-Metal `riscv64-unknown-elf-gcc` 10.2.0、QEMU 4.1.1（OpenSBI v0.4）。当前工具链构建沿用 A2 记录的 `-Dsbadaddr=stval` 兼容定义。

| 验证 | 命令 / 方式 | 结果 |
| --- | --- | --- |
| 完整评分入口 | `make grade DEFS=-Dsbadaddr=stval` | 退出码 2，总分 40/100。B 的持续时钟项通过 40/40；35 分断点项和 25 分非法指令项因当前 `exception_handler()` 仍为占位而失败。完整评分尚未通过。 |
| B 计时项独立复现 | 在 `/tmp/lab3-b2.aZNRyU/code` 的临时副本执行 `python3 tools/inject.py ...`，以 `DEFS=-Dsbadaddr=stval -DGRADE_LAB3_TIMER` 构建，再用 20 秒 `timeout` 启动 QEMU | 注入与构建退出码均为 0；QEMU 退出码为 0，运行约 8.2 秒，早于 timeout。串口输出包含一次 `[Test 3] ... PASS (40/40, ticks=200)` 和 10 次 `100 ticks`。结合代码在第 1000 次中断调用 `sbi_shutdown()`，这验证了关机路径；QEMU 串口没有单独打印“关机”文字。 |
| 软件与未知中断分支 | 在同一临时副本中用 `GRADE_LAB3_INTERRUPT_FALLBACK` 构建测试变体，只跳过 C 的异常用例，直接调用现有 B 分支测试 | 构建、QEMU 退出码均为 0。串口出现 `Supervisor software interrupt`、`Unknown interrupt: cause 15` 和 Trap frame 输出。 |
| 进程清理 | QEMU 与 GDB 进程名查询 | 两次 QEMU 运行结束后均未发现残留 `qemu-system-riscv64` 进程；未发现残留 `riscv64-unknown-elf-gdb` 进程。 |

完整评分日志和串口记录保存在 `report/_work/logs/`。独立分支测试在 `/tmp` 临时副本进行；未改动仓库中的测试注入脚本或 A/C 源文件。

独立计时复现的关键命令如下；构建 stdout/stderr 在 `b2-timer-build.log`，QEMU 串口在 `b2-timer-qemu.log`：

```bash
# 从仓库根目录
python3 code/tools/inject.py /tmp/lab3-b2.aZNRyU/code

# 在 /tmp/lab3-b2.aZNRyU/code 中
make clean
make V= 'DEFS=-Dsbadaddr=stval -DGRADE_LAB3_TIMER'
timeout 20s qemu-system-riscv64 \
  -machine virt -nographic -bios default \
  -device loader,file=bin/ucore.img,addr=0x80200000 \
  -serial file:/home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal/report/_work/logs/b2-timer-qemu.log \
  -monitor none -no-reboot
```

`GRADE_LAB3_INTERRUPT_FALLBACK` 变体使用相同构建和 QEMU 参数，串口文件为 `b1-fallback-qemu.log`；仅临时副本中的 `test_lab3.c` 增加了该宏分支，并跳过异常用例，以直接调用软件和未知中断分支。

## 提交

- B1：`7bd243a feat(lab3/b): implement interrupt handling`
- B2：本报告及测试记录随本阶段提交。

按用户后续要求，已删除本成员的临时 Prompt 记录。当前分支没有中央 `report/prompt.md`；A、C 成员的 Prompt 记录保留。
