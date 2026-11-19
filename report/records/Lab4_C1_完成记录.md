# Lab4 C1 进程调度完成记录

分支为 `work/lab4/ziqiu/c1`。实现依据见 [C1 进程调度提示词](../prompts/prompt_zq_c1.md)。

## 实现

- [proc.c](../../code/kern/process/proc.c) 中的 `cpu_idle()` 在收到重调度请求时调用 `schedule()`。`proc_run()` 在关中断期间更新 `current`、写入 Sv39 `satp`、刷新 TLB 并切换上下文。
- [sched.c](../../code/kern/schedule/sched.c) 中的 `schedule()` 沿进程链表轮转，选择首个可运行线程，找不到时退回 `idleproc`，并更新所选线程的 `runs`。
- [switch.S](../../code/kern/process/switch.S) 中的 `switch_to()` 保存并恢复 `ra`、`sp` 和 `s0` 至 `s11`。新线程首次切入沿现有的 `forkret` 路径进入线程入口。

## C2 验证结果

初次调试使用 QEMU 7.0.0，在临时副本中调整了启动加载和页表条件。最终改用 QEMU 4.1.1 和 GCC 兼容参数，在 WSL 临时副本中用当时的工作区源码执行仓库原有的 `make qemu` 和 `make grade`。

启动日志显示 `initproc` 进入 `init_main()` 并打印预期的三行输出。`make grade` 的调度项通过，完整评分为 `30/30`，退出码为 `0`。复现命令和截图见[个人实验报告](../personal_reports/report_ziqiu.md)。
