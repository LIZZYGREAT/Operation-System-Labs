# Lab2 B4：QEMU/RISC-V Sv39 与 TLB 调试执行手册

> 本手册按 B3 已核实的本机环境编写，供你逐步执行并填写真实结果。当前环境复用 QEMU 4.1.1 调试版，不编译新 QEMU、不改系统 QEMU、不改 ucore/Makefile，也不修改页表来人为制造异常。
>
> 记录时把“源码推导的预期”和“本次实际观察”分开写。没有观察到的结果标成“未运行”或“无法确认”，不要写成通过。

## 0. 本次要完成什么

沿一条确定的客体访存观察以下路径：

1. 客体指令使用的 VA、访问类型和特权级。
2. QEMU TCG 主软件 TLB 是否命中；若未命中，是否在 victim TLB 命中。
3. RISC-V 的 `riscv_cpu_tlb_fill()` 是否被调用，以及它是否进入 `get_physical_address()`。
4. Sv39 使用哪个 `satp` 根页表、访问了哪个 PTE、如何检查权限并形成 PA。
5. 成功时 PA 如何填入 QEMU 软件 TLB；失败时如何进入页错误或访问错误处理。

本手册默认先观察启动后的第一条高地址指令取指转换，再尝试观察 `kern_init()` 中的一次数据写。**当前启动页表在最高级就有可读、可写、可执行的 1 GiB 叶子映射，正常内核访问通常不会触发页错误。不要改 PTE 来强行制造页错误。**

## 1. 固定环境与源码位置

个人工作树：

```text
/home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal
```

Lab2 代码目录：

```text
/home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal/code
```

B3 已确认的 QEMU：

```text
源码：/home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1
版本：4.1.1
可调试程序：/home/lenovo/projects/os-lab/qemu/build-4.1.1/riscv64-softmmu/qemu-system-riscv64
OpenSBI 目录：/home/lenovo/projects/os-lab/qemu/qemu-4.1.1/share/qemu
```

关键源码文件：

```text
QEMU target/RISC-V：target/riscv/cpu_helper.c
QEMU 通用软件 TLB：accel/tcg/cputlb.c
QEMU x86_64 TCG 代码生成：tcg/i386/tcg-target.inc.c
QEMU RISC-V 指令翻译：target/riscv/insn_trans/trans_rvi.inc.c
ucore 启动与启动页表：code/kern/init/entry.S
ucore Sv39 位定义：code/kern/mm/mmu.h
ucore 初始化入口：code/kern/init/init.c
```

本机宿主是 x86_64，因此查 TCG 快路径时看 `tcg/i386/`；`tcg/riscv/` 是宿主机为 RISC-V 时的 TCG 后端，不是本机这次运行的后端。

## 2. 预检：确认目录、程序、符号与端口

建议先为两个交互终端开启独立记录。在终端 A 的普通 shell 中执行：

```sh
cd /home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal
mkdir -p docs/lab2/results
RUN_DATE="$(date +%Y%m%d)"
script -f "docs/lab2/results/b4_${RUN_DATE}_A_host.log"
```

`script` 会进入一个新的交互 shell；在这个 shell 中继续执行终端 A 的命令。终端 B 也执行同样的命令，但把日志名改成 `b4_${RUN_DATE}_B_guest.log`。全部结束后，先退出 GDB，再在 `script` 创建的 shell 中执行 `exit` 结束记录。这样可以保留实际输入与输出；下面的空白记录表可再根据日志填写。若当天重复运行，换一个文件名，避免覆盖上一轮日志。

新开终端，在个人工作树根目录执行：

```sh
cd /home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal
git branch --show-current
git status --short --branch
```

确认当前是 `work/lab2/ganwenjie`。`report/prompt.md` 可能已有之前任务留下的未提交改动；保留它，不要用 `git checkout`、`reset` 或清理命令处理。

确认 QEMU 的版本和调试符号：

```sh
cat /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1/VERSION
/home/lenovo/projects/os-lab/qemu/build-4.1.1/riscv64-softmmu/qemu-system-riscv64 --version
file /home/lenovo/projects/os-lab/qemu/build-4.1.1/riscv64-softmmu/qemu-system-riscv64
readelf -S /home/lenovo/projects/os-lab/qemu/build-4.1.1/riscv64-softmmu/qemu-system-riscv64 | rg '\.debug_info|\.debug_line|\.symtab'
```

确认内核符号和客体 GDB：

```sh
file code/bin/kernel
readelf -S code/bin/kernel | rg '\.debug_info|\.debug_line|\.symtab'
command -v gdb
gdb --version | head -n 1
command -v riscv64-unknown-elf-gdb
riscv64-unknown-elf-gdb --version | head -n 1
```

如果 `code/bin/kernel` 不存在或缺少调试符号，先停止并记录；不要在本任务中重建 QEMU。若只是内核需要更新，可在确认任务要求后使用仓库自己的构建目标。

确认 1234 端口未被旧 QEMU 占用：

```sh
ss -ltnp | rg ':1234\b'
```

有输出表示端口正在监听；先确认监听进程是否为遗留 QEMU，再按本手册的退出步骤处理。没有输出通常表示端口空闲；`rg` 因无匹配返回 1 属正常情况。

查看 Makefile 的实际展开，不启动 QEMU：

```sh
cd /home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal/code
make -n debug QEMU='/home/lenovo/projects/os-lab/qemu/build-4.1.1/riscv64-softmmu/qemu-system-riscv64 -L /home/lenovo/projects/os-lab/qemu/qemu-4.1.1/share/qemu'
```

输出中应包含 `-machine virt`、`-bios default`、`-s -S` 和正确的 QEMU 路径。`-s` 启动客体 GDB stub（TCP 1234）；`-S` 让客体 CPU 在启动时暂停。Makefile 只通过命令行变量覆盖 QEMU，不需要编辑 Makefile。

## 3. 推荐方式：宿主 GDB 直接启动 QEMU

此前尝试宿主 GDB attach 时实际得到过 `ptrace: Operation not permitted`。因此先用本节方法：宿主 GDB 自己启动 QEMU，不需要更改 ptrace 设置或用 sudo。这个方法把“运行 QEMU”和“调试 QEMU 宿主进程”放在同一终端，客体 GDB 仍用另一个终端。

因此本次推荐流程实际使用两个交互终端：终端 A 同时运行宿主 GDB 与其 QEMU inferior；终端 B 运行 RISC-V 客体 GDB。只有在另一个环境确认宿主 GDB 可以 attach 时，才将 QEMU、宿主 GDB、客体 GDB 分开为三个终端。

### 终端 A：启动宿主 GDB 与 QEMU

在终端 A 执行：

```sh
cd /home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal/code
make debug QEMU='gdb -q -ex "set substitute-path /home/lenovo/os-lab/qemu/src/qemu-4.1.1 /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1" -ex "set pagination off" --args /home/lenovo/projects/os-lab/qemu/build-4.1.1/riscv64-softmmu/qemu-system-riscv64 -L /home/lenovo/projects/os-lab/qemu/qemu-4.1.1/share/qemu'
```

说明：

- `set substitute-path` 将旧构建记录里的源码路径映射到当前 QEMU 源码目录，使宿主 GDB 能打开 `vl.c`、`cpu_helper.c` 等源文件。
- `-L` 指向已经存在的 OpenSBI 固件目录。缺少它时，本机 QEMU 曾报找不到 `opensbi-riscv64-virt-fw_jump.bin`。
- `make debug` 会在命令尾部加入仓库定义的虚拟机参数与 `-s -S`。
- 命令运行后应该进入**宿主机 GNU GDB**提示符。此时 QEMU 尚未由 GDB 的 `run` 命令启动。

先确认宿主 GDB 加载的是正确程序和函数：

```gdb
(gdb) info files
(gdb) info functions riscv_cpu_tlb_fill
(gdb) info functions get_physical_address
(gdb) list get_physical_address
```

若 GDB 无法识别这些符号，先停止并记录其输出，不要继续把符号缺失当成 TLB 路径未触发。

设置临时断点：

```gdb
(gdb) tbreak helper_tlb_flush
(gdb) tbreak riscv_cpu_tlb_fill if mmu_idx == 1
(gdb) tbreak get_physical_address if mmu_idx == 1
(gdb) tbreak tlb_set_page_with_attrs if mmu_idx == 1
(gdb) tbreak raise_mmu_exception
(gdb) info breakpoints
(gdb) run
```

`PRV_S` 的值是 1，因此 `mmu_idx == 1` 用来聚焦监管者模式。`MMUAccessType` 的值在 QEMU 4.1.1 中是：load=0、store=1、instruction fetch=2。所有断点都是 `tbreak`，命中后自动删除；它们不会写入项目或 QEMU 配置。

`run` 后 QEMU 会启动并停在 `-S` 设置的客体暂停状态。继续下面终端 B 的连接步骤。

### 终端 B：连接客体 RISC-V GDB

新开终端：

```sh
cd /home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal/code
make gdb
```

这是 **RISC-V 客体 GDB**，不是宿主 GDB。连接后先记录初始状态：

```gdb
(gdb) info registers pc satp
(gdb) x/4i $pc
```

通常此时客体 CPU 尚在固件/复位入口，`satp` 还未切到 ucore 的 Sv39 根页表。不要把这个初始值当成 ucore 启用分页后的值。

在客体 GDB 设置内核入口临时断点并运行：

```gdb
(gdb) tbreak kern_init
(gdb) continue
```

启动过程中，终端 A 的宿主 GDB 会依次停在设置的函数断点。终端 B 的 `continue` 可能一直显示等待，因为 QEMU **宿主进程**已经在终端 A 的断点上暂停；这是两层调试之间的执行依赖，不是 GDB stub 自动断开。

如果终端 A 先停在 `helper_tlb_flush()`，这是 `entry.S` 执行 `sfence.vma` 后触发的 TLB 刷新。记录命中位置与调用栈，然后在终端 A 输入：

```gdb
(gdb) bt
(gdb) continue
```

这样客体才能继续执行，并停到后续的 S-mode `riscv_cpu_tlb_fill()` / `get_physical_address()` 断点。每次宿主断点停住都会暂停整个 QEMU；记录完当前断点后都要在终端 A 输入 `continue`，才会到下一个宿主断点或让客体继续。

## 4. 观察一次成功的 Sv39 转换

### 4.1 在宿主 GDB 检查访问上下文

当终端 A 停在 `riscv_cpu_tlb_fill()` 时执行：

```gdb
(gdb) bt
(gdb) info args
(gdb) p env->priv
(gdb) p mmu_idx
(gdb) p access_type
(gdb) p/x address
(gdb) p/x env->satp
(gdb) list
```

先记录 `access_type`：

- `2` 是指令取指，通常对应切换到 ucore 高地址后取指。
- `0` 是数据读取。
- `1` 是数据写入。

`bt` 必须显示这是从正常 TCG 访存慢路径进入的调用链。若栈显示 `riscv_cpu_get_phys_page_debug()`，这是 GDB/调试查询触发的直接物理页查询，不是客体访存的 TLB refill；记录为“debug 查询路径”，不要混在客体访存结果里。

记录完参数和调用栈后，输入 `continue`，让同一次调用进入下面的 `get_physical_address()` 断点：

```gdb
(gdb) continue
```

继续到页表函数断点时检查：

```gdb
(gdb) bt
(gdb) info args
(gdb) p/x env->satp
(gdb) p addr
(gdb) p access_type
(gdb) p mmu_idx
(gdb) info locals
(gdb) list
```

在 `get_physical_address()` 中，关键字段包括 `mode`、`base`、`vm`、`levels`、`ptshift`、`idx`、`pte_addr`、`pte`、`ppn`、`mxr` 和 `sum`。在进入循环前部分变量尚未赋值，逐步 `next` 到 `for` 循环及 PTE 读取之后再记录：

```gdb
(gdb) next
(gdb) info locals
(gdb) next
(gdb) info locals
```

也可以从当前停止处设置一个源码行临时断点，再继续到循环体：

```gdb
(gdb) tbreak /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1/target/riscv/cpu_helper.c:237
(gdb) continue
(gdb) info locals
```

在 PTE 读取之后记录 `pte_addr` 和 `pte`。若 `-O2` 使局部变量显示 `<optimized out>`，照实记录；可继续用 `list`、`disassemble` 和函数参数确认流程，不要为了本次观察重编译或修改 QEMU。

从 `get_physical_address()` 返回至 `riscv_cpu_tlb_fill()` 后：

```gdb
(gdb) finish
(gdb) info locals
(gdb) p ret
(gdb) p/x pa
(gdb) p prot
```

确认返回码与 `pa` 后，输入 `continue`，让成功路径进入 `tlb_set_page_with_attrs()` 断点：

```gdb
(gdb) continue
```

在该断点记录：

```gdb
(gdb) bt
(gdb) info args
(gdb) info locals
(gdb) p/x vaddr
(gdb) p/x paddr
(gdb) p prot
(gdb) p mmu_idx
```

`paddr` 是送入通用 TLB 的客体物理地址页；`addend` 是 QEMU 为宿主 RAM 访问建立的偏移，不能把 `VA + addend` 误记为客体 PA。

检查完成后，在终端 A 执行 `continue`，让 QEMU 恢复；终端 B 的客体 GDB 随后应能继续并停在 `kern_init`。在终端 B 记录：

```gdb
(gdb) info registers pc satp
(gdb) x/4i $pc
(gdb) disassemble /m kern_init
```

### 4.2 当前启动页表的源码推导参考

以下是根据本仓库代码和 B3 已记录运行值推导的**核对参考**，不是你本次运行的实测结果。请用本次 GDB 显示值重新确认：

- B3 曾在 `kern_init` 观察到 `satp = 0x8000000000080205`。按 Sv39 格式，MODE=8，根页表 PPN=`0x80205`，根表物理地址为 `0x80205000`。
- `entry.S` 的 `boot_page_table_sv39` 只写第 511 项。该项是 `(0x80000 << 10) | 0xcf = 0x200000cf`，是带 V/R/W/X/A/D 的叶子 PTE。
- 高地址 `0xffffffffc02000d8` 的 VPN2 为 511，所以根页表 PTE 地址应为 `0x80205000 + 511 * 8 = 0x80205ff8`。
- 这是最高级叶子，Sv39 页表 walker 在第一层就结束，不会再访问下两级 PTE。VA 所在 1 GiB 区域映射到 PA 起点 `0x80000000`；该指令字节的 PA 推导为 `0x802000d8`。walker 的 `physical` 输出按页对齐，低 12 位偏移在后续访存中保留。
- 虽然架构 PTE 映射 1 GiB，`riscv_cpu_tlb_fill()` 传给 `tlb_set_page()` 的大小是 `TARGET_PAGE_SIZE`。QEMU 的软件 TLB 项按 4 KiB 粒度填充；访问同一 1 GiB 叶子内的另一个 4 KiB 页，仍可能再进入一次 refill 和页表函数。

源码定位：`code/kern/init/entry.S` 中 `csrw satp`、`sfence.vma` 和 PTE 初始化；QEMU `target/riscv/cpu_helper.c` 的 Sv39 参数选择、循环和超级页 PA 计算；`target/riscv/op_helper.c` 中 `helper_tlb_flush()`。

## 5. 可选：观察一条内核数据访问

在终端 B 的 `kern_init` 停止点，`kern_init()` 的第一段工作是 `memset(edata, 0, end - edata)`（`code/kern/init/init.c` 第 30 行）。它会对内核高地址数据区写零，可作为观察数据 store 的候选；函数序言也可能先写栈，因此以实际反汇编为准。

先在终端 B 查看源码和指令，找出准备观察的实际 store：

```gdb
(gdb) list kern_init
(gdb) disassemble /m kern_init
(gdb) info registers pc satp sp
```

若宿主 GDB 当前没有提示符，可在终端 A 按 `Ctrl-C` 暂停 QEMU 宿主进程，然后给宿主 GDB 添加只针对 S-mode store 的临时断点：

```gdb
(gdb) tbreak riscv_cpu_tlb_fill if mmu_idx == 1 && access_type == 1
(gdb) continue
```

回到终端 B，在选中的指令前设置源码断点或用 `si` 单步：

```gdb
(gdb) tbreak init.c:30
(gdb) continue
```

在行断点处再用 `disassemble /m kern_init` 确认实际指令；按 `si` 逐条执行，直到终端 A 停在 store refill。记录 store 的 VA、访问类型、satp、PTE、PA 和权限。处理宿主 GDB 停止后记得 `continue`，否则终端 B 看起来会一直无响应。

如果源码行断点不对应当前机器指令，或 `memset` 被优化成不同实现，就按反汇编逐条 `si`，只记录你实际看到的指令。不要凭 C 源码猜测 VA 或寄存器值。

## 6. TLB 命中、页表遍历与异常如何判定

### TLB 命中

普通 TCG 快路径是宿主 x86 生成代码中的标签比较与条件跳转；命中后直接使用 TLB `addend` 访问 RAM，绕过 `load_helper()`、通用 `tlb_fill()` 和 RISC-V `get_physical_address()`。进入 TCG 慢路径后，`load_helper()` 还会尝试 victim TLB；victim 命中也会绕过 RISC-V 页表 walker。带 `TLB_RECHECK` 的项是特殊情况，慢路径会再次执行 MMU 检查。

`tcg_out_tlb_load()` 是**生成快路径代码时**执行的 C 函数，不是每次客体访存都运行的命中处理函数。因此：

- 设 `get_physical_address` 断点没有命中，只说明当前没有观察到该函数执行；它本身不足以证明某条客体访存命中了 TLB。
- 证明某条具体访存命中，需要先确定客体指令 PC 和 VA，再核对该 `mmu_idx`、访问类型对应的 TLB 项，并观察生成代码的标签比较走命中分支。
- 如果本次没有检查到这条具体机器分支，把动态命中结论记为“未独立验证”。可同时记录静态源码证明的快路径行为。

### 页表成功

需要看到同一次调用栈内的证据链：

1. 正常访存调用进入 `riscv_cpu_tlb_fill()`。
2. `get_physical_address()` 返回 `TRANSLATE_SUCCESS`，`physical`/`prot` 合理。
3. 进入 `tlb_set_page()` / `tlb_set_page_with_attrs()`。
4. 客体 GDB 继续执行到所选访存之后，没有在这一访问上产生异常。

仅看到 `tlb_set_page_with_attrs()` 被调用仍要核对它的 caller 和 `mmu_idx`，避免把调试器地址查询混入。

### 页错误或访问错误

`raise_mmu_exception()` 按访问类型设置取指、读或写异常，并记录 `badaddr`。现代特权版本下，`satp.MODE` 非 Bare 且不是 PMP violation 时才选择 page fault；PMP violation 或 Bare 等条件可能对应 access fault。若断点未命中，记为“本次未观察到异常”，不要写成“页错误处理通过”。当前有效 1 GiB 映射不适合在不改内存映射的前提下强行制造页错误。

## 7. 断点暂停和退出顺序

当宿主 GDB 停在 `riscv_cpu_tlb_fill()`、`get_physical_address()` 等函数时，整个 QEMU 宿主进程都暂停；客体 GDB 的远程命令会等待。切回终端 A 执行 `continue` 后再检查终端 B。不要把这种等待误判为端口断开。

完成记录后按以下顺序退出：

1. 在终端 A 的宿主 GDB 提示符执行 `info breakpoints`；用 `delete` 清除本次剩余断点。
   如需记录进程号，可执行 `info inferiors`，区分 GDB 进程与 QEMU inferior。
2. 终端 B 执行：

   ```gdb
   (gdb) detach
   (gdb) quit
   ```

3. 终端 A 执行 `continue`，让 QEMU 恢复运行；在 QEMU 的 nographic 输入中按 `Ctrl-A`，松开后按 `x`，正常退出 QEMU。
4. 回到宿主 GDB 提示符执行 `quit`。确认输出显示 QEMU inferior 正常退出。
5. 另开终端确认没有遗留 QEMU 和 1234 端口监听：

   ```sh
   pgrep -af qemu-system-riscv64
   ss -ltnp | rg ':1234\b'
   ```

如进程由 host GDB 控制，优先按上述正常退出顺序；不要用 `kill -9` 作为常规清理方式。

## 8. 实验记录表（由实际执行者填写）

### 环境记录

| 字段 | 实测填写 |
|---|---|
| 日期 / 主机 |  |
| Git 分支与 HEAD |  |
| QEMU 源码版本 / 路径 |  |
| QEMU 可执行文件路径 / `--version` |  |
| QEMU 调试符号检查 |  |
| 内核符号检查 |  |
| 宿主 GDB 版本 |  |
| RISC-V GDB 版本 |  |
| 启动命令 |  |
| 宿主 GDB PID / QEMU inferior PID / GDB stub 端口 |  |

### 单次转换记录

| 字段 | 实测填写 |
|---|---|
| 观察对象（取指 / load / store） |  |
| 客体指令 PC |  |
| 客体 VA / 访问大小 |  |
| `access_type` / `mmu_idx` / `env->priv` |  |
| `satp` / 根页表 PA |  |
| TLB 主表 / victim / refill 的证据 |  |
| `levels` / `ptshift` / `i` / `idx` |  |
| PTE 的客体物理地址 / PTE 值 |  |
| PTE 的 V/R/W/X/U/A/D 解码 |  |
| `get_physical_address()` 返回状态 |  |
| PA 页 / 精确字节 PA / `prot` |  |
| `tlb_set_page_with_attrs()` 观察值 |  |
| fault / trap 字段（若有） |  |
| 结论（已动态确认 / 静态确认 / 未确认） |  |

### 运行结果

| 项目 | 结果或关键输出 |
|---|---|
| QEMU 启动与 OpenSBI |  |
| `sfence.vma` / `helper_tlb_flush` |  |
| 首次 Sv39 refill |  |
| PA 与源码推导是否一致 |  |
| 具体 TLB hit 分支是否动态确认 |  |
| 是否观察到 fault（无则写未观察） |  |
| 退出状态 / 遗留进程检查 |  |

把完整终端记录保存到你自己的实验记录位置，例如 `docs/lab2/results/b4_YYYYMMDD.txt`。如果将来把实测结论写进正式报告，保留原始命令和输出片段，并明确区分静态源码结论与动态观察。

## 9. 常见问题速查

| 现象 | 处理 |
|---|---|
| 报找不到 `opensbi-riscv64-virt-fw_jump.bin` | 确认 QEMU 命令含 `-L /home/lenovo/projects/os-lab/qemu/qemu-4.1.1/share/qemu`。 |
| 宿主 GDB attach 报 `Operation not permitted` | 不改 ptrace 设置、不 sudo；使用第 3 节由宿主 GDB 直接启动 QEMU 的方式。 |
| 客体 GDB 停在 `0x1000`，`satp=0` | QEMU 的 `-S` 暂停了客体；在客体 GDB 设置断点后执行 `continue`。 |
| 客体 GDB 在远程命令上等待 | 检查终端 A 的宿主 GDB 是否停在断点；先在宿主 GDB `continue`。 |
| `get_physical_address` 没触发 | 不据此判断 TLB 命中；确认选中的客体访存是否执行、调用栈是否为 guest access，并按第 6 节检查 TLB 证据。 |
| GDB 显示局部变量 `<optimized out>` | 记录下来；用参数、源码行、反汇编和仍可见的变量补充观察，不要更改 QEMU 构建。 |
| 没有观察到 `raise_mmu_exception` | 写“本次未观察到 fault”；不要人为篡改页表。 |
| `make gdb` 无法连接 :1234 | 确认宿主 GDB 启动的 QEMU 仍在运行、Makefile debug 命令含 `-s -S`，且端口没有被另一个进程占用。 |
