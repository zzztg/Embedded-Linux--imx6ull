# 实验08-02：TCP长度头与完整消息接收

状态：已收到板端与WSL互通日志。服务端关闭监听fd后收两条完整消息，frames2/payload_bytes13；客户端两条matches=yes且退出0。服务端退出状态贴文拼接为0_tcp_frames，暂只记录完成日志；未收到编译日志。学习者已正确解释声明8字节仅收3就EOF属于不完整消息，并主动联系串口自定义应用协议，理解消息格式与传输接口的分层。

教师隔离验证：Makefile以-Wall -Wextra -Werror构建ARM32位静态服务端及WSL x86-64客户端成功；另编译本机-O2服务端验证两条C客户端消息及双方退出0。独立Python协议端按网络字节序一次发送合并的空消息、含NUL内容和64字节最大内容，回显一致、消息数3/内容字节67；验证消息边界正常EOF返回0、半头/无内容/半内容EOF及超长65字节均拒绝并退出1。已关闭监听fd的现有连接仍完成传输。本段为准备阶段验证记录，学习者板端结果见上方状态。

## 为什么需要应用协议

TCP保证字节顺序，不保留每次send的边界。发送HELLO与i.MX6ULL，接收时可能一次看到两者，也可能分成多次；单靠read返回次数、缓冲是否装满、暂时没有更多数据都无法确定一条消息结束。

上一节客户端事先知道回显总长度16，本节改为每条消息先告诉接收者内容长度。一条连接里处理两次请求/回复，不把关闭连接当作每条消息的分隔符。

## 本节协议

```text
[4字节长度，网络字节序][指定长度的内容]
```

长度只计算内容，不包括4字节长度头，也不包括用于C字符串显示的NUL。HELLO为5字节，i.MX6ULL为8字节，两条分别占9和12字节，内容合计13字节、应用协议字节合计21字节；这些数字不包括TCP/IP自身的头部。

长度5的头部在线路上为00 00 00 05，后跟48 45 4C 4C 4F。这个头是我们定义的应用数据，同样由TCP传输，与内核处理的TCP头部不同。

uint32_t来自stdint.h，表示32位无符号整数，本平台四字节。发送用htonl编码，接收用ntohl转回主机字节序。不能把ASCII字符5直接当四字节头，也不能把主机原生整数的内存排列直接约定为跨平台格式。

## 三个辅助函数，均是我们写的

| 函数 | 返回规则 | 作用 |
|---|---|---|
| read_exact | 1完整字段；0字段开始前EOF；-1接口错误或半字段EOF | 持续累计到指定字节数 |
| send_frame | 0成功；-1失败 | 编码长度头，再发送内容 |
| recv_frame | 1完整消息；0消息边界正常EOF；-1错误或不完整消息 | 完整读头、检查长度、完整读内容 |

send_all沿用上一节。send_frame/recv_frame不是新的系统调用；阅读重点在include/frame_io.h，socket/bind/listen/accept/connect与上一节相同。recv_frame返回1与pthread函数返回0成功的规则不同，按本函数约定判断。

## read_exact为什么需要累计

```c
read(fd, data + received, length - received);
```

读到n>0就received+=n，再从缓冲区后面继续填充；EINTR重试。一次read读少是允许行为，不是错误。本节人为将每次read请求限制为最多READ_STEP=3字节，让4字节头一定不能一口读完，稳定观察累计。通常看到3/4、4/4，但实际也可能读得更少。

这个限制是应用主动缩小读取量，不代表TCP按3字节分包，不是TCP固定行为，也不是人为要求每次发送3字节。真实TCP无论怎样划分可读片段，都应使用同样的累计思路。

## recv_frame的处理步骤

1. read_exact取得4字节长度头；如果一字节也没读就EOF，表示对端在消息边界结束。
2. ntohl解码长度，检查不超过MAX_PAYLOAD=64且缓冲能容纳内容与本地打印NUL。
3. read_exact取得指定内容。长度为0时无需调用底层read，直接得到一条合法空消息。
4. 在本地缓冲末尾补NUL供文本日志使用，将长度写入输出参数，返回1。

原始内容可以含NUL，协议仍按长度处理和回显；printf文本日志不是二进制内容的完整展示。客户端本次用文本方便观察，并按长度与memcmp比较。缓冲是MAX_PAYLOAD+1，预留的一个字节只在本地用于显示，不发到网络。

超长头在读内容前即拒绝，不把远端长度直接当数组容量使用。这里的上限是应用协议规则，不是TCP最大消息或最大传输单元。

## 三种EOF不同

- 下一条消息头开始前EOF：正常结束，recv_frame返回0。
- 头只收到部分就EOF：不完整消息，返回-1，EPROTO。
- 头完整，但内容没到齐就EOF：不完整消息，即使内容一字节未到也不能当正常结束。

长度0是合法空消息，recv_frame返回1且length0，与返回0表示没有下一条消息不同。本节正常演示只有两条非空消息，边界由教师验证覆盖。

EPROTO和EMSGSIZE是辅助函数检测到协议不完整或长度超限时主动设置的errno。底层read返回0本身可以是正常EOF，不能把协议错误误解成那次read一定返回了-1。

## 连接与收发顺序

客户端connect一次后，对每条消息执行“发完整消息→收完整回显→比较”，第二条仍复用原连接，最后close。服务端循环recv_frame/send_frame，直到对端在消息边界结束。不是每条消息重新connect，也不是每次收一条就关闭。

服务端accept后关闭listen_fd，用于复习上一节的结论：已有连接仍然能完成两次交互。关闭监听入口不会自动断开这条连接。本程序只接受一次连接，再测需要重启服务端。

本节端口为8889，和上一节8888区分，不自动停止其他程序。

## WSL：阅读并编译两种架构

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/02_tcp_frames
cat include/frame_io.h
cat src/frame_server.c
cat src/frame_client.c
make -n
make
echo $?
file build/arm/frame_server build/host/frame_client
```

预期退出0；服务端ARM32位静态，客户端WSL x86-64。

## COM12：创建目录

```sh
mkdir -p /root/linux-study/08_network/02_tcp_frames
```

## WSL：传输服务端

```sh
scp -O build/arm/frame_server root@192.168.10.2:/root/linux-study/08_network/02_tcp_frames/
```

## COM12：先运行服务端

```sh
cd /root/linux-study/08_network/02_tcp_frames
chmod +x frame_server
./frame_server
```

看到listening on 0.0.0.0:8889后保留终端等待。

## WSL：运行本机客户端

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/02_tcp_frames
./build/host/frame_client 192.168.10.2
echo $?
```

观察多条read_exact进度，重点业务输出：

```text
client: frame=1, length=5, matches=yes, text="HELLO"
client: frame=2, length=8, matches=yes, text="i.MX6ULL"
client: connection closed
```

客户端退出0。COM12中重点应为listener closed而连接仍存在、收到两条完整消息、clean EOF; frames=2, payload_bytes=13，再关闭连接。服务端返回Shell后立即echo $?，应0。不要把read_exact打印行数量当作消息数量。

## 后续

下一步继续在网络应用中学习连接生命周期、半关闭、错误处理和多个请求等；独立信号/共享内存/消息队列专项保持取消。

参考：[TCP字节流语义](https://man7.org/linux/man-pages/man7/tcp.7.html)、[字节序](https://man7.org/linux/man-pages/man3/byteorder.3.html)、[read返回值](https://man7.org/linux/man-pages/man2/read.2.html)。
