# 实验06-08：FIFO命名管道

理解确认：学习者正确说明在本例阻塞O_RDONLY方式下，节点即使存在，新reader仍需在open等待writer；已澄清只需至少一读一写连接，而不是所有可能的参与进程都打开。FIFO基础收尾后进入08_network/01_tcp_echo。

状态：两轮板端实验已有输出，学习者分别按读端先启动、写端先启动报告。创建节点退出0、类型p、权限rw-------；两轮writer均发送20字节、退出0，reader均收到Hello through FIFO!与EOF/20字节，第二轮reader紧接退出0，第一轮reader未单独贴退出状态；第二轮结束后FIFO节点仍存在。未收到学习者编译日志，继续确认节点存在不等于对端存在、阻塞open与read的区别。本节补在已有06_ipc中；独立信号与IPC进阶模块已取消，其余机制按实际项目需要再学。

教师隔离验证：ARM版本通过-Wall -Wextra -Werror静态交叉编译，file确认为ARM32位静态程序；WSL本机-O2版本分别验证读端先启动和写端先启动时等待对端、20字节传输、关闭后的EOF与双方退出0；同一FIFO节点复用且不被重建覆盖；普通文件在传输前被拒绝，内容保持原样。正式build目录留给学习者构建，尚未记录学习者板端结果。

## 本节目标

理解命名管道的路径入口，让两个分别启动的独立进程通过同一个FIFO通信；观察阻塞open、数据传输、写端关闭后的EOF，以及通信结束后路径仍存在。把这些机制与已学匿名管道对应起来，不重做整套poll/信号进阶。

FIFO这里是文件系统中的“命名管道”，与上一章应用自己维护的环形队列不是同一种对象。它有路径，但数据由内核传递，不作为普通文件内容存入磁盘。只要权限允许，没有父子关系的进程也能通过同一路径打开通信。

## 接口先解释

```c
mkfifo(path, 0600);
```

创建FIFO节点。0600是八进制权限，允许所有者读写，最终还受umask影响；本实验两个终端都用板端root。创建节点不等于已经打开读端或写端，mkfifo不返回数据传输用的fd。成功0、失败-1并设置errno；路径已存在时报EEXIST，本例直接报错，不覆盖或自动删除原路径。

```c
open(path, O_RDONLY);  /* 读端 */
open(path, O_WRONLY);  /* 写端 */
```

本例采用阻塞打开：单独启动reader时可能停在open，直到writer打开；先启动writer则相反。不要把open阶段的等待误认成已经进入read。open日志成功后才进入读写循环。本例不使用O_RDWR同时占住两端，以免掩盖连接和EOF条件。

之后read/write/close、EINTR重试与部分写入处理沿用已有文件I/O知识。FIFO仍是字节流，不保留每次write的消息边界。写端全部关闭且内核数据读完后，read返回0；有写端但暂时没数据时，阻塞read等待。读端全部关闭时继续write可能触发已学SIGPIPE，属于现有基础知识，本节正常实验让reader一直读到EOF。

程序多做一个类型检查：fstat(fd,&info)读取已打开对象的元数据，struct stat的st_mode含类型和权限，S_ISFIFO(info.st_mode)检查它是不是FIFO。检查发生在写任何数据之前，不会把普通文件误当作FIFO覆盖。fstat失败-1、成功0；S_ISFIFO是判断宏。完整源码在src/fifo_demo.c。

## 程序三个入口

- create路径：调用mkfifo创建节点。
- read路径：打开读端，数据写到stdout，诊断和EOF统计写到stderr。
- write路径：打开写端，发送20字节Hello through FIFO!加换行，然后关闭写端。

命令分别为./fifo_demo create ./study.fifo、./fifo_demo read ./study.fifo、./fifo_demo write ./study.fifo。两个传输进程独立启动，不依赖fork继承fd；程序不会自动删除FIFO路径。

## WSL：亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/06_ipc/08_fifo
cat src/fifo_demo.c
make -n
make
echo $?
file build/fifo_demo
```

预期编译退出0，ARM32位静态程序。

## 开发板COM12：创建目录

```sh
mkdir -p /root/linux-study/06_ipc/08_fifo
```

## WSL：传输

```sh
scp -O build/fifo_demo root@192.168.10.2:/root/linux-study/06_ipc/08_fifo/
```

FIFO节点在开发板的Linux文件系统中创建，不在Windows源码目录/mnt/c中创建。

## 开发板终端A：COM12，创建并先启动读端

```sh
cd /root/linux-study/06_ipc/08_fifo
chmod +x fifo_demo
./fifo_demo create ./study.fifo
echo $?
ls -l study.fifo
./fifo_demo read ./study.fifo
```

ls类型首字符应为p。reader首先打印opening FIFO，然后等待，此时保留该终端，不再向它输入新命令。

如果节点已创建，不重复create，先用ls -l确认p类型，再运行read/write。EEXIST不表示要删掉任意同名文件。

## 开发板终端B：另一个SSH终端，启动写端

终端B也必须连接到同一块开发板192.168.10.2，使用root；不是在WSL本机执行板端路径。可在MobaXterm打开该SSH会话，或在另一个WSL终端先执行ssh root@192.168.10.2，再输入：

```sh
cd /root/linux-study/06_ipc/08_fifo
./fifo_demo write ./study.fifo
echo $?
```

writer应打印open succeeded和sent 20 bytes。终端A随后应看到Hello through FIFO!及reader: EOF; received 20 bytes。A返回Shell后立即执行echo $?，两个进程均应退出0。诊断行具体交错不作要求。

## 第二轮：先启动写端，观察节点复用

节点保留，不再次create。先在终端B运行write，它会在open等待；再在终端A运行read，双方继续传输并退出0。这说明无论谁先启动都能等待另一端，节点存在本身不代表对端进程存在。

通信结束后ls -l仍能看到study.fifo，它不会作为普通文件保留刚才的数据。若再次运行reader，没有新writer仍会在open等待，不会读出上一次的消息。

本节结束后可以保留节点；若要清理，只在确认ls -l首字符为p且两端程序均已结束后执行rm study.fifo。这里的删除移除路径，与close关闭进程持有的fd是两件事。

## 完成标准

能够解释FIFO与匿名管道的共同点和路径差别；完成读端先启动、写端先启动两轮传输，各20字节且观察EOF；理解节点生命周期与进程连接生命周期不同。教师验证不替代学习者板端实验。

完成本节后进入网络编程，不增加共享内存、消息队列或独立信号专项作为前置任务。

参考：[FIFO语义](https://man7.org/linux/man-pages/man7/fifo.7.html)、[mkfifo](https://man7.org/linux/man-pages/man3/mkfifo.3.html)、[管道读写与EOF](https://man7.org/linux/man-pages/man7/pipe.7.html)。
