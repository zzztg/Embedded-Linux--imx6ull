# 实验08-04：服务端连续接受连接

状态：已收到两轮板端与WSL结果。第一轮同一服务端连续接受两条连接，各回显16字节，总32字节；listen_fd保持3，两次connection_fd均4，服务端与两次客户端均紧接退出0。第二轮A使用hold并收到回显后等回车，A/B最终均matches=yes并关闭，服务端处理两条连接共32字节后退出；尚未贴第二轮各自退出状态。最终日志未保留A释放前B的中间状态，不能据此单独确认B在A保持连接时已connect/send但尚未收到回显。学习者已正确解释当前场景第一条连接未EOF，服务端阻塞read，尚不能执行下一次accept；补充该阻塞还要求暂时无数据可读，未EOF本身不代表read必然阻塞。未贴学习者编译日志。上一节已正确说明读到EOF不能据此判断对端进程退出。

教师隔离验证：在.validation/project中以-Wall -Wextra -Werror构建并确认ARM32位静态服务端、WSL x86-64客户端；另以本机-O2服务端完成四组回环验证：顺序两客户端共32字节；第一客户端hold时第二客户端已连接/发送但没有回显，释放后两者成功；空连接后仍服务下一条；第一条连接复位后继续服务下一条并最终退出1报告先前错误。未操作板子，正式build目录留给学习者构建。

## 目标与范围

同一个服务端进程连续接受两条连接，每条连接结束后继续接受下一条。为方便正常退出和检查状态，示例明确限制接受两条连接；这不是TCP的限制，也不是listen的backlog限制。没有创建线程或子进程，所以一次只处理一条连接。

复用01_tcp_echo的流式回显：收到数据就原样发回，客户端知道本次发送16字节，因此累计收满16字节后比较，再close。服务端持续read到EOF才结束这一条连接；这里不使用03_half_close那种“收到EOF才开始回复”的约定，也没有新增消息格式。TCP仍是字节流，read不保证一次读满16字节。

## 先看两层循环

```c
/* 省略错误处理的结构示意，完整代码见src/loop_server.c。 */
listen_fd = socket(...);
bind(listen_fd, ...);
listen(listen_fd, 4);

for (int client_number = 1; client_number <= 2; ++client_number) {
    connection_fd = accept(listen_fd, NULL, NULL);
    serve_connection(connection_fd, client_number, &echoed);
    close(connection_fd);
}
close(listen_fd);
```

外层循环管理连接：accept获得一条连接，调用自定义函数处理，关闭该连接，再accept下一条。serve_connection内部的循环管理同一条连接上的数据：read、send_all回显、再read，直到EOF或错误才返回。它不是系统函数；本节只是把原有读写循环移进一个函数，便于看清两层职责。

没有客户端等待时，阻塞在accept。已经接入一个客户端但它暂时不发数据也不结束发送时，阻塞在该connection_fd的read；发送很多数据而对端接收不及时，阻塞send也可能拖住服务端。本节用第二种情况进行观察。

正常EOF结束当前连接处理，不自动终止服务端。连接读写出错时示例也会关闭当前连接，再进入下一轮，并最终以1报告曾出现错误；accept本身发生非EINTR错误则结束示例。生产服务还需要更完整的错误分类、超时、退出等管理，后续按使用场景补充。

## 三个容易混淆的量

| 代码中的量 | 含义 |
|---|---|
| CLIENT_LIMIT=2 | 示例外层循环接受两条连接后退出 |
| listen(listen_fd,4)中的4 | 请求的待accept连接队列上限，受系统限制；不是累计最多服务4人，也不创建4个工作线程 |
| connection_fd | 当前连接的描述符，close后这个整数可以被下一次accept重新使用 |

两次accept都打印connection_fd=4是允许的。描述符是进程表中的一个编号，释放后可以复用；并不是两个客户端共用同一条连接。日志中的client_number是我们自己的计数，帮助区分本次第一条、第二条连接。

## 已建立连接，与应用接收处理的区别

内核维护监听套接字的待accept队列。服务端程序忙于第一条连接时，第二个客户端的TCP连接仍可能建立成功，数据也可以先到内核接收缓冲区；应用程序尚未调用第二次accept，就还没有读出它的数据、执行回显。

因此connect成功不证明服务端程序已经处理请求，send成功也不证明对方应用已读到数据。这一节会观察“第二客户端已connected、已sent，但仍没有echo”。条件是队列有空间、网络正常；不能推广成无论多少客户端connect都必然立即成功。

## 文件与接口

- src/loop_server.c：主函数保留listen_fd，外层for接受两条连接；serve_connection负责一条连接的读写，echoed指针返回这条连接累计回显字节数。
- src/loop_client.c：沿用发送16字节、累计接收、memcmp比较；hold模式在收到完整回显后，用已经学过的getchar等待回车再close。
- include/net_common.h：沿用send_all处理短写/EINTR，MSG_NOSIGNAL避免断开连接时由SIGPIPE直接终止进程；端口改为8891。
- Makefile：ARM静态服务端build/arm/loop_server，WSL本机客户端build/host/loop_client。

注意hold的位置：上一节是在shutdown之前等回车；本节是在完整回显之后、close之前等回车。客户端这时未结束发送方向，服务端已回显但还在read等待后续数据或EOF。

## 亲手编译：WSL

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/04_sequential_server
make -n
make
echo $?
file build/arm/loop_server build/host/loop_client
```

make后立即检查退出0。file应分别显示ARM 32位静态可执行文件、x86-64本机可执行文件；两者运行位置不同。

## 准备板端目录：COM12

```sh
mkdir -p /root/linux-study/08_network/04_sequential_server
```

若板子重启后eth0无IPv4地址，在COM12恢复已有直连配置：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up
```

## 传输：WSL

```sh
scp -O build/arm/loop_server root@192.168.10.2:/root/linux-study/08_network/04_sequential_server/
```

只传ARM服务端，客户端在WSL本机执行。

## 第一轮：同一个服务端，顺序运行两次客户端

COM12：

```sh
cd /root/linux-study/08_network/04_sequential_server
chmod +x loop_server
./loop_server
```

看到waiting for client 1/2后，在WSL当前实验目录执行第一条客户端：

```sh
./build/host/loop_client 192.168.10.2
echo $?
```

应回显16字节、matches=yes、退出0。观察COM12：第一条连接EOF、finished，然后waiting for client 2/2，服务端仍在运行，没有返回Shell。

同一WSL终端再次执行相同两条命令。第二个客户端完成后，COM12显示handled 2 connections、echoed_total=32并返回Shell，紧接执行echo $?，应0。两次accept的connection_fd可能相同；这叫编号复用。

## 第二轮：第一条连接不结束，第二条连接会怎样

准备COM12和两个WSL终端A/B，A/B都进入本节目录。WSL终端不是SSH登录板子的终端。

1. COM12再次运行./loop_server，等待client 1。
2. WSL A执行下面命令，收到matches=yes和press ENTER提示后，暂时不要回车。

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/04_sequential_server
./build/host/loop_client 192.168.10.2 hold
```

3. WSL B执行：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/04_sequential_server
./build/host/loop_client 192.168.10.2
```

此时A已有回显但仍保持连接；B通常能打印connected和sent 16 bytes，但尚无received/matches。COM12只accepted client 1，回显过16字节，还没有第一条EOF，也没有accepted client 2。这里不需要靠sleep制造先后关系，A的回车就是可控开关。

4. 回到WSL A按一次回车，A关闭连接；紧接echo $?，应0。
5. COM12读到第一条EOF后进入第二次accept，处理B的数据；WSL B现在收到16字节、matches=yes并结束，紧接echo $?，应0。
6. COM12完成两条连接后返回Shell，紧接echo $?，应0。

若启动时报bind失败且Address already in use，先检查是否旧服务端仍在运行，不要重复启动多个实例。实验若提前中断，先在对应终端结束残留实例，再重新跑完整两条连接。

## 理解检查

第一条连接只发了1字节，服务端已经回显，但客户端不再发送、也不close或SHUT_WR。此时第二条连接即使已在内核排队，当前程序会执行第二次accept吗？请用内外两层循环说明原因。

参考：[accept取出待处理连接并返回新的fd](https://man7.org/linux/man-pages/man2/accept.2.html)、[listen与待accept队列](https://man7.org/linux/man-pages/man2/listen.2.html)。
