# 实验 05-05：dup、dup2 与标准输出重定向

状态：学习者已完成两个程序的板端验证，退出状态均为 0。dup_demo 中原 fd=3、副本 fd=4，依次读取 ABCD（0→4）、EFGH（4→8），关闭原描述符后副本继续读 IJKL（8→12）。redirect_demo 中 file_fd=3、saved_stdout=4；before/restored 两行留在终端，stderr 诊断留在终端，cat 输出文件时仅看到 stdout: this line goes to the file.。继续检查保存描述符、复制整数及引用关系的理解；尚未收到本节 WSL 编译输出，不据此标记编译检查。上一节两种打开模式均已上板，学习者已正确推演：shared 模式子进程读后 lseek 到 0，父进程再读将取得 ABCD；需使用准确表述，改变的是共享偏移，fd 整数仍不变。

教师隔离验证通过：两个 ARM 静态程序构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。dup 模式验证 0→4→8→12 的共享偏移、关闭原描述符后副本仍可读，输入不变；重定向模式验证 before/restored 两行留在原 stdout，诊断留在 stderr，输出文件精确包含中间一行并清除旧内容。本机以捕获管道充当原 stdout 还验证了恢复的是原目标，不是固定串口。验证不替代学习者亲手编译或板端运行。

## dup：同一进程中的两个引用

~~~c
int fd = open(path, O_RDONLY);
int copy_fd = dup(fd);
~~~

dup 返回当前进程中最小的未使用描述符，两项引用同一打开文件描述，因而共享偏移与文件状态标志。它不复制磁盘文件内容，不重新 open，也不创建新进程。若本例只占用标准描述符，fd 通常为 3，copy_fd 为 4，但不要写死数字。

~~~text
当前进程表[3] ─┐
               ├→ 同一个打开文件描述（共享 offset）→ input.txt
当前进程表[4] ─┘
~~~

dup_demo 通过原描述符读 ABCD（0→4），通过副本读 EFGH（4→8），关闭原描述符后通过副本读 IJKL（8→12）。这证明同一进程中的不同描述符也能共享偏移，关闭一项不使另一项失效。输入保持不变。

## dup2：选择要使用的目标编号

~~~c
dup2(oldfd, newfd);
~~~

让 newfd 引用 oldfd 所指向的同一个打开文件描述，oldfd 保持有效。成功返回 newfd，失败返回 -1，不是成功返回 0。

oldfd 有效且与 newfd 不同时，若 newfd 已打开，则内部关闭其原引用再绑定新引用；关闭与重新绑定对描述符分配是原子的。不应先自行 close(newfd) 再 dup(oldfd)，否则中间空位可能被线程或信号处理中的其他分配占用。这里原子性针对描述符操作，不代表所有输出都无缓冲或多进程输出自动有序。

若 oldfd 有效且两参数相等，dup2 是无操作并返回该编号；oldfd 无效则失败，且不会因此关闭 newfd。

## stdout 怎样转向文件

标准输出最初通常通过描述符 1 连接终端。本例不使用 freopen，不更换 stdout 的 FILE 对象，而是改变描述符 1 的引用关系。

redirect_demo 顺序为：

1. 打印重定向前的信息，并 fflush(stdout)。
2. open 输出文件（本例 O_TRUNC 会清空该输出文件）。
3. dup(1) 保存原标准输出的另一份引用。
4. dup2(file_fd, 1)，使标准输出指向文件。
5. close(file_fd)，只保留描述符 1 对该输出文件的引用。
6. printf 一行，并 fflush(stdout)，确保这行按当前指向写入文件。
7. fprintf(stderr, ...) 仍走描述符 2。
8. dup2(saved_stdout, 1) 恢复原标准输出，随后关闭保存的副本。
9. 最后一条 printf 又输出到原位置。

~~~text
重定向前：1 → 原标准输出；file_fd → 文件；saved_stdout → 原标准输出
重定向后：1 → 文件；file_fd → 文件；saved_stdout → 原标准输出
关闭源项：1 → 文件；saved_stdout → 原标准输出
恢复之后：1 → 原标准输出
~~~

本例在 COM12 直接前台运行时，原位置是串口终端；如果调用者原本已将 stdout 指向管道或其他文件，恢复的也是那个原目标，并非强制打开串口。

## 为什么在改变目标前先刷新

C 库缓冲中尚未提交的字节不会记住输出时想去的旧目标。若先把 1 改到文件，再刷新以前积累的内容，它可能被送入文件；反过来，恢复 stdout 前不刷新，原本打算写文件的内容也可能在恢复后发到终端。因此在切换引用之前把待输出内容提交，是本例必要的边界处理。

不要假定 dup2 后 C 库会自动重新决定缓冲模式，也不要只依赖换行；这里显式 fflush。它只提交 C 库缓冲，不等于 fsync。printf 使用 stdout（FILE 包装及缓冲），STDOUT_FILENO 则是整数 1，这两者要区分。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/05_process/05_dup_redirect
cat src/dup_demo.c
cat src/redirect_demo.c
make -n
make
echo $?
file build/dup_demo build/redirect_demo
~~~

两个独立程序分别链接，预期均为 ARM 32 位静态程序。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/05_process/05_dup_redirect
~~~

WSL 当前目录传输：

~~~sh
scp -O build/dup_demo build/redirect_demo root@192.168.10.2:/root/linux-study/05_process/05_dup_redirect/
~~~

## COM12：先看 dup 的共享偏移

~~~sh
cd /root/linux-study/05_process/05_dup_redirect
chmod +x dup_demo redirect_demo
printf 'ABCDEFGHIJKLMNOP' > input.txt
./dup_demo input.txt
echo $?
~~~

预期依次读 ABCD、EFGH、IJKL；第三次是在原描述符关闭后通过副本读取；退出 0。

## COM12：再看输出重定向

~~~sh
./redirect_demo redirected.txt
echo $?
cat redirected.txt
~~~

程序直接运行期间，原标准输出位置应看到 before redirection 和 restored to original destination；stderr 的 setup 与 still at original destination 两行也留在原标准错误位置。输入 cat 命令之后才在终端显示文件内的一行：

~~~text
stdout: this line goes to the file.
~~~

退出状态应为 0。本例没有为程序命令加 >，是程序内部改变自己的描述符实现重定向。子程序进程的描述符修改不会修改启动它的 Shell 的描述符表；本例恢复原 stdout 是为了让同一程序后续输出回到原位置。

## 边界与后续

FD_CLOEXEC 是描述符标志，副本不与原项共享；dup 的新项默认未设置它，dup2 在两编号不同且成功复制时也清除目标项的该标志。它与共享在打开文件描述中的 O_APPEND 不同，后续结合 exec 观察。

本节没有 fork 或 exec，先把当前进程内的操作建立起来。后续将 fork、dup2、exec 配合，实现由父进程启动另一个程序并捕获其标准输出，进一步理解 Shell 的 >。

参考：[dup 与 dup2](https://man7.org/linux/man-pages/man2/dup.2.html)、[标准流与描述符](https://man7.org/linux/man-pages/man3/stdin.3.html)、[fflush](https://man7.org/linux/man-pages/man3/fflush.3.html)。
