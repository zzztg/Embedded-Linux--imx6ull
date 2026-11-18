# 实验07-08：两个消费者与关闭广播

状态：已收到两模式板端日志。正常模式先观察两名worker等待，10由worker1取走，0和30由worker2取走；关闭时full1，取完30后两者均退出，分别join的统计为count1/sum10及count2/sum30，总count3/sum40，完成销毁。worker2退出日志先于worker1，但join打印按主线程循环顺序1再2。empty模式两者均等待，关闭广播后两者均退出并join，总count0/sum0，完成销毁。未贴紧接程序的退出状态或编译日志。继续确认广播后的互斥和while检查如何避免重复领取。07-07两模式已有板端输出：关闭后排空30、0作为合法数据、正常count3/sum40与空任务count0/sum0，尚未贴退出状态；学习者正确解释修改closed不主动唤醒等待者，已补充允许虚假唤醒但不能依赖它保证退出。

教师隔离验证：ARM程序以-Wall -Wextra -Werror -pthread交叉编译并静态链接成功，file确认为ARM32位静态程序；WSL本机-O2版本在两名worker均出现初始等待日志后输入，验证正常/empty模式下数据顺序、无覆盖和重复领取、每线程统计与领取记录一致、两条退出及join、总count/sum、销毁和退出0。正式build目录留给学习者亲手构建。

## 只新增一个pthread接口

```c
int pthread_cond_broadcast(pthread_cond_t *cond);
```

成功0、失败直接返回错误编号，继续用check_pthread。signal在有等待者时至少解除一个等待者的等待，不承诺指定线程或严格只唤醒一个；broadcast通知当前所有在该条件变量上等待的线程。

本节保留一个生产者和一个槽位，将消费者从一个增加为两个。put只增加一条可用任务，因此仍signal(not_empty)；关闭改变所有消费者的退出判断，因此broadcast(not_empty)，让所有已等待消费者都能重新检查closed。代码也对not_full广播，完整通知这两类等待队列；本例主线程自己就是唯一生产者，关闭时不会有其他生产者等待，观察重点是not_empty。

## 为什么关闭时一个signal不够保证退出

两个消费者都睡在not_empty上，空槽位关闭时如果只signal一次，可能只解除一个消费者的等待。该消费者检查closed且空就结束，另一个可能仍等待，main的对应join无法完成。不能依赖signal碰巧解除多个等待、虚假唤醒或线程调度来保证退出。

广播仍不积存通知令牌；若某消费者在关闭后才检查，保留的closed状态保证它不会进入错误等待。

## 广播不等于同时拥有锁

两个线程收到通知后仍需竞争同一把mutex；wait成功返回时才已经持锁。它们依次检查full和closed、按状态取数据或退出，不能同时进入临界区。while(!full && !closed)仍然保留。

若将来对一次放入广播，可能多个消费者醒来，但只有一条任务；一个取走后，另一个拿锁会发现已空且未关闭，必须再次等待。广播是让所有人重新检查，不是复制任务给每个人。

## 两个线程的参数与统计

```c
pthread_t threads[WORKER_COUNT];
struct worker_info infos[WORKER_COUNT] = {0};
```

WORKER_COUNT是本程序宏，值2。threads数组保存两条线程的标识；infos数组保存两份参数与结果，每份有id、count、sum。{0}将成员初始化为0。

创建循环里先给infos[i].id赋值为i+1，再将&infos[i]传给相应worker。id是我们定义的显示编号，不是pthread_t或Linux TID。两个对象一直存活到join之后，不能偷换为把同一个循环变量&i交给所有线程。

每条线程只修改自己的info.count和info.sum。main在成功join对应线程后才读取这份结果，再用自己的total_count和total_sum汇总，避免让两个消费者无锁累加同一个共享统计变量。另一条线程仍可能执行，但它修改的是不同数组元素。

不能保证每个消费者都拿到任务，可能一个处理全部三条，另一个处理零条。mutex/条件变量不保证任务平均分配或编号交替。单槽位按顺序取走10/0/30，但多线程的后续业务处理完成顺序不一定与取走顺序相同。

## 输入与两种模式

创建两个消费者后，main打印提示，并在不持mutex时等待一次回车。先看到worker 1和worker 2各自打印empty and open; waiting，再按回车。等待日志会fflush(stdout)，便于观察；提示与两条日志的先后可以不同，不用sleep猜时机。

- ./broadcast_demo：生产10、0、30，随后广播关闭；总count3/sum40。
- ./broadcast_demo empty：不生产数据，直接广播关闭；总count0/sum0。此模式更直观观察两个等待者都结束。

提前回车也不能破坏状态协议，但为观察广播，应等两条初始等待日志。不要把“打印过等待日志”推广为对任意时刻内核状态的精确测量。

两条线程都要join完成才能销毁条件变量与mutex。只join一个或一看到某条exiting就destroy都不安全；日志说明业务路径走到了那里，不等于所有线程已停止使用同步对象。

## WSL：阅读与编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/08_broadcast
cat src/broadcast_demo.c
make -n
make
echo $?
file build/broadcast_demo
```

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/08_broadcast
```

## WSL：传输

```sh
scp -O build/broadcast_demo root@192.168.10.2:/root/linux-study/07_threads/08_broadcast/
```

## COM12：分别运行

```sh
cd /root/linux-study/07_threads/08_broadcast
chmod +x broadcast_demo
./broadcast_demo
```

等两条worker初始等待日志后，只按一次回车，程序结束后：

```sh
echo $?
./broadcast_demo empty
```

同样等两条初始等待日志后，只按一次回车，程序结束后：

```sh
echo $?
```

正常模式检查10/0/30各被取一次、两条worker都打印closed and empty; exiting、两条join都完成、总count3/sum40。empty模式没有put/take，总count0/sum0且两条worker都结束。两种模式均退出0。统计分配、退出先后和等待日志次数可以不同。

参考：[条件变量通知与广播](https://man7.org/linux/man-pages/man3/pthread_cond_broadcast.3p.html)、[等待与重新检查](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html)。
