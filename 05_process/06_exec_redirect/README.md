# 实验 05-06：fork、dup2、exec 组合实现输出重定向

状态：学习者已上板验证成功路径：父 PID 3099、子 PID 3100；父 stdout、child setup 及新程序 stderr 留在串口；child_stdout.txt 中只有新程序 stdout，PID=3100、PPid=3099、argc=3、参数正确；父回收取得子退出码 7，Shell 紧接检查父退出状态为 0。等待两类失败路径与理解检查；尚未收到本节 WSL 编译输出，不据此标记编译检查。上一节已讲清普通赋值 saved_stdout=1 不新增描述符表项，不能保留原终端引用；dup(1) 才创建另一份引用。恢复操作中的 dup2(1, 1) 是无操作，不会恢复旧目标。

教师隔离验证通过：两个 ARM 静态程序均构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。成功路径验证父 stdout 保留、子 stdout 仅入文件、子 stderr 保留、exec 前后子 PID 一致，子返回 7 且父回收退出 0；缺失程序路径验证输出文件被创建/截断但为空、子退出 127；缺失输出父目录验证子退出 126 且目标未运行。两类失败均回收子进程，父按本例设计退出 0。尚未进行学习者板端验证。

学习者失败路径 A 已上板验证：父 PID 20338、子 PID 20339，child execl 报 No such file or directory；父回收得到子退出码 127，父退出 0；exec_failed.txt 已创建且为 0 字节，cat 无输出。该结果确认输出文件准备成功不会保证目标程序启动成功；等待失败路径 B（输出父目录缺失）。

## 本节要建立的流程

父进程 fork；子进程 open 输出文件，用 dup2 把自己的描述符 1 改为引用文件，关闭多余的原 file_fd，再 execl 启动 args_demo。父进程不修改自己的描述符 1，继续向原目标打印，并通过 waitpid 回收子进程。

~~~text
fork 前：父[1] → 原标准输出

fork 后：父[1] → 原标准输出
         子[1] → 原标准输出

子 dup2 后：父[1] → 原标准输出
            子[1] → 输出文件

子 exec 后：父仍运行启动器，子进程改为运行 args_demo
            args_demo 的 stdout → 子[1] → 输出文件
~~~

描述符表在普通 fork 后各自拥有；复制来的表项最初可以共享打开文件描述，但修改子表中的某个表项，不会替换父表对应表项。注意与上一节共享偏移的区别：read/lseek 修改引用对象的状态，dup2 则改变指定描述符表项的引用关系。

## exec 为什么没有让重定向消失

exec 替换旧程序的代码、普通变量、栈等程序映像；打开的文件描述符默认保留，除非设置了 FD_CLOEXEC。这里子进程先设置好 1 的引用关系，成功 exec 后新程序继续用这个编号输出。

旧的 C 库 stdout 对象和缓冲不会作为旧程序状态继续存在，新程序有自己的标准流初始化过程；可以继承的是内核描述符。不要解释为旧 FILE 对象跨 exec 保留。

## 为什么子进程不用保存并恢复终端

父进程自己的 1 一直保留原目标。子进程从一开始就准备运行新程序，其标准输出应持续写文件；程序结束后进程退出，无须恢复到旧启动器。因此本节不调用 dup(1) 保存终端，也不在 exec 后安排成功恢复代码。

## 三个描述符相关动作

~~~c
file_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(file_fd, STDOUT_FILENO);
close(file_fd);
~~~

本例通常先得到 file_fd=3，dup2 后 1 与 3 都引用输出文件，关闭 3 后 1 继续引用文件。代码判断 file_fd != STDOUT_FILENO 才 close，避免源与目标碰巧相同就把 1 自己关闭。本次标准输出在启动前已打开，因此通常不触发这个边界。

O_TRUNC 会清空既有输出文件，重跑不累加。> 的常见截断行为与此对应；>> 通常改用 O_APPEND。重定向只改引用关系，截断/追加由打开选项决定。

## 两份代码及输出区分

exec_redirect 是启动器，接收程序路径与输出路径。args_demo 沿用前面打印 PID、参数并返回 7 的程序，本节增加一条 fprintf(stderr, ...) 作为对照。

父进程的 printf 输出留在原标准输出；子进程的设置诊断和新程序的 stderr 留在原标准错误；新程序的 stdout 写入输出文件。直接在 COM12 前台运行时，前两类显示在串口。

子进程在重定向前只向 stderr 写诊断，父进程在 fork 前显式 fflush(stdout)，防止复制未提交的 stdout 内容。args_demo 从 main 正常 return 7，C 库会刷新它自己的输出；启动器的失败路径使用 _exit，旧 stdout 不积累需要提交的子输出。

## 退出码约定

父进程保持本课程既有设计：回收并打印子结果，随后自己返回 0，不自动传播子退出码。子输出准备失败时主动 _exit(126)，execl 常见失败返回时主动 _exit(127)，目标程序正常执行后返回 7。126/127 是此代码的约定，不是 open/dup2/exec 的返回值，不能仅凭一个数可靠区分所有执行结果。

## WSL：阅读与亲手构建

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/05_process/06_exec_redirect
cat src/exec_redirect.c
cat src/args_demo.c
make -n
make
echo $?
file build/exec_redirect build/args_demo
~~~

预期生成两个 ARM 32 位静态程序。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/05_process/06_exec_redirect
~~~

WSL 当前目录传输：

~~~sh
scp -O build/exec_redirect build/args_demo root@192.168.10.2:/root/linux-study/05_process/06_exec_redirect/
~~~

## COM12：成功路径

~~~sh
cd /root/linux-study/05_process/06_exec_redirect
chmod +x exec_redirect args_demo
./exec_redirect ./args_demo child_stdout.txt
echo $?
cat child_stdout.txt
~~~

启动时串口应看到 parent before fork、parent 的原输出仍在、child setup、new program stderr、reaped 与子退出码 7，父退出为 0。父子之间的部分打印先后可能变化，不用其偶然顺序判断调度。

执行 cat 后才看到 new program: pid=...、argc=3、argv[0]=args_demo、argv[1]=hello、argv[2]=i.MX6ULL，以及 returning 7。文件中的 PID 应等于 child setup 和 parent child_pid 中的子 PID；它的 ppid 应为父 PID。文件不应混入父进程输出或 stderr 诊断。

## 失败路径 A：目标程序不存在

~~~sh
./exec_redirect ./missing_exec_parent_for_demo/args_demo exec_failed.txt
echo $?
ls -l exec_failed.txt
cat exec_failed.txt
~~~

前提是指定父目录确实不存在。预期 stderr 报 child execl，子退出 127；父仍回收，父退出 0。输出文件已在 exec 前成功创建/截断，但目标没有运行，因此预期文件为 0 字节。文件存在不代表程序启动成功。

## 失败路径 B：输出文件的父目录不存在

~~~sh
./exec_redirect ./args_demo ./missing_output_parent_for_demo/out.txt
echo $?
~~~

前提是输出父目录不存在。预期 child open output 报错、子退出 126，没有目标程序输出；父进程仍回收且退出 0。open 的 O_CREAT 只能创建文件，不能自动创建缺失的父目录。

## 与 Shell 的关系及边界

在 Shell 中执行 ./args_demo > child_stdout.txt 时，Shell 把重定向语法解释为描述符操作；目标程序不需要知道 > 的存在。本节用自己的启动器显式完成类似操作，不调用 system，也不启动额外 Shell。

这个模型是外部命令 stdout 重定向的基础，不覆盖 Shell 内建命令、管道、作业控制及不同 Shell 的实现优化。下一步可以将输出改接管道，实现父进程通过 read 直接取得子进程输出；在此前需理解管道方向和关闭不需要的端点。

参考：[execve 描述符保持](https://man7.org/linux/man-pages/man2/execve.2.html)、[dup2](https://man7.org/linux/man-pages/man2/dup.2.html)、[waitpid](https://man7.org/linux/man-pages/man2/wait.2.html)。
