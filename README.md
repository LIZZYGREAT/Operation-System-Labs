# Operation-System-Labs

操作系统课程实验小组仓库。

本仓库用于 3 人小组完成多个 Lab 的代码实现、实验报告、Prompt 记录与测试截图管理，并按照课程要求提交。

---

## 项目成员

- 甘文杰
- 向宇航
- 王子楸

---

## 一、课程交付要求

每次实验使用一个独立分支：

```text
lab1
lab2
lab3
...
```

每个 `labX` 分支最终必须保持如下结构：

```text
.
├── code/
│   └── 对应实验完成后的完整代码
│
└── report/
    ├── report.md
    ├── prompt.md
    └── images/
```

其中：

- `code/`：该实验最终可编译、运行、验收的完整代码；
- `report/report.md`：实验报告，按照课程提供的实验报告模板编写；
- `report/prompt.md`：汇总本实验实际使用过的所有 Prompt；
- `report/images/`：保存实验报告中引用的测试截图。

---

## 二、分支规范

### 1. `main`

`main` 只用于保存：

- 项目 README；
- 协作规范；
- 通用模板；
- 与具体 Lab 无关的仓库配置。

不要直接在 `main` 中完成某次实验。

### 2. `labX`

例如：

```text
lab1
lab2
lab3
```

`labX` 是该次实验的正式交付分支。

基本要求：

- 始终保持可提交状态；
- 不作为个人日常开发分支；
- 不允许随意直接 push；
- 修改应通过 Pull Request 合并；
- 禁止 force push。

### 3. 个人任务分支

每个成员开发时，从最新 `labX` 创建自己的任务分支：

```text
work/labX/<member>/<task>
```

例如：

```text
work/lab1/wenjie/gdb-trace
work/lab1/member-b/report
work/lab2/member-c/page-allocator
```

任务名使用简短英文 `kebab-case`。

不要创建：

```text
test
new
new2
final
final-final
dev
mybranch
```

等无法判断用途的分支。

---

## 三、每个 Lab 的初始化

拿到老师的新实验代码后，首先建立该 Lab 的原始基线，不要直接开始修改。

例如 Lab1：

```bash
git switch main
git pull --ff-only
git switch -c lab1
```

将老师提供的原始实验工程完整放入：

```text
code/
```

同时建立：

```text
report/
├── report.md
├── prompt.md
└── images/
```

第一次提交只用于记录老师原始代码：

```bash
git add .
git commit -m "chore(lab1): import instructor starter code"
git push -u origin lab1
```

建议同时创建基线 Tag：

```bash
git tag lab1-starter
git push origin lab1-starter
```

以后可以直接：

```bash
git diff lab1-starter..lab1
```

查看整个实验相对于老师原始代码的全部修改。

---

## 四、标准开发流程

每次开始新任务前：

```bash
git fetch origin
git switch labX
git pull --ff-only
```

然后创建个人任务分支：

```bash
git switch -c work/labX/<member>/<task>
```

完整流程：

```text
创建 / 领取 Issue
        ↓
从最新 labX 创建 work 分支
        ↓
理解任务与相关代码
        ↓
与 AI 交互
        ↓
实现 / 修改代码
        ↓
编译、运行、测试
        ↓
记录 Prompt
        ↓
提交 Commit
        ↓
Push 工作分支
        ↓
创建 Pull Request
        ↓
另一名成员 Review
        ↓
合并到 labX
        ↓
删除已完成的 work 分支
```

---

## 五、任务分工原则

每个明确任务至少包含：

```text
Owner
+
Reviewer
```

例如：

```text
A：实现
B：Review
C：最终复现
```

不要长期固定成：

```text
一个人只写代码
一个人只写报告
一个人只截图
```

每个人最终都必须理解：

- 这个 Lab 在解决什么问题；
- 修改了哪些代码；
- 为什么这样实现；
- AI 参与了哪些部分；
- AI 是否产生过错误；
- 如何验证最终实现；
- 关键测试结果是什么。

因为课程验收可能要求每个组员现场回答问题。

---

## 六、Issue 规范

建议一个明确任务对应一个 Issue。

例如：

```text
[Lab1] 完成 GDB 启动链追踪
[Lab1] 完成实验报告
[Lab1] 汇总 Prompt
[Lab2] 实现 page allocator
```

Issue 至少写清：

```markdown
## 目标

本任务需要完成什么。

## 完成条件

- [ ] 条件 1
- [ ] 条件 2
- [ ] 条件 3

## 负责人

@xxx

## Reviewer

@xxx
```

---

## 七、Pull Request 规范

所有个人任务分支合并进 `labX` 时都应通过 Pull Request。

建议 PR 描述：

```markdown
## Task

Closes #IssueNumber

## Changes

说明本次完成了什么。

## Verification

说明如何验证，例如：

make
make qemu


## Prompt

- [ ] 已记录本任务实际使用的 Prompt
- [ ] Prompt 中不存在 API Key / Token

## Report

- [ ] 如影响实验报告，已同步更新
- [ ] 如产生测试结果，已保存截图

## Checklist

- [ ] 修改范围与 Issue 一致
- [ ] 可以复现
- [ ] 没有无关修改
- [ ] 没有敏感信息

至少由另一名组员 Review 后再合并。
```

---

## 八、Commit 规范

Commit 按功能 / 修改内容划分，不按时间划分。

统一推荐：

```text
<type>(labX/<scope>): <description>
```

常用类型：

```text
feat
fix
test
docs
refactor
chore
```

示例：

```text
chore(lab1): import instructor starter code
feat(lab2/mm): implement page allocator
fix(lab2/mm): handle zero-size allocation
test(lab2): add allocator boundary tests
docs(lab2/report): explain page allocation workflow
docs(lab2/prompt): record allocator debugging prompts
```

避免：

```text
update
修改
今天完成
final
final2
fix bug
```

---

## 九、Prompt 管理

课程要求保留与 AI 的交互过程，因此不要只保存最终成功的 Prompt。

应尽量保留：

```text
第一次提问
    ↓
AI 生成结果
    ↓
测试 / 阅读发现问题
    ↓
追加约束或纠错
    ↓
再次生成
    ↓
继续验证
```

开发阶段为了避免 3 个人同时修改 `prompt.md` 导致冲突，可以临时使用：

```text
report/
└── _work/
    └── prompts/
        ├── member-a.md
        ├── member-b.md
        └── member-c.md
```

每个人只维护自己的 Prompt 日志。

最终提交前统一合并为：

```text
report/prompt.md
```

然后删除：

```text
report/_work/
```

---

## 十、截图规范

测试截图统一保存到：

```text
report/images/
```

推荐命名：

```text
01-build-success.png
02-qemu-start.png
03-gdb-pc-1000.png
04-gdb-opensbi.png
05-kernel-entry.png
```

不要使用：

```text
截图1.png
微信截图_xxx.png
QQ图片_xxx.png
屏幕截图(17).png
```

报告中使用相对路径：

```markdown
![GDB 跟踪结果](./images/03-gdb-pc-1000.png)
```

最终只保留报告实际引用、与实验验证有关的截图。

---

## 十一、代码冲突处理

开始任务前，应先在 Issue 中说明预计修改：

- 哪些文件；
- 哪些函数；
- 哪些模块。

如果两个人必须修改同一个核心文件，不要同时长时间开发。

推荐顺序：

```text
A 完成修改
    ↓
PR 合并进 labX
    ↓
B 更新自己的分支
    ↓
B 继续后续修改
```

开发过程中，如果 `labX` 已有其他成员的新修改：

```bash
git fetch origin
git merge origin/labX
```

禁止对共享分支执行：

```bash
git push --force
```

---

## 十二、敏感信息禁止提交

任何情况下都禁止提交：

```text
DeepSeek API Key
OpenAI API Key
Claude API Key
GitHub Token
SSH Private Key
.env 中的密码或 Token
其他个人访问凭证
```

尤其注意：

```text
report/prompt.md
```

也可能因为复制 Prompt 而误带敏感信息。

提交前建议检查：

```bash
git diff --cached
```

推荐 `.gitignore` 至少包含：

```gitignore
.env
.env.*
*.key
*.pem
```

---

## 十三、每个 Lab 的最终验收流程

准备提交时停止继续增加功能，并按以下顺序检查。

### 1. 目录结构

确认：

```text
code/
report/
├── report.md
├── prompt.md
└── images/
```

完整存在。

### 2. 编译与运行

重新执行该 Lab 要求的：

```text
编译
运行
测试
GDB / QEMU 验证
```

### 3. 独立复现

最终代码不能只在开发者自己的环境中测试。

至少另一名成员应基于最新 `labX` 重新运行完整实验。

### 4. Git 状态

```bash
git status
```

应显示：

```text
nothing to commit, working tree clean
```

### 5. 检查最终修改

如果存在：

```text
labX-starter
```

执行：

```bash
git diff labX-starter..labX
```

确认没有无关修改。

### 6. 检查报告

确认：

- `report.md` 内容完整；
- 图片全部存在；
- 图片链接正确；
- `prompt.md` 已汇总；
- 没有 API Key；
- 测试结论能够被实际复现。

### 7. 三人共同确认

每个人都应能够解释最终实现和实验原理。

---

## 十四、正式提交版本

最终确认后，在 `labX` 最新提交上创建 Tag：

```bash
git switch labX
git pull --ff-only

git tag -a labX-submit-v1 -m "LabX submission"
git push origin labX-submit-v1
```

例如：

```text
lab1-submit-v1
lab2-submit-v1
```

老师正式检查的仍然是：

```text
labX
```

Tag 只用于保存小组当时的正式提交快照。

如果老师允许修改并重新提交，则创建：

```text
labX-submit-v2
```

不要覆盖旧提交历史。

---

## 十五、Lab 之间的继承

不要默认：

```text
lab2 一定从 lab1 创建
lab3 一定从 lab2 创建
```

每次新 Lab 发布后，先确认老师提供的代码形式。

如果 Lab2 明确基于 Lab1：

```text
lab1
  ↓
lab2
```

如果老师提供了新的 Starter Code，则以老师新工程为准，不强行继承旧 Lab。

核心原则：

> 老师提供的实验起始工程优先于我们自己假定的 Git 分支继承关系。

---

## 十六、最重要的仓库规则

所有成员需要共同遵守：

1. `labX` 是正式交付分支，不是个人开发分支。
2. 日常开发使用 `work/labX/<member>/<task>`。
3. 每个任务至少有一个 Owner 和一个 Reviewer。
4. 每次 Lab 首先保存老师原始 Starter Code 基线。
5. 所有正式修改通过 Pull Request 合并。
6. Commit 按功能划分，不按日期划分。
7. 禁止 force push `main` 和 `labX`。
8. Prompt 必须保留真实交互与修正过程。
9. 测试截图必须能对应实验报告中的验证内容。
10. 禁止提交 API Key、Token、密码等敏感信息。
11. 写代码的人不能成为唯一验证代码的人。
12. 提交前至少由另一名成员进行独立复现。
13. 每个人都必须理解最终代码和实验原理。
14. `labX` 最终必须严格满足老师要求的目录结构。
15. 正式提交时使用 `labX-submit-vN` Tag 保存提交快照。

---

## 十七、常用 Git 命令

同步正式实验分支：

```bash
git fetch origin
git switch lab1
git pull --ff-only
```

创建任务分支：

```bash
git switch -c work/lab1/<member>/<task>
```

提交修改：

```bash
git add .
git commit -m "feat(lab1/<scope>): description"
git push -u origin work/lab1/<member>/<task>
```

查看状态：

```bash
git status
```

查看修改：

```bash
git diff
git diff --cached
```

同步 `labX` 的最新修改：

```bash
git fetch origin
git merge origin/lab1
```

查看相对 Starter Code 的完整修改：

```bash
git diff lab1-starter..lab1
```

---

## License

本仓库仅用于课程实验、学习与小组协作。
