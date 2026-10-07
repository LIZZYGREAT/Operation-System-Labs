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

