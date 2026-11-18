# 实验 06-06：SIGPIPE、EPIPE 与写入失败

状态：学习者已完成三模式上板验证：default 打印写前提示后结束，无 after write，Shell 状态 141；ignore 返回 -1/EPIPE（errno=32），caught_signal=0，主动退出 1；catch 返回 -1/EPIPE，caught_signal=13，主动退出 1。此前学习者指出新类型、变量和函数讲解不足，已补充[代码逐段讲解](代码逐段讲解.md)；当前仍停留本节，继续信号集合与屏蔽机制讲解，不能用实验通过代替原理掌握。源码保持不变，尚未收到本节 WSL 编译输出，不据此标记编译检查。06-05 的贴文文件名差异尚未核对，不将其作为推进阻碍。

教师隔离验证通过：ARM 静态程序构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。default 被 SIGPIPE 终止，未执行 after write；ignore 返回 -1/EPIPE、caught_signal=0；catch 返回 -1/EPIPE、caught_signal=SIGPIPE，两者均主动退出 1。另验证 Bash 将默认信号终止报告为 128+SIGPIPE，本机为 141。此验证不替代学习者亲手编译或板端运行。

学习安排更新（2026-11-11）：学习者保留当前信号初步理解，取消独立信号进阶章节，实际项目需要时再补具体机制，不标记已全面掌握。当前保留本节及07_pipe_backpressure，在08_fifo补齐命名管道后进入网络编程。

## 两端关闭的不同结果

| 条件 | 操作 | 结果 |
|---|---|---|
| 缓冲读空，所有写端引用关闭 | read 请求非零字节 | 返回 0，EOF |
| 所有读端引用关闭 | write 请求非零字节 | 产生 SIGPIPE；若不被默认动作终止，可观察 -1/EPIPE |

SIGPIPE 是信号，默认动作是终止进程；EPIPE 是 errno 中的错误原因，两者不是同一种返回值。信号由内核在写入无读者管道时产生，不需要另一个进程调用 kill。

本例关闭所有读引用后只写 1 字节，保证该次调用没有成功写入的数据。在更一般的大写入、并发关闭情形，还需处理实际写入长度和部分成功，不能把全部管道写入都简化成非全即无。

## 先固定状态，不依赖进程调度

本节只有一个进程：创建管道，关闭唯一的读端，再向写端 write 一个字节。没有 fork，也不需要 sleep；此时确定已经不存在任何读端引用，观察的失败不会取决于父子谁先关闭。

读者暂时不 read 不等于所有读端关闭。只要某个进程或某份副本还持有读端，写入可能先进入缓冲，空间不足时才等待；非阻塞写可能得到 EAGAIN。缓冲满和没有读者是不同条件。

## 三种模式

### default

用 sigaction 将 SIGPIPE 显式设为 SIG_DFL。写入触发信号，默认终止当前实验进程，因此不会执行 after write 及后面的错误分支。内核仍会在进程退出时释放它持有的描述符，Shell 不因这个子程序退出而一起退出。

在当前 Bash/常见板端 Shell 中，信号终止的状态通常按 128+信号编号表示；本例 SIGPIPE 常见为 13，因此 echo $? 常见为 141。这是 Shell 呈现的状态，不是 write 返回值，也不是程序显式 return 141。实际信号编号由程序打印，代码使用符号而不写死数字。

### ignore

用 sigaction 将 SIGPIPE 设为 SIG_IGN。信号被忽略，进程不会因其默认动作结束，因此 write 返回 -1，并通过 errno 报告 EPIPE。本例保存错误编号，再打印日志、关闭写端、return 1。

这个 1 是应用主动选择的退出码，表示发送失败；不是 EPIPE 数值。忽略信号并不会恢复读者，也不会让写入成功。不能像 EAGAIN 那样无休止立即重试这个已无读者的通道。

### catch

用 sigaction 注册 handle_sigpipe。处理函数只把信号编号写入 volatile sig_atomic_t，随后返回。主流程可以继续处理 write 的 -1/EPIPE，再报告 caught_signal。处理函数不调用 printf/perror，不在异步上下文中做不安全输出。

捕获 SIGPIPE 与忽略它的差别是处理函数会执行；本例处理函数正常返回，两种模式都无法改变写入失败的事实。该模式把早期信号章节中的回调、标志变量与本节错误处理连接起来。

## 忽略与屏蔽不同

SIG_IGN 改变收到信号时的处理动作；信号屏蔽字决定信号是否暂时阻塞递送。本例显式解除 SIGPIPE 屏蔽，避免从启动环境继承屏蔽状态而改变 default 模式的观察。sigprocmask 用 SIG_UNBLOCK 只解除这一种信号的屏蔽，不清空全部信号屏蔽字。

本节是单线程实验；多线程中的信号屏蔽和按操作避免 SIGPIPE 等做法后续结合线程、网络再讲，不把进程全局忽略信号当成所有场景的默认方案。

## errno 与 C 库缓冲

write 返回后首先在 -1 分支保存 errno，后面的 printf/strerror 不作为获取本次错误的依据。error 是 main 的局部副本；errno 仍由运行库提供线程存储。

写入前用 fflush(stdout) 提交模式提示，因为默认信号终止不会像正常从 main return 那样保证刷新待输出的 C 库缓冲。本例触发 SIGPIPE 的是对管道写端的底层 write，不使用 printf 往故障管道写，避免把故障时间混入 stdio 刷新阶段。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/06_ipc/06_sigpipe
cat src/sigpipe_demo.c
make -n
make
echo $?
file build/sigpipe_demo
~~~

预期生成 ARM 32 位静态程序。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/06_ipc/06_sigpipe
~~~

WSL 当前目录传输：

~~~sh
scp -O build/sigpipe_demo root@192.168.10.2:/root/linux-study/06_ipc/06_sigpipe/
~~~

## COM12：先对比 default 与 ignore

~~~sh
cd /root/linux-study/06_ipc/06_sigpipe
chmod +x sigpipe_demo
./sigpipe_demo default
echo $?
./sigpipe_demo ignore
echo $?
~~~

default 预期只有写入前提示，没有 after write，Shell 状态常见 141；Shell 是否另打印 Broken pipe 不作保证。ignore 预期 after write: returned -1、errno=EPIPE（常见 32，文字为 Broken pipe）、caught_signal=0，最后 EPIPE 提示，应用退出 1。

ignore 模式退出 1 是本例正确报告发送失败的预期结果，不是编译或传输失败。

## COM12：再观察处理函数

~~~sh
./sigpipe_demo catch
echo $?
~~~

预期仍是 -1/EPIPE，但 caught_signal 等于打印的 SIGPIPE 编号，常见 13；应用仍主动返回 1。信号处理正常返回并不自动重试或补全 write。

## 与已有捕获器的关系

父捕获器提前关闭读端后，仍向该管道输出的子程序可能遇到 SIGPIPE；反过来，父向子输入管道写而子提前退出，也可能触发。阻塞/非阻塞、poll 就绪都不保证读者永远存活，写入后仍需检查结果。

处理信号与处理错误返回是两层工作。只在默认模式的 write 后写 if(errno==EPIPE) 并不能保证执行到该分支。实际应用应明确选择信号策略，并决定停止、清理、报告或重建通信，而不是把出错条件当成“稍后肯定能写”。

参考：[管道两端的关闭语义](https://man7.org/linux/man-pages/man7/pipe.7.html)、[write](https://man7.org/linux/man-pages/man2/write.2.html)、[信号动作](https://man7.org/linux/man-pages/man7/signal.7.html)、[sigaction](https://man7.org/linux/man-pages/man2/sigaction.2.html)、[Bash 退出状态](https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html)。
