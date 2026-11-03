# B5-2：页初始化独立复现与反馈记录

> 执行日期：2026-10-23
>
> 分支：`work/lab2/ganwenjie`
>
> 代码基线：`3abc2cc`
>
> 审核范围：当前 Best-Fit 注册配置下的 `page_init()` 构建、启动和固定评分；没有修改 A 的代码、固定测试、评分脚本或分配器。

## 1. 执行前状态

执行前的 Git 状态为：

```text
## work/lab2/ganwenjie...origin/work/lab2/ganwenjie [ahead 5]
 M report/prompt.md
?? docs/
?? report/_work/
```

其中 `report/prompt.md` 和 `report/_work/` 是正在形成的 B5 记录；`docs/` 是既有未跟踪资料。`git diff --name-only -- code code/tools/test_lab2.c` 没有输出，说明实现源码和固定测试在验收前没有工作树修改。

当前位于个人工作分支，符合 `README.md:73-99` 的协作约定，没有直接修改或推送共享 `lab2` 分支。

环境检查：

| 工具 | 实际结果 |
| --- | --- |
| `make` | `/usr/bin/make` |
| QEMU | `/home/lenovo/projects/os-lab/qemu/current/bin/qemu-system-riscv64`，版本 4.1.1 |
| RISC-V GCC | `/home/lenovo/projects/os-lab/toolchain/current/bin/riscv64-unknown-elf-gcc`，版本 10.2.0 |

前置条件满足，可以执行当前代码的构建、QEMU 启动和固定评分。

## 2. 实际执行记录

### 2.1 构建

命令：

```sh
make -C code
```

退出码：`0`。

关键输出：

```text
make: Nothing to be done for 'TARGETS'.
```

该结果表示现有构建产物与当前源码一致，没有编译错误。

### 2.2 限时 QEMU 启动

命令：

```sh
timeout --foreground --signal=INT --kill-after=2s 8s make -C code qemu
```

退出码：`124`。这是内核完成初始化后停留在 `kern_init()` 的无限循环，由 `timeout` 主动结束，不是启动或断言失败。

关键日志：

```text
Physical Memory from DTB:
  Base: 0x0000000080000000
  Size: 0x0000000008000000 (128 MB)
  End:  0x0000000087ffffff

end    0xffffffffc0206078 (virtual)
memory management: best_fit_pmm_manager
Physical memory from DTB: [0x0000000080000000, 0x0000000088000000)
Managed physical memory end: 0x0000000088000000
Page descriptors: PA [0x0000000080207000, 0x0000000080347000)
Free physical pages: [0x0000000080347000, 0x0000000088000000), 31929 pages
check_alloc_page() succeeded!
```

日志与 B5-1 推导完全一致：内核结束地址之后布置描述符数组，描述符数组结束之前没有页被登记为空闲区，空闲区从下一个完整页 `0x80347000` 开始。

### 2.3 固定评分

命令：

```sh
make -C code grade
```

退出码：`0`。

关键输出：

```text
lab2 injection complete
-check page init: OK
-check best fit:  OK
Total Score: 20/20
```

`grade` 目标先执行 `clean`，但 `code/Makefile:205-212,220-223` 表明它只清理生成的 `obj/`、`bin/` 和评分临时输出，再把源码复制到 `/tmp/lab2_grade` 注入测试；没有删除或改写受版本控制的实现文件。评分结束后已重新执行 `make -C code`，完整重建退出码为 `0`。

### 2.4 执行后完整性检查

```sh
git diff --name-only -- code code/tools/test_lab2.c
```

无输出。执行前后均没有修改 `page_init()`、Best-Fit、固定测试或评分脚本，也没有残留 QEMU/GDB 进程。

## 3. Page Init 固定测试逐项复核

评分注入器把 `test_page_init()` 和 `test_best_fit()` 加到 `pmm_init()` 末尾：`code/tools/inject.py:146-169`。Page Init 的通过结果来自独立的 `test_page_init()`，不是用 `test_best_fit()` 的管理器指针检查代替。

| 固定检查 | 代码位置 | 本轮证据 | 结论 |
| --- | --- | --- | --- |
| `va_pa_offset` | `code/tools/test_lab2.c:19-23`；赋值在 `code/kern/mm/pmm.c:71` | Page Init 固定测试通过；B5-1 核对值为 `PHYSICAL_MEMORY_OFFSET` | 通过 |
| DTB 范围非零 | `code/tools/test_lab2.c:25-29`；来源在 `code/kern/driver/dtb.c:128-149` | QEMU 实际输出 base `0x80000000`、size `0x08000000` | 通过 |
| `npage` | `code/tools/test_lab2.c:31-41`；计算在 `code/kern/mm/pmm.c:88-95` | 固定测试通过；默认终点为 `0x88000000`，故 `npage=0x88000` | 通过 |
| `pages` 非空、页对齐且位于 `end` 后 | `code/tools/test_lab2.c:43-55`；布置在 `code/kern/mm/pmm.c:96-105` | `end PA=0x80206078`，描述符从页对齐的 `0x80207000` 开始 | 通过 |
| `nr_free` 合理 | `code/tools/test_lab2.c:57-65` | QEMU 实际登记 31929 页；固定测试通过 | 通过 |
| 内核尾页 Reserved | `code/tools/test_lab2.c:67-76` | 固定测试通过；初始化循环先将所有描述符设为 Reserved，空闲区晚于内核尾页 | 通过 |
| 描述符数组占用页 Reserved | `code/tools/test_lab2.c:78-89` | 固定测试确认数组起始物理页 Reserved；B5-1 静态检查确认整个数组位于 `free_begin` 之前 | 通过 |
| `page2pa()` / `pa2page()` 往返 | `code/tools/test_lab2.c:91-98`；转换在 `code/kern/mm/pmm.h:83-108` | 固定测试通过；B5-1 对内核尾页与首个空闲页进行了手工推导 | 通过 |

`test_best_fit()` 在 `code/tools/test_lab2.c:103-111` 只检查当前管理器指针；它不能证明页初始化正确。本轮结论以独立 Page Init 测试、QEMU 地址日志和静态区间核对三类证据共同支持。

## 4. First-Fit 基线的证据边界

当前正式代码注册 `best_fit_pmm_manager`。本轮没有直接改动 `init_pmm_manager()` 切换 First-Fit，也没有在共享工作树制造临时代码差异。

已有历史依据包括：

- Git 提交 `2ada293 feat: verify init function (#6)` 保存了 A2 初始化验证记录和截图；
- `report/report.md:172-176` 记录 First-Fit 基线曾输出 `memory management: default_pmm_manager` 和 `check_alloc_page() succeeded!`。

这些材料只能作为 A2 历史基线，不能声称是本轮独立重跑。B5 本轮的实际动态证据来自当前 Best-Fit 配置；由于 `page_init()` 通过统一 `init_memmap()` 接口交接空闲区，Page Init 固定测试仍独立验证了 A 的初始化结果。

## 5. 审核意见分类

### 通过

1. **默认 128 MiB DTB 布局：通过。**
   - 位置：`code/kern/mm/pmm.c:66-125`。
   - 输入：`[0x80000000, 0x88000000)`。
   - 预期：内核和描述符数组保持 Reserved，只登记描述符之后的完整页。
   - 实际：描述符区间 `[0x80207000, 0x80347000)`，空闲区间 `[0x80347000, 0x88000000)`，31929 页；QEMU 自检及 Page Init 固定测试均通过。
   - 影响：支持当前课程默认运行配置，无需修改。

2. **描述符容量和空闲页边界：通过。**
   - 位置：`code/kern/mm/pmm.c:98-123`。
   - 实际：先检查描述符容量，再计算数组末端；只有 `free_begin < managed_end` 时才以正页数调用 `init_memmap()`。
   - 影响：默认配置下没有越界或将管理结构自身交给分配器的证据。

3. **固定测试与 Best-Fit 自检职责可区分：通过。**
   - 位置：`code/tools/test_lab2.c:11-111`、`code/tools/grade.sh:231-244`。
   - 实际：评分分别报告 Page Init 与 Best-Fit 两项 OK，总分 `20/20`。

### 需 A 修复

当前测试配置下没有发现已复现的 A 代码缺陷，因此没有必须立即实施的最小修复要求。

### 证据不足 / 风险待验证

1. **DTB 内存终点超过 `PHYSICAL_MEMORY_END`。**
   - 位置：`code/kern/mm/pmm.c:88-94` 与 `code/tools/test_lab2.c:31-40`。
   - 风险：实现把 `npage` 截断到固定上限，测试却按原始 DTB 终点计算期望值。
   - 当前证据：仅静态发现契约不一致，没有构造更大 RAM 的失败用例，不能写成已复现 Bug。
   - 建议：由 A 确认实验是否只支持固定 128 MiB；若需要支持更大 RAM，再统一实现和测试的 `npage` 定义。

2. **异常或复杂 DTB。**
   - 位置：`code/kern/driver/dtb.c:39-100`。
   - 风险：当前解析器按两个 64 位 cell 读取首个内存区间，没有动态覆盖多段 RAM、不同 cell 数量、无效 magic/offset 等配置。
   - 当前证据：默认 QEMU DTB 通过；其他配置证据不足。

3. **低于 `nbase` 的 `pa2page()` 输入。**
   - 位置：`code/kern/mm/pmm.h:104-109`。
   - 风险：只检查 PPN 上界，没有显式检查下界。
   - 当前证据：`page_init()` 与固定测试传入的 PA 都在受管区间，没有实际失败；这不是当前 `page_init()` 的已复现问题。

4. **固定测试没有逐页检查整个描述符存储区。**
   - 位置：`code/tools/test_lab2.c:78-89`。
   - 风险：动态测试只检查描述符数组起始物理页 Reserved。
   - 当前证据：静态代码表明 `page_init()` 先保留全部描述符，且 `init_memmap()` 从数组末端之后开始，足以支持默认配置结论；仍缺少逐页动态断言。

## 6. 可选的最小独立测试建议

如 A 或团队希望扩大动态覆盖，可只在临时评分副本中增加以下断言，不改正式固定测试：

1. 根据 `PADDR(pages)`、`npage - nbase` 和 `sizeof(struct Page)` 计算描述符物理区间，逐页确认 `PageReserved(pa2page(pa))`。
2. 重新计算 `free_begin` 和 `free_count`，精确断言 `nr_free_pages() == free_count`，而不只检查非零和不超过总页数。
3. 在独立副本中分别用小于、等于和大于 128 MiB 的 QEMU RAM 配置验证截断契约；预先明确预期，不修改正式分支来迎合测试。

这些建议用于补足证据，不是当前默认配置通过 B5 审核的前置修复。

## 7. 可直接提交给 A 的反馈

> 已在 `work/lab2/ganwenjie` 对当前 `page_init()` 完成独立复现。`make` 退出 0；限时 QEMU 启动输出 DTB `[0x80000000,0x88000000)`、描述符 `[0x80207000,0x80347000)`、空闲区 `[0x80347000,0x88000000)` 共 31929 页，并到达 `check_alloc_page() succeeded!`；`make grade` 退出 0，Page Init 与 Best-Fit 均为 OK，总分 20/20。默认配置下没有发现需要修改的初始化缺陷。请确认实验是否只承诺固定 128 MiB 上限；若要支持更大 RAM，需要统一 `managed_end/npage` 与固定测试按 DTB 原始终点计算的预期。复杂 DTB、低于 `nbase` 的 `pa2page()` 输入及描述符区逐页动态检查目前属于证据不足，不作为已复现 Bug。审核未改动 `page_init()`、测试或分配器。

按团队要求，该反馈经 A 确认后，才能在最终报告或任务表中把 B5 标为正式交叉审核完成。
