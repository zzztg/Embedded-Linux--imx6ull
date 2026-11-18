# 实验 07-05：条件变量的等待、通知与重新检查

状态：已收到学习者两阶段板端输出：第一次通知后wait返回且ready仍0，worker重新等待；第二次发布ready=1后读取42，主线程join并完成条件变量和锁销毁。学习者正确解释等待期间若仍持mutex，主线程会卡在获取锁处，双方无法推进；已澄清主线程修改的是value和ready，cond_signal用于通知而不替业务条件赋值。尚未贴出紧接程序的退出状态或编译日志，继续在生产者消费者中应用该协议。上节trylock两阶段板端运行完成、退出0；学习者正确解释trylock成功后再次调用默认非递归lock会自身死锁，已经持有锁的线程无法走到后续unlock。

教师隔离验证：ARM静态程序通过-Wall -Wextra -Werror -pthread交叉编译，file确认为ARM32位静态程序；WSL本机-O2版本通过分阶段验证：观察首次等待后通知，确认ready仍0且重新等待，再发布并读取42，退出0。另验证连续提前输入仍可完成、输入EOF退出2。正式build目录留给学习者构建。

## 为什么锁之外还需要条件变量

互斥锁回答“现在谁能安全访问共享状态”，不自动回答“数据是否准备好”。获得锁后若发现ready为0，需要等待生产者把数据准备好；一直持锁等待会挡住生产者，反复解锁检查或sleep轮询又不利于及时等待。条件变量让线程在条件不满足时释放锁并等待，收到通知后重新拿锁检查状态。

## 四个对象分别做什么

| 名字 | 类型 | 本例作用 |
|---|---|---|
| value | int | 业务数据，生产者写42 |
| ready | int | 业务条件状态，0未准备好、1准备好 |
| mutex | pthread_mutex_t | 保护ready和value的访问 |
| changed | pthread_cond_t | 让线程等待和接收状态可能改变的通知 |

条件变量changed不是业务布尔变量ready。通知不会自动给ready赋值，不携带value，也不负责判断ready是否为真。所有运行期间对ready和value的访问都由同一个mutex协调。

```c
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
```

pthread_cond_t是条件变量类型；PTHREAD_COND_INITIALIZER是默认属性的静态初始化宏，与互斥锁初始化方式相似。

## 三个新增接口

| 接口 | 含义 |
|---|---|
| pthread_cond_wait(&changed, &mutex) | 持锁进入；释放锁并等待；成功返回前重新获取同一把锁 |
| pthread_cond_signal(&changed) | 通知正在等待的线程；本例只有一个等待者 |
| pthread_cond_destroy(&changed) | 不再有线程使用或等待后销毁条件变量 |

成功为0，错误编号直接返回，继续使用check_pthread。signal在这里是条件变量通知，与kill/SIGINT那种操作系统信号不同，不会调用注册的信号处理函数。

本节只使用普通默认属性，不涉及超时、广播、取消和健壮互斥锁；它们后续再展开。

## wait调用前后锁的状态

```c
pthread_mutex_lock(&mutex);
while (!ready) {
    pthread_cond_wait(&changed, &mutex);
}
/* 此处仍持有mutex，并且重新检查得知ready为真。 */
int received = value;
pthread_mutex_unlock(&mutex);
```

实际源码检查每个接口结果并添加观察日志。!ready表示ready为0；while每次循环都重新判断。

调用wait之前必须已经持有mutex。wait将释放mutex与进入等待作为配合使用的原子操作，避免检查完条件后手动解锁再等待造成通知落在缝隙中。这里的原子性针对遵守同一mutex协议的等待者/修改者，不是关闭中断。

wait等待期间不持有mutex，生产者可以拿锁、更新数据和ready、通知。wait成功返回前已经重新获取mutex，因此后面不用再lock一次；否则可能自身死锁。被通知不等于立刻返回：如果生产者仍持锁，等待者还需要等锁可用。

## 为什么用while而不是if

从wait成功返回，不代表业务条件一定成立，必须重新检查。原因包括允许的虚假唤醒，以及多消费者中数据已被其他线程取走。本实验先以一个额外通知直观展示：第一次通知后ready仍为0，线程应再次等待。

第一次通知是程序明确调用signal产生的，不能把这次现象称作系统产生的虚假唤醒；它只是说明“被通知”与“条件成立”是两件事。真正的虚假唤醒可能没有对应的显式通知，标准同样要求重新检查。

## 两次回车的实验过程

main创建worker后，在未持mutex的情况下等待用户输入。worker获取mutex，看到ready=0，打印并调用wait，释放mutex后等待。

第一次回车：main获取mutex，只调用signal，不修改ready，然后释放mutex。worker返回时重新持锁，读到ready仍为0，于是while继续，重新wait。

第二次回车：main获取mutex，先写value=42，再写ready=1，然后signal并解锁。worker返回并检查ready为1，复制value到自己的局部整数received，再解锁并打印42。

main最后join，再销毁条件变量和互斥锁。不会持mutex等待用户输入或join。

## 通知会不会“存起来”

signal时若没有线程等待，通知不会作为一个未来可以领取的令牌存起来。但业务状态ready会保留，线程之后获取mutex检查到ready=1，就不会再wait；所以这个程序允许生产者先发布数据。

防止错误等待的完整规则是：用同一mutex保护条件检查和修改，等待者用while判断，使用cond_wait原子地解锁并等待，生产者正确更新状态并通知。只调用signal、不保存业务状态，不能代替这个协议。

本例在锁内signal，随后才unlock。signal本身不释放mutex，也不把mutex直接交给工作线程。

## 输出顺序和输入要求

先等到看到worker的ready=0等待日志再按第一次回车。再等到看到它打印wait返回、ready仍为0以及再次等待后，才按第二次回车。每次只按回车，不输入其他字符。

main提示与worker初次等待日志可能先后不同，第二次输入提示也可能先于worker重新检查日志。若提前连续按两次回车，worker可能直接看到ready=1而跳过一次或全部等待，这是合法调度，不是条件变量失效。为了观察两次检查，请按上述阶段手动推进。

## 辅助代码没有新增线程机制

check_pthread沿用前节：异常时打印strerror(rc)并exit(1)，结束整个进程。press_enter是我们写的普通函数，printf提示、fflush(stdout)让提示立即可见、getchar等待一个输入字符；遇到EOF时exit(2)，因此输入提前结束会结束演示进程。

worker的参数本节不使用，创建时传NULL；(void)arg只是显式标明未使用这个形参，避免警告，不会调用函数或释放对象。

## WSL：亲手阅读与编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/05_cond_wait
cat src/cond_demo.c
make -n
make
echo $?
file build/cond_demo
```

预期编译退出0，ARM32位静态程序。

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/05_cond_wait
```

## WSL：本实验目录传输

```sh
scp -O build/cond_demo root@192.168.10.2:/root/linux-study/07_threads/05_cond_wait/
```

## COM12：运行和两次输入

```sh
cd /root/linux-study/07_threads/05_cond_wait
chmod +x cond_demo
./cond_demo
```

先观察worker等待，按第一次回车；观察ready仍为0、worker再次等待，按第二次回车。程序退出后再执行：

```sh
echo $?
```

一次典型输出，具体提示行顺序可能不同：

```text
main: press ENTER to notify without setting ready
worker: ready=0; waiting (wait releases mutex)

main: notify only; ready=0
main: press ENTER again to publish value=42
worker: wait returned; mutex reacquired; ready=0
worker: ready=0; waiting (wait releases mutex)

main: published value=42, ready=1; notifying worker
worker: wait returned; mutex reacquired; ready=1
worker: received value=42
main: worker joined; condition variable and mutex destroyed
```

退出0。关注实际ready状态和锁的持有关系，不按逐行顺序机械判断。

参考：[pthread_cond_wait](https://man7.org/linux/man-pages/man3/pthread_cond_wait.3p.html)、[pthread_cond_signal](https://man7.org/linux/man-pages/man3/pthread_cond_broadcast.3p.html)。
