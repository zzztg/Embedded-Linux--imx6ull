# 实验 06-03：非阻塞管道读取与 EAGAIN

状态：学习者已完成五步板端验证。管道读端 3、写端 4；read nonblocking=yes、write nonblocking=no；第 1 步与第 4 步空读均返回 -1、errno=11（EAGAIN/EWOULDBLOCK）；第 2、3 步分别读到 AB、CD，各 2 字节；关闭写端后第 5 步返回 0（EOF）；Shell 紧接检查程序退出状态为 0。继续 EAGAIN、errno 使用与避免忙轮询的理解讲解；尚未收到本节 WSL 编译输出，不据此标记编译检查。此前已正确区分 EOF 与进程退出：目标先关闭 stdout、再 sleep 时，父可取得 EOF，但 waitpid(..., 0) 仍会等待目标退出；waitpid 可以提前调用，并不是子退出后才允许调用。

教师隔离验证通过：ARM 静态程序构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。限时运行确认不会等待管道数据：读端非阻塞 yes、写端 no；依次出现 EAGAIN、AB、CD、EAGAIN、EOF，程序退出 0。两次 EAGAIN 不计为整个实验失败。此验证不替代学习者亲手编译或板端执行。

## 本节先固定一个进程

创建一条管道，由同一个进程持有两端，明确控制写入、读取、关闭顺序。本节不 fork，也不 exec，用来排除调度顺序影响，集中观察非阻塞 read 的返回语义。后续 poll 实验再与异步产生数据的其他进程配合。

若读端仍是默认阻塞模式，空管道且写端开着时，本程序的第一次 read 会等待，本进程也就走不到后面的 write。本节先设置 O_NONBLOCK，因此这次调用返回 EAGAIN，主程序才能继续向后执行。

## 设置方式

~~~c
int flags = fcntl(pipefd[0], F_GETFL);
fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
~~~

实际代码检查每个调用的失败。F_GETFL 成功返回现有访问模式和状态标志，失败返回 -1；F_SETFL 成功返回 0，失败返回 -1。通过按位或在原标志上添加 O_NONBLOCK，保留其他已有、可设置的状态标志，而不是仅传一个标志覆盖它们。

F_SETFL 不会把只读描述符改成可写，访问模式不能用这种方式切换。O_NONBLOCK 是打开文件描述的状态标志，fork/dup 产生的同一端引用共享它，不是各自独立的 FD_CLOEXEC 描述符标志。

普通 pipe 的读端和写端是不同的打开文件描述。本例仅给读端设置非阻塞，不会同时把写端也设为非阻塞；程序再次 F_GETFL 查询两端并打印 yes/no 验证。写端只写 4 字节且有空间，不需要制造写满行为。

## 最重要的状态表

以下请求长度均为非零，且没有其他竞争读者：

| 管道状态 | 默认阻塞 read | O_NONBLOCK read |
|---|---|---|
| 有可读数据 | 返回已读字节数 | 返回已读字节数 |
| 空，仍有写端引用 | 等待数据或结束 | 返回 -1，errno=EAGAIN（检查兼容的 EWOULDBLOCK） |
| 空，所有写端引用关闭 | 返回 0，EOF | 返回 0，EOF |

非阻塞改变的是暂时无法完成时的等待方式，不取消 EOF，也不意味着成功读取总能满足请求的全部长度。系统调用不等待管道数据到来，但不代表整个程序没有任何可等待的操作，也不保证严格固定的执行时长。

## read 的返回值和 errno

~~~c
ssize_t count = read(fd, buffer, sizeof(buffer));
if (count == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* 本次没有数据，不是 EOF；这次调用已经结束 */
    } else {
        /* 其他错误，分别处理 */
    }
}
~~~

仅在 read 返回 -1 时检查 errno；成功调用不必把旧 errno 清零，不能根据残留 errno 判断一次成功读取失败。本例 EINTR 重试，EAGAIN 则打印一次结果并返回调用者，不在这个函数里空转。

理解补讲：学习者正确判断 read 返回 2、errno 残留 11 不构成读取失败，但表示尚不清楚原因，当前停留讲解。read 的返回值负责报告本次结果（正数、0、-1）；只有 -1 这一失败分支下，errno 才用于说明本次失败原因。成功函数没有把 errno 清成 0 的保证，甚至允许修改 errno，因此 errno 非零不能独立作为失败证据。允许出现先 read=-1/errno=EAGAIN，随后 read=2/errno 仍为 EAGAIN 的状态；后者仍表示已经读取 2 字节。检查 errno 的入口条件必须是该接口约定的失败返回值，对 read 来说就是 -1。

perror 读取调用时的 errno。若错误处理前还要做日志或其他可能改动 errno 的函数调用，应在失败后立即把 errno 保存到局部变量。这里先讲 read 的清晰返回约定；有些接口成功返回值也可能与错误标志重叠，需另按各自文档处理，不能把 read 的规则机械推广为所有 C 函数。

EAGAIN 表示这次条件不满足，read 返回后没有在后台替你等待完成。数据到来后要另行 read，或用后续 poll 等机制等待就绪。它与异步 I/O 的提交完成模型不同。

Linux 本实验常见 errno 数值为 11，但代码应比较符号常量，不写死数字。EAGAIN 与 EWOULDBLOCK 在 Linux 上通常相同，检查两者有利于通用代码。

## 程序的五次观察

1. 空管道，写端开着：读返回 -1，EAGAIN。
2. 写入 ABCD 共 4 字节，然后最多读取 2 字节：得到 AB。
3. 再最多读取 2 字节：得到 CD。
4. 已读空，写端仍开着：再次返回 -1，EAGAIN。
5. 关闭写端，再读空管道：返回 0，EOF。

此例是单写者、一次性将 4 字节写入空管道后才开始读，便于观察；一般应用仍应按 read 的实际长度处理，不把每次长度或消息边界写死。打印 %.*s 用于本例无内嵌 NUL 的 ASCII 字节，不要求接收数组有字符串结束符。

源代码的 goto cleanup 只用于错误时集中关闭仍持有的描述符，不是重试循环。write_all 沿用之前的完整写入方法；读观察在 observe_read 中，main 负责安排状态变化。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/06_ipc/03_nonblock_pipe
cat src/nonblock_demo.c
make -n
make
echo $?
file build/nonblock_demo
~~~

预期生成 ARM 32 位静态程序。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/06_ipc/03_nonblock_pipe
~~~

WSL 当前目录传输：

~~~sh
scp -O build/nonblock_demo root@192.168.10.2:/root/linux-study/06_ipc/03_nonblock_pipe/
~~~

## COM12：观察返回值

~~~sh
cd /root/linux-study/06_ipc/03_nonblock_pipe
chmod +x nonblock_demo
./nonblock_demo
echo $?
~~~

预期看到 read nonblocking=yes，write nonblocking=no，接着是 EAGAIN、AB、CD、EAGAIN、EOF 五次读取结果，程序退出 0。两次 read=-1 是实验中正常的暂时无数据分支，不使整个程序退出失败。

## 为什么不直接 while 重试 EAGAIN

如果不停再次 read，长期没数据时程序可能持续消耗 CPU。非阻塞 I/O 把何时重试的决定交给应用，不自动帮应用避免忙循环。固定 sleep 轮询可能增加响应延迟；下一节用 poll 让内核等待一个或多个描述符就绪，再尝试读取，并仍处理 EAGAIN 等实际返回值。

本节只观察管道的非阻塞读取，不推广成普通磁盘文件的普遍异步行为；O_NONBLOCK 对普通文件通常不提供同样的等待规避效果。非阻塞写入、容量和 SIGPIPE/EPIPE 后续再分别实验。

参考：[fcntl 的 F_GETFL/F_SETFL](https://man7.org/linux/man-pages/man2/F_GETFL.2const.html)、[read 的返回值与 EAGAIN](https://man7.org/linux/man-pages/man2/read.2.html)、[errno 的使用约定](https://man7.org/linux/man-pages/man3/errno.3.html)、[管道语义](https://man7.org/linux/man-pages/man7/pipe.7.html)。
