# 实验 07-04：尝试获取互斥锁与 EBUSY

状态：学习者已上板，输出依次为创建前持锁、worker首次trylock返回EBUSY、join后主线程释放、再次trylock返回0并获得锁、解锁销毁，紧接echo $?为0。未收到学习者编译日志；继续确认trylock成功与持锁所有权的理解。上一节计数200000、退出0，学习者正确指出锁外读取旧值仍可能覆盖更新，已用两个线程先读取同一旧值再轮流写回解释完整临界区。

教师隔离验证：ARM版本使用-Wall -Wextra -Werror -pthread交叉编译并静态链接成功，file确认为ARM32位静态程序；WSL本机版本使用-O2及相同警告/线程选项编译，运行输出依次为首次EBUSY、join后释放、再次获取成功、解锁销毁，退出0。正式build目录留给学习者构建。

## 只增加一个核心接口

```c
int pthread_mutex_trylock(pthread_mutex_t *mutex);
```

本节继续使用默认属性的非递归、非健壮互斥锁。相对于已经学习的 pthread_mutex_lock：

| 锁的情况 | lock | trylock |
|---|---|---|
| 可以立即获取 | 返回0，调用者持有锁 | 返回0，调用者持有锁 |
| 由另一线程持有 | 等待锁可用，再获取并返回 | 立即返回EBUSY，没有获得锁 |

trylock 是一次获取尝试，不是只观察状态。返回0后必须按持有者规则访问资源并解锁；EBUSY后不能进入受保护区域，也不能去解锁其他线程持有的锁。不要把trylock用while无限重试包装成应用忙等；确实需要等待时，应使用合适的等待接口。

## EBUSY 与 errno 的区别

EBUSY 是 errno.h 中的错误编号宏，表示资源忙。include errno.h 是为了使用这个常量，不代表本接口把失败原因写在errno里。

```c
int rc = pthread_mutex_trylock(&mutex);
if (rc == EBUSY) {
    /* 没拿到锁，安排其他工作或按业务处理。 */
}
```

这里比较的是直接返回值rc，不能改成errno==EBUSY，也不能套用read的rc==-1判断。其他异常错误仍然通过check_pthread报告。EBUSY是本节预期会处理的状态，不直接当作程序必须退出的异常。

EBUSY描述这次获取没有成功，不保证函数返回后锁仍一直被持有；其他线程可能随时解锁。反过来，返回0则调用者确实持有锁，并不是先查询空闲再另行获取。

## 怎样固定顺序

```text
main：先拿到 mutex
  |
  +-- 创建 worker
  |      |
  |      +-- trylock：返回EBUSY，不等main解锁
  |      +-- 记录返回值，打印，然后返回
  |
  +-- join：等worker结束，自己仍持有mutex
  +-- 释放mutex
  +-- 再次trylock：返回0，自己获得mutex
  +-- 解锁、销毁
```

首次尝试时主线程一定持有锁：它在创建前就获取了锁，且要等worker结束才释放。第二次尝试时worker已结束，main已释放锁，没有其他竞争者，因此可以获取。无需用sleep猜时机，也不依赖碰巧发生某种调度。

主线程中first_result是普通整数，worker通过指针保存尝试结果；主线程在join成功后才读取它，避免了未同步的并发访问。这是对前两节线程参数和join的复用。

## 持锁join为什么在这里可行

这是为了固定实验顺序的特例：worker只有一次不等待的trylock，遇到EBUSY就结束，因此它的结束不依赖main解锁。不要直接照搬为一般业务结构。

如果把worker的trylock改成lock，主线程仍持锁join，就形成：

```text
main：等worker结束，之后才解锁
worker：等main解锁，之后才能结束
```

这就是死锁。只分析，不要求修改程序制造卡住。正常代码应尽量缩短持锁范围，避免在持锁时等待需要同一把锁才能完成的任务。

## 读源码时注意

- mutex、first_result、try_result、worker都是我们定义的名字。
- pthread_mutex_t是类型，PTHREAD_MUTEX_INITIALIZER是初始化宏，EBUSY是错误编号宏。
- check_pthread是上一节相同策略的辅助函数，异常时调用exit(1)终止整个进程。
- worker在EBUSY分支直接返回NULL，不调用unlock，因为它没有获得锁。main负责释放自己持有的锁。
- main第二次trylock成功后需要再次unlock，之后才能destroy。
- first_result用于确认首次尝试确实为EBUSY；它不是errno，也不是线程函数的返回指针。

## WSL：阅读、预览和编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/04_trylock
cat src/trylock_demo.c
make -n
make
echo $?
file build/trylock_demo
```

预期编译退出0，ARM32位静态程序，编译和链接均带-pthread。

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/04_trylock
```

## WSL：在本实验目录传输

```sh
scp -O build/trylock_demo root@192.168.10.2:/root/linux-study/07_threads/04_trylock/
```

## COM12：运行

```sh
cd /root/linux-study/07_threads/04_trylock
chmod +x trylock_demo
./trylock_demo
echo $?
```

预期：

```text
main: mutex locked before create
worker: trylock returned EBUSY; lock not acquired
main: worker joined; releasing mutex
main: trylock after release returned 0; lock acquired
main: mutex unlocked and destroyed
```

退出0表示完成且首次返回EBUSY；1表示意外接口错误；2表示首次结果与预期不符。输出中的先后顺序由创建、join和持锁范围保证，不推广为任意线程程序都具有固定输出顺序。

## 后续

下一节转入条件变量：如何等待“数据已经准备好”等业务条件，同时让其他线程能够拿到锁并推进工作，避免反复trylock或sleep轮询。先说明条件与锁的不同作用，再引入等待和通知接口。

参考：[pthread_mutex_lock / pthread_mutex_trylock / pthread_mutex_unlock](https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3p.html)。
