# Lab1 执行记录

> 记录原则：每一步写明目标、执行原因、命令、退出码、关键输出、结论及偏差。原始输出保存在同目录 `logs/`，避免把大量终端内容塞入最终报告。学生姓名/学号和人工截图由本人最终补齐。

## 基本信息

- 日期：2026-10-07
- 仓库：`/home/lenovo/os-lab/workspace/Operation-System-Labs`
- 当前分支：`work/lab1/wenjie`
- 起始 HEAD：`230c83e92c5f115f5d137b73b35cba4e9bd42afd` (`lab1-starter`，同时指向本地 `lab1` 和 `origin/lab1`)
- 实验目标：实际构建并运行 Lab1，借助静态 ELF 信息和 QEMU/GDB 动态观测证明 `0x1000 → 0x80000000 → 0x80200000 → kern_entry → kern_init`。
- 当前未跟踪的 `report/_work/` 在本次开始前已存在，包含此前的环境检查日志；本次只新增 `30-lab1-*` 及后续 `lab1-*` 文件，不覆盖旧记录。

## 第 1 节：仓库基线与启动源码分析

### 目的与原因

先确认分支、提交起点和工作区状态，避免把原有数据误认为本次成果。再阅读 Makefile、链接脚本、入口汇编和初始化 C 代码，形成启动路径假设，后续用 ELF 和 GDB 输出验证，而不是仅凭源码断言。

### 执行命令与结果

| 命令 | 退出码 | 关键结果 |
|---|---:|---|
| `date -Is`、`pwd` | 0 | `2026-10-07T16:08:36+08:00`；位于 `/home/lenovo/os-lab/workspace/Operation-System-Labs` |
| `git status --short --branch` | 0 | 分支 `work/lab1/wenjie`；只有开始前已存在的 `?? report/_work/` |
| `git branch --show-current` | 0 | `work/lab1/wenjie` |
| `git rev-parse HEAD` | 0 | `230c83e92c5f115f5d137b73b35cba4e9bd42afd` |
| `git log -1 --oneline --decorate` | 0 | HEAD 与 `lab1-starter`、`lab1`、`origin/lab1` 同一提交 |
| `git diff --stat` | 0 | 无已跟踪文件改动 |
| `sed -n '1,210p' code/Makefile` | 0 | 前缀为 `riscv64-unknown-elf-`；QEMU 命令使用 `-machine virt -nographic -bios default`，把 `bin/ucore.img` 放到 `0x80200000`；debug 附加 `-s -S`；GDB 连接 `localhost:1234` 并读取 `bin/kernel` |
| `cat code/tools/kernel.ld` | 0 | `ENTRY(kern_entry)`、`BASE_ADDRESS = 0x80200000` |
| `cat code/kern/init/entry.S` | 0 | `kern_entry` 中先 `la sp, bootstacktop`，再 `tail kern_init` |
| `cat code/kern/init/init.c` | 0 | `kern_init` 清零 `[edata, end)`，打印 `(THU.CST) os is loading ...`，随后进入无限循环 |
| `find code/tools -maxdepth 1 -type f` | 0 | 只有 `function.mk`、`kernel.ld`；没有 `tools/grade.sh` |
| `find report/images -maxdepth 1 -type f` | 0 | 只有 `.gitkeep`，尚无真实实验截图 |

### 分析结论（待运行验证）

1. QEMU reset 起点、OpenSBI 固件入口和 ucore 链接入口是不同层次；计划验证地址分别为 `0x1000`、`0x80000000` 和 `0x80200000`。
2. 链接脚本与 QEMU loader 对 `0x80200000` 的约定必须一致；ELF 的符号信息供 GDB 使用，raw `ucore.img` 供 QEMU loader 使用。
3. `kern_entry` 的关键工作是设置栈并跳入 C 初始化函数。后续应在 GDB 中比较 `sp` 和 `bootstacktop`，并实际命中 `kern_init`。
4. 计划要求起始干净，但当前工作分支已有先前留下的未跟踪 `report/_work/`。不切换/重建已有工作分支，也不删除这些内容；在最终 Git 状态中单独说明。
5. 本节源码审查通过；启动链结论仍需后续编译、运行和 GDB 观测确认。

### 原始输出

- `logs/30-lab1-baseline.log`
- `logs/31-lab1-source-review.log`

### 提交状态

- 本节单独提交；提交 author 按用户提供的 `LIZZYGREAT <2660550447@qq.com>` 设置。

## 第 2 节：环境检查与干净构建

### 目的与原因

运行构建前先从交互式登录 shell 检查 PATH 和工具版本，确保调用的是课程所需的 RISC-V bare-metal 编译器、GDB 和 QEMU。然后执行 `make clean` 再 `make`，以排除残留目标文件造成的假成功，并实际验证 Starter Code 与新安装工具链兼容。

### 执行命令与结果

| 命令 | 退出码 | 关键结果 |
|---|---:|---|
| `bash --login -ic '... command -v/version checks ...'` | 0 | GCC/GDB/QEMU 路径均来自 `~/os-lab`；GCC 10.2.0、GDB 10.1.0、QEMU 4.1.1、GNU Make 4.3 |
| `uname -a` | 0 | x86_64 WSL2 kernel `6.18.33.2-microsoft-standard-WSL2` |
| `cat /etc/os-release` | 0 | Ubuntu 22.04.5 LTS |
| `nproc` | 0 | 16 个可用处理器 |
| `make clean`（`code/`） | 0 | 输出 `rm -f -r obj bin`，清理成功 |
| `make`（`code/`） | 0 | 使用 `riscv64-unknown-elf-gcc` 编译源码，链接 `bin/kernel`，再生成 `bin/ucore.img` |

`bash --login -ic` 在无终端场景打印 `cannot set terminal process group` 和 `no job control` 提示；这是当前执行器没有交互终端造成的提示。所有检查命令仍正常执行并退出 0，PATH 与版本输出正确。

### 结论

环境检查和干净构建均通过；没有安装额外 apt 包，也没有修改 `code/` 下源码。构建输出证明工具链能够编译并链接当前 Lab1 Starter Code。接下来需要静态检查 ELF 和符号，确认入口地址不是仅凭 Makefile 推断。

### 原始输出

- `logs/32-lab1-environment-check.log`
- `logs/33-lab1-make-clean.log`
- `logs/34-lab1-make.log`

## 第 3 节：ELF、符号和入口反汇编

### 目的与原因

构建成功本身不能证明生成了正确架构或正确加载地址。`file` 和 `readelf` 用于验证 ELF 类型、RISC-V 架构与入口地址；`nm` 用于查看入口和栈边界等链接符号；`objdump` 用于将汇编伪指令与最终机器指令对应起来。

### 执行命令与结果

| 命令 | 退出码 | 关键结果 |
|---|---:|---|
| `file bin/kernel bin/ucore.img` | 0 | kernel 是静态 ELF64 RISC-V executable；ucore.img 是裸 data 镜像 |
| `riscv64-unknown-elf-readelf -h bin/kernel` | 0 | `Machine: RISC-V`，`Entry point address: 0x80200000` |
| `riscv64-unknown-elf-nm -n bin/kernel | grep ...` | 0 | `kern_entry=0x80200000`，`kern_init=0x8020000a`，`bootstack=0x80201000`，`bootstacktop=0x80203000`，`edata=end=0x80203008` |
| `riscv64-unknown-elf-objdump -d bin/kernel | grep -A12 '<kern_entry>'` | 0 | `la sp, bootstacktop` 展开为 `auipc sp,0x3` 和 `mv sp,sp`；`tail kern_init` 展开为跳转到 `0x8020000a` |
| `ls -lh bin/kernel bin/ucore.img` | 0 | kernel 约 48 KiB；镜像约 13 KiB |

### 结论

静态证据确认入口地址是 `0x80200000`，并且 `kern_entry` 正好位于 ELF entry。链接脚本和加载地址与输出一致。`bootstacktop` 的符号地址是 `0x80203000`，因此 GDB 动态进入 `kern_init` 后可直接比较该地址与 `sp`。静态验证通过；接下来运行普通 QEMU，验证 firmware 到内核的整体启动。

### 原始输出

- `logs/35-lab1-static-verify.log`

## 第 4 节：普通 QEMU 启动

### 目的与原因

先在没有 GDB 的普通运行模式下确认整条固件到内核路径可运行。内核最后进入 `while (1)`，因此用 10 秒超时收束进程；退出码 124 在这里是预期的超时状态，是否通过以 OpenSBI 和 ucore 启动输出为准。

### 执行命令与结果

| 命令 | 退出码 | 关键结果 |
|---|---:|---|
| `timeout 10s make qemu`（`code/`） | 124（预期） | OpenSBI v0.4 启动，QEMU virt 信息正常，随后出现 `(THU.CST) os is loading ...` |
| `ps -eo pid,comm,args | awk '$2 ~ /^qemu-system/ {print}'` | 0 | 没有剩余 QEMU 进程 |

### 结论

普通启动通过。OpenSBI 输出先于 ucore 消息，符合源码假设的 firmware → kernel 顺序。超时只用于终止预期持续运行的内核，启动链在超时前已完成；QEMU 进程没有残留。接下来用 `-s -S` 和 GDB 分别停在各启动层，补充动态地址和寄存器证据。

### 原始输出

- `logs/36-lab1-qemu.log`


## 第 5 节：GDB 动态跟踪启动链与栈初始化

### 目的与原因

静态符号只能说明预期地址，不能证明 QEMU 实际按该路径执行。先从复位 ROM 单步进入 OpenSBI，再用符号断点等待固件跳入内核；在 `kern_entry` 逐条执行栈设置指令，并在 `kern_init` 断点检查 PC 和 SP。这样可以把源码、链接地址与运行时寄存器对应起来。

### 调试过程与偏差

1. 默认受限环境中的 `setsid make debug` 无法创建 QEMU 的本机 GDB socket，输出 `Failed to create a socket: Operation not permitted`。该尝试没有启动调试目标；之后在显式允许本机 `localhost:1234` 的隔离命令中重试。
2. 第一次扩大范围的调试启动因后台非交互 shell 没有课程工具目录 PATH，输出 `qemu-system-riscv64: No such file or directory`。确认是命令搜索路径问题后，在重试中明确加入 `/home/lenovo/os-lab/qemu/current/bin` 与 `/home/lenovo/os-lab/toolchain/current/bin`。没有因此改动课程 Makefile。
3. 成功会话使用 `setsid make debug` 创建独立进程组，GDB 以 `-batch -x logs/41-lab1-startup-trace.gdb` 执行记录好的断点和检查命令。完整输出在 `logs/46-lab1-startup-trace.log`。

### 观测结果

| 检查点 | 运行时结果 | 含义 |
|---|---|---|
| QEMU 复位开始 | `PC=0x1000` | CPU 从 QEMU reset ROM 开始执行 |
| 对复位 ROM 单步 5 次 | `PC=0x80000000` | 控制流进入 OpenSBI 固件 |
| 命中 `kern_entry` | `PC=0x80200000` | 固件进入 ELF 的内核入口 |
| 执行 `auipc sp,0x3` 前 | `sp=0x8001bd80`；`bootstacktop=0x80203000` | 入口执行前 SP 仍是固件使用的栈地址 |
| 执行入口第一条指令后 | `PC=0x80200004`；`sp=0x80203000` | 汇编已把 SP 设置到内核栈顶；GDB 相等断言通过 |
| 执行第二条指令后 | `PC=0x80200008`；`sp=0x80203000` | SP 保持在内核栈顶，下一条跳转指向 `kern_init` |
| 命中 `kern_init` | `PC=0x8020000a`；`sp=0x80203000` | C 初始化入口地址正确，SP 与 `bootstacktop` 相同；GDB 入口断言通过 |

GDB 脱离后，QEMU 串口日志继续输出 OpenSBI 信息和 `(THU.CST) os is loading ...`。随后终止本次独立进程组；检查确认没有遗留 QEMU 进程或 `:1234` 监听端口。

### 结论

动态启动路径与静态分析一致：`0x1000 → 0x80000000 → 0x80200000 → kern_entry → 0x8020000a (kern_init)`。在 `kern_entry` 的第一条指令后，`sp` 已变为 `bootstacktop=0x80203000`；进入 `kern_init` 时该值仍成立。本节通过。后续将按计划再做一次干净构建和关键路径复跑，作为最终复核。

### 原始输出和命令

- `logs/37-lab1-qemu-debug.log`、`logs/37-lab1-qemu-debug-state.log`：默认沙箱本机 socket 限制及进程状态。
- `logs/38-lab1-qemu-debug.log`、`logs/38-lab1-reset-vector.gdb`、`logs/39-lab1-reset-vector.log`、`logs/40-lab1-reset-cleanup.log`：复位地址单步检查及清理。
- `logs/41-lab1-startup-trace.gdb`、`logs/42-lab1-qemu-debug.log`、`logs/44-lab1-startup-cleanup.log`：首次完整跟踪启动时发现的 PATH 问题及清理结果。
- `logs/45-lab1-qemu-debug.log`、`logs/46-lab1-startup-trace.log`、`logs/47-lab1-startup-cleanup.log`：成功的完整 GDB 跟踪、QEMU 输出及清理检查。

## 第 6 节：干净重建、最终复跑与收尾

### 目的与原因

为了避免第一次调试结果偶然成功，在最终复核中重新清理并完整构建，再分别重跑普通 QEMU 和带 GDB 的启动链。静态 ELF 检查用于确认新产物入口未变；GDB 命令脚本重复检查关键 PC、SP 断言和 C 入口；最后删除生成目录并确认无调试进程/端口残留，使仓库回到实验前的构建产物状态。

### 执行命令与结果

| 命令 | 退出码 | 关键结果 |
|---|---:|---|
| `make clean`；`make`（`code/`，课程工具路径显式加入 PATH） | 0 | 从零编译、链接 `bin/kernel`、生成 `bin/ucore.img` |
| `file bin/kernel bin/ucore.img` | 0 | 重新生成的 kernel 为 ELF64 RISC-V；镜像为 raw data |
| `readelf -h bin/kernel` 筛选 Machine/Entry | 0 | `Machine: RISC-V`，入口 `0x80200000` |
| `nm -n bin/kernel` 筛选关键符号 | 0 | `kern_entry=0x80200000`，`kern_init=0x8020000a`，`bootstacktop=0x80203000` |
| `timeout 10s make qemu`（分开保存原始输出和复核结果） | 124（预期） | OpenSBI 启动后输出 ucore 提示；超时结束后无 QEMU 残留 |
| `riscv64-unknown-elf-gdb -q -batch -x logs/52-lab1-final-replay.gdb` | 0 | 再次得到 `0x1000 → 0x80000000 → 0x80200000 → 0x8020000a`；SP 断言和入口断言通过 |
| GDB 脱离后等待串口输出 | — | OpenSBI 与 `(THU.CST) os is loading ...` 均出现在本次复跑日志 |
| `make clean` | 0 | `code/bin/`、`code/obj/` 已移除，返回实验前无构建产物的状态 |
| 检查 QEMU 进程和 `:1234` | 0 | 无 QEMU 进程，GDB 端口已关闭 |

第一次编写最终 QEMU 复核包装命令时，脚本试图一边向日志写入一边从同一个日志读取，`grep` 报 `input file is also the output`。这不影响 QEMU 实验本身（当次仍观察到预期退出码和内核输出）；随后将原始输出与检查结果拆成两个文件，复核命令退出 0。原尝试和修正后的证据都保留，便于追溯。

### 最终结论

干净构建、静态 ELF 检查、普通 QEMU 启动和 GDB 关键路径复跑全部通过。运行地址和栈指针结果可重复，与启动源码及链接符号吻合。最终没有修改课程 C/汇编源码，构建和调试产物已清理。

### 原始输出和命令

- `logs/48-lab1-final-clean-build-static.log`：最终干净构建及 ELF/符号检查。
- `logs/49-lab1-final-qemu.log`：初次最终输出检查包装错误；QEMU 原始输出见该文件本身。
- `logs/50-lab1-final-qemu-output.log`、`logs/51-lab1-final-qemu-check.log`：拆分后的普通 QEMU 原始输出、退出码和进程复核。
- `logs/52-lab1-final-replay.gdb`、`logs/53-lab1-final-replay-qemu.log`、`logs/54-lab1-final-replay-gdb.log`、`logs/55-lab1-final-replay-cleanup.log`：关键启动链最终复跑。
- `logs/56-lab1-final-cleanup.log`：清理构建产物和调试资源后的检查。

## 第 7 节：实验报告与提示词记录

### 目的与原因

把执行证据整理成便于提交和讲解的实验报告，同时保留实际收到的用户任务与目录、命令约束。只填写能够由源码或运行日志证明的事实；不猜测组员学号姓名，也不伪造终端截图或 AI 编程迭代。

### 完成内容

- 将原通用模板改写为 Lab1 RISC-V 启动报告：说明环境、启动逻辑、每项验证的目的和结果、地址/符号、`make grade` 缺少脚本的原因，以及本次没有修改课程源码。
- 在 `report/prompt.md` 按实际顺序记录本次任务和前序目录/命令约束，注明没有发生课程功能代码生成迭代。
- 在报告中列出人工待填的学号/姓名，以及建议截取的四类真实终端画面；`report/images/` 当前仍只有 `.gitkeep`，没有生成或伪装截图。
- 用 `git diff --check` 检查报告差异，没有空白错误；检查报告中的旧模板占位内容已删除，保留的待填项仅为个人信息和人工截图。

### 结论

报告正文与原始执行记录相互对应，截图和个人资料的待补事项明确。当前可直接用于本人核对、补充个人信息与截图后提交课程。
