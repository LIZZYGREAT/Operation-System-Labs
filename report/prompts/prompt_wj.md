# Lab4 A1：页表映射管理提示词

负责人：甘文杰；学号：2412700。

## [PROMPT]

### 任务

请先阅读 Lab4 的内存管理代码，完成虚拟地址与物理页之间的映射维护。主要是实现页表映射的建立、替换和删除，同时考虑物理页引用计数，以及原有 Sv39 页表结构和启动映射能否与这些操作配合使用。

### 操作要求

以 `lab4` 分支现有框架为基础，主要修改 `code/kern/mm/pmm.c`，补全 `LAB4 function1` 对应的 `page_insert()` 和 `page_remove()`，保留实验标记，并按分工填写学号 `2412700`。

先理解 `get_pte()`、`get_page()` 和物理页分配接口的关系，再实现映射操作。若原有页表遍历或启动映射检查妨碍正常执行，可以做必要的适配，但需说明原因；不要直接覆盖其他实验功能，也不要修改进程创建、调度或评分代码。

### 输出要求

完成代码后，检查映射建立、覆盖、解除时的引用计数和空闲页变化。优先使用框架已有的检查函数验证结果，保留真实的编译、运行输出。若运行被其他模块尚未实现的部分阻塞，请区分本模块问题与外部依赖。

## [RELY]

### 内存与页表结构

- Lab4 使用 RV64 的 Sv39 三级页表。虚拟地址依次通过 `PDX1(la)`、`PDX0(la)` 和 `PTX(la)` 定位各级表项，最终页内偏移由 `PGOFF(la)` 给出。
- 普通页面大小为 `PGSIZE=4096`。启动页表中已经存在内核高地址映射，部分映射使用超级页，不应直接按普通 4 KiB 叶子页表处理。
- `boot_pgdir_va` 是启动根页表的内核虚拟地址；`struct Page` 描述物理页，`page->ref` 记录映射引用情况。
- `PTE_V` 表示表项有效；`PTE_R`、`PTE_W`、`PTE_X`、`PTE_U` 描述访问权限。非叶子页表项与叶子页表项的用途不同，修改时应遵循 Sv39 页表语义。

### 已有接口

- `get_pte(pgdir, la, create)`：查找目标地址对应的页表项；在 `create` 为真时允许分配缺失的下级页表。
- `get_page(pgdir, la, ptep_store)`：查找已经映射到目标虚拟地址的物理页，并可返回对应表项位置。
- `alloc_page()`、`free_page(page)`：分配和释放物理页；`page2ppn()`、`pte2page()`、`page2kva()` 用于页描述符与地址转换。
- `page_ref()`、`page_ref_inc()`、`page_ref_dec()`：查询或修改物理页引用计数。
- `pte_create(ppn, perm)`：根据物理页号和权限构造表项；`tlb_invalidate(pgdir, la)`、`flush_tlb()` 用于映射更新后的地址翻译刷新。
- `check_pgdir()` 和 `check_boot_pgdir()` 是现有的页表检查入口；`pmm_init()` 会在启动阶段调用它们。

## [GUARANTEE]

保持现有公开接口与函数签名，完成：

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la,
                uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

需要时可以调整 `get_pte()` 和启动映射检查的局部实现，但不更改 `struct Page`、页表项类型和其他模块所使用的接口。`page_insert()` 的返回值应反映成功或失败，`page_remove()` 不应因为目标地址未映射而错误释放物理页。

## [SPECIFICATION]

### page_insert

- **Pre-Condition**：内存管理已初始化，提供的页表和物理页应有效；由函数负责处理可判断的无效参数。
- **Post-Condition**：目标虚拟页映射到指定物理页，页表项包含相应权限，物理页引用计数与实际映射一致。
- **Case 1**：原地址没有映射时，取得或创建页表项，建立新映射。
- **Case 2**：原地址映射到同一物理页时，更新权限，但不重复增加该页的引用计数。
- **Case 3**：原地址映射到另一物理页时，替换原映射，正确维护新旧物理页的引用计数。
- **Case 4**：页表分配失败时返回错误，不留下无用的中间页表，也不能破坏已有有效映射。
- **Requirements**：不能将启动阶段已有的超级页叶子当作下级页表继续访问；映射变化后按现有接口处理 TLB。

### page_remove

- **Pre-Condition**：传入待删除映射所在的页表和虚拟地址。
- **Post-Condition**：若存在映射，则清除对应页表项，减少物理页引用；引用归零时释放该页。
- **Case 1**：目标地址尚未映射时直接返回，不修改空闲页数量。
- **Case 2**：同一物理页被多个虚拟地址引用时，移除其中一个映射后不得提前释放物理页。
- **Requirements**：重复删除同一地址应当安全，不得发生重复释放。

### 验收要求

先运行原有 `check_pgdir()`、`check_boot_pgdir()`，核对其输出和资源计数；再结合评分用例检查映射替换、权限更新与解除操作。保留测试原本要验证的行为，不通过删除断言或跳过检查使启动通过。实现完成后核对 `git diff`，确保代码改动仍在 A1 的职责范围内。

---

# Lab4 A2：页表检查补充提示词

负责人：甘文杰；学号：2412700。

## [PROMPT]

### 任务

A1 已经完成页表映射操作，这一阶段主要补充对实现结果的检查。希望在原有页表测试的基础上，把容易遗漏的情况也覆盖到，尤其是重复映射、异常参数、映射删除和内存不足时的处理，确认这些操作不会破坏原有映射或资源状态。

### 操作要求

继续以 `code/kern/mm/pmm.c` 为主要修改文件，重点完善 `check_pgdir()` 的检查内容。先阅读原有断言，保留已有的测试顺序和清理流程，再补充必要的边界场景。`check_boot_pgdir()` 只需核对 A1 的修改能否正常执行，没有明确问题时不再调整。

本阶段以测试为主，不重新设计 `page_insert()`、`page_remove()`，也不改动 `tools/test_lab4.c`、`tools/grade.sh` 等评分文件。如果检查确实暴露出实现缺陷，应先记录触发条件，再作小范围修复。

### 输出要求

说明新增检查覆盖了哪些情况，运行前后空闲页数量是否一致，以及是否引入新的编译或启动问题。测试没有通过时保留错误信息，不为了使断言通过而改变原有功能含义。

## [RELY]

### 已完成的映射功能

- `page_insert()` 已支持新建映射、相同物理页重新映射及替换已有映射。
- `page_remove()` 在删除映射时减少物理页引用计数，必要时释放物理页；未映射地址应当安全返回。
- `get_pte()` 按 Sv39 页表层级查找表项，负责下级页表的按需分配与失败处理。
- 原有 `check_pgdir()` 已包含两页映射、替换、删除和页表回收等检查；`check_boot_pgdir()` 包含启动映射检查和实际内存访问。

### 检查手段

- `assert()` 用来核对映射结果和引用计数；`get_pte()`、`get_page()` 用来读取映射状态。
- `nr_free_pages()` 反映当前空闲物理页数，可以在操作前后记录并比较。
- `alloc_page()`、`alloc_pages(n)`、`free_page()`、`free_pages(base, n)` 用于构造测试条件和恢复环境。
- `-E_INVAL`、`-E_NO_MEM` 对应无效操作与内存不足时的失败结果。
- 测试运行在 `pmm_init()` 期间，需要保持后续 `kmalloc_init()`、`vmm_init()` 和线程初始化所依赖的页表、内存分配器状态。

## [GUARANTEE]

保持 A1 的公开接口不变，主要完善以下已有检查函数：

```c
static void check_pgdir(void);
static void check_boot_pgdir(void);
```

本阶段的代码应当能继续用于内核启动自检，不添加新的公开接口，也不改变评分入口。新增测试申请的物理页必须在测试结束前得到正确回收。

## [SPECIFICATION]

### check_pgdir

- **Pre-Condition**：物理页分配器和启动根页表已经初始化，当前测试占用的临时映射可在结束时清理。
- **Post-Condition**：原有映射、替换和释放检查继续通过；新增检查不会留下映射或物理页泄漏，空闲页数恢复到测试开始时的状态。
- **Case 1**：检查空指针等无效参数的返回结果，确认失败操作不影响物理页引用计数。
- **Case 2**：在启动超级页覆盖的地址上尝试建立普通映射，确认操作被拒绝且原映射保持不变。
- **Case 3**：对同一物理页重复插入并更新权限，确认引用计数和空闲页数量均不产生额外变化。
- **Case 4**：删除一个与其他地址共享物理页的映射，再重复删除相同地址，确认保留下来的映射不受影响。
- **Case 5**：构造下级页表分配不足的情况，检查失败返回、已分配页表的回滚和资源恢复。
- **Requirements**：在原有测试结束位置清理本次资源，不影响之后的 `check_boot_pgdir()` 与内核初始化；测试不应借助伪造返回值通过。

### check_boot_pgdir

- **Pre-Condition**：`check_pgdir()` 已结束并恢复原有空闲页状态。
- **Post-Condition**：启动阶段的内核地址映射仍可检查，原有临时别名映射的访存测试能够正常结束。
- **Requirements**：优先保留 A1 的现有实现，如发现问题再说明涉及的页表层级或地址转换原因。

### 验收要求

至少核对 `check_alloc_page() succeeded!`、`check_pgdir() succeeded!` 和 `check_boot_pgdir() succeeded!` 的真实输出，并观察内核是否继续进入后续初始化。使用 `make`、`make qemu` 复现检查；如当时 B/C 尚未完成，应记录其对完整测试的影响，待合并后再运行 `make grade`。测试结果以实际日志为准。

---

# Lab4 A：交叉验证提示词

负责人：甘文杰；学号：2412700。

## [PROMPT]

### 任务

A 部分已经完成，接下来按小组分工参与交叉验证。主要检查 C 负责的进程调度与上下文切换是否和 B 的线程创建正确衔接，并独立复测 B 的初始化结果。希望从实际执行过程和测试输出出发，确认各模块合并后没有接口或运行顺序上的问题。

### 操作要求

以三人代码已经合并的 `lab4` 分支为对象，先阅读 `proc.c`、`sched.c`、`switch.S` 和相关启动、Trap 代码，梳理从 `proc_init()` 到第一次运行 `init_main()` 的调用关系；之后检查多次调度时当前进程、寄存器现场和中断状态的变化。

使用原有构建和评分方法进行独立测试，不修改 B、C 的实现来掩盖问题，也不改动评分脚本。若发现问题，记录具体函数、触发条件和运行结果，交由对应负责人确认后再修订。

### 输出要求

给出一份简短的交叉验证记录，包含检查的主要接口、执行命令、实际结果和发现的问题。通过的部分写明依据；没有实际执行的检查不要写成已通过。

## [RELY]

### 进程创建与首次运行

- `proc_init()` 初始化 `idleproc`、`initproc` 和全局进程索引。`current` 初始指向 idle，随后由 `cpu_idle()` 进入调度过程。
- `kernel_thread()`、`do_fork()`、`copy_thread()` 构造新线程的内核栈、Trapframe 与 Context。
- `forkret()`、`forkrets()`、`__trapret`、`kernel_thread_entry` 负责将新线程的初始现场恢复为实际执行流。
- `struct context` 保存 `ra`、`sp` 和 `s0` 至 `s11`；Trapframe 保存更完整的寄存器状态，两者职责不同。

### 调度与验证接口

- `schedule()` 按 `proc_list` 查找 `PROC_RUNNABLE` 线程；`proc_run()` 更新 `current`、根页表状态，并通过 `switch_to()` 切换 Context。
- `switch_to()` 的寄存器存取偏移应与 `proc.h` 中 `struct context` 的字段顺序一致。
- `local_intr_save()`、`local_intr_restore()` 用于保存和恢复本地中断使能状态。要同时检查首次切换和线程再次切回后的行为。
- `tools/test_lab4.c` 包含 `test_lab4_process_create()` 和 `test_lab4_schedule()`，`make grade` 会在临时副本中执行这些测试。
- 现有 `do_exit()` 在 `init_main()` 返回后仍为 panic 占位。分析运行日志时应区分正常进入线程函数与此后尚未实现的退出流程。

## [GUARANTEE]

本阶段不新增功能接口，检查范围集中在以下已有实现：

```c
void proc_init(void);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
void cpu_idle(void);
void schedule(void);
void proc_run(struct proc_struct *proc);
void switch_to(struct context *from, struct context *to);
```

重点审查 C 的调度模块，同时独立复测 B 的创建与初始化模块。保留 A 已通过的页表检查作为完整启动链的前置条件。交付的是审查记录和测试证据，不直接修改其他成员的核心实现。

## [SPECIFICATION]

### 进程创建复测

- **Pre-Condition**：A、B、C 的代码已经合并，内核能够完成内存管理初始化。
- **Post-Condition**：`idleproc` 和 `initproc` 的 PID、状态、内核栈、父子关系及首次运行 Context 与本实验预期一致。
- **Requirements**：使用原有创建测试独立复测，并核对 `initproc` 确实能进入 `init_main()`，不只依据 PCB 静态字段判断完成情况。

### 调度代码审查

- **Pre-Condition**：进程链表中已有可运行线程，`current` 指向当前执行进程。
- **Post-Condition**：调度选择、`current` 更新和上下文恢复能够正确配合；线程让出 CPU 后，重新被调度时可以从原先的执行位置继续运行。
- **Case 1**：从 `idleproc` 发起第一次调度时，应能选择 `initproc`。
- **Case 2**：存在多个可运行线程时，应能轮流选中线程，不能始终运行同一个线程。
- **Case 3**：没有其他合适线程时，检查对当前线程或 idle 的处理是否正确，不出现无效链表访问。
- **Requirements**：核对 `switch_to()` 保存与恢复的寄存器集合、字段偏移及 `proc_run()` 的中断保护范围；结合实际代码判断，不假设已经实现时钟抢占。

### 综合运行验证

- **Pre-Condition**：编译环境与 QEMU 配置满足该项目的既有运行要求。
- **Post-Condition**：内存管理、进程创建和调度三个评分项均有对应的真实输出，且能说明线程创建后确实发生了调度和执行。
- **Case 1**：执行 `make` 与 `make qemu`，检查内存初始化、`initproc` 运行和异常输出发生的时机。
- **Case 2**：执行 `make grade`，核对 PMM、Process Create 和 Schedule 的独立测试结果，并保存完整评分日志。
- **Requirements**：若需要指定项目使用的 QEMU 路径或编译兼容参数，应在记录中写明实际命令；不要只保留最终分数而省略运行环境和异常信息。

### 验收要求

检查完成后记录所检查的提交或分支、相关文件、测试命令及结果。重点说明 B 创建的线程是否能够被 C 正确调度，以及重复切换能否返回原执行点。若综合评分为 `30/30`，应附上日志依据；若还有问题，分别列出属于 A、B、C 或原始框架的部分，再由小组讨论处理。
