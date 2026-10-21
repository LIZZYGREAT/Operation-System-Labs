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
