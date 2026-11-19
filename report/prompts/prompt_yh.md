# Lab4 B：进程创建与初始化提示词

负责人：向宇航；学号：2413318。

验证结论见 [Lab4_B1_完成记录.md](../records/Lab4_B1_完成记录.md)。

## [PROMPT]

### 任务

请在本项目中实现 Lab4 的进程创建与初始化模块，让内核能够把启动执行流登记为 `idleproc`，并创建拥有独立内核栈、可被后续调度器执行的 `initproc`。该任务包括 PCB 初始化、线程资源管理、进程索引登记和首次执行现场构造，六个接口共同交付这一能力。

### 操作要求

请读取工程并修改实际文件 `code/kern/process/proc.c`，仅补全 `LAB4 function2` 区域；保留实验标记，将该区域 `YOUR CODE` 替换为 `2413318`。保留已有 PID 分配、哈希索引、内核栈分配与释放、线程恢复入口、名称管理和 `init_main` 等逻辑。

不修改 `LAB4 function3`、调度器、切换汇编、页表实现、公共结构体布局、评分脚本或评分断言。不增加完整用户进程地址空间复制、进程退出回收或其他无关功能。

### 输出要求

依据下述上下文和规格实现全部交付接口；遇到上下文与源码不一致时，以工程真实定义为准并说明差异。完成后审查资源和状态约束，编译目标文件，尝试完整构建，如实说明依赖未完成造成的验证限制，不用空桩或伪造 PASS 绕过测试。

## [RELY]

### 数据布局与全局状态

以下定义来自 `proc.h` 和 `trap.h`，字段顺序影响恢复现场，不可修改。

```c
enum proc_state {
    PROC_UNINIT = 0, PROC_SLEEPING, PROC_RUNNABLE, PROC_ZOMBIE
};

struct context {
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

struct proc_struct {
    enum proc_state state;
    int pid;
    int runs;
    uintptr_t kstack;
    volatile bool need_resched;
    struct proc_struct *parent;
    struct mm_struct *mm;
    struct context context;
    struct trapframe *tf;
    uintptr_t pgdir;
    uint32_t flags;
    char name[PROC_NAME_LEN + 1];
    list_entry_t list_link;
    list_entry_t hash_link;
};

struct pushregs {
    uintptr_t zero, ra, sp, gp, tp, t0, t1, t2;
    uintptr_t s0, s1, a0, a1, a2, a3, a4, a5, a6, a7;
    uintptr_t s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    uintptr_t t3, t4, t5, t6;
};

struct trapframe {
    struct pushregs gpr;
    uintptr_t status;
    uintptr_t epc;
    uintptr_t badvaddr;
    uintptr_t cause;
};
```

- `PROC_NAME_LEN=15`，`MAX_PROCESS=4096`，`MAX_PID=MAX_PROCESS*2`。进程数量包含 idle；普通进程 PID 在 `(0, MAX_PID)` 内。
- `proc_list` 保存普通进程；`hash_list[HASH_LIST_SIZE]` 通过 PID 索引普通进程；`HASH_LIST_SIZE=1<<10`。idle 不加入这两个索引。
- `idleproc`、`initproc`、`current` 初始为 `NULL`；文件内 `static int nr_process=0`。
- `boot_pgdir_pa` 是内核根页表的物理地址；PCB 的 `pgdir` 使用该地址。`kstack` 是内核虚拟地址，不得混用两类地址。
- `bootstack` 是启动栈底；普通线程栈大小为 `KSTACKSIZE`，由 `KSTACKPAGE` 个物理页组成。
- 本实验只创建共享内核地址空间的线程，`current->mm==NULL`。

### 可复用接口与语义

- `kmalloc(size_t)` / `kfree(void *)`：分配和释放 PCB；分配失败返回 `NULL`。
- `setup_kstack(proc)` / `put_kstack(proc)`：分配和释放普通线程内核栈；前者成功设置 `kstack` 并返回 0，失败返回 `-E_NO_MEM`。
- `get_pid()`、`hash_proc(proc)`、`find_proc(pid)`：分配唯一 PID、登记哈希表和查询已登记进程。
- `list_init()`、`list_add()`、`memset()`、`memcpy()` 可直接使用。
- `wakeup_proc(proc)` 要求原状态既非 `PROC_ZOMBIE` 也非 `PROC_RUNNABLE`，调用后状态为 `PROC_RUNNABLE`。
- `local_intr_save(flag)` 保存并关闭本核中断，`local_intr_restore(flag)` 恢复先前使能状态。当前为单核实验，共享 PID 与索引登记需要中断保护。
- `set_proc_name(proc, name)` 清空目标名字后固定复制 `PROC_NAME_LEN` 字节；调用方须提供至少这么多可读字节的源缓冲区。
- `CLONE_VM=0x00000100`。`E_NO_MEM=4`、`E_NO_FREE_PROC=5`，接口返回负错误码。

### 首次执行约定

- `forkret()` 调用 `forkrets(current->tf)`；后者以 Trapframe 为恢复栈，进入 `__trapret`。
- `kernel_thread_entry` 从 `s0` 取目标函数、从 `s1` 取参数，将参数传入 `a0` 后调用目标函数；函数返回后调用现有 `do_exit()`。
- `read_csr(sstatus)` 可读取特权状态；`SSTATUS_SPP=0x100`、`SSTATUS_SPIE=0x20`、`SSTATUS_SIE=0x2`。
- `init_main(void *)` 使用传入字符串，预期输出 `Hello world!!`；原 `do_exit()` 仍是 panic 占位，不属于本次任务。

## [GUARANTEE]

必须实现以下全部接口，保持签名和链接可见性：

```c
static struct proc_struct *alloc_proc(void);
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc);
static void copy_thread(struct proc_struct *proc, uintptr_t esp,
                        struct trapframe *tf);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);
void proc_init(void);
```

这些接口分别交付干净 PCB、内核线程地址空间语义、合法初始现场、完整线程创建、函数到线程的包装和启动进程初始化。允许新增 `static` 辅助函数，仅供当前模块内部使用，不改变公开接口或数据布局。

## [SPECIFICATION]

### alloc_proc

- **Pre-Condition**：内核堆和启动页表已可用。
- **Post-Condition**：成功返回独立 PCB，状态为 `PROC_UNINIT`、PID 为 -1、`pgdir=boot_pgdir_pa`；计数、内核栈、调度标记、父指针、`mm`、Trapframe、Context、标志和名字全部为零；两个链表节点分别初始化为独立空环。PCB 尚未占用 PID，也未登记或唤醒。
- **Case 1**：堆分配失败时返回 `NULL`，全局进程状态不变。

### copy_mm

- **Pre-Condition**：`current` 和新 PCB 有效；当前模块仅支持内核线程。
- **Post-Condition**：`proc->mm=NULL`，保留共享内核页表，不分配或复制用户地址空间，返回 0。
- **Case 1**：`current->mm!=NULL` 时通过断言拒绝当前阶段不支持的使用方式。
- **Requirements**：保留 `clone_flags` 参数和接口形态，为后续实验扩展留出位置。

### copy_thread

- **Pre-Condition**：新 PCB 已持有完整内核栈，`tf` 指向可读的初始 Trapframe。
- **Post-Condition**：新线程持有位于自身栈顶下方、完整落在栈内的 Trapframe 副本；源帧不变，子线程 `a0=0`，其余现场按输入继承。`context.ra` 指向 `forkret`，`context.sp` 指向新线程 Trapframe，其余 Context 字段保持初始化值。
- **Case 1**：`esp=0` 时，恢复后的线程栈指针为自身内核栈顶。
- **Case 2**：`esp!=0` 时，恢复后的线程栈指针为传入值。
- **Requirements**：遵守现有 RISC-V 64 位恢复布局及栈对齐约束，不改变汇编偏移。

### do_fork

- **Pre-Condition**：进程管理结构已初始化，`current` 有效，`tf` 有效；运行在本实验单核环境，不从中断处理程序并发创建进程。
- **Post-Condition**：成功返回新线程 PID；线程父指针为调用时的 `current`，具有独立内核栈及完整初始现场，共享内核页表；在链表和哈希表中各登记一次，数量增加一次，最终为 `PROC_RUNNABLE`。调用者的执行现场不变。
- **Case 1**：已有进程数量达到 `MAX_PROCESS` 时返回 `-E_NO_FREE_PROC`，不申请资源或改变登记状态。
- **Case 2**：PCB 或内核栈申请失败时返回 `-E_NO_MEM`；本次已取得的资源全部释放，数量和索引不变。
- **Case 3**：地址空间辅助接口返回错误时，传播该负错误码并回收本次 PCB 和内核栈，不留下登记项。
- **Requirements**：新线程准备完整后才能公开登记和唤醒；PID 分配、哈希及链表登记、数量更新构成受中断保护的区域，结束后恢复原中断状态。不得释放父线程资源或未取得的栈，不为失败创建消耗 PID。

### kernel_thread

- **Pre-Condition**：线程创建设施可用，`fn` 是有效的内核函数入口，`arg` 在新线程使用期间保持有效。
- **Post-Condition**：新线程首次运行时能够经过现有恢复链进入 `kernel_thread_entry`，以 `arg` 调用 `fn`；初始帧中 `s0=fn`、`s1=arg`、`epc=kernel_thread_entry`，SPP 与 SPIE 置位、SIE 清零，其余未指定寄存器清零。传递的创建标志包含调用者标志和 `CLONE_VM`。
- **Case 1**：创建成功返回新线程 PID；失败透传 `do_fork` 的负错误码。
- **Requirements**：临时帧在返回前复制到线程私有栈，不能留下指向局部帧的悬挂指针。

### proc_init

- **Pre-Condition**：内存管理已初始化，启动栈可用；本函数仅在启动期调用一次，尚无已登记进程。
- **Post-Condition**：全局进程链表及哈希表可用，进程数量为 2。`current==idleproc`；idle 的 PID=0、状态为 `PROC_RUNNABLE`、栈为 `bootstack`、`need_resched=1`、名字为 `idle`，父指针、`mm`、`tf`、`runs`、标志和 Context 保持空初始值。`initproc` 的 PID=1、状态为 `PROC_RUNNABLE`、父指针为 idle、名字为 `init`、`mm=NULL`，具有独立内核栈及合法首次执行现场，且可通过 `find_proc(1)` 找到；目标函数为 `init_main`，参数为 `Hello world!!`。
- **Case 1**：idle 分配失败或 init 创建失败时 panic，不能作为初始化成功继续执行。
- **Requirements**：idle 复用启动现场，不为其申请普通线程栈，不将其加入普通进程索引。名称设置不得越界读取源字符串。

### 验收要求

目标文件应能使用工程现有工具链编译。结合原始 `test_lab4_process_create()` 核对初始化结果，并审查未直接覆盖的索引、现场布局和资源失败路径。完整构建或运行若被未完成的 C 模块阻塞，应保留真实诊断；C 合入后使用原始评分测试验证首次执行和线程调度，不能把静态审查写成运行通过。
