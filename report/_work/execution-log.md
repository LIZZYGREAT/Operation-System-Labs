# Lab1 执行记录

> 记录原则：每一步写明目标、执行原因、命令、退出码、关键输出、结论及偏差。原始输出保存在同目录 `logs/`，避免把大量终端内容塞入最终报告。学生姓名/学号和人工截图由本人最终补齐。

## 基本信息

- 日期：2026-10-07 至 2026-10-08
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

## 第 8 节：交互式复习中的工具环境复核

### 目的与原因

用户按学习流程亲自重跑工具检查，确认当前终端的 PATH 与 Lab1 所需工具可用。工作区执行规范位于 `../docs/lab1/Lab1_Agent_Execution_Plan.md`；本次复核后将继续按阶段执行干净构建。

### 执行命令与结果

| 命令 | 退出码 | 关键结果 |
|---|---:|---|
| `command -v make`、`make --version` | 未记录 | `/usr/bin/make`，GNU Make 4.3 |
| `command -v riscv64-unknown-elf-gcc`、`--version` | 未记录 | 工具链路径可解析，GCC 10.2.0 |
| `command -v riscv64-unknown-elf-gdb`、`--version` | 未记录 | 工具链路径可解析，GDB 10.1 |
| 查询 `riscv64-unknown-elf-ld`、`objcopy`、`objdump`、`readelf`、`nm` | 未记录 | 五个工具均打印出工具链路径 |
| `command -v qemu-system-riscv64`、`--version` | 未记录 | QEMU 路径可解析，版本 4.1.1 |

### 结论

所有必要工具都能从当前终端找到，版本与先前记录一致，环境检查通过。用户粘贴的输出没有包含退出码，因此此处不补写推测值。此次只做了工具查询，没有清理、构建或修改 `code/`。

### 原始输出

- `logs/57-lab1-guided-environment-check.log`

## 第 9 节：记录规则统一与归档范围调整

### 目的与原因

继续动手前先做一次项目审查，结果发现四处规则冲突，若不先统一会导致返工：① `prompt.md` 自身的记录约定（只存用户提示词原文）与 README §9、执行计划第十九节（记完整交互过程）不一致；② `report/_work/` 被误纳入提交，而 README §9 又要求提交前删除它；③ 截图命名存在三套互不相同的规范；④ `report.md` 中指向 `./_work/...` 的链接在 `_work` 不提交后必然失效。此外报告里的日期、AI 工具表述也与实际不符。

### 执行命令与结果

| 操作 | 结果 |
|---|---|
| 重写 `report/prompt.md` | 明确"只存用户提示词原文"的约定；补回被删的原始任务提示词与目录/命令约束 |
| 编辑 `.gitignore` | 增加忽略项（见本节"归档范围的两轮调整"） |
| `git rm -r --cached report/_work` | 60 个文件取消跟踪，**本地文件全部保留** |
| 编辑 `report/report.md` | 日期改为 `2026-10-07 至 2026-10-08`；AI 工具改为 `Codex、Claude Code（deepseek-flash）`；截图统一为数字前缀命名；`./_work/...` 链接改为纯文字说明并注明"过程记录、不纳入提交" |
| 检查构建产物是否被忽略 | 发现 `code/bin/`、`code/obj/` **未被忽略**，补入 `.gitignore` |

**归档范围的两轮调整：**

1. 第一轮：`.gitignore` 增加 `report/_work/`，整个目录退出提交。
2. 第二轮（按用户意见修正）：改为**只忽略 `report/_work/logs/`**，把 `Lab1_学习笔记.md` 与 `execution-log.md` 重新纳入提交——即"叙事记录进 git，原始终端日志不进"。`git add` 恢复这两个文件，`logs/` 下的 59 个原始终端日志从仓库移除并保持忽略。

### 结论

记录规则统一完成：`prompt.md` 只存用户提示词原文；`_work` 下学习笔记与执行记录提交、原始日志不提交；截图命名唯一；报告元信息与实际一致。构建产物已确认不会被误提交。

### 原始输出

- 本节以文件编辑为主，未新增原始日志文件；关键结论可通过 `git status`、`git check-ignore -v` 复核。

## 第 10 节：用户亲手复现 Lab1 的四个部分

### 目的与原因

此前 Lab1 的验证由 AI 执行并留有日志。本节由用户按执行计划的四个部分亲手重跑，目的是让每一项结论都能由本人在终端中亲自观察得到，而不是采信转述；同时把源码、静态产物、QEMU 输出与 GDB 寄存器四层证据串成一条完整链。

### 执行命令与结果

| 部分 | 命令 | 关键结果 | 判定 |
|---|---|---|---|
| 部分 1 构建 | `make clean`；`make`；`find`；`file`；`ls -lh` | 8 条 `+ cc`、`+ ld bin/kernel`、objcopy 生成镜像；`bin/kernel` 为 ELF（48K，含调试信息），`bin/ucore.img` 为 data（13K） | 通过 |
| 部分 2 静态 | `readelf -h`；`nm -n`（用 `grep -e` 写法）；`objdump -d` | `Entry point address: 0x80200000`；`kern_entry=0x80200000`、`kern_init=0x8020000a`、`bootstack=0x80201000`、`bootstacktop=0x80203000`、`edata=end=0x80203008`；`kern_entry` 展开为 `auipc sp,0x3` / `mv sp,sp` / `j 8020000a` | 通过 |
| 部分 3 普通运行 | `timeout 10s make qemu` | OpenSBI v0.4 横幅、`Firmware Base: 0x80000000`、`PMP0: 0x80000000-0x801fffff`，随后 `(THU.CST) os is loading ...`；退出码 124；无残留进程 | 通过 |
| 部分 4 GDB | 终端 A `make debug`，终端 B `make gdb` | `pc=0x1000`；5 次 `si` 后 `pc=0x80000000`；命中 `kern_entry` 时 `sp=0x8001bd80`，执行 `auipc` 后 `sp=0x80203000` 且等于 `&bootstacktop`；命中 `kern_init` 时 `pc=0x8020000a`、`sp=0x80203000`；`continue` 后串口输出启动信息 | 通过 |

**额外观测：** `objdump` 输出的 `kern_init` 前 6 条指令即 `memset(edata, 0, end - edata)` 的参数准备（`a0`/`a1`/`a2`），其中 `sub a2,a2,a0` 算出的长度是 0；`nm` 中 `etext` 不存在，印证 `PROVIDE` 只对"被引用且未被定义"的符号生效。

### 偏差与小插曲

1. `nm` 首次执行时 `grep -E '...'` 的引号在粘贴中被吞掉，shell 把 `|` 当作管道，报 `kern_entry|...: command not found`；改用多个 `-e`、不带引号的写法后正常。
2. `find bin obj -maxdepth 2` 只列出了 `obj/libs/*.o`，`obj/kern/**/*.o` 因深度限制被截断；实际 `obj/` 下共有 8 个 `.o`。
3. GDB 会话中用户误在提示符下敲入 `gdb`（`Undefined command`），并两次把寄存器写成 `%sp`（GDB 应使用 `$sp`）；均不影响结论。

### 结论

四个部分全部通过，启动链 `0x1000 → 0x80000000 → 0x80200000 → kern_entry → kern_init` 由用户亲自观测确认。收尾检查无残留 QEMU 进程、`1234` 端口已释放。

### 原始输出

- 本节由用户在终端直接执行，原始输出以截图形式保存于 `report/images/`（见第 11 节），未另存文本日志。

## 第 11 节：截图整理、报告更新与过程参考资料归档

### 目的与原因

把用户的真实终端画面整理为可核验的证据，插入报告对应小节；同时归档用户新加入仓库的过程参考文档。

### 截图整理

用户提供的截图原始为 6 张已命名文件加 2 张未命名文件。检查发现那 2 张未命名截图分别为 `nm` 符号表与 `readelf -h` + `objdump`，**恰好补上了原方案遗漏的"部分 2：静态入口分析"证据**。按实验顺序重排为 8 张：

| 文件名 | 内容 | 对应小节 |
|---|---|---|
| `01-build-success.png` | 干净构建输出与产物类型 | 4.2 |
| `02-static-elf-entry.png` | `readelf -h` 入口地址 + 入口反汇编 | 4.2 |
| `03-static-elf-symbols.png` | `nm -n` 关键符号 | 4.2 |
| `04-qemu-start.png` | OpenSBI 横幅与 ucore 启动信息 | 4.3 |
| `05-gdb-reset-vector.png` | `PC=0x1000` 与 reset ROM 反汇编 | 4.4 |
| `06-gdb-opensbi.png` | `PC=0x80000000` | 4.4 |
| `07-gdb-kernel-entry.png` | `kern_entry` 与 `sp` 切换 | 4.4 |
| `08-gdb-kern-init.png` | 命中 `kern_init` | 4.4 |

`report.md` 已在 4.2/4.3/4.4 插入 8 处图片引用，§五 的"截图待补"表改为"实验截图"；经脚本校验，8 个引用全部对应真实文件，无缺失、无旧名残留。

### 过程参考资料归档

用户把工作区 `docs/lab1/` 下的三份原理文档复制进 `report/_work/`，作为过程参考与复习材料：

| 文件 | 性质 |
|---|---|
| `Lab1_前置知识.md` | 面向初学者的原理讲解（从 CPU 首次取指到内核打印） |
| `Lab1_RISC-V讲解.md` | 同类原理讲解，另一版本 |
| `Lab1_RISC-V启动实验_任务原理代码与执行流程.md` | 任务、原理、代码结构与执行流程，篇幅最长 |

三份文档与工作区 `docs/lab1/` 中的同名文件内容一致，属重复归档；保留在 `report/_work/` 便于脱离 `docs/` 也能查阅。

### 结论

8 张截图与报告引用一一对应，报告证据链完整；过程参考资料已归档。

## 第 12 节：组员信息与分工

### 目的与原因

报告模板中的小组成员与分工为占位内容，需按小组实际情况填写后才能提交。

### 完成内容

| 成员 | 学号 | 分工 |
|---|---|---|
| 甘文杰 | 2412700 | 在 `work/lab1/wenjie` 分支完成 Lab1 启动验证并整理记录 |
| 王子楸 | 2412712 | 前期环境搭建（RISC-V 工具链与 QEMU 安装、PATH 配置）、项目流程与相关知识梳理 |
| 向宇航 | 2413318 | 复核完成过程有无问题、独立二次复现、报告整理 |

已写入 `report.md` 的"小组成员"与"小组分工"两处。

### 结论

报告个人信息与分工填写完毕，不再有占位内容。

### 原始输出

- 本节为报告内容编辑，未新增原始日志文件。
