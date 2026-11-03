# Lab2 B4：验收复核与报告截图速查

本页把 B4 指导书要求、已有实测记录和现场复核步骤合在一起，供执行验证和整理报告使用。实验依据是工作区的 `docs/lab2/Lab2 Instruction.md` 第 4 节；详细背景见同目录的 `b4_sv39_tlb_debug_manual.md`，真实运行过程见 `log.md`。

## 1. 验收目标与当前证据

| 指导书要求 | 已有证据 / 复核目标 |
|---|---|
| 说明 QEMU 地址翻译调用路径和关键分支 | `store_helper()` 的主 TLB、victim TLB、refill 分支；`tlb_fill()` → `riscv_cpu_tlb_fill()` → `get_physical_address()` |
| 动态展示一次客体 VA 到 PA 的转换 | 已追踪 S 模式 8 字节 store：VA `0xffffffffc0204ff8` → 字节 PA `0x80204ff8`，并观察到软件 TLB 安装 |
| 观察页表遍历 | 已读到 Sv39 根 PTE `0x200000cf`。这是根层叶子映射，实际只遍历一层；不应声称运行了三级 PTE 读取 |
| 找到并解释 QEMU 的 TLB 查找 | 动态捕获 TLB miss/refill；`tlb_hit()` 命中和 victim 命中路径由源码确认，没有独立动态捕获 hit |
| 对比 Bare 与 Sv39、QEMU TLB 与真实 CPU TLB | 已观察 `satp=0` 的直接映射和启用 Sv39 后的高地址转换；报告中说明 QEMU 使用 TCG 软件 TLB，真实 CPU 使用硬件 TLB |
| 说明失败分支 | 已读源码中的翻译失败、PMP 失败和异常处理；本次没有动态触发页错误/访问错误。指导书未要求篡改页表来制造错误，禁止把源码分析写成动态通过 |

**结论口径：** B4 的主要动态目标已有证据；命中和异常路径不是本次动态结果。若检查者特别要求现场触发页错误，需要另行确定安全的客体测试输入；不要改启动页表来伪造结果。

## 2. 运行环境与启动顺序

本流程使用已存在的 QEMU 4.1.1 调试版和个人工作树中的内核，不重新编译 QEMU。宿主 GDB 直接启动 QEMU，因此只需两个交互终端：A 同时承载宿主 GDB 与 QEMU，B 运行客体 RISC-V GDB。

### 终端 A：宿主 GDB 启动 QEMU

```sh
cd /home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal/code
make debug QEMU='gdb -q -ex "set substitute-path /home/lenovo/os-lab/qemu/src/qemu-4.1.1 /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1" -ex "set pagination off" --args /home/lenovo/projects/os-lab/qemu/build-4.1.1/riscv64-softmmu/qemu-system-riscv64 -L /home/lenovo/projects/os-lab/qemu/qemu-4.1.1/share/qemu'
```

在宿主 GDB 提示符设置临时断点，再启动 QEMU：

```gdb
(gdb) tbreak get_physical_address if mmu_idx == 1 && addr == 0x80200000
(gdb) tbreak tlb_set_page_with_attrs if mmu_idx == 1 && vaddr == 0x80200000
(gdb) tbreak get_physical_address if mmu_idx == 1 && addr == 0xffffffffc02000d8
(gdb) tbreak riscv_cpu_tlb_fill if mmu_idx == 1 && access_type == 1 && address == 0xffffffffc0204ff8
(gdb) tbreak /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1/target/riscv/cpu_helper.c:237 if addr == 0xffffffffc0204ff8
(gdb) tbreak /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1/target/riscv/cpu_helper.c:254 if addr == 0xffffffffc0204ff8
(gdb) tbreak tlb_set_page_with_attrs if mmu_idx == 1 && vaddr == 0xffffffffc0204000
(gdb) run
```

若代码行断点因本机源码路径不同无法解析，先检查 GDB 的 `set substitute-path` 映射，再按实际源码路径重设；不要把断点未命中记成路径未执行。

### 终端 B：客体 GDB

另开终端执行：

```sh
cd /home/lenovo/projects/os-lab/workspace/Operation-System-Labs-personal/code
make gdb
```

进入客体 GDB 后：

```gdb
(gdb) tbreak kern_init
(gdb) continue
```

终端 A 会先在启动早期的 Bare 访问停住。每次 A 停住并记录后，都在 A 输入 `continue`，让客体继续。B 命中 `kern_init` 后记录寄存器，再在 B 输入一次 `continue`，执行内核初始化并等待目标 store。

## 3. 停点检查与预期值

### A. Bare：确认直接映射

在 A 的 `get_physical_address()` 停点：

```gdb
(gdb) p/x addr
(gdb) p/x env->satp
(gdb) p mmu_idx
(gdb) p access_type
(gdb) bt
```

预期地址 `0x80200000`、`satp=0`。继续到 `tlb_set_page_with_attrs()` 后检查：

```gdb
(gdb) p/x vaddr
(gdb) p/x paddr
(gdb) p size
```

预期 `vaddr=0x80200000`、`paddr=0x80200000`。这说明 Bare 下 VA 与 PA 相同；QEMU 仍可把该转换结果放入自己的软件 TLB。

### B. Sv39 启用：记录高地址状态

A 命中高地址 `get_physical_address()` 时检查：

```gdb
(gdb) p/x addr
(gdb) p/x env->satp
(gdb) p mmu_idx
(gdb) p access_type
(gdb) bt
```

预期 VA `0xffffffffc02000d8`，`satp=0x8000000000080205`。检查后在 A 输入 `continue`。B 命中 `kern_init` 时执行并截图：

```gdb
(gdb) info registers pc satp
(gdb) x/4i $pc
```

预期 `pc=0xffffffffc02000d8`，`satp=0x8000000000080205`。在 B 再输入 `continue`，让启动继续到目标数据访问。

### C. TLB miss、页表项和 PA

A 命中目标 `riscv_cpu_tlb_fill()` 时执行：

```gdb
(gdb) p/x address
(gdb) p access_type
(gdb) p mmu_idx
(gdb) p/x env->satp
(gdb) bt
```

预期地址 `0xffffffffc0204ff8`、`access_type=MMU_DATA_STORE`（数值 1）、`mmu_idx=1`、`satp=0x8000000000080205`。调用栈应显示正常客体存储经 `store_helper()` / `tlb_fill()` 到达 RISC-V refill。

输入 `continue` 到页表循环断点。在 `cpu_helper.c:237` 可查看循环状态；输入一次 `next` 观察进入当前层，再用 `continue` 到 PTE 读取后的 `cpu_helper.c:254`。执行：

```gdb
(gdb) info locals
(gdb) p idx
(gdb) p/x pte_addr
(gdb) p/x pte
```

预期 `idx=511`、`pte_addr=0x80205ff8`、`pte=0x200000cf`。该项是 Sv39 根层叶子，因此本次不会继续读低两级页表。

再输入 `continue` 到 `tlb_set_page_with_attrs()`，检查：

```gdb
(gdb) p/x vaddr
(gdb) p/x paddr
(gdb) p prot
(gdb) p size
(gdb) p mmu_idx
```

预期 `vaddr=0xffffffffc0204000`、`paddr=0x80204000`、`prot=7`、`size=4096`、`mmu_idx=1`。这里的 `vaddr`/`paddr` 是页基址；要在 GDB 中展示原访问的页内偏移和字节 PA，可在 setter 停点执行：

```gdb
(gdb) set $page_pa = paddr
(gdb) bt 4
(gdb) frame 2
(gdb) p/x address
(gdb) p/x (address & 0xfff)
(gdb) p/x ($page_pa + (address & 0xfff))
(gdb) frame 0
```

当前调用栈通常是 `#0 tlb_set_page_with_attrs`、`#1 tlb_set_page`、`#2 riscv_cpu_tlb_fill`；先用 `bt 4` 确认，再选显示 `riscv_cpu_tlb_fill` 的帧。预期 `address=0xffffffffc0204ff8`、偏移 `0xff8`、计算结果 `0x80204ff8`。这条地址加偏移关系可作为 `b4_tlb_install_pa.png` 的截图内容。若已经继续执行离开该停点，需在下一次命中 setter 时再运行这些命令；已有的独立日志仍可用于报告中的算式。

#### 直观理解：`fill` 到底往 TLB 放了什么

`fill` 不是 RISC-V 客体执行的一条指令，而是 QEMU 在软件 TLB 未命中后调用的 C 函数路径：通用 `tlb_fill()` 转到 RISC-V 的 `riscv_cpu_tlb_fill()`，它完成地址翻译后调用 `tlb_set_page()` / `tlb_set_page_with_attrs()`。最后，`tlb_set_page_with_attrs()` 在 `cputlb.c:835` 用 `copy_tlb_helper_locked(te, &tn)` 把新记录复制到主 TLB。

可以把这条记录想成“这一整页的快速通行卡”，而不是把原始 PTE `0x200000cf` 原样塞进 TLB：

| 本次访存信息 | 含义 |
|---|---|
| VA 页 `0xffffffffc0204000` → GPA 页 `0x80204000` | 这一页虚拟地址对应的客体物理页 |
| `prot=7` | 本次可读、可写、可执行 |
| `size=4096` | 这次安装一条 4 KiB 页的缓存记录；根层叶子 PTE 描述的映射范围更大，但本次 refill 按 4 KiB 项缓存被访问的这一页。原始访存仍是 8 字节 |
| `mmu_idx=1` | QEMU 用来区分本次地址空间/特权访问上下文的 TLB 索引 |
| 页内偏移 `0xff8` | 保留原访问在页内的位置，因此字节 GPA 是 `0x80204000 + 0xff8 = 0x80204ff8` |

QEMU 的 `CPUTLBEntry` 并没有一个直接叫作“客体物理页号”的字段。它保存 `addr_read`、`addr_write`、`addr_code` 这些虚拟地址匹配与访问许可信息，以及 `addend`（把客体 VA 换算成 QEMU 宿主地址时使用）；对应的 `CPUIOTLBEntry` 另存访存区域元数据。下一次同页访问命中时，QEMU 可利用这些缓存信息快速访问，不必再次走 Sv39 页表遍历。它是 QEMU TCG 的软件 TLB 记录，不是把 PTE 复制成真实 RISC-V CPU 的硬件 TLB 项。

本实验需要展示 VA 页、PA 页、权限和页大小时，在 `tlb_set_page_with_attrs()` 停点打印 `vaddr`、`paddr`、`prot`、`size` 就足够；这些值已经能说明填入的映射。若想额外观察 QEMU 内部结构，可在下一次运行中把断点设在 `cputlb.c:836`（紧跟实际复制语句之后），再尝试打印 `index`、`te->addr_read`、`te->addr_write`、`te->addr_code` 和 `te->addend`。调试构建可能因优化把局部变量显示为 `<optimized out>`；内部字段不是本实验核心验收所必需的证据。

#### 为什么这次填的是 `0xffffffffc0204ff8`，不是 `0x80200000`

QEMU 不是自行挑一个地址放入 TLB。**客体正在执行的指令先发起访存；该指令的有效地址决定本次要查哪个 TLB 项。** `riscv_cpu_tlb_fill()` 的 `address` 是访存目标地址，而不是正在执行的指令地址（PC）。查看调用栈可以区分它们：本次栈里有 `store_helper()`、`helper_le_stq_mmu()`，且 `access_type=MMU_DATA_STORE`，所以 `0xffffffffc0204ff8` 是一条客体 store 要写到的数据地址。

此前的 `0x80200000` 是另一阶段的访问：`access_type=MMU_INST_FETCH`，也就是启动早期 CPU 从该地址取指；当时 `satp=0`，处于 Bare 模式。后来内核切换到 Sv39，并执行到一条需要写数据的指令，算出的数据虚拟地址是 `0xffffffffc0204ff8`。这个地址所在页当时没有可用的 TLB 项，于是 `store_helper()` 的主 TLB 和 victim TLB 检查都未命中，才调用 `tlb_fill()`。页表遍历把它解析到 PA 页 `0x80204000`，QEMU 随后为这个虚拟页安装缓存项。**这不是从 `0x80200000` 跳转到 `0xffffffffc0204ff8` 去执行；一个是先前取指地址，一个是后来 store 的数据地址。**

因此 TLB 填充的选择规则可以概括成：哪条客体访存指令实际访问了哪个虚拟页，并且该页在当前 `mmu_idx` 下没有命中，就为那个页进行 refill；其他地址（例如 `0x80200000`）只有在它们自己再次发生未命中时才会触发相应填充。QEMU 按页缓存，所以本次具体安装的是 `0xffffffffc0204000` 这一页，不是只缓存单个字节地址 `...4ff8`。

检查后在 A 输入 `continue`，让这次 refill 返回客体存储路径。

如果任一断点没有命中，先记录实际停点、`info breakpoints` 和当前寄存器/参数；不要补写预期值冒充本次结果。该固定 VA 依赖当前内核映像与启动页表保持一致。

## 4. 源码截图：证明分支和比较结论

在另一个普通终端执行以下命令，截取带行号的源码输出。它们是**源码证据**，不代表异常分支被动态触发。

```sh
nl -ba /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1/accel/tcg/cputlb.c | sed -n '1515,1529p'
nl -ba /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1/target/riscv/cpu_helper.c | sed -n '237,280p'
nl -ba /home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1/target/riscv/cpu_helper.c | sed -n '355,365p;471,480p'
```

截图中应能看清：`store_helper()` 先检查主 TLB，再检查 victim TLB，二者未命中才调用 `tlb_fill()`；RISC-V refill 成功后安装映射，失败则走 probe 返回或异常处理。

报告对比可以写：真实 CPU 的 TLB 是处理器硬件中的地址转换缓存；QEMU TCG 使用软件 TLB 数据结构和生成的宿主代码加速客体访存。两者都缓存转换与权限信息，但本次观察到的是 QEMU 的模拟实现，不是宿主 x86 或真实 RISC-V 硬件 TLB 的内部状态。Bare 下 `satp=0` 使架构转换为 VA=PA；Sv39 下则使用 `satp` 根页表和 PTE 完成转换。

## 5. 截图命名与报告安排

建议把真实截图保存到个人仓库的 `report/images/`，按以下名称整理。截图保留 GDB 命令、停点和结果，避免只截单个数字：

| 文件名 | 截图内容 |
|---|---|
| `b4_host_breakpoints_setup.png` | 宿主 GDB 的 B4 条件断点配置 |
| `b4_guest_gdb_connection.png` | 客体 GDB 连接 QEMU GDB stub |
| `b4_guest_kern_init_breakpoint.png` | 客体 GDB 设置并继续到 `kern_init` |
| `b4_bare_translation_request.png` | `get_physical_address()` 中的 `satp=0` 和 `addr=0x80200000` |
| `b4_bare_tlb_install.png` | Bare TLB 安装时 `vaddr=0x80200000`、`paddr=0x80200000` |
| `b4_sv39_instruction_translation.png` | Sv39 下 `kern_init` 高地址取指的宿主调用栈 |
| `b4_guest_kern_init_sv39.png` | 客体 GDB 的 `pc` 与 Sv39 `satp` |
| `b4_store_tlb_miss.png` | 目标 VA、store 类型、`mmu_idx`、调用栈 |
| `b4_sv39_root_pte.png` | 循环状态、`idx`、`pte_addr`、`pte` |
| `b4_tlb_mapping_install.png` | TLB 安装的 VA 页、PA 页、权限和页大小 |
| `b4_tlb_install_kernel_result.png` | TLB 安装后内核继续运行并通过物理页分配器自检 |

正式报告可沿“调试方法 → Bare/Sv39 观察 → 一次 store 的 VA→PTE→PA→TLB 安装 → QEMU 软件 TLB 与真实硬件 TLB 的区别 → 未覆盖路径”组织。真实 GDB 输出和源码结论分开写。AI 协作过程可从同目录的 `log.md` 选用：例如变量不在当前栈帧导致 `No symbol`，以及源码行断点落到相邻 helper 的排查；不要把误停点写成目标 store 的证据。

## 6. 报告文字骨架与插图语法

可根据最终截图微调这段文字，不要把未观察的分支写成实测：

> 本次使用 QEMU 4.1.1 调试版和双 GDB 观察 RISC-V 客体访存。启动早期 `satp=0` 时，Bare 访问 `0x80200000` 直接映射到同一物理地址；切换到 Sv39 后，跟踪到 S 模式对 VA `0xffffffffc0204ff8` 的 8 字节存储。该次访问走过 QEMU TCG 软件 TLB 的 miss/refill 路径，根页表 PTE 位于 `0x80205ff8`，值为 `0x200000cf`。QEMU 得到页对齐 PA `0x80204000`、权限 `prot=7`，并安装 4 KiB 软件 TLB 项，因此该字节的 PA 为 `0x80204ff8`。源码显示主 TLB 与 victim TLB 未命中后才进入 refill；本次没有动态捕获 TLB hit 或页错误/访问错误路径，也没有单步到最终的宿主 RAM 写指令。

向 `report/report.md` 插入截图时可用：

```markdown
![Bare 模式的 satp 与访问 VA](./images/b4_bare_translation_request.png)
![Bare 模式的 VA 与 PA](./images/b4_bare_tlb_install.png)
![Sv39 下 kern_init 的 PC 与 satp](./images/b4_guest_kern_init_sv39.png)
![目标存储触发的软件 TLB miss](./images/b4_store_tlb_miss.png)
![Sv39 根页表项](./images/b4_sv39_root_pte.png)
![转换结果与软件 TLB 安装](./images/b4_tlb_mapping_install.png)
```

## 7. 退出

截图完成后，在 B 按 `Ctrl-C` 停住客体，执行 `detach`、`quit`。在 A 恢复 QEMU 后，nographic 窗口按 `Ctrl-A` 再按 `x` 退出 QEMU，最后退出宿主 GDB。确认没有遗留 QEMU 进程后，再把截图插入 `report/report.md`，并在正文注明 TLB hit 和 fault 分支仅作源码分析、未动态验证。
