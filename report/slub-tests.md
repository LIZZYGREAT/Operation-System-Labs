# 简化 SLUB 内核测试

## 开关与运行

测试源文件为 `code/kern/mm/slub_test.c`，公开入口为 `slub_check()`。
`SLUB_TEST=1` 才编译测试逻辑及静态控制表，并在 `pmm_init()`、
`slub_init()` 完成后执行。入口还比较 `slub_init()` 前后空闲页数。
默认不定义此宏，不调用测试，也不分配测试控制表。

建议从 `code` 目录使用独立的构建目录，避免 Makefile 不跟踪编译参数变化而
复用另一种开关状态的目标文件；下列目录位于已有的 Git 忽略目录内。

```bash
cd code
# 开启并编译
make OBJDIR=obj/slub-test BINDIR=bin/slub-test DEFS=-DSLUB_TEST=1 -j2
# 开启并运行；Ctrl-A X 可退出 QEMU
make OBJDIR=obj/slub-test BINDIR=bin/slub-test DEFS=-DSLUB_TEST=1 qemu
# 或限时运行并记录串口输出
timeout 20s make OBJDIR=obj/slub-test BINDIR=bin/slub-test DEFS=-DSLUB_TEST=1 qemu > /tmp/slub-test.log 2>&1

# 关闭：返回默认构建目录，不传 SLUB_TEST
make -j2
make qemu
```

如果此前曾在默认 `obj`、`bin` 目录启用测试，应先 `make clean` 再默认构建。
若在同一个测试构建目录改变宏，也应先清理该目录或使用全新构建目录。
内核成功后仍无限空转，因此 `timeout` 返回 124 并不表示测试失败；必须核对日志。
只有出现全部九个逐项 PASS 及最终标记，且没有 panic，才能报告本组测试通过。
任何断言失败均 panic，日志包含场景、步骤和表达式；数据错误另含槽位、字节偏移、
预期值与实际值。失败后不会继续打印最终通过标记。

`make grade` 的原有评分用例不包含这些 SLUB 测试，不能代替此验证。
本次未修改 `tools/inject.py`、评分用例或原有 PMM 框架测试，也未运行 `make grade`。

## 实现与第一份规格的核对

实际 `slub.c` 使用八个规定类别，循环选择第一个不小于请求的类别；
大于 2048 字节使用 `alloc_pages()`，计入头部后检查加法溢出并向上取页数。
通过 `page2pa(page) + va_pa_offset` 返回内核地址，没有使用被注释的 `KADDR()`。
头部按 16 字节取整，独立于用户对象区域；空闲槽才存放空闲链指针。
部分空闲页按类别管理，满页摘链，最后一个对象释放时立即归还底层。
大对象保存实际页数，释放时恢复对应 Page 并归还全部页面。
初始化仅初始化静态链表，不重置底层分配器，也没有递归申请元数据。

此次静态核对及运行没有发现违反第一份规格的问题，未修改 SLUB 实现。
当前 RV64 布局头部取整为 64 字节，2048 字节类别每页只能放一个对象；
其余类别每页能放多个对象。共享页测试使用 16、32、64、128 字节对象，
没有把“每个小对象一页”误作正确行为。

## 场景、断言与错误检出能力

所有场景以当前 `nr_free_pages()` 为基线，结束时释放所有测试对象并精确比较。
控制记录为 160 项静态对象表；每项保存原始指针、请求长度和数据种子。
临时物理页表容量由工程支持的物理地址范围计算，当前为 32768 项，
不代表或硬编码当前实际空闲页数。测试不使用宿主 malloc，也不用 SLUB 申请控制数据。

| 场景 | 操作 | 预期与主要断言 | 可发现的错误 |
| --- | --- | --- | --- |
| 1 零大小与 NULL | 空状态及有存活对象时调用 alloc(0)、free(NULL)，再正常申请 | 返回 NULL，页数不变，存活数据正确，后续分配有效 | 零大小消耗页面、NULL 释放破坏状态 |
| 2 类别边界 | 同时保留 1 及全部八个边界的前、当、后代表值，共 25 个对象 | 返回有效内核地址、16 字节对齐；完整请求范围逐字节写读；范围两两不重叠 | 边界越界、错误向下取整导致空间不足、对齐及地址映射错误 |
| 3 隔离与保持 | 11 个混合大小对象共存，交错释放偶数槽并换大小重分配 | 每步验证所有存活对象的完整数据和两两范围 | 对象别名、空闲链覆盖活对象、释放破坏邻居、大小混淆 |
| 4 释放复用 | 16 个 32 字节对象共享一页，隔个释放，再申请 8 个同类对象 | 部分释放保持占页数，再申请不增占底层页；无需地址完全相同 | 丢失空闲槽、过早回收、复用时无谓申请新页 |
| 5 多 slab 与共享 | 80 个 128 字节对象共存，按置换顺序释放；再分配 16 个 16 字节对象 | 总请求超过一页；返回地址属于多个不同页，实际扣页等于不同页数；16 个小对象仅扣一页 | 满 slab 扩展失败、跨页别名、每对象独占一页、多 slab 回收泄漏 |
| 6 立即回收 | 两个同页 64 字节对象及另一个 512 字节对象存活，逐个释放 | 第一次释放页数不变；最后一个同页对象释放立即多一空闲页；另一个类别继续完整有效 | 页过早归还、延迟回收、计数错误、跨类别串扰 |
| 7 大对象 | 2049、PGSIZE-1、PGSIZE、PGSIZE+1、2*PGSIZE+123 同时共存并完整写读；再逐个独立申请释放 | 扣页至少覆盖请求长度及返回地址实际跨越的页；直映范围内页面未被标为空闲块首；各次独立释放恢复其前基线 | 未计元数据引起越界、截短用户区域、多页别名、少还或多还页 |
| 8 失败与溢出 | 超可用页请求、size_t 最大值及附近三种请求；保留小/大对象，用 PMM 占满剩余页；零页时尝试新类别和大对象，再只还一页 | 所有失败返回 NULL 且页数、活对象不变；零页时已有 64 类空位仍能分配；一页不足以满足跨两页请求，却能建立 128 类 slab；临时页全部归还后能再申请大对象 | 溢出误成功、失败泄漏、失败破坏元数据、错误回滚、耗尽后不能恢复 |
| 9 重复混合 | 384 轮固定算式选择槽位和大小，至多使用 32 个活槽，轮末统一清理 | 每次申请/释放及每轮验证存活数据、区间隔离，最终精确恢复页数 | 长期链表损坏、累积泄漏、类别切换后错误复用 |

模式依赖对象种子及字节位置，使用 volatile 字节访问读写完整请求长度，
并非只检查首字节或只检查指针不同。新分配写入前先检查与旧对象的区间隔离。
测试不读取 SLUB 私有链表、头部结构或私有大小常量；也不复制实现的头部大小公式
生成“大对象应占几页”的预期。页面扣除数由前后计数观测，释放必须恢复该次基线。
特别是大对象跨页下界来自返回地址及请求长度，独立于分配器内部布局。

这些检查同时观察两层行为：多个小对象共享底层页证明存在对象分配层；
页不足时已有 slab 的空位仍可使用，证明对象容量与物理页容量不同；
完整数据和区间比较检验对象隔离；部分释放、末对象释放及场景基线比较检验页生命周期。

## 本次实际运行记录（2026-10-22）

实际测试构建使用了独立目录 `obj-slub-test`、`bin-slub-test`：

```bash
cd code
make OBJDIR=obj-slub-test BINDIR=bin-slub-test DEFS=-DSLUB_TEST=1 -j2
timeout 20s make OBJDIR=obj-slub-test BINDIR=bin-slub-test DEFS=-DSLUB_TEST=1 qemu > /tmp/ucore-slub-test.log 2>&1
```

编译链接成功，QEMU 实际输出的关键内容如下（摘录，不是预期输出）：

```text
check_alloc_page() succeeded!
[SLUB] START: baseline=31862, slots=160, mixed-rounds=384
[SLUB 1] PASS: free pages restored to 31862
[SLUB 2] PASS: free pages restored to 31862
[SLUB 3] PASS: free pages restored to 31862
[SLUB 4] PASS: free pages restored to 31862
[SLUB 5] PASS: free pages restored to 31862
[SLUB 6] PASS: free pages restored to 31862
[SLUB 7] size=2049 charged=1 payload-pages>=1
[SLUB 7] size=4095 charged=2 payload-pages>=1
[SLUB 7] size=4096 charged=2 payload-pages>=1
[SLUB 7] size=4097 charged=2 payload-pages>=2
[SLUB 7] size=8315 charged=3 payload-pages>=3
[SLUB 7] PASS: free pages restored to 31862
[SLUB 8] PMM exhaustion exercised: held=31859, small/large failures verified
[SLUB 8] PASS: free pages restored to 31862
[SLUB 9] PASS: free pages restored to 31862
[SLUB] ALL 9 CASES PASSED: baseline=31862 final=31862
```

其中 `payload-pages>=` 是仅按长度算的下界，另有按实际返回地址跨页数的断言。
4095/4096 字节实际占两页，说明本实现的元数据确实产生额外占页，并已验证完整回收。
受控耗尽路径本次实际执行，没有跳过或用超大请求替代。
运行达到预设超时后停止 QEMU，退出码为 124。

## 验证边界

- 本次运行环境为当前 QEMU virt、RV64、Best-Fit、默认 128 MiB RAM；没有运行 Buddy、
  其他内存配置或真实硬件，也没有测试并发、SMP、NUMA，这些不在模块契约内。
- 边界、共享与复用验证公开可观察行为；最小类别选择另由实际源码循环核对。
  不声称仅凭黑盒结果能证明所有内部类别决策或穷尽全部 size_t 输入。
- 大对象连续性结合工程已核对的线性直映和底层连续页接口、完整跨页读写验证；
  未独立遍历硬件页表或建立每页归属追踪器。PageProperty 只是一项辅助检查，
  不能单独证明任意页的完整所有权。
- 没有覆盖总空闲量足够但连续块不足的所有碎片化形态，没有对每个类别分别耗尽；
  小对象新 slab 失败和大对象页不足失败已实际触发。
- 没有测试重复初始化、重复释放、内部偏移或非法指针，未依赖新内存清零。
- 通过不代表所有可能操作序列都正确；没有使用重新初始化来恢复资源计数。
