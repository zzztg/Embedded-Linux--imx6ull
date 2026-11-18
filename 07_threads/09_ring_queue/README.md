# 实验07-09：有界环形队列

理解补充：学习者正确计算“取两条、放50”后的head0/tail2/count2，曾将逻辑顺序0/30/40误认为buffer[0..2]的物理排列；已澄清初始物理数组为[40,0,30]、head1，对应逻辑领取顺序为下标1/2/0上的0/30/40。操作后的剩余逻辑数据为40/50，学习者明确是术语理解问题。

状态：已收到两模式板端日志。正常模式预填充3条后head0/tail0/count3，生产者三次满等，每取走10/0/30后补入40/50/60；关闭时remaining3，随后全部排空；worker2处理全部6条、sum190，worker1处理0条，两者退出并join、销毁完成，属允许调度。empty模式count0/sum0，两者退出并join、销毁完成。未贴紧接程序的退出状态或编译日志；继续确认下标回绕与逻辑顺序。07-08两消费者正常与empty模式已有板端输出，均完成退出和join、总count3/sum40与count0/sum0，尚未贴退出状态。学习者最初把广播后一名消费者取走唯一任务后另一名消费者的动作理解为退出；已纠正为“空且未关闭则重新等待，空且关闭才退出”，学习者确认理解。

教师隔离验证：ARM版本以-Wall -Wextra -Werror -pthread交叉编译并静态链接成功，file确认为ARM32位静态程序。WSL本机-O2版本逐条核对put/take日志中的实际下标、head/tail/count状态转换、FIFO和恰好一次交付、个人统计与总count6/sum190，以及正常/empty关闭、两者退出和join、销毁及退出0。另固定边界验证：满时head==tail、取一条补一条回绕、0为合法数据、满队列关闭后排空、结束不修改输出、关闭后拒绝提交。正式build目录留给学习者构建。

## 从一个槽位扩展到三个

不增加新的pthread接口，继续一个生产者、两个消费者和正常关闭协议。新内容是数据结构：一个长度3的数组，以及读位置head、写位置tail、队列中有效数据量count。

```c
#define CAPACITY 3
static int buffer[CAPACITY];
static int head = 0;
static int tail = 0;
static int count = 0;
```

head表示下一次取出的位置，tail表示下一次放入的位置；它们是应用维护的数组下标，不是内核文件偏移。count是未被取走的数据数量，不是已经累计生产或消费的总数，也不同于infos[i].count这份个人统计。

约束始终为0<=count<=CAPACITY，0<=head,tail<CAPACITY。空为count==0，满为count==CAPACITY。使用独立count区分空满，因此数组三个元素都可以存有效任务，不需要额外预留一个空位。

## 为什么称作环形

数组仍是连续的三个整数，逻辑上下标循环：0→1→2→0。用C取余运算%实现：

```c
tail = (tail + 1) % CAPACITY;
head = (head + 1) % CAPACITY;
```

3%3等于0，因此下标2的下一位置回到0，而不是访问越界的buffer[3]。所有下标非负，不涉及负数取余。

| 操作完成后 | head | tail | count | 按领取顺序的有效数据 |
|---|---:|---:|---:|---|
| 初始 | 0 | 0 | 0 | 无 |
| 放10 | 0 | 1 | 1 | 10 |
| 放0 | 0 | 2 | 2 | 10、0 |
| 放30 | 0 | 0 | 3 | 10、0、30 |
| 取10 | 1 | 0 | 2 | 0、30 |
| 放40 | 1 | 1 | 3 | 0、30、40 |

head==tail在空和满时都可能出现，不能单靠它判断状态。最后一行数组物理内容是[40,0,30]，逻辑读取从head=1开始，因此顺序仍为0、30、40。取走后可以保留旧值，是否有效由head/tail/count决定，不靠清零判断。

## 放入和取走

生产者持锁后：while(count==CAPACITY && !closed)等待not_full；如果已closed就拒绝。可以放入时，写buffer[tail]、推进tail、count++，然后signal not_empty再解锁。完整的检查/写入/下标/数量更新都在同一个临界区，不是只给数组赋值加锁。

消费者持锁后：while(count==0 && !closed)等待not_empty；如果循环后count仍0，则已经关闭且空，解锁返回结束。否则先通过*out复制buffer[head]，再推进head、count--，通知not_full，解锁返回成功。

index是函数内局部变量，用来保存本次实际访问的数组下标，便于打印。日志中的head/tail/count都是本次操作完成后的状态。该局部index不是跨线程共享队列状态。

## 与前几节的关系

- 互斥：两消费者及生产者共同维护同一组队列状态，全部使用同一把mutex。
- 等待：满则生产者释放锁等待空间，空且未关闭则消费者释放锁等待数据。
- 通知：每放入一条signal not_empty，每取走一条signal not_full；已在队列中的数据可以被消费者连续取走，不需要一条通知令牌对应一次take。
- 关闭：closed=1后广播两类等待者，不清空count或数据；消费者取完全部剩余数据后才退出。
- 回收：每名消费者使用自己的infos元素统计，main分别join后汇总，再销毁同步对象。

缓冲增大能吸收短暂速度差，不能让生产者无限领先或自动提高消费者长期处理速度。生产持续快于消费时仍然会满并背压。两个消费者的分配不保证平均，取走顺序为FIFO，锁外处理完成先后不保证FIFO。

## 为什么先填满再创建消费者

正常模式先用put_item放入10/0/30，保证能观察head0/tail0/count3这个满状态，然后才创建两消费者并提交40/50/60。预填充最多CAPACITY条，不能在消费者创建前尝试放满全部6条，否则第4条会等待空间，而消费者还没创建，无法推进。

expected_count<CAPACITY ? expected_count : CAPACITY是C条件运算符，计算不超过容量的预填充数量。预填充时没有其他线程，打印head/tail/count不需要额外加锁。消费者创建后的队列日志都在mutex保护下打印。

## 两种模式

- ./ring_demo：提交10、0、30、40、50、60；总count6/sum190，读写位置发生回绕。
- ./ring_demo empty：提交0条；两消费者在关闭且空时退出，总count0/sum0。

本节自动运行，不要求输入。容量3、消费者2、任务6是演示配置。已有参数解析、线程数组和统计方式沿用07-08。

## WSL：阅读和亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/09_ring_queue
cat src/ring_demo.c
make -n
make
echo $?
file build/ring_demo
```

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/09_ring_queue
```

## WSL：本实验目录传输

```sh
scp -O build/ring_demo root@192.168.10.2:/root/linux-study/07_threads/09_ring_queue/
```

## COM12：运行

```sh
cd /root/linux-study/07_threads/09_ring_queue
chmod +x ring_demo
./ring_demo
echo $?
./ring_demo empty
echo $?
```

正常模式固定起始：

```text
main: put value=10 at=0; head=0 tail=1 count=1
main: put value=0 at=1; head=0 tail=2 count=2
main: put value=30 at=2; head=0 tail=0 count=3
main: before starting workers; head=0 tail=0 count=3
```

之后调度日志不固定。检查所有put/take日志count始终0到3、下标始终0到2；take按10/0/30/40/50/60且各一次；最终两消费者退出并join，总count6/sum190，完成销毁、退出0。empty总count0/sum0且正常退出0。关闭时remaining可以为0到3，不要求最后take出现在关闭之前。

参考：[条件变量等待](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html)、[条件变量通知与广播](https://man7.org/linux/man-pages/man3/pthread_cond_broadcast.3p.html)。
