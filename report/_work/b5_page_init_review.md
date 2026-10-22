# B5：`page_init()` 独立复核记录

> 复核日期：2026-10-23
>
> 复核分支：`work/lab2/ganwenjie`
>
> 复核代码基线：`3abc2cc`（工作树仅有本次 Prompt/审核记录变更）
>
> 结论范围：当前 QEMU 默认 128 MiB DRAM 配置与仓库固定测试；未修改 A 的 `page_init()`、分配器或测试。

## 1. 审核结论

| 分类 | 结论 |
| --- | --- |
| 通过 | 默认 DTB 内存区间 `[0x80000000, 0x88000000)` 下，`page_init()` 正确排除了 OpenSBI/内核、`Page` 描述符数组及其页对齐填充，只把 `[0x80347000, 0x88000000)` 的 31929 个完整页交给 Best-Fit。 |
| 通过 | 所有 `Page` 描述符先被初始化为 Reserved；`best_fit_init_memmap()` 只清除上述空闲区的 Reserved 状态并设置一个空闲块首项，因此保留区和空闲区不重叠。 |
| 通过 | 实际 `make`、限时 `make qemu` 和 `make grade` 均完成预期检查；固定测试的 Page Init 与 Best-Fit 两项均为 OK，总分 `20/20`。 |
| 存疑 | 若 DTB 报告的内存终点高于 `PHYSICAL_MEMORY_END`，实现会把 `npage` 截断到 `0x88000`，但固定测试按未截断的 DTB 终点计算期望值。这是固定 128 MiB 设计上限与测试契约的潜在不一致；当前默认配置没有触发。 |
| 存疑 | `pa2page()` 只检查 PPN 上界，没有检查 `PPN(pa) >= nbase`。当前 `page_init()` 和固定测试传入的 PA 均在受管区间内，因此没有实际失败；对低于 `0x80000000` 的 PA 调用仍依赖调用者前置条件。 |
| 需 A 修复 | 当前默认配置和已执行测试下未发现可复现的 `page_init()` 缺陷。若课程要求支持大于固定上限的 RAM，应由 A 明确 `npage` 的契约并统一实现与固定测试预期。 |

因此，B5 的独立审核结论是：**当前测试配置下通过；没有已复现且必须立即修改的 A 代码问题；保留两个未在替代 DTB 配置上动态验证的边界风险。** 按团队流程，在 A 确认审核意见前不把 B5 标为正式交叉审核完成。

## 2. 调用顺序与输入来源

1. `kern_init()` 先清零 BSS，再调用 `dtb_init()`，最后调用 `pmm_init()`：`code/kern/init/init.c:28-40`。
2. `dtb_init()` 将启动参数中的 DTB 物理地址加上固定映射偏移，找到 `memory*` 节点的首个 `reg` 属性，并保存 `memory_base`、`memory_size`：`code/kern/driver/dtb.c:39-105,107-150`。
3. `pmm_init()` 先初始化 Best-Fit 管理器，再调用 `page_init()`，之后才运行分配器自检：`code/kern/mm/pmm.c:35-45,127-151`。
4. `page_init()` 从 `get_memory_base()`、`get_memory_size()` 读取已保存的 DTB 结果：`code/kern/mm/pmm.c:66-76`。

当前 DTB 解析器假设 `reg` 使用两个 64 位值（地址、长度），并只取第一个内存区间。该假设与当前 QEMU virt 的 DTB 一致；它不是对任意 cell 数量、多段 RAM 或任意 DTB 的通用解析器。

## 3. 默认配置的完整地址推导

当前运行得到：

- `memory_base = 0x80000000`
- `memory_size = 0x08000000`（128 MiB）
- `memory_end = memory_base + memory_size = 0x88000000`
- `PGSIZE = 0x1000`，`sizeof(struct Page) = 40 = 0x28`
- `PHYSICAL_MEMORY_OFFSET = 0xffffffff40000000`

### 3.1 DTB 输入 → `nbase` / `npage`

`nbase` 是固定的 DRAM 起始物理页号：

```text
nbase = DRAM_BASE / PGSIZE
      = 0x80000000 / 0x1000
      = 0x80000
```

`page_init()` 将可管理终点限制为固定上限并向下页对齐：

```text
managed_end = ROUNDDOWN(min(memory_end, PHYSICAL_MEMORY_END), PGSIZE)
            = ROUNDDOWN(min(0x88000000, 0x88000000), 0x1000)
            = 0x88000000

npage       = PPN(managed_end) = 0x88000
page_count  = npage - nbase    = 0x8000 = 32768
```

这里的 `npage` 是“受管物理页号上界”，不是空闲页数。`pages[0]` 描述 PPN `0x80000`，`pages[page_count - 1]` 描述 PPN `0x87fff`。依据：`code/kern/mm/pmm.c:20-22,88-96` 和 `code/kern/mm/pmm.h:83-108`。

### 3.2 内核 `end`：VA → PA

链接脚本从 `0xffffffffc0200000` 布置内核，并在 BSS 后定义 `end`：`code/tools/kernel.ld:7-47`。当前内核符号表给出：

```text
end VA       = 0xffffffffc0206078
kernel_end PA= PADDR(end)
             = end VA - va_pa_offset
             = 0xffffffffc0206078 - 0xffffffff40000000
             = 0x80206078
```

`page_init()` 在调用 `PADDR(end)` 前先设置 `va_pa_offset = PHYSICAL_MEMORY_OFFSET`，并确认 DTB 区间完整包含 `[KERNEL_BEGIN_PADDR, kernel_end)`：`code/kern/mm/pmm.c:71-86`。启动栈 `[0xffffffffc0203000, 0xffffffffc0205000)` 和启动页表 `0xffffffffc0205000` 均位于 `end` 之前，见 `code/kern/init/entry.S:44-70`，所以都包含在内核保留范围内。

### 3.3 `Page` 描述符数组

描述符数组从内核结束后的第一个完整页开始：

```text
descriptors_pa  = ROUNDUP(0x80206078, 0x1000)
                = 0x80207000

descriptor_bytes= page_count * sizeof(struct Page)
                = 32768 * 40
                = 0x140000

descriptors_end = 0x80207000 + 0x140000
                = 0x80347000

pages VA        = descriptors_pa + va_pa_offset
                = 0xffffffffc0207000
pages VA end    = 0xffffffffc0347000
```

乘法前的容量检查使用除法确认 `page_count` 个描述符能放入 `[descriptors_pa, managed_end)`，避免先乘法再越界：`code/kern/mm/pmm.c:98-104`。数组的物理起点和虚拟起点都页对齐。

### 3.4 空闲区 → `init_memmap()`

```text
free_begin = ROUNDUP(descriptors_end, PGSIZE)
           = 0x80347000

free_count = (managed_end - free_begin) / PGSIZE
           = (0x88000000 - 0x80347000) / 0x1000
           = 0x7cb9
           = 31929 pages
```

`page_init()` 的实际交接为：

```text
init_memmap(pa2page(0x80347000), 31929)
```

`PPN(0x80347000) = 0x80347`，相对 `nbase` 的描述符索引为 `0x347`，因此：

```text
pa2page(free_begin)
    = &pages[0x80347 - 0x80000]
    = &pages[0x347]
    = 0xffffffffc020f318
```

反向转换满足：

```text
page2pa(&pages[0x347])
    = (0x347 + 0x80000) << 12
    = 0x80347000
```

依据：`code/kern/mm/pmm.c:103-123`、`code/kern/mm/pmm.h:83-108`。

## 4. VA、PA、PPN 与 `Page *` 对照

| 对象 | VA 或 C 指针 | PA | PPN | 对应 `Page *` / 含义 |
| --- | --- | --- | --- | --- |
| 内核入口 | `0xffffffffc0200000` | `0x80200000` | `0x80200` | `&pages[0x200] = 0xffffffffc020c000`，Reserved |
| 当前 `end` | `0xffffffffc0206078` | `0x80206078` | `0x80206` | `&pages[0x206] = 0xffffffffc020c0f0`，尾页 Reserved |
| 描述符存储起点 | `pages = 0xffffffffc0207000` | `0x80207000` | `0x80207` | 描述该物理页的是 `&pages[0x207] = 0xffffffffc020c118`，Reserved |
| 首个空闲物理页 | 由固定映射可访问为 `0xffffffffc0347000` | `0x80347000` | `0x80347` | `&pages[0x347] = 0xffffffffc020f318`，交给 `init_memmap()` |
| 受管区终点 | 不含在区间内 | `0x88000000` | `0x88000` | `npage` 上界，不存在对应的有效数组元素 |

`PADDR()` 把内核虚拟地址转换为物理地址；`pa2page()` 把物理地址所属 PPN 转换为描述符指针。两者不能互换：对 `pages` 使用 `PADDR()` 得到的是“描述符数组存放在哪里”，对某个空闲 PA 使用 `pa2page()` 得到的是“描述该物理页的 `struct Page` 在数组中的位置”。

## 5. 保留区与空闲区状态检查

| 物理范围 | 页描述符状态 | 审查依据与结论 |
| --- | --- | --- |
| `[0x80000000, 0x80200000)` | Reserved | `pages` 覆盖从 `nbase` 开始的全部页，初始化循环先为每项设置 Reserved；OpenSBI/内核前区域不会进入空闲区。通过。 |
| `[0x80200000, 0x80207000)` | Reserved | 包含内核镜像、启动栈、启动页表和 `end` 所在尾页；`free_begin` 晚于该范围。通过。 |
| `[0x80207000, 0x80347000)` | Reserved | 这是描述符数组自身占用的物理内存；初始化循环完成后没有对该范围调用 `init_memmap()`。通过。 |
| `[0x80347000, 0x88000000)` | 非 Reserved；首项设置 `PageProperty`，`property=31929` | `best_fit_init_memmap()` 逐页验证原先 Reserved，再清空空闲页 flags，并只在块首设置属性：`code/kern/mm/best_fit_pmm.c:28-47`。通过。 |
| `>= 0x88000000` | 不受当前 `pages` 数组管理 | `npage=0x88000` 是上界；默认 DTB 恰好在此结束。通过。 |

当前默认终点已页对齐，没有末尾残页。对未页对齐的 DTB 终点，`managed_end = ROUNDDOWN(...)` 会排除末尾不完整页；`PPN(memory_end)` 与向下对齐后的 PPN 相同，所以固定测试中的 `expect_npage` 仍一致。

## 6. 异常与边界分支静态审查

| 情形 | 当前行为 | 结论 |
| --- | --- | --- |
| DTB 基址或大小为 0 | `page_init()` panic：`invalid DTB memory range` | 明确拒绝；未动态注入该 DTB。 |
| `memory_base + memory_size` 溢出 | 加法前用 `memory_size > UINT64_MAX - memory_base` 检查并 panic | 通过静态审查。 |
| DTB 区间不完整包含内核 | 检查基址、`kernel_end` 与固定上限并 panic | 通过静态审查。 |
| 描述符容量不足 | 乘法前以可用字节数除以 `sizeof(struct Page)` 检查并 panic | 通过静态审查。 |
| 描述符之后没有完整空闲页 | 不调用 `init_memmap()`，避免触发其 `n > 0` 断言；打印无空闲页 | 安全处理，但固定测试因要求 `nr_free_pages() > 0` 不会通过；未动态注入。 |
| DTB 终点小于固定上限且不页对齐 | 向下对齐，只管理完整页 | 通过静态审查。 |
| DTB 终点大于 `0x88000000` | 实现截断 `managed_end`/`npage`；测试仍期待原 DTB 终点 PPN | 存疑：设计上限与固定测试契约不一致，未动态注入。 |

## 7. 固定测试覆盖与缺口

`tools/inject.py:146-169` 将测试复制到临时评分树，并在 `pmm_init()` 末尾调用 `test_page_init()`、`test_best_fit()`。`test_page_init()` 覆盖：

- `va_pa_offset` 是否等于固定映射偏移；
- DTB 内存基址和大小是否非零；
- `npage` 是否等于 DTB 内存终点的 PPN；
- `pages` 是否非空、页对齐且位于链接符号 `end` 之后；
- 是否存在合理数量的空闲页；
- 内核尾页和描述符数组起始页是否 Reserved；
- `page2pa()` / `pa2page()` 是否能往返。

依据：`code/tools/test_lab2.c:11-101`。

固定测试没有逐页检查整个描述符数组的 Reserved 状态，也没有精确比较 `free_begin`/`free_count`，不覆盖无效 DTB、容量不足、无空闲页、RAM 超过固定上限、多段 RAM 或低于 `nbase` 的 `pa2page()` 调用。因此本审核没有用 `20/20` 替代上述源码检查。

## 8. 实际运行证据

### 构建

```sh
make -C code
```

退出码 `0`。评分后再次完整重建也退出 `0`。

### 限时 QEMU 启动

```sh
timeout --foreground --signal=INT --kill-after=2s 8s make -C code qemu
```

退出码 `124`，原因是内核完成初始化后进入预期的无限循环，由 `timeout` 主动结束。关键输出：

```text
Physical memory from DTB: [0x0000000080000000, 0x0000000088000000)
Managed physical memory end: 0x0000000088000000
Page descriptors: PA [0x0000000080207000, 0x0000000080347000)
Free physical pages: [0x0000000080347000, 0x0000000088000000), 31929 pages
check_alloc_page() succeeded!
```

### 固定评分

```sh
make -C code grade
```

退出码 `0`：

```text
-check page init: OK
-check best fit:  OK
Total Score: 20/20
```

评分脚本实际检查串口中是否出现 `[Test 1] Page Init: PASS` 与 `[Test 2] Best-Fit: PASS`：`code/tools/grade.sh:231-244`。

## 9. 提交给 A 的最小审核意见

1. 默认 128 MiB QEMU 配置下，`page_init()` 的地址计算、Reserved 状态和空闲区交接均通过独立静态复核及实际运行，无需修改。
2. 请确认本实验是否只承诺 `PHYSICAL_MEMORY_END = 0x88000000` 的固定内存上限。如果需要接受更大的 DTB RAM 区间，应统一 `npage` 的定义、受管上限及 `test_page_init()` 对 `npage` 的预期；当前代码与测试在该情形下会不一致。
3. `pa2page()` 对低于 `nbase` 的 PA 没有防护，但当前 B5 路径没有传入此类地址。除非接口契约要求接受任意 PA，否则不建议为本实验扩大修改范围。
