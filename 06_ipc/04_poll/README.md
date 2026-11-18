# 实验 06-04：poll 等待就绪、超时与管道挂断

状态：学习者已完整上板验证。父 PID 698、子 PID 699，读端 3、写端 4；第一次回车前连续 poll 超时；第一次回车后 poll 返回 1、POLLIN（0x1），按 4/4/4/4 收到完整 16 字节，read 得到 EAGAIN 后回到 poll 并再次超时；第二次回车后 POLLHUP（0x10），read 返回 EOF、累计 16 字节，回收子退出 0，Shell 紧接检查父退出 0。继续事件位组合与返回计数的理解检查；尚未收到本节 WSL 编译输出，不据此标记编译检查。06-03 五步已上板，学习者已说明理解 errno 的存储归属：observe_read 中访问 errno 不使其成为局部变量；局部的 error 才是 errno 值的副本。

教师隔离验证通过：生成 ARM 静态程序；WSL 本机版本以 -Wall -Wextra -Werror 编译。控制第一次输入前观察到超时，第一次输入后 POLLIN、接收完整 16 字节、EAGAIN 回到 poll，第二次输入后 POLLHUP、EOF、回收且父子退出 0。另验证一次送入两个换行的快速发送/关闭情形，仍接收全部数据并处理 EOF，未依赖固定事件组合。此验证不替代学习者亲手编译或板端执行。

## 非阻塞 read 与 poll 的分工

O_NONBLOCK 让暂时无数据的 read 返回 EAGAIN；poll 让线程在内核中等事件或超时，避免立即重试 read 的忙循环。poll 不读取数据，不将数据复制进接收数组，也不是后台线程；它返回后，调用者仍需 read。

poll 本身可以阻塞。我们把等待集中在可同时关注多个描述符、允许超时的位置；read 则使用非阻塞模式，以便把当前数据读空后返回事件循环。本节先监视一个描述符，建立语义，后续再扩展多个。

## 结构体与参数

~~~c
struct pollfd watch = {
    .fd = pipefd[0],
    .events = POLLIN,
    .revents = 0
};
int ready = poll(&watch, 1, 1000);
~~~

fd 是被监视的描述符；events 是应用输入的事件位掩码；revents 是内核输出的实际事件位掩码。第二个参数 1 是结构体数组的元素个数，不是描述符编号。最后的 1000 是本次等待的毫秒数，不是固定 sleep：事件在期限内到来时可提前返回。

| poll 返回值 | 含义 |
|---|---|
| 大于 0 | revents 非零的数组元素数量，即有事件/错误的描述符数量 |
| 0 | 本次等待超时，没有得到事件 |
| -1 | 调用失败，按 errno 处理；本例 EINTR 重试 |

一个描述符同时出现两个事件位时，返回值仍按一个数组元素计数。本节 nfds=1，因此事件发生时返回 1，不表示收到 1 字节。poll 的 0 是超时，read 非零长度请求的 0 才表示本例 EOF，不能混为一谈。

timeout=-1 表示没有超时限制，0 表示立即检查当前状态，大于 0 表示最多等待相应毫秒（可能因时钟粒度和调度略超时）。O_NONBLOCK 不会把 poll 的超时参数自动改成 0；它们是不同设置。

## 事件位

- POLLIN：有数据可读。
- POLLHUP：管道写端引用全关闭；缓冲仍可能有数据，不能直接丢弃读端。
- POLLERR：错误条件；本例报告为失败。
- POLLNVAL：监视的描述符无效；本例报告为失败。

POLLHUP、POLLERR、POLLNVAL 不需要加入 events，条件成立时内核仍可在 revents 返回它们。事件可以组合，应用用按位与检查，不用 revents == POLLIN 代替所有可读处理。

代码对 POLLIN/POLLHUP 等状态都尝试读取，持续处理正数字节数；遇 EAGAIN 结束当前读取批次、回到 poll；遇 read=0 才按 EOF 结束通道。若 POLLIN 与 POLLHUP 同时出现，仍先读完剩余数据。POLLHUP 本身不保证缓冲已空。

read 非阻塞仍有价值：poll 告诉当前就绪状态，不保证你随后任意多次读取都能得到数据；多线程/多读者还可能改变状态。本例没有竞争读者，但循环读空后需要安全得到 EAGAIN，而不是又卡在 read。

## 本例两次回车控制状态

fork 前建管道并把读端设为非阻塞。子进程关闭读端，父进程关闭写端。

1. 子先在 getchar 等待第一次回车，不写数据，写端保持打开。父 poll 等 1000ms 后打印 timeout，再次进入 poll。
2. 第一次回车后子写入 ABCDEFGHIJKLMNOP 共 16 字节，再等待第二次回车，暂不关闭写端。父得到 POLLIN，循环 read（4 字节数组），读到 16 字节后 EAGAIN，回到 poll。
3. 第二次回车后子关闭写端并退出。父得到 POLLHUP，继续 read 取得 EOF，再关闭读端、回收子进程。

程序开头用 setvbuf 把 stdout 设为无缓冲，只为及时显示观察日志；它不改变管道就绪规则。父子共享同一前台进程组，子从终端读取回车，父只等待管道。请前台执行，不加 &。

若 stdin 已结束，子会继续发送并关闭，仍可退出，但观察窗口会缩短。请按两阶段操作，不要一次粘贴两个换行或在程序运行期间输入后续 Shell 命令。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/06_ipc/04_poll
cat src/poll_demo.c
make -n
make
echo $?
file build/poll_demo
~~~

预期生成 ARM 32 位静态程序。读代码时先看 main 的 poll/read 主循环，再看 child_write 的两个输入阶段，write_all 沿用既有逻辑；show_events 只把位掩码转为便于观察的符号。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/06_ipc/04_poll
~~~

WSL 当前目录传输：

~~~sh
scp -O build/poll_demo root@192.168.10.2:/root/linux-study/06_ipc/04_poll/
~~~

## COM12：分阶段观察

~~~sh
cd /root/linux-study/06_ipc/04_poll
chmod +x poll_demo
./poll_demo
~~~

先等待至少一条 parent: poll timeout，再按第一次回车。观察 POLLIN、依次收到完整 16 字节、read would block; back to poll。在出现该消息之后，再按第二次回车。观察 POLLHUP、read EOF total=16、回收子进程并报告子退出 0。结束后紧接检查：

~~~sh
echo $?
~~~

预期为 0。如果过快按了第二次回车，可能同时出现 POLLIN 和 POLLHUP，或不出现 EAGAIN；也可能首次只报告 POLLIN，随后循环 read 时写端已经关闭，直接读到 EOF，不再发生单独报告 POLLHUP 的下一次 poll。事件与调度可以变化，代码仍应读完 16 字节再处理 EOF。观察符号而不把具体 revents 数值或事件组合写死。

## 等待与忙循环的区别

没数据时用 poll(...,1000) 在内核等待，超时后可做一次周期任务再等待。这里每秒的 timeout 打印是观察动作，实际应用不必一直打印。poll(...,0) 若在紧循环里反复调用，也会造成忙轮询，不自动节省 CPU。

等待中的父进程不能同时沿同一条执行路径做任意其他工作；多路复用的价值是等多个输入中的任意事件，并通过返回事件分派处理，而不是自动获得并行线程。

## 边界和后续

本例父发生致命错误时关闭读端，并尝试终止本例自己创建的子进程后回收，避免子仍停在等待终端输入而使错误路径无休止等待。这是错误清理，不是 poll 工作机制。正常路径不会发送该信号。

当前只处理一个读端，不能据此完整解决 stdout/stderr 双管道的调度、公平性或超大输出问题；后续再用两个 pollfd 同时等待两个通道。普通文件通常始终报告就绪，poll 不能把磁盘文件变成通用异步 I/O；本节对象是管道。

参考：[poll 结构、返回值、事件与超时](https://man7.org/linux/man-pages/man2/poll.2.html)、[管道 EOF 与容量](https://man7.org/linux/man-pages/man7/pipe.7.html)。
