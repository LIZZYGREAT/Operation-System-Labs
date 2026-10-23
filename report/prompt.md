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

#### 提示一：核对项目环境、课程要求与 B 线分工

```
[PROMPT]

任务：
检查当前 ucore Lab2 项目的环境、配置、课程要求和 B 线分工，重点阅读 requirements.md。

操作要求：
以当前仓库中的实际文件和代码为准，先阅读检查，不修改源代码，不提交代码，不运行未要求的测试。区分本人负责的 B 线工作与其他成员负责的工作。

输出要求：
说明项目环境和构建入口、requirements.md 中与本实验相关的要求、B 线任务及其依赖，并记录检查时实际看到的实现状态。对没有证据的内容明确标注。

[RELY]

- docs/lab2/requirements.md。
- docs/lab2 下的三人分工指南和实验实施方案。
- README、Makefile、PMM 接口和分配器实现。

[GUARANTEE]

- 只进行阅读和整理，不修改源代码、测试或构建配置。
- 不把 A 线的 page_init() 工作归入 B 线实现。

[SPECIFICATION]

检查实验工具链和 Makefile 提供的构建、运行、调试入口；梳理 B1、B2 及 B 线其余交付项、依赖和验收要求；记录 Best-Fit 固定测试限制、实验报告要求和代码当前状态。结论以实际文件为依据，不推测未检查的信息。
```

#### 提示二：梳理 First-Fit 实现并准备 Best-Fit 需求

```
[PROMPT]

任务：
阅读 First-Fit 的实现逻辑、函数职责和重要变量，并据此梳理后续 Best-Fit 连续物理页分配器需要实现的内容。

操作要求：
以当前源码和课程要求为准，只做阅读分析，不修改代码，不直接编写实现。引用实际函数名、数据结构和文件路径。

输出要求：
按执行顺序解释 First-Fit 的初始化、分配、拆分、释放和相邻块合并，说明 Best-Fit 与 First-Fit 的差异及后续实现清单。

[RELY]

- kern/mm/default_pmm.c、kern/mm/default_pmm.h。
- kern/mm/best_fit_pmm.c、kern/mm/best_fit_pmm.h。
- kern/mm/pmm.h、kern/mm/memlayout.h、libs/list.h。
- Lab2 requirements 和 B 线分工说明。

[GUARANTEE]

- 不实现或修改任何代码。
- 说明以真实源码为准；源码与文档不一致时指出差异。

[SPECIFICATION]

解释 free_list、nr_free、PageProperty、property 和 page_link 的状态约定；跟踪 init_memmap()、First-Fit alloc_pages()、free_pages() 的主要状态变化；指出 Best-Fit 应改变候选块的选择步骤，以及接口、拆分、释放合并和固定测试兼容要求。
```

#### 提示三：按执行顺序阅读 PMM 与固定测试

```
[PROMPT]

任务：
阅读当前仓库的 PMM、First-Fit、Best-Fit、页面布局、链表代码和 Lab2 测试，以初次接触本实验、但已了解虚拟内存与 C 语言基础的学生为读者，按实际执行顺序解释物理页分配器。

操作要求：
只阅读以下源码，不编写、修改或提交代码，也不直接给实现方案：
- kern/mm/pmm.c、kern/mm/pmm.h
- kern/mm/default_pmm.c、kern/mm/default_pmm.h
- kern/mm/best_fit_pmm.c、kern/mm/best_fit_pmm.h
- kern/mm/memlayout.h
- libs/list.h、libs/defs.h
- tools/test_lab2.c（仅了解后续测试）

引用实际函数名和文件路径。遇到源码与指导书或任务描述冲突时，先指出冲突，不擅自修订代码。

输出要求：
解释内核使用独立物理页分配器的原因、page_init() 与具体算法的职责、PMM 调用链、关键数据结构、页面登记/分配/释放时的状态变化、First-Fit 与 Best-Fit 的区别和固定测试约束；最后列出已有功能、待实现项和必须保持不变的代码。

[RELY]

- 本任务列出的 PMM、分配器、页面布局、链表和测试源码。
- Lab2 requirements 与 B 线分工说明。

[GUARANTEE]

- 只阅读源码和测试，不修改任何文件。
- 明确标出由其他成员负责且尚未完成的步骤，不越界补写。

[SPECIFICATION]

1. 梳理 pmm_init() → init_pmm_manager() → page_init() → init_memmap() → check_alloc_page() 的调用关系。
2. 解释 struct Page、free_area_t、free_list、nr_free、property 和 page_link 的作用。
3. 跟踪连续空闲页从登记、分配到释放的描述符和链表变化。
4. 对照 default_pmm.c 说明 First-Fit 选择规则及 Best-Fit 的差异点。
5. 说明 basic_check()、best_fit_check() 对空闲链表、页面标志和计数的约束。
```

#### 提示四：形成 Best-Fit 需求规格与接口设计

```
[PROMPT]

任务：
只进行 Best-Fit 模块的需求分析与接口设计，形成可供后续实现与验收的 Best-Fit Spec，暂不修改文件。

任务边界：
- 唯一目标模块是 kern/mm/best_fit_pmm.c 中待完成的 Best-Fit 连续物理页分配器。
- 遵守 pmm.h 的 struct pmm_manager 接口，复用 Page 描述符和现有双向链表。
- 不改 basic_check()、best_fit_check()、default_pmm.c、pmm.h、memlayout.h、list.h。
- 暂不修改 pmm.c 的分配器注册，不处理 A 负责的 page_init()。

输出要求：
给出接口职责、状态不变量、接口前置/成功/失败状态、Best-Fit 选择规则、拆分与释放合并规则、固定测试约束和验收测试矩阵。

[RELY]

- 上一阶段阅读的 PMM 接口、First-Fit、Best-Fit 固定测试和链表实现。
- kern/mm/pmm.h、kern/mm/default_pmm.c、kern/mm/best_fit_pmm.c。
- kern/mm/memlayout.h、libs/list.h、libs/defs.h。

[GUARANTEE]

- 本阶段只产出规格，不修改任何文件。
- 不改变其他成员任务或测试约束。

[SPECIFICATION]

1. 说明 init、init_memmap、alloc_pages、free_pages、nr_free_pages、check 及 best_fit_pmm_manager 的职责和注册关系。
2. 规定空闲块起点标志和 property、按物理地址排序的 free_list、nr_free 总量及已分配页不在空闲链表中的不变量。
3. 明确各接口的前置条件、成功效果、失败效果和失败时保持不变的状态。
4. Best-Fit 从所有大小不小于 n 的空闲块中选择最小者；同尺寸时选择低地址块；无候选时返回 NULL 且不改状态。
5. 规定分配拆分、按地址登记释放页、左右两侧相邻块合并及计数和标志维护。
6. 覆盖精确匹配、多候选选最小块、超总量失败、总量足够但无连续块、拆分、左/右/双侧合并、反复分配释放后恢复和空链表失败。
7. 说明固定测试临时调整 free_list 与 nr_free、之后恢复原状态时实现必须满足的兼容要求。
```

#### 提示五：实现完整 Best-Fit 分配器

```
[PROMPT]

任务：
在当前 ucore Lab2 仓库内实现完整的 Best-Fit 连续物理页分配器，使其能够作为 pmm_manager 的独立实现接入内核，而不是只补充一个查找函数。

允许修改：
- 仅 kern/mm/best_fit_pmm.c 中 //lab2 function2-2 YOUR CODE 对应的实现区域。
- 必要的前置声明也放在该区域，保证与下方已有测试函数兼容。

禁止修改：
- best_fit_pmm.c 中已有 basic_check() 与 best_fit_check() 的任何断言、语句或签名。
- default_pmm.c、pmm.h、best_fit_pmm.h、memlayout.h、libs/list.h 等框架和接口。
- kern/mm/pmm.c，包括 A 负责的 page_init() 和 B2 才会修改的 init_pmm_manager()。
- Makefile、评分脚本或测试注入脚本；不得删除、跳过或放宽测试。

操作要求：
复用现有 C 数据结构，不引入不必要的抽象层、兼容分支、其他分配算法或动态内存依赖。保留 lab2 标记；YOUR CODE 标记处使用学号 2412700，不编造其他学号。

[RELY]

- kern/mm/pmm.h：struct pmm_manager 及 alloc_pages/free_pages 调用约定。
- kern/mm/memlayout.h：struct Page、free_area_t、页面标志和相关宏。
- libs/list.h、libs/defs.h：双向链表操作和 le2page。
- kern/mm/default_pmm.c：First-Fit 初始化、地址排序、拆分、释放合并和计数更新参考。
- kern/mm/best_fit_pmm.c：basic_check()、best_fit_check() 的固定行为约束。

[GUARANTEE]

实现并提供可注册的 best_fit_init()、best_fit_init_memmap()、best_fit_alloc_pages()、best_fit_free_pages()、best_fit_nr_free_pages()、best_fit_check() 调用关系及 const struct pmm_manager best_fit_pmm_manager。必须使原有固定测试能访问 free_list 与 nr_free。

[SPECIFICATION]

- init 建立空的双向链表并将 nr_free 置零。
- init_memmap(base,n) 初始化合法连续区间的 Page 描述符和引用计数，将区间登记为一个地址有序空闲块并更新总量。
- alloc_pages(n) 遍历空闲块，选择大小不小于 n 且最小的块；同尺寸选低地址块。失败返回 NULL，保持链表、标志和计数不变；成功返回首个 Page，必要时正确登记余块。
- 成功分配须正确更新 PageProperty、property、链表节点和 nr_free；已分配页不得留在空闲链表。
- free_pages(base,n) 恢复页面状态，按物理地址插入空闲块，检查并合并相邻的左、右空闲块；避免重叠、重复节点或错误块首标志，nr_free 只增加释放页数。
- nr_free_pages 返回空闲页总数，正常状态下等于所有空闲块 property 的总和。
- 管理器 check 回调实际调用原有 best_fit_check()，不得以空检查替换。
- 兼容固定测试对 free_list 与 nr_free 的临时隔离/恢复、精确匹配、1 页和 2 页块选择、失败和合并断言。

完成后说明修改文件、各接口职责、不变量、实际构建/静态检查命令和结果、未验证或受阻项；不要自动提交 Git。
```

#### 提示六：只读审查并独立验证 B1 实现

```
[PROMPT]

任务：
审查已经完成的 Best-Fit B1 实现，并在当前环境允许的范围内验证；不要修改其他模块或进行 B2 切换。

审查重点：
1. 确认 Git diff 仅包含允许的 Best-Fit 实现，basic_check() 和 best_fit_check() 未修改。
2. 核对 pmm_manager 函数指针签名、注册结构、编译可见性、引用顺序及 best_fit_check() 回调。
3. 人工推演或独立检查精确匹配、最小合适块、地址排序、拆分、左右和双侧合并、空链表失败、总量足够但无连续块。
4. 检查 nr_free、property、PageProperty、已分配块链表状态和释放后空闲块不重叠。
5. 检查固定测试临时隔离并恢复 free_list 与 nr_free 的过程。
6. 若发现可定位错误，只修复 best_fit_pmm.c 的实现区域。

操作要求：
不得跳过或修改测试，不得添加占位 page_init()，不修改其他成员负责的代码。如果 page_init() 缺失导致完整构建失败，说明这是 A 任务的前置阻塞，并继续完成独立 B1 检查。

输出要求：
先说明检查方法，再给出实际执行命令、结果和失败原因；最后列出已验证项、未验证项、修复的具体 Bug 与原因及当前交付状态。不要自动提交。

[RELY]

- kern/mm/best_fit_pmm.c、kern/mm/best_fit_pmm.h。
- kern/mm/pmm.h、kern/mm/default_pmm.c、kern/mm/pmm.c。
- 固定测试、Git diff 和当前可用工具链。

[GUARANTEE]

- 固定测试和外部框架保持原样。
- 不切换 B2，不补写 A 的 page_init()。
- 只修复属于 B1 且能明确定位的问题。

[SPECIFICATION]

至少核对候选选择、排序、拆分、释放合并、失败路径、计数和页面标志、链表一致性及固定测试隔离恢复；区分独立检查结果与完整内核构建结果，不把未运行的检查记为通过。
```

#### 提示七：将 Best-Fit 接入内核并完成 B2 验收

```
[PROMPT]

任务：
将已实现的 Best-Fit 接入 Lab2 内核，并完成 B2 验收。

前置条件：
- 确认 kern/mm/pmm.c 中 page_init() 已由 A 完成，不再是空缺的 YOUR CODE 占位。
- 使用 default_pmm_manager 能完成物理内存初始化并通过原有 First-Fit 自检。
- B1 best_fit_pmm.c 接口已实现且固定测试未修改。
- 若任一条件不满足，报告缺少的证据并停止接入，不补写 A 的代码或跳过基线。

操作要求：
前置条件满足时，只在 kern/mm/pmm.c 的 init_pmm_manager() 中将 pmm_manager 从 default_pmm_manager 切换成 best_fit_pmm_manager；遵守现有头文件、函数指针和打印逻辑，不修改 page_init()。

验证要求：
检查 pmm_init() 调用链 best_fit_init() → page_init() → best_fit_init_memmap() → best_fit_check()，以及 alloc_pages/free_pages 的动态分派。使用仓库 Makefile，依次执行 make、make qemu、make grade；工具链可用时安全结束持续运行的 QEMU，并记录真实输出和退出码。固定 best_fit_check() 必须实际运行成功；make grade 输出应有 [Test 1] Page Init: PASS 和 [Test 2] Best-Fit: PASS。不得以管理器注册指针检查代替算法自检。

失败处理：
区分构建问题、A 的页初始化问题、B 的分配算法问题和启动/环境问题；只修复属于 B 且已明确定位的错误。跨成员问题先反馈，不擅自改写。

输出要求：
列出变更 diff 摘要、完整测试命令、关键控制台输出、成功/失败结论及是否达到 B2 验收标准。不要修改测试脚本，不要自动提交 Git。

[RELY]

- pmm.c、pmm.h、Best-Fit B1 实现和固定测试。
- Makefile、QEMU 和自动评分脚本。
- A 对 page_init() 的实现及 First-Fit 基线结果。

[GUARANTEE]

- 只改 init_pmm_manager() 中的管理器选择。
- 不修改 page_init()、分配算法、测试或评分脚本，除非属于 B 且错误定位明确。

[SPECIFICATION]

只有满足全部前置条件后才切换并运行完整验收。每条命令和结果均须如实记录；QEMU 的预期超时停止应与启动失败区分。若评分没有明确显示两个指定 PASS 项，不声称达到验收标准。
```

#### 提示八：整理 B1/B2 实验报告的可核查材料

```
[PROMPT]

任务：
只读核查已完成的 Best-Fit B1/B2，并整理可用于 Lab2 实验报告的真实材料；不修改源码，不提交 Git。

输出内容：
1. 需求目标、输入输出、PMM 调用链和模块边界。
2. 实现接口及初始化、空闲块登记、Best-Fit 选择、拆分、释放合并、查询和自检职责。
3. First-Fit 与 Best-Fit 的共同管理结构、选择策略差异及具体空闲块分配示例。
4. 可由代码或测试证明的 Spec 行为。
5. 实际 Prompt 使用顺序、实现版本、测试命令和结果；若有 Bug，按“现象 → 定位依据 → 修改内容 → 复测结果”记录；不得杜撰未发生的 Bug。
6. 回答 Best-Fit 时间开销、碎片化和空闲块组织方面的改进空间，并区分理论可能性与本实验实现。
7. 列出未独立验证的边界情况及原因，不将未运行测试写成通过。

[RELY]

- 当前源码、固定测试和 Git diff/log。
- 前序 Best-Fit Spec、Prompt 和实际测试记录。
- 仓库 Makefile 及已有实验报告材料。

[GUARANTEE]

- 只读检查，不修改源码，不提交。
- 只记录有实际代码、命令、输出或历史记录支持的事实。
- 对证据缺失处明确标注，供人工复核。

[SPECIFICATION]

整理为简洁、可复核的 Markdown 报告素材；测试结果必须与实际命令记录一致，区分固定自检、独立验证、自动评分和未验证边界。
```

#### 提示九：将核实后的实验材料整理到 report.md

```
[PROMPT]

任务：
将上一阶段核查得到的 Best-Fit 实验材料记录到 report.md，保持报告现有格式一致、协调。

操作要求：
依据仓库中现有报告结构补入内容，不伪造测试结果、Bug 或实现细节；此前已确认的报告材料可写入对应章节。

输出要求：
完成后说明修改的报告章节和写入内容，并确保 Markdown 结构与相邻章节协调。

[RELY]

- 上一阶段整理的、由源码和实际测试记录支持的 Best-Fit B1/B2 材料。
- 当前 report.md 的标题层级、图表和章节格式。

[GUARANTEE]

- 只编辑 report.md，不改源码、固定测试或其他成员文档。
- 不记录与实验无关的分支、提交或推送对话。

[SPECIFICATION]

将需求、实现职责、First-Fit/Best-Fit 对比、测试结果、改进空间和未验证边界放入合适章节；有证据缺口的内容保留明确说明，不把未运行项目写成通过。
```

#### 提示十：审查 B3 双重 GDB 调试环境

```
[PROMPT]

任务：
完成 Lab2 B3 的环境审查和调试方案确认，先不要编译新的 QEMU，也不要修改项目文件。目标是弄清本机现有工具链、QEMU 版本及源码位置，并说明怎样分别调试 QEMU 宿主进程和运行在其中的 RISC-V ucore。

操作要求：
以当前工作目录中的实际仓库与工具为准，从 code/ 目录读取 Makefile 的 debug、gdb、qemu 目标；检查 QEMU 安装路径、版本、可用源代码及 GNU GDB、RISC-V GDB。检查 git status、分支和当前可用的调试权限。优先使用项目已有工具，不能直接假定指导书提到的 QEMU 4.1.1 就是当前运行的版本。

输出要求：
给出简洁的环境检查表、实际执行命令及结果、三个终端的角色、需要的最少构建/调试动作与明确的阻塞项；最终向我说明是否可以开始 B3-2。不得在这一阶段改代码。

[RELY]

- README.md 中的分支协作规范。
- code/Makefile 的 make debug、make gdb、make qemu，以及 QEMU 变量覆盖方式。
- code/tools/gdbinit、code/bin/kernel（如果当前构建后存在）。
- 本机 command -v、--version、readelf、pgrep、gdb、riscv64-unknown-elf-gdb 等工具。
- 实验指导书中的双 GDB 任务要求，只作为任务参考，不代替实际版本检查。

[GUARANTEE]

- 交付一份真实环境表：宿主架构、QEMU 二进制路径与版本、QEMU 源码路径/版本、宿主 GDB、RISC-V GDB、工具链和必要权限。
- 交付一份明确的“QEMU 进程 / QEMU GDB stub / ucore 客体 CPU”三者关系说明。
- 不更改 Makefile、内核源码、系统 QEMU 安装及系统 ptrace 安全配置。

[SPECIFICATION]

Pre-Condition：本地仓库可以读取，B1/B2 已有集成代码，但不预设所有调试工具已经安装。

Post-Condition：确定下一阶段要使用的 QEMU 源码和可执行文件，识别需要新增或重新编译的组件；记录实际版本是否与指导书一致。

Case 1：现有 QEMU 源码与带符号可执行文件版本匹配，则复用现有构建，避免重复安装。

Case 2：只有系统 QEMU 或源码/可执行文件不匹配，则说明需要单独编译的范围和路径，不直接替换系统程序。

Case 3：GDB、工具链或 ptrace 权限不足，报告具体错误、最小修复方案及其影响，不静默变更安全设置。

Requirements：不得把 make gdb 当成宿主机 GDB；不得因为终端里只有某个程序名就推测其架构或版本。最后给出 B3-2 的前置条件是否满足。
```

#### 提示十一：完成 B3 双 GDB 最小联调

```
[PROMPT]

任务：
完成 B3 的实际环境准备与最小联调。根据 B3-1 确认的 QEMU 版本和源码路径，生成或复用可让宿主机 GDB 设置源码断点的 QEMU 可执行文件，并用当前 Lab2 内核完成一次双 GDB 连通性验证。

操作要求：
先确认源码与目标 QEMU 版本匹配，检查已有 build 目录和构建系统，再采用对应版本支持的调试编译参数构建。不要照搬不匹配版本的 configure/meson/ninja 命令，不在未确认的目录执行破坏性清理命令，也不要 sudo make install 替换系统 QEMU。

在 code/ 目录使用 QEMU 变量覆盖方式调用 make debug，而不是永久改写 Makefile。建立三个终端：终端1运行 QEMU；终端2用宿主机 GDB 调试 QEMU 进程本身；终端3通过 make gdb 连接 QEMU 的 GDB stub，观察 RISC-V 内核寄存器和指令。

若宿主 GDB attach 被 ptrace 策略阻止，优先采用在宿主 GDB 下直接启动调试版 QEMU 的等效方案，说明与三终端模式的差别；修改系统安全参数前必须征询我。

[RELY]

- B3-1 环境审查结果（版本、路径、权限）。
- code/Makefile 的 -s -S、QEMU 变量及 make gdb 配置。
- 与 QEMU 可执行文件严格对应的源代码及调试符号。
- code/bin/kernel 的调试符号和实际构建产物。

[GUARANTEE]

- 交付能重复执行的 QEMU 调试版构建命令及其准确产物路径。
- 交付三个终端各自的启动、设置断点、continue/单步、退出命令与实际验证结果。
- 至少确认：宿主 GDB 能解析 QEMU 的一个真实源码符号或停在其函数入口；客体 GDB 能连接 QEMU 并读取 $pc、$satp 与反汇编指令。
- 不更改内核算法、测试与 Makefile；不替换系统 QEMU；不自动提交。

[SPECIFICATION]

Pre-Condition：B3-1 明确了匹配的版本与源代码位置，内核可构建；若不满足，则只给出阻塞信息。

Post-Condition：获得一套能从头复现的双 GDB 调试流程，包含启动时序、端口、符号文件、实际 PID 或直接启动方法，以及截图/日志保存位置。

Case 1：宿主 GDB attach 成功，分别验证 QEMU 进程上的源码断点与 ucore 的客体指令断点。

Case 2：无法 attach，但可在宿主 GDB 内运行 QEMU，改用直接启动方式验证；不得申请不必要的系统级权限。

Case 3：QEMU 调试构建失败，记录第一处真实错误、依赖及版本，停止后续步骤；不要用无符号的二进制冒充可源码调试版本。

Case 4：宿主 GDB 暂停 QEMU 时客体 GDB 暂时无响应，应解释这一执行依赖并安排 continue/step 的合理顺序，不误判为连接失败。

Requirements：逐条列出实际命令、退出码和关键输出。此阶段只验证双层调试可用，不要求完成 Sv39 逐级页表分析；完成后明确 B3 是否能够验收。
```

#### 提示十二：分析 QEMU RISC-V Sv39 访存与 TLB 路径

```
[PROMPT]

任务：
进入 B4，但先不要急于打随机断点。使用 B3 已经确认的 QEMU 源码版本，找出 RISC-V 访存、地址转换缓存、TLB 未命中处理、Sv39 页表遍历和权限检查涉及的真实函数，画出一条由指令访存到物理地址的调用关系。只阅读源码，不修改项目。

操作要求：
不要凭记忆硬写 QEMU 函数名；必须通过源码搜索和调用方确认。区分 QEMU 自身的 TCG/软件 TLB 加速路径，与 RISC-V 客体架构的 Sv39 硬件页表翻译逻辑；明确哪些路径会在 TLB 命中时绕过一般的页表遍历函数。

输出要求：
给出对应版本的源码路径、函数名、调用关系、关键分支条件及阅读定位信息；在每个关键节点说明输入是什么（VA、访问类型、特权级、satp 等）、输出是什么，以及下一步访问什么。给出下一阶段最适合观察的断点候选位置，不要直接宣称已经跟踪成功。

[RELY]

- B3 已确认的 QEMU 版本和完整源码。
- code/kern/init/entry.S 的 satp 切换与启动页表。
- code/kern/mm/mmu.h、code/kern/mm/pmm.c；现有 Lab2 内核可用于验证。
- GDB 的 info functions、break、bt、list、info locals 和源码搜索能力。

[GUARANTEE]

- 输出一个基于真实函数的调用路径，不使用无依据的“翻译函数”占位名代替结论。
- 区分页表遍历与缓存命中路径，注明需动态调试才可确认的分支。
- 不修改 QEMU、ucore、Makefile、测试或报告正文。

[SPECIFICATION]

Pre-Condition：B3 双 GDB 可以使用，宿主 GDB 能读取匹配源码符号。

Post-Condition：得到下一阶段可执行的断点方案和观察字段清单，包括如何识别一次地址转换成功、页错误或无页表遍历。

Case 1：源码有独立 RISC-V 地址转换函数，解释它如何根据 satp、VPN、PTE、权限计算 PA。

Case 2：TLB 命中不经过该函数，明确应在其他路径观察，避免用“断点没触发”证明没有 TLB。

Case 3：实际 QEMU 版本与指导书示例不同，依据当前源码重建路径，不强行套用 QEMU 4.1.1 的文件结构。

Requirements：输出代码依据，区分已静态确认和需要运行确认的结论；禁止新增冗余日志、永久断点或模拟器补丁。
```

#### 提示十三：独立复核 page_init() 的地址计算与初始化不变量

```text
[PROMPT]

任务：
执行 B5，对 A 已实现的 page_init() 做独立代码复核。目标不是重新开发 A 的功能，而是确认 DTB 提供的物理内存范围如何一步步变为 Page 描述符数组及最终的空闲物理页区间，是否满足真实内存布局与实验固定测试要求。

操作要求：
只读检查 code/kern/mm/pmm.c 中的 page_init()、kern/init/init.c 的调用顺序、kern/driver/dtb.c 的内存范围提取、kern/mm/memlayout.h、pmm.h 的宏和转换函数、tools/test_lab2.c 的测试断言。结合当前内核高虚拟地址映射，分别列出 VA、PA、PPN 与 Page*，不要混淆 PADDR 与 pa2page。

输出要求：
按“DTB 输入 → nbase/npage → 内核 end 物理地址 → Page 数组起止位置 → 空闲区起点/终点 → init_memmap 注册”完整推导变量和地址范围。列出每条检查所依据的代码、计算关系、预期条件和审查结论；对实际问题写清复现条件与影响，不擅自修改 A 的文件。

[RELY]

- code/kern/mm/pmm.c：page_init() 和 pmm_init()。
- code/kern/driver/dtb.c、dtb.h；code/kern/init/init.c。
- code/kern/mm/memlayout.h、pmm.h、default_pmm.c、best_fit_pmm.c。
- code/tools/test_lab2.c 的 Page Init 断言。
- kernel.ld 的 end 符号与固定 128 MiB DRAM 地址上界。

[GUARANTEE]

- 交付一份具体地址/状态检查表和代码定位依据。
- 检查描述符数组本身不能被分配、内核镜像/启动页表/栈所占页面均保留、只有完整空闲页进入分配器。
- 明确当前实现的运行假设和已测配置，不用仅通过 grade 推断任意 DTB/RAM 布局都正确。
- 只审阅，发现问题由 A 负责人处理；不修改 page_init() 或已完成的 Best-Fit。

[SPECIFICATION]

Pre-Condition：A1/A2 已有基线实现，B2 可以使用 Best-Fit 正常启动的历史记录。

Post-Condition：能够解释 page_init() 如何计算每个地址和计数，及每个 Page 描述符最终属于保留区还是空闲区，并输出通过/存疑/需修复项。

Case 1：默认 DRAM [0x80000000,0x88000000)，确认 nbase、npage、描述符容量、页对齐和空闲页总量的关系。
Case 2：DTB 内存范围无效或不包含内核，核对其拒绝初始化的行为；这些情况如未实际测试，应只标注静态检查结果。
Case 3：报告内存上限与 PHYSICAL_MEMORY_END 不同，检查 managed_end 截断及其与 test_lab2.c 期望 npage 计算的关系，区分设计限定与实际 Bug。
Case 4：内核 end、描述符数组或对齐计算靠近内存末端，检查容量乘法、空闲区不越界、空闲区为空时是否跳过 init_memmap。

Requirements：特别核对 PHYSICAL_MEMORY_OFFSET、KERNEL_BEGIN_PADDR、PageReserved、SetPageProperty、page2pa/pa2page 与实际分配区间；避免要求 A 增加与课程无关的复杂兼容或过度防御。
```

#### 提示十四：复现页初始化验收并形成 B5 审核意见

```text
[PROMPT]

任务：
在 B5 的只读审查基础上，独立复现当前代码的页初始化验收，形成提交给 A 负责人的审核意见。重点证明是否有证据支持 page_init() 已正确排除内核与 Page 描述符数组，并能将其余完整页面交给分配器。

操作要求：
从当前仓库可正常构建的版本开始，检查 Git 状态和调试环境。在不修改 A 代码和固定测试的前提下，按实际可行流程进行 make、限时 make qemu、make grade。当前默认注册 Best-Fit，因此如需复核 First-Fit 历史基线，应使用已有 A2 日志、独立干净工作树或经同意的临时分支，不能直接破坏正式共享代码和已通过的 B2 接入。

复核 Page Init 固定测试涵盖的 offset、DTB、npage、pages 对齐/位置、nr_free、内核尾页保留、描述符数组占用页保留和 page2pa/pa2page 往返；对测试未覆盖的内容，结合代码静态检查，必要时提出最小的独立测试建议。

输出要求：
提供具体命令、退出码和关键日志、每条审核意见的代码位置及影响程度；将结论分为“通过/需 A 修复/证据不足”。不改 A 代码，不自动提交。若发现问题，给出可直接交给 A 的最小修复要求，但由 A 决定并实施。

[RELY]

- B5-1 的地址布局分析与问题清单。
- code/Makefile、tools/grade.sh、tools/test_lab2.c。
- report/report.md 的 A2/B2 历史记录及当前 Git 状态。
- README.md 的工作分支与独立复现规范。

[GUARANTEE]

- 交付能独立复核的 B5 结论，不以 Best-Fit 的注册检查代替页初始化测试。
- 不改动其他成员负责的源码、测试或评分逻辑；不能以清理构建目录为由删除他人的修改。
- 失败时保留证据，分清环境、启动、A 初始化、B 分配算法等问题来源。

[SPECIFICATION]

Pre-Condition：A1/B2 当前代码可获取；如本地工具缺失，先记录阻塞并停止实际运行。

Post-Condition：B5 审查有可复现依据；针对 A 的问题形成明确的代码位置、输入条件、预期状态、实际结果与建议。

Case 1：全部实际执行的检查通过，记为当前测试配置下 B5 审核通过，并说明尚未覆盖的特殊 RAM/DTB 情形。
Case 2：构建或运行失败，记录首个具体错误和归属，不替 A 或 B 隐式修改代码。
Case 3：仅静态发现潜在问题但没有失败用例，明确标注为“风险/待验证”，不写成已复现 Bug。

Requirements：与 A 交叉审核后再将 B5 正式标为完成；保留实际运行证据和反馈记录。
```


### 任务三：Buddy System（伙伴系统）

#### 第一份提示词：Buddy 实现与接入

##### [PROMPT]

任务：参考现有 ucore 框架，从零实现 Buddy System 物理页分配器并接入内核。

仓库：`A:/OS/exp/Operation-System-Labs`。以下代码路径相对于 `code/`。

- 新建 `kern/mm/buddy_pmm.c`：实现分配器、管理器注册和基础自检。
- 新建 `kern/mm/buddy_pmm.h`：声明管理器，说明分配与释放约定。
- 最小修改 `kern/mm/pmm.c` 和 `Makefile`：支持通过 `PMM=buddy` 选择 Buddy，并支持 `first_fit`、`best_fit`，保留当前默认选择。
- 新建仓库内 `report/buddy_usage.md`：记录设计、切换步骤和验证结果。

操作要求：先阅读实际源码，遵循仓库分支规范，在项目中修改真实文件；保留其他成员代码，不改 `page_init()`、公共结构体、已有分配器及固定测试。使用模块私有静态结构，不依赖动态内存分配。本次只考虑单 hart 串行调用。

保留 `lab2` 注释，新增贡献注释使用本人真实学号；学号未提供时向用户询问，不编造。不自动提交或推送。

输出要求：说明修改文件、核心设计和实际验证结果；未运行或被环境阻塞的项目如实注明。实际执行后将提示词和修正过程补入仓库 `report/prompt.md` 的 Buddy 小节。

##### [RELY]

先阅读 `kern/mm/pmm.h`、`pmm.c`、`memlayout.h`、`mmu.h`、`default_pmm.c`、`libs/list.h`、`libs/defs.h` 和构建规则，以真实定义为准。

可依赖 `struct Page`、`struct pmm_manager`、页面状态宏、链表操作、`page2ppn()`、`pa2page()`、`assert()`、`cprintf()` 等已有资源。

- `pages[0]` 对应物理页号 `nbase`，描述符覆盖 `[nbase, npage)`；`npage` 是页号上界，描述符数量为 `npage - nbase`。
- 调用顺序为 `init_pmm_manager()` → `page_init()` → `check_alloc_page()`；`page_init()` 会将可用页面通过 `init_memmap(base, n)` 交给管理器。
- 管理器的 `init()` 执行时 `pages`、`npage` 尚未建立，只能初始化自身状态；阶数组容量根据框架支持范围确定。

##### [GUARANTEE]

在 `buddy_pmm.c` 中实现并注册以下接口：

```c
static void buddy_init(void);
static void buddy_init_memmap(struct Page *base, size_t n);
static struct Page *buddy_alloc_pages(size_t n);
static void buddy_free_pages(struct Page *base, size_t n);
static size_t buddy_nr_free_pages(void);
static void buddy_check(void);

const struct pmm_manager buddy_pmm_manager;
```

在 `buddy_pmm.h` 中声明 `extern const struct pmm_manager buddy_pmm_manager;`。管理器名称为 `"buddy_pmm_manager"`，六个回调均指向对应实现。允许增加私有辅助函数。

##### [SPECIFICATION]

###### 统一约定

1. 每个阶维护一个空闲链表，阶 `k` 的块含 \(2^k\) 页，首页物理页号按块大小对齐；另维护总空闲页数。
2. 请求 `n` 页时向上取整到最小的二次幂，实际占用整个块；释放时传回原始请求 `n`。例如申请 3 页实际占用 4 页，`free_pages(p, 3)` 归还全部 4 页，计数按 4 页变化。不支持部分释放。
3. 伙伴按绝对物理页号计算：`q = page2ppn(p)`，伙伴页号为 `q ^ ((size_t)1 << k)`。仅同阶、合法范围内、确认为空闲块首页的真正伙伴可以合并；访问其描述符前先检查范围。
4. 空闲块首页设置 `PageProperty`，`property` 记录块页数；已分配首页清除此标记，`property` 记录原始请求长度，并用模块私有且未被占用的 `flags` 位标识已分配首页。内部页清除旧首页标记，`property` 为 0；保留位含义不变。
5. 一个节点只能属于一个链表；拆分和合并正确摘链、更新标记。所有已登记页既不丢失也不重复管理，保留页不进入空闲区，空闲计数等于各阶空闲块页数之和。
6. 零长度、越界、重复登记、重复释放、内部页释放和错误释放长度，通过断言或 `panic()` 报告，检查先于状态修改。地址、取整和移位运算防止越界与溢出。

###### buddy_init

Pre-Condition：启动时尚未登记页面。

Post-Condition：初始化各阶空闲链表，总空闲页数为 0；不访问尚未建立的描述符数组。

###### buddy_init_memmap

Pre-Condition：管理器及描述符数组已初始化，`n > 0`，区间合法且尚未登记，传入页均为保留状态。

Post-Condition：清除传入页的保留标记、初始化引用计数等元数据，将整个区间组织为符合对齐的二次幂空闲块，空闲页数增加 `n`。

Case：起点不满足大块对齐或长度不是二次幂时，分成多个合法块覆盖全部页面，不浪费页、不扩大区间。支持多次登记不重叠区间，合并合法伙伴，不能跨保留间隙。

###### buddy_alloc_pages

Pre-Condition：管理器已初始化，`n > 0`。

Post-Condition：成功返回实际分配块首页的 `Page *`，记录原始请求，空闲页数减少实际占用；失败返回 `NULL` 且状态不变。

Case 1：目标阶有空闲块，直接取出；没有时从最近的可用高阶逐级拆分，将剩余半块放回相应阶。

Case 2：总量不足、没有合适连续块，或请求超出支持范围、取整会溢出，返回 `NULL`。已分配块互不重叠，取整余量不能再次分配。

###### buddy_free_pages

Pre-Condition：`base` 是尚未释放的分配块首页，`n` 与原始请求长度相同。

Post-Condition：归还整个实际分配块，与同阶空闲伙伴逐级合并；每次选择低地址为新首页，清除旧标记。

Requirements：检查指针范围和元素对齐后再访问描述符。空闲页数只增加本次归还的实际页数，合并不重复增加；伙伴不满足条件或到达最大阶时停止。

###### buddy_nr_free_pages

Pre-Condition：管理器已初始化。

Post-Condition：返回所有阶的空闲页总数，不修改状态，不把块数量或分配块内部碎片计为空闲页。

###### buddy_check

Pre-Condition：页面登记已完成，由现有 `check_alloc_page()` 调用。

Post-Condition：用真实断言检查单页与非二次幂分配、实际计数、不重叠及释放后复用；回收全部自检资源，恢复原空闲页数和合法状态。资源不足时明确说明未执行用例。

Requirements：不能留空、借用其他分配器检查或只打印成功。完整独立测试后续另写。

###### 接入与验证

- 在 `init_pmm_manager()` 中通过编译宏选择管理器，保留原初始化流程；`Makefile` 将 `PMM` 参数传给编译器，拒绝非法选项，复用已有源码收集规则。
- 在实际实验环境中执行 `make clean`、`make PMM=buddy`、`make qemu PMM=buddy`，检查真实管理器名称及自检输出；切换后端须干净构建，演示后恢复原默认配置。
- 提供 Best-Fit 的切换与基础评分步骤，最终小组默认后端仍应为 Best-Fit。基础评分中的 Best-Fit 选择检查不能用于判定 Buddy 正确性；不改评分脚本、不新增 `kern/test`。
- 记录实际运行结果及限制，保留原有测试；基础自检通过后仍需独立测试与另一位成员复现。

#### 第二份提示词：Buddy 独立测试

##### [PROMPT]

任务：阅读现有 Buddy 实现，设计并实现独立测试函数，验证页面登记、分配、拆分、释放、合并和边界行为，并说明各用例为什么能够发现错误。

仓库：`A:/OS/exp/Operation-System-Labs`。以下代码路径相对于 `code/`。

- 新建 `kern/mm/buddy_test.c`、`buddy_test.h`：实现受控测试环境、各项测试及统一入口。
- 最小修改 `pmm.c`、`Makefile`：增加显式测试开关，默认关闭，仅在选择 Buddy 时允许启用。
- 在仓库 `report/buddy_usage.md` 补充覆盖说明、命令和实际结果，将实际使用的提示词记录到 `report/prompt.md`。

操作要求：在现有个人任务分支中修改真实文件，新增注释注明 `lab2` 和学号 `2412712`。不修改公共接口、其他分配器、固定测试及评分脚本，不新增 `kern/test`。先验证原实现；若发现真实算法错误，记录失败用例及原因，仅做必要修复，保留原断言后重测。不自动提交或推送。

输出要求：列出用例目的、预期、实际结果及覆盖理由，提供可复现命令。测试复杂度不作要求，但不得删除检查、修改预期来迎合错误实现，或将未执行的项目标为通过。

##### [RELY]

阅读 `buddy_pmm.c`、`buddy_pmm.h`、`pmm.c`、`pmm.h`、`memlayout.h`、`libs/list.h` 及构建规则，以实际代码为准。已有 `buddy_check()` 仅为基础自检，不能代替本次独立测试。

- 可通过 `buddy_pmm_manager` 的回调，以及公共 `alloc_pages()`、`free_pages()`、`nr_free_pages()` 测试真实实现；不要复制一份分配器作为被测代码。
- `pages[0]` 对应物理页号 `nbase`，描述符覆盖 `[nbase, npage)`；`npage` 是页号上界。
- 请求向上取整至最小二次幂。申请 3 页实际占用 4 页，必须用 `free_pages(p, 3)` 完整归还；错误长度、部分释放和重复释放属于非法调用。
- 伙伴关系依据绝对物理页号，不能仅凭相邻判断；空闲块首页的 `property` 为块页数，已分配首页的 `property` 为原始请求长度。
- QEMU 4.1.1 已安装在 WSL：`/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64`。必须显式使用该版本，保留原启动流程。

##### [GUARANTEE]

在 `buddy_test.c` 实现以下测试函数，在 `buddy_test.h` 声明统一入口；允许增加私有辅助函数：

```c
void buddy_test_all(void);
static void test_init_memmap(void);
static void test_split_merge(void);
static void test_rounding(void);
static void test_failure(void);
static void test_reuse(void);
static void test_invalid_request(unsigned int case_id);
```

构建接口：`PMM=buddy BUDDY_TEST=1` 启用测试；`BUDDY_TEST_CASE=0` 执行正常用例，其他编号各执行一个非法调用用例。说明编号与预期断言的对应关系，拒绝非法选项。

##### [SPECIFICATION]

###### 测试隔离与统一规则

1. 在 `pmm_init()` 的 `init_pmm_manager()` 之后、`page_init()` 之前调用可选测试入口。此时真实空闲页尚未登记，可以安全建立受控状态；不在正常分配运行中重置管理器。
2. 使用模块私有静态 `Page` 数组作为测试描述符，临时设置 `pages`、`npage`，保留 `nbase`。各用例重置测试区，将待登记页标为保留，再调用真实管理器登记指定区域。测试区大小应足以构造下述场景。
3. 该数组仅用于验证管理元数据。其页号可能对应真实保留内存，禁止通过这些模拟页的物理或虚拟地址写数据。每个正常用例回收自己的分配；全部完成后清空测试管理状态、恢复原全局变量，再由原 `page_init()` 初始化真实内存。
4. 用简单的逐页记录核对范围、不重叠与实际占用，独立计算期望页数，不直接将分配器返回的计数当成预期。不依赖同阶链表的插入顺序。
5. 每项使用真实断言并输出用例名；正常用例全部执行成功后才打印汇总通过。默认构建不运行扩展测试，原有 `buddy_check()` 继续保留。

###### buddy_test_all

Pre-Condition：已选择并初始化 Buddy，尚未调用真实 `page_init()`；测试开关开启。

Post-Condition：正常模式执行全部正常用例并恢复初始环境，随后正常启动；非法调用模式只运行指定用例。场景构造失败必须报错，不能跳过后仍宣称全部通过。

###### test_init_memmap

Pre-Condition：使用空的受控管理器，描述符已初始化为保留状态。

Post-Condition：核对完整登记、计数及保留区保护。

Case：覆盖对齐的二次幂区域、起点不满足大块对齐且长度为 13 页的区域、多次登记的相邻区域，以及中间留有保留页的分离区域。逐页申请至耗尽，确认只返回登记页、不丢页、不重复；归还后计数恢复，保留页未被分配或错误合并。

###### test_split_merge

Pre-Condition：只登记一个对齐的完整块，例如连续 16 页。

Post-Condition：验证整块分配、小块拆分、真正伙伴逐级合并和相邻非伙伴不合并。

Case 1：整块申请成功且计数归零；归还后拆成单页申请，按页号核对所有页恰好使用一次。

Case 2：按物理位置释放真正伙伴，检查可重新申请对应大块，最终能够合并回完整块。

Case 3：页从 0 编号，仅释放块内相邻的第 1、2 号页，其绝对页号不互为伙伴；其他页保持分配，申请 2 页必须失败。再释放对应伙伴后检查逐级合并，避免把任意相邻页都当伙伴。

###### test_rounding

Pre-Condition：测试区足以容纳多个同时存活的分配块。

Post-Condition：覆盖请求 1、2、3、5、8、9 页，分别按实际占用 1、2、4、8、8、16 页检查计数、对齐和互不重叠；按原始请求长度释放，全部回收后计数恢复。取整产生的余量不能再次分配。

###### test_failure

Pre-Condition：分别构造空管理器、全部占用及碎片化空闲状态。

Post-Condition：无可用页、请求超过容量、超大请求（如 `(size_t)-1`）和取整溢出风险均安全返回 `NULL`，失败前后的计数与可分配页面集合保持一致。

Case：先把完整区域按单页全部分配，再仅释放每对伙伴中的一页，形成总空闲页数足够、却没有 2 页块的状态；申请 2 页应失败。随后仍能正确分配剩余单页并完整回收，验证失败未破坏状态。

###### test_reuse

Pre-Condition：使用已知大小的受控区域。

Post-Condition：以固定、可复现的混合页数请求和不同释放顺序重复多轮；每步核对计数及存活区间，每轮全部释放后恢复初始状态，并再次申请完整块，检验泄漏、重复入链和合并遗漏。

###### test_invalid_request

Pre-Condition：每个用例在独立的 QEMU 启动中执行，先构造合法前置状态并输出用例标识。

Post-Condition：零页申请、重复释放、错误释放长度、内部页释放、重复登记及越界登记分别触发对应断言。错误长度至少包含“申请 3 页却传 4 释放”，验证原始请求契约。

Requirements：预期断言会终止本次启动，不与正常用例连续执行。核对用例标识及对应断言位置或条件；仅发生超时、任意崩溃或未进入内核，不能判定通过。若非法操作意外返回，立即报告测试失败。

###### 编译、运行与交付

- 每次改变测试开关、后端或用例编号前干净构建。正常运行示例：`make qemu PMM=buddy BUDDY_TEST=1 BUDDY_TEST_CASE=0 QEMU=/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64`；实际执行前先 `make clean`。
- 正常测试后必须继续完成真实内存初始化和原基础自检；逐个独立启动非法用例，保存实际输出及预期判断。测试代码在默认关闭时也应正常编译。
- 最后关闭扩展测试，检查原有默认后端和显式 Best-Fit 的构建启动，恢复任务开始时的默认配置。不以 `make grade` 的管理器选择检查替代 Buddy 测试。
- 报告逐项解释：用例针对哪种错误、为什么能发现、实际是否通过。若修复算法，保留失败与修复证据并重跑相关用例；不伪造结果。


### 扩展任务 Challenge：任意大小的内存单元 SLUB 分配算法

#### 提示词一：实现并接入简化 SLUB

```text
[PROMPT]

在现有 RISC-V ucore Lab2 工程中，从零实现简化的 SLUB 内存对象分配模块，并接入内核。

实现两层分配：
第一层复用现有物理页分配接口；
第二层在页面之上提供按字节申请、释放内存对象的能力。

新增 kern/mm/slub.c 和 kern/mm/slub.h。必要时对构建配置和内核初始化入口作最小修改，使模块在 pmm_init() 成功完成后初始化。

操作要求：
必须修改工程中的实际文件，而不是仅展示代码。
先阅读真实工程，确认接口、地址映射与编译规则，再实现。
保留现有物理内存初始化、Best-Fit、Buddy 和框架测试，不重新初始化或替换底层页分配器，不修改评分脚本。
本任务只实现模块和初始化接入，完整测试留给第二份提示词完成。

输出要求：
完成后说明修改文件、接口行为、两层分配关系、关键资源管理方式及编译方法。
明确哪些检查实际执行过；无法编译运行时说明原因，不声称已经通过测试。
解释这是简化教学实现，以及省略了哪些 Linux SLUB 功能。

[RELY]

以当前工程实际代码为准，优先阅读：
- kern/mm/pmm.h、pmm.c；
- kern/mm/memlayout.h、mmu.h；
- kern/mm/default_pmm.c、best_fit_pmm.c；
- kern/init/init.c；
- libs/list.h、defs.h；
- Makefile。

现有可用接口包括：
struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

地址转换资源包括：
page2pa()、pa2page()、PADDR()、va_pa_offset，
以及工程实际提供的页面大小和对齐宏。

struct Page 是物理页描述符，不是可直接读写的对象内存。
分配器向调用者返回的必须是内核可访问的虚拟地址。
不要假设工程已经提供可用的 KADDR()，应核对真实代码。

底层物理内存初始化和页分配器视为前置能力。
当前可使用 Best-Fit，不要求先完成 Buddy。
内核环境不能依赖宿主机 malloc/free 或标准 C 运行库。

[GUARANTEE]

必须新增并公开以下接口：

void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);

允许增加模块内部的 static 辅助函数、类型、宏和状态。
不改变现有页分配接口和 struct Page 的公开布局。

提供以下分配能力：
- 小对象支持 16、32、64、128、256、512、1024、2048 字节类别；
- 请求向上匹配能容纳它的最小类别；
- 大于 2048 字节的请求使用底层连续页分配；
- 所有成功返回的指针至少按 16 字节对齐；
- 释放接口不要求调用者再次提供对象大小。

[SPECIFICATION]

1. slub_init()

Pre-Condition：
物理内存初始化完成，底层页分配接口可以使用。
仅在启动阶段调用一次，此时没有本模块的存活对象。

Post-Condition：
模块处于可接收分配请求的初始状态。
采用按需申请页面的方式，初始化本身不占用底层物理页。
不改变已有页分配器的选择和空闲区。

Requirements：
仅考虑单核、串行调用，不实现并发同步、NUMA、每 CPU 缓存等机制。

2. slub_alloc(size_t size)

Pre-Condition：
slub_init() 已完成。

Post-Condition：
成功时返回至少可访问 size 字节、按 16 字节对齐的内核虚拟地址。
不同存活分配的可访问区域互不重叠。
新分配不能破坏其他存活对象的数据。
不要求返回内存清零。

Case 1：size 为 0。
返回 NULL，模块状态和底层空闲页数不变。

Case 2：size 在 1～2048 之间。
使用最小适合的大小类别。
同一页面能够容纳多个小对象，体现页面之上的对象分配，而不是每个小对象单独申请一页。
当前类别没有可用对象时，按需向底层申请新页。
页面中的元数据不能与返回给用户的对象区域重叠。

Case 3：size 大于 2048。
向底层申请足够的连续页面，提供至少 size 字节的可访问空间。
页数计算必须考虑元数据和对齐开销。
记录释放所需的信息，使 slub_free() 能准确归还完整分配。

Case 4：底层空间不足，或者大小计算发生溢出。
返回 NULL。
回滚本次申请中尚未交付给调用者的资源，不遗留泄漏或损坏管理状态。
已有存活对象仍保持有效。

Requirements：
管理元数据不得依赖本模块递归申请自身内存。
不得把物理地址或 Page 描述符地址直接作为返回指针。
不得以“内部按类别取整”为由减少调用者请求的可访问字节数。

3. slub_free(void *ptr)

Pre-Condition：
模块已经初始化。
ptr 为 NULL，或为本模块返回且尚未释放的原始指针。
重复释放、内部偏移指针及其他非法指针不属于本次必须支持的输入。

Post-Condition：
对应分配被释放，其他存活对象的数据和有效性保持不变。
内部空闲对象计数和底层页面占用保持一致。

Case 1：ptr 为 NULL。
不执行操作，不改变状态。

Case 2：释放小对象。
该对象重新可被分配。
只要所属页面仍有其他存活对象，就不能把该页归还底层。
页面中最后一个存活对象被释放后，立即回收该页。

Case 3：释放大对象。
向底层归还该分配实际占用的全部连续页面，包括元数据所需空间。

Requirements：
所有对象均释放后，底层空闲页数应恢复到调用 slub_init() 前的基线。
释放不要求调用者知道对象类别、申请大小或实际占用页数。
```

#### 提示词二：为前述实验生成测试

```text
[PROMPT]

任务：
针对第一份提示词生成的简化 SLUB 实现，设计并实现可在现有 RISC-V ucore Lab2 内核中运行的测试，验证不同大小对象的申请、释放、资源回收及异常行为。

先阅读第一份提示词的规格和实际生成代码，核对两者是否一致。
新增 kern/mm/slub_test.c 和 kern/mm/slub_test.h，并提供显式开启的内核测试入口。

操作要求：
必须修改工程实际文件，而不是仅列出测试思路。
本任务主要新增测试；不得通过降低断言要求、修改原有框架测试或改变 SLUB 规格来使测试通过。
发现 SLUB 实现不符合规格时，保留能暴露问题的测试，报告预期、实际结果和定位依据，不擅自将错误行为当成正确行为。

输出要求：
交付可编译的测试代码、开启与关闭步骤、运行命令和覆盖说明。
逐项解释测试能发现哪些错误，哪些行为尚未验证。
只报告真实运行结果，区分实际输出与预期输出。
说明 make grade 不包含这些 SLUB 测试，不能代替本任务的验证。

[RELY]

依赖第一份提示词约定的接口：

void slub_init(void);
void *slub_alloc(size_t size);
void slub_free(void *ptr);

依赖的行为契约：
- 0 字节申请返回 NULL；
- 小对象类别为 16、32、64、128、256、512、1024、2048；
- 大于 2048 字节的请求采用连续页分配；
- 返回指针至少按 16 字节对齐；
- slub_free(NULL) 无副作用；
- 空 slab 页立即归还底层；
- 全部对象释放后，底层空闲页数恢复；
- 分配失败不得破坏既有对象和资源状态。

可以使用工程现有的：
nr_free_pages()、assert()、cprintf()、页面大小宏，
以及实际存在的字节访问和内存操作工具。

以实际工程头文件为准，不使用宿主 malloc/free。
测试所需指针表、大小记录和数据模式记录使用有界静态数组，避免内核栈溢出，也避免用被测分配器申请测试控制数据。

不依赖 SLUB 私有链表和内部布局作为唯一判断依据。

[GUARANTEE]

必须新增：

void slub_check(void);

允许将各测试场景拆分成 static 辅助函数。

提供显式测试开关：
仅在开启时，于 pmm_init() 和 slub_init() 完成后调用 slub_check()。
默认基础启动与评分行为保持原样。
不重复调用 slub_init() 来清除测试造成的资源问题。

新增测试不要放在 kern/test，也不要修改 tools/inject.py 或原有评分用例。

[SPECIFICATION]

slub_check()

Pre-Condition：
物理页分配器和 SLUB 均已初始化。
没有测试之外的 SLUB 存活对象。
测试运行期间没有其他代码并发申请或释放物理页。

Post-Condition：
全部场景通过时，输出明确的逐项结果和最终通过标记。
所有成功申请的测试资源均被释放，底层空闲页数恢复到测试开始的基线。
若断言失败，应能从日志定位失败场景，不能继续输出最终通过标记。

必须覆盖：

Case 1：零大小与空指针。
验证 slub_alloc(0) 返回 NULL；
验证 slub_free(NULL) 不改变空闲页数和后续分配行为。

Case 2：类别边界与对齐。
覆盖 1，以及各类别边界前、边界处和边界后的代表值；
至少包括 15、16、17，31、32、33，1023、1024、1025，2047、2048、2049。
检查返回值和 16 字节对齐，并在完整请求范围内读写。

Case 3：对象不重叠与数据保持。
同时保留多个不同大小的分配，为每个对象写入独立数据模式；
检查所有存活区域两两不重叠；
交错执行申请和释放后，重新验证其余存活对象的完整数据。
不能只验证指针不同或只检查第一个字节。

Case 4：释放后复用。
释放部分小对象，再申请同类别对象。
验证新申请有效且不破坏其他存活对象，不要求返回地址与释放前完全相同。
在已有空闲容量足够的受控场景中，检查不需要额外占用底层页面。

Case 5：跨多个 slab 页。
在同一小对象类别中保留足够数量的对象，使其总请求字节数超过一页。
验证分配确实跨越多个页面，并检查对象数据、独立性和回收。
另设一个适量小对象场景，确认不是每个小对象都独占一页。

Case 6：页面回收。
在其他对象仍存活时释放部分对象，确认存活对象继续有效。
释放全部对象后，验证底层空闲页数恢复到场景开始的基线。
不能靠重新初始化分配器恢复计数。

Case 7：大对象。
覆盖 2049 字节、PGSIZE-1、PGSIZE、PGSIZE+1 和超过两页的请求。
读写完整请求范围，检查与其他对象不重叠。
释放后检查实际页面全部归还，特别关注元数据导致的额外页占用。

Case 8：失败和大小溢出。
验证明显超过可用内存的请求和接近 size_t 上限的请求返回 NULL；
确认失败前后空闲页数不变，既有对象数据不变，之后仍能正常申请和释放。
另设计受控的底层页不足场景：用有界静态记录保存临时占用页，确保所有临时资源均可回收。
不能只把“大请求被拒绝”等同于底层申请失败路径已经验证；若环境限制导致该路径未实际触发，应明确注明。

Case 9：重复混合操作。
以固定、可复现的顺序重复混合大小的申请与释放。
过程中持续验证存活对象数据，结束后检查空闲页数恢复。
测试次数和数组容量应有明确上限，不依赖运行环境碰巧终止。

Requirements：
每个测试说明“操作、预期、断言、可发现的错误”。
失败测试不能仅依据实现中的同一套大小计算逻辑生成预期，避免实现和测试一起算错。
资源计数比较应使用当前场景记录的基线，不能写死机器内存大小。
重复释放和非法指针不属于第一份规格的有效输入，不将这些场景混入正常通过测试。
最终提供适合写入实验报告的覆盖说明，解释为何这些测试能检验两层分配、对象隔离和页面回收。
```
