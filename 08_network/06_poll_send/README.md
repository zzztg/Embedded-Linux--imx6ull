# 实验08-06：非阻塞发送、保存进度与POLLOUT

状态：已收到板端及WSL结果。A使用hold暂不读取，服务端首次send请求524288字节、实际接受5792字节，随后EAGAIN并回poll；B首次接受46336字节，B先提交完524288字节并关闭时active仍1，A之后完成；EAGAIN计数分别176/4，服务端总提交1048576字节。两个客户端均收到524288字节且pattern matches=yes，均紧接退出0；服务端退出状态贴文为0poll_send|，记录程序结束日志与拼接的0，不单独要求重跑。未贴学习者编译日志。学习者已正确推演length100、sent40、send返回15后下一次偏移55、请求45字节，EAGAIN后保留进度；补充准确地址为response+55，sent保持55，返回poll等待可写机会。关键理解检查通过；已说明首次send resumed不代表整份响应完成。上一节已正确解释空闲等待poll与监听POLLIN。

教师验证：在.validation/project以-Wall -Wextra -Werror构建ARM32位静态服务端和WSL x86-64客户端，另编译-O2本机服务端。验证A hold且未读取时服务端出现EAGAIN，B仍完整下载512 KiB并退出0，随后释放A也完整接收且逐字节校验通过，服务端总计提交1048576字节并退出0；顺序两个正常客户端也通过；待发送期间连接复位后清理当前连接，下一客户端仍成功，服务端最终退出1报告既往错误；客户端拒绝提前EOF及长度正确但内容错误的响应。观察保存在.validation/backpressure_observation.txt。教师验证与上面的学习者板端记录分开，未代替学习者执行正式编译或板端命令。

## 本节先单独观察发送方向

上一节连接接收不会阻塞，但send_all仍可能在发送缓冲不足时等住整个服务端。本节不再发送请求做回显，改为固定数据下载：客户端连接后，服务端自动发送512 KiB，即524288字节的循环字母数据，然后关闭连接。客户端读到EOF后检查长度和每一个字节。固定长度约定来自共用头文件，内容为ABCDEFGHIJKLMNOPQRSTUVWXYZ反复排列，末尾按总长度截断，不包含额外字符串结束符。

为了看清发送，服务端无需读取任何请求，连接项只关注POLLOUT；监听项仍关注POLLIN。累计接受两个客户端，每条连接发送独立的完整响应；两条都处理完后退出。这是一节有明确结束条件的发送实验，不是完整的双向回显框架。

主实验：A连接后暂不调用read，服务端发给A时出现EAGAIN；保持A不读，B正常接收并完成下载；再按回车让A开始读，验证A收到完整且不重复的数据。

## 背压如何传回来

```text
A应用暂不read
    → A内核接收缓冲逐渐占满
    → TCP流量控制限制继续接收
    → 服务端内核发送缓冲不能继续腾出空间
    → 非阻塞send返回-1，errno为EAGAIN/EWOULDBLOCK
```

A应用不read不等于A内核立即停止接收；起初仍可能接收并确认一部分数据。因此服务器不会从第一个字节就必然EAGAIN，具体进度和次数随系统和网络变化。

## 发送缓冲与发送进度

发送来源是服务端的static char response[RESPONSE_BYTES]，启动时生成一次，传输期间保持不变，生命周期覆盖全部连接。

每个客户端有自己的struct send_state：

| 成员 | 含义 |
|---|---|
| length | 这条响应需要提交的总字节数，本例固定524288 |
| sent | send返回正数后累计已被本机内核接受的字节数 |
| stalls | 返回EAGAIN/EWOULDBLOCK的次数，仅用于观察 |
| resume_reported | 是否已经打印过首次恢复发送日志，避免刷屏 |

核心状态只有length和sent，满足0<=sent<=length；剩余量为length-sent，下一次从response+sent开始发送。日志与统计成员不控制发送内容。

例如length=100、sent=40，则[0,40)已提交，[40,100)待提交。下一次send请求60字节，若实际返回15，则sent更新为55，下次请求45字节。若返回EAGAIN，sent仍为40，保留全部待发送内容，返回事件循环等待机会。

这里的sent不是文件偏移，不使用lseek；也不证明对端应用已经读取或处理了数据。send成功表示内核接受对应字节。最终接收长度与逐字节校验由客户端完成。

本例两个客户端发送相同内容，因此共享一份传输期间不变的response，各自保存独立sent。如果以后每个响应内容不同，需要各连接独立保存相应输出内容。尚未提交完的内容不能被覆盖，也不能使用已经结束生命周期的局部缓冲。

## 非阻塞与POLLOUT分别负责什么

服务端复用fcntl的F_GETFL/F_SETFL，把监听fd和每个接受到的连接fd都设为O_NONBLOCK。连接fd的send因此不会等待缓冲区腾出空间，通常会短写或返回EAGAIN。MSG_NOSIGNAL继续用于避免SIGPIPE直接终止进程，不负责设置非阻塞。

POLLOUT表示可以尝试写入；它不保证这一次请求的所有剩余字节都能一次发送。即使poll报告POLLOUT，实际send返回值仍需检查。

```c
ssize_t n = send(fd, response + state->sent,
                 state->length - state->sent, MSG_NOSIGNAL);
```

- n>0：只把n加到sent，然后判断是否还有剩余。
- n==-1且EAGAIN/EWOULDBLOCK：暂时写不进去，sent不变，保留连接，回到poll。
- EINTR：本例也返回事件循环，后续继续尝试。
- 其他错误：关闭当前连接，继续照看另一条；最终退出状态1报告曾发生错误。
- 本例请求长度始终大于0，send返回0视为无法推进的异常并结束该连接，不当作接收EOF。

不要在EAGAIN后原地while无限重试，那会变成忙等，阻碍其他连接。try_send最多调用send四次就让出本轮机会，即使一直成功也会返回主循环，再处理其他就绪项。

## 只在有待发送数据时关注POLLOUT

连接登记时已有一份响应，因此events=POLLOUT。只要sent<length就保留监视；全部提交后，本例直接close连接，并把对应poll数组项fd设为-1，不再监视它。

如果以后要保持连接等待下一次业务，队列为空时应撤销POLLOUT，只在产生新待发送数据时再关注它。多数空闲TCP连接经常可写，无数据却一直关注POLLOUT会让poll频繁立即返回，造成空转。

本例不需要等对方EOF才关闭发送完成的连接。正常close后，已交给内核的数据及后续FIN由TCP处理；客户端仍应先收到数据，再观察EOF。服务端提交完成不代表客户端进程此时已经返回。

## 新接口setsockopt，逐项说明

sys/socket.h声明：

```c
int setsockopt(int fd, int level, int option_name,
               const void *option_value, socklen_t option_length);
```

fd表示配置哪个socket；SOL_SOCKET表示socket层配置；option_name指定选项；option_value传入选项值的地址；option_length传入这个值的字节数。成功返回0，失败-1并设置errno。

本节服务端：

```c
int send_buffer_bytes = 4096;
setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
           &send_buffer_bytes, sizeof(send_buffer_bytes));
```

SO_SNDBUF申请较小的发送缓冲，方便观察背压。hold客户端在connect前用SO_RCVBUF申请较小的接收缓冲；正常客户端不缩小接收缓冲。4096是请求设置值，不是保证恰好发送4096字节才EAGAIN；Linux会对缓冲设置作内部调整和记账，TCP也有自己的缓冲规则。

监听socket还在bind前设置SO_REUSEADDR，值为int 1。因为本例由服务端主动结束数据传输，这个选项便于在常见TIME_WAIT场景重新绑定、重复实验；旧服务端仍在同一地址端口监听时，仍需先结束旧实例。

## 自定义函数与主循环

- set_nonblocking(fd)：封装已学fcntl设置；0成功、-1错误，不是新的系统API。
- try_send(fd,number,&states[i])：读取并更新该连接发送状态。返回1表示全部提交、0表示还有待发送数据、-1表示连接错误。这是我们自己定义的返回约定，不是send原始返回值。
- 主循环保留上一节的accepted/active及三个poll数组项。监听项POLLIN时接入，连接项POLLOUT时尝试发送，完成或失败才关闭并active减1。
- 每个新接受的连接都单独设置O_NONBLOCK；Linux的accept不会自动继承监听fd的这个属性。

先读struct send_state和try_send，再读main中事件登记与处理；最后看客户端hold和接收校验。客户端是单连接观察工具，可以阻塞等待read；本节要求避免互相阻碍的是服务端事件循环。

## 亲手编译：WSL

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/06_poll_send
make -n
make
echo $?
file build/arm/send_server build/host/send_client
```

应编译退出0。send_server是ARM32位静态程序；send_client是WSL x86-64程序。

## 创建目录与传输

COM12：

```sh
mkdir -p /root/linux-study/08_network/06_poll_send
```

若板子重启后eth0无IPv4地址，在COM12恢复：

```sh
ifconfig eth0 192.168.10.2 netmask 255.255.255.0 up
```

WSL本节目录：

```sh
scp -O build/arm/send_server root@192.168.10.2:/root/linux-study/08_network/06_poll_send/
```

## 主实验：A停止读取，B仍能完整下载

需要COM12和两个WSL本机终端A/B；不要把A/B命令放到板端SSH中。

COM12：

```sh
cd /root/linux-study/08_network/06_poll_send
chmod +x send_server
./send_server
```

看到端口8893监听后，WSL A：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/06_poll_send
./build/host/send_client 192.168.10.2 hold
```

A显示not reading yet; press ENTER to start receiving后先不要回车。本节hold发生在接收数据之前，与上一节收到回显后才hold不同。

观察COM12，等待出现client 1 send would block; progress=.../524288; back to poll。保留这个中间状态，进度数值不要求固定。之后仍不释放A，在WSL B运行：

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/08_network/06_poll_send
./build/host/send_client 192.168.10.2
echo $?
```

B应显示EOF、received=524288、pattern matches=yes并退出0。COM12应显示client 2提交524288/524288并关闭、active=1；此时A仍处于回车提示。

回到A按回车，开始接收。A最终也应received=524288、pattern matches=yes，返回Shell后立即echo $?，应0。COM12最终queued_total=1048576 bytes，退出后立即echo $?，应0。

first send的实际接受量、EAGAIN_count以及首次恢复日志的具体时序由系统决定，不要求照抄验证数字。A应用尚未read时内核也可能先腾出一些空间，因此send resumed after EAGAIN的首次日志不一定恰好出现在按回车之后；核心是A未读取期间无法提交完全部响应，B却能完成，A恢复后内容完整。

## 理解检查

length=100、sent=40时，一次send返回15，下一次send的起始地址和长度分别是什么？若下一次返回EAGAIN，这两个进度变量应该怎样变化？

参考：[send与非阻塞错误](https://man7.org/linux/man-pages/man2/send.2.html)、[poll与POLLOUT](https://man7.org/linux/man-pages/man2/poll.2.html)、[socket缓冲和选项](https://man7.org/linux/man-pages/man7/socket.7.html)。
