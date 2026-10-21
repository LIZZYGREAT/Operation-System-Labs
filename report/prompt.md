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

## 功能模块2：物理内存分配功能

### 任务二：实现 Best-Fit 连续物理内存分配算法

> 本文件只记录本人的 Lab2 实验相关提示词。收到与实验任务相关的新消息后，在完成该轮工作后，将其整理为下方 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 结构再追加。分支、提交状态及提示词文件本身的管理对话不收录；不纳入其他成员的任务提示。

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

## Best-Fit 需求

空闲块首地址必须设置 `PageProperty`，并以 `property` 保存连续页数；`free_list` 按物理地址排序；`nr_free` 等于所有空闲块页数之和。已分配页不得保留在空闲链表中，空闲块不得重叠。

分配必须从所有 `property >= n` 的块中选最小块；同尺寸时选物理地址较低者。无连续块时返回 `NULL`，不得改变任何空闲状态。分配过大的块须拆分余块并正确更新起始标志、链表节点及计数。

释放时按地址插入，分别检查左右相邻块并合并；合并完成后只有合并块首保留 `PageProperty`，`nr_free` 增加实际释放页数，不因合并重复计数。

固定测试会临时隔离 `free_list` 并将 `nr_free` 置零，之后恢复保存的链表头和计数。因此实现不能依赖额外缓存，也不能只凭 `nr_free` 判断链表中一定存在候选块。

## 实现用 Prompt

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

## 分配器行为

`init` 建立空链表并将空闲页数清零。`init_memmap(base,n)` 要求 `n > 0`、页面处于保留状态；清理描述符状态、引用计数，将整个区间登记为一个地址有序空闲块。

`alloc_pages(n)` 要求 `n > 0`。在所有足够大的块中选择最小者，同尺寸选择低地址；失败时返回 `NULL` 且状态不变。成功时返回块首描述符；若块较大，登记剩余连续区间，更新 `PageProperty`、`property`、链表和 `nr_free`。

`free_pages(base,n)` 要求释放的是已分配的连续页面。恢复释放页的标志和引用状态，按物理地址插入，并分别合并左邻、右邻空闲块。释放和合并后不得出现重叠、重复节点或错误块首标志，`nr_free` 仅增加本次释放页数。

## 验收场景

精确匹配；多候选选择最小块及同尺寸低地址优先；请求超过总量；总量足够但无连续块；拆分；左合并、右合并、双侧合并；反复分配释放后恢复；空链表失败；固定测试临时隔离并恢复 `free_list`、`nr_free`。

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

## B2 接入与验收

Pre-Condition：`page_init()` 已实现；default 管理器可以初始化内存并通过 First-Fit 自检；B1 Best-Fit 回调已完成，固定测试未修改。

Post-Condition：`init_pmm_manager()` 注册 Best-Fit，内核初始化经由该管理器完成，固定自检与自动评分通过。

Case 1：前置条件缺失

报告缺少的证据并停止切换，不修改其他成员负责的实现，也不跳过基线。

Case 2：前置条件满足

只修改管理器注册指针，执行构建、QEMU 与 `make grade`。若 QEMU 始终运行，使用有界超时安全结束并区分预期停止与启动失败。

Requirements：确认固定 `best_fit_check()` 实际成功；自动评分必须通过 Page Init 和 Best-Fit 两项。不能只以管理器指针注册检查代替算法自检。

**检查与处理记录**

- 前置检查发现 `page_init()` 已由 A 实现，当前 default 管理器和 B1 源码均未改动，Best-Fit 固定测试未修改。
- default 基线 `make` 退出 0；`timeout --foreground --signal=INT --kill-after=2s 6s make qemu` 启动到内核并输出 `memory management: default_pmm_manager`、`check_alloc_page() succeeded!`，之后以 124 结束，这是超时安全中断内核空闲循环。
- 仅将 `kern/mm/pmm.c` 的 `init_pmm_manager()` 注册指针改为 `best_fit_pmm_manager`。`make` 退出 0；QEMU 输出 `memory management: best_fit_pmm_manager` 及 `check_alloc_page() succeeded!`，证明管理器固定自检通过。
- `make grade` 退出 0，评分为 `Total Score: 20/20`，Page Init 与 Best-Fit 两项均为 `OK`。评分脚本将 QEMU 串口写入临时文件，因此另在临时副本运行原注入脚本并从串口确认 `[Test 1] Page Init: PASS`、`[Test 2] Best-Fit: PASS`；测试与脚本未修改。
- `git diff --check` 通过；源码 diff 仅改 `pmm.c` 中注册指针，未自动提交。
