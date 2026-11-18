# 实验07-07：单槽位的关闭与正常退出

状态：已收到学习者两种模式板端日志。正常模式10/0/30依次取走，关闭时full=1，关闭后仍取走30，再报告关闭且空，count3/sum40并完成join和销毁；empty模式关闭时full=0，count0/sum0并完成join和销毁。两次紧接程序的退出状态及学习者编译日志尚未贴出。继续确认设置closed与通知等待者是不同动作。07-06已收到10/20/30按序先放后取、count3/sum60和销毁完成的板端日志，尚未贴退出状态。学习者正确解释：若解锁后才复制slot，生产者可能已写入下一条任务；已补讲复制与标空必须在同一临界区内。

教师隔离验证：ARM程序以-Wall -Wextra -Werror -pthread交叉编译并静态链接成功，file确认为ARM32位静态程序；WSL本机-O2版本验证正常及empty模式，检查占用状态、数据顺序、不覆盖、0作为合法数据、关闭后排空才结束，均退出0。另以单线程核心边界验证固定覆盖“满时关闭后取出0、再取返回结束且不修改输出、关闭后put拒绝”。正式build目录留给学习者亲手构建。

## 从固定次数改为状态驱动退出

上一节消费者用for循环明确取三次。本节生产者仍用三条数据演示，但消费者不知道任务数，不读取生产者的expected_count，而是重复调用take_item，直到接口报告已关闭且无剩余数据。

新增普通整数closed，0表示仍允许提交，1表示以后不再接收新任务。它与slot、full使用同一把mutex保护。这里是应用协议的关闭状态，不是文件描述符close系统调用，也不意味着销毁互斥锁。

| full | closed | 消费者动作 |
|---|---|---|
| 0 | 0 | 暂时没数据，等待 |
| 1 | 0 | 取出数据 |
| 1 | 1 | 先取走剩余数据 |
| 0 | 1 | 已结束，退出 |

因此close_slot不把full清零，也不清除slot。关闭停止接收新任务，已经接收的数据仍然需要处理。

## 消费者的等待条件

```c
while (!full && !closed) {
    pthread_cond_wait(&not_empty, &mutex);
}
```

只有“空并且未关闭”才等待。&&表示两个条件都成立。关闭后，即使槽位空，也不能继续等一个永远不会再来的任务。循环退出时仍持有锁；此时若!full为真，就必然已经closed，可以解锁并返回结束状态。若full为真，无论closed如何，都先复制数据并标空。

所有锁和条件变量调用保持前节的错误检查策略，本节不新增pthread接口。

## 返回状态与业务数据分开

上一节take_item直接返回整数任务值；本节改为：

```c
static int take_item(int *out);
```

- 返回1：取到一条数据，并通过*out写给调用者。
- 返回0：已关闭且空，没有新数据，out所指整数不在这条路径上被修改。
- 意外pthread错误仍由check_pthread报告并结束演示进程，不是take_item返回0。

调用者：

```c
int item;
while (take_item(&item)) {
    result->count++;
    result->sum += item;
}
```

这里while判断的是take_item的返回状态，数据存入item；不是判断item是否为0。第一次成功读取前虽然item未初始化，但take_item只在返回1时写入它；循环体只有在返回1时才执行，所以不会使用未初始化数据。

本例生产10、0、30，0是合法数据，最终count3/sum40，不能把0当作结束标记。这与read返回0表示EOF的“状态和数据分别表达”有相似之处，但不要把本函数状态规则套成系统调用返回字节数的规则。

put_item也改为返回1表示接收成功，0表示已关闭、拒绝提交。它在持锁时先while(full && !closed)等待；只要closed为1，就解锁拒绝，而不是永远等空位。main是唯一生产者，在所有提交完成后才关闭，正常演示不会走拒绝分支。

## 为什么关闭后还要通知

```c
pthread_mutex_lock(&mutex);
closed = 1;
pthread_cond_signal(&not_empty);
pthread_cond_signal(&not_full);
pthread_mutex_unlock(&mutex);
```

修改closed不会自动唤醒已经睡在cond_wait中的线程。通知让等待者有机会重新拿锁、检查full和closed并作决定。

虽然not_empty名字表示等非空，实际允许消费者继续的状态现在是“有数据或已关闭”。它是对象名称，不会限制只能在放入数据时通知。关闭也可能使等待结束，故同样需要通知。

本例只有一个生产者和一个消费者，所以分别signal这两类等待者足够。不要直接推广为多个等待线程的通用关闭协议，多等待者通常需要broadcast，后续再学。通知不积存，但closed状态保留；若消费者在关闭之后才检查，会直接决定取剩余数据或退出，不会再错误等待。

close_slot先设置状态再通知；join放在关闭之后，且调用join时不持mutex。否则消费者可能等新数据，主线程却等消费者结束，形成无法推进的等待。

## 正常关闭与销毁的顺序

```text
提交最后一条任务
    ↓
close_slot：不再接收新任务，通知等待者
    ↓
消费者取完剩余任务，检查到关闭且空，正常返回
    ↓
main成功join
    ↓
销毁条件变量和互斥锁
```

close_slot返回不等于任务处理完成，也不等于worker已结束。消费者完成处理在本例通过join确认。关闭标记是正常业务协议，不是发送SIGTERM、取消线程或提前destroy。

## 两种运行方式

- ./close_demo：提交10、0、30然后关闭，期望count3/sum40。
- ./close_demo empty：一条任务也不提交，直接关闭，期望count0/sum0；消费者仍应正常结束。

main通过argc/argv及strcmp识别empty。items是生产者的普通整数数组，expected_count只用于提交与主线程最终核对，不传给消费者。错误参数返回2。

## WSL：阅读与亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/07_slot_close
cat src/close_demo.c
make -n
make
echo $?
file build/close_demo
```

预期编译退出0，ARM32位静态程序。编译与链接均有-pthread。

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/07_slot_close
```

## WSL：在本实验目录传输

```sh
scp -O build/close_demo root@192.168.10.2:/root/linux-study/07_threads/07_slot_close/
```

## COM12：分别运行

```sh
cd /root/linux-study/07_threads/07_slot_close
chmod +x close_demo
./close_demo
echo $?
./close_demo empty
echo $?
```

无需回车推进。正常模式要看到10、0、30各取一次，最后count=3,sum=40；empty模式没有put/take日志，最后count=0,sum=0。两种运行都应看到closed and empty、join完成、销毁完成，退出0。

queue: closed=1; full=1表示关闭时还留有任务，之后必须看到对应take；full=0表示关闭前最后任务已取走。两种调度都合法，不要求等待日志或关闭日志固定出现在最后一次take之前。

参考：[条件变量等待](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html)、[条件变量通知](https://man7.org/linux/man-pages/man3/pthread_cond_broadcast.3p.html)。
