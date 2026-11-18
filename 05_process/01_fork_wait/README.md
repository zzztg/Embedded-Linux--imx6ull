# 实验 05-01：fork 创建进程与 waitpid 回收

状态：学习者已上板运行。父 PID 13718，子 PID 13719；fork 前输出一次，子进程返回值为 0、父进程取得子 PID；子变量改为 99，父进程回收后仍为 10；waitpid 返回 13719，收取子退出码 7。尚未提供 WSL 编译输出与紧接运行的 echo $?，不据此标记已检查。本节继续讲解和理解检查。

教师隔离验证：.validation/project 内 ARM 静态编译成功；WSL 本机版本以 -Wall -Wextra -Werror 编译，直接运行和输出重定向两种情形均验证了子分支返回 0、子变量改成 99、父变量保持 10、waitpid 回收正确子 PID、收取退出码 7，父进程退出 0。本机验证不代表已在开发板验证。

## 本章路线

先理解 fork 的返回分支、普通变量隔离、退出状态与回收。后续再学习 exec 替换程序、继承的文件描述符与共享文件偏移、僵尸和孤儿进程的观察、管道以及进程组与信号。本节只创建一个子进程。

## 程序与进程

程序是磁盘上的代码文件；进程是运行实例，具有 PID、执行状态、虚拟地址空间以及内核管理的资源。单片机常见的一个 main 循环不能完整描述 Linux 上多个进程的运行；进程也不等同于中断回调或共享普通变量的 RTOS 任务。

fork 成功后，原进程叫父进程，新进程叫子进程。子进程从 fork 返回处继续执行，不会重新从 main 开始。两个进程执行同一程序的后续代码，但返回值不同。

| 所处进程/结果 | fork 返回值 |
|---|---|
| 父进程，成功 | 新建子进程的 PID，大于 0 |
| 子进程 | 0 |
| 创建失败 | 原进程返回 -1，无子进程产生，errno 表示原因 |

因此 pid_t child_pid = fork() 之后存在两份各自的 child_pid：父进程的值是子 PID，子进程的值是 0。子进程自己的真实 PID 由 getpid() 取得，并不是 0；getppid() 返回父 PID。

单核 CPU 也能通过调度让多个进程交替推进。fork 后父子谁先执行没有保证；本程序不能根据某一次输出顺序推断固定调度顺序。

## 普通变量的隔离

fork 时，子进程获得父进程地址空间内容的副本。子进程起初 number 为 10，随后改成 99，不会修改父进程的 number。父进程回收之后仍为 10。

Linux 通常采用写时复制：开始可共享物理页，在必要的写入时建立私有副本，以避免立即复制全部内存。这是实现优化，不改变本例普通变量互不影响的语义。共享内存属于显式共享机制，本轮不设专章，实际项目需要时再学；继承的文件描述符又有自己的共享规则，不能把所有资源都理解成完全独立。

## waitpid 的三个重点

~~~c
waited = waitpid(child_pid, &status, 0);
~~~

第一个参数指定等待哪个子进程；第二个参数接收编码后的状态；第三个参数为 0，本例等待指定子进程终止。如果子进程尚未退出，父进程阻塞等待，不是反复空循环占用 CPU；如果它已经退出但尚未回收，可以直接取走状态。

成功返回的是回收的子 PID，不是退出码。status 也不能直接当退出码使用：先用 WIFEXITED 判断正常退出，成立后再用 WEXITSTATUS 取退出码。信号终止则用 WIFSIGNALED 和 WTERMSIG。本例接收到 EINTR 时重试 waitpid。

子进程退出后，内核通常暂存 PID、终止状态等回收信息，形成僵尸状态；回收会清除这些信息。僵尸已不执行代码。普通 waitpid 是等待或收取已有结果，不负责终止子进程。后续专门观察这一生命周期。

## 两个进程的退出码

子分支 return 7 结束子进程；它不会结束父进程，也不会返回父进程的调用点。本例父进程通过 waitpid 得到 7，随后自己 return 0，所以在 Shell 中紧接 ./fork_demo 执行 echo $? 预期为 0。

父进程在系统调用成功收取状态后选择报告并正常结束；它没有把子进程的非零状态转成自己的失败码。这是本例的设计，实际应用应自行确定子任务失败的处理方式。

    本例是单线程程序，子分支从 main return，正常刷新自己的 C 库输出。fork 前的 fflush(stdout) 用于提交已有输出，避免zo待输出的 C 库缓冲被复制后，父子正常退出时各刷一遍。多线程程序 fork 后的调用限制以及 exec 失败使用 _exit 的习惯留到相关章节讲解。

## WSL：阅读和亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/05_process/01_fork_wait
cat src/fork_demo.c
make -n
make
echo $?
file build/fork_demo
~~~

预期生成 ARM 32 位静态程序。不能直接在 x86_64 WSL 中执行 ARM 产物；本节目标产物在开发板运行。

## 传输与执行

COM12 先创建目录：

~~~sh
mkdir -p /root/linux-study/05_process/01_fork_wait
~~~

WSL 当前目录传输：

~~~sh
scp -O build/fork_demo root@192.168.10.2:/root/linux-study/05_process/01_fork_wait/
~~~

COM12 执行：

~~~sh
cd /root/linux-study/05_process/01_fork_wait
chmod +x fork_demo
./fork_demo
echo $?
~~~

## 预期观察

~~~text
before fork: pid=1200, number=10
parent: pid=1200, child_pid=1201, number=10
child: pid=1201, ppid=1200, fork_return=0, number=10
child: changed number=99; exiting with 7
parent: waitpid returned 1201, number=10
parent: child exit code=7
~~~

示例 PID 不是固定值；parent 分支的首行和 child 分支的输出先后可能不同。before fork 在本例正常输出时只出现一次；父进程回收后的两行在子进程正常输出完成并退出后产生。本例共享终端并不建立普遍的跨进程输出顺序保证，重定向的 C 库缓冲也会影响实际显示时机。

关键理解检查：为什么同一行 fork 后能进入两个不同分支？为什么父进程等到子进程结束后，number 仍然是 10？为什么终端最后看到子退出码 7，而 echo $? 却为 0？

参考：[fork](https://man7.org/linux/man-pages/man2/fork.2.html)、[wait/waitpid](https://man7.org/linux/man-pages/man2/wait.2.html)。
