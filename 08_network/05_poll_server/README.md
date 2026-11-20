# 实验08-05：poll同时照看监听与两条连接

状态：2026-11-18收到板端及WSL结果。服务端接受两条连接，fd分别4/5，active达到2；第二条连接回显16字节后先EOF并关闭，active仍1；第一条随后EOF，active为0，最终总32字节。A使用hold，A/B均收到16字节、matches=yes；服务端与两客户端均紧接退出0。已验证第二条连接可在第一条保持打开时完成回显与关闭。学习者正确解释MSG_DONTWAIT使单次recv不阻塞，空闲时等待poll，B连接使监听项就绪；已补充具体为watches[0].revents的POLLIN，这是就绪事件，区别于SIGINT等信号，等待期间线程休眠。尚未收到学习者编译日志。上一节已正确解释顺序服务端阻塞当前连接read会挡住下一次accept。

教师验证：在.validation/project以-Wall -Wextra -Werror构建ARM32位静态服务端及WSL x86-64客户端，另编译-O2本机服务端。四组回环验证通过：A hold期间B收到回显且退出0，随后A释放、服务端退出0，总32字节；顺序两客户端也共32字节并退出0；1025字节含NUL二进制数据跨多轮接收并在EOF前全部回显，第二条空连接正常关闭；第一条连接复位后清理并继续服务第二条，最终退出1报告既往错误。hold观察保存在.validation/hold_observation.txt。教师验证与上面的学习者板端记录分开，未代替学习者执行正式编译或板端命令。

## 本节先解决什么

同一个服务端进程、同一个线程，用poll等待监听套接字以及已接入的客户端。A收到回显后保持连接，B仍应完成连接、发送、回显、关闭，无需等A按回车。本节固定累计接受两条连接，第二条接入后关闭监听套接字，继续处理已接入连接；两条都关闭后正常退出。不是永久运行或可无限接入的服务。

这是一节接收事件分派实验。发送继续沿用send_all，发送方向仍是阻塞式；默认两个客户端各发16字节且及时读回显。若对端长期不读又持续造成大量回显，send_all仍可能阻塞整个循环，完整的非阻塞发送、待发送缓冲与POLLOUT下一节讲解。本节不承诺任意负载下各连接都互不影响。

## 从独占一条连接，变成每轮照看就绪项

上一节：accept A后，在A自己的读写循环里等待，直到A结束才处理B。

本节的事件循环：

```text
poll等待一组fd
    监听fd有POLLIN → accept一次，将新连接登记进数组
    连接A有接收事件 → 尝试接收一次，若有数据就回显
    连接B有接收事件 → 尝试接收一次，若有数据就回显
回到poll
```

没有事件的连接不调用接收函数。A空闲时，程序可以阻塞在poll；B的连接或数据到来能使poll返回。poll不创建线程，也不会替应用读取、accept或发送；每次回显处理仍由同一线程依次执行。

每轮每个就绪连接最多接收一次，是复用06_ipc/05_poll_streams的公平处理方法。一次读到64字节后，即使该连接还有数据，也先给数组里的其他就绪连接机会；下一轮poll仍能报告未读数据。不能在当前连接内一直等到EOF再返回，否则又回到上一节的问题。

## poll数组与变量逐个说明

```c
struct pollfd watches[3];
```

struct pollfd来自poll.h，之前在管道章节已经用过；每个元素包含fd、events、revents。

| 项目 | 含义 |
|---|---|
| watches[0] | 监听socket，POLLIN表示可以尝试accept |
| watches[1] | 本次第一个接入的连接，POLLIN表示可以尝试接收 |
| watches[2] | 本次第二个接入的连接 |
| events=POLLIN | 应用希望关注的事件 |
| revents | poll本次返回的实际事件，按位检查，可同时出现多个标志 |
| fd=-1 | 该数组项暂不监视；poll会忽略它，不是自动执行close |
| accepted | 累计接入多少条连接，关闭连接不减少它 |
| active | 目前仍由服务端保持打开的连接数，接入加1、关闭减1 |
| totals[1]/totals[2] | 各连接累计回显字节数，和数组项索引对应 |

数组索引和文件描述符是两个概念：watches[1].fd可能是4，watches[2].fd可能是5；若第一条先关闭，也可能复用成4。索引表示我们存放哪一条连接，fd是系统分配的整数。本示例固定两条连接各用一个项，不重复利用数组项容纳第三条连接。

```c
int ready = poll(watches, 3, -1);
```

第二个参数3是数组元素数，不是当前active数，也不是最大fd加1。第三个参数-1表示无限等待事件，避免无事可做时忙转。成功返回值是revents非零的元素数；-1表示错误，EINTR可以重试。遍历时检查整个数组里的revents，不把ready当作最后一个有效索引。

新增连接注册时先将revents初始化0，因为刚完成的那次poll还没监视这个新连接；下一轮poll才获取它的接收事件。close连接后将fd改为-1，防止继续监视已失效或被复用的编号。

## recv与MSG_DONTWAIT，先理解再看代码

来自sys/socket.h：

```c
ssize_t recv(int fd, void *buffer, size_t length, int flags);
```

fd、buffer、length与read含义相同。对本节TCP流，flags为0时接收行为类似read；第四个参数允许附加选项。

```c
ssize_t n = recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT);
```

MSG_DONTWAIT只让这一次接收按非阻塞方式进行，不把整个fd改成非阻塞。返回值：

- n>0：实际收到的字节数，可能小于缓冲区大小；回显这一部分后返回事件循环。
- n==0：接收方向EOF；本节回显协议已发回此前读到的数据，所以关闭这条连接。
- n==-1且errno为EAGAIN/EWOULDBLOCK：现在没数据，也未读到EOF；保留连接，回到poll。
- EINTR：本示例也保留连接，回到poll等待或重试。
- 其他错误：报告错误并关闭当前连接，继续照看其他连接，最终退出状态1表明曾有错误。

poll提供就绪通知，接收仍以实际返回值为准。MSG_DONTWAIT保证接收时即使已无数据也不等待。这样不需要为了此次接收修改连接的发送行为；阻塞send_all的限制见本节开头。

监听fd则复用已经学过的fcntl(F_GETFL)、fcntl(F_SETFL,flags|O_NONBLOCK)，保证accept尝试没有可取连接时返回EAGAIN而不是阻塞。Linux的accept返回的连接fd不会继承监听fd的O_NONBLOCK；本节连接接收依靠MSG_DONTWAIT。

## 两个自定义函数与挂断处理

send_all在include/net_common.h中，沿用前几节，处理短写、EINTR和MSG_NOSIGNAL。

echo_once在src/poll_server.c中，是自己的函数，不是系统API。它尝试recv一次；有数据则回显并更新total指向的计数。返回1表示保留监视、0表示EOF、-1表示连接错误；这套三值是我们自己的约定，不等于recv的原始返回值。主循环据此决定是否close和active减1。

POLLIN、POLLHUP或POLLERR出现时都尝试接收，以实际返回值决定。即使POLLHUP与POLLIN同时出现，也不能只看到挂断就直接丢掉尚未读取的数据。本例一次接收后回poll，直到后续recv返回0才按正常EOF关闭。POLLNVAL表示描述符无效，作为程序错误报告和清理，不能把它当正常EOF。

## 源码阅读顺序

1. 客户端src/poll_client.c：与上一节同样的默认/hold行为，只改为端口8892。
2. 服务端main初始化socket与fcntl，登记watches[0]。
3. 主while条件：累计还没接满2条，或者仍有活动连接，就继续poll。
4. 监听就绪：accept一次，登记新连接；接满2条后close监听项并设fd=-1。
5. for遍历连接项，调用echo_once，按返回值保留或关闭，回到poll。

## 亲手编译：WSL

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/05_poll_server
make -n
make
echo $?
file build/arm/poll_server build/host/poll_client
```

make应退出0；服务端应ARM 32位静态程序，客户端应WSL x86-64程序。两份产物由Makefile中的不同编译器构建。

## COM12创建目录，WSL传输

COM12：

```sh
mkdir -p /root/linux-study/08_network/05_poll_server
```

若板子重启后eth0没有192.168.10.2，在COM12恢复：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up
```

WSL本节目录：

```sh
scp -O build/arm/poll_server root@192.168.10.2:/root/linux-study/08_network/05_poll_server/
```

## 主实验：A不关闭，B仍应完成

需要COM12和两个WSL本机终端A/B。A/B不是SSH登录开发板的终端。

COM12：

```sh
cd /root/linux-study/08_network/05_poll_server
chmod +x poll_server
./poll_server
```

看到监听端口8892后，WSL A：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/05_poll_server
./build/host/poll_client 192.168.10.2 hold
```

A收到16字节、matches=yes，提示press ENTER to close，先不要回车。

WSL B：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/05_poll_server
./build/host/poll_client 192.168.10.2
echo $?
```

关键观察：A仍停在回车提示处，B已经收到16字节、matches=yes、关闭且退出0。COM12应在A未关闭时已accepted client 2，第二个连接EOF后active=1；此时服务端仍未退出，因为A还开着。

回到A按回车，A关闭，紧接echo $?应0；COM12读到A的EOF、active=0，最终accepted=2、echoed_total=32 bytes并退出，紧接echo $?应0。连接1与2的EOF顺序可以是先2后1；数组索引从小到大遍历不要求连接必须按接入顺序退出。

本轮不要求所有日志行顺序固定，read也不保证一次恰好16字节。完成标准是A保持期间B已经拿到完整回显，最终两连接累计32字节且退出0。若提示bind Address already in use，先检查旧实例是否尚在运行，不要反复启动同端口服务端。

## 理解检查

A收到回显后保持连接、B尚未启动时，没有数据或新连接需要处理，服务端此刻主要阻塞在poll还是recv？B连接后，为什么这个等待能结束？

参考：[poll数组、事件与等待](https://man7.org/linux/man-pages/man2/poll.2.html)、[accept与非阻塞监听](https://man7.org/linux/man-pages/man2/accept.2.html)、[recv与MSG_DONTWAIT](https://man7.org/linux/man-pages/man2/recv.2.html)、[send可能因发送缓冲不足阻塞](https://man7.org/linux/man-pages/man2/send.2.html)。
