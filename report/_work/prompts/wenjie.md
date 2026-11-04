# Lab3 B1/B2/B3：Prompt

## 前置问题 1：先弄清楚 Lab3 到底要完成什么

### [PROMPT]

我想先把这次实验的整体任务弄明白，不急着看某一个函数。能不能从内核启动开始，按实际运行顺序带我梳理一下：内存初始化、设置 Trap 入口、初始化时钟、开启中断分别在做什么？A、B、C 三个人的代码最后又是怎么连起来的？先说明主线，再讲每个人负责的部分和测试依赖。

### [RELY]

- `report/任务分工.md` 中 A1—A4、B1—B4、C1—C3 的职责和测试依赖。
- `code/kern/init/init.c` 中 `kern_init()` 的执行顺序。
- `code/kern/mm/pmm.c`、`code/kern/trap/trap.c`、`code/kern/trap/trapentry.S`、`code/kern/driver/clock.c`。
- 当前实验的三项主测：A 的 35 分、B 的 40 分、C 的 25 分。

### [GUARANTEE]

本次只讨论和解释，不修改文件。输出一份按时间顺序组织的启动流程，并说明三个成员的实现是怎样相互依赖的。

### [SPECIFICATION]

Pre-Condition：以当前仓库中的 Lab3 工程为起点，假设我只知道内核可以由 QEMU 启动，还没有完全理解 Trap 的执行过程。

Post-Condition：我能够说清楚 `kern_init()` 中主要初始化步骤的先后关系，知道 B1 的代码什么时候开始参与运行，以及为什么 B2、B3 都依赖其他成员的工作。

Case 1：从启动进入 `kern_init()`，解释 `dtb_init()`、`pmm_init()`、`idt_init()`、`clock_init()`、`intr_enable()` 各自的作用，并以当前源码为准说明实际调用顺序；如果有重复调用，也直接指出。

Case 2：将 A 的内存与现场保存恢复、B 的中断处理、C 的异常处理连接到同一条执行流程，而不是把它们当作互相独立的功能。

Case 3：解释为什么 35 分测试需要断点异常处理，40 分测试需要持续的时钟中断，25 分测试还需要软件中断及未知中断的输出。

Requirements：重点解释“为什么先做这一步，以及它给后一步提供了什么”，暂时忽略与主线无关的底层细节。

## 前置问题 2：一次 Trap 发生以后，CPU 和内核分别做了什么

### [PROMPT]

我现在知道大概的初始化顺序了，但还有一个疑问：程序本来正在执行，为什么发生中断以后就能跑到我们写的 C 函数？处理完又为什么能回到原来的位置？

### [RELY]

- `code/kern/trap/trapentry.S` 的 `SAVE_ALL`、`__alltraps`、`RESTORE_ALL`、`sret`。
- `code/kern/trap/trap.h` 的 `struct pushregs` 与 `struct trapframe`。
- `code/kern/trap/trap.c` 的 `idt_init()`、`trap()`、`trap_dispatch()`、`interrupt_handler()`、`exception_handler()`。
- `code/kern/driver/intr.c` 的中断开关函数。

### [GUARANTEE]

不修改代码。给出从触发到返回的完整调用与状态变化过程，并区分硬件自动记录的状态、软件保存的寄存器和 C 函数读取或修改的信息。

### [SPECIFICATION]

Pre-Condition：`stvec` 已设置到 `__alltraps`，内核正在正常执行，并且时钟中断已经开启。

Post-Condition：能够解释 Trap frame 的来源和去向，以及 `tf->epc` 最后如何影响 `sret` 返回的位置。

Case 1：CPU 发生 Trap 时记录 `sepc`、`scause`、`sstatus` 等控制状态，转到 `stvec` 对应的入口；说明此时通用寄存器还需要软件保存。

Case 2：`SAVE_ALL` 在栈上建立 Trap frame，汇编将帧地址通过 `a0` 传入 `trap(tf)`；C 层根据 `cause` 分派处理，返回后恢复保存的状态。

Case 3：对比时钟中断与断点异常。前者通常从被打断的位置继续执行，后者在本实验中需要把保存的 EPC 推进到下一条指令，避免反复触发同一个断点。

Requirements：不要只解释函数名；每一步要说明此时 `sp`、Trap frame 和返回地址处在什么状态。

## 前置问题 3：先搞清楚实验怎么测试，再决定怎么检查自己的任务

### [PROMPT]

同一个实验会分成 35、25、40 三组测试，直接运行 `make grade` 后，脚本是怎么分别检查这三组内容的？

请你结合实际脚本解释构建、注入测试、启动 QEMU、读取串口输出、判断 PASS 的过程。
### [RELY]

- `code/Makefile`、`code/tools/grade.sh`、`code/tools/inject.py`。
- `code/tools/test_lab3.c`、`code/tools/test_lab3_context.S`。
- `report/任务分工.md` 的主测与交叉复测要求。

### [GUARANTEE]

只分析测试，不修改评分脚本和源码。输出三项测试各自的触发方式、检查条件、依赖模块与能证明的范围。

### [SPECIFICATION]

Pre-Condition：A、B、C 的相关实现已存在，可以在具备 RISC-V 工具链和 QEMU 的环境中执行评分。

Post-Condition：能够区分“评分脚本检查了某个输出”“函数实际执行了某条路径”和“整个功能在所有情况下都正确”这三种不同层次的结论。

Case 1：分析 `GRADE_LAB3_CONTEXT`：为什么要设置寄存器、触发 `ebreak`，返回后又逐项比较值。

Case 2：分析 `GRADE_LAB3_ILLEGAL`：除了非法指令返回，还为什么要检查软件中断、未知中断和未知异常输出。

Case 3：分析 `GRADE_LAB3_TIMER`：为什么需要连续达到 200 次时钟中断，而不是只收到第一次就通过。

Requirements：说明超时、正常退出、脚本匹配输出三者的区别。不要把同一次完整评分自动等同于每个成员都完成了独立复测。

## B1 问题 1：中断处理函数是怎么区分类型的

### [PROMPT]

我负责的是 B1，也就是 `interrupt_handler()`。我现在的理解是：Trap 入口已经由 A 保存好现场，传到 C 代码以后，我应该只负责判断中断类型并执行对应操作，不需要再保存一遍寄存器。

请你结合代码说明 `tf->cause` 从哪里来，为什么要把 `scause` 的最高位和后面的原因编号分开，以及 `IRQ_S_TIMER`、`IRQ_S_SOFT` 和未知中断分别应该怎么处理。尤其是 `trap_dispatch()` 已经区分了中断与异常，为什么进入 `interrupt_handler()` 后仍要取出编号？

### [RELY]

- `code/kern/trap/trap.c` 中 `trap_dispatch()`、`interrupt_handler()`、`print_trapframe()`。
- `code/kern/trap/trap.h` 中 `struct trapframe`。
- `code/libs/riscv.h` 中相关中断编号和 CSR 定义（以仓库实际文件为准）。
- A 的 `__alltraps` 和 C 的 `exception_handler()` 已分别负责现场链路与异常路径。

### [GUARANTEE]

本次先解释 B1 的设计和现有实现，不修改 A、C 代码。重点核对下面的接口及它的分支行为。

```c
void interrupt_handler(struct trapframe *tf);
```

### [SPECIFICATION]

Pre-Condition：`tf` 指向 Trap 入口保存的有效现场，当前调用由 `trap_dispatch()` 判定为中断。

Post-Condition：根据中断编号完成对应处理，普通可返回的中断分支返回统一 Trap 恢复流程；本函数不负责执行 `sret`。

Case 1：时钟中断。先调用 `clock_set_next_event()`，再更新 `ticks`，按规定次数输出，并在达到设定的关机阈值时执行关机。

Case 2：Supervisor 软件中断。输出 `Supervisor software interrupt`，不按时钟中断进行计数。

Case 3：其他中断。输出 `Unknown interrupt: cause N`，调用 `print_trapframe(tf)`，说明为什么保留现场信息方便排查。

Requirements：区分 `tf->cause` 原始值和去掉中断标记后的原因编号；不要把中断原因编号与异常原因编号混为一谈。

## B1 问题 2：为什么时钟中断里还要设置下一次事件

### [PROMPT]

我有一个实现上的疑问：`clock_init()` 已经调用过 `clock_set_next_event()`，为什么每次进入时钟中断以后还要再调用一次？

请你从 `clock_set_next_event()` 的具体代码出发解释时钟是怎样工作的。然后帮我检查 B1 当前的处理顺序：先预约下一次事件，再计数，每 100 次输出一次，达到 1000 次后关机。

### [RELY]

- `code/kern/driver/clock.c` 的 `clock_init()`、`clock_set_next_event()` 和 `ticks`。
- `code/kern/trap/trap.c` 的 `TICK_NUM`、`TICK_SHUTDOWN_NUM`、`print_ticks()`、`interrupt_handler()`。
- `code/libs/sbi.c` 与相关头文件中的 `sbi_set_timer()`、`sbi_shutdown()`。
- `code/tools/test_lab3.c` 的时钟测试与 `report/b1_b2_interrupt_usage.md`。

### [GUARANTEE]

不新增功能。对照源码解释下列已有接口之间的配合关系。

```c
void clock_init(void);
void clock_set_next_event(void);
void interrupt_handler(struct trapframe *tf);
```

### [SPECIFICATION]

Pre-Condition：启动期间已经完成 `clock_init()`，中断开关允许处理时钟事件，`ticks` 从零开始。

Post-Condition：能够解释为何持续时钟中断依赖重新预约事件，清楚区分 100 次输出、200 次评分和 1000 次关机三个条件。

Case 1：`clock_set_next_event()` 通过读取当前时间并调用 `sbi_set_timer()` 设置下一次事件，而不是自动建立永久重复的定时器。

Case 2：每次时钟中断到达后，先预约下一次事件再增加 `ticks`。在 `ticks` 为 100 的整数倍时调用 `print_ticks()`。

Case 3：现有 B1 实现将 `TICK_SHUTDOWN_NUM` 设为 `TICK_NUM * 10`，在第 1000 次调用 `sbi_shutdown()`；这是代码选择的阈值，不要写成实验分工已经指定了 1000。

Requirements：结合真实时序说明。不能把“输出 PASS (40/40, ticks=200)”误解为 QEMU 已经正常关机。

## B2 问题 1：40 分时钟测试应该如何判断是否真正通过

### [PROMPT]

B1 实现以后，我想检查它到底有没有连续工作。帮我看看 40 分评分的触发过程：测试程序在哪里等待 `ticks` 到 200，评分脚本又需要匹配什么输出？

### [RELY]

- `code/tools/test_lab3.c` 的 `test_lab3_timer_is_armed()`、`test_lab3_wait_for_timer()`。
- `code/tools/grade.sh` 的 `GRADE_LAB3_TIMER` 与输出匹配条件。
- `report/b1_b2_interrupt_usage.md` 和 `report/_work/logs/b2-grade.log`。
- 现有构建环境可能需要 `-Dsbadaddr=stval` 的 CSR 兼容参数。

### [GUARANTEE]

只分析和指导核验，禁止为了拿分去修改评分脚本。给出可以直接复查的命令、预期输出及失败时的定位顺序。

### [SPECIFICATION]

Pre-Condition：A 的 Trap 入口和 B 的时钟处理已可用，当前工作目录是仓库的 `code/`。

Post-Condition：能根据实际日志区分 B2 是否通过、完整评分是否通过，以及版本和依赖可能造成的差异。

Case 1：执行完整评分；记录构建命令、QEMU 版本、工具链版本、返回码，以及 `GRADE_LAB3_TIMER` 对应的 40/40 结果。使用的兼容参数以实际环境为准，例如 `make grade DEFS=-Dsbadaddr=stval`。

Case 2：确认时钟测试同时出现 `100 ticks` 和包含 `ticks=200` 的通过标记；说明评分脚本通过串口输出匹配判定，而不是直接证明所有可能的中断情形。

Case 3：如果 35 分或 25 分失败，查看失败发生在哪个测试分支，并检查对应 A/C 依赖，不能只根据总分把失败全部归到 B 的时钟处理。

Requirements：保留原始输出，不编造自己没有跑过的命令、得分或退出码。

## B2 问题 2：需要怎样检查关机和兜底分支

### [PROMPT]

我发现 40 分测试主要关注 `ticks` 能否到 200，但 B1 还有关机、软件中断和未知中断这些分支。仅靠评分 PASS 好像不能证明它们都正确执行了。

我想把这些额外检查做清楚：一方面让内核继续运行到 1000 次，看 `sbi_shutdown()` 是否真的使 QEMU 退出；另一方面单独验证软件中断和未知中断的打印。帮我解释不同日志分别能说明什么。

### [RELY]

- `code/kern/trap/trap.c` 的 B1 分支、`TICK_NUM`、`TICK_SHUTDOWN_NUM`。
- `code/tools/inject.py`、`grade.sh`、`test_lab3.c`。
- `report/b1_b2_interrupt_usage.md`。
- `report/_work/logs/b2-timer-qemu.log`、`b1-fallback-qemu.log` 及对应构建日志。

### [GUARANTEE]

说明独立测试的设计与证据要求。正式源码、官方评分脚本和其他成员的实现不修改；如需补充测试，仅在 `/tmp` 的独立副本中进行。

### [SPECIFICATION]

Pre-Condition：B1 的中断处理已经实现，基本时钟评分至少能跑到 200 次。

Post-Condition：可以分别核对持续运行后的关机行为、每 100 次一次的输出，以及软件中断和未知中断的兜底打印。

Case 1：持续时钟测试。让 QEMU 运行到设定关机阈值，记录实际退出码、是否超时、`100 ticks` 出现次数及评分通过标记。对于当前实现，预期在 1000 次中断时调用 `sbi_shutdown()`；不能只凭没有输出就认定关机成功。

Case 2：软件中断。构造合理的 Trap frame 或执行对应中断路径，检查 `Supervisor software interrupt`，并确认没有误更新 `ticks`。

Case 3：未知中断。使用不属于当前已处理分支的中断编号，检查 `Unknown interrupt: cause N` 及 Trap frame 信息；说明直接调用 handler 测的是分支逻辑，不等于硬件实际触发了该中断。

Requirements：分别保存或指出日志来源。明确“官方评分”和“额外补测”不是同一项测试；未运行的补测只列为建议，不标记为通过。

## B3 问题 1：复测 A 之前，我需要理解哪些代码

### [PROMPT]

现在准备做 B3，也就是复测 A 的 35 分项和内存基础。带我先看懂 A 的两条主线：第一条是内存从 DTB 信息变成可管理的空闲页，第二条是 `ebreak` 触发后，寄存器如何保存到 Trap frame，再被恢复。

### [RELY]

- `code/kern/mm/pmm.c`、`best_fit_pmm.c`、`memlayout.h`、`pmm.h`。
- `code/kern/trap/trapentry.S` 与 `trap.h`。
- `code/tools/test_lab3_context.S` 和 `test_lab3.c`。
- `report/a1_memory_usage.md`、`report/a2_trap_usage.md`、`report/a3_a4_test.md`。

### [GUARANTEE]

只对 A 的实现进行只读分析，不修改队友代码。输出两条可以独立跟踪的执行流程，并解释 35 分测试具体比较了什么。

### [SPECIFICATION]

Pre-Condition：A1、A2 已有真实实现，C 的断点异常处理能够推进 `tf->epc`，这样断点测试才可以正常返回。

Post-Condition：理解内存自检与 Trap 上下文测试分别在检验什么，也知道通过一次 35/35 不等于每个寄存器或每种异常都已被动态测试覆盖。

Case 1：内存。说明 `dtb_init()` 提供物理内存范围，`page_init()` 建立页描述符和保留区，Best-Fit 管理剩余空闲页；检查管理器是否实际被选中，并关注 `check_alloc_page() succeeded!` 等真实自检输出。

Case 2：保存现场。`SAVE_ALL` 将通用寄存器与控制状态按 `trapframe` 的布局存入栈，`a0` 把帧首地址传给 `trap()`；当前 RV64 的帧为 36 个 8 字节槽。

Case 3：恢复现场。`RESTORE_ALL` 使用帧中的 `status`、`epc` 更新 CSR，再恢复通用寄存器，最后恢复原始 `sp`，由 `sret` 返回。

Case 4：35 分测试。测试汇编先给 26 个寄存器设置已知值，执行 `ebreak`，返回后逐项比较；评分还检查 `Breakpoint at` 输出，因此既依赖 A 的现场恢复，也依赖 C 的断点处理。

Requirements：区分“我本次复测 A 的代码”和“C 的异常处理是 35 分项的必要依赖”；不要把测试没有逐项比较的 `sp`、`ra` 说成已经被该用例直接逐值检查。

## B3 问题 2：怎样独立复测并记录结果，才算完成交叉检查

### [PROMPT]

我已经完成了 B3 的操作，请你帮我按原始版本、编译命令、运行结果和实际覆盖范围整理检查思路。

### [RELY]

- `report/任务分工.md` 的 B3 复测职责。
- `report/a1_memory_usage.md`、`report/a2_trap_usage.md`、`report/a3_a4_test.md`，作为参考而不是本人的独立复测证据。
- `code/tools/grade.sh`、`test_lab3_context.S` 和当前 `trapentry.S`。
- 本人 B3 实际使用的提交号、测试命令、构建环境、QEMU 输出和日志（以运行时记录为准）。

### [GUARANTEE]

只协助核对和整理 B3 证据，不修改 A 的代码或官方测试。最终应形成一份有版本、命令、观察结果和结论的复测记录；未提供的数据保持待补充。

### [SPECIFICATION]

Pre-Condition：A 已完成 A3 的 35 分主测，B 已自行执行 B3 复测，并且有可追溯的运行版本或输出。

Post-Condition：可以独立说明 B3 的测试依据与结果，不把 A 的日志当作自己的复测日志；能区分内存自检、35 分断点上下文和完整 100 分评分。

Case 1：核对测试环境和版本。记录分支、提交号、实际使用的 GCC 与 QEMU 版本，以及 CSR 兼容参数（如有）。若从个人分支取代码，说明依赖是否已包含 A 和 C 的实现。

Case 2：核对内存基础。检查内存探测、Best-Fit 管理器初始化和固定分配器自检的真实输出；如进行额外 GDB 检查，应分别记录观察到的页面数量和链表状态。

Case 3：核对 35 分项。重新构建并执行原始测试，检查 `Breakpoint at`、寄存器比较后的 PASS 以及 `35/35`，解释这些结果各自对应的代码阶段。

Case 4：完整评分如果同时得到 25/25、40/40，应作为本次集成测试的附带结果记录；不能用它代替 B3 的主要结论，也不能在缺少日志时虚构具体执行时间或退出码。

Requirements：输出保持学生实际记录的口吻，清楚说明“我验证了什么、通过什么现象判断、还有什么没有单独验证”。遇到不能确认的结果，明确标为待核对，不要补写不存在的调试过程。

---
