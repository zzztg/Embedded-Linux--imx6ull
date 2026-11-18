# 实验07-11：单调时钟与条件变量初始化属性

理解补充：学习者正确解释cond_init之后把attr设为CLOCK_REALTIME只修改配置对象，不会改变已有changed；已确认changed仍需使用原CLOCK_MONOTONIC时间基准。2026-11-11路线调整：取消信号进阶专项，补06_ipc/08_fifo后进入网络编程。

状态：两模式均已收到板端日志。默认模式先按CLOCK_MONOTONIC初始化并销毁属性对象，后完成ETIMEDOUT、ready0、outcome=timeout、join及实际条件变量和锁的销毁；ready模式同样初始化后取得42、outcome=data并完成join和清理。未贴紧接程序的退出状态或学习者编译日志，未独立测量板端等待时长，也未修改系统时间测试跳变。继续确认初始化后属性对象的修改不影响已有条件变量。07-10两模式已上板且各自退出0；学习者正确回答固定截止12:00:03时，在12:00:02醒来且条件不满足，应继续等剩余约1秒。已解释每次通知后重设3秒属于不同业务规则，且通知不自动提供时间戳。

教师隔离验证：ARM版本以-Wall -Wextra -Werror -pthread交叉编译并静态链接成功，file确认为ARM32位静态程序。WSL本机-O2版本属性销毁后仍正常使用条件变量，默认模式约3.013秒后ETIMEDOUT、join及销毁完成、退出0；ready模式跳过等待取得42、完成清理并退出0。未修改系统时钟，日期跳变隔离的性质依据时钟与条件变量规范说明。正式build目录留给学习者构建。

## 选择计时基准

CLOCK_REALTIME表示系统日期时间，可被设置、校时，日期跳变会影响以它计算的绝对等待期限。CLOCK_MONOTONIC是适合测量经过时间的单调时钟，不会因手动修改系统日期而发生相同的时间跳变。

这里不把MONOTONIC说成绝对不受所有校时影响：Linux下它可能受渐进频率调整影响，不计系统挂起时间，相邻读取也可能相等；它不是线程实际消耗的CPU时间，也不表示年月日。本节不扩展挂起计时和RAW时钟。

例如REALTIME截止12:00:03，等待中将系统时钟拨到12:10:00，绝对期限已经过去；向后拨则可能延后。MONOTONIC以自己的时间轴计算当前值加3秒，可以避免这种日期跳变的影响。不需要修改系统日期来做本实验。

## 两处时钟必须一致

1. 条件变量内部解释abstime所用的时钟，配置为CLOCK_MONOTONIC。
2. clock_gettime获取deadline也使用CLOCK_MONOTONIC。

不能只改clock_gettime，而条件变量仍用默认CLOCK_REALTIME。两个时间轴的起点不同，结构体里的数字即使类型相同，也不能跨时间轴直接比较，可能导致立即超时或等待远超预期。

## 属性对象与运行对象

```c
pthread_condattr_t attr;
static pthread_cond_t changed;
```

attr是初始化条件变量时使用的配置对象；changed是线程实际等待、通知的同步对象。两个类型不同、职责和生命周期不同。initialize_condition是本程序自己写的普通辅助函数。

四步配置：

```c
pthread_condattr_init(&attr);
pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
pthread_cond_init(&changed, &attr);
pthread_condattr_destroy(&attr);
```

| 接口 | 本例作用 |
|---|---|
| pthread_condattr_init | 初始化属性对象，先取得默认配置 |
| pthread_condattr_setclock | 修改属性中的时钟选择 |
| pthread_cond_init | 按给定属性初始化changed |
| pthread_condattr_destroy | 配置使用完毕后清理attr |

这些接口成功0、错误直接返回编号，实际代码全部通过check_pthread检查。attr需要先init后setclock，不能把未初始化的局部对象直接传去配置。

cond_init使用配置完成初始化后，之后修改或destroy这个attr不会改变已经初始化的changed。因此可以在创建工作线程之前destroy attr，changed仍能继续等待和通知；attr并不是线程运行期间必须一直指向的配置存储。销毁attr后不能继续使用它，除非重新初始化。

## 为什么取消PTHREAD_COND_INITIALIZER

以前changed使用默认静态初始化宏；本节要指定时钟，选择通过cond_init初始化。

文件作用域的static pthread_cond_t changed只准备存储空间，C对静态存储的清零不等于我们可以跳过POSIX初始化协议。必须先执行initialize_condition，再创建使用它的线程。不能先用PTHREAD_COND_INITIALIZER初始化，再不经destroy直接cond_init一次，这是重复初始化、行为未定义。

同样不能在仍有线程等待或使用时destroy再init来改时钟。这里在任何线程创建前初始化，所有使用者join之后销毁。

## 生命周期顺序

```text
attr init → 设置MONOTONIC → changed init → attr destroy
                                           |
                              创建线程，使用changed等待
                                           |
                              join → changed destroy
```

pthread_condattr_destroy与pthread_cond_destroy销毁的是不同对象。前者只结束临时配置对象的使用，后者结束实际同步对象的使用。

## 等待过程沿用上一节

worker持mutex读取CLOCK_MONOTONIC，deadline.tv_sec加3；while外只计算一次deadline，通知或虚假唤醒后继续使用同一个期限。timedwait返回ETIMEDOUT时仍持锁，重新检查ready，选择数据或超时业务结果，最后解锁返回。超时不是强制退出线程，约3秒也不是硬实时返回保证。

mutex继续使用PTHREAD_MUTEX_INITIALIZER。给changed配置时钟，不会改变互斥锁类型，也不会设置系统时间。

## WSL：亲手编译

```sh
cd /mnt/c/Users/dingf/Desktop/imx6ull/07_threads/11_monotonic
cat src/mono_demo.c
make -n
make
echo $?
file build/mono_demo
```

预期编译退出0，ARM32位静态程序。

## COM12：创建目录

```sh
mkdir -p /root/linux-study/07_threads/11_monotonic
```

## WSL：传输

```sh
scp -O build/mono_demo root@192.168.10.2:/root/linux-study/07_threads/11_monotonic/
```

## COM12：两种模式

```sh
cd /root/linux-study/07_threads/11_monotonic
chmod +x mono_demo
./mono_demo
echo $?
./mono_demo ready
echo $?
```

默认模式先打印condition initialized with CLOCK_MONOTONIC; attributes destroyed，再约3秒后超时，outcome=timeout，join及实际条件变量、锁的销毁完成，退出0。

ready模式同样先初始化条件变量并销毁属性，然后在创建worker前准备42，worker直接取得42、不调用timedwait，outcome=data，完成join和清理，退出0。

本实验验证配置后的等待和属性对象结束后运行对象仍能使用；没有修改系统时间来实测校时跳变，关于时钟行为依据接口规范解释。

参考：[时钟含义](https://man7.org/linux/man-pages/man3/clock_gettime.3.html)、[时钟属性](https://man7.org/linux/man-pages/man3/pthread_condattr_getclock.3p.html)、[属性初始化与销毁](https://man7.org/linux/man-pages/man3/pthread_condattr_destroy.3p.html)、[条件变量初始化与销毁](https://man7.org/linux/man-pages/man3/pthread_cond_destroy.3p.html)。
