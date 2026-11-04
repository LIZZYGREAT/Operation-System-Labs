# 模板
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


---

# Lab3 A1：Lab2 内存管理依赖回填与基础自检提示词

## [PROMPT]

任务：参考小组已经完成的 Lab2 实现，将物理内存初始化和 Best-Fit 分配器接入 Lab3 新框架，完成 A1 的内存基础自检，为后续 A2 的 Trap 入口与上下文恢复提供可用的内存管理基础。

仓库：`A:/OS/exp/Operation-System-Labs`。以下代码路径相对于 `code/`，文档路径相对于仓库根目录。

- 修改 `kern/mm/pmm.c`：在 `lab2` 标记处回填 `static void page_init(void)`，并在 `init_pmm_manager()` 中选择 `best_fit_pmm_manager`。
- 修改 `kern/mm/best_fit_pmm.c`：在 `lab2` 标记处回填 Best-Fit 的私有状态、管理函数和管理器定义，完整保留现有 `basic_check()`、`best_fit_check()`。
- 新建 `report/a1_memory_usage.md`：记录迁移来源、适配点、内存布局、验证命令、真实结果和后续依赖。

操作要求：

1. 先阅读当前源码、`report/任务分工.md` 和 Lab2 历史实现，以 Lab3 的真实定义为准。必须在项目中修改实际文件，完成实现和允许范围内的验证。
2. 在当前个人任务分支 `work/lab3/ziqiu` 上工作，先检查 Git 状态，保留已有未提交修改。读取 Lab2 使用 `git show lab2:<path>`，不要为取代码切换当前工作区，不自动提交、推送或合并。
3. 逐项迁移需要的函数和私有状态，禁止用 Lab2 的整个 `pmm.c`、`memlayout.h`、`pmm.h` 或 `Makefile` 覆盖 Lab3 文件。保留 Lab3 公共分配接口中的 `local_intr_save()`、`local_intr_restore()`，以及当前原子位操作宏。
4. 本次只做 A1。保留 `trapentry.S`、`trap.c`、启动汇编、设备树解析器和正式 `init.c`；两个 Trap 处理函数的临时占位已补好，本次不重新补占位，不实现 A2、B、C 的任务。
5. 不迁入 Buddy、SLUB、Lab2 的扩展测试和后端选择开关，不增加无关功能或修改构建规则。保留 First-Fit 实现、公共接口、`struct Page`、`struct pmm_manager`、固定自检、评分脚本和注入脚本。
6. 保留迁入代码原有的 `lab2` 标记和贡献者学号。本人新增的适配说明使用 `lab3 A1` 和学号 `2412712`，不要将其他成员的原始实现改署为本人贡献。

输出要求：说明修改文件、Lab2 代码来源、Lab3 适配点、关键地址与计数关系及实际验证结果。使用下面的上下文、接口清单和行为规格；发现历史实现与当前框架不兼容时，做本任务范围内的必要适配并说明原因。

实际执行本提示词后，再将使用过的 Prompt、追加约束和真实修正过程补入仓库 `report/prompt.md` 的 A1 小节，保留现有模板和内容。没有运行的检查、环境阻塞和后续模块依赖必须如实标注。

## [RELY]

### 阅读顺序与参考来源

先阅读当前 Lab3 文件：

- `kern/mm/pmm.c`、`pmm.h`：PMM 初始化顺序、全局状态、地址转换及带中断保护的公共接口。
- `kern/mm/best_fit_pmm.c`、`best_fit_pmm.h`、`default_pmm.c`：Best-Fit 待填位置、管理器声明、固定测试及 First-Fit 参考实现。
- `kern/mm/memlayout.h`、`mmu.h`，`libs/defs.h`、`list.h`、`atomic.h`、`riscv.h`：内存布局、页面标记、链表、对齐及原子操作。
- `kern/driver/dtb.h`、`dtb.c`，`kern/init/init.c`、`entry.S`、`kern/sync/sync.h` 和 `Makefile`：DTB 信息来源、直接映射、启动顺序、中断保护和构建入口。

再只读查看 Lab2 的相关实现及历史提示词：

```bash
git show lab2:code/kern/mm/pmm.c
git show lab2:code/kern/mm/best_fit_pmm.c
git show lab2:code/kern/mm/memlayout.h
git show lab2:report/prompt.md
```

编写本提示词时，本地 `lab2` 指向 `ba88beae418e63456d55f92f29cbd6e5db9a421c`。实际执行时记录所采用的提交，不把后续变化默认当成本次已核查的来源。

指导书重点为：

- [Lab2 练习：物理内存初始化与 Best-Fit](http://127.0.0.1:8000/lab2026/_book/lab2/lab2_2_1_exercise.html)。
- [Lab3 练习：开头的 Lab2 依赖说明](http://127.0.0.1:8000/lab2026/_book/lab3/lab3_2_1_exercise.html)。
- [Lab3：中断的关闭和开启](http://127.0.0.1:8000/lab2026/_book/lab3/lab3_3_5_atomic.html)。

### 当前框架的关键事实

1. `kern_init()` 在清零 BSS 后调用 `dtb_init()`，随后调用 `pmm_init()`。PMM 内部顺序为 `init_pmm_manager()` → `page_init()` → `check_alloc_page()`，最后设置并输出启动页表的虚拟、物理地址。保留这个顺序。
2. `init_pmm_manager()` 当前仍选择 `default_pmm_manager`；仅回填 Best-Fit 文件不会使内核使用该算法，必须完成本次管理器切换。
3. `struct Page` 包含 `ref`、`flags`、`property` 和 `page_link`。`free_area_t` 包含 `free_list` 与 `nr_free`。Best-Fit 的固定自检直接使用后两者，需提供相应的模块私有状态及同名访问方式。
4. `pages[0]` 对应物理页号 `nbase`，描述符覆盖 `[nbase, npage)`，数量为 `npage - nbase`。`npage` 是物理页号上界。`page2ppn()`、`page2pa()`、`pa2page()` 均依赖该约定。
5. `PADDR()` 依赖 `va_pa_offset`；使用它前先将偏移量初始化为当前 `PHYSICAL_MEMORY_OFFSET`。链接符号 `end` 是内核镜像结束位置的虚拟地址，`pages` 是描述符数组的虚拟地址，两者都需要与物理地址区分。
6. Lab3 提供 `KERNBASE`、`KMEMSIZE`、`KERNTOP`、`PHYSICAL_MEMORY_OFFSET`，但未提供 Lab2 新增的 `PHYSICAL_MEMORY_END`、`KERNEL_BEGIN_PADDR`、`KERNEL_BEGIN_VADDR`。迁移时根据现有宏和映射关系，在 `pmm.c` 内推导内核物理起点及管理上界，避免引用未定义名称或修改公共头文件来恢复旧宏。当前内核物理起点可由 `KERNBASE - va_pa_offset` 得到，管理上界可由 `KERNTOP - va_pa_offset` 得到。
7. `alloc_pages()`、`free_pages()`、`nr_free_pages()` 已在调用管理器前保存并关闭本 hart 中断，调用后按原状态恢复。原本关闭的中断不能被无条件开启；本次沿用现有保护，不扩展为多 hart 分配器。
8. 可调用 `get_memory_base()`、`get_memory_size()`、`init_memmap()`、`pa2page()`、`set_page_ref()`、链表接口、页面状态宏、`assert()`、`panic()` 和 `cprintf()`，使用工程现有签名。

## [GUARANTEE]

在 `pmm.c` 中实现或最小修改：

```c
static void page_init(void);
static void init_pmm_manager(void);
```

在 `best_fit_pmm.c` 中实现并注册：

```c
static void best_fit_init(void);
static void best_fit_init_memmap(struct Page *base, size_t n);
static struct Page *best_fit_alloc_pages(size_t n);
static void best_fit_free_pages(struct Page *base, size_t n);
static size_t best_fit_nr_free_pages(void);

const struct pmm_manager best_fit_pmm_manager;
```

允许迁入用于按地址插入空闲块的私有辅助函数。管理器名称为 `"best_fit_pmm_manager"`，六个回调分别指向对应管理函数和现有 `best_fit_check()`；`best_fit_pmm.h` 已有 `extern` 声明，保持一致。

现有 `basic_check()`、`best_fit_check()` 和 `check_alloc_page()` 作为验证依赖保留，不重新实现、不删除断言、不改变预期。

## [SPECIFICATION]

### 统一状态约定

1. Best-Fit 使用按页面地址递增排列的双向空闲链表，每个空闲连续块只链接首页；首页设置 `PageProperty`，其 `property` 为块页数。其他页不能残留会被识别为空闲块首页的标记。
2. `nr_free` 表示空闲页总数，等于链表所有空闲块页数之和。一个页面不能被重复管理，已分配区与空闲区互不重叠，保留页不能被分配。
3. 申请 `n` 页实际占用恰好 `n` 页。允许按框架固定测试释放已分配连续区间中的合法子区间；不使用 Buddy 的二次幂取整或“只能按原始请求长度整块释放”的契约。
4. 本次沿用 Lab2 的合法调用约定：登记和释放区间合法且不重叠，长度大于零；保留原有断言。无需新增一套分配历史表或非法调用测试系统。

### page_init

Pre-Condition：当前内核直接映射已建立；DTB 解析流程已执行，但查询结果仍需校验；当前 PMM 已初始化为空管理状态，尚未登记真实空闲页。

Post-Condition：正确设置 `va_pa_offset`、`npage`、`pages`，建立与页号转换一致的描述符数组；OpenSBI、内核、启动页表、描述符数组及对齐占用均处于保留范围；仅将剩余合法完整页交给 `init_memmap()`，输出探测范围、管理上界、描述符占用和初始空闲区。

Case 1：内存信息有效且有剩余完整页。先取得 DTB 起点与长度，检查加法溢出及内核覆盖关系，将报告末端限制到框架管理上界并向下按页对齐。设置 `npage` 后，以 `npage - nbase` 计算描述符数量，将数组放在内核末端之后的页对齐位置。先验证容量，再初始化全部描述符为保留、引用计数为零的确定状态；描述符末端向上对齐后，登记至管理末端之间的完整空闲页。

Case 2：DTB 起点或长度无效、范围计算溢出、报告范围不能覆盖内核，或管理上界不成立。使用 `panic()` 终止，不访问错误地址，不继续登记页面，不把默认 128 MiB 当成真实探测结果。

Case 3：DTB 报告范围大于框架上界，或边界没有页对齐。只管理两者共同允许的完整页面；内核、描述符及其对齐余量都不得进入空闲区。固定 `nbase` 下，描述符覆盖的不可用低地址页保持保留。

Case 4：空间不足以容纳完整描述符数组。计算乘法、地址加法和向上对齐前检查容量与溢出风险，通过 `panic()` 结束，禁止越界写描述符。

Case 5：描述符能够建立，但已无剩余完整页。保持描述符与保留状态合法，明确输出此情况，不调用长度为零的 `init_memmap()`。后续固定分配自检可能因缺页失败，应如实说明。

Requirements：使用依赖全局状态的转换接口前先建立相应状态；不能把 `npage` 当作描述符数量分配数组；先初始化全部保留描述符，再登记空闲页；不直接操作 Best-Fit 内部链表代替 `init_memmap()`。

### init_pmm_manager

Pre-Condition：内核正在初始化物理内存管理，尚未由 `page_init()` 登记空闲页。

Post-Condition：`pmm_manager` 指向 `best_fit_pmm_manager`，保留真实管理器名称输出并调用其 `init()`；后续公共接口和固定自检均分派到 Best-Fit。

Requirements：只做管理器选择的必要修改，不迁入 Lab2 的 Buddy 测试入口及 `PMM_USE_*` 分支，不改变 `pmm_init()` 的后续流程。

### best_fit_init

Pre-Condition：首次初始化管理器，真实空闲页尚未登记。

Post-Condition：初始化私有空闲链表，`nr_free` 为零；不访问尚未建立的 `pages` 数组，不分配动态内存。

### best_fit_init_memmap

Pre-Condition：管理器和描述符数组已初始化；`n > 0`，`[base, base + n)` 为合法、尚未登记的保留页区间。

Post-Condition：初始化区间内的页面状态和引用计数，将首页组织为长度为 `n` 的空闲块，按地址顺序插入链表，`nr_free` 增加 `n`；区间外页面不受影响。

Requirements：保留对传入页保留状态的断言。初始化阶段与 `page_init()` 配合，不重复登记、不把描述符自身占用空间加入空闲区。

### best_fit_alloc_pages

Pre-Condition：管理器已初始化，`n > 0`，空闲链表及计数满足状态约定。

Post-Condition：成功返回连续 `n` 页的首页，清除其空闲块首页标记，空闲页数减少 `n`；失败返回 `NULL`，链表、标记和计数保持不变。

Case 1：存在足够大的空闲块。从所有长度不小于 `n` 的块中选择最小者，同尺寸时选择低地址块。精确匹配时移除该块；块较大时取其前 `n` 页，将余下部分作为新的空闲块保留在正确链表位置。

Case 2：总空闲页数不足，或总量足够但没有满足长度的连续块。返回 `NULL`，不能只按总量判断成功，也不能在失败路径遗失或拆坏空闲块。

Requirements：保持真正的 Best-Fit 选择规则。不能直接使用 First-Fit 实现来代替，也不能将任意非零 `property` 值当作空闲首页的依据。

### best_fit_free_pages

Pre-Condition：`n > 0`，传入区间位于有效页面范围内，全部为合法已分配、尚未释放的页；允许固定测试使用的合法子区间释放。

Post-Condition：重置待释放页的引用计数和相关标记，建立空闲块并按地址插入；与物理上连续的前后空闲块合并，清除被合并首页的旧标记并正确摘链；空闲页总数仅增加本次释放的 `n` 页。

Case 1：无相邻空闲块。保留新块及其长度。

Case 2：与前块、后块或两侧均相邻。依次完成所需合并，合并后的首页为最低地址页，块长度为参与区间长度之和；前向合并后基于更新的首页继续检查后块。

Requirements：相邻依据页面地址与块长度判断，链表相邻本身不足以合并；不跨保留区或已分配区合并；合并不能重复增加 `nr_free` 或造成节点重复入链。

### best_fit_nr_free_pages

Pre-Condition：管理器已初始化。

Post-Condition：返回真实空闲页总数，不修改管理状态，不把空闲块数量当作页数。

### 固定自检与中断保护

Pre-Condition：Best-Fit 已被选中，`page_init()` 已建立合法空闲区，默认内存配置足以执行框架自检。

Post-Condition：现有 `basic_check()`、`best_fit_check()` 通过，`check_alloc_page()` 输出 `check_alloc_page() succeeded!`；自检前后的空闲页总数和合法空闲布局恢复。

Requirements：保留两个固定自检的函数体、断言及评分输出。不得用只打印成功、调用 First-Fit 检查或检查注册指针代替算法自检。核对 `alloc_pages()`、`free_pages()`、`nr_free_pages()` 的原有中断保护保持完整；A1 的内存自检不能证明后续 Trap 进入与返回已经正确。

### 编译、运行与交付

1. 在 WSL 的实际仓库 `code` 目录先执行 `make clean`、`make`，保留真实构建结果。检查 Best-Fit 管理器能够链接，旧版宏引用和未定义符号已消除；工具链问题如实记录，不修改无关模块迎合环境。
2. QEMU 使用已安装的 4.1.1：`/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64`。确认路径与版本后执行：

   ```bash
   make qemu QEMU=/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64
   ```

   保留当前 `-bios default` 和镜像加载方式，不改变启动协议。设置合理的运行时限或在完成观察后安全退出，超时本身不能证明测试通过。
3. 运行时核对真实管理器名称、DTB 报告范围、受限后的管理范围、描述符占用、初始空闲区、固定自检成功输出及启动页表地址输出。独立按布局计算初始空闲页数并与记录核对，不能仅复制分配器返回的计数作为预期。
4. A2 及 B、C 尚未完成时，正常启动可能在开启时钟后进入未完成的 Trap 链路。若因此无法稳定观察 A1，可使用独立代码副本，仅在副本 `kern_init()` 的 `pmm_init()` 返回后、`clock_init()` 与 `intr_enable()` 之前暂停。该副本运行相同的正式 PMM 实现和固定自检；记录副本路径、源版本及唯一的测试控制改动，不把临时暂停逻辑写入正式工程。保留日志后清理本任务创建的临时副本。
5. 对无效 DTB、管理上界限制、对齐、描述符容量和无剩余完整页等边界，至少逐项做代码核查；如实际构造了运行场景，记录具体输入和输出。结果表明确区分“代码核查通过”“实际运行通过”“未执行”，不得将静态检查写成运行结果。
6. 本次验收为“A1 编译与内存基础自检”。35 分上下文与断点主测属于 A3，依赖 A2 和 C 的真实异常处理；完整 `make grade` 还依赖 B、C 的实现。不要要求 A1 单独取得 Lab3 的 100 分，也不要套用 Lab2 的评分输出宣称 Lab3 通过。
7. 在 `report/a1_memory_usage.md` 记录：Lab3 基线和迁入的 Lab2 提交、修改清单、地址推导与描述符布局、保留区和空闲页核对、中断保护保留情况、环境与命令、日志位置、真实结果及后续依赖。实际执行后补充 `report/prompt.md` 的 A1 记录，保留原有内容和修正过程。
8. 交付前检查最终差异：仅包含 A1 的两个实现文件和相关文档，正式启动流程、Trap 代码、固定测试及公共结构未被修改；没有遗留测试暂停、临时代码副本或无关改动。提供可供 B 后续独立复测的步骤，不提前宣称复测已完成。

## 本次执行与修正记录

执行日期：2026-11-03。个人分支：`work/lab3/ziqiu`。

- 用户执行指令：“OK，按照这个提示词去完成A1吧”。执行期间追加强调：“注意啊版本是4.1.1”。全部实际 QEMU 运行均使用 `/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64`，已核对版本为 4.1.1。
- 迁移来源：Lab2 提交 `ba88beae418e63456d55f92f29cbd6e5db9a421c`；Lab3 基线为 `6ad00e708fe2000f9cb8a2ddef297c5d1015bea1`。实际读取源码后，仅迁入 `page_init()` 和 Best-Fit 管理函数及其私有状态，保留原贡献者学号；在 `init_pmm_manager()` 中选择 Best-Fit。
- Lab3 适配：旧版内核物理起点和内存上界宏未定义，改为根据当前映射宏在 `pmm.c` 内推导；补充对齐前的内核末端范围检查。没有覆盖公共头文件、启动代码或构建规则。
- 首次干净构建失败：GCC 15.1.0 对应的汇编器不识别原框架的 `sbadaddr`。采用命令行 `DEFS=-Dsbadaddr=stval` 进行名称兼容，正式 Trap 源码保持原样；随后完整编译、链接和镜像生成通过。失败与成功日志分别保留在 `report/logs/a1_build_plain.log`、`a1_build.log`。
- QEMU 4.1.1 启动实际输出 `memory management: best_fit_pmm_manager` 和 `check_alloc_page() succeeded!`。默认 128 MiB 配置下，空闲物理区间为 `[0x80347000, 0x88000000)`，共 31,929 页。正式启动使用 4 秒时限，退出码 124 仅表示到时结束；通过结论来自真实自检输出。
- 补充 GDB 检查在 `pmm_init()` 返回后、`clock_init()` 前暂停正式镜像。导出并检查 32,768 个真实描述符，确认 839 页保留、31,929 页空闲，引用计数、唯一空闲块及双向链表连接正确，且最终空闲页计数与布局独立计算结果相符。调试中修正了非登录 Shell 的工具链路径、GDB 导出表达式及 PowerShell 管道尾部回车造成的日志文件名问题；这些是验证工具使用问题，没有因此修改算法或固定测试。临时二进制导出已删除。
- 对比基线确认两个固定自检和三个带中断保护的公共包装函数保持不变。无效 DTB、超出管理上界、尾页对齐、描述符空间不足及无剩余空闲页等分支完成代码核查，未注入这些异常场景运行，不将其描述为运行通过。
- 本次没有提交、推送或合并代码。完整 Lab3 评分及中断、异常功能等待 A2、B、C；B 的独立复测尚待执行。

实际修改、内存布局、边界核查、复现命令和证据索引见 [A1 内存管理记录](./a1_memory_usage.md)。源码文件哈希及修改范围见 `report/logs/a1_source_check.json`。


---

# Lab3 A2：Trap 入口与上下文恢复实现提示词

## [PROMPT]

任务：补全 Lab3 的 Trap 入口和上下文恢复，使当前框架形成“Trap 进入 → 保存上下文 → 调用 C 语言处理函数 → 恢复上下文 → 返回原执行流”的完整链路。

仓库：`A:/OS/exp/Operation-System-Labs`。以下代码路径相对于 `code/`，文档路径相对于仓库根目录。

- 修改 `kern/trap/trapentry.S`：只在两个 `lab3 function1 YOUR CODE` 位置补全 `RESTORE_ALL` 和 `__alltraps` 的缺失逻辑，与现有 `SAVE_ALL`、`__trapret` 配合。
- 新建 `report/a2_trap_usage.md`：简要记录帧布局、入口与恢复逻辑、`sp` 的特殊处理及实际验证结果。

操作要求：

1. 先阅读实际源码，在当前个人分支 `work/lab3/ziqiu` 修改真实文件。保留 A1 已完成的内存管理代码和现有未提交修改，不自动提交、推送或合并。
2. 保持最小实现。沿用现有框架对有效 Trap frame、可用内核栈和合法调用的前置约定，不增加无需求的参数检查、栈范围检查或额外恢复机制，不扩展用户态切栈、多 hart 同步等功能。
3. 保留 `SAVE_ALL` 的完整实现、`struct pushregs`、`struct trapframe`、`trap()` 签名，以及 `__alltraps`、`__trapret` 的全局符号和原有对齐要求。保留 `__trapret` 调用 `RESTORE_ALL` 后执行 `sret` 的结构。
4. 正式工程中不修改 `trap.c`、`trap.h`、`init.c`、启动汇编、公共头文件、Makefile、固定测试和评分脚本，不新增 `kern/test`。中断和异常处理由 B、C 负责，本次不替换其占位函数。
5. 保留原有 `lab3 function1` 标记，新增实现说明注明 `lab3 A2` 和本人学号 `2412712`。

输出要求：说明修改位置、帧布局、参数传递、寄存器恢复顺序及实际验证结果。以以下行为规格和真实源码为依据，自行选择合适的 RISC-V 指令，不机械复制指导书伪代码。实现保持直接、清楚，验证控制在本任务所需范围内。

实际执行本提示词后，再将使用过的 Prompt 和必要的修正过程追加到 `report/prompt.md` 的 A2 小节，保留原有模板及 A1 记录。未执行的验证和未就绪的依赖如实注明。

## [RELY]

### 源码与已有状态

先阅读：

- `kern/trap/trapentry.S`：现有 `SAVE_ALL`、两个待填位置及返回入口。
- `kern/trap/trap.h`：`struct pushregs`、`struct trapframe` 和 `trap()` 声明。
- `kern/trap/trap.c`：`idt_init()`、`trap()`、`trap_dispatch()` 和当前处理函数状态。
- `libs/riscv.h`：`LOAD`、`STORE`、`REGBYTES`、`SSTATUS_SIE`、`SSTATUS_SPIE`、`SSTATUS_SPP` 等实际定义。
- `kern/init/init.c`、`Makefile`：初始化顺序及构建、运行入口。
- `tools/test_lab3_context.S`、`test_lab3.c`、`inject.py`、`grade.sh`：现有上下文测试的触发方式、依赖及通过条件，仅按验证需要阅读和使用。

A1 已在当前工作区回填 `page_init()` 和 Best-Fit，并完成内存自检；本次保留这些实现。执行时仍以实际源码状态为准。

编写本提示词时，`interrupt_handler()`、`exception_handler()` 均为会触发 `panic()` 的临时占位。它们能够供工程链接，但不能完成正常 Trap 返回。后续若已有 B、C 的真实实现，按当时实际状态判断可运行的测试。

指导书重点为：

- [练习：功能模块一](http://127.0.0.1:8000/lab2026/_book/lab3/lab3_2_1_exercise.html)。
- [RISC-V 中断介绍](http://127.0.0.1:8000/lab2026/_book/lab3/lab3_3_1_introduce.html)。
- [中断入口点](http://127.0.0.1:8000/lab2026/_book/lab3/lab3_3_2_trapentry.html)。
- [中断处理程序](http://127.0.0.1:8000/lab2026/_book/lab3/lab3_3_3_trap.html)。

### Trap frame 的实际布局

当前目标为 RISC-V64，`REGBYTES` 为 8。`SAVE_ALL` 在当前栈上建立 36 个寄存器宽度的帧，共 288 字节。以下槽位必须与 `trap.h` 一致；实现使用现有宏，避免另建一套偏移定义。

| 内容 | 槽位 | 当前字节偏移 |
| --- | --- | --- |
| `gpr.zero`，即 `x0` | 0 | 0 |
| `gpr.ra`，即 `x1` | 1 | 8 |
| `gpr.sp`，即 Trap 前的 `x2` | 2 | 16 |
| `x3` 至 `x31` | 3 至 31 | 24 至 248 |
| `status`，保存的 `sstatus` | 32 | 256 |
| `epc`，保存的 `sepc` | 33 | 264 |
| `badvaddr`，保存的异常地址 | 34 | 272 |
| `cause`，保存的 Trap 原因 | 35 | 280 |

`SAVE_ALL` 先通过 `sscratch` 暂存 Trap 前的 `sp`，为帧分配空间后，再将原 `sp` 写入第 2 槽，并将 `sscratch` 清零。因此，保存后的当前 `sp` 是帧基址，第 2 槽保存的才是最终需要恢复的栈指针。

`idt_init()` 将 `stvec` 指向 `__alltraps`。C 层接口为：

```c
void trap(struct trapframe *tf);
```

按 RISC-V 调用约定，首个参数通过 `a0` 传递；调用 C 函数时栈保持 16 字节对齐。当前 288 字节的帧大小保持入口已有的栈对齐。

## [GUARANTEE]

必须补全以下现有汇编接口：

| 接口 | 交付要求 |
| --- | --- |
| `RESTORE_ALL` | 根据当前帧恢复通用寄存器、返回状态及返回地址 |
| `__alltraps` | 使用已有保存过程，将帧地址传给 `trap()`，并在 C 函数返回后衔接统一恢复入口 |

已有 `SAVE_ALL`、`__trapret` 和 C 层 `trap()` 是本次依赖，不重新实现。保留原有 `sret`；`RESTORE_ALL` 完成恢复后由该指令执行特权级返回。

## [SPECIFICATION]

### __alltraps

Pre-Condition：处理器已因 Trap 进入当前入口，当前内核栈可用，并满足现有框架的栈对齐约定；入口中的 `SAVE_ALL` 保持原样。

Post-Condition：通过已有 `SAVE_ALL` 建立一次完整 Trap frame，将其地址作为 `a0` 参数调用 `trap(tf)`；C 函数正常返回后，当前 `sp` 仍指向同一帧，随后进入 `__trapret` 执行恢复。

Requirements：

- 使用栈上已经保存的真实帧，不重新复制帧或另建上下文结构。
- 区分“调用 C 函数前后的帧基址”和“Trap 前的原始栈指针”；进入恢复过程前，帧必须仍可通过当前 `sp` 访问。
- C 调用可能覆盖 `ra`、参数寄存器和临时寄存器，后续恢复依赖帧中的保存值；不能假设这些寄存器跨 C 调用保持不变。
- 中断与异常共用此入口，分类及具体处理交给已有 C 层。入口不判断异常指令长度，不自行推进 `epc`。

### RESTORE_ALL

Pre-Condition：`trap()` 已正常返回，当前 `sp` 指向符合上述布局的有效 Trap frame；C 处理函数对帧所做的合法更新已经完成。

Post-Condition：`x1` 至 `x31` 按帧中的值恢复，`x0` 保持硬连线零；`sepc` 和 `sstatus` 来自当前帧的 `epc`、`status`。恢复完成后，原有 `sret` 根据这些状态返回执行流，栈不遗留本次 Trap frame 的空间占用。

Case 1：C 处理未改变保存上下文。恢复保存的通用寄存器、栈指针及返回状态，使被中断执行流能够继续。

Case 2：C 处理合法更新了 `tf->epc` 或其他保存字段。以更新后的帧为准恢复；例如断点处理推进后的 `epc` 必须用于返回，不能重新读取进入 Trap 时的旧 `sepc` 覆盖它，也不能再次增加指令长度。

Requirements：

- 恢复覆盖全部可写通用寄存器，包括调用者保存寄存器和被调用者保存寄存器；`a0` 也应恢复，不能遗留传入 C 层的帧指针。
- 恢复 CSR 所需的临时寄存器最终也要恢复，避免取出返回状态时覆盖已恢复的原值。
- 在最后一次依赖当前帧基址的访存完成后，再恢复第 2 槽中的 `x2(sp)`；不能提前切换 `sp` 导致后续加载使用错误地址。
- 按保存的 `sstatus` 和 `sret` 语义处理返回特权级与中断状态。当前实验在内核态触发测试，不固定清除 `SPP` 返回用户态，也不在恢复途中额外开启中断。
- `sret` 会按体系结构规则更新 `SIE`、`SPIE`、`SPP`，不能将返回后的这些位与帧中值简单作逐位相等判断。
- `badvaddr`、`cause` 用于诊断与分派，无需在返回时写回相应 CSR；沿用 `SAVE_ALL` 建立的 `sscratch` 约定。

### 构建与必要验证

1. 完成后做一次干净构建，并检查实际指令与帧布局、参数传递和 `sp` 恢复顺序相符。沿用 A1 已确认的工具链兼容参数，保留 `SAVE_ALL` 中的旧 CSR 名称：

   ```bash
   make clean
   make DEFS=-Dsbadaddr=stval
   ```

2. 需要运行时，必须使用 **QEMU 4.1.1**，先确认实际版本，普通启动显式指定路径：

   ```bash
   make qemu DEFS=-Dsbadaddr=stval QEMU=/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64
   ```

   保留 `-bios default` 和现有镜像加载方式。运行设置合理时限；编译成功、内存自检成功或单纯超时均不能证明 Trap 返回正确。
3. C 的真实断点处理可用时，使用现有上下文测试完成一次针对性联调，核对断点输出、寄存器检查及返回后继续执行。现成测试需要在注入后的临时副本中构建，单独增加 `GRADE_LAB3_CONTEXT` 宏不会将 `tools` 中的测试自动接入正式工程。保留原测试，不另建测试框架或测试替身。
4. 若使用 `grade.sh`，注意它直接调用 `qemu-system-riscv64`，不读取 Makefile 的 `QEMU` 变量；通过当前会话的 `PATH` 确保实际调用 4.1.1，并确认内部构建仍保留 CSR 兼容参数。不要为此修改评分脚本或替换系统安装。
5. 若 C 仍为占位，记录“返回链路的运行验证依赖 C 的真实处理函数”，完成本次实现、构建和必要源码核对即可，不为获得通过标记擅自补写 C 的任务。若发现具体错误，再针对该错误检查和重测；不反复执行相同验证，不重跑 A1 的全量描述符检查或无关评分项。
6. 现有上下文测试检查了一组寄存器，不能据此宣称所有通用寄存器、所有特权级和嵌套场景均已动态覆盖。正式 35 分主测按 A3 分工记录；40 分时钟、25 分异常与兜底及完整 100 分验收留待相应模块就绪。

### 交付

`report/a2_trap_usage.md` 简要说明两个修改点、帧与寄存器的对应关系、C 调用前后的栈状态，以及必要验证的命令、结果和限制。实际使用的提示词与修正过程追加到 `report/prompt.md`。

交付前检查本次新增差异仅涉及 `trapentry.S` 的两个待填位置和 A2 文档，A1 与其他成员代码、固定测试及正式启动流程保持原样。未运行的项目明确说明，不将依赖占位造成的失败归为已经通过。

## 本次执行与修正记录

执行日期：2026-11-03。个人分支：`work/lab3/ziqiu`。用户执行指令：“OK，按照这个提示词去完成A2吧”。沿用此前“避免防御性编程和过度验证”的要求。

- 先核对实际帧布局、调用接口及当前 C 层状态，确认 `exception_handler()`、`interrupt_handler()` 仍为会触发 `panic()` 的占位。
- 仅在 `trapentry.S` 的两个待填位置补充实现：`__alltraps` 将当前帧基址通过 `a0` 传给 `trap()`，返回后顺序进入 `__trapret`；`RESTORE_ALL` 先恢复 `sstatus` 和 `sepc`，再恢复通用寄存器，最后从第 2 槽恢复 `sp`。保留 `SAVE_ALL` 和原有 `sret`，新增说明注明 `lab3 A2 - 2412712`。
- 实现未增加额外运行时检查、上下文结构或测试框架，未修改 A1、C 层处理、公共结构体、初始化流程、固定测试或构建规则。
- 使用 WSL 工具链执行一次 `make clean` 和 `make DEFS=-Dsbadaddr=stval`，完整构建通过；检查反汇编，确认实际参数传递、CSR 与通用寄存器槽位、最后恢复 `sp` 和执行 `sret` 的顺序正确。沿用兼容参数，未修改 `SAVE_ALL` 中的旧 CSR 名称。
- 当前 C 的断点处理尚未实现，本次没有运行需要其正常返回的上下文测试，也没有构造替身或重复执行无关检查。动态返回验证及 A3 的 35 分主测等待 C 的真实异常处理；本次未启动 QEMU，后续运行仍要求显式使用 QEMU 4.1.1。
- 本次没有提交、推送或合并。实现说明、必要命令和验证限制见 [A2 Trap 入口与上下文恢复记录](./a2_trap_usage.md)。


## 提交整理记录（2026-11-03）

用户授权将当前 A1、A2 实现提交并推送至个人分支 `work/lab3/ziqiu`。

- 保留当前文件重命名：`prompt.md` 改为 `prompt_a1,a2.md`，报告模板 `report.md` 改为 `report_还暂时没写.md`。以上提示词原文中的旧路径保留为执行当时的记录。
- A1 的原始日志和辅助检查脚本已清理，本次提交保留实现及验证摘要；对应说明文档已去除失效链接和依赖已删除脚本的步骤。
- 本次仅整理文档引用，源码自上次验证后未改动；不重复运行构建和测试。A2 动态返回及 A3 主测仍等待 C 的真实异常处理。


---

# Lab3 A3+A4：上下文主测与异常复测提示词

## [PROMPT]

任务：基于当前已经合入 A、B、C 实现的 Lab3 代码，由 A 完成 A3“Trap 入口、寄存器恢复与断点返回”35 分项主测，以及 A4 对 C“非法指令与兜底”25 分项的独立复测，记录真实结果和简要解释。

仓库：`A:/OS/exp/Operation-System-Labs`。以下代码路径相对于 `code/`，文档路径相对于仓库根目录。

- 使用已有评分入口和测试，不新增内核功能或测试框架。
- 维护 `report/a3_a4_test.md`：记录测试版本、环境、A3 和 A4 结果、关键机制及验证范围，注明 `lab3 A3+A4` 和学号 `2412712`。
- 后续需要重新复现时，保存本次评分控制台日志到 `report/_work/logs/a3-a4-grade.log`；原有 `a3-grade.log` 保留为最初 A3 运行记录。
- 实际执行后，将本提示词和必要修正过程追加到 `report/_work/prompts/ziqiu.md` 的 A3+A4 小节，保留原有执行记录。

操作要求：

1. 在当前个人分支 `work/lab3/ziqiu` 工作。该分支已同步至 `lab3` 的 `e88ec44`，执行时记录实际提交号，无需切换共享分支。不自动提交、推送或合并。
2. 先阅读与本次测试直接相关的源码。后续重新复现时，运行一次现有完整评分，同时核对 A3 的 35 分和 A4 的 25 分即可。评分自身会执行三组测试和必要的干净构建，无需提前另跑一轮完整构建或分别重复启动三组测试。仅整理已完成的材料时，不重新运行测试。
3. 保持最小操作范围，避免防御性编程和过度验证。保留现有内核、测试及预期；不增加运行时检查、测试替身、辅助验证框架或额外测试矩阵。
4. 只有出现具体失败时，才围绕该问题检查和重测。若定位到 A 自己实现中的真实错误，做必要修复并记录原因；B、C 模块的问题注明归属，不擅自重写其他成员任务。不通过修改测试或输出通过标记迎合评分。
5. 本次必须实际执行，由 A 形成独立记录。已有 B、C 日志仅作背景，不能直接作为本次结果。

输出要求：说明测试所用版本、环境和命令、A3 的 35 分结果、A4 的 25 分复测结果、总评分及必要解释。未执行或失败的项目如实记录。结果通过后完成文档即可，不为了增加验证数量重复运行。

## [RELY]

先阅读以下相关部分：

- `kern/trap/trapentry.S`、`trap.h`：帧布局、`__alltraps`、`RESTORE_ALL` 以及原始 `sp` 的恢复。
- `kern/trap/trap.c`：`trap()` 分派、断点处理和 `tf->epc` 的更新。
- `tools/test_lab3_context.S`、`test_lab3.c`：现有寄存器测试和 35 分通过输出。
- `test_lab3.c` 的非法指令与兜底分支：A4 需要复测的非法指令返回、软件中断提示、未知中断提示及未知异常输出。
- `tools/inject.py`、`grade.sh`、`Makefile` 的 `grade` 目标：测试注入位置、临时副本、实际评分条件及构建命令。
- `report/任务分工.md`：A 主测 35 分；B 主测 40 分；C 主测 25 分。

当前关键事实：

1. A1、A2 以及 B、C 的真实实现均已合入当前版本。C 的异常处理能够识别断点，并按指令前缀将 `tf->epc` 增加 2 或 4 字节。部分旧文档仍保留占位说明，以实际源码为准。
2. `SAVE_ALL` 建立 36 槽的帧，RISC-V64 下共 288 字节；第 2 槽保存原始 `sp`。A2 通过 `a0` 传入帧指针，在恢复其他内容后最后恢复 `sp`，再执行 `sret`。
3. `test_lab3_register_context()` 设置一组寄存器值，执行 `ebreak`，返回后比较这些值；成功返回 0，失败返回 1。该用例显式比较 `t0` 至 `t6`、`s0` 至 `s11`、`a1` 至 `a7`，共 26 个寄存器。
4. `test_lab3_exception_context()` 在 `GRADE_LAB3_CONTEXT` 模式中调用上述汇编用例，并根据返回值输出 35 分项结果。评分同时要求串口出现 `Breakpoint at` 和 `[Test 1] Trap entry and breakpoint return: PASS`。
5. `make grade` 在临时副本中注入测试，不改正式 `init.c`。只增加评分宏不能让 `tools` 中的测试自动进入普通构建；应使用现有入口。
6. C 的历史记录已有 100/100，其中上下文项为 35/35；本次仍需独立运行。
7. A4 按分工独立复测 25 分项。评分要求非法指令返回 PASS、`Illegal instruction at`、`Supervisor software interrupt`、`Unknown interrupt: cause 15` 以及 cause 20 的 Trap frame 输出。

## [GUARANTEE]

实际使用并保留以下已有测试接口：

```c
uintptr_t test_lab3_register_context(void);
void test_lab3_exception_context(void);
```

本次不需要新增内核函数。交付为真实评分结果、`report/a3_a4_test.md` 和个人 Prompt 追加记录；报告需要图片时，保留一张本次真实结果的截图到 `report/images/a3_a4_grade.png`，不为截图重复运行测试。历史复测若按用户要求未保存日志，应保留这一事实，只补记实际结果。

## [SPECIFICATION]

### 现有上下文与断点测试

Pre-Condition：使用当前集成代码，A2 的汇编入口与恢复、C 的真实断点处理均可用；评分注入和用例选择由现有工具完成。

Post-Condition：实际触发断点，经保存现场、C 层处理、恢复上下文和 `sret` 返回，继续执行寄存器比较；评分结果与真实用例输出一致。

Case 1：测试通过。确认评分中的 `trap entry, register restore, and breakpoint EPC` 为 `PASS (35/35)`。说明该结果对应断点处理、返回后的执行，以及用例所比较寄存器的恢复。

Case 2：测试失败。保留原始输出，先区分构建、启动、断点处理或寄存器比较失败，再针对具体原因处理。仅发生超时、打印过断点信息或内存自检通过，都不足以判定 35 分通过。

Requirements：不改动测试函数、比较值或通过条件；不额外构造完整寄存器测试或 16/32 位异常组合测试。

### A4：非法指令与兜底复测

Pre-Condition：C 的真实异常处理及 B 的中断兜底依赖已就绪，使用原有 `GRADE_LAB3_ILLEGAL` 用例，由 A 独立构建并运行。

Post-Condition：非法指令处理后继续执行，软件中断、未知中断及未知异常均出现规定输出，评分给出 `PASS (25/25)`；结论来自 A 的实际运行。

Requirements：按现有 25 分项范围复测即可，解释各输出对应的分支；发现具体错误再处理，不额外扩展测试矩阵。

### 评分环境与执行

Pre-Condition：在 WSL 登录环境的实际 `code` 目录工作，RISC-V 工具链可用。

Post-Condition：使用 **QEMU 4.1.1** 执行一次完整评分，记录真实退出状态，同时提取 A3 的 35 分与 A4 的 25 分结果。已执行材料的整理以原有结果和日志保存状态为准。

可采用以下命令：

```bash
cd /mnt/a/OS/exp/Operation-System-Labs/code
mkdir -p ../report/_work/logs
export PATH="/home/catalpa/.local/opt/qemu-4.1.1/bin:$PATH"
command -v qemu-system-riscv64
qemu-system-riscv64 --version
set -o pipefail
make grade CC='riscv64-unknown-elf-gcc -Dsbadaddr=stval' 2>&1 | tee ../report/_work/logs/a3-a4-grade.log
```

`grade.sh` 直接调用 `qemu-system-riscv64`，因此通过本次会话的 `PATH` 指定 4.1.1。当前评分脚本会为各用例重新设置 `DEFS`；兼容定义通过命令行 `CC` 传入，使子构建同时保留旧 CSR 名称兼容和各自的评分宏，无需修改 Makefile 或评分脚本。

Requirements：保留现有 BIOS 和镜像加载方式。使用 `pipefail` 保留评分失败状态，不能将 `tee` 成功当成测试通过。评分成功时会清理临时副本，控制台日志是本次保留的评分证据；如因失败留下副本，定位问题后仅清理本次创建的临时内容。

### 结果说明与交付

Pre-Condition：本次评分已完成，或已经获得明确的实际失败原因。

Post-Condition：`report/a3_a4_test.md` 清楚记录以下内容：

- 学号 `2412712`、实际提交号、分支、日期、编译器和 QEMU 版本。
- 实际命令、评分退出状态、A3 的 35 分主测与 A4 的 25 分复测结果、日志保存状态；完整评分顺带得到的 40 分项和总分也如实记录。
- 简要解释帧指针传递、原 `sp` 的保存与最后恢复，以及 C 更新 `epc` 后如何经 `sepc`、`sret` 继续执行。
- A4 的非法指令返回及规定兜底输出对应哪些现有检查；分两轮运行时，区分每轮目的、结果和日志，不能把第一轮日志写成第二轮新日志。
- 本次测试的覆盖范围和实际问题；没有发生修复时直接说明，不编造迭代。

Requirements：现有用例显式比较 26 个寄存器，不能将通过结果描述为所有寄存器、所有特权级或嵌套 Trap 均已动态验证。`sp`、`ra` 参与返回链路，但该用例未单独逐值比较它们；按实际测试能力表述结论即可。

A3 的 35 分主测与 A4 的 25 分复测按上述要求通过后，记录两项完成。40 分结果作为附带集成结果，不据此宣称其他成员交叉复测或共同终审完成。保留原有成员资料，将更新规格和实际结果记录到 `report/_work/prompts/ziqiu.md`，不重建旧的 `report/prompt.md`。

## 材料归档说明（2026-11-04）

第一轮评分日志原名为 `a3-grade.log`，现存材料文件名为 `report/_work/logs/a3a4-grade.log`；以上提示词中的历史文件名保留原文。第二轮 A4 复测未另存日志，现存日志仍对应第一轮评分。报告截图统一使用 `.png` 后缀。
