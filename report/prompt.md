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

## Buddy System：C 线实现用 Prompt 与实际记录

### 2026-10-21：C 线 Buddy 实现、内核接入与 QEMU 4.1.1 验证

**用户请求与确认**

> 好，那现在按照这份提示词完成 Buddy System 的实现和接入，第二份测试提示词等实现完成后再说。

本人学号由用户明确提供为 `2412712`。

> 那你先按照指导书帮我安装 QEMU 4.1.1，再用这个版本继续验证 Buddy，之前新增的兼容辅助文件也删掉。

**执行与修正记录**

- 在个人分支 `work/lab2/ziqiu/buddy` 实现 `buddy_pmm.c`、`buddy_pmm.h`；仅在 `pmm.c` 增加管理器选择，在 Makefile 增加 `PMM` 参数。保留 A 的初始化、公共结构体、B 的分配器与固定测试、启动代码和评分脚本。
- 使用按阶空闲链表与绝对物理页号的 XOR 伙伴计算；申请向上取整到二次幂，首页记录原始请求，释放按实际占用归还。新增实现与接入代码标注本人学号。
- 初次验证误用了默认 QEMU 7.0.0，启动出现分页后的取指异常，曾借助临时 GDB 文件排错。用户纠正后停止将该环境作为正式验证依据；已删除全部兼容辅助文件及旧日志。
- 从指导书指定地址获取官方 QEMU 4.1.1 源码，在 WSL 用户目录编译安装，启用调试信息；使用实际输出为 4.1.1 的模拟器和原启动命令重新验证。
- Buddy 编译、启动与基础自检通过，空闲页数恢复为 31929；First-Fit、Best-Fit 的原有启动自检均通过，非法 PMM 参数被拒绝。验证后恢复基线默认 First-Fit 构建。未修改 Buddy 算法来迎合测试。
- 运行说明及命令见 `buddy_usage.md`，原始输出见三个 `*_qemu_4.1.1.log`。基础评分、完整独立测试与组员复现未执行；第二份测试提示词尚未编写。没有提交或推送。

**实际采用的第一份提示词原文**

下文保留提示词编写时的原文；其中“当前尚未执行”描述的是编写当时状态，实际执行结果以上方记录为准。
# Lab2：Buddy System 实现提示词

依据 `report/prompt.md` 模板编写。本提示词用于实现与接入，独立测试提示词后续另写；当前尚未执行。

## [PROMPT]

任务：参考现有 ucore 框架，从零实现 Buddy System 物理页分配器并接入内核。

仓库：`A:/OS/exp/Operation-System-Labs`。以下代码路径相对于 `code/`。

- 新建 `kern/mm/buddy_pmm.c`：实现分配器、管理器注册和基础自检。
- 新建 `kern/mm/buddy_pmm.h`：声明管理器，说明分配与释放约定。
- 最小修改 `kern/mm/pmm.c` 和 `Makefile`：支持通过 `PMM=buddy` 选择 Buddy，并支持 `first_fit`、`best_fit`，保留当前默认选择。
- 新建仓库内 `report/buddy_usage.md`：记录设计、切换步骤和验证结果。

操作要求：先阅读实际源码，遵循仓库分支规范，在项目中修改真实文件；保留其他成员代码，不改 `page_init()`、公共结构体、已有分配器及固定测试。使用模块私有静态结构，不依赖动态内存分配。本次只考虑单 hart 串行调用。

保留 `lab2` 注释，新增贡献注释使用本人真实学号；学号未提供时向用户询问，不编造。不自动提交或推送。

输出要求：说明修改文件、核心设计和实际验证结果；未运行或被环境阻塞的项目如实注明。实际执行后将提示词和修正过程补入仓库 `report/prompt.md` 的 Buddy 小节。

## [RELY]

先阅读 `kern/mm/pmm.h`、`pmm.c`、`memlayout.h`、`mmu.h`、`default_pmm.c`、`libs/list.h`、`libs/defs.h` 和构建规则，以真实定义为准。

可依赖 `struct Page`、`struct pmm_manager`、页面状态宏、链表操作、`page2ppn()`、`pa2page()`、`assert()`、`cprintf()` 等已有资源。

- `pages[0]` 对应物理页号 `nbase`，描述符覆盖 `[nbase, npage)`；`npage` 是页号上界，描述符数量为 `npage - nbase`。
- 调用顺序为 `init_pmm_manager()` → `page_init()` → `check_alloc_page()`；`page_init()` 会将可用页面通过 `init_memmap(base, n)` 交给管理器。
- 管理器的 `init()` 执行时 `pages`、`npage` 尚未建立，只能初始化自身状态；阶数组容量根据框架支持范围确定。

## [GUARANTEE]

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

## [SPECIFICATION]

### 统一约定

1. 每个阶维护一个空闲链表，阶 `k` 的块含 \(2^k\) 页，首页物理页号按块大小对齐；另维护总空闲页数。
2. 请求 `n` 页时向上取整到最小的二次幂，实际占用整个块；释放时传回原始请求 `n`。例如申请 3 页实际占用 4 页，`free_pages(p, 3)` 归还全部 4 页，计数按 4 页变化。不支持部分释放。
3. 伙伴按绝对物理页号计算：`q = page2ppn(p)`，伙伴页号为 `q ^ ((size_t)1 << k)`。仅同阶、合法范围内、确认为空闲块首页的真正伙伴可以合并；访问其描述符前先检查范围。
4. 空闲块首页设置 `PageProperty`，`property` 记录块页数；已分配首页清除此标记，`property` 记录原始请求长度，并用模块私有且未被占用的 `flags` 位标识已分配首页。内部页清除旧首页标记，`property` 为 0；保留位含义不变。
5. 一个节点只能属于一个链表；拆分和合并正确摘链、更新标记。所有已登记页既不丢失也不重复管理，保留页不进入空闲区，空闲计数等于各阶空闲块页数之和。
6. 零长度、越界、重复登记、重复释放、内部页释放和错误释放长度，通过断言或 `panic()` 报告，检查先于状态修改。地址、取整和移位运算防止越界与溢出。

### buddy_init

Pre-Condition：启动时尚未登记页面。

Post-Condition：初始化各阶空闲链表，总空闲页数为 0；不访问尚未建立的描述符数组。

### buddy_init_memmap

Pre-Condition：管理器及描述符数组已初始化，`n > 0`，区间合法且尚未登记，传入页均为保留状态。

Post-Condition：清除传入页的保留标记、初始化引用计数等元数据，将整个区间组织为符合对齐的二次幂空闲块，空闲页数增加 `n`。

Case：起点不满足大块对齐或长度不是二次幂时，分成多个合法块覆盖全部页面，不浪费页、不扩大区间。支持多次登记不重叠区间，合并合法伙伴，不能跨保留间隙。

### buddy_alloc_pages

Pre-Condition：管理器已初始化，`n > 0`。

Post-Condition：成功返回实际分配块首页的 `Page *`，记录原始请求，空闲页数减少实际占用；失败返回 `NULL` 且状态不变。

Case 1：目标阶有空闲块，直接取出；没有时从最近的可用高阶逐级拆分，将剩余半块放回相应阶。

Case 2：总量不足、没有合适连续块，或请求超出支持范围、取整会溢出，返回 `NULL`。已分配块互不重叠，取整余量不能再次分配。

### buddy_free_pages

Pre-Condition：`base` 是尚未释放的分配块首页，`n` 与原始请求长度相同。

Post-Condition：归还整个实际分配块，与同阶空闲伙伴逐级合并；每次选择低地址为新首页，清除旧标记。

Requirements：检查指针范围和元素对齐后再访问描述符。空闲页数只增加本次归还的实际页数，合并不重复增加；伙伴不满足条件或到达最大阶时停止。

### buddy_nr_free_pages

Pre-Condition：管理器已初始化。

Post-Condition：返回所有阶的空闲页总数，不修改状态，不把块数量或分配块内部碎片计为空闲页。

### buddy_check

Pre-Condition：页面登记已完成，由现有 `check_alloc_page()` 调用。

Post-Condition：用真实断言检查单页与非二次幂分配、实际计数、不重叠及释放后复用；回收全部自检资源，恢复原空闲页数和合法状态。资源不足时明确说明未执行用例。

Requirements：不能留空、借用其他分配器检查或只打印成功。完整独立测试后续另写。

### 接入与验证

- 在 `init_pmm_manager()` 中通过编译宏选择管理器，保留原初始化流程；`Makefile` 将 `PMM` 参数传给编译器，拒绝非法选项，复用已有源码收集规则。
- 在实际实验环境中执行 `make clean`、`make PMM=buddy`、`make qemu PMM=buddy`，检查真实管理器名称及自检输出；切换后端须干净构建，演示后恢复原默认配置。
- 提供 Best-Fit 的切换与基础评分步骤，最终小组默认后端仍应为 Best-Fit。基础评分中的 Best-Fit 选择检查不能用于判定 Buddy 正确性；不改评分脚本、不新增 `kern/test`。
- 记录实际运行结果及限制，保留原有测试；基础自检通过后仍需独立测试与另一位成员复现。

### 2026-10-21：执行第二份 Buddy 测试提示词

**用户原始提示**

> 现在按照第二份提示词完成 Buddy 的测试代码，用 QEMU 4.1.1 运行验证，记录结果。

**执行与验证记录**

- 在 `work/lab2/ziqiu/buddy` 分支新增 `buddy_test.c`、`buddy_test.h`，在真实 `page_init()` 前使用 64 个静态描述符建立受控状态；仅操作管理元数据，不访问其页号指向的真实物理内存。用独立逐页归属表核验计数、区间、标记和回收。
- Makefile 新增默认关闭的 `BUDDY_TEST` 和 `BUDDY_TEST_CASE`；`pmm.c` 仅增加条件头文件和测试调用。新增 `tools/run_buddy_tests.sh` 自动干净构建、校验 QEMU 4.1.1 版本并核对实际日志。
- 八个正常场景全部通过，包括 13 页非对齐登记、分离区域与保留间隙、伙伴/非伙伴区分、逐级合并、非二次幂请求、碎片化失败与 16 轮混合分配释放。正常测试恢复全局状态后，原内核继续初始化真实内存，空闲页数为 31926，原 Buddy 自检通过。测试代码增大镜像，关闭测试后为 31929 页。
- 六次独立启动分别验证零长度申请、重复释放、申请 3 页按 4 页释放、内部页释放、重复登记和越界登记。均匹配到指定用例标识及真实 `buddy_pmm.c` 对应断言；未将超时或任意崩溃当作通过。
- 测试关闭后的 Buddy、Best-Fit、默认 First-Fit 启动自检通过；四个非法构建配置被拒绝。最终恢复默认 First-Fit 构建。实际环境日志记录 QEMU 4.1.1、OpenSBI 0.4、RISC-V GCC 14.2.0。
- 本轮测试代码和脚本首次运行通过，未发现需修复的算法错误；没有修改 Buddy 算法、其他分配器、公共结构体、启动代码或评分测试。结果、覆盖理由及命令补入 `buddy_usage.md`，实际输出在 `buddy_tests/`。
- 未运行基础评分或组员交叉复现，未提交或推送。

**实际使用的第二份提示词原文**
# Lab2：Buddy System 测试提示词（第二份）


## [PROMPT]

任务：阅读现有 Buddy 实现，设计并实现独立测试函数，验证页面登记、分配、拆分、释放、合并和边界行为，并说明各用例为什么能够发现错误。

仓库：`A:/OS/exp/Operation-System-Labs`。以下代码路径相对于 `code/`。

- 新建 `kern/mm/buddy_test.c`、`buddy_test.h`：实现受控测试环境、各项测试及统一入口。
- 最小修改 `pmm.c`、`Makefile`：增加显式测试开关，默认关闭，仅在选择 Buddy 时允许启用。
- 在仓库 `report/buddy_usage.md` 补充覆盖说明、命令和实际结果，将实际使用的提示词记录到 `report/prompt.md`。

操作要求：在现有个人任务分支中修改真实文件，新增注释注明 `lab2` 和学号 `2412712`。不修改公共接口、其他分配器、固定测试及评分脚本，不新增 `kern/test`。先验证原实现；若发现真实算法错误，记录失败用例及原因，仅做必要修复，保留原断言后重测。不自动提交或推送。

输出要求：列出用例目的、预期、实际结果及覆盖理由，提供可复现命令。测试复杂度不作要求，但不得删除检查、修改预期来迎合错误实现，或将未执行的项目标为通过。

## [RELY]

阅读 `buddy_pmm.c`、`buddy_pmm.h`、`pmm.c`、`pmm.h`、`memlayout.h`、`libs/list.h` 及构建规则，以实际代码为准。已有 `buddy_check()` 仅为基础自检，不能代替本次独立测试。

- 可通过 `buddy_pmm_manager` 的回调，以及公共 `alloc_pages()`、`free_pages()`、`nr_free_pages()` 测试真实实现；不要复制一份分配器作为被测代码。
- `pages[0]` 对应物理页号 `nbase`，描述符覆盖 `[nbase, npage)`；`npage` 是页号上界。
- 请求向上取整至最小二次幂。申请 3 页实际占用 4 页，必须用 `free_pages(p, 3)` 完整归还；错误长度、部分释放和重复释放属于非法调用。
- 伙伴关系依据绝对物理页号，不能仅凭相邻判断；空闲块首页的 `property` 为块页数，已分配首页的 `property` 为原始请求长度。
- QEMU 4.1.1 已安装在 WSL：`/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64`。必须显式使用该版本，保留原启动流程。

## [GUARANTEE]

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

## [SPECIFICATION]

### 测试隔离与统一规则

1. 在 `pmm_init()` 的 `init_pmm_manager()` 之后、`page_init()` 之前调用可选测试入口。此时真实空闲页尚未登记，可以安全建立受控状态；不在正常分配运行中重置管理器。
2. 使用模块私有静态 `Page` 数组作为测试描述符，临时设置 `pages`、`npage`，保留 `nbase`。各用例重置测试区，将待登记页标为保留，再调用真实管理器登记指定区域。测试区大小应足以构造下述场景。
3. 该数组仅用于验证管理元数据。其页号可能对应真实保留内存，禁止通过这些模拟页的物理或虚拟地址写数据。每个正常用例回收自己的分配；全部完成后清空测试管理状态、恢复原全局变量，再由原 `page_init()` 初始化真实内存。
4. 用简单的逐页记录核对范围、不重叠与实际占用，独立计算期望页数，不直接将分配器返回的计数当成预期。不依赖同阶链表的插入顺序。
5. 每项使用真实断言并输出用例名；正常用例全部执行成功后才打印汇总通过。默认构建不运行扩展测试，原有 `buddy_check()` 继续保留。

### buddy_test_all

Pre-Condition：已选择并初始化 Buddy，尚未调用真实 `page_init()`；测试开关开启。

Post-Condition：正常模式执行全部正常用例并恢复初始环境，随后正常启动；非法调用模式只运行指定用例。场景构造失败必须报错，不能跳过后仍宣称全部通过。

### test_init_memmap

Pre-Condition：使用空的受控管理器，描述符已初始化为保留状态。

Post-Condition：核对完整登记、计数及保留区保护。

Case：覆盖对齐的二次幂区域、起点不满足大块对齐且长度为 13 页的区域、多次登记的相邻区域，以及中间留有保留页的分离区域。逐页申请至耗尽，确认只返回登记页、不丢页、不重复；归还后计数恢复，保留页未被分配或错误合并。

### test_split_merge

Pre-Condition：只登记一个对齐的完整块，例如连续 16 页。

Post-Condition：验证整块分配、小块拆分、真正伙伴逐级合并和相邻非伙伴不合并。

Case 1：整块申请成功且计数归零；归还后拆成单页申请，按页号核对所有页恰好使用一次。

Case 2：按物理位置释放真正伙伴，检查可重新申请对应大块，最终能够合并回完整块。

Case 3：页从 0 编号，仅释放块内相邻的第 1、2 号页，其绝对页号不互为伙伴；其他页保持分配，申请 2 页必须失败。再释放对应伙伴后检查逐级合并，避免把任意相邻页都当伙伴。

### test_rounding

Pre-Condition：测试区足以容纳多个同时存活的分配块。

Post-Condition：覆盖请求 1、2、3、5、8、9 页，分别按实际占用 1、2、4、8、8、16 页检查计数、对齐和互不重叠；按原始请求长度释放，全部回收后计数恢复。取整产生的余量不能再次分配。

### test_failure

Pre-Condition：分别构造空管理器、全部占用及碎片化空闲状态。

Post-Condition：无可用页、请求超过容量、超大请求（如 `(size_t)-1`）和取整溢出风险均安全返回 `NULL`，失败前后的计数与可分配页面集合保持一致。

Case：先把完整区域按单页全部分配，再仅释放每对伙伴中的一页，形成总空闲页数足够、却没有 2 页块的状态；申请 2 页应失败。随后仍能正确分配剩余单页并完整回收，验证失败未破坏状态。

### test_reuse

Pre-Condition：使用已知大小的受控区域。

Post-Condition：以固定、可复现的混合页数请求和不同释放顺序重复多轮；每步核对计数及存活区间，每轮全部释放后恢复初始状态，并再次申请完整块，检验泄漏、重复入链和合并遗漏。

### test_invalid_request

Pre-Condition：每个用例在独立的 QEMU 启动中执行，先构造合法前置状态并输出用例标识。

Post-Condition：零页申请、重复释放、错误释放长度、内部页释放、重复登记及越界登记分别触发对应断言。错误长度至少包含“申请 3 页却传 4 释放”，验证原始请求契约。

Requirements：预期断言会终止本次启动，不与正常用例连续执行。核对用例标识及对应断言位置或条件；仅发生超时、任意崩溃或未进入内核，不能判定通过。若非法操作意外返回，立即报告测试失败。

### 编译、运行与交付

- 每次改变测试开关、后端或用例编号前干净构建。正常运行示例：`make qemu PMM=buddy BUDDY_TEST=1 BUDDY_TEST_CASE=0 QEMU=/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64`；实际执行前先 `make clean`。
- 正常测试后必须继续完成真实内存初始化和原基础自检；逐个独立启动非法用例，保存实际输出及预期判断。测试代码在默认关闭时也应正常编译。
- 最后关闭扩展测试，检查原有默认后端和显式 Best-Fit 的构建启动，恢复任务开始时的默认配置。不以 `make grade` 的管理器选择检查替代 Buddy 测试。
- 报告逐项解释：用例针对哪种错误、为什么能发现、实际是否通过。若修复算法，保留失败与修复证据并重跑相关用例；不伪造结果。
