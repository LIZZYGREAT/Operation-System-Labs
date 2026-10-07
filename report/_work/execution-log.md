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

