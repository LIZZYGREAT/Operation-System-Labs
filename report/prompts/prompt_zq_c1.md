[PROMPT]

**任务**

在 `A:/OS/exp/Operation-System-Labs` 的 `work/lab4/ziqiu/c1` 分支完成 Lab4 C1 进程调度，使内核启动后能从 `idleproc` 切换到 `initproc` 并执行 `init_main`。

- `code/kern/process/proc.c` 实现 `cpu_idle` 和 `proc_run`，负责响应空闲线程的调度请求，以及切换当前线程、页表和执行现场。
- `code/kern/schedule/sched.c` 实现 `schedule`，负责从进程链表选择可运行线程并在需要时触发切换。
- `code/kern/process/switch.S` 实现 `switch_to`，负责保存和恢复 RV64 上下文。

**操作要求**

必须在项目中修改实际文件。只在上述三个文件的 `//LAB4 function3 YOUR CODE` 标记处补全四个函数，保留现有声明和逻辑。按前置条件直接实现，将分支和检查限定在规格要求的行为内。

**输出要求**

以工程实际代码为准，使用 [RELY]、[GUARANTEE] 和 [SPECIFICATION] 给出的接口、签名和行为。完成后在 `code/` 下用 `make qemu` 验证启动，以控制台输出 `this initproc, pid = 1, name = "init"` 和 `To U: "Hello world!!".` 为 C1 验收依据。`init_main` 返回后，现有 `do_exit()` 会触发 `process exit!!.` 的 panic。简要列出改动文件和实际验证结果。

[RELY]

- `proc.h` 提供 `struct proc_struct`、`struct context`、`PROC_RUNNABLE` 和 `le2proc(le, list_link)`。调度使用 `state`、`need_resched`、`runs` 和 `list_link`，切换使用 `pgdir` 和 `context`。
- `proc_list` 是带表头的双向循环链表，通过 `list_next()` 遍历。`idleproc` 在链表之外。`proc_init()` 将 `current` 设为 `idleproc`，设置它的 `need_resched`，并创建已加入链表、状态为 `PROC_RUNNABLE` 的 `initproc`。
- `sync.h` 提供 `local_intr_save(flag)` 和 `local_intr_restore(flag)`。`flag` 为 `bool`，保存调用前的本地中断使能状态。保存操作通过 `read_csr(sstatus)` 读取 `SSTATUS_SIE`，这对接口可以嵌套使用。
- `struct proc_struct` 的 `pgdir` 存放根页表物理地址，现有内核线程由 `alloc_proc()` 赋值为 `boot_pgdir_pa`。使用 `riscv.h` 的 `write_csr()`、`SATP_MODE_SV39`、`RISCV_PGSHIFT` 和 `pmm.h` 的 `flush_tlb()`。Sv39 的 `satp` 编码参照 `code/kern/mm/pmm.c` 的 `enable_paging()`。`lsatp()` 使用 Sv32 编码，本实验直接写 `satp`。
- RV64 下 `struct context` 依次存放 `ra`、`sp`、`s0` 至 `s11`，每项占 8 字节。`copy_thread()` 已将新线程的 `context.ra` 设为 `forkret`，将 `context.sp` 指向已准备好的 trapframe。首次切入沿 `forkret → forkrets → __trapret` 恢复 trapframe，再由 `sret` 进入 `kernel_thread_entry`。

[GUARANTEE]

```c
void cpu_idle(void);                                      // code/kern/process/proc.c
void schedule(void);                                      // code/kern/schedule/sched.c
void proc_run(struct proc_struct *proc);                   // code/kern/process/proc.c
void switch_to(struct context *from, struct context *to);  // code/kern/process/switch.S
```

`cpu_idle` 遵守 `proc.h` 中已有的 `noreturn` 声明。`switch_to` 使用与 `proc.c` 声明一致的全局汇编符号。

[SPECIFICATION]

## `cpu_idle`

**Pre-Condition**  `kern_init()` 已完成进程初始化并开启中断，进入时 `current == idleproc`。

**Post-Condition**  持续循环检查 `current->need_resched`，该标志为真时调用 `schedule()`。空闲线程恢复执行后继续检查。函数始终留在循环中。

## `schedule`

**Pre-Condition**  `proc_list`、`current` 和 `idleproc` 已初始化。非空闲的 `current` 已经挂入 `proc_list`。调用前本地中断可以开启或关闭。

**Post-Condition**  清除调用时 `current->need_resched`，选出下一线程，将其 `runs` 加一。仅当选中线程与 `current` 不同时调用 `proc_run(next)`。调用者恢复执行时恢复它进入 `schedule()` 前的本地中断状态。

- **Case 1**  当前线程是 `idleproc` 时，从 `proc_list` 的第一个进程开始。否则从当前线程的后继节点开始，遇到表头时绕回链表开头。选中遇到的第一个 `PROC_RUNNABLE` 线程。非空闲的当前线程在绕回后最后参与本轮选择。
- **Case 2**  遍历一轮后没有 `PROC_RUNNABLE` 线程时，选中 `idleproc`。空链表也走这条路径。

**Requirements**  清除调度标志前保存并关闭本地中断，贯穿遍历、计数和切换调用。只对实际进程节点使用 `le2proc(le, list_link)`。本次调用继续执行时再调用 `local_intr_restore(flag)`。

## `proc_run`

**Pre-Condition**  `current` 指向当前线程。`proc` 指向已准备好运行的线程，具有有效的上下文和页对齐的根页表物理地址。

**Post-Condition**  目标线程执行时，`current`、`satp` 和 CPU 上下文均对应目标线程。发生切换时，原线程的上下文已保存。原线程日后恢复执行时，从 `switch_to()` 调用处继续，并恢复它进入 `proc_run()` 前的本地中断状态。

- **Case 1**  `proc == current` 时直接返回。
- **Case 2**  `proc != current` 时，保存并关闭本地中断，记录旧线程 `prev`，将 `current` 更新为 `proc`。按 `((uintptr_t)SATP_MODE_SV39 << 60) | (proc->pgdir >> RISCV_PGSHIFT)` 构造 `satp`，其中 ASID 为 0。写入 `satp` 后调用 `flush_tlb()`，再调用 `switch_to(&prev->context, &proc->context)`。原线程恢复后调用 `local_intr_restore(flag)`。

**Requirements**  `current` 更新、`satp` 写入、TLB 刷新和上下文切换按顺序在本地中断关闭时执行。新线程首次运行时由现有的 trapframe 恢复路径接管执行。

## `switch_to`

**Pre-Condition**  按 RV64 调用约定，`a0` 指向旧线程的 `from`，`a1` 指向已准备好恢复的 `to`。调用方已完成 `current` 和页表切换，并保持本地中断关闭。

**Post-Condition**  用 `sd` 将旧线程的 `ra`、`sp`、`s0` 至 `s11` 保存到 `from`，再用 `ld` 从 `to` 恢复同一组寄存器。`ret` 跳到恢复后的 `ra`，使用恢复后的 `sp` 继续执行。首次切入时，这个返回地址是 `forkret`。

**Requirements**  在 `.text` 中导出 `switch_to`。按 `struct context` 布局使用偏移 `0`、`8`、`16` 至 `104`，步长为 8 字节。汇编入口只负责这组寄存器的保存、恢复和返回。
