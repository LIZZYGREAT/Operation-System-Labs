# Lab4 A1 页表映射与 A2 页表测试完成记录

负责人：Wenjie；学号：2412700。记录日期：2026-11-19。

## 实现范围

A1 在 `work/wenjie-a1` 上实现，并随 PR #24 合入 `lab4`；A2 是同一页表模块的测试补充，在 `work/wenjie-a2` 完成并随 PR #26 合入。相关代码位于 `code/kern/mm/pmm.c`。

- `page_insert()` 建立新映射、更新同页权限、替换不同物理页，并维护叶子页引用与 TLB；无效参数和上层超级页冲突返回 `-E_INVAL`，页表分配失败返回 `-E_NO_MEM`。
- `page_remove()` 清除映射、使对应 TLB 项失效、减少引用计数，并在引用归零时释放物理页；无映射和重复删除安全返回。
- `get_pte()` 遇到 Sv39 上层超级页时不将叶子误当页表；第二级页表分配失败时回滚本次新建的上级页表项和页表页。
- `check_boot_pgdir()` 使用 64 位物理地址范围核对 1 GiB、2 MiB 和 4 KiB 映射，并保留字符串别名的真实读写检查。

## A2 页表自检补充

在原有 `check_pgdir()` 中增加了无效参数、同页同地址更新权限、别名删除后映射仍有效、重复删除不重复释放、超级页冲突保护以及第二级页表分配失败回滚检查。失败路径测试将空闲页暂时压到一页，确认 `page_insert()` 返回 `-E_NO_MEM` 后根表项、页引用和空闲页数量恢复。

另外按 Sv39 规范将非叶子 PTE 改为只设置 `V`，并把旧的 `PTE_U` 根表断言替换为两级非叶子项 `V=1` 且 `R/W/X/U/A/D=0` 的检查。叶子权限仍由原测试检查；`check_boot_pgdir()` 还实际访问合法的 `R|W` 映射。未修改 `tools/test_lab4.*`、评分注入脚本或队友的报告文件。

## 提交记录

- A1：`0f7289d` 实现删除；`bb4cf1f` 实现插入与回滚；`4531755` 检查 Sv39 超级页映射；`c778055` 校验待映射页；随后由 `f77a3d0` 标注 A1 代码归属。
- A2：`6921c66` 补充页表自检；`6e9be1b` 补充超级页冲突与分配失败回滚检查。
- 集成审查后，非叶子 PTE 规范修订记录为 `7b2332a`。

## 验证结果

在当前集成代码上执行：

```bash
cd code
make clean && make
timeout 15s make qemu
make grade QEMU=qemu-system-riscv64 \
  'DEFS=-Wno-error=int-conversion -Wno-error=incompatible-pointer-types' \
  GRADE_TMP=/tmp/lab4-review-grade-20261119
```

- `make clean && make`：退出码 0，内核编译和链接成功。
- QEMU：退出码 0；`check_alloc_page()`、`check_pgdir()`、`check_boot_pgdir()`、`check_vmm()` 均成功，随后出现 `init_main()` 的预期输出。`init_main()` 返回后，原框架 `do_exit()` 占位触发 `process exit!!.`。
- `make grade`：退出码 0；PMM、Process Create、Schedule 均为 `OK`，总分 `30/30`。这是完整集成评分，其中 PMM 项覆盖 A1/A2，不代表评分仅运行了 A 组。

原始输出与同组其他记录一样保存在独立日志文件中：

- A1 QEMU 启动与页表自检：[lab4_a1_qemu.log](../logs/lab4_a1_qemu.log)
- A2 页表相关 PMM 评分及完整集成结果：[lab4_a2_grade.log](../logs/lab4_a2_grade.log)

评分编译仍会报告固定测试中线程函数指针类型不匹配的警告；按方案未改动固定测试文件。最初沙箱内评分因 QEMU GDB socket 被拒绝产生空输出，获准在沙箱外重跑后取得上述 `30/30`；排错和总体审查日志保存在 `lab4_review_*.log`。
