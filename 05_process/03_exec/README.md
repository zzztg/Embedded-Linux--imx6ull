# 实验 05-03：fork 与 exec 配合启动另一个程序

状态：学习者已上板验证成功与失败两条路径。成功路径父 PID 28740、子 PID 28742，exec 前后子 PID 与 PPid 保持不变；新程序 argc=3，argv 分别为 args_demo、hello、i.MX6ULL；目标返回 7，父进程回收并报告子退出码 7，Shell 紧接检查启动器退出状态为 0。失败路径父 PID 14902、子 PID 14903，缺失路径导致 execl 报 No such file or directory，无新程序输出；子进程以 127 退出并被回收，父进程退出 0。板端临时 IP 已恢复为 192.168.10.2/24。实验完成，继续原理理解检查；尚未收到本节 WSL 编译输出，不据此标记编译检查。

教师隔离验证通过：两个 ARM 静态程序均构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。成功路径确认 exec 前后 PID、PPid 相同，新参数 argc=3，目标程序退出 7；缺失路径确认无新程序输出、错误处理执行、子退出 127；两条路径均回收子进程、启动器退出 0。此记录不替代学习者编译或开发板验证。

## 两个不同的操作

fork 创建新进程，子进程继续执行原程序；exec 家族用新程序替换调用进程的程序映像，不额外创建进程。成功替换后，原有代码、栈、堆和普通变量不再作为原程序继续运行；PID 保持不变。本例父子关系也保持不变。

exec 不是调用一个普通函数、等它结束后再回到旧代码。成功后由新程序的启动代码进入新 main；新程序退出时，这个进程就结束了，父进程仍通过同一个子 PID 收取状态。

本节有两个可执行文件：exec_demo 是启动器，args_demo 是被执行的新程序。它们各有一个 main，分别编译、分别链接，不能把两个目标文件直接链接为一个程序。

## execl 的参数

~~~c
execl(argv[1], "args_demo", "hello", "i.MX6ULL", (char *)NULL);
~~~

从左向右的含义为：要执行的文件路径；新程序的 argv[0]；新程序的 argv[1]；新程序的 argv[2]；表示参数列表结束的空指针。

启动器执行 ./exec_demo ./args_demo 时，启动器的 argv[1] 是路径 ./args_demo。新程序得到的是另一组参数：argc=3，argv[0] 为 args_demo，argv[1] 为 hello，argv[2] 为 i.MX6ULL，argv[3] 为 NULL。终止空指针不计入 argc。

路径决定实际加载哪个文件；argv[0] 是传给新程序的名称字符串，通常按惯例与程序名对应，它本身不负责选择文件。

execl 中 l 表示用逐项列表传参数。不带 p 的本函数使用指定路径，不会根据 PATH 去搜索；./args_demo 相对于进程当前工作目录，而不是相对于启动器文件所在目录。本节在板端进入两个程序所在目录后再执行。

## 成功与失败

execl 成功不会返回旧代码，后面的 perror 和 _exit 不执行。文件不存在、无执行权限、文件格式或架构不匹配等问题会导致启动失败。常见失败返回 -1 并设置 errno，本例随后 perror 报告原因、_exit(127) 结束子进程。不能把 execl 当作成功返回 0 的普通接口。

127 是本例在失败分支主动选择的子进程退出码，不是 execl 返回值或 errno，也不能只凭 127 区分所有失败原因。极少数内核加载失败可能发生在旧映像已拆除之后，这类情况不保证返回旧程序；入门实验观察的是路径查找等常见失败。

## 与 C 库缓冲的联系

成功 exec 不会替旧程序调用 exit 或刷新旧的 C 库缓冲，所以子进程在打印 before exec 后显式 fflush，确保这条输出在替换前已提交。启动器也在 fork 前刷新，以避免复制未提交的输出。

_exit 是结束当前进程的函数，放在普通函数中也能终止进程；return 只返回当前函数。_exit 不刷新 C 库 stdio 缓冲，也不调用 atexit 注册的退出函数，故常用于 fork 后 exec 失败的子进程路径。本例在需要输出的位置已经刷新；perror 写 stderr。本例为单线程教学代码，不把这些 printf/perror 用法推广成多线程 fork 后的通用模板。

## 哪些内容保留

本例 PID、父 PID、当前工作目录保持；execl 默认继承调用者的环境。普通变量和 C 库缓冲不作为旧程序的运行状态继续保留；捕获的信号处理函数会在成功 exec 后重置为默认行为。

文件描述符默认跨 exec 保留，标记 FD_CLOEXEC 的会关闭。串口上的标准输出因此能被新程序继续使用；后续文件描述符继承和重定向实验再深入这一点。exec 的替换与保持不能简单理解成所有资源都清空。

## WSL：亲手编译两个程序

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/05_process/03_exec
cat src/exec_demo.c
cat src/args_demo.c
make -n
make
echo $?
file build/exec_demo build/args_demo
~~~

预期创建 build，编译两份源文件、分别链接两个 ARM 静态程序。教师隔离验证不替代此步骤。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/05_process/03_exec
~~~

WSL 当前目录传输两个程序：

~~~sh
scp -O build/exec_demo build/args_demo root@192.168.10.2:/root/linux-study/05_process/03_exec/
~~~

## COM12：成功替换

~~~sh
cd /root/linux-study/05_process/03_exec
chmod +x exec_demo args_demo
./exec_demo ./args_demo
echo $?
~~~

观察 child before exec 和 new program 的 PID 相同，ppid 也相同；新程序打印 argc=3 和三个参数，再返回 7；父进程收取退出码 7，最终自己返回 0，故 Shell 中预期为 0。父进程首行和子进程输出的先后没有固定保证。

本例启动器保持上一节的行为：报告子结果，但不自动转交为自己的退出码。若实际应用希望启动器也报告失败，需明确实现状态传播；不是内核自动把子进程退出码写成父进程退出码。

## COM12：常见启动失败

使用本节特意预留的不存在目录作为路径。如果曾自行创建了同名目录，请换一个确实不存在的父目录。

~~~sh
./exec_demo ./missing_exec_parent_for_demo/args_demo
echo $?
~~~

预期有 child before exec，但没有 new program；execl 报 No such file or directory；父进程收取子退出码 127，自己仍按本例设计返回 0。该路径失败证明子进程创建与新程序加载是两个阶段：fork 可以成功，随后 exec 失败。

## 需要能独立解释

为什么替换前后子 PID 一样？为什么不能在 execl 后写一条预期成功时执行的语句？为什么新程序 argc 是 3 而不是 4？新程序结束后为什么不会回到旧 main？启动失败时为什么仍然要回收子进程？

## exec 家族速查（后续逐步使用）

| 名称组成 | 含义 |
|---|---|
| l | 参数逐项列出 |
| v | 参数用指针数组提供 |
| p | 文件名不含 / 时按 PATH 搜索 |
| e | 显式提供环境数组 |

本节用 execl；execv 适合参数数组，execvp 兼有数组和 PATH 搜索。execve 是底层系统调用接口。直接使用本节 execl 不会让 Shell 解析管道、重定向、通配符等字符串。

fork+exec+wait 是理解 Shell 启动外部命令的基本模型；实际 Shell 还包含内建命令、重定向、进程组和实现优化，并不是每条输入都按完全相同方式创建进程。

参考：[exec 家族](https://man7.org/linux/man-pages/man3/exec.3.html)、[execve](https://man7.org/linux/man-pages/man2/execve.2.html)、[_exit](https://man7.org/linux/man-pages/man2/_exit.2.html)。
