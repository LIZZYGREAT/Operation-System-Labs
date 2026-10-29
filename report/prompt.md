# Prompt 记录

## 2026-10-29：添加中断与异常处理函数的临时占位

用户请求：

> 那你直接帮我补一下，然后推到 Git 上，然后说明提示一下这个东西只是占位用的。

本次处理范围：

- 在 `code/kern/trap/trap.c` 的 `lab3 function2 YOUR CODE` 区域定义
  `interrupt_handler()` 和 `exception_handler()`，保留原有待实现标记。
- 两个函数均为临时占位：调用时打印 Trap frame 并触发 `panic`，明确报告尚未实现。
- 在代码注释和 README 中说明占位用途，后续需要替换函数体完成真实处理逻辑。
- 本次不实现中断、异常处理，不修改测试脚本或初始化流程。

验证结果：

- 在 WSL Ubuntu 的 `code` 目录执行 `make obj/kern/trap/trap.o`，编译成功。
- 使用 `riscv64-unknown-elf-nm -g --defined-only obj/kern/trap/trap.o`，确认两个处理函数均生成全局函数符号。
- `git diff --check` 通过。
- 上述检查仅覆盖该源文件的编译及函数符号，不代表整个实验已完成或评分测试通过。
