# Lab3 C1/C2： Prompt

## C1：异常处理

### [PROMPT]

任务：在 `code/kern/trap/trap.c` 中替换 `exception_handler()` 的占位实现，完成非法指令、断点和其他异常的处理。

操作要求：先阅读实验手册、任务分工及真实源码，在 `work/yuhang/c1-c2` 分支修改实际文件。只修改此函数，保持函数签名、Trap frame 布局和其他函数不变，保留实验标记，新增 `lab3 C1 - 2413318` 注释。不扩展用户态地址探测或长指令解码。

输出要求：说明异常分类、输出格式和 EPC 更新依据。先编译验证，再独立提交 C1 实现。

### [RELY]

- `trap.h` 中的 `struct trapframe`，其中 `cause` 为异常原因，`epc` 为触发异常的指令地址。
- `riscv.h` 中的 `CAUSE_ILLEGAL_INSTRUCTION`、`CAUSE_BREAKPOINT`。
- `defs.h` 中的 `uint16_t`、`uintptr_t`，现有 `cprintf()` 和 `print_trapframe()`。
- A 已实现 Trap 保存与恢复，B 已实现中断处理。统一恢复流程会将修改后的 EPC 写回 `sepc`。
- 本实验处理可读内核代码地址处的 16/32 位指令，使用指令首个半字的低两位区分长度。

### [GUARANTEE]

```c
void exception_handler(struct trapframe *tf);
```

### [SPECIFICATION]

Pre-Condition：`tf` 指向有效 Trap frame；需要跳过的异常指令地址可读，指令长度属于本实验的 16/32 位范围。

Post-Condition：非法指令与断点输出原始 EPC，更新返回地址到下一条指令；其他异常输出 Trap frame 后返回。

Case 1：非法指令。输出 `Illegal instruction at 0x%016lx\n`，地址参数为原始 EPC；读取 EPC 处的 `uint16_t`，低两位不等于 `0b11` 时增加 2，否则增加 4。

Case 2：断点。输出 `Breakpoint at 0x%016lx\n`，按相同的编码规则更新 EPC，同时支持压缩断点与普通断点。

Case 3：其他异常。只调用 `print_trapframe(tf)`，不修改 EPC，不添加 panic。

Requirements：异常输出在 EPC 更新之前完成；不得将所有非法指令或断点的长度固定为同一个值。复用两个已处理分支的长度判断，保持实现简洁。

## C2：异常与兜底验收

### [PROMPT]

任务：在 C1 实现及 A、B 依赖具备的版本上，完成 C 的 25 分主测，并记录完整评分及必要的指令长度补充测试。

操作要求：正式评分脚本保持不变；补充测试只注入独立 `/tmp` 副本。保存真实日志、退出状态、环境和实现提交。测试结果与实现限制如实记录，不虚构故障或迭代。不执行推送、合并或 C3 复测。

输出要求：交付 C1/C2 使用记录、可复现的补充测试脚本与证据，更新 README 过时的占位提示，独立提交 C2 材料。

### [RELY]

- `code/Makefile` 的 `grade` 入口及 `tools/inject.py`、`grade.sh`、`test_lab3.c`、`test_lab3_context.S`。
- 本机 RISC-V 工具链和 QEMU；沿用 `DEFS=-Dsbadaddr=stval` 兼容定义。
- 原始评分中的软件中断、未知中断和未知异常用例。

### [GUARANTEE]

交付 `report/c1_c2_exception_usage.md`、测试日志及可复现补充测试脚本。生产代码不增加测试接口。

### [SPECIFICATION]

Pre-Condition：C1 实现已提交，A 的内存和 Trap 链路、B 的中断处理均可用。

Post-Condition：记录真实的 C2 25 分结果及完整集成评分；补充验证 16/32 位指令的 EPC 增量、实际返回和未知异常 EPC 不变。

Case 1：官方评分。运行 `make grade DEFS=-Dsbadaddr=stval`，核对 35/35、25/25、40/40 和总分 100/100，保存完整输出与命令退出码。

Case 2：长度补测。在临时副本使用固定编码产生 16/32 位非法指令和断点，验证每次异常返回后能继续执行；直接调用 handler 检查 EPC 分别增加 2/4 字节。

Case 3：兜底。构造未知异常并直接调用 handler，确认打印后返回且 EPC 不变；复用官方用例核对软件中断和未知中断提示。

Requirements：检查失败时保留证据并定位原因。只修复 C1 范围内的缺陷；依赖模块的问题归属对应成员。未执行的工作不描述为通过。
