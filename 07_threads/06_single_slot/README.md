# 实验 07-06：单槽位生产者与消费者

状态：已收到学习者板端输出，10/20/30分别先put后take，count=3、sum=60、expected_sum=60，join和同步对象销毁完成。生产者满等出现两次，消费者空等未出现，属于允许的调度结果；未从日志推断不可见的锁竞争过程。实际板端提示目录为07_threads/06_single_shot，本地材料目录为06_single_slot；记录差异，不自动改名。尚未贴出紧接程序的退出状态或编译日志。继续确认取走数据时在锁内复制的必要性。上一节07-05已收到两阶段板端输出，学习者正确解释等待期间若不释放mutex，主线程会阻塞在获取锁处，无法修改业务状态并通知；已澄清业务条件ready与条件变量changed的区别。07-05退出状态尚未贴出。本节复用其条件变量协议，不增加新的pthread接口；已核对正式源码与隔离验证源码的SHA256一致。

教师隔离验证：ARM版本通过-Wall -Wextra -Werror -pthread交叉编译并静态链接，file确认为ARM32位静态程序。WSL本机-O2版本验证日志中的单槽位占用规则、无未取走任务覆盖、10/20/30各取一次且顺序一致、count3/sum60、正常销毁和退出0；不要求固定等待日志数量。正式build目录留给学习者构建。

## 本节解决什么问题

主线程是生产者，按顺序提交10、20、30三个整数；工作线程是消费者，逐个取走并累计。共享缓冲只有一个槽位，必须满足：槽位满时生产者不能覆盖尚未取走的数据；槽位空时消费者不能把旧内容再取一次。

TASK_COUNT是本程序定义的宏，值为3，生产和消费两边都执行相同次数。这是固定工作量的简化实验，不包含“生产结束但任务数量未知”的退出协议，后续再引入关闭状态。

## 共享状态和两个条件变量

```c
static int slot = 0;
static int full = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;
```

| 对象 | 含义 |
|---|---|
| slot | 实际存放的一条整数任务 |
| full | 业务状态，0空、1有一条尚未取走的任务 |
| mutex | 保护slot和full，双方使用同一个锁对象 |
| not_empty | 消费者等待“槽位非空”的条件变量 |
| not_full | 生产者等待“槽位未满”的条件变量 |

not_empty和not_full只是我们给条件变量取的名称，它们不会自动检查槽位。条件由while(full)与while(!full)判断；通知只提示相关等待者重新检查。

## 生产者put_item

```c
pthread_mutex_lock(&mutex);
while (full) {
    pthread_cond_wait(&not_full, &mutex);
}
slot = item;
full = 1;
pthread_cond_signal(&not_empty);
pthread_mutex_unlock(&mutex);
```

实际源码检查接口错误。满时wait释放mutex并等待，因此消费者有机会拿锁取走任务；wait成功返回已重新持锁，再检查full。空时写入任务并置满，然后通知等非空的消费者。

整个“检查是否空 → 写入 → 标记满”必须在同一把锁保护下完成。只锁住slot赋值不能维护整个槽位状态协议。

## 消费者take_item

```c
pthread_mutex_lock(&mutex);
while (!full) {
    pthread_cond_wait(&not_empty, &mutex);
}
int item = slot;
full = 0;
pthread_cond_signal(&not_full);
pthread_mutex_unlock(&mutex);
return item;
```

消费者先将slot内容复制到自己的局部item，再标记空、通知生产者。函数返回的是整数值，不是局部变量地址，所以函数结束不影响调用者收到该整数副本。

full=0不要求slot清零。取走10之后，slot中可以还保留10，但full=0表示那不是一条可再次领取的任务。可用性由状态表示，不能靠slot是不是0判断，因为0也可以是合法任务数据。

消费者解锁后，生产者可能马上把slot改成下一个任务；消费者仍然拥有自己的item副本。复制取走与后续处理应区分，耗时处理通常放在锁外。

## 一轮状态变化

```text
full=0（空）
    |
    | 生产者持锁写slot=10，置full=1，通知not_empty
    v
full=1（有任务）
    |
    | 消费者持锁复制slot到item，置full=0，通知not_full
    v
full=0（可接受下一条）
```

如果生产者很快，下一次put会发现满并等待，形成背压；如果消费者先运行或很快，take发现空并等待。它与前面管道容量不足时发送方等待的思想相通，但这里是同一进程内应用自己实现的共享槽位，不是内核管道。

## 为什么要两个条件变量

本例有两类等待理由：消费者等非空，生产者等未满。分开命名和通知，能清楚表达“状态改变后应该提醒谁”。两者都配合同一把mutex，保护同一组状态，不是每个条件变量配一把独立锁。

通知不自动改变full，也不释放mutex。signal时如果另一方还没等待，不会留下通知令牌；但full已在锁内更新，另一方之后检查就能决定是否还需等待。

## 参数、统计与结束

main创建struct result对象，初始count和sum均为0，将其地址传给worker。只有worker修改这两个成员；main直到join成功后才读取，因此统计对象没有未同步的并发访问，不额外使用槽位mutex。不是所有共享地址都必须每次加锁，关键是是否存在需要协调的冲突访问。

worker按固定次数取任务，累计count=3、sum=60；main按固定次数提交并计算expected_sum=60。两边的循环次数一致，消费者自然结束，main再join并销毁两个条件变量和互斥锁。

本例只保证取走任务按提交顺序且不覆盖，put返回不表示任务处理已完成。所有任务处理完成在本例由join保证。动态任务结束、错误退出、多生产者/多消费者等机制后续逐步展开。

check_pthread沿用前节错误策略。put_item和take_item是普通C辅助函数，不是新系统接口。take_item的int返回值是业务任务值，与pthread函数返回错误编号的约定不同。

## WSL：亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/06_single_slot
cat src/slot_demo.c
make -n
make
echo $?
file build/slot_demo
```

预期ARM32位静态程序，编译退出0。

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/06_single_slot
```

## WSL：本实验目录传输

```sh
scp -O build/slot_demo root@192.168.10.2:/root/linux-study/07_threads/06_single_slot/
```

## COM12：运行

```sh
cd /root/linux-study/07_threads/06_single_slot
chmod +x slot_demo
./slot_demo
echo $?
```

无需回车或其他输入。一次可能的输出：

```text
main: one slot; producing 3 tasks
main: put value=10; full=1
main: full=1; waiting for an empty slot
worker: take value=10; full=0
worker: full=0; waiting for an item
main: put value=20; full=1
main: full=1; waiting for an empty slot
worker: take value=20; full=0
worker: full=0; waiting for an item
main: put value=30; full=1
worker: take value=30; full=0
main: joined; count=3, sum=60, expected_sum=60
main: synchronization objects destroyed
```

等待日志的数量与位置受调度影响，可以不同；不要求每一类等待都必然出现。检查每次put后对应take，没有下一条覆盖上一条，取走顺序为10、20、30，最后count3、sum60，退出0。日志在持锁区域打印，是为了让这次教学中的状态变化可观察，不建议把耗时业务处理放进临界区。

参考：[条件变量等待](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html)、[条件变量通知](https://man7.org/linux/man-pages/man3/pthread_cond_broadcast.3p.html)。
