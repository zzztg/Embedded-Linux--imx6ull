# 实验08-08 UDP数据报与回显

2026-11-20主实验已上板。服务端在UDP 8895端口收到同一来源 `192.168.10.5:51574` 的5、8、0字节数据报，共回显3条、13字节载荷。客户端三条都校验通过，双方退出0。截断理解检查通过。使用6字节数组和 `MSG_TRUNC` 接收先后排队的8字节与5字节数据报，第一次写入6字节、返回8，第二次写入5字节、返回5。第一条未复制的2字节在第一次接收时丢弃。

隔离构建通过，产物为ARM32位静态服务端和WSL x86-64客户端。本机回环另验证两个源端口的分别回送、64字节二进制载荷、65字节超限丢弃和客户端等待超时。

| 维度 | TCP | 本节使用的UDP |
|---|---|---|
| 接收单位 | 有序字节流，由应用定义消息边界 | 一次成功接收取出一条数据报，保留消息边界 |
| 对端地址 | 与已建立的连接关联 | 每次发送给出目标，每次接收可以取得来源 |
| 交付 | 协议负责重传与有序交付，连接也可能失败 | 协议允许丢失、重复和乱序，需要应用按业务决定如何处理 |
| 返回0的含义 | 对非零长度的流式接收，表示接收方向EOF | 对本节的recvfrom，表示收到有效载荷为空的数据报 |

UDP服务端用 `socket(AF_INET, SOCK_DGRAM, 0)` 创建IPv4数据报套接字，再用 `bind` 绑定8895端口。同一个fd接收请求并发送回显。客户端也是UDP套接字，第一次发送时由内核自动分配本地端口。本例每条数据报发送时都显式提供目标地址。

客户端先调用三次发送，载荷依次是 `HELLO`、`i.MX6ULL` 和空载荷。服务端的64字节接收缓冲可以装下这13字节，但每次接收仍只取一条数据报，分别返回5、8、0。服务端成功回显三条之后主动关闭fd并退出。空数据报也计入三条。客户端给每条预期回复留3秒的poll等待时间，按来源、长度和字节内容匹配三条样本，允许三条以不同顺序回来，重复或意外回复作为实验失败报告。超时只表明等待期间未取得预期回复。

## 接收与回送

```c
struct sockaddr_in peer = {0};
socklen_t peer_length = sizeof(peer);
ssize_t n = recvfrom(fd, buffer, sizeof(buffer), MSG_TRUNC,
                     (struct sockaddr *)&peer, &peer_length);
```

前四项是套接字、缓冲区地址、容量和接收标志。后两项让内核填写来源地址，`peer` 保存来源IPv4地址与端口。`socklen_t` 是套接字地址长度的类型，调用前给 `peer_length` 填入结构体容量，返回后它保存实际地址长度。每次调用前都重新初始化。`struct sockaddr *` 的转换沿用bind中传入IPv4地址结构的做法。

接收返回 `-1` 时查看 `errno`。返回值大于等于0时表示取到一条数据报。Linux UDP上，本例传入 `MSG_TRUNC`，可在缓冲不足时取得原始数据报的长度。比如收到65字节，`n` 返回65，数组只存得下64字节，多余的1字节已被丢弃。先检查 `n` 是否超过容量，再拿它当缓冲区中的有效字节数。本例直接丢弃超限数据报，之后继续等待。

```c
sendto(fd, buffer, (size_t)n, 0,
       (const struct sockaddr *)&peer, sizeof(peer));
```

`sendto` 的前四项是套接字、载荷地址、长度和发送标志。后两项指定目标地址及其长度，服务端就把这条请求的来源作为回送目标。`send_datagram` 是本例把发送及错误判断封装起来的自定义函数。一次成功发送提交一条完整的数据报，空载荷时应返回0。使用TCP的 `send_all` 把一条载荷拆成多次发送，会变成多条UDP数据报。发送成功表示本机接受提交，实验由收到并核对回复来确认本次往返。

服务端通过 `inet_ntop` 把来源IPv4地址转换成点分十进制字符串，`INET_ADDRSTRLEN` 给出其文本缓冲所需容量。转换失败返回 `NULL`，并设置 `errno`。`ntohs` 把来源端口从网络字节序转为主机字节序后打印。

## 亲手编译与上板

WSL中编译，核对服务端为ARM静态程序，客户端为x86-64程序。

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/08_udp_echo
make -n
make
echo $?
file build/arm/udp_server build/host/udp_client
```

在COM12创建目录。

```sh
mkdir -p /root/linux-study/08_network/08_udp_echo
```

若板子重启后丢失直连IP，在COM12执行 `ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up` 恢复配置。回到WSL传输服务端。

```sh
scp -O build/arm/udp_server root@192.168.10.2:/root/linux-study/08_network/08_udp_echo/
```

在COM12启动服务端。

```sh
cd /root/linux-study/08_network/08_udp_echo
chmod +x udp_server
./udp_server
echo $?
```

看到 `UDP bound to 0.0.0.0:8895` 后，在WSL运行客户端。

```sh
./build/host/udp_client 192.168.10.2
echo $?
```

观察客户端收到三条 `matches=yes`，其中空报文显示 `length=0 (empty datagram)`。服务端应打印来源地址与端口，最终 `echoed_datagrams=3; payload_total=13`。双方预期退出0。服务端会等到成功回显三条，若本轮客户端报错或超时，可在COM12用Ctrl+C结束仍在等待的服务端，再从两端重跑。

思考题。假设第一条8字节数据报和第二条5字节数据报都已到达，接收数组只有6字节。第一次实际写入几字节？如果传入 `MSG_TRUNC` 会返回几？第二次接收从哪条数据报开始？

[Linux UDP](https://man7.org/linux/man-pages/man7/udp.7.html) · [recvfrom 与 MSG_TRUNC](https://man7.org/linux/man-pages/man2/recv.2.html) · [sendto](https://man7.org/linux/man-pages/man2/send.2.html)
