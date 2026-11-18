# 实验 06-05：poll 同时捕获 stdout 与 stderr

状态：学习者已上板运行。父 PID 4360、子 PID 4361；stdout 按 17+22=39 字节接收，stderr 按 17+28=45 字节接收；各次 poll 返回 1；stderr 先 EOF、active 从 2 变为 1，随后 stdout EOF、active 变为 0，再回收子退出 7，父退出 0。所贴 stdout/stderr 文件均含本轮子 PID 4361，wc 分别 39、45，合计 84。注意运行命令贴文末尾显示 captured_s，而后续读取 captured_stderr.txt，需核对是粘贴截断还是实际文件名不同，避免读到旧文件。继续多路处理与公平性理解检查；尚未收到本节 WSL 编译输出，不据此标记编译检查。上一节已正确解释 POLLIN 与 POLLHUP 同时出现时必须先读完剩余数据，不能直接关闭读端。

教师隔离验证通过：两个 ARM 静态程序构建成功；WSL 本机版本以 -Wall -Wextra -Werror 编译。验证了各文件精确内容及各自 total、exec 失败文本进入 stderr 文件；先产生 262144 字节 stderr 再产生 262144 字节 stdout 均完整捕获；另用输入控制目标先关闭 stdout，观察 active=1 时捕获器仍存活，随后产生的 stderr 继续被捕获。各场景完成两个 EOF 和子回收，捕获器按约定退出 0。此验证不替代学习者亲手编译或板端运行。

## 为什么需要两条管道

子进程在 exec 前把 stdout 接第一条管道写端，stderr 接第二条管道写端。父进程同时监视两个读端，并将各自字节保存到两个不同文件。本节仍是单线程事件循环，不创建两个读取线程。

~~~text
子[1] → stdout 管道 → 父读端 A → stdout 文件
子[2] → stderr 管道 → 父读端 B → stderr 文件
~~~

如果只把 1 和 2 都 dup2 到同一条管道，数据会混在一起，读端无法可靠恢复它们原先来自哪个标准流。本节用通道身份区分来源，不依靠分析文本内容。

## 为什么不能先读完 stdout，再读 stderr

目标可能保持 stdout 打开，同时大量写 stderr。若父只阻塞等待 stdout 的数据或 EOF，stderr 管道可能写满，目标阻塞在 stderr 写入，不能继续关闭 stdout；父子互等。两路都设非阻塞，并由同一次 poll 等待任意事件，才能避免这种读取次序造成的停顿。

## pollfd 数组

~~~c
struct pollfd watches[2];
watches[0].fd = stdout_read_fd;
watches[0].events = POLLIN;
watches[1].fd = stderr_read_fd;
watches[1].events = POLLIN;
int ready = poll(watches, 2, 1000);
~~~

poll 返回后遍历两个元素，根据各自 revents 处理。返回 2 表示两个元素都有事件，返回 1 表示一个元素有事件；同一元素同时含 POLLIN/POLLHUP 仍只计一个。不能把 ready 当成有事件元素的数组下标，也不能假设第 0 项必定先有事件。

## 本节一次事件只读一块

与上一节循环读空一个通道不同，本节每轮为每个就绪通道最多 read 一次，最多 256 字节，然后回到外层 poll。这样避免一路持续产生数据时，当前线程一直读它而忽略另一条。

理解补讲：学习者能联系到 stderr 未及时消费、缓冲满后写入阻塞，但不清楚“每轮最多一次读取”的含义，当前停留讲解。这里一轮指一次 poll 返回后，for 循环从 i=0 检查到 i=1；若两项都有事件，先给 stdout 一次读取机会，再给 stderr 一次机会，之后回到 poll。若只有一项有事件，就只处理它，不强迫另一项跟着读，也不在空的另一路等待。

例如一次处理先从 stdout 读到 256 字节，再把已就绪的 stderr 20 字节读走；即使 stdout 还有数据，也不在该 for 循环里反复读 stdout。下一轮 poll 会继续报告 stdout 的现有数据就绪，不会为了凑足 1000ms 而强制等待。限制的是单项本轮处理量，不是要求两路数据量相等，也不是创建两个线程。

长期不读 stderr 的正确因果顺序是：数据积压，空间不足时目标的 stderr 写入才可能阻塞。是否发展为死锁取决于实际依赖与输出行为；单纯“读到 EAGAIN 再换路”不必然死锁，这里的限制也用于减少另一通道长期等不到处理的风险。每轮各给一次机会不等于严格实时公平，写文件和其他处理本身仍可能耗时。

poll 按当前就绪状态报告事件：数据尚未读完时，下次仍可报告 POLLIN；写端已关闭而数据未读完时，仍可报告 POLLIN/POLLHUP。不需要等新数据到来才再次报告，所以本节不用在一个通道内一直读到 EAGAIN。

EAGAIN 表示这次无数据，继续检查其他通道并回到 poll；read=0 时，只关闭当前读端、把它的 watches[i].fd 设为 -1，并将 active 减 1。poll 会忽略负 fd 的数组元素。两路都 EOF，active=0，才离开捕获循环并 waitpid。

不要只把 events 设为 0 来替代移除：POLLHUP 等条件仍可能在 revents 返回，若结束通道仍被反复报告，会造成无意义循环。

## 子进程的端点处理

两个 pipe 各有读写端，子进程继承全部四个端点。先 dup2 写端到 1/2，再关闭两条管道的全部原编号，保留 1/2 作为仅需的写引用；同时关闭父为输出文件打开的描述符副本。父关闭两个写端，只保留两个读端。

本节在正常串口环境执行，stdin/stdout/stderr 启动时都已打开，因此新建管道和文件描述符均大于 2；子关闭原编号不会误关刚设置的 1/2。这是本教学启动器的环境前提，不是处理任意缺失标准描述符的通用重定向实现。

父诊断使用父自己的 stdout，留在串口；child setup 在改写 stderr 之前打印，也留在串口。子成功 exec 后，stream_writer 的两类输出均被捕获，不再直接显示在串口。

若 execl 常见失败返回，perror 使用子进程此刻的 stderr，因此错误文本会进入 stderr 捕获文件，而不是原串口。失败子进程 _exit(127) 释放两路写引用，父照常读到 EOF 并回收。

## 两份源文件

poll_capture 负责建管道、打开两个输出文件、fork、子 dup2/exec、父 poll/读取/落文件和回收。代码变长主要因为两个资源集合和错误清理，核心模式与上一节一致。

stream_writer 依次向 stdout 和 stderr 各输出两行，主动刷新 stdout，最后返回 7。同一通道内字节顺序保留；两条独立管道不提供全局时间顺序，不能根据父先处理哪个元素推断目标先执行哪个打印。

输出文件使用 O_TRUNC，每次运行会清空旧内容。请给出两个不同的输出文件。本例按实际 read 长度写文件，不给每个块添加字符串结束符、不把一次 read 当成一行或一个完整消息。

## WSL：阅读与亲手构建

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/06_ipc/05_poll_streams
cat src/stream_writer.c
cat src/poll_capture.c
make -n
make
echo $?
file build/poll_capture build/stream_writer
~~~

预期生成两个 ARM 32 位静态程序。读启动器先找 watches 数组、poll 后的 for 循环，以及 read=0 时 fd=-1/active-- 三个位置，再看创建与清理部分。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/06_ipc/05_poll_streams
~~~

WSL 当前目录传输：

~~~sh
scp -O build/poll_capture build/stream_writer root@192.168.10.2:/root/linux-study/06_ipc/05_poll_streams/
~~~

## COM12：两路分别接收

~~~sh
cd /root/linux-study/06_ipc/05_poll_streams
chmod +x poll_capture stream_writer
./poll_capture ./stream_writer captured_stdout.txt captured_stderr.txt
echo $?
cat captured_stdout.txt
cat captured_stderr.txt
wc -c captured_stdout.txt captured_stderr.txt
~~~

无需按回车。终端看到两通道的事件、读取长度、各自 EOF 和累计字节、both channels reached EOF，随后回收子退出 7。父按教学约定报告捕获过程正常而返回 0，不自动传播子退出码。

stdout 文件预期为 stdout: pid=实际子PID 及 stdout: work complete 两行；stderr 文件为 stderr: pid=同一子PID 及 stderr: diagnostic complete 两行。各自 wc -c 应与本次日志中各通道 total 一致，排除 wc 最后合计行。poll 的 1/2、事件组合和读取分块会随调度改变，不要求每次相同。

## 可选失败对照

~~~sh
./poll_capture ./missing_exec_parent_for_demo/stream_writer failed_stdout.txt failed_stderr.txt
echo $?
cat failed_stdout.txt
cat failed_stderr.txt
~~~

确认目标父目录不存在。预期 stdout 为 0 字节，stderr 文件包含 child execl 错误，两个通道均 EOF，回收子退出 127，父返回 0。这与只重定向 stdout 时错误留在串口不同。

## 边界与后续

两路 EOF 不保证进程已经结束，最后仍要 waitpid。任何后代保留写引用都可能延迟相应 EOF。本例没有目标超时控制；目标长期不关闭通道时，捕获器会继续等待，不把一次 poll 超时当成失败或 EOF。

为输出文件执行 write 本身可能等待存储，这个事件循环不是所有步骤都保证不阻塞。这里学习的是及时读取两条子输出管道。若父捕获失败，本例关闭持有端点，并尝试终止自己创建的子进程后回收，避免错误路径留下等待者。

本节展示基本公平处理机会，不构成实时调度保证。后续可继续学习关闭时机、超时/取消及双向协议；不在此把所有进程管理功能叠加进来。

参考：[poll 的数组、返回值及负 fd](https://man7.org/linux/man-pages/man2/poll.2.html)、[管道容量与 EOF](https://man7.org/linux/man-pages/man7/pipe.7.html)、[dup2](https://man7.org/linux/man-pages/man2/dup.2.html)。
