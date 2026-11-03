# Lab2 B4 实际调试记录

> 这里只记录已经实际观察到的运行结果。源码推导和后续待验证事项会明确区分。

## 2026-10-22：启动期 S 模式取指与 Bare 地址处理

### 实际观察

- QEMU 版本与源码：QEMU 4.1.1，源码位于 `/home/lenovo/os-lab/qemu/src/qemu-4.1.1`（按 GDB 输出中的路径记录）。
- 宿主 GDB 在线程 `Thread 3 "qemu-system-ris"` 停在 `riscv_cpu_tlb_fill()`。
- 参数为 `address=0x80200000`、`access_type=MMU_INST_FETCH`、`mmu_idx=1`、`size=0`、`probe=false`、`retaddr=0`。
- 调用栈依次经过 `tlb_fill()`、`get_page_addr_code()`、`tb_htable_lookup()`、`tb_find()`、`cpu_exec()`、`tcg_cpu_exec()` 与 `qemu_tcg_cpu_thread_fn()`。这确认本次是 QEMU TCG 执行客体指令取指时进入的软件 TLB 填充慢路径；不是 GDB 调试查询触发的 `riscv_cpu_get_phys_page_debug()` 路径。
- 随后停在 `get_physical_address()` 入口，参数 `addr=0x80200000`、`access_type=2`、`mmu_idx=1`，并实际读到 `env->satp=0x0`。

### 已完成的 Bare 映射观察

用户随后从 `get_physical_address()` 执行到调用者，并观察到 `ret=1`、`pa=0x80200000`、`prot=7`。接着同一次地址请求停在 `tlb_set_page_with_attrs()`，其参数显示 `vaddr=0x80200000`、`paddr=0x80200000`、`prot=7`、`mmu_idx=1`、`size=4096`。这证明这次 Bare 取指的 VA 与 PA 相同，并且 QEMU 将该页交给软件 TLB 填充路径。

**`ret=1` 的枚举含义暂不据此记录为成功值。** 当前可读取的 QEMU 4.1.1 源码将 `TRANSLATE_SUCCESS` 定义为 `0`、`TRANSLATE_FAIL` 定义为 `1`；而本次实际运行随后命中了 `tlb_set_page_with_attrs()`，证明运行时走到了 `riscv_cpu_tlb_fill()` 的成功填充分支。数值与当前可读源码定义不一致，需结合运行时对应源码或调用者栈帧再核对。

当前停在 `tlb_set_page_with_attrs()`。这里的参数名是 `vaddr` 和 `paddr`，没有 `addr`；因此 `p/x addr` 返回 `No symbol` 是预期的上下文差异，不是程序错误。之前额外设置的普通断点 6 已禁用；用于高地址页表遍历的临时断点 7 已设置。

用户继续执行后，宿主 GDB 命中临时断点 `helper_tlb_flush()`（`target/riscv/op_helper.c:145`）。该函数参数为 `env`，没有 `addr`，因此在此处查询 `addr` 同样会得到 `No symbol`。用户随后读到 `env->satp=0x8000000000080205`、`env->pc=0x80200038`，确认这时 `satp` 已切换到 Sv39；断点列表显示普通断点 6 已禁用，高地址条件临时断点 7 仍启用。

用户随后读到 `helper_tlb_flush()` 停点的 `env->satp=0x8000000000080205`、`env->pc=0x80200038`。继续后，临时断点 7 命中 `get_physical_address()`：`addr=0xffffffffc02000d8`、`satp=0x8000000000080205`、`mmu_idx=1`、`access_type=2`（`MMU_INST_FETCH`）。调用栈经过 `riscv_cpu_tlb_fill()`、`tlb_fill()`、`get_page_addr_code()`、`tb_htable_lookup()`，并进入 `helper_lookup_tb_ptr()` / `code_gen_buffer()` 的 TCG 执行路径。这确认实际客体取指触发了一次高地址的 Sv39 转换请求。

### Sv39 第一层页表读取

用户在 `get_physical_address()` 的循环处（`cpu_helper.c:237`）观察到 `levels=3`、`ptidxbits=9`、`ptesize=8`、`base=0x80205000`、`ptshift=18`、`i=0`。随后停在 PPN 计算语句之前（`cpu_helper.c:254`），实测 `idx=511`、`pte_addr=0x80205ff8`、`pte=0x200000cf`。`vm` 显示 `<optimized out>`，但先前实际 `satp=0x8000000000080205` 的 MODE 字段为 8；`mode=1` 表示 S 特权级，不是 `satp.MODE`。

按 PTE 位定义解读，`0x200000cf` 的 V/R/W/X/A/D 位均为 1，U 位为 0，PPN=`0x80000`。由于 R/W/X 至少一位为 1，它是叶子 PTE；源码分支与该值相符时，遍历在 Sv39 顶层结束。页对齐 PA、最终权限以及随后的软件 TLB 填充仍待继续观察。

之后用户在客体 GDB 输入 `continue`，命中临时断点 `kern_init()`（`kern/init/init.c:30`，停在 `memset(edata, 0, end - edata)` 处）。实测 `pc=0xffffffffc02000d8`、`satp=0x8000000000080205`；反汇编从 `auipc a0,0x6`、`addi a0,a0,-192` 等指令开始，源码行对应清零内核数据区。该结果确认客体已在 Sv39 启用状态下到达高地址的内核 C 初始化入口。用户尚未提供 `get_physical_address()` 最终 `physical`/`prot` 或这次高地址的 `tlb_set_page_with_attrs()` 参数，因此这些中间值不记为已直接观察。

### 继续执行到早期内存初始化

用户在客体 GDB 于 `kern/init/init.c:31`（`dtb_init()` 调用处）命中临时断点后再次执行 `continue`；宿主 GDB 继续运行 QEMU，串口输出实际到达 DTB 解析、内存管理器初始化及物理页分配器自检。输出包含 `memory management: best_fit_pmm_manager`、管理区间 `[0x80000000, 0x88000000)`、`Free physical pages: ... 31929 pages` 和 `check_alloc_page() succeeded!`，随后打印 `satp virtual address` 与 `satp physical address`。因此客体已越过 `dtb_init()`，执行到 Lab2 早期内存初始化并通过了该次页分配器自检。

这段输出没有包含宿主 GDB 命中 `riscv_cpu_tlb_fill()` 条件断点的提示，也没有提供新的客体 GDB 停点；所以它只能证明客体继续向前执行，不能证明预定的 S 模式数据存储 TLB refill 已被捕获。需要先确认两个 GDB 当前是否仍在运行，再决定是否在后续运行中重设断点。

用户随后在客体 GDB 按 `Ctrl-C`，GDB 报告 `Program received signal SIGINT`，停在 `kern_init()` 的 `while (1)`（`kern/init/init.c:43`）；实测 `pc=0xffffffffc0200110`、`satp=0x8000000000080205`。这表明客体已越过启动与内存管理初始化，停在内核末尾的空循环，Sv39 仍启用。用户报告宿主 GDB 端按 `Ctrl-C` 没有返回提示符；客体 GDB 中断只暂停客体执行，不代表宿主 QEMU 进程也已停止。该停点不影响已经记录的自检结果，但本轮未捕获目标数据存储 TLB refill。

当前宿主 GDB 的 `info break` 实测：断点 5 和 10 位于 `raise_mmu_exception()`（源码行 359、355），断点 6 位于 `get_physical_address()` 且已禁用；没有 `riscv_cpu_tlb_fill()` 的数据存储条件断点。因此，之前没有看到目标 refill 停点与断点列表相符；不能据此判断当时没有发生任何数据存储或 TLB refill。

重新启动并设置高地址 S 模式存储条件断点后，宿主 GDB 实际命中 `riscv_cpu_tlb_fill()`：`address=0xffffffffc0204ff8`、`access_type=MMU_DATA_STORE`、`mmu_idx=1`、`size=8`、`probe=false`。`bt` 显示调用路径为 `riscv_cpu_tlb_fill()` ← `tlb_fill()` ← `store_helper()` ← `helper_le_stq_mmu()` ← TCG `code_gen_buffer()`，确认这是一条客体 8 字节存储触发的 QEMU TCG 软件 TLB 填充慢路径。存储值为十进制 `2147486210`（十六进制 `0x80000a02`）。这次访问的 `env->satp` 和页表遍历结果尚待在 `get_physical_address()` 中读取。

随后同一访问命中 `get_physical_address()`：`addr=0xffffffffc0204ff8`、`access_type=1`、`mmu_idx=1`、`env->satp=0x8000000000080205`。调用栈从 `get_physical_address()` 返回到 `riscv_cpu_tlb_fill()`，再经过 `tlb_fill()`、`store_helper()` 与 `helper_le_stq_mmu()`。这直接确认此次存储请求在 Sv39 状态下进入页地址翻译函数；PTE 读取、权限结果、最终 PA 与 TLB 安装参数仍待观察。

用户在该次 `get_physical_address()` 的遍历循环入口读取局部变量：`mode=1`、`base=2149601280`（`0x80205000`）、`levels=3`、`ptidxbits=9`、`ptesize=8`、`ptshift=18`、`i=0`、`va_bits=39`。这与 `satp` 所示的 Sv39 根页表及三级遍历一致；`mode=1` 是当前 S 特权级。变量 `vm`、`sum`、`mxr` 因编译优化显示为 `<optimized out>`。当前层的 PTE 地址和值待继续执行后读取。

同一高地址存储访问在根层读到 `idx=511`、`pte_addr=0x80205ff8`、`pte=0x200000cf`。这与此前高地址取指从同一根页表项读到的值一致。按 Sv39 PTE 位定义，PPN=`0x80000`，V/R/W/X/A/D 均为 1，U/G 为 0；该项在根层就是叶子项，具备 S 模式存储所需的写权限且 A/D 已置位。由根层 1 GiB 映射与该虚拟地址在 `0xffffffffc0000000` 区间内的偏移，可推得预期 PA 为 `0x80204ff8`；QEMU 函数最终返回值、`prot` 和 TLB 安装参数仍需运行时确认。

用户从 `get_physical_address()` 执行 `finish` 后，GDB 报告函数返回值 `$43=0`；调用者上下文中读到 `ret=1`、页对齐物理地址 `pa=0x80204000`、`prot=7`。`pa` 加上该 VA 的页内偏移 `0xff8` 得到字节 PA `0x80204ff8`，与根层大页映射推导一致；`prot=7` 表示读、写、执行权限均允许。函数返回值与调用者局部 `ret` 的数值不一致，暂不单靠 `ret` 判定分支；下一步以是否实际进入 `tlb_set_page_with_attrs()` 或异常处理路径确认运行时结果。

随后实际命中 `tlb_set_page_with_attrs()`：`vaddr=0xffffffffc0204000`、`paddr=0x80204000`、`prot=7`、`mmu_idx=1`、`size=4096`。这确认该次转换成功并将 4 KiB 页映射装入 QEMU 软件 TLB。结合前述 VA 页内偏移 `0xff8`，本次存储的字节物理地址为 `0x80204ff8`。虽然 `finish` 显示的函数返回值为 0、调用者局部 `ret` 显示 1，运行时实际进入 TLB 安装函数这一控制流证据确认了本次地址转换成功；局部 `ret` 数值仍不单独用于判断。

用户对 `tlb_set_page_with_attrs()` 执行 `finish` 后，停在其包装函数 `tlb_set_page()`（`accel/tcg/cputlb.c:849`）。实测调用栈为 `tlb_set_page()` ← `riscv_cpu_tlb_fill()`（`target/riscv/cpu_helper.c:472`）← `tlb_fill()`（`accel/tcg/cputlb.c:878`）← `store_helper()`（`include/exec/cpu-all.h:404`）。这进一步确认架构专属填充函数在安装 TLB 项后返回到通用 TLB 慢路径，最终回到触发它的客体存储辅助函数。

继续 `finish` 后，用户停在 `riscv_cpu_tlb_fill()` 的 `return true`（`target/riscv/cpu_helper.c:474`）。源代码窗口显示前一分支为 `if (ret == TRANSLATE_SUCCESS)`，其内已调用 `tlb_set_page()`；局部变量显示 `pa=2149597184`（`0x80204000`）、`prot=7`、`pmp_violation=false`，`ret` 因编译优化显示 `<optimized out>`。因此实际执行位置确认 RISC-V refill 走成功分支并向通用 TLB 路径返回 `true`；此前单独打印的 `ret=1` 不作为判断依据。

用户再从 `riscv_cpu_tlb_fill()` 执行 `finish`，返回到通用 `tlb_fill()`（`accel/tcg/cputlb.c:879` 的 `assert(ok)`），GDB 明确显示返回值 `true`。调用栈仍连到原始 `store_helper()`、`helper_le_stq_mmu()` 与 TCG `code_gen_buffer()`。这确认架构专属 TLB 填充回调的成功结果已传回通用 TCG TLB 层；接下来可观察原存储辅助函数继续处理该次写操作。

用户从 `tlb_fill()` 执行 `finish` 后回到 `store_helper()`（`accel/tcg/cputlb.c:1524`），调用栈仍为 `store_helper()` ← `helper_le_stq_mmu()` ← TCG `code_gen_buffer()`。源码显示控制流处于原先 `tlb_hit()` 未命中且 `victim_tlb_hit()` 未命中的分支：调用 `tlb_fill()` 后重新计算 TLB `index` 并重新读取 `entry`。实测 `mmu_idx=1`、`index=4`；若干局部变量（包括 `tlb_addr`、`haddr`）显示 `<optimized out>`。这确认本次访问确实走过软件 TLB miss/refill 慢路径并返回到原始存储辅助函数；实际写入的后续语句还未单步观察。

用户在 `store_helper()` 中再次读取到 `addr=0xffffffffc0204ff8`、`size=8`、`val=0x80000a02`，确认回填后处理的仍是原始存储请求。源码接着检查 IO、未对齐访问与 `TLB_RECHECK` 特殊路径；RAM 写入语句位于后续代码，尚待继续查看。当前只记录 `val` 为本次写入值，不据此推断目标对象类型。

用户查看 `store_helper()` 的后续源码（`cputlb.c:1555-1595`），看到 IO 写入及跨页未对齐访问处理。按已观测参数，页内偏移 `addr & 0xfff = 0xff8`，8 字节写入的末字节偏移为 `0xfff`，没有跨页；地址也按 8 字节对齐。结合目标 PA 位于 DTB 报告的 RAM 区间，预期将跳过 IO 与跨页特殊分支并走普通对齐写路径，但尚未通过单步动态确认这些分支条件。

用户继续查看 `store_helper()`（`cputlb.c:1590-1665`），确认普通对齐写标签 `do_aligned_access` 在 1606 行，先通过 `haddr = addr + entry->addend` 得到宿主 RAM 地址；`size==8` 且小端时在 1630 行执行 `stq_le_p(haddr, val)`。栈参数显示 `big_endian=false`、`size=8`。这只是源码路径确认，当前运行尚未动态命中该实际写入语句。

用户尝试在 `cputlb.c:1630` 设置源码行临时断点时，GDB 将断点地址解析到 `cputlb.c:1641` 的 `helper_ret_stb_mmu()`。随后命中的是另一条 1 字节写：`addr=0xffffffffc0206018`、`val=0`；这与此前输出的 `edata=0xffffffffc0206018` 相同，推测是 `memset(edata, 0, ...)` 的后续操作，但目前没有客体源码行或调用栈直接确认。该停点不是原先 `addr=0xffffffffc0204ff8`、`size=8` 的访问，不能据此声称直接观察到了那条写入语句。

### B4 阶段性结论（2026-10-22）

本次动态追踪已覆盖一条代表性的客体存储地址翻译，不需要继续逐条单步到最终宿主内存写指令：

- **动态观察到的未命中路径：** 对客体 VA `0xffffffffc0204ff8` 的 8 字节存储触发 `store_helper()` 中的软件 TLB 未命中，随后进入 `tlb_fill()`、`riscv_cpu_tlb_fill()` 与 `get_physical_address()`。
- **动态观察到的 Sv39 结果：** `satp=0x8000000000080205`；根页表项地址 `0x80205ff8`，PTE 值 `0x200000cf`。QEMU 返回页对齐 PA `0x80204000`、`prot=7`，随后安装映射 `VA 0xffffffffc0204000 → PA 0x80204000`，大小 `4096` 字节。原访问页内偏移为 `0xff8`，对应字节 PA `0x80204ff8`。
- **动态观察到的返回路径：** 成功安装 TLB 项后，`riscv_cpu_tlb_fill()` 返回 `true`，结果传回通用 `tlb_fill()`，再回到原 `store_helper()`。这是一次成功的 TLB refill。
- **源码确认、未动态捕获的命中路径：** `store_helper()` 检查 `tlb_hit()`；命中时不进入 miss/refill 分支，因此不会调用 `tlb_fill()`、`riscv_cpu_tlb_fill()` 或 Sv39 页表遍历。此次调试没有单独捕获一条 TLB 命中访问。
- **额外停点的范围：** 后续停在 `helper_ret_stb_mmu()` 的访问是 1 字节零写，地址为 `edata`。它不是前述 8 字节访问，不能作为该访问最终写入指令的证据，也不影响已捕获的地址翻译链。

因此，B4 的核心证据已齐：一条真实的 TLB 未命中到 PA/TLB 安装的动态链路，以及 TLB 命中绕过页表遍历的源码依据。**尚未独立动态验证**的是 TLB 命中路径和页错误路径；也没有必要为完成本项继续单步到普通 RAM 写指令。后续报告应把动态观察与源码确认分开陈述。

### QEMU 4.1.1 源码调用链与分支核对

以下静态路径按工作区上级目录中的 `/home/lenovo/projects/os-lab/qemu/src/qemu-4.1.1` 核对，其 `VERSION` 文件内容为 `4.1.1`。GDB 输出使用的 `/home/lenovo/os-lab/qemu/src/qemu-4.1.1` 路径在当前执行环境中不可见；但函数位置与 GDB 记录的行号吻合，例如 `get_page_addr_code()` 第 1032 行、`store_helper()` 第 1519–1525 行、`tlb_fill()` 第 878 行、`riscv_cpu_tlb_fill()` 第 438–479 行。

#### 数据存储路径（与本次动态追踪对应）

1. TCG 执行客体存储指令时，生成代码调用相应的 MMU helper。本次 8 字节小端存储进入 `helper_le_stq_mmu()`，它把 `addr`、`val`、访问大小 `8`、`big_endian=false` 交给 `store_helper()`。
2. `store_helper()` 先计算 `mmu_idx`、直接映射 TLB 的 `index` 和 entry，并检查地址对齐。然后检查当前 entry 是否覆盖该页：
   - `tlb_hit(...)` 命中：直接使用缓存项，不调用 `tlb_fill()`，也不运行 RISC-V 页表遍历。
   - 主 TLB 未命中但 `victim_tlb_hit(...)` 命中：从 victim TLB 恢复缓存项，也不调用架构 refill。
   - 两级 TLB 都未命中：调用通用 `tlb_fill()`，随后重新查询 index 和 entry。
3. 通用 `tlb_fill()` 通过 `CPUClass->tlb_fill` 动态分派到 RISC-V 的 `riscv_cpu_tlb_fill()`。非 probe 的正常 refill 要么成功返回并满足 `assert(ok)`，要么经异常路径退出。
4. `riscv_cpu_tlb_fill()` 调用 `get_physical_address()`。它取得页对齐 PA 和权限；如果启用了 PMP，还会检查物理区间权限。成功时调用 `tlb_set_page()`，然后返回 `true`。`tlb_set_page()` 再转到 `tlb_set_page_with_attrs()`，创建 QEMU TCG 软件 TLB 项。
5. 成功返回 `store_helper()` 后，源码处理 MMIO、`TLB_RECHECK` 和跨页非对齐访问等特殊情况；普通 RAM 路径从 TLB entry 的 `addend` 计算 QEMU 宿主指针，并按大小和端序执行实际读写。本次的 `addr` 页内偏移 `0xff8`、大小 `8`，不会跨越 4 KiB 页。运行时捕获到 refill 返回原 helper，但没有单步到最终的 `stq_le_p()`；该最后一步由源码确认。

#### 指令取指路径

客体指令的取指慢路径由 QEMU 的 `cpu_exec()` / `tb_find()` 查找翻译块，再通过 `get_page_addr_code()` 取得指令页地址。`get_page_addr_code()` 检查 `entry->addr_code`：命中时复用 TLB；主 TLB 和 victim TLB 都未命中时，以 `MMU_INST_FETCH` 调用通用 `tlb_fill()`，之后复用同一 RISC-V `riscv_cpu_tlb_fill()` → `get_physical_address()` 地址转换路径。本次早期 Bare 取指与高地址 Sv39 取指的 GDB 调用栈都实际经过了 `get_page_addr_code()`。

#### Sv39 页表遍历和权限判断

- `get_physical_address()` 根据 `mmu_idx` 和 `mstatus` 确定有效特权级；M 模式或未启用 MMU 时直接 VA=PA。系统模式下 `satp.MODE=Bare` 也走直接映射。本次早期记录的 `satp=0` 对应 VA=PA；切换到 `satp=0x8000000000080205` 后，MODE=8（Sv39），PPN 指向根页表物理基址 `0x80205000`。
- Sv39 配置为三级、每级 9 位索引、每个 PTE 8 字节。函数先检查 VA 是否为合法 39 位符号扩展地址，再按每级 VPN 计算 PTE 地址并读 PTE。无效 V 位、保留的 R/W/X 组合、特权级与 U/S 位不匹配、超级页 PPN 未按层级对齐，或访问权限不满足时返回翻译失败。
- 权限条件按访问类型判断：取指要求 X；普通读要求 R（或 `MXR` 允许时可由 X 满足）；写要求 W。S 模式访问 U 页面还受 `SUM` 和取指限制。允许访问后，QEMU 按需设置 PTE 的 A 位；存储还需 D 位。之后根据叶子层级把较大映射的低 VPN 位合入 PPN，得到页对齐 PA，并转换出 TLB 的 R/W/X 权限。
- 本次根项 `0x200000cf` 中 V/R/W/X/A/D 均为 1、U 为 0，PPN 为 `0x80000`；它是顶层叶子项，适用于 S 模式存储。函数得到页对齐 PA `0x80204000`、`prot=7`。实际字节 PA 需再加 VA 页内偏移 `0xff8`，为 `0x80204ff8`。

#### 失败分支及本次验证范围

`riscv_cpu_tlb_fill()` 翻译成功并通过可选 PMP 检查后安装 TLB 项；翻译/PMP 失败时，probe 请求返回 `false`，普通访存则由 `raise_mmu_exception()` 按取指、读、写选择相应异常，再通过 `riscv_raise_exception()` 退出当前客体执行。本次没有触发异常分支。页错误/访问错误区分与异常号选择已由源码确认，不代表已动态验证。

源码参照：[`target/riscv/cpu_helper.c`](https://github.com/qemu/qemu/blob/v4.1.1/target/riscv/cpu_helper.c)、[`accel/tcg/cputlb.c`](https://github.com/qemu/qemu/blob/v4.1.1/accel/tcg/cputlb.c)。

## 2026-10-23：解释软件 TLB refill 的直观含义

用户询问 `fill` 对应向 TLB 加入什么。核对 QEMU 4.1.1 源码后，明确 `tlb_fill()` / `riscv_cpu_tlb_fill()` 是 QEMU 处理软件 TLB miss 的 C 函数路径，不是 RISC-V 客体指令；转换成功后，`tlb_set_page_with_attrs()` 在 `accel/tcg/cputlb.c:835` 通过 `copy_tlb_helper_locked(te, &tn)` 将候选项写入主 TLB。

本次实测项可直观描述为一张页级缓存记录：客体 VA 页 `0xffffffffc0204000` 映射到客体 PA 页 `0x80204000`，访问权限 `prot=7`，缓存页大小 `4096` 字节；原始 8 字节访存的页内偏移 `0xff8` 不变，所以字节 PA 为 `0x80204ff8`。QEMU `CPUTLBEntry` 保存 `addr_read`、`addr_write`、`addr_code` 等地址匹配/许可信息以及用于换算宿主地址的 `addend`，而不是直接复制客体 PTE 或存一个直白的 GPA 字段；IOTLB 伴随保存区域元数据。这是 QEMU TCG 软件 TLB，不是客体真实硬件 TLB 的内容。详细解释已补入 `b4_report_validation.md`，未修改 `prompt.md`。

## 2026-10-23：区分触发 refill 的数据地址与执行地址

用户追问为什么本次填入的是 `0xffffffffc0204ff8` 而不是早先看到的 `0x80200000`。解释要点：QEMU 不主动挑地址；正在执行的客体访存指令决定查找哪个地址对应的 TLB 项。`riscv_cpu_tlb_fill()` 的 `address` 是访存有效地址，不是客体 PC。本次调用栈经过 `store_helper()` / `helper_le_stq_mmu()`，且 `access_type=MMU_DATA_STORE`，所以 `0xffffffffc0204ff8` 是 store 的数据目标地址。早先 `0x80200000` 的调用是 `MMU_INST_FETCH` 且 `satp=0`，属于启动早期 Bare 模式的指令取指；这是另一个阶段、另一种访问。

本次 store 访问所在页在当前 `mmu_idx` 下没有主 TLB 或 victim TLB 命中，故进入 refill；页表解析得到 VA 页 `0xffffffffc0204000` 对应 PA 页 `0x80204000`，再安装该页的 QEMU 软件 TLB 项。它不表示 CPU 从 `0x80200000` 跳去执行 `0xffffffffc0204ff8`。相关执行逻辑已补充到 `b4_report_validation.md`。
