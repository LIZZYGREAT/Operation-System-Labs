# 本次任务的真实提示词与执行约束

本次工作验证现有 Lab1 Starter Code，没有让 AI 生成课程功能代码；以下保留真实用户任务与实际约束，用于说明执行范围。没有补写虚构的“代码实现迭代提示词”。

## 用户原始任务

> 然后你查看 ./docs/Lab1_Agent_Execution_Plan.md和workflow.md，完成本次任务。要求细致完成，最好还能记录好每一步的目的为什么这么做，方便我后续自己阅读和理解，最后我会进行报告，所以需要你仔细完成。每完成一个小节，及时commit

## 本次工作沿用的目录与命令约束

以下为同一任务上下文中用户此前明确给出的要求：

> check the ./docs/environment_install.md and follow the md to install and there is no any resources provided by my course so you just don't need to ask me to provide you. if there is anything wrong with the network and "sudo",then ask me.

> oh you need to install in a right place ,check the os-lab/

> compiler is another course project, do anything inside os-lab

> you can do the commands without asking me if the commands are not dangerous or must need my check.

这些约束将课程工具放在 `~/os-lab`，将实验工作限制在 `os-lab` 工作区内，并允许直接执行安全、可逆的本地命令。

## 提交身份

用户提供本次 Git 提交署名：`LIZZYGREAT <2660550447@qq.com>`。提交时通过单次 Git 命令参数使用该身份，没有改写全局 Git 配置。

## 实际执行方式

- 先阅读执行计划和 workflow，再检查分支、Starter Code、工具和已有工作区记录。
- 依次进行干净构建、ELF/符号检查、普通 QEMU 启动、GDB 启动链与栈检查。
- 将失败尝试、修正原因、成功输出和清理状态保存在 `report/_work/logs/` 与 `report/_work/execution-log.md`。
- 最终从干净构建复跑关键检查，并依据真实结果编写 `report/report.md`。
