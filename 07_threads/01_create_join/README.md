# 实验 07-01：创建线程、等待结束与共享内存

状态：学习者已上板运行，主线程与工作线程打印的进程 PID 均为 4023；工作线程将整数从 10 改为 99，主线程在 join 后读到 99，退出状态 0。尚未收到学习者编译日志。2026-11-11路线调整：线程基础第一轮已完成，FIFO补入06_ipc/08_fifo；共享内存、消息队列等不再设独立进阶专项，实际用到时再学。本节所说线程共享地址空间仍是已学基础概念。

教师隔离验证通过：ARM 静态程序以 -pthread 编译链接成功；WSL 本机版本以 -Wall -Wextra -Werror -pthread 编译。验证主线程与工作线程打印相同进程 PID，工作线程初始值 10、改成 99，join 之后主线程读取 99，退出 0。此验证不替代学习者亲手编译或板端运行。

## 当前应用学习主线

当前顺序为：FIFO补课；网络编程；通过已有驱动操作硬件接口；调试、工程化与综合项目；再进入驱动、设备树与系统构建。独立信号和IPC进阶专项已按学习者要求取消，相关知识在项目需要时补充，完整进度以根目录学习路线为准。

## 本节只增加两个核心接口

pthread_create 在当前进程中创建线程；pthread_join 等待指定的可连接线程结束并完成相应资源回收。本节不加入互斥锁、条件变量或信号配置。

进程是运行程序的资源组织单位，一个进程可以有多个执行线程。本例 main 由主线程执行，新线程从 worker 函数开始执行，并非像 fork 一样在创建调用返回处得到另一条分支。

同一进程内线程共享地址空间、进程 PID 和文件描述符表；各自有执行栈及部分线程私有状态。单核 i.MX6ULL 也可以由调度器交替运行多个线程，不意味着同时占用多个 CPU 核。

## 先解释类型和函数入口

~~~c
pthread_t worker_thread;
static void *worker(void *arg);
~~~

pthread_t 是系统定义的线程标识类型，worker_thread 是我们自己取的变量名，用于以后指定要等待哪个线程。不要把它当作 PID 或任意用 %d 打印；本节只传回 pthread 接口。

worker 是线程入口函数。参数 void *arg 是通用数据指针，本例接收主线程传来的 int 地址；返回类型 void * 表示入口结束时返回一个指针值，本例用 NULL，不返回数据。worker 名称不是内置关键字，可以改名。

~~~c
int *number = arg;
*number = 99;
~~~

本例用 C，void * 可以赋给对象指针。局部指针 number 保存同一个 int 对象的地址；*number 操作的是那个对象。它不是在工作线程里创建另一份内容为 10 的 int 副本。指针变量自身仍有它自己的局部生命周期。

## pthread_create 的四个参数

~~~c
int rc = pthread_create(&worker_thread, NULL, worker, &number);
~~~

| 参数 | 用途 |
|---|---|
| &worker_thread | 输出新线程标识的地址 |
| NULL | 使用默认线程属性，本例为可连接线程 |
| worker | 新线程入口的函数地址，不加括号、不立即当普通函数调用 |
| &number | 传给入口 arg 的数据指针 |

注意第一个地址是保存线程标识的位置，第四个地址是业务数据位置，不能混淆。

这里传的是 main 中 number 的地址。main 在 join 之前不离开作用域，因此工作线程使用时这个对象仍然存活。不能机械推广为返回一个已经结束的普通函数的局部变量地址，或把生命周期短于线程使用期的对象地址交给线程。

## pthread_join

~~~c
rc = pthread_join(worker_thread, NULL);
~~~

第一个参数是标识本身，而不是它的地址；第二个参数 NULL 表示本节不接收线程入口返回的指针值。若工作线程还没结束，主线程可以在这里阻塞等待；若已结束，可以直接完成连接。它不负责强制终止目标线程。

join 成功后，工作线程已经结束，主线程可以观察它此前对本例 number 的修改。join 在本例同时提供执行顺序与内存同步，不靠 sleep 猜时机。主线程只在创建前和 join 成功后访问 number，工作线程在中间修改，所以本例没有未同步的并发读写。

不要删掉 join 再把竞争结果当成可预测实验；未同步的并发读写将在下一节解释。volatile 不替代线程同步，本例不需要给 number 加 volatile。

## 本节特别注意返回值约定

pthread_create/pthread_join 成功返回 0，失败直接返回错误编号，不能照搬 read/write 的 -1+errno 判断法。

~~~c
if (rc != 0) {
    fprintf(stderr, "pthread_create: %s\n", strerror(rc));
}
~~~

这里 strerror 使用 rc，不是 errno。pthread 接口的约定再次说明必须按具体接口的返回值规则处理错误。本节这两个函数不以 EINTR 返回错误，不需要套用 read 的 EINTR 重试循环。

## 进程与线程的变量对照

fork 示例中父子拥有独立普通内存，子改 number 不改变父变量。本例工作线程通过指针访问主线程的同一个 int，join 后主线程应看到 99。

各线程有自己的栈不等于栈内存被进程隔离。在对象有效期内，可以通过有效地址访问同一地址空间中的对象。本例就是主线程栈上的 int 被工作线程通过指针访问；同名局部变量则不会自动变成同一个对象。

getpid 在两个线程里返回相同进程 PID。pthread_t 线程标识与 Linux 内核 TID 是另一个话题，后续再观察，不为打印编号额外加入系统调用。

之前讨论的 errno 是每线程独立状态，即使一般内存共享，也不把两个线程的 errno 理解成共用一个全局错误变量。

## 线程结束与进程结束

worker 中 return NULL 结束这条工作线程，不结束主线程。main 中 return 0 则结束整个进程，包括仍存活的其他线程。本例先 join 再从 main 返回。

没有 fork，因此不是主进程和子进程；本节称主线程、工作线程。POSIX 线程也没有 waitpid 那样限定只能由父进程回收子进程的同类父子关系。

## 编译选项

Makefile 新增我们自己命名的 THREADFLAGS=-pthread，在编译和链接两步都传给 GCC，启用适当的线程编译/链接支持。其余 -std=c11、警告、静态链接与自动依赖方式保持一致。

## WSL：阅读与亲手编译

~~~sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/01_create_join
cat src/thread_demo.c
make -n
make
echo $?
file build/thread_demo
~~~

预期为 ARM 32 位静态程序，预览中编译、链接两步都有 -pthread。

## 传输

COM12 创建目录：

~~~sh
mkdir -p /root/linux-study/07_threads/01_create_join
~~~

WSL 当前目录传输：

~~~sh
scp -O build/thread_demo root@192.168.10.2:/root/linux-study/07_threads/01_create_join/
~~~

## COM12：运行

~~~sh
cd /root/linux-study/07_threads/01_create_join
chmod +x thread_demo
./thread_demo
echo $?
~~~

预期输出：

~~~text
main: pid=实际PID, before create number=10
worker: pid=同一PID, initial number=10
worker: changed number=99; returning
main: after join number=99
~~~

退出状态为 0。本例首行在 create 之前，最后一行在 join 之后，因此这几行有明确顺序；不能把它推广成任意线程的输出都有固定顺序。

## 之后的线程学习

先学参数/返回值/生命周期，再学共享变量的数据竞争与互斥锁、条件变量的等待通知、生产者消费者与退出回收。不会只堆加 API 名称；每次先解释接口和机制，再进入代码。

参考：[pthread_create](https://man7.org/linux/man-pages/man3/pthread_create.3.html)、[pthread_join](https://man7.org/linux/man-pages/man3/pthread_join.3.html)、[线程共享与独立状态](https://man7.org/linux/man-pages/man7/pthreads.7.html)。
