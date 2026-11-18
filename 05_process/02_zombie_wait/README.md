# 实验 05-02：观察僵尸状态与回收

状态：学习者已完成板端完整观察。子 PID 5352、父 PID 5351；回收前子进程 State 为 Z，父进程 State 为 S，父进程的 PPid 为 344；按回车后 waitpid 成功回收 5352、收取子退出码 7；COM12 紧接运行检查父进程退出状态为 0；SSH 再查看 /proc/5352/status 提示 No such file or directory。本节上板验证完成，尚未收到本节 WSL 编译输出，不据此标记编译检查。前一节的理解检查已确认：普通函数 return 返回调用者；main 的 return 结束当前进程；sleep 不会完成回收。

教师隔离验证通过：ARM 静态程序编译成功；WSL 本机版以 -Wall -Wextra -Werror 编译；验证脚本在父进程保持存活、尚未收到输入时观察到子进程 Z (zombie)，随后输入回车，确认回收正确子 PID、子退出码为 7、父退出码为 0，且子进程 /proc 记录消失。尚未进行学习者板端验证。

## 实验目的

子进程立即正常退出，返回 7；父进程停在 getchar，暂不调用 waitpid。用另一个终端查看子进程的 /proc/PID/status，确认 State 为 Z；再在原终端按回车，父进程调用 waitpid，取得退出码，之后同一个 /proc/PID 路径应消失。

采用等待回车的方式，给观察留出足够时间。getchar 只是延迟父进程继续执行，本身不回收子进程。如果标准输入结束，程序会继续回收并退出，因此需要在有交互输入的前台运行，不加 &。

## 状态含义

- 父进程尚未退出，在等终端输入，通常可看到 S，表示可中断等待。
- 子进程已退出，但状态尚未被收取，看到 Z，表示僵尸。
- 本例 waitpid 成功后，子进程的回收信息清除，其 /proc/PID 路径通常消失。

睡眠中的进程还能在条件满足后继续运行；僵尸进程已经不再执行代码。回收不是再次杀死进程，也不是 sleep 时间足够长就自动完成。

在本例父进程仍存活、未设置特殊 SIGCHLD 处理的条件下，退出状态保留给父进程收取。显式忽略 SIGCHLD、设置 SA_NOCLDWAIT 等行为不在本次实验中。父进程先结束的重新托管机制将在后续孤儿进程章节说明，不将其等同于本例。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/05_process/02_zombie_wait
cat src/zombie_demo.c
make -n
make
echo $?
file build/zombie_demo
~~~

预期为 ARM 32 位静态程序。

## 准备两个终端

COM12 串口与 SSH 都连接同一块开发板。它们是两个不同的终端，第二个终端可查看进程状态，而第一个终端保留给前台程序输入。

先在 COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/05_process/02_zombie_wait
~~~

在 WSL 当前目录传输：

~~~sh
scp -O build/zombie_demo root@192.168.10.2:/root/linux-study/05_process/02_zombie_wait/
~~~

仍在 WSL 建立第二个板端终端：

~~~sh
ssh root@192.168.10.2
~~~

连接成功后，该终端执行的是板端命令；exit 可回到 WSL。若板端 IP 已丢失，在 COM12 恢复 eth0 的 192.168.10.2/24 后再连接。

## COM12：运行，先不要按回车

~~~sh
cd /root/linux-study/05_process/02_zombie_wait
chmod +x zombie_demo
./zombie_demo
~~~

父进程会打印实际 child_pid 及对应的 cat /proc/PID/status 命令。先等待子进程打印 exiting with 7，不要按回车。父子打印顺序没有固定保证，打印退出提示与实际结束之间也有短暂间隔，所以以 /proc 的 State 为观察证据。

## SSH 终端：查看子进程

复制 COM12 中提示的 cat /proc/实际子PID/status 命令，在 SSH 终端执行。关注 Name、State、Pid、PPid。

State 预期为 Z (zombie)，Pid 对应子进程，PPid 对应父进程。如仍为 R 或 S，稍后重新查看；不能单凭出现一条退出提示就认定已经进入 Z。

可同样用父 PID 查看父进程 status，通常 State 为 S。父进程的 S 与子进程的 Z 是不同生命周期状态。

## COM12：按回车回收

只按一次回车。程序应打印 reaped pid 和 child exit code=7，然后回到 Shell。紧接着执行：

~~~sh
echo $?
~~~

预期父进程退出码为 0。

## SSH：再次查看同一个子 PID 路径

重复之前的 cat 命令，通常会显示 No such file or directory。/proc 是内核提供的进程视图，不能把路径消失理解为我们删除了磁盘上的一个进程文件。若 PID 恰好已复用，检查 Name 和 PPid，不将新进程当成原子进程。

最后输入 exit 离开 SSH，回到 WSL。

## 与上一节 wait 的关系

C 中的 waitpid 由本例父进程回收自己的子进程。Shell 的 wait 内建命令只能等待它管理的相应子任务；在第二个 Shell 中执行 wait 不能替本例父进程收取这个子进程。共享同一块板子不意味着所有进程都具有同一父子关系。

参考：[wait 与僵尸进程](https://man7.org/linux/man-pages/man2/wait.2.html)、[proc/PID/status](https://man7.org/linux/man-pages/man5/proc_pid_status.5.html)。
