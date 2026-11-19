# Lab4 B1：进程创建与初始化完成记录

负责人：向宇航；学号：2413318。日期：2026-11-17。

## 实现范围

工作分支为 `work/lab4/yuhang/b1`。仅补全 `code/kern/process/proc.c` 的 `LAB4 function2` 区域，实现 `alloc_proc`、`copy_mm`、`copy_thread`、`do_fork`、`kernel_thread`、`proc_init` 六个函数，保留原公共接口及 A/C 代码。

- PCB 清零，初始 PID 为 -1，使用内核根页表物理地址，链表节点独立初始化。
- 内核线程共享内核地址空间，`mm=NULL`，不实现用户进程地址空间复制。
- 每个普通线程拥有独立内核栈，栈顶下方存放 Trapframe；Context 指向 `forkret` 和该 Trapframe，恢复后进入 `kernel_thread_entry`，以正确参数调用目标函数。
- 创建失败返回负错误码，已取得的 PCB 和内核栈按对应路径释放；成功创建的线程在现场完整后登记、计数并唤醒。PID 与索引登记操作保存、关闭并恢复本核中断。
- idle 的 PID=0、使用启动栈、不进入普通进程索引；init 的 PID=1、父线程为 idle，名字分别为 `idle`、`init`。`current` 初始为 idle，其 `need_resched` 置位。

现有 `set_proc_name` 固定读取 15 字节，本次调用使用完整大小的名称缓冲区，避免从短字符串字面量越界读取；未修改共享名称接口。

四段式需求提示词见 [prompt_yh.md](../prompts/prompt_yh.md)，不记录实际对话。原 `prompt_个人.md` 未修改。

## 验证结果

### 正式工作区编译

在 `code/` 下执行：

```bash
make obj/kern/process/proc.o
make
```

进程管理目标文件编译成功，无该文件的编译诊断。完整构建在链接阶段失败，原始诊断为：

```text
code/kern/init/init.c:35: undefined reference to `cpu_idle'
make: *** [Makefile:130: bin/kernel] Error 1
```

其中 `cpu_idle` 属于尚未实现的 C 模块。本次未增加空桩或修改正式启动入口。原始构建日志保存在本机 `/tmp/lab4-b1-proc-build.log`、`/tmp/lab4-b1-build.log`，它们是临时文件，不是仓库交付物。

### 独立 QEMU 进程创建验证

为在 C 完成前验证 B，使用 `/tmp/lab4-b1-check-4fqabbrh/code` 的独立工程副本构建验证内核。正式工作区中的启动文件、评分文件及 C 区域保持不变。

副本中把原始 `tools/test_lab4.c` 原样加入内核编译；启动流程在 `proc_init()` 后调用原始 `test_lab4_process_create()` 和补充检查，然后通过 SBI 关机。该入口没有启用时钟，也没有进入 `cpu_idle` 或执行任何新线程。未使用的调度测试随原工程的链接垃圾回收被移除，不提供 C 的替代实现。

补充检查临时附加在副本的 `proc.c` 中，以访问内部辅助函数和计数，覆盖：

- PCB 全字段初始值、名称清零、两个链表空环及共享页表地址。
- Trapframe 位于独立内核栈内；非零栈参数、子线程返回值、输入帧不变及其余字段完整复制。
- init 的默认栈顶、16 字节对齐、函数入口、函数参数、SPP/SPIE/SIE 状态位、名字及父指针。
- 额外线程获得 PID=2，完成登记和唤醒，中断使能状态保持不变；检查后从索引移除并释放该测试线程资源。
- 临时设置进程数量达到上限时，`do_fork` 返回 `-E_NO_FREE_PROC`，空闲物理页数和索引保持不变；检查后恢复计数。

副本构建返回 0，QEMU 返回 0；实际输出包括：

```text
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
kmalloc_init() succeeded!
check_vmm() succeeded.
[Test Process Create] PASS
[B1 extended creation checks] PASS
```

运行命令：

```bash
cd /tmp/lab4-b1-check-4fqabbrh/code
make
qemu-system-riscv64 -machine virt -nographic -bios default \
  -device loader,file=bin/ucore.img,addr=0x80200000
```

原始日志保存在本机 `/tmp/lab4-b1-check-build.log`、`/tmp/lab4-b1-check-qemu.log`。上述命令依赖已准备的临时验证副本；其结果证明创建与现场构造检查通过，不代表正式内核的调度运行或完整评分通过。

### 审查与检查

`git diff --check` 通过。PCB 分配失败、栈分配失败和 `copy_mm` 返回错误后的资源清理经过代码审查；本次未通过故障注入运行覆盖内存不足路径。当前 `copy_mm` 在支持的内核线程调用下恒成功，其错误清理分支用于保持创建接口完整。

## 集成后待验收

等待 C 实现 `cpu_idle`、`schedule`、`proc_run`、`switch_to` 后，在完整工程执行构建、QEMU 和原始 `make grade`，确认首次执行链及多次调度。本次未运行完整评分，不声明 `30/30`。

原 `do_exit()` 仍为教学框架的 panic 占位；本次独立检查未进入该路径，正式运行时应记录它在 `init_main()` 返回后的行为。

尚未进行其他组员的独立复核，未提交、推送或合并分支。开始实现时分工方案文档已有未提交改动，本次保留该改动。
