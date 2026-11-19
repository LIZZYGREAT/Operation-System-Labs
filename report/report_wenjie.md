# Lab4 整体审查修订记录（Wenjie）

审查基线为 `63bbe1e`，本次在 `fix/lab4-review` 分支修订。按要求未修改队友的 Markdown；`report/personal_reports/report_ziqiu.md` 已恢复为基线版本，本记录及本次日志单独保存在本文件和 `report/logs/lab4_review_*.log`。

## 修订内容

- **大块 `kmalloc` 页数计算**：`find_order()` 改为按请求字节数向上取整页数，再求最小覆盖阶数；超过 `KMALLOC_MAX_ORDER` 时返回失败，避免整数窄化、下取整少分配和越界移位。新增由 `LAB4_KMALLOC_SELFTEST` 控制的专项检查，覆盖 4096、8192、8193、16384、16385 字节、分配容量、请求末尾写入、释放后页数恢复及超上限请求。专项检查默认不运行，不改变常规启动行为。
- **Sv39 非叶子 PTE**：`get_pte()` 使用 `ptd_create()`，只设置 `V`。`check_pgdir()` 保留原有映射和引用检查，并改为分别核对两级非叶子项的 `V=1`、`R/W/X/U/A/D=0`。`check_boot_pgdir()` 保留真实别名读写，并新增合法 `R|W` 叶子权限断言。
- **名称复制边界**：`set_proc_name()` 最多复制 `PROC_NAME_LEN` 字节，遇到 NUL 即停止，短名称不会越界读取，目标缓冲区继续保证 NUL 终止。
- **类型和记录修正**：释放大块时将内核虚拟地址显式转成 `void *` 后传给 `kva2page()`。评分结果的 `30/30` 记录写在本文件；队友的报告文件保持未修改。

未修改 `tools/test_lab4.*`、`tools/grade.sh` 或 `tools/inject.py`。注入测试仍有线程入口函数类型不匹配的编译警告；按方案保留固定测试文件不变。

## 验证

| 命令 | 结果 |
|---|---|
| `make clean && make` | 退出码 0，内核编译链接成功 |
| `timeout 15s make qemu` | 退出码 0；页表、VMM 自检和 `init_main()` 预期输出出现；随后原框架 `do_exit()` 占位触发 `process exit!!.` |
| `timeout 15s make qemu 'DEFS=-DLAB4_KMALLOC_SELFTEST'` | 退出码 0；出现 `[kmalloc boundary] PASS`，专项边界检查通过 |
| `make grade QEMU=qemu-system-riscv64 'DEFS=-Wno-error=int-conversion -Wno-error=incompatible-pointer-types' GRADE_TMP=/tmp/lab4-review-grade-20261119` | 退出码 0；PMM、Process Create、Schedule 均为 `OK`，总分 `30/30` |

本机 QEMU 为 4.1.1。最初在沙箱内运行评分时，QEMU 创建 GDB TCP socket 被拒绝，空输出导致 0/30；原始失败记录在 `report/logs/lab4_review_grade.log`，获准在沙箱外重跑后得到上表中的实际 30/30。完整构建、常规 QEMU、专项 QEMU 和最终评分输出分别保存在 `report/logs/lab4_review_final_build_qemu.log`、`report/logs/lab4_review_kmalloc_qemu.log` 和 `report/logs/lab4_review_final_grade.log`。

`R=0, W=1` 的叶子权限输入仍按原接口传递，没有擅自改写 `perm` 契约；本次实际读写验证使用合法 `R|W` 映射。固定评分测试的函数签名警告也未通过修改队友测试文件处理。
