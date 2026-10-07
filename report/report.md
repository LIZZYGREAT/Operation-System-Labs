# 操作系统实验报告：Lab1 RISC-V 启动流程

## 实验基本信息

| 项目 | 内容 |
|---|---|
| 实验名称 | Lab1：RISC-V 内核启动与调试 |
| 小组成员 | `[请本人填写：学号-姓名]` |
| 完成日期 | 2026-10-07 |
| 仓库分支 | `work/lab1/wenjie` |
| 实验记录 | [逐步执行记录](./_work/execution-log.md) |

### 小组分工

本次记录只覆盖当前工作区内完成的 Lab1 启动验证。成员姓名、学号和分工请根据实际小组情况填写；执行环境没有提供这些个人信息，因此这里不代填。

## 一、实验目的

1. 从 Makefile、链接脚本、汇编入口和 C 初始化代码中梳理 ucore 的启动路径。
2. 使用课程指定的 RISC-V 工具链进行干净构建，并通过 ELF 头、链接符号和反汇编确认内核入口与栈的位置。
3. 在 QEMU 中验证 OpenSBI 能否把控制权交给内核，并用 GDB 动态观察复位地址、内核入口、栈设置和 `kern_init`。
4. 保留每一步的目的、命令、结果和偏差，便于复现实验及讲解。

## 二、实验环境

| 项目 | 实际环境 |
|---|---|
| 主机环境 | Ubuntu 22.04.5 LTS，运行于 WSL2；x86_64，16 个可用处理器 |
| GNU Make | 4.3 |
| RISC-V GCC | `riscv64-unknown-elf-gcc` 10.2.0 |
| RISC-V GDB | `riscv64-unknown-elf-gdb` 10.1.0 |
| QEMU | `qemu-system-riscv64` 4.1.1 |
| 工具安装位置 | `~/os-lab/toolchain/current`、`~/os-lab/qemu/current` |
| 实验代码目录 | `Operation-System-Labs/code/` |
| AI 工具 | Codex（GPT-6 系列） |

调试命令使用 `localhost:1234` 连接 QEMU GDB stub。受限执行环境第一次禁止创建该本机 socket；在受控的本机调试运行中重试后通过。后台非交互 shell 也曾未继承工具目录 PATH，显式加入 `~/os-lab` 下的 QEMU 和工具链目录后解决。整个 Lab1 验证没有遇到需要 `sudo` 或外网下载的问题。

## 三、实验整体逻辑分析

本实验追踪的是从 QEMU 上电到 ucore C 初始化函数的控制流：

```text
QEMU reset ROM 0x1000
        ↓
OpenSBI firmware 0x80000000
        ↓
ucore ELF 入口 kern_entry 0x80200000
        ↓
设置 sp = bootstacktop = 0x80203000
        ↓
kern_init 0x8020000a
        ↓
清零 BSS、输出启动信息并保持运行
```

QEMU 的 `qemu` 目标通过 `-machine virt -nographic -bios default` 启用 virt 机器和默认固件，再把 `bin/ucore.img` 加载到 `0x80200000`。`debug` 目标另外使用 `-s -S`：开启 GDB stub 并让 CPU 初始暂停。GDB 读取 `bin/kernel`，使用其中的 ELF 符号设置 `kern_entry`、`kern_init` 断点。

链接脚本 `code/tools/kernel.ld` 把 `kern_entry` 设为 ELF 入口，并从 `0x80200000` 开始布局。`code/kern/init/entry.S` 先把 `sp` 设为 `bootstacktop`，再跳到 `kern_init`。`code/kern/init/init.c` 清零 `[edata, end)`，输出 `(THU.CST) os is loading ...`，随后进入无限循环。因此普通运行预期不会自行退出，测试用超时结束 QEMU 属于预期控制方式。

## 四、实验内容与实现

### 4.1 仓库与源码检查

**目的：** 确认当前分支、起始提交和已有未跟踪内容，再从源码建立待验证的启动路径，避免误删工作区原有记录或把源码推断当作运行证据。

**检查结果：** 当前分支为 `work/lab1/wenjie`，起始提交为 Starter Code `230c83e`。开始时已有未跟踪的 `report/_work/` 环境记录；本次保留这些文件，只新增并提交明确列出的 Lab1 记录。`code/tools/` 中没有 `grade.sh`。

**课程代码改动：** 没有修改 C、汇编或 Makefile。Starter Code 已具备本实验需要验证的最小启动路径，故本次重点是构建、静态检查和动态验证，避免为通过验证而改动课程代码。

### 4.2 干净构建与静态检查

**目的：** 先删除构建输出再编译，以确认结果来自当前源码；随后检查 ELF 架构、入口地址和符号，并用反汇编解释汇编伪指令实际生成的指令。

**命令：** 在 `code/` 目录依次运行 `make clean`、`make`，再运行 `file`、`readelf -h`、`nm -n` 和 `objdump -d` 检查产物。

| 检查内容 | 结果 |
|---|---|
| 构建 | 退出码 0；生成 `bin/kernel` 和 `bin/ucore.img` |
| ELF 类型/架构 | 静态 ELF64 RISC-V executable；raw 镜像为 data |
| ELF 入口 | `0x80200000` |
| 关键符号 | `kern_entry=0x80200000`；`kern_init=0x8020000a`；`bootstack=0x80201000`；`bootstacktop=0x80203000`；`edata=end=0x80203008` |
| 入口反汇编 | `la sp, bootstacktop` 展开为 `auipc` 和 `mv`；`tail kern_init` 跳转到 `0x8020000a` |

ELF 保留符号和调试信息，供 GDB 定位函数；`ucore.img` 是供 QEMU loader 使用的裸镜像。链接地址与 Makefile 的加载地址均为 `0x80200000`，两者一致。

### 4.3 普通 QEMU 启动

**目的：** 不连接 GDB，先确认默认 OpenSBI 固件和内核可以在 QEMU 中完成启动。

**命令：** `timeout 10s make qemu`。命令在 10 秒后返回 124，因为 `kern_init` 按设计进入无限循环；判定启动结果时检查控制台输出和遗留进程。

**结果：** 控制台先输出 OpenSBI v0.4 和 QEMU virt 平台信息，再输出 `(THU.CST) os is loading ...`。超时后无残留 QEMU 进程。最终复跑也观察到相同输出。

### 4.4 GDB 启动链与栈检查

**目的：** 在运行时确认 reset ROM、OpenSBI、内核汇编入口及 C 初始化函数的地址顺序，并直接验证 `kern_entry` 设置的栈值。

**方法：** `make debug` 启动 `-s -S` QEMU；GDB 脚本连接 `localhost:1234`，从复位地址单步五次，然后在 `kern_entry` 和 `kern_init` 设置符号断点。命令文件与完整输出保存在 `report/_work/logs/41-lab1-startup-trace.gdb`、`46-lab1-startup-trace.log`；最终复跑对应 `52` 至 `55` 号日志。

| GDB 停点/动作 | 观测结果 | 解释 |
|---|---|---|
| 初始 PC | `0x1000` | QEMU reset ROM 起点 |
| 对 reset ROM 单步 5 次 | `PC=0x80000000` | 执行流到达 OpenSBI 固件入口 |
| 命中 `kern_entry` | `PC=0x80200000` | 固件已将控制权交给 ucore |
| 执行栈设置第一条指令之前 | `sp=0x8001bd80`，`bootstacktop=0x80203000` | 此时 SP 仍是 OpenSBI 栈地址 |
| 单步执行 `auipc sp,0x3` 后 | `PC=0x80200004`，`sp=0x80203000` | SP 已设置为内核栈顶；GDB 相等断言通过 |
| 再执行第二条指令 | `PC=0x80200008`，`sp=0x80203000` | SP 保持正确，下一条跳转到 C 初始化入口 |
| 命中 `kern_init` | `PC=0x8020000a`，`sp=0x80203000` | PC 与函数符号一致，栈指针仍等于 `bootstacktop`；GDB 入口断言通过 |

GDB 脱离后让目标继续运行，QEMU 串口仍输出 ucore 启动信息。结束时检查无 QEMU 进程，`localhost:1234` 端口已关闭。

### 4.5 AI 协作与迭代记录

本次任务以 Starter Code 验证和实验报告整理为主，没有让 AI 实现课程功能，也没有修改课程源码。因此不存在可如实报告的代码生成迭代次数或功能实现提示词。`report/prompt.md` 保留了实际用户任务与执行约束，没有编造模型对话。逐步执行记录和每次环境诊断见 [execution-log.md](./_work/execution-log.md)。

## 五、测试与验证

| 验证项目 | 结果 | 证据 |
|---|---|---|
| `make clean && make` | 通过，退出码 0 | `_work/logs/48-lab1-final-clean-build-static.log` |
| ELF 架构、入口和符号 | 通过：RISC-V，入口 `0x80200000` | 同上；初次静态检查见 `_work/logs/35-lab1-static-verify.log` |
| 普通 QEMU 启动 | 通过；出现 ucore 启动信息，超时码 124 符合预期 | `_work/logs/50-lab1-final-qemu-output.log`、`_work/logs/51-lab1-final-qemu-check.log` |
| GDB 地址链、栈和 C 入口 | 通过；两项 GDB 断言均通过 | `_work/logs/54-lab1-final-replay-gdb.log`、`_work/logs/55-lab1-final-replay-cleanup.log` |
| `make grade` | 未运行 | Makefile 的 `grade` 目标会调用缺失的 `tools/grade.sh`，因此没有运行该目标 |

### 截图待补

当前 `report/images/` 中只有 `.gitkeep`，没有真实终端截图。请报告提交前从实际终端/调试器截取并保存以下内容，再把表中占位状态替换成对应图片，不能用日志文本伪装截图：

| 建议文件名 | 截图内容 | 状态 |
|---|---|---|
| `lab1-build.png` | `make clean` 和 `make` 成功输出 | 待本人截图 |
| `lab1-qemu.png` | OpenSBI 与 ucore 启动信息 | 待本人截图 |
| `lab1-gdb-reset.png` | GDB 初始 PC `0x1000` 及 OpenSBI 地址 `0x80000000` | 待本人截图 |
| `lab1-gdb-kernel.png` | `kern_entry`、`sp=bootstacktop=0x80203000` 与 `kern_init` | 待本人截图 |

## 六、实验总结与收获

### 对操作系统启动过程的理解

1. 固件入口和内核入口是两个不同阶段：QEMU 首先从 `0x1000` 的 reset ROM 开始，然后到 OpenSBI 的 `0x80000000`，再由固件跳入加载在 `0x80200000` 的 ucore。
2. 内核入口汇编先建立自己的栈，再进入 C 函数。若过早使用 C 运行时而没有有效栈，函数调用和局部状态都无法可靠工作。本次 GDB 直接观察到 `sp` 从 OpenSBI 地址变为 `bootstacktop`。
3. ELF 和裸镜像用途不同：ELF 用于保留入口、符号及调试信息；QEMU 的 loader 加载 raw `ucore.img`。链接脚本与 QEMU 加载地址必须一致。
4. 本次 Lab1 Starter Code 只验证最初启动和串口输出，不涵盖中断初始化、内存管理、进程调度等后续 OS 原理内容；不能把这次最小启动结果等同于完整操作系统功能。

### AI 协作开发的经验

把任务路径、可修改目录和逐节提交要求明确给出后，实验记录可以直接对应到报告。记录命令时应写清目的、退出码和关键输出；例如普通 QEMU 因无限循环返回 124 是有上下文的预期结果。调试器连接失败时，区分本机 socket 权限与外网问题；后台 shell 里显式核对 PATH，可以避免把环境问题误判为源码问题。AI 生成的结论仍应由 ELF、串口输出和 GDB 寄存器观察支撑。

本次各实验小节的记录已按要求分别提交。最终课程源码无改动，构建产物和 QEMU 调试资源均已清理。逐节提交记录见仓库 Git 历史。
