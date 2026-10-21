# Buddy System：设计、运行与验证

负责人：王子楸，学号 2412712。日期：2026-10-21。

## 实现范围

- `code/kern/mm/buddy_pmm.c`、`buddy_pmm.h`：Buddy 的六个管理器回调、注册对象和基础自检。
- `code/kern/mm/pmm.c`：仅增加头文件与管理器选择分支，原有 `page_init()` 和调用顺序保持不变。
- `code/Makefile`：增加 `PMM=first_fit|best_fit|buddy` 参数；默认仍为本次基线的 First-Fit。

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

## QEMU 4.1.1 安装记录

实际环境为 WSL Ubuntu 26.04，宿主 GCC 15.2.0，内核交叉编译器为 RISC-V GCC 15.1.0。

从指导书指定的 `https://download.qemu.org/qemu-4.1.1.tar.xz` 获取官方发布源码，保留原源码，执行了以下配置、编译和用户目录安装：

```bash
mkdir -p ~/build/qemu-4.1.1
cd ~/build/qemu-4.1.1
~/src/qemu-4.1.1/configure \
    --prefix="$HOME/.local/opt/qemu-4.1.1" \
    --target-list=riscv32-softmmu,riscv64-softmmu \
    --enable-debug --disable-werror
make -j8
make install
```

`--disable-werror` 用于允许旧版 QEMU 在现代宿主工具链上的警告；实验内核自己的 `-Werror` 保持不变。实际安装的两个 RISC-V 模拟器均输出 `QEMU emulator version 4.1.1`，64 位模拟器包含调试信息且未剥离符号。构建与安装日志位于 WSL 的 `~/build/qemu-4.1.1/`。

本机 64 位程序路径为 `/home/catalpa/.local/opt/qemu-4.1.1/bin/qemu-system-riscv64`。系统原有其他 QEMU 版本保持原状；运行实验时必须显式指定本版本，或在当前终端调整 `PATH`。不要把本机绝对路径写死到全组 Makefile。

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

# 恢复本次基线默认的 First-Fit
make clean
make qemu QEMU="$QEMU411"
```

最终小组基础提交需要采用 Best-Fit。完成公共集成时再统一设置默认值；本次保留原默认选择，只提供显式切换。基础评分可使用下列命令，但本轮未运行评分，不能将它写成通过记录：

```bash
make grade PMM=best_fit QEMU="$QEMU411"
```

评分流程会调用 Python 并注入固定检查，执行前遵守本机 Python 命令的权限约定。Best-Fit 选择检查用于基础提交，不作为 Buddy 正确性的证明。

## 实际验证结果

以下结果全部使用已安装的 QEMU 4.1.1、其默认 OpenSBI 0.4 和原有 `make qemu` 启动方式获取，未使用额外 `-kernel` 参数、兼容脚本或调试器改动执行位置。

| 检查 | 实际结果 |
|---|---|
| Buddy 干净构建及启动 | 通过，输出 `memory management: buddy_pmm_manager` |
| Buddy 基础自检 | 通过，输出 `buddy_check: passed (free pages restored: 31929)` |
| 公共检查入口 | 输出 `check_alloc_page() succeeded!` |
| Best-Fit 干净构建及原有自检 | 通过，选择 `best_fit_pmm_manager` |
| 默认 First-Fit 干净构建及原有自检 | 通过，选择 `default_pmm_manager` |
| 非法 `PMM=invalid` | 构建阶段按预期拒绝 |
| 受保护源文件与差异检查 | 原有算法、启动代码和固定测试未修改；`git diff --check` 通过 |

原始启动日志：

- [Buddy](buddy_qemu_4.1.1.log)
- [Best-Fit](best_fit_qemu_4.1.1.log)
- [默认 First-Fit](first_fit_qemu_4.1.1.log)

基础自检在两轮申请/释放中检查单页、申请 3 页实际占用 4 页、块不重叠、回收复用，以及各阶块的对齐、首页标记、链表和空闲计数一致性；另检查超大请求失败且不改变空闲计数。

本轮结束时已恢复默认 First-Fit 构建。完整独立测试、另一名成员交叉复现和基础评分尚未完成，后续第二份测试提示词另行设计。

## 真实问题与处理

最初误用了 WSL 中默认的 QEMU 7.0.0，与课程环境不一致。排错时曾增加临时的 GDB 启动兼容文件；该过程不能作为规定环境下的正式验收。用户指出版本要求后，已安装 4.1.1，删除 `buddy_boot_compat.gdb`、`run_pmm_compat.sh` 及对应日志，并直接按原启动流程重新取得上面的验证结果。

WSL 首次从官网下载时出现 TLS 连接错误，随后通过 Windows 下载同一官方源码包，再在 WSL 中编译安装。源码包 SHA-256 为 `ed6fdbbdd272611446ff8036991e9b9f04a2ab2e3ffa9e79f3bab0eb9a95a1d2`，用于标识本次实际使用的文件。
