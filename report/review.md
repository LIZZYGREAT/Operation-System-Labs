# C3：SLUB 复核

日期：2026-10-22；基线：`f2d4661`；分支：`work/lab2/ziqiu/c3-slub`。

## 复核结果

- 环境：WSL Ubuntu、RISC-V GCC 15.1.0、QEMU 4.1.1 / OpenSBI v0.4，128 MiB、单核、Best-Fit。
- 静态检查：在单核串行、合法指针的接口契约内，未发现 SLUB 实现缺陷；检查了类别选择、对齐、大小溢出、链表转换及页面回收。
- 独立编译：开启及关闭测试的版本均成功，无编译警告或错误；使用原始启动方式，无需调试器辅助。
- 运行结果：零大小/NULL、类别边界、对象隔离、释放复用、多 slab、即时回收、大对象、失败与耗尽、384 轮混合操作，共九组全部通过，无 panic。
- 资源恢复：测试前后均为 31862 个空闲页；耗尽场景实际临时占用 31859 页。关闭测试后，默认启动和 PMM 自检通过，无 SLUB 测试输出。

## 运行命令

先在 PowerShell 中进入 WSL Ubuntu：

```powershell
wsl -d Ubuntu
```

然后在 WSL 中执行以下命令。显式指定 QEMU 4.1.1，避免使用 PATH 中的其他版本。

### 开启测试

```bash
cd /mnt/a/OS/exp/Operation-System-Labs/code
make OBJDIR=obj/slub-test BINDIR=bin/slub-test DEFS=-DSLUB_TEST=1 -j2
timeout 20s make \
  OBJDIR=obj/slub-test BINDIR=bin/slub-test DEFS=-DSLUB_TEST=1 \
  QEMU="$HOME/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64" \
  qemu > /tmp/slub-test.log 2>&1
cat /tmp/slub-test.log
```

应出现九个逐项 `PASS` 及最终标记，且无 panic：

```text
[SLUB] ALL 9 CASES PASSED: baseline=31862 final=31862
```

空闲页数可随构建变化，以本次运行的 baseline 与 final 相等为准。内核完成后继续循环，超时退出码 124 本身不表示失败。交互运行时去掉 `timeout 20s` 及输出重定向，按 Ctrl-A、X 退出。

### 关闭测试

以下命令仍在 `code` 目录执行：

```bash
## 普通编译
make -j2
make QEMU="$HOME/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64" qemu
```

若默认 `obj`、`bin` 曾开启过 `SLUB_TEST`，先执行 `make clean`。同一构建目录更改编译宏时也需清理或换用新目录，避免旧目标文件影响结果。

## 测试评价与边界

现有测试对完整数据、对象隔离、页面回收和真实耗尽的检查较充分。可以说明 SLUB 的功能已经实现。
