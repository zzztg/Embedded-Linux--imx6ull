# 实验08-07：带有界输出缓冲的非阻塞回显

状态：2026-11-19收到板端及WSL结果。服务端接受两条连接，均观察到输出缓冲满并暂停接收；B输入EOF时pending=1，随后output drained after EOF，再以received=queued=1025、full_count=16关闭，active仍1；A之后EOF时pending=0，累计1025字节并关闭。最终received_total=queued_total=2050；A/B均echo matches=yes，B紧接退出0。未贴A、服务端退出状态及学习者编译日志。学习者正确解释input_eof1、length50、sent30表示输入已读尽但仍需发送20字节，全部回显后才能close；补充保留POLLOUT、撤销POLLIN，从output+30继续，短写/EAGAIN仍保存进度。主实验与关闭条件关键理解检查通过。08-06已正确理解短写及EAGAIN保留进度。

教师验证：在.validation/project以-Wall -Wextra -Werror构建ARM32位静态服务端及WSL x86-64客户端，另编译-O2本机服务端。四组回环验证通过：A hold期间B完整回显1025字节并先关闭，之后A半关闭并完整回显，总收发2050字节、三端退出0；预置1025字节二进制数据和FIN，确定观察到EOF时仍有待发数据，随后发完再关闭，第二条空连接也通过；128 KiB+17字节的不同内容上传、慢读取期间另一客户端仍完整完成，恢复后大流量逐字节一致；连接复位后清理当前连接并服务下一条，最终退出1报告既往错误。观察分别保存在.validation/hold_observation.txt与eof_pending_observation.txt。教师验证与上面的学习者板端记录分开，未代替学习者执行正式编译或板端命令。

## 本节组合什么

05节接收后直接调用阻塞send_all；06节只发送预先生成的数据。本节恢复流式回显，将每次recv得到的数据存入当前连接自己的输出缓冲，由后续send逐步提交。接收与发送都使用已学O_NONBLOCK，复用poll、fcntl、recv、send、shutdown、setsockopt，没有新增系统API。

每条连接只有64字节应用输出缓冲。它限制应用暂存量，不限制整条连接只能传64字节；发完一批就能复用。内核还有自己的TCP缓冲，这里的64字节仅指应用数组。

客户端默认发送1025字节二进制数据，内容包含NUL，发送方向结束后仍接收全部回显，最终检查长度及每一个字节。服务端不要求请求长度固定1025，也不把recv的一次返回当成消息边界；它持续回显字节流，直到输入EOF且输出排空。示例累计接受两条连接，两条都处理完后退出。

## 先理解每条连接的状态

```c
struct connection {
    unsigned char output[64];
    size_t length;
    size_t sent;
    int input_eof;
    size_t received;
    size_t queued;
    unsigned int full_count;
};
```

| 成员 | 含义 |
|---|---|
| output | 这条连接自己的待发送数据数组；不会与另一条连接共用可变内容 |
| length | 当前这一批已放入数组的有效数据长度，也是下一次追加接收的下标 |
| sent | 当前这一批已交给本机内核的前缀长度 |
| input_eof | 是否已经recv返回0，只记录输入结束，不表示连接马上关闭 |
| received | 这条连接累计recv字节数，不随缓冲复用归零 |
| queued | 这条连接累计被send接受的字节数，不保证对端应用已读取 |
| full_count | 尾部达到容量上限的次数，只用于观察 |

与上一节区别：上一节length是整份固定响应长度；本节length/sent属于当前可复用批次。每批发完，两者都归零；累计量由received/queued保留。

始终满足：

```text
0 <= sent <= length <= 64
[0, sent)        已交给内核的前缀
[sent, length)   待发送数据
[length, 64)     可以追加接收的尾部空间
```

recv从output+length开始，最多请求64-length字节；成功n后length+=n。send从output+sent开始，最多请求length-sent字节；成功n后sent+=n。EAGAIN不改变进度。

即使前缀已经提交，只要整批没发完，本例也不把前缀空间重新用于接收，不搬移未发数据、不实现环形队列。尾部满时先停止recv，等整批发送完成后令length=sent=0，再复用整个数组。这个选择降低状态管理复杂度，吞吐效率以后可以按项目需要改进。

## 事件由状态决定

每次poll之前，wanted_events查看当前连接状态：

```c
short events = 0;
if (!input_eof && length < 64)
    events |= POLLIN;
if (sent < length)
    events |= POLLOUT;
```

| 当前状态 | 关注事件 | 原因 |
|---|---|---|
| 未EOF，缓冲为空 | POLLIN | 等待输入，暂时没有内容可发送 |
| 未EOF，存在待发内容且尾部有空间 | POLLIN与POLLOUT | 可以继续追加，也可以继续发送 |
| 未EOF，尾部已满 | POLLOUT | 暂停接收，避免覆盖尚未发出的数据 |
| 已EOF，仍有待发内容 | POLLOUT | 不再接收，继续把输出发完 |
| 已EOF，输出为空 | 关闭当前连接 | 本次回显已经完成 |

这里POLLIN|POLLOUT是按位组合；POLLIN表示关注接收机会，POLLOUT表示关注提交输出的机会。events是应用根据状态提出的关注项，revents是poll本次返回的实际事件，不能混为一谈。

本节每个就绪连接每轮最多recv一次、send一次，然后处理其他连接。没有事件就等待poll，不在某个连接里等EOF或原地重试EAGAIN。监听fd继续用POLLIN接受连接，连接项则动态调整。

## 缓冲满时，为什么允许暂停读取

应用缓冲没有空位时，继续recv会覆盖数据或需要额外内存。本例撤销POLLIN关注，让暂时未取出的数据留在内核TCP接收缓冲。必要时TCP流量控制会让压力传回发送方。之后输出排空，应用重新关注POLLIN，继续接收，并不主动丢弃数据。

只有关注事件不会改变fd的阻塞属性。服务端监听和连接仍分别通过fcntl设置O_NONBLOCK，所以即使就绪状态变化，recv/send也以实际返回值为准。SO_SNDBUF与SO_REUSEADDR沿用06节，仅用于观察和便于重复实验。

## EOF与输出排空是两个条件

recv返回0后只设置input_eof=1。即使还有20字节未发送，这些数据仍然有效，需要保留POLLOUT继续发送。

正常关闭条件是：

```c
input_eof && sent == length
```

这表示输入已经结束，而且目前的输出已经全部交给内核。正常close后，TCP继续处理已提交的数据与FIN；客户端仍应读尽数据再观察到EOF。网络错误则报告并关闭当前连接，不保证错误发生后还能把剩余数据送达。

EOF后撤销POLLIN也有意义：EOF本身可以使接收持续表现为就绪，反复读取只会重复得到0，可能让事件循环空转。我们只需记录一次EOF，随后专注排空输出。

POLLHUP不代替recv的实际返回结果。代码在有空间时仍尝试接收剩余输入；有待发数据时也检查实际send结果。错误通常由recv/send返回值确认，POLLNVAL作为描述符管理错误报告并清理。

## 函数与阅读顺序

1. struct connection：先区分批次进度与连接累计值。
2. wanted_events：根据状态返回关注事件，是自定义函数；const指针表示它只查看状态。
3. handle_client：根据revents接收一次、发送一次；返回0保留，1表示正常完成，-1表示连接错误。这是自定义返回约定，不是recv/send的原始返回值。
4. main：每次poll前重算连接events；accept登记新连接；处理完成或失败后close、fd=-1、active减1。仍沿用accepted累计两条、active当前活动数量。
5. 客户端：构造小请求，send_all提交，shutdown(SHUT_WR)结束发送方向，read到EOF，逐字节核对。

客户端是固定1025字节的小请求工具，采用先发完再读。通用的大流量双向客户端也需要交替推进收发；服务端使用非阻塞不会自动改变客户端行为。本节教师的大流量验证采用独立推进上传和接收的方法。

## 亲手编译：WSL

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/07_buffered_echo
make -n
make
echo $?
file build/arm/buffered_server build/host/buffered_client
```

make应退出0。服务端为ARM32位静态程序，客户端为WSL x86-64程序。

## 创建板端目录，传输

COM12：

```sh
mkdir -p /root/linux-study/08_network/07_buffered_echo
```

若板子重启后丢失IP，在COM12恢复：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up
```

WSL本节目录：

```sh
scp -O build/arm/buffered_server root@192.168.10.2:/root/linux-study/08_network/07_buffered_echo/
```

## 主实验：两条连接独立回显与正常结束

COM12：

```sh
cd /root/linux-study/08_network/07_buffered_echo
chmod +x buffered_server
./buffered_server
```

看到8894监听后，WSL A：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/07_buffered_echo
./build/host/buffered_client 192.168.10.2 hold
```

A发送1025字节后，在shutdown之前等待回车，先不要回车。服务端会边接收边回显，A的内核能先接收这份小回显；主实验不要求这1025字节使TCP发送出现EAGAIN。重点是缓冲复用、动态关注与输入结束条件。

WSL B：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/07_buffered_echo
./build/host/buffered_client 192.168.10.2
echo $?
```

B应发送1025字节，执行SHUT_WR，再收到1025字节、echo matches=yes，退出0。COM12应显示B已关闭、active仍1。可以观察output buffer full日志以及input EOF; pending=...，如果pending大于0，随后应出现output drained after EOF再关闭；pending为0也合法，表示读到EOF之前输出已经提交完毕。

回到A按回车，A执行SHUT_WR并接收校验，最终received=1025、echo matches=yes。A结束后立即echo $?应0。COM12最终received_total=2050 queued_total=2050，结束后立即echo $?应0。

注意hold的位置：06节在接收固定下载前等待；本节在发送请求之后、SHUT_WR之前等待。不要仅凭同一个模式名推断程序停在相同阶段。

read/recv分段、full_count以及EOF时pending值由调度与网络决定，不要求所有日志顺序与教师完全一致。

## 可选：空请求

重新启动COM12服务端，在WSL运行一次empty，再运行一次默认客户端：

```sh
./build/host/buffered_client 192.168.10.2 empty
echo $?
./build/host/buffered_client 192.168.10.2
echo $?
```

空请求直接SHUT_WR，应收到0字节且matches=yes；第二条仍1025字节并正常回显。服务端两连接累计1025字节，三次退出状态均应0。

## 理解检查

某条连接已经input_eof=1，但length=50、sent=30，此时应该关闭，还是继续等待哪个事件？如果一次send成功提交剩余20字节，之后应该怎样处理？

参考：[poll事件数组与挂断](https://man7.org/linux/man-pages/man2/poll.2.html)、[recv返回值与EOF](https://man7.org/linux/man-pages/man2/recv.2.html)、[send短写与非阻塞](https://man7.org/linux/man-pages/man2/send.2.html)。
