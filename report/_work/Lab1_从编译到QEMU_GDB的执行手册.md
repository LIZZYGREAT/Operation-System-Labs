# Lab1：从编译到 QEMU/GDB 的完整执行手册

> 对应仓库：`LIZZYGREAT/Operation-System-Labs`，分支 `lab1`。  
> 使用环境：WSL2 Ubuntu 22.04；RISC-V 交叉编译器、QEMU 4.1.1、RISC-V GDB。  
> 本文依据当前仓库的 `code/Makefile`、`tools/kernel.ld`、`kern/init/entry.S`、`kern/init/init.c` 和仓库报告所记载的实际验证结果编写。  
> **用途：手动完整复现 Lab1；每条命令都解释“输入是什么、执行了什么、观察什么、为什么能得出结论”。**

# 一、先明确操作目标与场景

## （一、）这次不是直接“运行一个 C 程序”

这次实验要完成一条连续的证据链：

```text
已有的 C / 汇编源码
    ↓  交叉编译
.o 目标文件
    ↓  链接器 + kernel.ld
bin/kernel（ELF，含入口和符号）
    ↓  objcopy
bin/ucore.img（裸镜像）
    ↓  QEMU 在虚拟 CPU 执行之前装入内存
0x80200000：内核机器指令已就位
    ↓  QEMU 将虚拟 CPU 置于复位状态
PC = 0x1000：Reset ROM
    ↓
PC = 0x80000000：OpenSBI
    ↓
PC = 0x80200000：kern_entry
    ↓  设置内核栈
sp = 0x80203000
    ↓
PC = 0x8020000a：kern_init
    ↓  cprintf → SBI ecall → OpenSBI
终端看到 (THU.CST) os is loading ...
```

需要分别证明：

1. **静态正确**：文件确实生成、是 RISC-V ELF、内核入口和栈符号在约定地址。
2. **整体启动正确**：不附加调试器时，QEMU 能显示 OpenSBI 与内核输出。
3. **动态执行正确**：GDB 实际看到 CPU 的 PC 依次到达复位 ROM、OpenSBI、内核汇编入口和 C 初始化函数。

## （二、）两套命令不能混用

| 命令在哪执行 | 提示符示意 | 举例 | 控制的对象 |
|---|---|---|---|
| Ubuntu Shell | `lenovo@...:~/.../code$` | `make`, `readelf`, `make debug` | 主机上的构建工具与 QEMU 进程 |
| GDB 交互界面 | `(gdb)` | `p/x $pc`, `si`, `continue` | QEMU 内部的虚拟 RISC-V CPU |

**`(gdb)` 中不能直接当 Shell 使用 `make`；Shell 中也不能直接当普通命令使用 `p/x $pc`。**

## （三、）操作前的已知信息

当前仓库的源码和报告共同给出了以下基准；下文“预期”指的是这份代码和该套工具环境下的结果，不是所有 RISC-V 机器都固定如此。

| 项目 | 本次参考值 |
|---|---|
| 开发环境 | WSL2 / Ubuntu 22.04.5 / x86-64 |
| RISC-V GCC | `riscv64-unknown-elf-gcc` 10.2.0 |
| RISC-V GDB | `riscv64-unknown-elf-gdb` 10.1 |
| QEMU | `qemu-system-riscv64` 4.1.1 |
| CPU 复位 PC | `0x1000` |
| OpenSBI 起始地址 | `0x80000000` |
| 内核装入与入口地址 | `0x80200000` |
| `kern_init` 当前地址 | `0x8020000a` |
| `bootstack` | `0x80201000` |
| `bootstacktop` | `0x80203000` |

# 二、准备：确认自己处于正确目录、工具可用

## （一、）进入 Lab1 代码目录

在 **WSL 的 Shell** 中：

```bash
cd ~/os-lab/workspace/Operation-System-Labs/code
pwd
```

- `cd`：change directory，切换当前工作目录。
- `~`：当前 Linux 用户的家目录（`$HOME`）。
- `pwd`：print working directory，输出当前目录的绝对路径；用于确认后续 `make` 能找到这里的 `Makefile`。

预期路径类似：

```text
/home/lenovo/os-lab/workspace/Operation-System-Labs/code
```

**注意：** `make clean` 会清除 `code/obj` 和 `code/bin` 等构建结果。若已有 QEMU/GDB 在使用旧镜像，先结束旧会话，再清理和构建。

## （二、）检查工具是否在 PATH 中

```bash
command -v riscv64-unknown-elf-gcc
command -v riscv64-unknown-elf-gdb
command -v qemu-system-riscv64
```

`command -v <命令>`：让 Shell 查找命令可执行文件；成功时显示路径，找不到时通常返回非零状态。

路径应指向已经部署的工具（例如 `~/os-lab/toolchain/current/bin/`、`~/os-lab/qemu/current/bin/`），不能只因为 `/usr/bin` 中有另一版本的 QEMU 就忽略版本差异。

查看版本：

```bash
riscv64-unknown-elf-gcc --version
riscv64-unknown-elf-gdb --version
qemu-system-riscv64 --version
```

这里 `--version` 是对应可执行程序的版本查看选项，不会启动内核。此步骤是环境确认，并非 Lab1 的主要技术验收。

# 三、第一阶段：`make clean` 与 `make`——从源码生成内核

## （一、）为什么先执行 `make clean`

```bash
make clean
```

命令由两部分组成：

- `make`：读取当前目录的 `Makefile`，按照目标（target）及其依赖关系执行规则。
- `clean`：明确指定运行 `Makefile` 中名为 `clean` 的目标。它不是 Linux 通用的 `clean` 命令，而是这份工程自定义的目标名称。

当前仓库相关代码：

```makefile
clean:
	$(V)$(RM) $(GRADE_GDB_IN) $(GRADE_QEMU_OUT) cscope* tags
	-$(RM) -r $(OBJDIR) $(BINDIR)
```

工程中：

```makefile
OBJDIR := obj
BINDIR := bin
```

因此会删除旧的中间产物和最终构建产物，例如：

```text
code/obj/    # .o、.d 等生成物
code/bin/    # kernel、ucore.img 等生成物
```

目的：**排除旧编译结果，让后面的 `make` 证明“当前这套源码可以从零完成构建”。**它不会执行 RISC-V 内核，也不会清理 `report/` 中的报告。

## （二、）编译整个工程

```bash
make
```

不写 target 时，`make` 执行工程配置的默认目标；当前 Makefile 将默认目标指向构建产物集合。

参考输出的主要阶段：

```text
+ cc kern/init/entry.S
+ cc kern/init/init.c
+ cc kern/libs/stdio.c
+ cc kern/driver/console.c
+ cc libs/printfmt.c
+ cc libs/readline.c
+ cc libs/sbi.c
+ cc libs/string.c
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img
```

### （1.）`+ cc ...` 表示什么？

例如：

```text
+ cc kern/init/init.c
```

这是 Makefile 打印的简化提示。实际由 RISC-V 交叉编译器处理源文件，生成 `.o` 目标文件：

```text
init.c → init.o
entry.S → entry.o
```

`.S`（大写 S）是需要经过预处理的汇编源文件，故可以使用 `#include <mmu.h>`、`#include <memlayout.h>` 这样的宏和头文件。

`.o` 不是最终可执行内核：它保存目标代码、section、符号及可能尚未解决的重定位信息。不同文件间的函数引用还需要链接器解决。

### （2.）`+ ld bin/kernel` 表示什么？

链接命令来自 Makefile：

```makefile
$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
```

其中：

- `LD`：`riscv64-unknown-elf-ld`，RISC-V 链接器。
- `-T tools/kernel.ld`：让链接器使用此链接脚本，而不是普通应用程序的默认内存布局。
- `-o $@`：指定输出文件；`$@` 是 Makefile 的自动变量，表示当前目标，此处为 `bin/kernel`。
- `$(KOBJS)`：此前编译出的目标文件集合。

链接器做两件重要工作：**把各目标文件合成为一个内核，并给代码、数据和引用到的符号分配最终地址。**

当前 `tools/kernel.ld`：

```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;
SECTIONS {
    . = BASE_ADDRESS;
    .text : {
        *(.text.kern_entry .text .stub .text.* .gnu.linkonce.t.*)
    }
    PROVIDE(etext = .);
    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    }
    . = ALIGN(0x1000);
    .data : {
        *(.data)
        *(.data.*)
    }
    .sdata : {
        *(.sdata)
        *(.sdata.*)
    }
    PROVIDE(edata = .);
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }
    PROVIDE(end = .);
    /DISCARD/ : {
        *(.eh_frame .note.GNU-stack)
    }
}
```

先只抓住关键语句：

- `ENTRY(kern_entry)`：指定 ELF 入口为符号 `kern_entry`。
- `. = BASE_ADDRESS`：链接脚本的位置计数器从 `0x80200000` 开始。
- `.text / .rodata / .data / .bss`：把各目标文件的输入 section 组织到输出 section 中。
- `ALIGN(0x1000)`：让下一部分地址按 4096 字节边界对齐。
- `PROVIDE(edata = .)`、`PROVIDE(end = .)`：在所需条件下提供区间边界符号，供 C 代码中的 `extern` 引用。

这些是**构建时的地址安排**，不是 CPU 在运行时逐行执行 `kernel.ld`。

### （3.）`objcopy` 又做了什么？

实际命令：

```bash
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img
```

逐项看：

| 部分 | 含义 |
|---|---|
| `riscv64-unknown-elf-objcopy` | 操作目标文件格式的工具 |
| `bin/kernel` | 输入 ELF 文件，含入口、符号和可能的调试信息 |
| `--strip-all` | 从输出中剔除符号等辅助信息；本例输出裸二进制时并不会保留 ELF 符号结构 |
| `-O binary` | 指定输出格式为 raw binary，即裸二进制 |
| `bin/ucore.img` | 最终生成的裸内核镜像文件名 |

注意：`ucore.img` **不是磁盘分区镜像**，而是该工程供 QEMU loader 装入的机器码和数据字节集合。它自身不记录 `0x80200000` 这个加载地址。

### （4.）这一阶段验收什么？

`make` 成功应能看到：

```text
bin/kernel
bin/ucore.img
```

在当前 `lab1` 仓库的报告中，构建已成功通过。对应截图为：

```text
report/images/01-build-success.png
```

# 四、第二阶段：静态检查——确认编译出的内容是什么

## （一、）查看文件类型：`file`

```bash
file bin/kernel bin/ucore.img
```

格式：

```text
file 文件1 文件2 ...
```

`file` 读取文件内容特征，判断文件格式；不会运行文件。

本实验预期：

```text
bin/kernel      → ELF 64-bit ... RISC-V ...
bin/ucore.img   → data
```

为什么 `ucore.img` 可能仅显示 `data`？因为裸二进制没有 ELF 文件头，`file` 通常无法仅凭扩展名或文件内容确定其专属格式。它不是“无效文件”。

## （二、）查看 ELF 头：`readelf -h`

```bash
riscv64-unknown-elf-readelf -h bin/kernel
```

- `readelf`：读取 ELF 格式信息。
- `-h`：header，显示 ELF 文件头。
- `bin/kernel`：要检查的 ELF 文件。

重点看：

```text
Machine:                           RISC-V
Entry point address:               0x80200000
```

这证明：**链接结果是 RISC-V 程序，并且 ELF 中登记的入口地址是 `0x80200000`。**

注意，这还只是文件元数据；它不证明 CPU 运行时真的到了这个地址。

## （三、）查看符号表：`nm -n`

```bash
riscv64-unknown-elf-nm -n bin/kernel
```

- `nm`：列出目标文件或可执行文件中的符号及地址。
- `-n`：按符号地址的数值顺序排序（numerical sort）。

如果输出过长，使用 Shell 管道过滤：

```bash
riscv64-unknown-elf-nm -n bin/kernel | grep -E 'kern_entry|kern_init|bootstack|edata|end'
```

这里：

- `|`：Shell 管道，把左侧命令的输出送给右侧。
- `grep`：按模式筛选文本行。
- `-E`：启用扩展正则表达式，这里用 `|` 表示“匹配其中任意一个名称”。

本次参考结果：

```text
kern_entry    = 0x80200000
kern_init     = 0x8020000a
bootstack     = 0x80201000
bootstacktop  = 0x80203000
edata         = 0x80203008
end           = 0x80203008
```

重点理解：`entry.S` 中只写了 `bootstacktop:` 标签，没有填写数字；其最终地址由**编译后的 section、链接脚本、前面各段长度、对齐和栈占用空间**共同决定，最后由链接器形成符号地址。

## （四、）反汇编入口：`objdump -d`

```bash
riscv64-unknown-elf-objdump -d bin/kernel
```

- `objdump`：显示目标文件的多类信息。
- `-d`：disassemble，反汇编可执行 section，把机器码解释为汇编指令。

只想看 `kern_entry` 附近：

```bash
riscv64-unknown-elf-objdump -d bin/kernel | grep -A 10 '<kern_entry>:'
```

- `grep -A 10`：显示匹配行及其后面 10 行；便于定位函数入口。

当前参考反汇编：

```asm
0x80200000 <kern_entry>:     auipc sp,0x3
0x80200004 <kern_entry+4>:   mv    sp,sp
0x80200008 <kern_entry+8>:   j     0x8020000a <kern_init>
```

源码本来是：

```asm
kern_entry:
    la sp, bootstacktop
    tail kern_init
```

这里反映了两件事：

1. `la` 和 `tail` 是汇编伪指令；实际执行时已经变为 RISC-V 机器指令。
2. `j` 可编码为压缩指令（RVC），所以该次跳转后的 `kern_init` 地址是 `0x8020000a`，而不是简单按每条指令固定 4 字节计算。

对应截图：

```text
report/images/02-static-elf-entry.png
report/images/03-static-elf-symbols.png
```

# 五、第三阶段：普通启动 `make qemu`——先验证能否跑起来

## （一、）启动命令

仍在 `code/` 的 Shell 中：

```bash
make qemu
```

Makefile 中 `qemu` 目标：

```makefile
qemu: $(UCOREIMG) $(SWAPIMG) $(SFSIMG)
	$(V)$(QEMU) \
		-machine virt \
		-nographic \
		-bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000
```

`qemu` 是 Makefile 的目标名，不是 `make` 自带的参数。运行时，Make 会先确认依赖的 `ucore.img` 已存在且无需重建，随后启动 QEMU。

等效的 QEMU 程序调用为：

```bash
qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -bios default \
  -device loader,file=bin/ucore.img,addr=0x80200000
```

参数逐项说明：

| 参数 | 功能 | 结合本次实验 |
|---|---|---|
| `qemu-system-riscv64` | 启动 RISC-V 系统模拟器 | 在 x86-64 主机上模拟 RISC-V 机器 |
| `-machine virt` | 选择机器类型 | 选择 QEMU `virt` 平台及其地址布局 |
| `-nographic` | 不使用图形窗口 | 串口控制台通过当前终端显示 |
| `-bios default` | 启用默认固件 | 当前 QEMU 4.1.1 使用默认 OpenSBI |
| `-device loader,file=...` | 指定加载文件 | `bin/ucore.img` 是主机文件 |
| `addr=0x80200000` | 指定加载地址 | QEMU 将内核字节预先放到虚拟 RAM 中的该位置 |

**精确区分：**在本工程里，内核是由 QEMU 的 loader 在虚拟 CPU 开始执行前装入内存的。OpenSBI 不是通过打开主机上的 `bin/ucore.img` 来完成文件读取；OpenSBI 负责必要的固件初始化和最终交接控制权。

## （二、）为什么能看到输出

本次启动流程：

```text
QEMU 装入 OpenSBI 与 ucore.img
        ↓
虚拟 CPU：PC=0x1000（Reset ROM）
        ↓
PC=0x80000000（OpenSBI）
        ↓
PC=0x80200000（kern_entry）
        ↓
sp=0x80203000（内核启动栈）
        ↓
PC=0x8020000a（kern_init）
        ↓
cprintf → SBI ecall → OpenSBI → 虚拟串口
```

参考输出包含：

```text
OpenSBI v0.4
...
(THU.CST) os is loading ...
```

OpenSBI 版本、CPU/设备信息可能受到 QEMU/固件版本影响；**`(THU.CST) os is loading ...` 是当前 `init.c` 中真实写出的内核启动标记**。

对应截图：`report/images/04-qemu-start.png`。

## （三、）怎样退出普通 QEMU？

当前内核最终执行：

```c
while (1)
    ;
```

它按设计一直运行，不会自动回到 Shell。

在 `-nographic` 的 QEMU 终端里：

```text
先按 Ctrl + A
再按 X
```

这是 QEMU 的终端转义组合，用于退出模拟器；它不是在 Shell 中敲入文字 `Ctrl+A X`，也不是 GDB 指令。

**退出普通 QEMU 后，再开始下一阶段的 `make debug`，避免同时启动多个 QEMU 或误连旧端口。**

# 六、第四阶段：双终端启动 QEMU + GDB 调试

## （一、）为什么要用两个终端

普通 `make qemu` 是让虚拟 CPU 自由执行。我们现在要观察“刚复位”的状态，所以必须在 CPU 执行前把它暂停，再由 GDB 控制它逐条执行。

```text
终端 A（Shell）                  终端 B（Shell → GDB）
make debug                       make gdb
   │                                │
   └── QEMU GDB Stub :1234 ◄────────┘
          │
          └── 模拟的 RISC-V CPU（暂停中）
```

这两个终端必须指向同一个实验 `code/` 目录，但彼此是不同的 Shell 会话。

## （二、）终端 A：启动带调试接口的 QEMU

新开一个 WSL 终端 A：

```bash
cd ~/os-lab/workspace/Operation-System-Labs/code
make debug
```

当前 Makefile 的 `debug` 与 `qemu` 几乎相同，只是在 QEMU 参数后增加：

```text
-s -S
```

- **`-s`（小写）**：QEMU 简写选项，开启 GDB 远程调试服务，默认监听 TCP 端口 `1234`。
- **`-S`（大写）**：启动时停止虚拟 CPU；不让它立刻从 `0x1000` 向下执行，等待调试器指令。

两者的区别：`-s` 负责“能连接”，`-S` 负责“先别跑”。如果仅有 `-s`，等你打开 GDB 时，内核可能已经执行到无限循环，就错过了最初的复位代码。

此时终端 A 可能暂时没有 OpenSBI 输出，这是预期行为：虚拟 CPU 还未运行。

## （三、）终端 B：打开 GDB 并连接

再开一个 WSL 终端 B：

```bash
cd ~/os-lab/workspace/Operation-System-Labs/code
make gdb
```

`Makefile` 的 `gdb` 目标等效于：

```bash
riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'
```

逐项说明：

- `riscv64-unknown-elf-gdb`：面向 RISC-V 裸机目标的调试器。
- `-ex '...'`：在 GDB 启动后自动执行一条 GDB 命令。这里使用了三次，所以依次执行三条。
- `file bin/kernel`：从 ELF 加载入口、符号、调试信息。这不会把内核再次写进 QEMU；QEMU 已经加载 `ucore.img`。
- `set arch riscv:rv64`：明确目标架构为 64 位 RISC-V。
- `target remote localhost:1234`：通过 GDB Remote Serial Protocol 连接本机 QEMU 提供的 TCP 调试接口。
- `localhost`：运行 GDB 和 QEMU 的同一个 WSL 环境；`1234` 是 QEMU 的默认 GDB 端口。

连接成功后，可能看到：

```text
Remote debugging using localhost:1234
0x0000000000001000 in ?? ()
(gdb)
```

`??` 不是错误：`bin/kernel` 只包含我们自己内核的调试符号，并不包含 QEMU Reset ROM 的函数名和源码信息。

**从下一阶段开始，所有命令都在终端 B 的 `(gdb)` 提示符下输入。**

# 七、第五阶段：逐条执行你给出的 GDB 指令

> 下文严格按你提供的指令顺序讲解。每一步同时说明 GDB 命令的语法、它在本实验中的目的，以及参考观测结果。真正运行时，请逐条输入，不要把整段说明文字复制进 GDB。

## （一、）先理解几个 GDB 基本语法

| 写法 | 意义 | 例子 |
|---|---|---|
| `(gdb)` | GDB 交互提示符 | 后面才输入 GDB 命令；不需要手打 `(gdb)` |
| `$pc` | 当前目标 CPU 的 PC 寄存器 | `$pc=0x1000` |
| `$sp` | 当前目标 CPU 的栈指针寄存器 | `$sp=0x8001bd80` |
| `p` | `print`，求值并显示表达式 | `p $pc` |
| `/x` | 以十六进制显示结果 | `p/x $pc` |
| `x`（命令） | `examine`，从某内存地址检查内容 | `x/5i $pc` |
| `i`（格式符） | 反汇编为指令 | `x/5i $pc` |
| `&` | 取符号的地址（地址运算符） | `&bootstacktop` |
| `*`（断点语法） | 按表达式算出的**地址**设断点 | `break *0x80200000` |

这里 GDB 命令 `x` 和格式字符 `/x` 的“x”含义不同：

- `p/x` 的 `x` 是 **hexadecimal**，控制如何显示表达式结果。
- `x/5i` 的 `x` 是整个 **examine 命令**，`i` 才表示把内存内容解释成指令。

## （二、）第 1 条：`p/x $pc`——看 CPU 现在要执行哪里

```gdb
p/x $pc
```

把它拆开：

```text
p     = print，计算并打印一个表达式
/x    = 用十六进制格式显示
$pc   = 当前目标 CPU 的程序计数器寄存器
```

参考输出：

```text
$1 = 0x1000
```

`$1` 是 GDB 分配给该次显示结果的历史编号，**不是 CPU 寄存器**。真正的寄存器是 `$pc`。

为什么看 PC？因为它确定 CPU 当前执行到哪里。这里证明我们在 QEMU 的复位起点，而不是在 `kern_init` 中。

## （三、）第 2 条：`x/5i $pc`——看接下来五条指令

```gdb
x/5i $pc
```

语法一般形式：

```text
x/[数量][显示格式][数据单位大小] 地址表达式
```

本例：

```text
x     = examine，查看目标内存
/5    = 查看 5 个项目
i     = 以机器指令（instruction）的反汇编形式显示
$pc   = 从 PC 指定的地址开始
```

参考输出：

```asm
0x1000: auipc t0,0x0
0x1004: addi  a1,t0,32
0x1008: csrr  a0,mhartid
0x100c: ld    t0,24(t0)
0x1010: jr    t0
```

**注意：这条指令只是“看”，不会让虚拟 CPU 执行任何指令，PC 仍停在 `0x1000`。**

### （1.）五条 RISC-V 指令分别干什么？

**`auipc t0,0x0`**（Add Upper Immediate to PC）：

```text
t0 ← PC + (0 << 12)
   = 0x1000
```

即得到复位代码所在的地址基准。`<< 12` 表示将立即数左移 12 位；本例立即数为 0，所以结果直接等于当前指令地址。

**`addi a1,t0,32`**（Add Immediate）：

```text
a1 ← t0 + 32
   = 0x1000 + 0x20
   = 0x1020
```

把当前配置中的设备树（DTB）地址放进 `a1`。这里 `32` 是十进制，等于十六进制 `0x20`。

**`csrr a0,mhartid`**（CSR Read）：

读取机器级控制/状态寄存器 `mhartid` 中的硬件线程编号（hart ID），保存进 `a0`；当前主要观察 hart 0。

**`ld t0,24(t0)`**（Load Doubleword，64 位加载）：

```text
从地址 t0 + 24 读取 8 字节
即从 0x1000 + 0x18 = 0x1018 读取 64 位值
把该值放入 t0
```

当前 ROM 在这里保存后续固件入口地址：`0x80000000`。注意，“从内存地址 `0x1018` 取出 `0x80000000`”，不是跳转到 `0x1018`。

**`jr t0`**（Jump Register）：

```text
PC ← t0
   = 0x80000000
```

使执行流进入 OpenSBI。

## （四、）接着连续执行五次 `si`

```gdb
si
si
si
si
si
```

`si` 是 `stepi` 或 `step instruction` 的缩写：**要求被调试的虚拟 CPU 执行一条机器指令，然后再次暂停**。

| 已执行几次 `si` | 执行的指令地址 | 执行后 PC（本次参考） |
|---:|---|---|
| 1 | `0x1000` 的 `auipc` | `0x1004` |
| 2 | `0x1004` 的 `addi` | `0x1008` |
| 3 | `0x1008` 的 `csrr` | `0x100c` |
| 4 | `0x100c` 的 `ld` | `0x1010` |
| 5 | `0x1010` 的 `jr` | `0x80000000` |

为什么第五次不是 `0x1014`？因为 `jr t0` 是跳转指令，会**修改 PC 为寄存器里保存的目标地址**，不再按顺序执行 ROM 的下一条指令。

注意，`si` 的“单步”单位是**机器指令**，不是一行 C 源码或一行汇编伪指令。

## （五、）`p/x $pc`——确认已进入 OpenSBI

```gdb
p/x $pc
```

参考：

```text
$2 = 0x80000000
```

我们已经动态证明：

```text
QEMU Reset ROM (0x1000)
    ↓
OpenSBI 固件入口 (0x80000000)
```

此时 CPU 已到达 OpenSBI 的入口附近，但还没有完成固件初始化，因此下一步不要继续一条条单步跟踪整个 OpenSBI。它内部指令很多，而当前练习需要的是“最终何时进入内核”。

对应截图：

```text
report/images/05-gdb-reset-vector.png
report/images/06-gdb-opensbi.png
```

## （六、）`break *0x80200000`——设置内核入口地址断点

```gdb
break *0x80200000
```

`break`（可缩写为 `b`）是**设置断点**：告诉调试器“当 CPU 将要执行到某个位置时暂停”。

### （1.）`break` 常见的几种用法

```gdb
break kern_init
```

按照函数名下断点，要求 GDB 已知道该符号；我们先前 `file bin/kernel` 已加载符号。

```gdb
break *0x80200000
```

按照明确的指令地址下断点。这里的 `*` 告诉 GDB：**后面的表达式表示要断下来的地址**，而不是函数名或源码行号。

```gdb
break kern/init/init.c:8
```

按源码文件和行号下断点（具体行号随源码变化，仅作语法例子）。

这里我们故意选 `break *0x80200000`：因为要证明执行流实际进入内核首地址，而不是只看源码函数名。

### （2.）为什么设置断点以后还要 `continue`？

`break` 只安装断点，并**不会自己启动 CPU**。现在虚拟 CPU 仍暂停在 OpenSBI 入口。

## （七、）`continue`——运行到内核入口再停下

```gdb
continue
```

`continue` 可缩写为：

```gdb
c
```

它告诉 QEMU：**从当前暂停位置继续执行，直到遇到断点、异常或人工中断等事件。**

从 `0x80000000` 开始，CPU 执行 OpenSBI 内部初始化代码；最终按照当前机器的启动安排，把控制权交给内核，断点在 `0x80200000` 命中。

参考停点：

```text
Breakpoint ..., kern_entry () at kern/init/entry.S:7
```

注意：`continue` 不是“自动跳到断点”；它是真的让中间的 OpenSBI 指令执行，只是不用人工一条条 `si`。

## （八、）`p/x $pc`——确认已进入内核

```gdb
p/x $pc
```

参考：

```text
$... = 0x80200000
```

这个地址同时满足：

```text
kernel.ld 的 BASE_ADDRESS = 0x80200000
ELF Entry = 0x80200000
QEMU loader 的 addr = 0x80200000
GDB 实际 PC = 0x80200000
```

前面三项只是构建/配置证据，这一步才是**运行时证据**。

## （九、）`x/3i $pc`——查看内核的前三条真实指令

```gdb
x/3i $pc
```

与之前 `x/5i $pc` 完全相同，只是数量从 5 改成 3。

参考：

```asm
0x80200000 <kern_entry>:   auipc sp,0x3
0x80200004 <kern_entry+4>: mv    sp,sp
0x80200008 <kern_entry+8>: j     0x8020000a <kern_init>
```

这些是 `entry.S` 实际编译链接后机器指令的反汇编；源码是：

```asm
kern_entry:
    la sp, bootstacktop
    tail kern_init
```

### （1.）`auipc sp,0x3` 为什么能得到栈顶？

`auipc` 的公式仍然是：

\[
rd = PC + (imm \ll 12)
\]

现在 `PC=0x80200000`，立即数 `imm=3`，所以：

\[
sp = 0x80200000 + (3 \ll 12)
   = 0x80200000 + 0x3000
   = 0x80203000
\]

这正好等于 `bootstacktop` 的符号地址。

### （2.）`mv sp,sp` 为什么什么都没做？

一般 `la` 伪指令可能展开成 `auipc + addi`。当前符号低位偏移恰好为 0，第二条表现为 `mv sp,sp`，是一个不改变寄存器值的操作。**不能据此认为所有 `la` 都只需要一条 `auipc`。**

### （3.）`j` 和源码里的 `tail` 是什么关系？

`tail kern_init` 表示无需返回到 `kern_entry` 的尾跳转。最终链接后的机器指令目标是 `0x8020000a`。它不要求像普通 `call` 一样保存回到 `kern_entry` 的返回地址。

## （十、）`p/x $sp`——看内核切栈前的状态

```gdb
p/x $sp
```

`sp`（Stack Pointer）是当前 CPU 的栈指针寄存器，其名称与 `$pc` 一样通过 `$` 在 GDB 中访问。

参考：

```text
sp = 0x8001bd80
```

这表示进入我们自己的内核汇编入口时，`sp` 还保留着固件阶段的值。内核尚未执行 `la sp, bootstacktop`。

## （十一、）`p/x &bootstacktop`——查询链接符号地址

```gdb
p/x &bootstacktop
```

拆开：

- `p`：计算并打印表达式。
- `/x`：十六进制显示。
- `bootstacktop`：来自 `entry.S`、经链接器定位的符号。
- `&`：取它的地址；**我们要的是该标签代表的位置，不是去读取那里存储的内容。**

参考：

```text
&bootstacktop = 0x80203000
```

这里比较的不是“两个栈的大小”，而是：

```text
切栈前 sp           = 0x8001bd80
内核预定栈顶地址     = 0x80203000
```

它们显然不同。

关于该符号的来源，`entry.S` 有：

```asm
.section .data
.align PGSHIFT
.global bootstack
bootstack:
    .space KSTACKSIZE
.global bootstacktop
bootstacktop:
```

而 `KSTACKSIZE = 2 × 4096 = 8192` 字节，所以：

```text
bootstack     = 0x80201000
bootstacktop  = 0x80203000
```

从低地址到高地址预留了 8 KiB 的内核启动栈；栈按照调用约定向低地址增长。`bootstacktop` 是这片预留区间的高端边界。

## （十二、）`si`——真正执行内核的第一条指令

```gdb
si
```

当前 PC 为 `0x80200000`，此时执行的是：

```asm
auipc sp,0x3
```

执行之后：

```text
PC = 0x80200004
sp = 0x80203000
```

为什么 PC 变为 `0x80200004`？因为执行的是一条四字节普通指令，之后顺序进入下一条。与此同时，`auipc` 还把计算出的地址写入了 `sp`。

## （十三、）`p/x $sp`——验证内核栈初始化完成

```gdb
p/x $sp
```

参考：

```text
$... = 0x80203000
```

与前面的：

```gdb
p/x &bootstacktop
```

结果相同，因而验证了：

\[
sp = \&bootstacktop = 0x80203000
\]

这是真正运行一条指令后观察到的变化，证明 `kern_entry` 确实建立了内核的启动栈指针。

## （十四、）补充执行一次 `si`——越过 `mv sp,sp`

你提供的原始序列中，此处下一条就写了 `break kern_init`。**那样同样可以成功**：让 `continue` 执行余下的 `mv` 和跳转即可。

但如果为了完整复盘三条入口指令，我建议再执行一次：

```gdb
si
```

这次执行 `0x80200004` 的：

```asm
mv sp,sp
```

结果：

```text
PC = 0x80200008
sp = 0x80203000
```

`sp` 不变，下一条就是跳往 `kern_init` 的 `j`。这一步属于**推荐的额外观测**，不是课程要求必须多做的步骤。

## （十五、）`break kern_init`——按函数名设第二个断点

```gdb
break kern_init
```

与前面的：

```gdb
break *0x80200000
```

对比：

| 写法 | 断点依据 | 本次结果 |
|---|---|---|
| `break *0x80200000` | 已知的机器指令地址 | 在 `kern_entry` 停下 |
| `break kern_init` | ELF 符号名，经 GDB 解析为地址 | 在 `kern_init` 停下 |

GDB 之所以知道 `kern_init` 的名字和地址，是因为 `make gdb` 启动时已经执行了：

```gdb
file bin/kernel
```

如果没有加载这份 ELF 符号信息，则应当使用明确的机器指令地址来设置断点，或先加载符号。

## （十六、）`continue`——执行跳转并进入 C 初始化

```gdb
continue
```

CPU 会继续执行到 `kern_init` 入口处，参考：

```text
Breakpoint ..., kern_init () at kern/init/init.c:8
```

这表示我们已从汇编入口过渡到 C 函数。

## （十七、）`p/x $pc`——确认 C 入口的具体地址

```gdb
p/x $pc
```

参考：

```text
$... = 0x8020000a
```

这个结果与前面的 `nm -n bin/kernel` 相互印证：

```text
kern_init = 0x8020000a
```

也与源码执行顺序吻合：

```text
kern_entry
   ↓
设置 sp
   ↓
tail kern_init
   ↓
CPU 真正到达 0x8020000a
```

你还可以执行一个**额外检查**：

```gdb
p/x $sp
```

预期仍为：

```text
0x80203000
```

这说明内核已经带着自己的栈进入 C 函数。

对应截图：

```text
report/images/07-gdb-kernel-entry.png
report/images/08-gdb-kern-init.png
```

# 八、第六阶段：继续执行内核、观察输出，然后安全退出

## （一、）如果还要观察 `kern_init` 的输出

当前 CPU 由于断点停在 `kern_init` 的开头，所以终端 A 还未必看到内核的 `(THU.CST)...`。

在终端 B 的 GDB 中：

```gdb
continue
```

让 CPU 继续执行 `kern_init`：

```c
int kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);
    const char *message = "(THU.CST) os is loading ...\n";
    cprintf("%s\n\n", message);
    while (1)
        ;
}
```

输出链：

```text
cprintf
→ vcprintf
→ vprintfmt
→ cputch
→ cons_putc
→ sbi_console_putchar
→ sbi_call
→ ecall
→ OpenSBI 的固件服务
→ QEMU 虚拟控制台
```

终端 A 应出现：

```text
(THU.CST) os is loading ...
```

之后 CPU 会进入 `while(1)`，这是当前 Lab1 的设计，不是 GDB 卡死。

## （二、）如何停止、断开和退出 GDB

当 GDB 因 `continue` 处于目标正在运行的状态时，在**终端 B**按：

```text
Ctrl + C
```

GDB 会向目标发送中断请求，通常让虚拟 CPU 暂停并重新出现 `(gdb)` 提示符。

再在 GDB 中输入：

```gdb
detach
quit
```

- `detach`：调试器与目标断开连接，通常让 QEMU 继续独立运行；它**不会删除内核镜像**。
- `quit`：退出 GDB 程序，返回终端 B 的 Shell。

为什么不直接关闭终端？因为我们希望明确区分“退出调试器”和“终止虚拟机”，并避免留下不清楚的进程状态。

## （三、）如何退出 QEMU

回到终端 A，QEMU 仍在运行时按：

```text
Ctrl + A，然后 X
```

回到 Shell 后，说明 QEMU 已退出。

如果 QEMU 已因其他原因退出，则不需要再发送该快捷键。

## （四、）可选的环境清理检查

在 Shell：

```bash
pgrep -af qemu-system-riscv64 || true
```

- `pgrep`：查找进程。
- `-a`：显示完整命令行。
- `-f`：按照完整命令行进行匹配。
- `|| true`：若没有匹配进程，避免因该退出状态影响后续 Shell 操作。

注意：该命令用于**检查**，不是清理；即使查到无关进程，也不要执行不加区分的 `pkill -9 qemu`。

# 九、你提供的原始 GDB 指令：完整可直接输入版

> **先完成 `make clean`、`make`、静态检查、普通 `make qemu` 并退出；再让终端 A 执行 `make debug`，终端 B 执行 `make gdb`。下面这一段只在终端 B 出现 `(gdb)` 后逐条输入。**

```gdb
p/x $pc
x/5i $pc
si
si
si
si
si
p/x $pc
break *0x80200000
continue
p/x $pc
x/3i $pc
p/x $sp
p/x &bootstacktop
si
p/x $sp
break kern_init
continue
p/x $pc
```

这套原始顺序本身可用。

如需更完整验证，建议在“第一次 `p/x $sp` 之后、`break kern_init` 之前”补充一次 `si`：越过 `mv sp,sp`。进入 `kern_init` 后再补充一次 `p/x $sp`。它们不会改变实验的核心目的，但更容易把入口汇编三条机器指令观察完整。

## （一、）缩写速查

| 全称或语义 | 常见缩写 | 用途 |
|---|---|---|
| `print` | `p` | 计算并显示表达式 |
| `print /x` | `p/x` | 十六进制显示 |
| `examine /Ni` | `x/Ni` | 从地址反汇编 N 条指令 |
| `stepi` | `si` | 执行 1 条机器指令，再暂停 |
| `break` | `b` | 设置断点 |
| `continue` | `c` | 继续运行到断点或其他暂停事件 |
| `info registers` | `i r` | 查看寄存器状态 |
| `detach` | — | 断开与目标连接 |
| `quit` | `q` | 退出 GDB |

## （二、）GDB `x` 命令还能怎么看内存？

这不是本次练习必需命令，但有助于从语法上理解 `x/5i`：

```gdb
x/5i $pc
```

从 PC 开始反汇编五条指令。

```gdb
x/gx 0x1018
```

从内存 `0x1018` 读取一个 8 字节（`g`，giant word）数据，并用十六进制（`x`）显示；可用于验证 Reset ROM 所读取的下一阶段入口值。本次参考是 `0x80000000`。

```gdb
x/4gx $sp
```

从栈指针指向的内存开始，以 8 字节为单位显示四个十六进制值。**它查看的是栈附近的实际内存内容，不是求 `$sp` 寄存器的数值。**

所以：

```text
p/x $sp   → SP 寄存器当前存着哪个地址
x/4gx $sp → 从 SP 指向的地址开始，内存中存了什么
```

两者的区别非常重要。

# 十、验收清单与实验截图对应关系

| 阶段 | 命令或操作 | 核心判断 | 当前仓库截图 |
|---|---|---|---|
| 构建 | `make clean && make` | `bin/kernel` 和 `bin/ucore.img` 生成 | `01-build-success.png` |
| ELF 入口 | `readelf -h`、`objdump -d` | RISC-V / 入口 `0x80200000` / 实际指令 | `02-static-elf-entry.png` |
| 符号地址 | `nm -n` | `kern_init`、`bootstacktop` 等符号匹配 | `03-static-elf-symbols.png` |
| 整体启动 | `make qemu` | OpenSBI + `(THU.CST)` 输出 | `04-qemu-start.png` |
| Reset ROM | `p/x $pc`、`x/5i $pc` | `PC=0x1000`，看见五条复位指令 | `05-gdb-reset-vector.png` |
| 进入固件 | 五次 `si`、`p/x $pc` | `PC=0x80000000` | `06-gdb-opensbi.png` |
| 进入内核 | `break *0x80200000`、`continue` | 命中 `kern_entry`、栈切换正确 | `07-gdb-kernel-entry.png` |
| 进入 C | `break kern_init`、`continue` | `PC=0x8020000a`、SP 保持有效 | `08-gdb-kern-init.png` |

所有截图均应来自仓库 `report/images/` 中对应的真实文件；本文只列引用位置，不生成或修改截图。

**本仓库的 `Makefile` 虽有 `make grade` 目标，但当前 `code/tools/` 不含 `grade.sh`，所以本手册不把 `make grade` 列为通过条件，也不虚构该测试的成功结果。**

# 十一、常见错误与定位方式

## （一、）`make: command not found`、`qemu-system-riscv64: command not found`

先看：

```bash
command -v make
command -v qemu-system-riscv64
```

通常属于工具未安装或 PATH 未包含既有安装目录，不意味着必须重新编译工具链。优先检查课程已经配置好的 `~/os-lab` 工具目录。

## （二、）`make gdb` 连接 `localhost:1234` 失败

优先确认：

1. 终端 A 是否真的执行 `make debug` 且 QEMU 仍在运行。
2. 是否启动了普通 `make qemu` 而不是带 `-s -S` 的 `make debug`。
3. 是否已有别的 QEMU 占用或影响端口 `1234`。
4. GDB 与 QEMU 是否在同一个 WSL 网络环境。

可在 Shell 检查：

```bash
ss -ltn | grep ':1234' || true
```

`ss -ltn` 表示查看监听中的 TCP 端口；此命令只检查端口，不会连接 GDB。

## （三、）GDB 一连接就停在 `0x1000 in ?? ()`

这是正常情况。当前 GDB 用的是 `bin/kernel` 符号表，Reset ROM 并不是这个 ELF 的一部分，所以没有可用的源码函数名。

## （四、）`continue` 以后看起来卡住

需要区分：

- 如果断点在正确位置且尚未命中，CPU 正在执行中间的指令。
- 如果已经进入 `kern_init` 并继续执行，则最终会进入 `while (1)`，不再自动停止。
- 如果需要重新操作 GDB，可在 GDB 终端用 `Ctrl+C` 中断目标。

## （五、）为什么 `break kern_init` 能用，`break 某固件函数` 却可能找不到？

因为 `bin/kernel` 含有我们内核的符号和调试信息，**不包含 OpenSBI 的完整符号**。按内核函数名设断点可用；调试固件内部函数则需要额外的固件符号信息，或直接使用地址。

## （六、）为什么某次反汇编地址略有变化？

源码、编译选项、工具链版本、链接优化或 RVC 压缩指令选择可能影响具体机器指令及符号地址。`0x8020000a`、`0x80203000` 是**当前仓库这一份构建的实测值**；重新修改代码后，应以最新 `nm`、`objdump`、GDB 为准。

# 十二、用一句话串起整次实验

> 我们先用 `make clean && make` 从源码生成新的 RISC-V ELF 内核和裸镜像；用 `readelf`、`nm`、`objdump` 验证内核入口、栈符号及真实机器指令；再用 `make qemu` 验证 OpenSBI 和内核能完整启动；最后通过 `make debug` 暂停 QEMU 的虚拟 CPU、`make gdb` 连接它，在 GDB 中用 `p/x` 读寄存器、用 `x/5i` 查看机器指令、用 `si` 逐条执行、用 `break` 设置断点、用 `continue` 跑到断点，动态确认 `PC=0x1000 → 0x80000000 → 0x80200000 → 0x8020000a`，以及内核入口将 `sp` 设置为 `bootstacktop=0x80203000`，最后观察 SBI 方式打印的内核启动信息。
