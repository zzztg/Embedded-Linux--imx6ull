# 实验08-01：TCP回显，WSL客户端与开发板服务端

理解确认：学习者正确解释accept返回连接fd后，关闭监听fd不影响已有连接收发；已澄清监听对象与已连接socket对象不同。后续进入02_tcp_frames学习应用消息边界。

状态：2026-11-16已收到板端与WSL互通结果。板端listen_fd3、accept后connection_fd4，读取并回显16字节，观察对端发送方向结束后关闭并退出0；WSL客户端连接192.168.10.2:8888、发送接收各16字节、内容比较yes，退出0。未收到学习者编译日志，继续确认监听与连接fd的区别，不以运行成功代替理解。06-08 FIFO两轮传输已完成，学习者正确解释阻塞O_RDONLY在没有写端时停在open，而已连接后的read在全部写端关闭且缓冲读尽后返回EOF。

教师隔离验证：Makefile使用-Wall -Wextra -Werror成功构建ARM32位静态服务端及WSL x86-64客户端。另编译本机服务端，在WSL回环验证C客户端16字节回显并比较一致、512字节含NUL二进制数据跨多次read仍原样返回、空连接EOF、双方清理及退出0，并验证无效IPv4参数返回2。正式build目录留给学习者亲手构建；未把本机回环测试记作开发板网络验证。

## 两个运行位置和两种编译器

| 程序 | 运行位置 | 编译器 | 产物 |
|---|---|---|---|
| tcp_server.c | ARM开发板，COM12终端启动 | arm-linux-gnueabihf-gcc，静态链接 | build/arm/tcp_server |
| tcp_client.c | WSL的x86_64 Linux，本机启动 | gcc | build/host/tcp_client |

make同时构建两个程序，使用熟悉的src/include/build目录、模式规则和.d依赖。ARM_CC与HOST_CC是我们命名的Make变量，两条模式规则分别调用不同编译器，避免将同一份.o交给不同架构链接。host客户端留在WSL，不传到开发板。客户端终端本节是WSL，不是上一节的开发板SSH终端。

## IP、端口、TCP与socket

板子IP为192.168.10.2，端口选择8888。IP定位网络接口，端口定位本机TCP服务入口；本例与SSH的22端口不同。TCP连接是双向字节流，可在同一连接中发送和接收，保证正常交付的数据有序，但不提供应用消息边界；连接可能断开或报错，不能把可靠传输理解为不会失败。

socket是内核通信端点，socket(AF_INET,SOCK_STREAM,0)创建IPv4流式套接字，本例对应TCP。成功返回fd，失败-1并设置errno。它与文件/管道一样进入本进程fd表，之后可以read、发送和close，不是文件系统路径。

## 服务端四步

| 接口 | 作用 |
|---|---|
| socket | 创建通信端点 |
| bind | 绑定本地地址和端口 |
| listen | 进入监听状态，由内核接收连接请求 |
| accept | 取出一条可用连接，返回新的已连接fd |

accept(listen_fd,NULL,NULL)中NULL表示本节不取回对端地址。阻塞accept在没有待接受连接时等待。accept不创建应用工作线程或子进程；本例只有main。

listen_fd用于接受连接，connection_fd用于与某个客户端收发。accept不把原listen_fd改成连接fd，两者都需要在适当时关闭。listen(...,1)中的1涉及待接受连接队列，本程序只处理一个客户端是因为代码只调用一次accept，不是把1当作终生连接次数限制。

## 地址结构体和字节序

```c
struct sockaddr_in address = {0};
address.sin_family = AF_INET;
address.sin_port = htons(TCP_PORT);
address.sin_addr.s_addr = htonl(INADDR_ANY);
```

sockaddr_in是IPv4地址结构。sin_family选择地址族，sin_port存端口，sin_addr存IPv4地址；嵌套的s_addr是地址数值字段。htons把16位数从主机字节序转为网络字节序，htonl对应32位数。网络字节序为大端，避免不同CPU解释不同；不要靠把十进制端口直接赋进去碰运气。

INADDR_ANY表示监听本机所有IPv4接口，打印为0.0.0.0。客户端连接的是实际板子IP192.168.10.2，不能把0.0.0.0当作板子地址使用。

bind和connect使用通用const struct sockaddr *参数，所以把IPv4结构体地址转换成该指针类型，并传sizeof结构体；没有复制出另一个地址结构。

客户端inet_pton(AF_INET,argv[1],&server.sin_addr)把点分十进制IPv4文本转换为地址字段，其结果已按网络格式存放，无需再htonl一次。返回1成功、0文本无效、-1接口错误；它不负责DNS域名解析，本例参数必须是IPv4文本。

## 客户端建立连接

客户端socket后填服务器地址，再调用connect。客户端没有显式bind，内核自动选择适合路由的本地IP及临时端口。connect成功表示连接建立，不代表服务端应用已经完成某次业务处理；服务器内核可以先完成连接、排队，再由应用accept取走。

socket和accept成功返回非负fd；bind/listen/connect成功0，失败-1并设置errno，使用perror。inet_pton的返回规则单独检查。

## 数据和结束约定

客户端发送固定的16字节Hello from WSL!加换行，不发送C字符串末尾NUL。服务端每次read多少，就原样发回多少，不把buffer当作NUL结束字符串。读分几次、发送分几次都不要求固定。

include/net_common.h的send_all沿用之前write_all的累计偏移思路。send与write相似，但支持套接字选项；本例MSG_NOSIGNAL让对端断开导致的发送错误通过-1/errno处理，避免本次发送触发SIGPIPE结束程序。这是局部发送行为，不引入信号进阶模块。send成功可能只接受部分字节，因此循环；成功也不表示对端应用已处理，最终由客户端收到并比较回显验证本次数据。

客户端知道自己本次发送16字节，因此循环接收满16字节后比较，随后close。服务端读尽缓冲后read返回0，表示对端发送方向已结束，结束本次服务并关闭连接fd和监听fd。TCP是全双工，read0不应一概解释为对端整个程序退出；后续半关闭再展开。本例客户端确实关闭了连接。

客户端固定长度是当前测试约定，不是TCP自动划分消息。不能推广成任何服务都知道接下来该读多少，后续会专门讲消息边界。client的reply多预留一个零初始化字节用于printf，TCP不会自动添加NUL。memcmp按实际16字节比较内容。

## WSL：亲手编译两个程序

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/01_tcp_echo
cat src/tcp_server.c
cat src/tcp_client.c
cat include/net_common.h
make -n
make
echo $?
file build/arm/tcp_server build/host/tcp_client
```

预期退出0；server为ARM32位静态程序，client为WSL x86-64程序。编译和链接命令分别出现ARM交叉编译器和gcc，不省略客户端编译。

## 开发板COM12：创建目录并检查地址

```sh
mkdir -p /root/linux-study/08_network/01_tcp_echo
ifconfig eth0
```

如果重启后没有192.168.10.2，在COM12恢复之前的临时配置：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up
```

## WSL：传输服务端

```sh
scp -O build/arm/tcp_server root@192.168.10.2:/root/linux-study/08_network/01_tcp_echo/
```

客户端保留在WSL。

## 开发板COM12：先启动服务端

```sh
cd /root/linux-study/08_network/01_tcp_echo
chmod +x tcp_server
./tcp_server
```

看到server: listening on 0.0.0.0:8888后保留该终端，它在accept等待连接。

## WSL：启动本机客户端

确认提示符为dingf@localhost等WSL身份，而不是板端root@100ask。仍在本节源码目录：

```sh
./build/host/tcp_client 192.168.10.2
echo $?
```

预期：

```text
client: connecting to 192.168.10.2:8888; fd=实际fd
client: sent 16 bytes
client: received 16 bytes; echo follows:
Hello from WSL!
client: echo matches: yes
```

退出0。COM12中服务端应接受连接、累计回显16字节、观察peer send side ended、关闭两个fd并退出。它返回Shell后立即echo $?，应为0。一次可能输出：

```text
server: listening on 0.0.0.0:8888; listen_fd=3
server: accepted one connection; connection_fd=4
server: read 16 bytes; echoing
server: peer send side ended; echoed 16 bytes
server: sockets closed; exiting
```

read日志可能分多次，只要求总计16。本例只处理一次连接，想再运行客户端，先重新启动板端tcp_server。

## 常见结果怎样理解

accept持续等待且没启动客户端是预期行为；客户端不是FIFO的另一个open，而是主动connect到IP和端口。connect报Connection refused先检查服务端是否仍运行、地址和端口是否正确；连接一直没建立再检查IP和路由。bind报Address already in use先检查是否已有服务监听8888，不自动结束未知进程或修改网络设置。

## 完成标准

亲手构建两种架构产物，WSL客户端与板端服务端完成一次16字节回显，内容比较yes、双方退出0；能解释监听fd与连接fd的区别，以及本例为什么客户端接收满16字节后先close。

参考：[TCP语义与建立连接](https://man7.org/linux/man-pages/man7/tcp.7.html)、[socket](https://man7.org/linux/man-pages/man2/socket.2.html)、[accept](https://man7.org/linux/man-pages/man2/accept.2.html)。
