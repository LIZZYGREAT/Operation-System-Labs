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

> 本文件记录 Lab2 实际使用的 AI 提示词及协作过程。后续每次用户发来新消息，在完成该轮工作后，追加日期、用户原始提示和处理摘要；用于代码生成的 Prompt 则另设小节，保留原文。

### 2026-10-20：项目环境、要求与 B 线分工核对

**用户原始提示**

> 请你先查看当前的项目的环境和配置，查看docs文档下的项目要求，以及我的任务B分工。尤其需要查看requirements.md了解我们的要求，每次我跟你的对话需要你进行记录到prompt.md。

**检查与处理记录**

- 阅读 `docs/lab2/requirements.md`、`docs/lab2/Lab2 三人分工指南.md`、`docs/lab2/Lab2 实验实施方案与 Workflow.md`、`docs/environment_report.md`，并查看仓库 README、Makefile 和 PMM 相关接口/实现。
- 仓库位于 `Operation-System-Labs/`，当前分支为 `lab2`，HEAD 为 `103dfca init lab2`，检查时工作区干净。仓库 README 规定报告目录为 `report/`，提示词记录文件为 `report/prompt.md`；本文件按此约定新建。
- 当前环境实测：GNU Make 4.3；RISC-V GCC 10.2.0；RISC-V GDB 10.1；QEMU 4.1.1。项目 Makefile 提供 `make debug`（QEMU `-s -S`）和 `make gdb`（客体 RISC-V GDB 连接 `localhost:1234`）。尚未确认 QEMU 是否为带源码调试信息的构建。
- 代码现状：`code/kern/mm/best_fit_pmm.c` 保留 `//lab2 function2-2 YOUR CODE` 占位及固定测试；`code/kern/mm/pmm.c` 仍注册 `default_pmm_manager`，`page_init()` 仍是 `//lab2 function1 YOUR CODE` 占位。测试框架要求固定的 Best-Fit 检查不得修改。
- B 线交付包括：B1 实现 Best-Fit 管理器且不改固定测试；B2 接入 Best-Fit，并完成编译、QEMU、grade 和额外分配器行为检查；B3 准备可调试 QEMU 与双 GDB 环境；B4 记录真实 VA→PTE→PA 转换及 TLB/页表路径；B5 复核 A 线 `page_init()` 的地址、对齐、保留区和空闲区边界。B2 依赖 A2 先用 First-Fit 验证初始化。
- `requirements.md` 还要求实验报告记录整体实现逻辑、核心模块、各练习的 spec/prompt、AI 生成代码中的 bug 与修复、OS 原理对应关系；必须回答 First-Fit 与 Best-Fit 的改进空间问题，并提交 Buddy 的两份 Prompt。SLUB 属于 Challenge。提交时需保留 `lab2` 注释，并按课程要求处理 `YOUR CODE` 学号标记。
- 本轮只检查和记录，没有修改实验代码，也没有运行构建或测试。

### 2026-10-20：梳理 First-Fit 与 Best-Fit 实现要求

**用户原始提示**

> 你先进行阅读First-Fit部分的实现逻辑，函数逻辑，重要变量等，并为我梳理，我们稍后要进行的Best-Fit要求我们实现哪些内容。

**检查与处理记录**

- 阅读 `default_pmm.c`、`memlayout.h`、`pmm.h`、`list.h`、`best_fit_pmm.c`，并对照 `requirements.md` 与 B 线分工指南。
- 梳理 First-Fit 的空闲块链表、`property`/`PageProperty`、`nr_free` 状态约定，以及初始化登记、首次适配分配、拆分、释放、地址排序和前后相邻块合并过程。
- 明确 Best-Fit 需要在 `best_fit_pmm.c` 补齐 `pmm_manager` 全部函数、按最小可容纳块分配并正确拆分/合并，保留 `free_list`、`nr_free` 供固定自检使用且不得修改测试；随后在 `pmm.c` 将管理器切换为 `best_fit_pmm_manager`。
- 本轮只阅读和梳理，没有修改实验代码，也没有运行构建或测试。

### 2026-10-20：按执行顺序阅读 PMM 与 Best-Fit 固定测试

**用户原始提示**

> ok，接下来我们正式开始实验部分的准备。任务是Best-Fit 连续物理页分配器任务。请先不要编写、修改或提交任何代码，也不要直接给我实现方案。
>
> 请阅读当前仓库的以下文件：
> - kern/mm/pmm.c、kern/mm/pmm.h
> - kern/mm/default_pmm.c、kern/mm/default_pmm.h
> - kern/mm/best_fit_pmm.c、kern/mm/best_fit_pmm.h
> - kern/mm/memlayout.h
> - libs/list.h、libs/defs.h
> - tools/test_lab2.c（只用于了解后续测试）
>
> 请以初次接触本实验、但已了解虚拟内存与 C 语言基础的学生为读者，从实际执行顺序解释：
> 1. 内核为什么需要独立的物理页分配器；page_init() 与具体分配算法分别负责什么。
> 2. pmm_init() → init_pmm_manager() → page_init() → init_memmap() → check_alloc_page() 之间的调用关系；当前哪一步属于未实现的其他成员任务。
> 3. struct Page、free_area_t、free_list、nr_free、property、page_link 分别保存什么状态。
> 4. 从 init_memmap(base,n) 登记连续空闲页，到 alloc_pages(n) 分配、free_pages(base,n) 回收，Page 描述符和链表如何变化。
> 5. default_pmm.c 中 First-Fit 的选择策略是什么，Best-Fit 应在哪个逻辑环节与其不同。
> 6. 固定测试 basic_check()/best_fit_check() 对内部数据结构有哪些明确约束。
>
> 请引用实际函数名和文件路径，不要泛泛罗列概念。请最后列出你发现的已有功能、待实现部分及必须保持不变的代码。遇到源代码与指导书或我的描述冲突时，优先指出冲突，不要擅自修订代码。

**检查与处理记录**

- 按用户指定清单逐项阅读 `pmm.c`、`pmm.h`、First-Fit/Best-Fit 文件、`memlayout.h`、`list.h`、`defs.h` 和 `tools/test_lab2.c`，并搜索 PMM 源码中的 `page_init` 与管理器注册符号。
- 梳理了实际调用路径、默认分配器的初始化/页块登记/First-Fit 分配/释放合并，以及测试对链表、页标志和空闲页总数的约束；说明了 Best-Fit 相对于 First-Fit 只在候选空闲块选择策略上不同。
- 记录源码现状与注释差异：`pmm.c` 调用了 `page_init()`，但所查代码中没有它的定义；`pmm.c` 注释称 first-fit/best-fit/worst-fit/buddy 都可用，但当前内核代码只定义了 `default_pmm_manager`，Best-Fit 仅有声明和测试框架，未见 worst-fit 或 buddy 注册实现。`tools/test_lab2.c` 的 Best-Fit 外部测试只检查管理器指针，具体算法行为由 `best_fit_check()` 覆盖。
- 本轮只阅读、搜索并整理，没有修改实验代码，没有提交，也没有运行构建或测试。

## 实现用 Prompt

（后续实际使用 AI 生成或修改代码时，在此按任务记录 Spec、Prompt 原文、输出问题和修正过程。）

### 2026-10-20：审查 Best-Fit 实现并独立验证

**用户原始提示**

> 请审查上一阶段生成的 Best-Fit 实现，并在当前环境允许的范围内验证。先不要改其他模块，也不要进行 B2 切换。
>
> 审查重点：确认改动范围与固定测试未变；核对 pmm_manager 回调签名、注册结构和 best_fit_check() 引用；覆盖 Best-Fit 选择、排序、拆分、合并、失败、计数与标志位；核验固定测试隔离并恢复 free_list/nr_free 的兼容性。若发现明确错误，只修复 best_fit_pmm.c。
>
> 先说明检查方法，再给实际命令、结果和失败原因。page_init() 缺失时说明 A 任务阻塞，完成可独立进行的 B1 检查；不得跳过或修改测试，也不得添加占位 page_init()。最后列出已验证、未验证、修复 Bug 与交付状态，不提交。

**检查与处理记录**

- 对照 `git diff`，唯一跟踪文件改动为 `code/kern/mm/best_fit_pmm.c`；固定测试部分与 HEAD 的后缀逐字一致，`git diff --check` 通过。`report/` 是已有未跟踪记录目录，本条记录按此前要求补入。
- 对照 `pmm.h` 的函数指针签名和 `best_fit_pmm.h` 的 extern 声明；Best-Fit 各回调均为匹配的静态函数，管理器对象含 `.check = best_fit_check`，前置声明允许对象在测试函数定义前引用原始检查。RISC-V 目标语法检查通过，`nm` 确认管理器和所有回调/检查函数均在对象文件中。
- 以临时独立 harness 编译实际 `best_fit_pmm.c` 并调用原有 `best_fit_pmm_manager.check()`；固定检查及最优块选择/同尺寸低地址优先、精确匹配、地址排序、拆分、左右/双侧合并、碎片化失败、超总量失败、空链表但计数未清零、重复分配释放恢复均通过。harness 首次运行的手写总页数期望值有误，修正测试期望后重编重跑通过，生产代码没有因此修改。
- `make -j2` 因 `kern/mm/pmm.c:77` 调用未声明的 `page_init()` 且 `-Werror` 而失败；这是 A 任务尚未完成导致的完整构建阻塞。本轮未修改 `pmm.c`、未切换 B2、未修复 Best-Fit 代码。随后 `make clean` 清除了本轮生成的 `code/obj`。
- 当前可独立交付 B1 源码审查结果；尚未完成依赖 `page_init()` 的全项目链接、QEMU 启动及集成验收。