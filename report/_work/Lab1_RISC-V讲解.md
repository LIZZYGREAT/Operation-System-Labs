# Lab1 前置知识：从 CPU 复位到内核运行，逐层理解每个组件

> 适用项目：`LIZZYGREAT/Operation-System-Labs`，分支 `work/lab1/wenjie`。  
> 定位：**知识笔记**，不是操作清单，也不是实验报告。读这份笔记时，先理解“为什么必须有这一层”，再理解“本实验这一层具体做了什么”。  
> 说明：文中会区分**一般计算机/RISC-V 机制**与**本次 Lab1 已观察的事实**。不要把 QEMU 4.1.1 的具体地址和指令误认为所有 RISC-V 机器的统一规定。

## 一、从最根本的问题开始：操作系统为什么不能像普通程序那样启动？

### 1. 普通应用程序是怎样运行的

平时执行：

```bash
./hello
```

实际上已经有一个正在运行的操作系统替它完成很多工作：

- 从文件系统找到 `hello`；
- 根据可执行文件格式，把代码和数据映射到内存；
- 为进程准备虚拟地址空间、栈以及运行环境；
- 设定 CPU 执行位置并开始运行；
- 为程序提供文件、终端、内存管理等服务。

因此普通应用可以从 `main()` 的角度思考，但操作系统内核不能：**内核启动时，还没有另一个已经工作的通用操作系统替它做这些事。**

### 2. 内核启动时缺了什么

假设我们已经编译出 `kernel`，仍然必须回答：

1. 谁把它放进内存？放在哪个地址？
2. CPU 刚复位时从哪个地址取指？
3. 机器的最低层初始化由谁负责？
4. CPU 怎样获得内核入口地址？
5. 内核自己的栈由谁提供？
6. 内核最初怎样输出一行字符，证明自己已经开始运行？

于是才需要一条分层启动链，而不是直接调用 `kern_init()`。

### 3. 本次实验最核心的因果链

```text
交叉编译器         解决“生成什么 CPU 的指令”
    ↓
链接器与链接脚本   解决“指令和数据最终放在哪个地址”
    ↓
QEMU loader       解决“把镜像字节放到虚拟机器内存哪里”
    ↓
Reset ROM         解决“CPU 复位后第一步怎样进入固件”
    ↓
OpenSBI           解决“机器级固件初始化，以及内核可调用的底层服务”
    ↓
kern_entry        解决“切到内核自己的最小执行环境，尤其是栈”
    ↓
kern_init         运行最初的 C 语言内核逻辑
    ↓
SBI console       解决“没有 Linux 用户态时怎样向终端打印”
```

**这次 Lab1 的学习目标就是能够从源码、构建产物和 GDB 输出解释并验证这条链。**

---

## 二、CPU、指令集、寄存器：理解启动过程需要的最少硬件知识

### 1. CPU 到底在执行什么

CPU 不认识 `main()`、`kern_init()` 这样的高级语言名称。它最终执行的是放在内存中的**机器指令**。

CPU 的简化运行循环：

```text
读取 PC 指向的指令
    ↓
译码
    ↓
执行：计算 / 读写内存 / 修改寄存器 / 跳转
    ↓
更新 PC
    ↓
重复
```

本实验用的目标指令集是 **RV64：64 位 RISC-V**。你的 WSL 虽然运行在 x86-64 主机上，但最终内核中的指令由 QEMU 模拟的 RISC-V CPU 执行。

### 2. ISA 是什么，为什么需要交叉编译

ISA（Instruction Set Architecture，指令集架构）规定 CPU 能理解哪些机器指令、有哪些寄存器、指令如何修改状态等。

```text
x86-64 GCC 产生 x86-64 指令
RISC-V GCC 产生 RISC-V 指令
```

由于开发机和目标 CPU 的 ISA 不同，故使用 `riscv64-unknown-elf-gcc`。它在 x86-64 Linux 上运行，但生成 RISC-V 可执行代码，这叫**交叉编译（Cross Compilation）**。

### 3. `PC` 是什么

`PC`（Program Counter，程序计数器）保存当前待执行的指令地址。在 GDB 中：

```gdb
p/x $pc
```

`/x` 表示按十六进制显示。

**关键：PC 是地址，不是指令本身。** PC 从 `0x1000` 变为 `0x80000000`，说明下一条取指位置发生了跳转。

### 4. `SP` 是什么

`sp`（Stack Pointer，栈指针）在 RISC-V 中是寄存器 `x2` 的 ABI 名称。它指向当前栈顶附近的位置。调用函数、保存寄存器以及分配局部变量都可能使用它。

```gdb
p/x $sp
```

本次实验中，我们最重要的观察之一就是：**刚进入 `kern_entry` 时的 `sp` 与完成内核栈设置后的 `sp` 不相同。**

### 5. `a0`、`a1`、`a7` 和 `t0` 是什么

RISC-V 的通用寄存器有数字名称，也有按照调用约定命名的名称：

| 寄存器 | 常见角色 | 本实验实例 |
|---|---|---|
| `x1 / ra` | 返回地址 | 普通函数调用使用 |
| `x2 / sp` | 栈指针 | 内核入口切到 `bootstacktop` |
| `x5 / t0` | 临时寄存器 | Reset ROM 临时保存跳转地址 |
| `x10 / a0` | 参数/返回值 | Reset ROM 传 hart ID；SBI 传字符参数 |
| `x11 / a1` | 参数 | Reset ROM 传设备树地址 |
| `x17 / a7` | 参数 | 本实验旧版 SBI 调用号 |

这些名字是 **寄存器调用约定（ABI）** 的一部分：约定谁负责传参数、保存什么状态，而不是每个寄存器天生只能干一件事。

### 6. `hart` 是什么

hart = **hardware thread**，可以理解为一个独立的 CPU 指令执行上下文。它不完全等同于“物理 CPU 封装”，多核处理器可能有多个 hart。

本次 Reset ROM 里：

```asm
csrr a0, mhartid
```

就是读取当前 hart 的硬件 ID，并放进 `a0`。OpenSBI 可以据此识别当前执行的是哪个 hart。

---

## 三、Reset、Reset Vector、ROM：为什么 CPU 复位后不能直接跳进我们的内核？

### 1. 什么是 Reset（复位）

复位是把 CPU/机器带入一个规定的初始状态，然后开始执行第一段启动代码。这里的“初始”并不意味着所有内存都归零，更不意味着任何 C 语言运行环境已经存在。

此时机器至少必须有一个明确答案：

> **PC 一开始应该指向哪里？**

这个位置叫 **Reset Vector（复位向量）**。

### 2. 为什么必须有 Reset Vector

假设 CPU 刚上电时 PC 是任意值，那么 CPU 甚至不知道从哪里取第一条指令，系统无法可预测地启动。

因此平台必须约定：

```text
Reset 发生
    ↓
PC 设置为平台规定的入口
    ↓
执行第一阶段启动指令
```

这个地址由硬件平台/模拟平台定义，**不是由我们 C 语言函数名决定**。

### 3. 什么是 ROM，为什么启动代码常放在 ROM

ROM（Read-Only Memory）本意是只读存储器。对于真实设备，启动时首先执行的代码需要放在通电后可立即读取、内容预先已知的位置，因此常由只读存储、Flash 或其他固件存储机制承载。

QEMU 中的 **Reset ROM** 是虚拟平台提供的一块启动代码区域；它模仿硬件复位后可执行的固定启动入口。

### 4. 为什么 Reset ROM 不直接等于 OpenSBI

两者职责不同：

- Reset ROM：**尽可能少地完成第一跳**，识别 hart 和下一阶段入口，传递必要参数。
- OpenSBI：**完整得多的机器级固件**，初始化平台运行环境，提供长期可用的 SBI 服务，并向内核交接控制权。

采用小型 Reset ROM 可以让最开始的启动过程很简单，也便于之后切换不同固件。

### 5. 本次实测的 Reset ROM

GDB 看到初始：

```text
PC = 0x1000
```

反汇编：

```asm
0x1000: auipc t0,0x0
0x1004: addi  a1,t0,32
0x1008: csrr  a0,mhartid
0x100c: ld    t0,24(t0)
0x1010: jr    t0
```

逐条理解：

| 指令 | 这次执行时的核心作用 |
|---|---|
| `auipc t0,0` | 根据当前 PC 得到基准地址 `0x1000`，保存到 `t0` |
| `addi a1,t0,32` | `a1=0x1020`，指向本平台准备的设备树信息入口 |
| `csrr a0,mhartid` | `a0` 接收当前 hart ID |
| `ld t0,24(t0)` | 从 `0x1018` 位置读取下一阶段跳转目标到 `t0` |
| `jr t0` | 跳到 `t0` 指向的地址 |

执行第五条以后，本次 GDB 实测：

```text
PC = 0x80000000
```

这证明 Reset ROM 把控制流交给了 OpenSBI。

### 6. 为什么强调“本次实测”

`0x1000`、这五条指令以及 `0x1020` 的布局是**当前 QEMU 4.1.1 virt 启动路径**的观测结果。真实 RISC-V 芯片和其他 QEMU 版本可能有不同复位入口或指令序列。它们不是整个 RISC-V ISA 统一规定的“所有机器都这样”。

---

## 四、Firmware、Bootloader 和 Kernel：为什么启动过程要分层？

### 1. 什么是 Firmware（固件）

固件是负责底层硬件初始化、机器控制或为上层软件提供基础服务的代码。它常在操作系统之前执行，也可能在操作系统运行期间持续响应底层请求。

固件不等于整个操作系统。它通常不负责普通用户进程调度、文件系统等完整 OS 功能。

### 2. 什么是 Bootloader（引导加载器）

Bootloader 的典型职责是找到、读取、准备下一阶段程序（例如 kernel），并决定把控制权交给哪里。

具体机器的 bootloader 可能还负责读取磁盘、解析文件系统、验证签名、传递内核参数等。

**并不是每一次启动都必须经过一个单独的、名字叫 Bootloader 的程序。** 简化实验常常省略复杂 bootloader，由模拟器或固件直接准备内核。

### 3. 本实验里各自是谁

```text
QEMU 的 -device loader
    ↓
把 ucore.img 字节放到内存中

QEMU Reset ROM
    ↓
机器复位后的最初执行路径

OpenSBI
    ↓
运行机器级固件，随后交给内核

kern_entry
    ↓
真正的内核汇编入口
```

要特别区分：**QEMU 的 loader 是模拟器提供的加载机制，不等于 CPU 正在运行一段普通 bootloader 程序。**

### 4. 谁把镜像放进内存，谁决定开始执行

这是两个不同问题：

- `-device loader,file=...,addr=...`：负责把镜像放到指定内存地址。
- 固件的下一阶段交接逻辑：负责让 CPU 最终跳到内核入口。

**“镜像已经在 `0x80200000`”不自动意味着“PC 就会变成 `0x80200000`”。** 本次 QEMU 默认固件与当前镜像的入口配置相匹配，所以最后确实跳进了内核。

---

## 五、RISC-V 特权级：为什么需要 M-mode 和 S-mode？

### 1. 为什么不让所有程序拥有最高权限

如果普通代码都能直接修改机器全局状态、控制硬件、读取所有内存，那么程序之间无法安全隔离。CPU 因此提供不同**特权级（Privilege Mode）**。

常见层次：

```text
M-mode（Machine）
    最底层、最高机器权限

S-mode（Supervisor）
    操作系统内核通常运行的权限层

U-mode（User）
    普通用户程序的权限层
```

权限层次并不意味着所有平台都有全部模式；本实验讨论的是支持 S-mode 的 RISC-V 机器。

### 2. M-mode 与 S-mode 为什么分开

机器级固件需要处理某些平台初始化、机器级中断/配置等事项；操作系统则主要关心进程、内存管理、用户程序和通用资源管理。

如果 OS 的每个底层操作都要依赖某个厂商直接提供的私有硬件接口，会损失平台兼容性。将机器级服务封装成接口，可以让 S-mode 内核用较稳定的方式请求底层能力。

这就引出了 SBI。

### 3. 内核是不是最高权限

在常见 RISC-V S-mode OS 模型中：**不是**。M-mode 比 S-mode 更底层、权限更高。

这也是初学者容易困惑的地方：

```text
“操作系统内核”
≠
“CPU 定义的最高特权级”
```

### 4. 什么是 Trap、Exception 和 Interrupt

理解 SBI 之前需要知道：CPU 可以因为某个事件暂时改变控制流，转而执行特定处理程序。这种控制流转移统称 **Trap**。

- Exception（异常）：与当前正在执行的指令同步相关，如 `ecall`、非法指令、缺页等。
- Interrupt（中断）：通常来自外部或异步事件，如定时器或设备中断。

`ecall` 在硬件意义上会引发一个环境调用异常；由哪一层处理，与 CPU 当前特权级和异常委托配置等有关。<sup>注：本文用概念说明，不要求此时掌握完整 CSR 异常委托寄存器。</sup>

### 5. `mret` 又是什么

在 RISC-V 特权体系中，机器级陷入处理程序完成任务、返回先前/指定特权上下文时，可以通过类似 `mret` 的特权返回机制转移执行。

因此 OpenSBI 向 S-mode 内核交接并不是普通 C 函数 `return kern_init()` 的关系，而要遵守硬件特权状态转换规则。

本次 Lab1 的日志证明 OpenSBI 最终进入内核入口，但没有逐条验证 OpenSBI 内部的特权切换代码；这里的 `mret` 是**机制补充**。

---

## 六、SBI 究竟是什么？与 OpenSBI 有什么区别？

### 1. 先用已有知识类比：接口与实现

例如：

```text
C 标准库定义函数接口
    ↓
某个具体 C 库实现这些函数
```

同理：

```text
SBI = Supervisor Binary Interface
      上下层之间“怎样请求服务”的约定

OpenSBI = Open-source implementation of SBI
          实现这些约定的一套实际机器级固件
```

**SBI 是规范/接口；OpenSBI 是具体实现。** 二者不是同一个软件的两个名字。<sup>见文末参考资料。</sup>

### 2. 什么叫 Binary Interface

API 常从“源代码中的函数接口”理解；ABI（Application Binary Interface）或 Binary Interface 更关心编译成机器代码后双方怎样沟通，例如：

- 调用号放哪个寄存器；
- 参数放哪个寄存器；
- 用哪条指令发起请求；
- 返回值从哪个寄存器读取；
- 支持哪些服务以及出错如何表示。

所以 SBI 不是内核中的普通 `printf` 库，而是一份约定**机器代码如何向底层固件请求服务**的接口。

### 3. 本实验真正使用的服务

当前 `code/libs/sbi.c`：

```c
uint64_t SBI_CONSOLE_PUTCHAR = 1;

void sbi_console_putchar(unsigned char ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}
```

含义：请求底层固件向控制台输出一个字节。

### 4. 旧版 SBI 的寄存器调用约定

本次代码：

```asm
mv x17, sbi_type    # a7 = legacy extension ID
mv x10, arg0        # a0 = 第一个参数（此处是字符）
mv x11, arg1        # a1 = 第二个参数
mv x12, arg2        # a2 = 第三个参数
ecall
```

即：

```text
a7 = 1       （legacy SBI console putchar）
a0 = 字符的编码
    ↓
ecall
    ↓
OpenSBI 处理请求
```

实际源码用 GCC inline assembly 的占位符写出这些指令，并在 `ecall` 后从 `a0` 取回返回值。

### 5. 为什么必须用 `ecall`，不能直接调用 OpenSBI 的 C 函数

普通 `call`/`jal` 只是修改控制流，不自动完成软件特权级边界处理。

而 `ecall` 的目的就是：**按 ISA 规定产生环境调用异常，由相应的底层执行环境接管和返回。** 当前场景中是 S-mode 软件向 M-mode OpenSBI 请求 SBI 服务。<sup>实际陷入处理依赖平台配置。</sup>

### 6. SBI 与系统调用（syscall）的区别

容易混淆的是这两条链：

```text
普通应用程序（U-mode）
    ↓ 系统调用
操作系统内核（S-mode）
```

和：

```text
操作系统内核（S-mode）
    ↓ SBI 调用
机器级固件 OpenSBI（M-mode）
```

两者都可能使用 `ecall` 作为机器指令，但**调用者、被调用服务层次、调用约定和处理目标不同**。

### 7. 为什么实验用 OpenSBI v0.4、Legacy SBI

QEMU 串口日志真实显示：

```text
OpenSBI v0.4
Runtime SBI Version : 0.1
```

本次 `a7=1` 的 `SBI_CONSOLE_PUTCHAR` 属于**旧版 SBI 的 Legacy Console Putchar**。现代 SBI 已演进出不同的扩展划分和调用约定，不能把这份旧项目的 `a7=1` 误写成所有 SBI 版本的通用形式。

本次实验用旧版是因为**课程指定的 QEMU 4.1.1 + 当前 Starter Code 与之配套**，不是让你现在升级整个 SBI 调用层。最新标准中的旧版接口仍有单独的历史兼容说明。

### 8. OpenSBI 启动后会不会“消失”

不会简单消失。虽然它把主要执行流交给了内核，但 SBI 请求仍可能通过异常处理机制进入底层固件。

因此要区分：

```text
启动交接：OpenSBI → Kernel
运行期服务：Kernel → SBI → OpenSBI → 返回 Kernel
```

---

## 七、设备树（Device Tree）是什么？为什么 Reset ROM 还传 `a1`？

### 1. 代码怎样知道“机器里有什么硬件”

内核如果需要访问内存、串口、定时器等硬件，至少要知道设备有哪些、其地址和中断连接是什么。不同平台的设备布局可能不同。

**设备树（Device Tree）** 是一种描述硬件组成的结构化数据；其二进制形式通常称为 **DTB（Device Tree Blob）**，也常称 FDT（Flattened Device Tree）。

### 2. 为什么通过地址传递

设备树是内存中的一块数据，因此启动阶段只需要把它的地址传给下一阶段。

OpenSBI 的常见启动约定中：

```text
a0 = hart ID
a1 = FDT/DTB 地址
```

本次 Reset ROM 指令 `addi a1,t0,32` 使 `a1=0x1020`。这对应本次 QEMU 复位数据区域中的设备树入口。

### 3. 本次是否需要解析整份设备树

不需要。Lab1 关注的是**能否把必要的启动信息带给下一层**，而不是实现完整驱动探测。知道 `a1` 为什么存在即可。

---

## 八、QEMU 是什么：为什么它不只是“一个运行别的程序的工具”？

### 1. QEMU 的两种常见模式

QEMU 至少可以区分：

- User-mode emulation：主要模拟目标架构的用户态程序，并依赖宿主系统提供许多 OS 层能力。
- System emulation：模拟目标机器中的 CPU、内存、设备、固件启动路径等，让一个 OS/kernel 运行。

**本次使用的是 system emulation**：`qemu-system-riscv64`。

### 2. `virt` 是什么

```bash
-machine virt
```

选择 QEMU 的 RISC-V 通用虚拟开发板，不是说“运行在 WSL 就是 virt”。它定义虚拟机器的设备/内存平台，QEMU 能够为它提供固件、串口、设备树等。

### 3. `-bios default` 负责什么

```bash
-bios default
```

在当前 RISC-V `virt` 场景里，让 QEMU 使用它提供的默认 OpenSBI 固件。**这解释为什么普通 `make qemu` 时先出现 OpenSBI 的启动 banner。**

### 4. `-device loader,file=...,addr=...` 负责什么

```bash
-device loader,file=bin/ucore.img,addr=0x80200000
```

它把 raw image 的字节放到指定的虚拟机器内存地址。它不承担 GDB 符号解析，也不等于用 `file bin/kernel` 自动执行内核。

### 5. `-nographic` 负责什么

```bash
-nographic
```

将模拟串口/监视器与终端绑定，不另外打开图形显示窗口。因此 OpenSBI 和内核的字符输出可以直接出现在你的 WSL 终端。

### 6. 为什么 QEMU 能运行不依赖 Windows 的裸机内核

因为 RISC-V 内核执行时看到的是**QEMU 模拟的硬件环境**，不是 Windows 的用户态 API。真实 x86 处理器执行的是 QEMU 这个宿主进程；QEMU 则解释/翻译并维护虚拟 RISC-V CPU 和设备状态。

---

## 九、ELF、链接脚本和裸镜像：谁决定内核放在哪？

### 1. 从 `.c` 到 `.o`

编译单个文件一般得到 `.o`（目标文件），其中已经包含目标 ISA 的机器代码和尚待解析的符号/重定位信息，但不同文件之间的地址和引用未全部定好。

### 2. 为什么还要链接（Link）

链接器需要把多个 `.o` 整合起来，并决定：

- 从哪个符号进入；
- 各 section 在最终程序中的位置；
- 跨文件调用引用到哪个具体地址；
- 调试符号对应到什么位置。

本实验由：

```text
code/tools/kernel.ld
```

控制主要链接布局。

### 3. 什么是 section

常见 section：

| Section | 常见内容 |
|---|---|
| `.text` | 机器指令 |
| `.rodata` | 字符串常量等只读数据 |
| `.data` | 已初始化、可写的全局数据 |
| `.bss` | 未初始化或零初始化的全局/静态数据 |

不要把“源码的 `.data` 指令”与运行时“栈（stack）”混淆。本实验 `bootstack` 是在汇编的 `.data` 段预留空间，真正的栈使用行为由 `sp` 指向它后才发生。

### 4. 本实验的链接脚本

```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;
SECTIONS {
    . = BASE_ADDRESS;
    /* .text、.rodata、.data、.bss 等 */
}
```

其中：

```text
ENTRY(kern_entry)
    设置 ELF 入口符号

. = BASE_ADDRESS
    设置后续 section 的布局起点
```

注意：**ELF 入口字段是构建结果的一部分；CPU 的复位地址仍然由机器启动平台决定。** 二者不是一个概念。

### 5. 为什么同时要有 `bin/kernel` 和 `bin/ucore.img`

```text
bin/kernel
    ELF，可用于 readelf/nm/objdump 和 GDB 的符号/源码调试

bin/ucore.img
    objcopy -O binary 后的裸二进制，供 QEMU loader 按地址装入
```

本次：

```text
ELF Entry = 0x80200000
QEMU loader 地址 = 0x80200000
GDB 实测 kern_entry PC = 0x80200000
```

三者一致，组成“链接—加载—执行”的证据链。

### 6. 什么是 `edata`、`end`，为什么要清零 BSS

链接脚本向内核提供内存布局边界符号：

```ld
PROVIDE(edata = .);
/* .bss ... */
PROVIDE(end = .);
```

内核 C 初始化：

```c
memset(edata, 0, end - edata);
```

原因是：**未初始化的全局/静态数据在 C 语言语义上应当初始为零**；对于裸机内核，不能假设会有一个现成 OS loader 负责清零，于是启动代码自行保证。

本次链接结果：

```text
edata = 0x80203008
end   = 0x80203008
```

因此本次实际清零长度为 **0 字节**。这是“当前产物的特例”，不意味着 BSS 初始化机制没有意义。

---

## 十、Kernel Entry 是什么：为什么需要 `kern_entry` 而不是直接写 C？

### 1. 入口的含义

“入口”可以理解为某一阶段**第一条应该执行的机器指令地址**。

本次有几个不同层次的入口：

```text
机器复位入口：0x1000
固件入口：    0x80000000
内核入口：    0x80200000 = kern_entry
C 初始化入口：0x8020000a = kern_init
```


### 2. 为什么先写汇编

进入 C 函数意味着要遵守编译器产生代码时所依赖的 ABI，例如有效的栈指针、适当的栈对齐，以及必要的全局数据初始化条件。

最早的执行入口不能默认这些已经满足。因此用极少量汇编把执行环境准备好，是很常见的内核启动方式。

### 3. 本次 `kern_entry` 做什么

`code/kern/init/entry.S`：

```asm
kern_entry:
    la sp, bootstacktop
    tail kern_init
```

分两步：

```text
1. 为内核设置一个可信赖、独立的初始栈
2. 直接转到 C 语言 kern_init
```

### 4. 为什么不能一直用 OpenSBI 留下的旧栈

GDB 观察：

```text
进入 kern_entry 前：sp = 0x8001bd80
设置后：          sp = 0x80203000
```

OpenSBI 的栈属于固件运行环境。内核应有自己控制的栈空间，以避免对固件内部布局和生命周期产生不必要的依赖。

### 5. 栈到底是什么

栈是一段按照约定用于保存调用现场、局部变量等数据的内存区域。RISC-V 常用 ABI 里栈向较低地址增长：

```text
高地址：bootstacktop = 初始 sp
│
│ 栈可用空间
│
低地址：bootstack
```

栈不是一块“特殊硬件内存”，而是**普通内存 + 栈指针与调用约定**的组合。

### 6. 为什么栈顶名字叫 `bootstacktop`

`bootstack` 表示这片栈区域的开始（低地址端），`bootstacktop` 表示栈区域的高地址端。本次约定从高端作为初始 SP。

`entry.S` 中：

```asm
.align PGSHIFT
bootstack:
    .space KSTACKSIZE
bootstacktop:
```

相关常量：

```c
#define PGSIZE 4096
#define PGSHIFT 12
#define KSTACKPAGE 2
#define KSTACKSIZE (KSTACKPAGE * PGSIZE)
```

先定义变量：

- `PGSIZE`：一页的字节数；
- `KSTACKPAGE`：栈占用的页数；
- `KSTACKSIZE`：栈区域总字节数。

因此：

$$
KSTACKSIZE=KSTACKPAGE\times PGSIZE=2\times4096=8192\ \mathrm{B}=8\ \mathrm{KiB}
$$

本次符号地址：

```text
bootstack    = 0x80201000
bootstacktop = 0x80203000
```

地址差：

$$
0x80203000-0x80201000=0x2000=8192\ \mathrm{B}
$$

### 7. `la` 和 `tail` 为什么不是最终机器指令

它们是**汇编伪指令**，写法更方便，但汇编器/链接器会把它们变为真实的目标 ISA 指令。

本次 GDB 看到：

```asm
0x80200000: auipc sp,0x3
0x80200004: mv    sp,sp
0x80200008: j     0x8020000a <kern_init>
```

原因：`la sp,bootstacktop` 可以通过 PC 相对寻址计算符号地址；当前目标距离恰好使低位补偿量为零，故第二条显示为无操作效果的 `mv sp,sp`。这不是 `la` 普遍只有一条有效机器指令的规律。

`tail kern_init` 表示**尾跳转**：不会按普通调用方式期待返回到 `kern_entry`。本次链接结果的跳转编码还可以用 RISC-V 压缩指令，因此 `kern_init` 出现在 `0x8020000a` 而不一定是四字节边界。

### 8. 之后为什么才可以执行 `kern_init`

`kern_init` 是 C 函数，入口建立好栈后才有理由按 C 调用约定继续执行。在这个最小实验中，它做的事是清理 BSS 范围、打印消息，然后无限循环；不涉及进程创建或调度。

---

## 十一、内核为什么能够打印：没有 Linux，`cprintf` 最后是谁来输出？

### 1. 普通 `printf` 为什么不能直接照搬

平时 Linux 应用打印到终端，背后有 C 库和操作系统的文件/终端服务；但当前 kernel 正在成为操作系统，尚不能依赖另一个 Linux 帮它执行用户态输出。

因此这个工程有自己的一套打印路径。

### 2. 本实验实际调用链

```text
kern/init/init.c
cprintf("%s\n\n", message)
    ↓
kern/libs/stdio.c
vcprintf
    ↓
libs/printfmt.c
vprintfmt（解析 %s 等格式串）
    ↓
cputch（逐个字符输出）
    ↓
kern/driver/console.c
cons_putc
    ↓
libs/sbi.c
sbi_console_putchar
    ↓
sbi_call
    ↓
ecall
    ↓
OpenSBI console service
    ↓
QEMU 虚拟 console / serial
    ↓
WSL 终端（-nographic）
```

这里可以看出每层的抽象：

- `cprintf` 只关心格式化输出；
- `vprintfmt` 只关心如何把格式串变成字符序列；
- `cons_putc` 只关心 console 的单字符操作；
- `sbi_console_putchar` 负责 SBI 级调用；
- OpenSBI 负责最终对接底层 console 机制。

### 3. 这里的 `ecall` 算不算“系统调用”

它是一条**环境调用指令**。在这里它承载的是 **SBI 固件服务调用**；不要简单等同于应用程序向 OS 发起的 Linux `write` 系统调用。

### 4. 现有源码中的限制

`console.c` 还有一些空实现的键盘/串口中断接口；`readline.c` 也提供了读取一行的代码。但它们**存在于工程中不等于当前 Lab1 已有完整交互终端或中断驱动**。本次真实用到的是字符输出路径。

---

## 十二、GDB、QEMU GDB Stub、断点：调试不是“直接调试 Windows 程序”

### 1. GDB 的调试对象是谁

RISC-V 内核指令是在 QEMU 的虚拟 CPU 上运行，不是在 Windows x86 CPU 上以原生程序方式运行。

因此：

```text
RISC-V GDB
    ↕ GDB Remote Protocol (TCP 1234)
QEMU 的 GDB Stub
    ↕
虚拟 RISC-V CPU / 寄存器 / 内存
```

`gdbstub` 是 QEMU 内置的远程调试接口：接收 GDB 指令，代替 GDB 操作虚拟 CPU 状态。

### 2. 为什么 `-s` 和 `-S` 必须区分

```text
-s：启动 GDB 远程服务，默认 TCP 1234
-S：让虚拟 CPU 启动时先暂停，不自动运行
```

没有 `-s`，GDB 通常无法按这套方式连接；没有 `-S`，GDB 连上前最早的启动代码可能已经执行完。

### 3. 为什么 GDB 要加载 `bin/kernel`

```gdb
file bin/kernel
```

这个命令让 GDB 获得 ELF 中的符号和调试信息，例如 `kern_entry`、`kern_init`、源码行号；但实际执行的二进制由 QEMU 从 `ucore.img` 加载，两者职责不同。

### 4. GDB 究竟观察了什么

| 命令 | 观察目标 |
|---|---|
| `p/x $pc` | PC 的十六进制值 |
| `p/x $sp` | 栈指针 |
| `x/5i $pc` | 反汇编 PC 附近的机器指令 |
| `si` | 执行单条目标机器指令 |
| `break *0x80200000` | 在指定地址设置断点 |
| `break kern_init` | 在符号对应地址设置断点 |
| `continue` | 继续执行直到断点/异常/中断等 |

其中 `si` 的粒度是**机器指令**，不是一行汇编伪指令，更不是一行 C。

### 5. 为什么还要用 readelf/nm 而不是只看 GDB

它们分别解决两件事：

```text
readelf / nm / objdump
    静态：二进制内“设计和编译出了什么”

GDB
    动态：CPU 实际“跑到了哪里，寄存器变成了什么”
```

静态看到 `kern_entry = 0x80200000`，并不能单独证明固件一定正确跳到了这个位置。GDB 命中断点则补上了运行期证据。

---

## 十三、把本次实际地址连起来：这些十六进制数的角色各不同

| 地址 | 当前实验中代表什么 | 来源/证据 |
|---|---|---|
| `0x1000` | QEMU 虚拟平台的 Reset ROM 起点 | GDB 初始 PC |
| `0x1018` | 本次 Reset ROM 用于读取下一阶段目标地址的数据位置 | GDB 反汇编 `ld t0,24(t0)` |
| `0x1020` | 本次 Reset ROM 传递设备树地址的区域入口 | `addi a1,t0,32` |
| `0x80000000` | OpenSBI 固件入口/基址 | OpenSBI banner + GDB |
| `0x80200000` | `kern_entry`、ELF 入口、内核加载地址 | Makefile + kernel.ld + readelf + GDB |
| `0x8020000a` | 当前构建的 `kern_init` 入口 | nm + GDB |
| `0x80201000` | 当前 `bootstack` 区域低端 | nm |
| `0x80203000` | 当前 `bootstacktop`、初始化后的 `sp` | nm + GDB |

**分类记忆：**

- “谁先执行”：`0x1000 → 0x80000000 → 0x80200000`；
- “内核从哪进入 C”：`0x8020000a`；
- “内核栈放哪里”：`0x80201000 ～ 0x80203000`。

绝不能把 `0x8020000a` 等具体链接值当成 ISA 强制规定，它们会随代码和构建产物变化。

---

## 十四、容易混淆的问题，用反事实方式检查理解

### 1. 已经把 kernel 加载到内存，为什么不能省掉 Reset ROM？

因为“字节放在那里”和“CPU 的 PC 开始指向它”是两件事。CPU 复位后仍必须先有确定入口，再通过启动链进入下一阶段。某些平台可以采用更直接的引导方案，但**当前 QEMU virt 配置**就是通过 Reset ROM 和 OpenSBI 交接。

### 2. 已有 OpenSBI，为什么还要 `kern_entry`？

OpenSBI 提供固件环境，不替代内核初始化。内核必须建立并管理自己的栈等状态，之后才能安全运行内核 C 代码。

### 3. 已有 OpenSBI，为什么还要 SBI？

OpenSBI 是**实现**，SBI 是**交互约定**。如果没有稳定约定，OS 就难以通用地请求不同固件的服务。

### 4. 已有 `cprintf`，为什么还要 console、sbi.c 那么多层？

这些层分别处理格式化、字符设备抽象、固件调用机制。分开后，高层打印代码无需依赖具体底层实现，后续替换 console 驱动更容易。

### 5. 为什么不直接用 Linux `printf`？

因为本次代码是运行在模拟 RISC-V 机器上的裸机内核，那里尚不存在供它依赖的另一个 Linux 用户态系统。

### 6. 为什么 GDB 可以断在 `kern_init`，却不需要源码级调试 OpenSBI？

因为 GDB 加载了**我们的 ELF 符号**，知道 `kern_init` 的地址；OpenSBI 在该 GDB 会话中没有对应源码/符号映射时，仍然可以按地址和反汇编观察其指令。

### 7. 为什么 `make qemu` 停不下来不算死机？

当前 `kern_init` 最后明确 `while (1)`，表示内核有意保持运行。自动化 `timeout` 返回 124 需要结合“预期启动信息是否出现”和“进程是否清理”判断，不可只看返回码。

### 8. 为什么不能把 QEMU 版本随意换掉？

QEMU 的默认固件、复位 ROM、启动布局和工具行为可能随版本变化。课程本次要求 QEMU 4.1.1，且已经有对应真实 GDB 证据，所以应以这个具体环境进行解释。

---

## 十五、建议现在怎样学习：按依赖关系，不要背术语表

### 1. 第一轮：只理解“谁依赖谁”

```text
CPU 要执行 → 必须有 PC 初值 → Reset Vector/Reset ROM
Reset ROM 交固件 → OpenSBI 初始化并准备 OS 运行环境
OS 进入 C → 必须有有效 stack → kern_entry
OS 要输出 → 需要下层服务 → SBI / OpenSBI
整个过程需要证据 → readelf/nm + GDB
```

能口头回答这五步，再看细节。

### 2. 第二轮：自己打开真实代码对应验证

```text
code/Makefile
    观察 -bios default、loader、-s -S

code/tools/kernel.ld
    观察 ENTRY、BASE_ADDRESS、edata、end

code/kern/init/entry.S
    观察 la、tail、bootstack

code/kern/init/init.c
    观察 memset、cprintf、while (1)

code/kern/libs/stdio.c
code/kern/driver/console.c
code/libs/sbi.c
    追踪字符输出到 ecall
```

### 3. 第三轮：对照实际运行证据

仓库现有：

```text
report/_work/logs/35-lab1-static-verify.log
report/_work/logs/36-lab1-qemu.log
report/_work/logs/54-lab1-final-replay-gdb.log
```

针对每一个关键数值都找到其证据，而不是把结果当知识点背下来。

### 4. 第四轮：尝试不看答案讲清楚

> 编译器生成了什么？QEMU 加载了什么？CPU 先执行了谁？OpenSBI 为什么存在？进入 kernel 前做了什么？kernel 的 `cprintf` 为什么能输出？我们怎样证明这些事情真的发生？

这些问题全都能讲顺以后，本次实验才算真正理解。

---

## 十六、术语速查（仅作复习，不替代前面的因果解释）

| 术语 | 一句话定位 |
|---|---|
| ISA | CPU 指令/寄存器等架构约定 |
| Cross Compiler | 在一种架构的机器上生成另一种架构的代码 |
| Hart | RISC-V 独立硬件执行上下文 |
| PC | CPU 当前待执行机器指令地址 |
| Reset Vector | CPU 复位后规定的最初取指地址 |
| Reset ROM | 承载最初复位启动指令的 ROM 区域/虚拟区域 |
| Firmware | 较底层的启动与硬件服务软件 |
| Bootloader | 典型情况下负责寻找、准备并启动下一阶段程序 |
| M-mode | RISC-V 最底层机器特权模式 |
| S-mode | 通常运行 OS kernel 的特权模式 |
| SBI | Supervisor 与执行环境之间的二进制服务接口 |
| OpenSBI | 实现 SBI 的开源固件项目 |
| `ecall` | 触发环境调用异常的 RISC-V 指令 |
| DTB/FDT | 二进制形式的硬件配置描述信息 |
| ELF | 带入口、段/节、符号和可选调试信息的可执行文件格式 |
| Raw image | 不含标准 ELF 元数据的直接装载字节镜像 |
| Linker Script | 决定可执行文件地址、section 和入口布局的脚本 |
| Entry | 某一阶段开始执行的目标指令地址/符号 |
| ABI | 二进制级的寄存器、参数、栈等调用约定 |
| Stack / SP | 维护函数运行上下文的内存区域 / 指向它的寄存器 |
| GDB Stub | QEMU 对外暴露的远程调试服务 |
| Breakpoint | 让目标程序运行到特定位置时暂停的调试机制 |

---

## 十七、参考资料与“实验事实”边界

**实验中已验证的内容**以当前仓库的源码、`report/report.md` 和 `_work/logs/` 为准；尤其是地址、版本、汇编反汇编、GDB 栈指针数据，不应被新的通用说明覆盖。

**用于补充通用原理的官方资料：**

1. [RISC-V SBI 规范仓库](https://github.com/riscv-non-isa/riscv-sbi-doc)：区分 SBI 接口与具体实现。
2. [RISC-V SBI Legacy Extensions](https://docs.riscv.org/reference/sbi/ext-legacy.html)：旧版 console putchar EID `0x01`。
3. [OpenSBI 官方固件文档](https://github.com/riscv-software-src/opensbi/blob/master/docs/firmware/fw.md)：说明 hart ID、设备树、不同固件交接方式。
4. [RISC-V Privileged ISA：Machine-Level ISA](https://docs.riscv.org/reference/isa/priv/machine.html)：`ecall`、特权异常等基础定义。
5. [QEMU RISC-V System Emulator](https://www.qemu.org/docs/master/system/target-riscv.html)：`virt`、`-bios default` 与 OpenSBI。
6. [QEMU GDB Usage](https://www.qemu.org/docs/master/system/gdb.html)：`-s`、`-S`、远程 GDB 调试。

> **版本差异提醒：** 上述文档中的 QEMU/OpenSBI/SBI 通用说明可能对应较新的版本；本次作业依赖 QEMU 4.1.1 + OpenSBI v0.4 + Runtime SBI 0.1，具体实现/输出以当前真实日志为准。
