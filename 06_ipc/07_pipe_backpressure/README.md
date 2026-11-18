# 实验 06-07：非阻塞写入、管道容量与背压

状态：学习者已上板验证完整流程：初始 poll=1/POLLOUT=yes；累计写入 65536 字节后 EAGAIN，poll=0/POLLOUT=no；实际取回 65536 字节后 read 得到 EAGAIN；再检查 poll=1/POLLOUT=yes，重新写入 2 字节并读到 OK；Shell 紧接检查退出状态为 0。尚未收到本节 WSL 编译输出，不据此标记编译检查。2026-11-11路线调整：独立信号和IPC进阶专项取消，保留本节，新增08_fifo作为IPC基础收尾。

教师隔离验证通过：ARM 静态程序构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。观察到 POLLOUT 为 yes→no→yes，本机本次累计写入及排空均为 65536 字节，重新写入和读取 OK 成功，程序退出 0。不将本机累计量作为板端容量保证。此验证不替代学习者亲手编译或板端执行。

## 新增知识与已有接口

本节主要新增 POLLOUT：用于关注写入就绪。pipe、fcntl、O_NONBLOCK、read、write、pollfd、poll、EAGAIN 和清理方式都沿用已经学习的接口。没有 fork、exec 或信号处理函数。

同一进程先保留两端，不读取地写入；写到暂时无法继续时，停止发送，再排空管道并观察恢复。整个发送阶段读端始终打开，避免把“没有空间”和“没有读者”混在一起。

## 背压是什么

接收方消耗慢于发送方时，有限的通道缓冲会逐渐占满，发送方不能无限继续。这种接收能力对发送速度的约束称为背压。阻塞写入可能等待；非阻塞写入遇到本次无法完成的条件时可返回 -1/EAGAIN，让应用保存未完成的数据，等条件恢复后再发送。

EAGAIN 不表示本次数据已经排队成功，也不会在后台自动补发。EPIPE 则是无读者的另一种条件，不能当作普通空间不足处理。

## 新事件 POLLOUT

~~~c
struct pollfd watch = {
    .fd = pipefd[1],
    .events = POLLOUT,
    .revents = 0
};
int ready = poll(&watch, 1, 0);
~~~

监视写端，事件从 POLLIN 换成 POLLOUT。timeout=0 只检查当前状态，不等待；本例在三个阶段各检查一次，不是在循环里反复 poll(0) 忙等待。

初始空管道应报告可写；本次写到 EAGAIN 后检查预期没有 POLLOUT；排空之后应重新可写。POLLOUT 不是写入操作，也不承诺任意长度都能一次写完。真正 write 后仍要看实际返回字节数，处理 EAGAIN、错误以及较大请求的部分写入。

## 七步流程

1. 两端设非阻塞，检查空管道写端可写。
2. 每次请求写 256 个 A，按实际返回长度累计 written，首次 EAGAIN 时停止，不重试空转。
3. 检查写端当前不报告 POLLOUT。
4. 用 1024 字节数组不断读取并检查内容，读到 EAGAIN 时说明暂时已读空，得到 drained；写端仍开着，所以不是 EOF。
5. 检查写端重新报告 POLLOUT。
6. 重新发起 write，发送 OK 两字节。
7. 从读端验证收到 OK，最后关闭两端，退出 0。

written 是本次固定写入方式在 EAGAIN 前成功进入管道的累计字节数；drained 是实际取回的累计字节数，两者应相等。不把某台机器的累计量硬编码为所有系统的管道容量。

本节为便于控制状态而把管道彻底读空，不能推断实际程序必须读空整个管道才允许恢复写入。管道内部容量、缓冲槽与写入请求大小共同影响何时能再写，不能用简单减去一个字节来承诺任意新写入一定成功。

256 字节的小块不超过 POSIX 对 PIPE_BUF 的最低保证，适合本节观察。本节不把该数当作消息长度协议；PIPE_BUF 的写入原子性与整个管道容量是不同概念，后续多写者协议再深入。发送时按返回长度累计，读取也不依赖固定分块。

## 阅读代码

- set_nonblocking 是我们写的辅助函数，只包装已经学过的 F_GETFL/F_SETFL。
- probe_writable 也是辅助函数，调用 poll 并打印写端是否报告 POLLOUT。
- main 的第一个循环在有进展时填充，遇 EAGAIN 退出；第二个循环在有进展时排空，遇 EAGAIN 退出。
- 两个循环都没有在 EAGAIN 后立即 continue，因此不把无数据/无空间变成忙循环。
- cleanup 只集中关闭持有的两端。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/06_ipc/07_pipe_backpressure
cat src/backpressure_demo.c
make -n
make
echo $?
file build/backpressure_demo
~~~

预期生成 ARM 32 位静态程序。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/06_ipc/07_pipe_backpressure
~~~

WSL 当前目录传输：

~~~sh
scp -O build/backpressure_demo root@192.168.10.2:/root/linux-study/06_ipc/07_pipe_backpressure/
~~~

## COM12：运行

~~~sh
cd /root/linux-study/06_ipc/07_pipe_backpressure
chmod +x backpressure_demo
./backpressure_demo
echo $?
~~~

预期 POLLOUT 呈 yes→no→yes，accepted_total 与 drained_total 相等，恢复写入返回 2 并读取 OK，退出 0。此处无须输入回车，也不创建磁盘数据文件。

## 与实际发送循环的联系

理解补讲：学习者正确选择在 write 已返回 4096 后，从待发送数据偏移 4096 继续，但将理由解释为管道内的共享偏移，当前需纠正。这个偏移是发送程序自己的累计进度，不是普通文件那样可 lseek 的内核文件偏移。管道按 FIFO 保存尚未被消费的字节；读端已经消费多少，不能仅从发送者 write 的返回值推出。

若一段待发送数据共有 8192 字节，先成功写入 4096 字节，再遇 EAGAIN，发送程序应保留 sent=4096，随后 write(fd, data+sent, 8192-sent)。成功返回正数才累加 sent，EAGAIN 不推进也不清零。读者取走数据只会改变缓冲内容与可用空间，不会要求发送程序重发已被内核接受的前 4096 字节。write 成功也不等于读者已经读取或处理完毕。

实际应用通常由其他进程读取。发送者遇 EAGAIN 后保留尚未发送的数据及偏移，关注 POLLOUT；获得机会后继续发送，发送完则取消不需要的 POLLOUT 关注。不能一直关注本来就可写的空闲通道而没有数据可发，否则 poll 可能持续立即返回。

对较大非阻塞写入，若先成功发送一部分、下一次 EAGAIN，应保存进度；不能从消息开头全部重发。本节先建立空间不足与恢复的状态模型，后续再实现带发送缓冲的完整事件循环。

参考：[管道容量与非阻塞写入](https://man7.org/linux/man-pages/man7/pipe.7.html)、[POLLOUT 与等待参数](https://man7.org/linux/man-pages/man2/poll.2.html)。
