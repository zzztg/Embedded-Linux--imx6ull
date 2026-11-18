# 实验 07-03：数据竞争与互斥锁

状态：已收到学习者板端运行输出：counter=200000、expected=200000、锁销毁完成，紧接echo $?为0；本轮检查当前源码，加锁和解锁已恢复。尚未收到学习者编译日志，继续检查完整临界区的理解。上节已正确解释 main 局部对象生命周期与返回工作线程局部对象地址失效；已澄清后者为悬空指针，不能叫虚地址，也不能以某次读到残留值证明有效。

学习者实验补充：此前自行注释加锁/解锁后询问为什么仍为200000；当时检查源文件和WSL构建的add_many反汇编确认无锁调用，计数器更新包含ldr/加一/str。已解释单核短任务可能未在更新中间切换、输出正确不代表无数据竞争。随后收到本轮退出0输出，当前源码中的锁调用已由学习者恢复。未从最终计数推断具体调度过程。

教师隔离验证：ARM静态程序通过 -Wall -Wextra -Werror -pthread 编译链接，file确认为ARM32位静态程序；WSL本机版本开启-O2并以同样警告与线程选项编译，运行计数200000、销毁锁成功、退出0。正式build目录留给学习者亲手构建。运行结果用于检查材料，不替代互斥正确性的理论分析，也不宣称测试覆盖所有可能的调度。

## 先理解需要保护的操作

之前主线程在 join 成功后才读取结果。本节主线程和一个工作线程在 join 之前都执行 add_many，各自把同一个 counter 加一十万次。正确结果应为二十万。

普通 int 的 counter++ 是读、修改、写的复合操作，不能因为是一条 C 语句就认定为线程间原子操作。下面只用于分析“覆盖更新”的概念，不代表实际汇编固定为三条，也不把无同步 C 程序的结果限定为这个模型：

| 步骤 | 线程 A | 线程 B | counter |
|---|---|---|---|
| 1 | 读取 0 | | 0 |
| 2 | | 读取 0 | 0 |
| 3 | 计算并写回 1 | | 1 |
| 4 | | 计算并写回 1 | 1 |

预期累加两次，示意中却只有一次效果。单核也可能在操作尚未完成时切换线程，所以不能依靠单核避免问题。实际 C 程序中，不同线程对同一普通对象的冲突访问缺少同步顺序，至少一个为写入，构成数据竞争，行为未定义。即使某次无锁程序得到二十万，也不能证明正确；本节不运行未定义行为程序来碰运气，不用 volatile 或 sleep 伪装修复。

## 互斥锁是什么

```c
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;
```

- pthread_mutex_t：POSIX 定义的互斥锁类型。
- counter_mutex：本程序的锁对象名称，不是内置关键字。
- PTHREAD_MUTEX_INITIALIZER：默认属性的静态初始化宏，锁初始为未加锁状态；不是一个需要调用的函数，也不是业务计数值。
- 文件作用域的 static 限制名字在本源文件中可见，并提供静态存储期；不会使每条线程获得一份独立对象。counter 和 counter_mutex 各有一份。

本节新增的接口：

| 接口 | 作用 |
|---|---|
| pthread_mutex_lock(&counter_mutex) | 获取这把锁；若另一线程持有，则等待其可用 |
| pthread_mutex_unlock(&counter_mutex) | 持有者释放这把锁 |
| pthread_mutex_destroy(&counter_mutex) | 所有使用者结束后，销毁已解锁的锁对象 |

本例使用普通默认属性，不涉及健壮锁、递归锁等属性。接口成功返回0，错误通过返回编号报告，本例用 strerror(rc) 说明。

## 临界区与共同遵守的规则

```c
pthread_mutex_lock(&counter_mutex);
counter++;
pthread_mutex_unlock(&counter_mutex);
```

真实源码检查了每次调用结果。成功加锁到解锁之间的代码称为临界区；完整的读、改、写都必须放在其中。两个线程竞争的是同一个锁对象，才能保证这段操作不会相互穿插。互斥锁也提供线程之间需要的内存同步。

锁不会自动拦截 counter 的所有访问，不会自动把某个变量绑定到它。若另一线程不加锁直接修改 counter，或使用另一把锁，保护规则就被破坏。把读取放到锁外，只锁住写回也可能覆盖其他线程已经完成的更新。

互斥锁不等于关闭中断或停止整个进程调度。持锁线程仍可能被切换出去；其他线程也仍可执行，只是遵守规则的线程不能同时持有同一把锁。等待 lock 不是应用自己写 while 循环忙等；具体实现可能有短暂自旋等优化，不在本节展开。线程获取锁的次序和次数分配没有固定交替保证。

## 本例执行关系

```text
main: 初始化 counter=0，锁初始化
  |
  +-- pthread_create --> worker: add_many(100000)
  |
  +-- main: add_many(100000)
  |
  +-- pthread_join：等待 worker 结束
  |
  +-- 读取 counter=200000
  |
  +-- 销毁互斥锁，main 返回
```

每次加一使用同一个锁，然后释放；两个线程有机会交替获得锁，但不要求一人一次。串行调度也合法，正确性不依赖恰好发生怎样的切换。

join 仅保证最终读取发生在工作线程结束后，不能修复此前无锁读写形成的数据竞争或业务错误。本例运行中靠互斥锁保护更新，运行结束时靠 join 确认可以读取最终结果和销毁锁。

初始化和最终读取没有加锁，因为第一次访问发生在创建线程之前，最后一次访问发生在 join 成功之后，期间没有其他线程访问该对象。不要推广为运行中读变量不需要锁；运行中的读取也必须遵守同步规则。

## 普通辅助函数与局部变量

check_pthread 是本程序编写的辅助函数，不是新的系统接口。它检查 rc，错误时用 strerror 输出说明，并调用 stdlib.h 声明的 exit(1)，结束整个演示进程。与线程入口 return NULL 只结束当前线程不同。采用这种错误策略是为了保持本节主线简洁。

add_many 是普通 C 函数。main 和 worker 都可以调用它，每次调用有各自的 iterations、i、rc 等局部执行状态，而文件作用域的 counter 和 counter_mutex 是同一份。相同函数名或相同局部变量名不代表局部对象共享。

本节延续参数副本的写法：worker 用 int iterations = *(int *)arg 复制迭代次数，之后无需修改 main 的 iterations。main 没有在运行期间修改它，并等待工作线程结束，参数有效期正确。

## 注意锁的使用边界

所有参与者使用同一个锁对象，不能通过普通赋值复制锁来代替共享原锁。获取成功后应由持有者解锁，避免漏解锁；本节不在持锁期间 return、join 或做阻塞I/O。不要重复获取已经由自身持有的默认锁。销毁时必须确认没有线程持锁、等待或继续使用，销毁后也不能继续操作。

这些约束后续结合死锁、条件变量和退出回收逐步展开。

## WSL：阅读与亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/03_mutex
cat src/mutex_demo.c
make -n
make
echo $?
file build/mutex_demo
```

预期编译退出0，ARM32位静态程序，编译和链接均有-pthread。不要删除锁去重复运行无同步代码，将某次正常计数当作正确性依据。

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/03_mutex
```

## WSL：传输

在本实验WSL目录中：

```sh
scp -O build/mutex_demo root@192.168.10.2:/root/linux-study/07_threads/03_mutex/
```

## COM12：运行

```sh
cd /root/linux-study/07_threads/03_mutex
chmod +x mutex_demo
./mutex_demo
echo $?
```

预期：

```text
main: before create counter=0
main: after join counter=200000, expected=200000
main: mutex destroyed
```

退出0表示整个程序完成且计数符合预期；1表示接口报告错误；2表示计数与预期不符。正常输出只打印初始和最终状态，不将打印放进十万次的临界区，不以输出交替作为线程交替的证明。

## 参考

- [互斥锁加锁与解锁](https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html)
- [互斥锁初始化与销毁](https://man7.org/linux/man-pages/man3/pthread_mutex_destroy.3p.html)
