# 网络编程

从板端TCP回显服务与WSL客户端开始，复用现有交叉编译、以太网直连、文件描述符、读写循环和资源回收知识。

| 小节 | 内容 | 状态 |
|---|---|---|
| [01_tcp_echo](01_tcp_echo/README.md) | TCP建立连接、监听与连接fd、IPv4和端口、双端回显与关闭 | 板端与WSL完成16字节回显、比较yes，两端退出0；已正确解释关闭监听fd不影响已有连接 |
| [02_tcp_frames](02_tcp_frames/README.md) | 长度头、完整字段读取、同连接多消息及不完整EOF | 两条消息上板比较yes，内容13字节；已正确解释不完整EOF，并联系串口应用协议 |
| [03_half_close](03_half_close/README.md) | SHUT_WR、读到EOF后仍回复、单请求结束约定 | 默认及hold均已上板回显16字节，hold双方退出0；已正确解释EOF不代表对方进程退出 |
| [04_sequential_server](04_sequential_server/README.md) | 外层accept循环、单连接生命周期、顺序处理与连接排队 | 两轮上板均两连接/32字节，客户端均matches=yes；第一轮三次退出0；已正确解释当前连接read阻塞会挡住下一次accept；第二轮未贴中间等待状态及退出码 |
| [05_poll_server](05_poll_server/README.md) | 同时监视监听与两连接、每轮接收一次、MSG_DONTWAIT | 已上板：连接2先完成并关闭时连接1仍打开，最终回显32字节，三端退出0；已正确解释空闲等待poll、监听就绪后处理新连接，补充POLLIN与信号区别；发送仍阻塞，POLLOUT下一节补充 |
| [06_poll_send](06_poll_send/README.md) | O_NONBLOCK发送、短写与进度、EAGAIN后POLLOUT恢复 | 已上板：B先完成时A仍未关闭，各524288字节且内容yes，客户端均退出0；已正确推演短写后偏移55/剩余45及EAGAIN保留进度 |
| [07_buffered_echo](07_buffered_echo/README.md) | 有界输出缓冲、动态POLLIN/POLLOUT、输入EOF后排空 | 已上板：B EOF时pending1并排空后关闭、A pending0，各1025字节且yes，总2050，B退出0；已正确解释输入结束后先发完剩余20字节再关闭，补充POLLOUT |
| [08_udp_echo](08_udp_echo/README.md) | UDP消息边界、sendto/recvfrom寻址、空数据报与截断 | 已上板，5、8、0字节三条回显均校验通过，双方退出0。截断与超时已在本机验证，截断理解检查通过 |

后续逐步安排：连接生命周期、短读短写和应用消息边界、多个请求/客户端、UDP、poll在网络中的使用、断开和错误处理，再与线程队列及硬件应用结合。信号进阶和共享内存/消息队列不作为本阶段前置课程，按项目需要补充。
