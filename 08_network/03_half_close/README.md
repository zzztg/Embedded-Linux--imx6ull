# 实验08-03：TCP半关闭与EOF结束约定

状态：已收到板端与WSL两轮结果。默认模式客户端SHUT_WR后仍读到16字节和EOF、比较yes、退出0；服务端先读16再EOF、在EOF后回复并关闭，退出码贴文拼接0_half_close，暂只记录完成日志。hold模式出现回车提示，回车后执行半关闭并完成相同流程，双方退出0。第二轮贴文中的客户端/服务端标题互换，以实际half_server和half_client命令及提示符确定角色。未收到编译日志；学习者已正确指出EOF不能直接推断对方进程已退出，并补讲发送方向结束且缓冲数据读尽的含义。08-02已正确理解不完整消息及串口应用协议联系。

教师隔离验证：Makefile以-Wall -Wextra -Werror构建ARM32位静态服务端及WSL x86-64客户端成功，另编译本机-O2服务端进行回环验证。默认模式SHUT_WR后客户端仍收到16字节响应及EOF、比较yes，双方退出0；hold模式先观察服务端收到16但没有EOF/响应，再输入回车，验证半关闭触发EOF后回复。另验证空请求、64字节含NUL的二进制请求，以及65字节超限拒绝。教师验证与上面的学习者板端记录分开。

## 本节只新增shutdown

TCP有两个独立的数据方向。同一个已连接fd既能发，也能收。shutdown(fd,SHUT_WR)关闭本端发送方向，不移除fd，也不关闭接收方向；之后仍可read对端响应，最后再close释放描述符。

shutdown成功0，失败-1并设置errno，用perror。SHUT_RD关闭接收方向，SHUT_WR关闭发送方向，SHUT_RDWR关闭两个方向；本节仅使用SHUT_WR。shutdown不是再建立连接，也不是暂停后以后重新打开发送方向；同一连接不能靠它恢复发送。

本端发送方向结束后，对端在读尽此前的数据后read返回0。不是shutdown返回就保证对方立即看到EOF或已经处理完请求。对端read0也不意味着它不能继续发送，本节会实际在读0后再发送响应。

## 新的应用结束约定

上一节用长度头支持同一连接中的多条消息。本节为观察半关闭，使用另一种约定：一次连接仅一个请求，客户端发送方向EOF表示请求结束；服务器收完整个请求后才把全部内容原样回复，然后关闭连接，客户端读到响应方向EOF。

不使用长度头，服务端事先不知道请求恰好16字节。应用最多接收64字节，防止缓冲越界；上限不是报文边界。EOF就是本次请求的结束约定，因此与长度头协议中的“声明8却只收到3”的不完整判定不同；正确性由双方选择的协议决定。

## 默认模式顺序

```text
客户端：send全部请求
客户端：shutdown(SHUT_WR)，不再发送但保留读取
服务端：读完请求后read0，判定请求结束
服务端：在读0之后发送完整响应，再close
客户端：持续读取响应直到read0，比较内容，再close
```

本次数据Hello from WSL!加换行，共16字节。客户端知道预期内容只是为了验证，接收函数按EOF结束，不像08-01按已知16字节结束读取。

如果客户端发完直接close，该fd已失效，不能继续用它接收响应；不要用它代替SHUT_WR。如果省略shutdown却直接等待响应，客户端等服务端回复，服务端等客户端结束发送，双方可能一直等待。此错误只分析，不要求修改制造卡住。

## hold模式观察时机

客户端发送16字节后先等待一次回车，不立即半关闭；此时客户端停在getchar，服务端已读到数据但还会继续read等EOF，不会发送回复。

等COM12看到server的total=16后，在WSL客户端按回车。客户端执行SHUT_WR，服务端读到EOF后才打印sending ... AFTER EOF并发送响应，双方完成退出。hold不是死锁，用户输入可以让它继续推进，不依赖sleep猜时机。

## 辅助函数与缓冲

send_all沿用前节循环处理部分发送、EINTR和MSG_NOSIGNAL。receive_until_eof是我们写的普通函数：用32字节临时数组读到的数据，经容量检查后用memcpy复制到累计缓冲区；读0时返回0表示成功完成并通过length写出总长度，接口错误或超过容量返回-1。

memcpy复制指定字节，不依赖NUL。所有请求字节仍按二进制长度回显。客户端多预留1字节零初始化用于显示；协议传输不包含自动补的NUL。即使累计已经64字节，还会尝试读取临时chunk来确认EOF或检测超限，不能用read(fd,...,0)的返回0冒充真实EOF。

端口8890，仍一个连接一个服务进程，accept后关闭监听fd。每轮需要重新启动服务端，两个程序和编译器分别为ARM板端half_server与WSL x86-64的half_client。

## WSL：阅读与亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/03_half_close
cat include/stream_io.h
cat src/half_server.c
cat src/half_client.c
make -n
make
echo $?
file build/arm/half_server build/host/half_client
```

## COM12：创建目录

```sh
mkdir -p /root/linux-study/08_network/03_half_close
```

## WSL：传输服务端

```sh
scp -O build/arm/half_server root@192.168.10.2:/root/linux-study/08_network/03_half_close/
```

## 第一轮，COM12启动服务端

```sh
cd /root/linux-study/08_network/03_half_close
chmod +x half_server
./half_server
```

看到listening on 0.0.0.0:8890后，在WSL当前实验目录：

```sh
./build/host/half_client 192.168.10.2
echo $?
```

默认自动运行，客户端应看到write side shut down但随后仍收到16字节、EOF和matches=yes，退出0。COM12应先read16，再EOF16，然后sending 16-byte reply AFTER EOF、关闭连接。服务端返回Shell后紧接echo $?，应0。

## 第二轮，hold观察

COM12重新执行./half_server，看到监听后，WSL运行：

```sh
./build/host/half_client 192.168.10.2 hold
```

此时WSL出现press ENTER提示，先不要回车。观察COM12：它可以读到16字节，但尚无EOF和发送回复日志。再在WSL按一次回车，双方应完成相同的半关闭和回复流程。各自返回Shell后立即echo $?，应均0。

## 完成标准

能够说明关闭发送方向为何仍可接收；解释服务端读0为何仍能发送；从hold观察出“数据已到”与“请求已结束”的不同；明确EOF作为本次单请求结束约定，与上一节长度头的适用方式不同。

参考：[shutdown](https://man7.org/linux/man-pages/man2/shutdown.2.html)、[接收与EOF](https://man7.org/linux/man-pages/man2/recv.2.html)、[TCP全双工字节流](https://man7.org/linux/man-pages/man7/tcp.7.html)。
