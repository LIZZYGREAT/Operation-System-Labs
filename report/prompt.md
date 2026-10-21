# 提示词汇总

## 模板

```
[PROMPT]
做什么、改哪里、怎么改

任务：说清楚目标文件和模块的具体功能。如果涉及多个文件，每个文件负责什么，要在这里交代。不要让AI自己猜该改哪个文件。

操作要求：约束AI的行为方式。比较重要的一条是"必须在项目中修改实际文件"——如果不强调这一点，AI很可能只输出代码片段而不真正改文件。此外还可以说清楚：保留现有代码，只在标记位置补全，不要删除既有逻辑。

输出要求：告诉AI使用下面[RELY]、[GUARANTEE]、[SPECIFICATION]中提供的信息，以工程真实代码为准，不要引入无关功能。

[RELY]
可以依赖什么——宏定义、数据结构、已有函数

提供AI完成任务所需的最小可信上下文——宏定义、数据结构、可调用的函数、相关全局变量。

[GUARANTEE]
必须实现或修改什么——需要交付的函数列表

列出这次任务必须实现或修改的所有函数签名。它是一份"交付清单"：这些函数必须在最终代码中出现，签名必须和声明一致。

[SPECIFICATION]
每个函数的详细行为规格

## <func1>

这是提示词最核心的部分。对每个主要函数，都要用统一的格式描述它的行为：

Pre-Condition：调用这个函数前，系统应该处于什么状态？参数有什么约束？

Post-Condition：函数执行完成后，系统应该处于什么状态？这里描述的是函数执行后的总体状态，包括数据结构、资源、全局状态等方面必须满足的条件。

Case 1、Case 2……：在Post-Condition下面，按照不同的执行情况分别描述函数的具体行为。每个Case都用一段话说明在什么条件下触发、需要执行什么操作，以及执行完成后需要满足什么状态。对操作系统代码来说，一个函数可能存在多种不同的执行路径，例如正常执行、参数异常、资源不足或特殊系统状态等。将这些情况分别列出，可以更准确地描述函数的完整行为。

Requirements：特殊的时序要求、并发约束、性能要求，或者其他特殊需求。

其中，Case和Requirements都是可选的，是否使用应根据具体函数的实际情况决定。如果函数只有单一执行路径或不存在需要单独描述的特殊执行路径，不需要设置Case；如果函数没有额外的特殊要求，不需要设置Requirements。
```

## 功能模块1：物理内存初始化功能

### 任务一：实现物理内存探测与页描述符初始化功能


```
[PROMPT]

任务：
完成 Lab2 的物理内存初始化功能，在 kern/mm/pmm.c 中实现 static void page_init(void)。

该功能需要根据设备树提供的物理内存信息，建立可管理物理页的Page 描述符布局，并将排除 OpenSBI、内核镜像和描述符数组占用空间后的完整空闲页面交给当前物理内存管理器。

操作要求：
必须在项目中修改实际文件，而不是仅展示代码。
先阅读相关源码，以工程中的真实定义为准。
保留已有代码，在标有“lab2 function1 YOUR CODE”的位置补全实现。
本任务只负责物理内存初始化，不实现或修改分配算法，不切换当前 pmm_manager，不修改启动页表、设备树解析器、公共接口或已有测试。

输出要求：
使用下面提供的上下文、接口清单和行为规格完成修改。
完成后说明修改位置、主要地址关系、验证方法和实际验证结果。
如果构建被其他尚未完成的模块阻塞，明确指出阻塞原因，不擅自修改其他成员负责的模块，也不将未运行的检查描述为已经通过。


[RELY]

以下信息来自本实验框架，使用前应与当前工程核对。

1. 调用环境

kern_init() 已通过 dtb_init() 读取设备树信息。
pmm_init() 在调用 page_init() 前，已经选择并初始化当前
pmm_manager；page_init() 返回后会进行分配器检查。

2. 相关文件

kern/mm/pmm.c：初始化入口、相关全局变量和 init_memmap()。
kern/mm/pmm.h：地址转换、页描述符转换和分配器接口。
kern/mm/memlayout.h：内存布局、Page 结构与页面状态宏。
kern/mm/mmu.h：页面大小和物理页号相关定义。
kern/driver/dtb.h：设备树内存信息查询接口。
libs/defs.h：ROUNDUP、ROUNDDOWN 等工具宏。

3. 内存布局宏

#define PHYSICAL_MEMORY_END     0x88000000
#define PHYSICAL_MEMORY_OFFSET  0xFFFFFFFF40000000
#define KERNEL_BEGIN_PADDR      0x80200000
#define KERNEL_BEGIN_VADDR      0xFFFFFFFFC0200000

页面大小使用工程中的 PGSIZE，页号转换使用 PPN 等现有定义，不要自行引入另一套名称或不一致的常量。

4. 页描述符结构

struct Page {
    int ref;
    uint64_t flags;
    unsigned int property;
    list_entry_t page_link;
};

Page 描述符记录某个物理页的状态；描述符本身的地址不等于它所描述页面的物理地址。描述符数组本身也占用物理内存。

5. 相关全局状态

struct Page *pages;
size_t npage;
uint64_t va_pa_offset;
const size_t nbase = DRAM_BASE / PGSIZE;

pages[0] 对应物理页号 nbase。npage 表示可管理物理内存的物理页号上界，不是空闲页数。描述符数组覆盖的页号区间为 [nbase, npage)。

6. 可使用的已有资源

uint64_t get_memory_base(void);
uint64_t get_memory_size(void);

static void init_memmap(struct Page *base, size_t n);
static inline struct Page *pa2page(uintptr_t pa);

extern char end[];

以及：
PADDR、PPN、ROUNDUP、ROUNDDOWN；
SetPageReserved、PageReserved、set_page_ref；
panic()、cprintf()。

其中：
end 是内核镜像结束位置的虚拟地址。
PADDR 使用 va_pa_offset 进行转换。
pa2page 使用 pages、npage、nbase 建立对应关系。
init_memmap 将页面交给当前 pmm_manager，其长度参数必须大于零。
这些已有接口只作为依赖使用，不属于本次需要重新实现的函数。


[GUARANTEE]

必须实现的接口：

static void page_init(void);

该函数没有参数，也没有返回值。
它负责建立正确的物理页描述符布局、维护相关全局状态，
并注册初始空闲物理页。

允许按需在 pmm.c 中新增 static 辅助函数。
不得改变公开接口或其他模块已有行为。


[SPECIFICATION]

## page_init

Pre-Condition：

- 内核已进入当前框架提供的虚拟地址映射环境。
- 设备树解析流程已经执行，可以通过现有查询接口获取内存信息；查询结果仍可能无效，需要本函数检查。
- 当前 pmm_manager 已完成内部管理状态初始化，但尚未接管本函数将注册的空闲页面。
- 内核结束符号、页面大小、内存管理上界和相关数据结构均使用当前工程的真实定义。

Post-Condition：

- 成功返回时，va_pa_offset 正确表示当前内核直接映射区域的虚拟地址与物理地址偏移关系。
- npage 正确表示实际可管理范围的物理页号上界；管理范围不得超过设备树报告的内存范围和框架支持的物理内存上界。
- pages 指向位于内核镜像之后、按页对齐的连续描述符数组，其覆盖范围与现有页号转换接口保持一致。
- 描述符数组必须完整位于可访问、可管理的内存中，不与内核镜像重叠。
- OpenSBI、内核镜像、描述符数组及相关对齐占用不会进入空闲区，对应页面保持保留状态。
- 当前 pmm_manager 仅接管合法范围内、排除上述占用后剩余的完整物理页。
- 输出探测到的物理内存范围，使初始化结果可以检查。

Case 1：内存信息有效，且有足够空间用于描述符和空闲页

建立与现有地址转换接口一致的描述符布局，确保已占用页面受到保护，并将剩余的完整空闲页注册给当前物理内存管理器。注册页面对应的地址、页数和描述符范围必须相互一致。

Case 2：设备树内存信息无效

当查询结果为空、大小为零、地址范围计算溢出，或者报告范围与当前框架及内核占用不兼容，无法建立合法布局时，通过panic() 终止初始化，不继续使用错误地址或注册空闲页。

Case 3：报告内存超过框架支持上界

实际管理范围限制在框架支持范围以内，不能因为设备树报告了更多内存就访问或注册框架范围之外的页面。描述符数量、可用空间和相关地址关系均以受限后的范围为准。

Case 4：存在非页对齐边界

描述符数组和空闲区起点满足页对齐要求。不能将部分被占用的页面或内存末尾不足一页的部分注册为空闲。对齐产生的空间占用必须计入布局判断。

Case 5：无法容纳完整描述符数组

如果内核之后的合法空间无法容纳所需描述符数组，必须通过 panic() 终止，不能越界写入或继续构造无效空闲区。

Case 6：描述符可合法建立，但没有剩余完整空闲页

保持合法的描述符和保留状态，不调用 init_memmap()，不把零页、越界页面或已占用页面交给分配器。明确输出没有可注册完整空闲页的情况。

Requirements：

- 严格区分虚拟地址、物理地址、物理页号和 Page 描述符地址。
- 使用依赖相关全局状态的转换接口前，该状态必须已经有效。
- 不能将默认的 128 MiB 配置直接写成探测结果。
- 不重复注册页面，不通过直接修改分配器内部链表代替 init_memmap()。
- 保留已有 lab2 标记；没有提供真实学号时，不编造学号。
- 验证至少涵盖正常布局、管理上界限制、对齐、已用页面保留、无效内存信息及无剩余完整页这些场景。
- 不修改现有测试或伪造检查输出来满足验收。
```

## 功能模块2：物理内存分配功能

### 任务二：实现 Best-Fit 连续物理内存分配算法

> 以下记录 B 线在 Lab2 中的需求分析、实现和验收。各条提示均按本文件开头的 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 模板整理；处理记录保留实际检查结果。

### 2026-10-20：项目环境、要求与 B 线分工核对

[PROMPT]

任务：检查当前 ucore Lab2 项目的环境、配置、课程要求和 B 线分工，重点阅读 `requirements.md`。

操作要求：以当前仓库文件为准，先检查项目和文档，不修改代码。

输出要求：概述环境、课程要求、B 线任务边界和当前实现状态；记录实际检查结果。

[RELY]

- `docs/lab2/requirements.md`、B 线分工指南和实验实施方案。
- 仓库 README、Makefile、PMM 接口和相关分配器实现。

[GUARANTEE]

- 只阅读和整理，不修改源代码，不运行未要求的测试。
- 区分 Best-Fit 本人负责的工作与其他成员负责的 `page_init()`。

[SPECIFICATION]

检查实验环境和构建/调试入口；梳理 B1、B2 及 B 线其他交付项和依赖；记录 Best-Fit 固定测试、实验报告要求及当时源码状态。

**检查与处理记录**

- 阅读 `docs/lab2/requirements.md`、`docs/lab2/Lab2 三人分工指南.md`、`docs/lab2/Lab2 实验实施方案与 Workflow.md`、`docs/environment_report.md`，并查看 README、Makefile 和 PMM 相关接口/实现。
- 当时实测 GNU Make 4.3、RISC-V GCC 10.2.0、RISC-V GDB 10.1、QEMU 4.1.1。B 线包括 B1 Best-Fit、B2 接入与验收、B3 调试环境准备、B4 地址转换记录、B5 对 A 线初始化的复核；B2 依赖 A2 的 First-Fit 基线验证。
- 记录当时 `best_fit_pmm.c` 的占位、`pmm.c` 注册 First-Fit、`page_init()` 尚未实现，以及固定测试不可修改等状态。本轮没有修改实验代码或运行测试。

### 2026-10-20：梳理 First-Fit 与 Best-Fit 实现要求

[PROMPT]

任务：阅读 First-Fit 的实现逻辑、函数职责和重要变量，并据此梳理后续 Best-Fit 需要实现的内容。

操作要求：以当前源码和课程要求为准，只做阅读分析，不修改代码。

输出要求：按执行顺序解释 First-Fit 的初始化、分配、释放与合并过程，并指出 Best-Fit 的接口和策略差异。

[RELY]

- `kern/mm/default_pmm.c`、`kern/mm/best_fit_pmm.c`。
- `kern/mm/pmm.h`、`kern/mm/memlayout.h`、`libs/list.h`。
- Lab2 requirements 和 B 线分工说明。

[GUARANTEE]

- 不实现或修改任何代码。
- 结论引用实际函数、数据结构和文件路径。

[SPECIFICATION]

解释 `free_list`、`nr_free`、`PageProperty`、`property` 的状态约定，说明 `init_memmap()`、First-Fit 分配、拆分、释放及地址相邻合并；指出 Best-Fit 应改变的候选块选择步骤，以及固定测试要求。

**检查与处理记录**

- 梳理了 First-Fit 的空闲块链表、页标志和计数约定，以及初始化登记、首次适配、拆分、释放、地址排序和前后相邻块合并过程。
- 明确 Best-Fit 需要补齐管理器接口、选择最小可容纳块、处理拆分/合并，并保留固定测试可访问的 `free_list` 与 `nr_free`。本轮只阅读和梳理。

### 2026-10-20：按执行顺序阅读 PMM 与 Best-Fit 固定测试

[PROMPT]

任务：阅读 PMM、First-Fit、Best-Fit、页面布局和链表代码，并了解 Lab2 固定测试；面向具备虚拟内存和 C 基础的初学者，按实际执行顺序解释物理页分配器。

操作要求：只阅读指定源码，不编写、修改或提交代码，也不直接给实现方案。引用实际函数名和路径；若源码与指导书或描述冲突，先指出冲突。

输出要求：解释独立物理页分配器的作用、`page_init()` 与算法职责、PMM 调用链、关键结构状态、页面从登记到分配/释放的变化、First-Fit 与 Best-Fit 的差异，以及固定测试约束；最后列出现有功能、待实现项和必须不变代码。

[RELY]

- `kern/mm/pmm.c`、`kern/mm/pmm.h`。
- `kern/mm/default_pmm.c`、`kern/mm/default_pmm.h`。
- `kern/mm/best_fit_pmm.c`、`kern/mm/best_fit_pmm.h`。
- `kern/mm/memlayout.h`、`libs/list.h`、`libs/defs.h`。
- `tools/test_lab2.c`，仅用于了解后续测试。

[GUARANTEE]

- 不修改源码或测试。
- 对其他成员的未实现工作明确标注，不越界补写。

[SPECIFICATION]

1. 说明内核为何需要独立的物理页分配器，以及 `page_init()` 和具体算法的职责边界。
2. 梳理 `pmm_init()` → `init_pmm_manager()` → `page_init()` → `init_memmap()` → `check_alloc_page()` 调用关系，指出当时由 A 负责的未实现步骤。
3. 解释 `struct Page`、`free_area_t`、`free_list`、`nr_free`、`property` 和 `page_link` 保存的状态。
4. 跟踪 `init_memmap(base,n)`、`alloc_pages(n)`、`free_pages(base,n)` 对 Page 描述符和链表的改变。
5. 对比 `default_pmm.c` 的 First-Fit 选择与 Best-Fit 应改变的逻辑环节。
6. 说明 `basic_check()`、`best_fit_check()` 对内部结构的明确约束。

**检查与处理记录**

- 按指定清单阅读 PMM 接口、两个分配器、页面结构、链表和 `tools/test_lab2.c`，并搜索注册及初始化调用。
- 记录当时 `pmm.c` 调用了但源码中尚未定义 `page_init()`；`tools/test_lab2.c` 的 Best-Fit 外部检查只检查注册指针，算法断言由 `best_fit_check()` 执行。本轮只阅读，没有修改或测试。

### 2026-10-20：Best-Fit 需求分析与接口设计

[PROMPT]

任务：基于已阅读的 ucore Lab2 代码，为 Best-Fit 连续物理页分配器形成可供实现与验收的需求规格，当前阶段不修改文件。

操作要求：唯一目标是 `kern/mm/best_fit_pmm.c`；遵守 `pmm.h` 的 `struct pmm_manager`，复用 `Page` 描述符和现有双向链表。不得修改固定测试、First-Fit、公共接口、内存布局、链表框架、`pmm.c` 注册或 A 负责的 `page_init()`。

输出要求：说明接口职责、状态不变量、各接口前置/成功/失败状态、Best-Fit 选择与并列规则、拆分/合并行为、固定测试约束及测试矩阵；以实际源代码和指导书冲突时指出差异。

[RELY]

- `kern/mm/pmm.h`：分配器接口。
- `kern/mm/memlayout.h`：`struct Page`、`free_area_t`、页面标志。
- `kern/mm/default_pmm.c`：First-Fit 的链表排序、拆分、合并和计数参考。
- `kern/mm/best_fit_pmm.c`：固定的 `basic_check()`、`best_fit_check()` 行为。
- `libs/list.h`、`libs/defs.h`：双向链表及描述符反查宏。

[GUARANTEE]

- 仅提交需求分析与接口设计，不改任何文件。
- 覆盖 `init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages`、`check` 和 `best_fit_pmm_manager`。
- 给出精确匹配、最优块选择、分配失败、拆分、单侧/双侧合并、碎片化及空链表等验收场景。

[SPECIFICATION]

### 状态不变量

空闲块首地址设置 `PageProperty`，`property` 保存连续页数；`free_list` 按物理地址排序；`nr_free` 等于所有空闲块页数之和。已分配页不在空闲链表中，空闲块不得重叠。

### 分配与回收

从所有 `property >= n` 的块中选择最小块，同尺寸选择低地址。无连续块时返回 `NULL` 且不修改空闲状态。分配较大块时登记余块并更新标志、链表和计数。释放时按地址插入并分别合并左右相邻块，合并后仅块首保留 `PageProperty`，总空闲数只增加本次释放页数。

### 固定测试约束与场景

固定测试会临时隔离 `free_list` 并调整 `nr_free`，完成后恢复保存的状态；实现不能只凭 `nr_free` 推断链表中有可分配块。验收场景包括精确匹配、最小合适块、同尺寸低地址、超总量失败、碎片化失败、拆分、左/右/双侧合并、重复分配释放后恢复和空链表失败。

### 2026-10-20：实现 Best-Fit 分配器

[PROMPT]

任务：在当前 ucore Lab2 仓库中实现完整的 Best-Fit 连续物理页分配器，使其可作为独立 `pmm_manager` 实现注册。将 `//lab2 function2-2 YOUR CODE` 标记替换为 `//lab2 function2-2 2412700`。

操作要求：只修改 `kern/mm/best_fit_pmm.c` 中实现占位区域；必要的前置声明也放在该处。不得改动 `basic_check()`、`best_fit_check()`、其他源文件、公共接口、构建/评分脚本或测试。保留 lab2 标记，不自动提交。

输出要求：实现全部接口；实际运行当前环境可执行的静态检查或构建，并说明修改、状态不变量、命令结果和未验证事项。若 `page_init()` 阻塞完整构建，准确指出其属于 A 任务，不得补占位实现。

[RELY]

- `kern/mm/pmm.h`：`struct pmm_manager` 与外部调用约定。
- `kern/mm/memlayout.h`：`Page`、`free_area_t`、页面标志和 property 宏。
- `libs/list.h`、`libs/defs.h`：双向链表及节点操作。
- `kern/mm/default_pmm.c`：First-Fit 的初始化、地址排序、分裂、释放合并和计数参考。
- `kern/mm/best_fit_pmm.c` 中不可修改的固定测试。

[GUARANTEE]

- 实现 `best_fit_init()`、`best_fit_init_memmap()`、`best_fit_alloc_pages()`、`best_fit_free_pages()`、`best_fit_nr_free_pages()`、`best_fit_pmm_manager` 和调用原有 `best_fit_check()` 的检查入口。
- 让原有固定测试可访问 `free_list` 与 `nr_free`；不切换 `pmm.c` 中的管理器，不实现 A 负责的 `page_init()`。
- 不修改测试来规避失败，不创建额外算法、动态分配依赖或不必要抽象。

[SPECIFICATION]

### 分配器行为

`init` 建立空链表并将空闲页数清零。`init_memmap(base,n)` 要求 `n > 0`、页面处于保留状态；清理描述符状态、引用计数，将整个区间登记为一个地址有序空闲块。

`alloc_pages(n)` 要求 `n > 0`。在所有足够大的块中选择最小者，同尺寸选择低地址；失败时返回 `NULL` 且状态不变。成功时返回块首描述符；若块较大，登记剩余连续区间，更新 `PageProperty`、`property`、链表和 `nr_free`。

`free_pages(base,n)` 要求释放的是已分配的连续页面。恢复释放页的标志和引用状态，按物理地址插入，并分别合并左邻、右邻空闲块。释放和合并后不得出现重叠、重复节点或错误块首标志，`nr_free` 仅增加本次释放页数。

### 验收场景

精确匹配；多候选选择最小块及同尺寸低地址优先；请求超过总量；总量足够但无连续块；拆分；左合并、右合并、双侧合并；反复分配释放后恢复；空链表失败；固定测试临时隔离并恢复 `free_list`、`nr_free`。

### 2026-10-20：审查 Best-Fit 实现并独立验证

[PROMPT]

任务：审查已实现的 Best-Fit 分配器，并在不接入 B2 的前提下做独立源码检查与可执行验证。

操作要求：先核对改动边界和固定测试是否原样保留，再检查函数签名、函数指针、Best-Fit 固定测试引用、链表与计数维护。只在发现可定位的 B1 错误时修改 `best_fit_pmm.c`；不得修改外部框架、测试或临时补写 A 的 `page_init()`。

输出要求：列出检查方法、实际命令和输出，区分固定自检、独立场景检查与全项目构建阻塞，并说明已验证和未验证项目。

[RELY]

- `kern/mm/best_fit_pmm.c`、`kern/mm/best_fit_pmm.h`。
- `kern/mm/pmm.h`、`kern/mm/default_pmm.c`、`kern/mm/pmm.c`。
- 当前 Git diff、固定测试和可用 RISC-V 工具链。

[GUARANTEE]

- 固定 `basic_check()` 和 `best_fit_check()` 不得改写。
- 不进行 B2 管理器切换，不修改 A 负责的 `page_init()`。
- 不以跳过测试或占位代码制造通过结果。

[SPECIFICATION]

检查范围包括：候选选择和相同大小的确定性规则、地址排序、拆分、左右及双侧合并、空链表失败、总空闲量足够但无连续块、计数和页面标志、已分配页不留在空闲链表、释放不产生重叠、固定测试临时隔离与恢复 `free_list`/`nr_free`。若完整构建因 `page_init()` 缺失失败，应归属 A 前置条件并如实记录。

**检查与处理记录**

- B1 的源代码差异位于实现区域，固定测试未修改；`git diff --check` 通过。函数签名与 `pmm_manager` 匹配，管理器 `.check` 调用原 `best_fit_check()`。
- 独立 harness 记录覆盖精确匹配、最优块和同尺寸低地址、排序、拆分、单侧/双侧合并、碎片化/总量不足失败、空链表且 `nr_free` 未清零、重复分配释放恢复。首次 harness 失败源于手工填写的总页数期望值，修正 harness 后重跑通过；没有修改生产代码修复 Bug。
- 当时完整构建因 A 的 `page_init()` 未实现而失败；未修改 A 的代码。RISC-V 语法检查和对象符号检查通过。

### 2026-10-20：B2 接入 Best-Fit 并验收

[PROMPT]

任务：将已完成的 Best-Fit 分配器接入 Lab2 内核，完成 B2 构建与运行验收。

操作要求：先确认 `page_init()`、default First-Fit 基线和 B1 实现均已完成；只在 `pmm.c` 的 `init_pmm_manager()` 切换分配器，不修改 `page_init()`、测试或评分脚本。失败时区分构建、页初始化、分配器与运行环境问题。

输出要求：报告改动、调用链、实际测试命令与输出、验收结论；不自动提交。

[RELY]

- `kern/mm/pmm.c` 负责管理器注册、物理页初始化调用及分配接口动态分派。
- `kern/mm/best_fit_pmm.c` 已提供 `best_fit_pmm_manager` 与固定 `best_fit_check()`。
- `tools/test_lab2.c` 和 `tools/grade.sh` 提供 Lab2 自动验收；评分脚本会检查 Page Init 与 Best-Fit 的输出。

[GUARANTEE]

- 基线通过后，将注册对象切换为 `best_fit_pmm_manager`，保留既有打印及其他初始化代码。
- 验证 `pmm_init()`、`page_init()`、`init_memmap()`、`check_alloc_page()` 及分配/释放接口的动态分派关系。
- 实际执行 Makefile 构建、QEMU 运行和 `make grade`；不改测试脚本、不补写 A 的实现。

[SPECIFICATION]

### B2 接入与验收

Pre-Condition：`page_init()` 已实现；default 管理器可以初始化内存并通过 First-Fit 自检；B1 Best-Fit 回调已完成，固定测试未修改。

Post-Condition：`init_pmm_manager()` 注册 Best-Fit，内核初始化经由该管理器完成，固定自检与自动评分通过。

Case 1：前置条件缺失。报告缺少的证据并停止切换，不修改其他成员负责的实现，也不跳过基线。

Case 2：前置条件满足。只修改管理器注册指针，执行构建、QEMU 与 `make grade`。若 QEMU 始终运行，使用有界超时安全结束并区分预期停止与启动失败。

Requirements：确认固定 `best_fit_check()` 实际成功；自动评分必须通过 Page Init 和 Best-Fit 两项。不能只以管理器指针注册检查代替算法自检。

**检查与处理记录**

- 前置检查发现 `page_init()` 已由 A 实现，default 基线和 B1 固定测试均具备。default 基线构建、QEMU 初始化自检通过。
- 仅修改 `pmm.c` 的 `init_pmm_manager()` 注册指针。Best-Fit 构建退出 0；QEMU 输出 `memory management: best_fit_pmm_manager` 和 `check_alloc_page() succeeded!`，表明固定 `best_fit_check()` 返回成功。QEMU 以 124 结束是有界超时停止内核空闲循环。
- `make grade` 退出 0，`Total Score: 20/20`；评分中 Page Init 和 Best-Fit 均为 `OK`。另在临时副本串口运行中观察到 `[Test 1] Page Init: PASS` 和 `[Test 2] Best-Fit: PASS`。该串口运行的完整 shell 命令和临时路径未留存。
- 本阶段没有发现并修复 Best-Fit 生产代码 Bug。
