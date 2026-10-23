# Buddy System：设计、运行与验证

负责人：王子楸，学号 2412712。首次验证日期：2026-10-21。合并同步日期：2026-10-23。

## 实现范围

- `code/kern/mm/buddy_pmm.c`、`buddy_pmm.h`：Buddy 的六个管理器回调、注册对象和基础自检。
- `code/kern/mm/pmm.c`：增加头文件、管理器选择和可选测试入口，原有 `page_init()` 实现保持不变。
- `code/Makefile`：增加 `PMM=first_fit|best_fit|buddy` 参数和默认关闭的独立测试开关；合并同步后默认使用 Best-Fit，显式 First-Fit 定义 `PMM_USE_FIRST_FIT`。
- `code/kern/mm/buddy_test.c`、`buddy_test.h`：独立测试与受控描述符环境。
- `code/tools/run_buddy_tests.sh`：固定使用 QEMU 4.1.1 的测试执行与结果核对脚本。

未修改公共结构体、First-Fit/Best-Fit 算法及固定测试、启动汇编、启动页表和评分脚本。

## 分配与释放约定

每阶一个空闲链表，阶 `k` 的块包含 \(2^k\) 个连续页，按绝对物理页号对齐。伙伴页号通过 `page2ppn(page) ^ ((size_t)1 << k)` 计算；只有同阶空闲伙伴能够合并。

申请向上取整到最小的二次幂；释放传原始申请页数。例如：

```c
struct Page *p = alloc_pages(3);  // 实际占用 4 页，空闲页数减 4
if (p != NULL) {
    free_pages(p, 3);            // 整个块归还，空闲页数加 4
}
```

不支持部分释放，也不能将上述释放长度改为 4。分配失败返回 `NULL`；零长度、错误释放长度、重复释放等调用错误使用断言报告。

空闲块首页的 `PageProperty` 为 1，`property` 记录块页数；已分配首页使用模块私有的 `flags` 第 2 位标记，`property` 记录原始请求。内部页的首页标记与 `property` 清零。总空闲页数单独统计，拆分和合并不重复改变总数。

初始区域可分解为多个符合对齐的块，支持非二次幂长度和多次登记互不重叠的区域。管理器 `init()` 早于 `page_init()`，因此最大阶由框架支持范围推导，不依赖当时尚未建立的描述符数组。本实现面向单 hart 串行调用。


## 运行与切换

在 WSL 终端执行，以下命令以仓库的 `code/` 为工作目录：

```bash
cd /mnt/a/OS/exp/Operation-System-Labs/code
QEMU411="$HOME/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64"
"$QEMU411" --version
make clean
make PMM=buddy
make qemu PMM=buddy QEMU="$QEMU411"
```

看到输出后，用 Ctrl+A、松开，再按 X 退出 QEMU。切换后端必须先 `make clean`：

```bash
# Best-Fit
make clean
make qemu PMM=best_fit QEMU="$QEMU411"

# First-Fit（必须显式选择）
make clean
make qemu PMM=first_fit QEMU="$QEMU411"

# 恢复默认 Best-Fit
make clean
make qemu QEMU="$QEMU411"
```

merge 提交 `c86c559` 将 `pmm.c` 的选择顺序调整为 Buddy → 显式 First-Fit → 默认 Best-Fit，同时仅简化了 `page_init()` 的贡献注释，未改变其逻辑。此次同步将 Makefile 的默认 `PMM` 改为 `best_fit`，并为 `PMM=first_fit` 补充 `PMM_USE_FIRST_FIT`，使构建参数与管理器选择一致。未指定参数时选择 `best_fit_pmm_manager`；显式 `PMM=first_fit` 选择 `default_pmm_manager`。基础评分可使用下列命令，但本次同步不运行评分，不能将它写成新增通过记录：

```bash
make grade PMM=best_fit QEMU="$QEMU411"
```

评分流程会调用 Python 并注入固定检查，执行前遵守本机 Python 命令的权限约定。Best-Fit 选择检查用于基础提交，不作为 Buddy 正确性的证明。

## 合并前实际验证结果（2026-10-21）

以下为合并前版本的历史记录，不能视为当前 merge 版本的运行结果。以下结果全部使用已安装的 QEMU 4.1.1、其默认 OpenSBI 0.4 和原有 `make qemu` 启动方式获取，未使用额外 `-kernel` 参数、兼容脚本或调试器改动执行位置。

| 检查 | 实际结果 |
|---|---|
| Buddy 干净构建及启动 | 通过，输出 `memory management: buddy_pmm_manager` |
| Buddy 基础自检 | 通过，输出 `buddy_check: passed (free pages restored: 31929)` |
| 公共检查入口 | 输出 `check_alloc_page() succeeded!` |
| Best-Fit 干净构建及原有自检 | 通过，选择 `best_fit_pmm_manager` |
| 合并前默认 First-Fit 干净构建及原有自检 | 通过，选择 `default_pmm_manager` |
| 非法 `PMM=invalid` | 构建阶段按预期拒绝 |
| 受保护源文件与差异检查 | 原有算法、启动代码和固定测试未修改；`git diff --check` 通过 |

当时的启动自检回归覆盖 Buddy、Best-Fit 和默认 First-Fit。旧日志不再随仓库保存；当前脚本生成的是合并同步后的新日志，覆盖范围见下文。

基础自检在两轮申请/释放中检查单页、申请 3 页实际占用 4 页、块不重叠、回收复用，以及各阶块的对齐、首页标记、链表和空闲计数一致性；另检查超大请求失败且不改变空闲计数。

上述为第一份提示词实现阶段的结果，当时恢复了默认 First-Fit 构建。本次第二份提示词的独立测试结果如下；另一名成员交叉复现和基础评分仍未执行。

## 真实问题与处理

最初误用了 WSL 中默认的 QEMU 7.0.0，与课程环境不一致。排错时曾增加临时的 GDB 启动兼容文件；该过程不能作为规定环境下的正式验收。用户指出版本要求后，已安装 4.1.1，删除 `buddy_boot_compat.gdb`、`run_pmm_compat.sh` 及对应日志，并直接按原启动流程重新取得上面的验证结果。

WSL 首次从官网下载时出现 TLS 连接错误，随后通过 Windows 下载同一官方源码包，再在 WSL 中编译安装。源码包 SHA-256 为 `ed6fdbbdd272611446ff8036991e9b9f04a2ab2e3ffa9e79f3bab0eb9a95a1d2`，用于标识本次实际使用的文件。

## 第二份提示词：合并前独立测试结果（2026-10-21）

实际使用 QEMU 4.1.1、默认 OpenSBI 0.4、RISC-V GCC 14.2.0。下文保留本次结果摘要；重新运行脚本后，`report/buddy_tests/environment.log` 记录运行环境，`summary.log` 记录核对结果，其余日志记录构建和串口输出。

### 测试隔离与判断方法

仅在 `PMM=buddy BUDDY_TEST=1` 时，在管理器初始化之后、真实 `page_init()` 之前调用测试入口。测试使用 64 个静态描述符，临时替换 `pages`、`npage`，通过真实管理器回调和公共申请/释放接口操作；不读写模拟页号所对应的物理内存。

预期结果由独立逐页归属表维护，区分保留、空闲和已分配块，按请求长度计算实际占用。每次操作核对页面覆盖、不重叠、保留页保护、首页标记、对齐、链表双向连接及空闲总数。每个正常用例归还所有分配；结束后清空测试管理状态并恢复原全局变量，再继续正常启动。

### 正常用例

| 用例标识 | 验证内容与覆盖理由 | 实际结果 |
|---|---|---|
| `init_aligned` | 登记 16 页、逐页耗尽并回收，检查重复分配和丢页 | PASS |
| `init_unaligned_13` | 从第 3 页起登记 13 页并耗尽，检查起点对齐处理、非二次幂尾部和越界登记 | PASS |
| `init_adjacent_regions` | 分两次各登记 8 页，再申请完整 16 页，检查跨登记调用的合法合并 | PASS |
| `init_reserved_gap` | 登记两个间隔开的 4 页区间，申请 8 页失败，单页申请只能落在登记集合，检查跨保留间隙的错误合并 | PASS |
| `split_merge_nonbuddy` | 整块申请后拆成 16 个单页；只释放编号 1、2 时申请 2 页失败；补齐伙伴后申请 4 页，再完整回收申请 16 页，区分伙伴与普通相邻页 | PASS |
| `rounding_original_length` | 请求 1、2、3、5、8、9 页分别扣除 1、2、4、8、8、16 页；同时存活时检查重叠；另在仅有 4 页时申请 3 页后申请单页必须失败，检查取整余量保护和原长度释放 | PASS |
| `failure_preserves_state` | 空状态、耗尽、超容量、最大整数及高位请求均失败；8 个孤立空闲页不能满足 2 页请求，但仍可逐页分配并完整回收，检查失败路径未破坏状态 | PASS |
| `reuse_16_rounds` | 16 轮混合请求，按不同排列释放，每轮重新申请完整 64 页，检查泄漏、重复入链和合并遗漏 | PASS |

本次正常启动输出了八个 PASS，随后输出：

```text
[Buddy test] ALL PASS; fixture restored; continuing real page_init
Free physical pages: [0x000000008034a000, 0x0000000088000000), 31926 pages
buddy_check: passed (free pages restored: 31926)
check_alloc_page() succeeded!
```

启用测试时测试代码和静态数组增大内核镜像，因此本次真实可用页数为 31926；关闭测试时恢复为 31929。这一差异来自镜像占用，测试内的分配已全部归还。

### 非法调用用例

每个编号分别干净构建并独立启动。脚本同时核对用例标识、`buddy_pmm.c` 的 panic 位置和对应断言文本；QEMU 使用 Ctrl+A、X 正常退出。超时、其他位置崩溃或非法操作意外返回均判为失败。

| 编号 | 非法操作 | 预期且实际触发的断言 | 结果 |
|---|---|---|---|
| 1 | `alloc_pages(0)` | `n > 0` | 符合预期 |
| 2 | 对已释放块再次释放 | `!PageReserved(base) && !PageProperty(base)` | 符合预期 |
| 3 | 申请 3 页，却用长度 4 释放 | `(base->flags & BUDDY_ALLOCATED) && base->property == n` | 符合预期 |
| 4 | 释放分配块内部页 | 同上，内部页没有已分配首页标记 | 符合预期 |
| 5 | 重复登记已接管的页面 | `PageReserved(base + i)` | 符合预期 |
| 6 | 从第 60 页开始登记 8 页，超出 64 页范围 | `n <= npage - nbase - index` | 符合预期 |

重新运行脚本会生成 `report/buddy_tests/invalid_1.log` 至 `invalid_6.log`。这些 panic 是明确设计的拒绝非法调用行为，不能与正常用例应无 panic 的要求混淆。

### 复现命令

在 WSL 的仓库 `code/` 目录运行完整验证：

```bash
bash tools/run_buddy_tests.sh \
    "$HOME/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64"
```

脚本检查版本，每次切换配置前清理构建，依次运行正常测试、六个非法调用、关闭测试的 Buddy/显式 First-Fit/显式 Best-Fit/默认 Best-Fit 回归及四个非法构建配置检查。默认后端回归放在最后；检查全部通过后保留默认 Best-Fit 构建。回归日志分别为 `regression_buddy.log`、`regression_first_fit.log`、`regression_best_fit.log`、`regression_default.log`。

手动运行单个配置时：

```bash
QEMU411="$HOME/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64"
make clean
make qemu PMM=buddy BUDDY_TEST=1 BUDDY_TEST_CASE=0 QEMU="$QEMU411"
```

将编号 0 改成 1 至 6 可分别复现非法用例；编号 0 运行正常用例。每次修改参数前执行 `make clean`。默认 `BUDDY_TEST=0`，扩展测试不执行，也不链接测试用静态数组。

### 回归、限制与修正记录

- 合并前关闭扩展测试后的 Buddy、Best-Fit 和原默认 First-Fit 均正常启动并通过原有自检，日志为 `regression_*.log`，且不包含扩展测试输出。
- 不匹配的后端、非法用例编号、非法开关值，以及关闭测试却指定非法用例的配置，都在构建阶段被正确拒绝。
- 本轮没有发现 Buddy 算法缺陷，没有修改 `buddy_pmm.c`、`buddy_pmm.h` 或原有断言。新增代码及集成注释使用学号 2412712。
- 测试源码及脚本首次执行即通过，未发生需要修复的测试或算法错误；如实记录结果，不编造 bug 经历。
- `make grade`、另一名成员的独立复现、多核并发和实际物理页数据读写不在本轮验证结果中。该套测试针对现有接口契约和单 hart 页管理行为。
- 未自动提交或推送本次测试改动。


## 合并同步验证（2026-10-23）

本次使用 QEMU 4.1.1（`/usr/local/bin/qemu-system-riscv64`）、默认 OpenSBI 0.4 和 RISC-V GCC 10.2.0，执行：

```bash
bash -n code/tools/run_buddy_tests.sh
bash code/tools/run_buddy_tests.sh /usr/local/bin/qemu-system-riscv64
```

完整脚本退出码为 0，结果如下：

| 检查 | 本次结果 |
|---|---|
| 八个正常场景、环境恢复及真实 Buddy 自检 | 全部通过，公共自检成功 |
| 六个非法调用 | 分别在预期断言处 panic，全部符合预期 |
| `PMM=buddy`，关闭扩展测试 | `buddy_pmm_manager`，公共自检成功 |
| `PMM=first_fit`，关闭扩展测试 | `default_pmm_manager`，公共自检成功 |
| `PMM=best_fit`，关闭扩展测试 | `best_fit_pmm_manager`，公共自检成功 |
| 不传 `PMM`，关闭扩展测试 | `best_fit_pmm_manager`，公共自检成功 |
| 四个非法测试配置 | 全部在构建阶段按预期拒绝 |

四种后端回归均没有扩展测试输出或 panic。另通过 `make -Bn` 核对默认及三个显式后端的编译宏映射，并确认 `make -n PMM=invalid` 拒绝非法参数；脚本语法检查和 `git diff --check` 通过。

本次正常 Buddy 测试启用时登记 31926 页，关闭测试的四种回归均登记 31929 页；这是本次输出记录，脚本不将历史页数作为固定断言。脚本最终输出 `ALL VALIDATION PASSED; default Best-Fit build restored.`，保留默认 Best-Fit 构建。

当前运行日志位于 `report/buddy_tests/`，为本地生成的未跟踪文件；`environment.log`、`summary.log` 与 `regression_*.log` 均属于本次运行，不能作为合并前日志引用。未运行 `make grade`，未修改分配算法或固定测试。
