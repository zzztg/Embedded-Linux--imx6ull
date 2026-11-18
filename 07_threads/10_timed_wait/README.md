# 实验07-10：条件变量的绝对截止时间与超时

状态：两种模式均已上板。默认模式日志为ETIMEDOUT、重新持锁且ready0、outcome=timeout，join及销毁完成，紧接退出0；ready模式取得42、outcome=data，join及销毁完成，紧接退出0。板端未独立测量经过时长，未收到学习者编译日志；继续确认一次计算绝对截止时间与重新检查条件的理解。07-09两种模式已有板端输出，count6/sum190及空任务count0/sum0，均退出业务流程并join、销毁，尚未贴Shell退出状态。学习者正确推演head0/tail2/count2，起初把逻辑领取顺序当作数组物理排列；已澄清head是下标、从head开始领取count条，环形逻辑序列与物理布局的区别，学习者明确误解来源。

教师隔离验证：ARM版本以-Wall -Wextra -Werror -pthread交叉编译并静态链接，file确认为ARM32位静态程序。WSL本机-O2版本默认模式约3.008秒后ETIMEDOUT并正常join、清理、退出0；ready模式跳过等待并取得42、退出0。另在同一等待期间发出四次不改变ready的通知，工作线程每次重新检查后仍用原截止时间，约3.000秒结束；验证工作线程已释放锁。正式build目录留给学习者亲手编译。

## 为什么先用一个工作线程

本节单独观察“条件不成立时等到什么时刻”，使用一个工作线程和main，复用条件变量、业务ready、结果指针和join。不继续堆队列细节。超时等待的概念随后可以用于队列take/put。

## 新接口与类型先解释

```c
int pthread_cond_timedwait(pthread_cond_t *cond,
                          pthread_mutex_t *mutex,
                          const struct timespec *abstime);
```

前两个参数与cond_wait相同，第三个是绝对截止时刻的地址。调用前持锁，等待时释放锁；成功返回或返回ETIMEDOUT时已重新获得mutex。超时不自动终止线程，不销毁锁，也不等于数据值0；后续是否退出、重试或报错由业务代码决定。

time.h声明struct timespec，它包含tv_sec（秒部分，time_t类型）和tv_nsec（纳秒部分，long类型）。1秒为1000000000纳秒，有效tv_nsec为0至999999999。不是两个时间长度相加的独立超时参数，而是共同描述一个时间点。

clock_gettime(CLOCK_REALTIME, &deadline)读取当前系统实时时钟，成功0，失败-1并设置errno；所以使用perror。pthread_cond_timedwait成功0，超时直接返回ETIMEDOUT，其他错误直接返回对应编号，所以使用rc判断，不读errno。这是两套接口返回规则，不能混用。

## 截止时间，不是等待时长

```c
struct timespec deadline;
clock_gettime(CLOCK_REALTIME, &deadline);
deadline.tv_sec += 3;
```

假设当前为100秒500000000纳秒，截止就是103秒500000000纳秒。这里仅加整数秒，不改变有效的纳秒部分。若扩展为加毫秒，需要处理纳秒进位，不能直接让tv_nsec超出有效范围。

不能只设置tv_sec=3、tv_nsec=0来表达从现在起等待3秒：对于CLOCK_REALTIME，那是在纪元起点后的第3秒，通常早已过去。

本例条件变量用默认静态初始化，其时钟为CLOCK_REALTIME，截止时间也必须来自该时钟。不能只把clock_gettime改成CLOCK_MONOTONIC就声称完成切换，条件变量自身的时钟属性也要匹配。CLOCK_REALTIME受系统校时影响；工程中测量经过时长通常考虑CLOCK_MONOTONIC并配置条件变量时钟属性，后续系统时间专题展开。本实验期间保持系统时钟稳定即可，不要求板子日期准确。

3秒是这次条件等待的截止预算，不是整个函数严格3秒内返回的硬期限；调度和重新获取mutex都可能使实际返回更晚，也不包含调用timedwait之前的锁竞争时间。本例没有其他线程持mutex造成争用。

## 为什么deadline只计算一次

deadline在while外计算。wait返回0仍需while检查ready，如果条件仍不满足，使用同一个deadline继续等，避免每次额外通知或虚假唤醒都把总等待预算重新加3秒。

例如最初截止12:00:03，12:00:02醒来发现ready仍0，应继续等到12:00:03。若重新计算当前时间加3秒，就延长到12:00:05，违背本次等待预算。

## ETIMEDOUT之后的处理

代码遇到ETIMEDOUT时跳出等待循环，但仍持锁，再检查ready：若为真，按本例策略优先接收已经准备好的数据；否则记录无数据超时并结束本次工作。超时边界附近可能与生产者更新条件竞争，不能只看返回码就不检查共享状态。

当前实验默认没有生产者更新状态，因此观察到ETIMEDOUT且ready0；ready模式则在创建线程前已经准备好，while直接跳过。两模式不是演示“每次cond_timedwait返回0就有数据”，该规则仍然错误。

wait_result是我们定义的结果结构，got_data表示是否取得数据，value保存取得的数据。只有got_data为1才将value视为业务结果。工作线程写结果，main在join后读取。check_pthread沿用先前策略，但ETIMEDOUT在调用它之前单独处理，属于本实验预期情况。

## 两种模式与预期

- ./timed_demo：没有数据到来，约3秒后ETIMEDOUT，worker自己解锁并return，main join后打印outcome=timeout，整个程序退出0表示预期超时路径完成。
- ./timed_demo ready：main在创建worker前将value42、ready1，worker检查已就绪而不调用timedwait，取得42并完成，退出0。

不需要按回车，不用sleep模拟数据到达。ready模式用来观察业务条件已经成立时应跳过等待，并不是一个生产者在等待中发通知的实验；通知路径已在07-05等实验观察。

## WSL：阅读与亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/10_timed_wait
cat src/timed_demo.c
make -n
make
echo $?
file build/timed_demo
```

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/10_timed_wait
```

## WSL：传输

```sh
scp -O build/timed_demo root@192.168.10.2:/root/linux-study/07_threads/10_timed_wait/
```

## COM12：运行

```sh
cd /root/linux-study/07_threads/10_timed_wait
chmod +x timed_demo
./timed_demo
echo $?
./timed_demo ready
echo $?
```

默认典型输出：

```text
main: no data will be published; observing a timeout
worker: ready=0; timed wait with the original 3-second deadline
worker: ETIMEDOUT; mutex reacquired; ready=0
worker: deadline reached without data; ending this wait
main: joined; outcome=timeout
main: synchronization objects destroyed
```

ready模式：

```text
main: data prepared before creating worker
worker: received value=42
main: joined; outcome=data, value=42
main: synchronization objects destroyed
```

两者退出0。默认模式如果发生虚假唤醒，可额外打印返回0和再次等待，仍使用同一截止时间。

参考：[pthread_cond_timedwait](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html)、[clock_gettime与timespec](https://man7.org/linux/man-pages/man3/clock_gettime.3.html)、[条件变量时钟属性](https://man7.org/linux/man-pages/man3/pthread_condattr_getclock.3p.html)。
